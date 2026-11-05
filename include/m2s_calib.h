// m2s_calib.h — measures how a game really responds to the right stick, using only the video.
// No dependencies on Qt or ffmpeg, so it can be tested against simulated games (see calibtest.cpp).
//
// It pushes the stick to known values and watches the picture slide sideways:
//   1. settle    hold still, measure how much the shift estimate jitters at rest
//   2. latency   short stick pulses, time from command to the picture moving
//   3. deadzone  raise the stick in small steps until the picture starts to move
//   4. levels    hold each stick level, measure the speed: this is the game's response curve
//   5. ramp      push from rest: does speed keep building while held? (ease-in or smoothing)
//   6. turn      hold full stick until the view repeats: one full turn gives real degrees per second
//
// Requirements in game: stand still in an open spot with a varied background, don't touch anything.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>
#include "mouse_to_stick.h"

namespace m2s {

struct CalibResult {
    bool ok = false;
    std::string message;            // why it failed, or a warning worth showing
    double deadzone = 0;            // stick value where the camera starts to move
    double wmax_deg_s = 0;          // camera deg/s at full stick
    double latency_ms = 0;          // stick command to picture moving, measured on this Mac
    bool linear = false;            // game response is a straight line past the deadzone
    double curve_worst = 0;         // how far the measured points were from that line
    double px_per_turn = 0;         // picture slide for one full turn (0 if not recognised)
    double ramp_ratio = 1;          // held speed divided by speed right after pushing from rest
    bool wmax_estimated = false;    // full turn wasn't recognised: wmax assumes a 103 degree view
    bool ramp_detected = false;     // speed grows while the stick is held (ease-in or acceleration)
    std::vector<CurvePoint> curve;  // stick -> fraction of full speed
};

class AutoCalib {
public:
    static constexpr int N = 320;   // picture is reduced to one row of N brightness values
    static constexpr double kFovDeg = 103;   // assumed view width; only used to even out the picture

    void start(double t) {
        std::lock_guard<std::mutex> l(mtx_);
        result_ = CalibResult();
        phase_ = Settle; phaseStart_ = t; frames_ = 0; restSum_ = 0; restN_ = 0;
        haveProf_ = false; haveRef_ = false; latencies_.clear(); trial_ = 0; trialStart_ = t;
        trialSeen_ = false; dzStep_ = 1; dzHits_ = 0; dzFirst_ = -1; levels_.clear(); level_ = 0;
        std::fill(meanProf_, meanProf_ + N, 0.0); meanN_ = 0; A_ = 0; lastT_ = t;
        active_ = true; finished_ = false;
    }
    void abort() { std::lock_guard<std::mutex> l(mtx_); if (active_) fail("cancelled"); }
    bool active() { std::lock_guard<std::mutex> l(mtx_); return active_; }
    // True once, right after the run ends. Result is then available.
    bool takeFinished(CalibResult& out) {
        std::lock_guard<std::mutex> l(mtx_);
        if (!finished_) return false;
        finished_ = false; out = result_;
        return true;
    }
    std::string status() { std::lock_guard<std::mutex> l(mtx_); return phaseName(); }

    // Picture tools, also used to measure real camera turns in tests.
    static double profile(const uint8_t* y, int w, int h, int stride, int step, int off, double* p) { return makeProfile(y, w, h, stride, step, off, p); }
    static bool slide(const double* a, const double* b, double& s) { Shift m = shift(a, b); s = m.s; return m.ok; }

    // Call from the input thread. Returns the stick x to send right now.
    double stick(double t) {
        std::lock_guard<std::mutex> l(mtx_);
        if (!active_) return 0;
        lastT_ = t;
        double e = t - phaseStart_;
        switch (phase_) {
        case Settle:
            if (e < 1.0) return 0;
            if (frames_ < 15) return fail("no video frames arrived"), 0.0;
            noise_ = restN_ ? restSum_ / restN_ : 0;
            enter(Latency, t); trialStart_ = t; trialSeen_ = false;
            return 0;
        case Latency: {
            double te = t - trialStart_;
            if (te >= 1.0) {
                if (trialSeen_) latencies_.push_back(trialLatency_);
                if (++trial_ >= 5) {
                    if (latencies_.size() < 3)
                        return fail("the picture didn't react to the stick: is the game in control, "
                                    "and is there a varied background?"), 0.0;
                    std::sort(latencies_.begin(), latencies_.end());
                    latency_ = latencies_[latencies_.size() / 2];
                    enter(Deadzone, t); dzStart_ = t; haveRef_ = false; dzRefTime_ = t + latency_ + 0.1;
                    return stepValue();
                }
                trialStart_ = t; trialSeen_ = false; te = 0;
            }
            return te < 0.25 ? 0.6 : 0;
        }
        case Deadzone: {
            if (t - dzStart_ >= latency_ + 0.1 + kDzHold) {
                double moved = (haveRef_ && haveProf_) ? std::fabs(shift(ref_, prof_).s) : 0;
                if (moved >= kDzMovedPx) {
                    if (dzHits_++ == 0) dzFirst_ = stepValue();
                    if (dzHits_ >= 2) { startLevels(t); return levels_[0]; }
                } else { dzHits_ = 0; dzFirst_ = -1; }
                if (++dzStep_ > 40) return fail("camera never moved below 40% stick"), 0.0;
                dzStart_ = t; haveRef_ = false; dzRefTime_ = t + latency_ + 0.1;
            }
            return stepValue();
        }
        case Levels: {
            if (e >= levelSettle() + kLevelHold) {
                speed_[level_] = (lvlPx_ + lvlOff_) / std::max(lvlTl_ - lvlT0_, 1e-3);
                haveRef_ = false;
                if (++level_ >= (int)levels_.size()) { enter(Ramp, t); return 0; }
                phaseStart_ = t; resetAcc();
            }
            return levels_[level_];
        }
        case Ramp: {
            double on = e - kRampRest;
            if (on < 0) return 0;
            if (on >= kRampHold + latency_) {
                double early = earlyT_ > 0 ? earlyS_ / earlyT_ : 0, late = lateT_ > 0 ? lateS_ / lateT_ : 0;
                if (early > 1) { result_.ramp_ratio = late / early; result_.ramp_detected = result_.ramp_ratio > 1.2; }
                enter(Turn, t); haveRef_ = false; return 1.0;
            }
            return 0.5;
        }
        case Turn:
            if (e >= 30.0) { finish(0, true); return 0; }
            return 1.0;
        default: return 0;
        }
    }

    // Call from the video thread with the luma plane. `step` and `offset` select the byte per pixel,
    // so 8-bit (step 1, offset 0) and P010 (step 2, offset 1) both work.
    void onFrame(const uint8_t* y, int w, int h, int stride, double t, int step = 1, int offset = 0) {
        double p[N];
        double contrast = makeProfile(y, w, h, stride, step, offset, p);
        std::lock_guard<std::mutex> l(mtx_);
        if (!active_) return;
        frames_++;
        double dt = haveProf_ ? t - profT_ : 0;
        Shift sh{0, false};
        if (haveProf_ && contrast > 1.0) sh = shift(prof_, p);
        switch (phase_) {
        case Settle:
            if (sh.ok) { restSum_ += std::fabs(sh.s); restN_++; }
            break;
        case Latency:
            if (!trialSeen_ && sh.ok && t > trialStart_ && std::fabs(sh.s) > std::max(0.4, 4 * noise_)) {
                trialSeen_ = true; trialLatency_ = t - trialStart_;
            }
            break;
        case Deadzone:
            if (!haveRef_ && t >= dzRefTime_) { std::copy(p, p + N, ref_); haveRef_ = true; }
            break;
        case Levels:
            if (dt > 0 && t - phaseStart_ >= levelSettle()) {
                if (!haveRef_) { std::copy(p, p + N, ref_); haveRef_ = true; lvlT0_ = t; lvlPx_ = lvlOff_ = 0; lvlTl_ = t; }
                else {
                    // Compare against a held keyframe so slow speeds add up to whole pixels.
                    Shift k = shift(ref_, p);
                    if (k.ok) {
                        lvlOff_ = std::fabs(k.s); lvlTl_ = t;
                        if (lvlOff_ >= 12) { lvlPx_ += lvlOff_; lvlOff_ = 0; std::copy(p, p + N, ref_); }
                    }
                }
                for (int i = 0; i < N; i++) meanProf_[i] += p[i];
                meanN_++;
            }
            break;
        case Ramp: {
            double on = t - phaseStart_ - kRampRest - latency_;     // time since the push reached the picture
            if (sh.ok && dt > 0 && on >= 0.02 && on < 0.18) { earlyS_ += std::fabs(sh.s); earlyT_ += dt; }
            if (sh.ok && dt > 0 && on >= 0.6) { lateS_ += std::fabs(sh.s); lateT_ += dt; }
            break;
        }
        case Turn:
            if (t - phaseStart_ >= latency_ + 0.2) {
                double q[N];
                for (int i = 0; i < N; i++) q[i] = p[i] - (meanN_ ? meanProf_[i] / meanN_ : 0);
                if (!haveRef_) { std::copy(q, q + N, ref_); haveRef_ = true; refT_ = t; A_ = 0; break; }
                if (sh.ok) A_ += sh.s;
                if (std::fabs(A_) > kTurnMinPx) {
                    // At speed no frame lands exactly on the starting view, so allow an offset and
                    // correct the time by it: travelled = one turn + offset.
                    Shift m = shift(ref_, q);
                    if (m.ok && corrAt(ref_, q, m.s) > 0.75) { double elapsed = t - refT_; result_.px_per_turn = std::fabs(A_ - m.s); finish(elapsed * (1.0 - m.s / A_), false); }
                }
            }
            break;
        default: break;
        }
        std::copy(p, p + N, prof_); haveProf_ = true; profT_ = t;
    }

private:
    enum Phase { Idle, Settle, Latency, Deadzone, Levels, Ramp, Turn };
    struct Shift { double s; bool ok; };
    static constexpr double kDzHold = 0.45, kDzMovedPx = 0.6, kLevelHold = 1.0, kRampRest = 0.5, kRampHold = 1.2;
    static constexpr double kTurnMinPx = 0.7 * 360 * N / kFovDeg;     // can't be a full turn before 252 degrees
    static constexpr int kMaxShift = 64;

    void enter(Phase ph, double t) { phase_ = ph; phaseStart_ = t; resetAcc(); }
    void resetAcc() { accSum_ = accT_ = earlyS_ = earlyT_ = lateS_ = lateT_ = 0; }
    double stepValue() const { return dzStep_ * 0.01; }
    double levelSettle() const { return latency_ + 0.2; }
    const char* phaseName() const {
        switch (phase_) { case Settle: return "settling"; case Latency: return "latency"; case Deadzone: return "deadzone";
                          case Levels: return "curve"; case Ramp: return "ramp"; case Turn: return "full turn"; default: return "idle"; }
    }

    void startLevels(double t) {
        dz_ = std::max(0.0, dzFirst_ - 0.005);
        static const double f[] = {0.03, 0.08, 0.15, 0.25, 0.4, 0.55, 0.7, 0.8, 0.88, 0.92, 0.96, 1.0};
        levels_.clear();
        for (double v : f) levels_.push_back(dz_ + (1.0 - dz_) * v);
        speed_.assign(levels_.size(), 0);
        level_ = 0; enter(Levels, t); haveRef_ = false;
    }

    void fail(const std::string& why) {
        result_.ok = false; result_.message = why; active_ = false; finished_ = true; phase_ = Idle;
    }

    void finish(double period, bool timedOut) {
        double top = speed_.empty() ? 0 : speed_.back();
        if (top <= 0) { fail("couldn't measure the speed at full stick"); return; }
        if (timedOut || period <= 0) {
            result_.wmax_deg_s = top / (N / kFovDeg);        // picture spans the assumed view width
            result_.wmax_estimated = true;
            result_.message = "full turn wasn't recognised, so the turn speed is an estimate";
        } else {
            result_.wmax_deg_s = 360.0 / period;
        }
        // The scan moves in 1% steps. A line through the slowest speeds finds the edge more exactly.
        if (levels_.size() >= 4) {
            double sx = 0, sy = 0, sxx = 0, sxy = 0;
            for (int i = 0; i < 4; i++) { sx += levels_[i]; sy += speed_[i]; sxx += levels_[i] * levels_[i]; sxy += levels_[i] * speed_[i]; }
            double den = 4 * sxx - sx * sx, a = den > 1e-12 ? (4 * sxy - sx * sy) / den : 0;
            // Only trust it within the scan's own step: curved games bend this line.
            if (a > 0) { double fit = (sx - sy / a) / 4; if (std::fabs(fit - dz_) <= 0.006 && fit >= 0) dz_ = fit; }
        }
        result_.deadzone = dz_;
        result_.latency_ms = latency_ * 1000;
        result_.curve.clear();
        result_.curve.push_back({dz_, 0});
        double m = 0;
        for (size_t i = 0; i < levels_.size(); i++) {
            m = std::max(m, speed_[i] / top);                 // curve must never go backwards
            result_.curve.push_back({levels_[i], std::min(m, 1.0)});
        }
        result_.curve.back().frac = 1.0;
        // Within measurement noise of a straight line? Then the exact line beats a noisy table.
        double worst = 0;
        for (const CurvePoint& c : result_.curve) worst = std::max(worst, std::fabs(c.frac - (c.stick - dz_) / (1.0 - dz_)));
        if (worst <= 0.04) { result_.linear = true; result_.curve_worst = worst; result_.curve.clear(); }
        result_.ok = true; active_ = false; finished_ = true; phase_ = Idle;
    }

    // Average a band of the picture into one row, normalised. Returns the contrast before normalising.
    static double makeProfile(const uint8_t* y, int w, int h, int stride, int step, int off, double* p) {
        int y0 = (int)(h * 0.13), y1 = (int)(h * 0.50), rows = 0;   // above the held weapon and the HUD
        std::fill(p, p + N, 0.0);
        // A flat picture stretches toward its edges. Sample evenly by viewing angle instead, so a
        // camera turn slides every column by the same amount.
        int xs[N];
        const double half = kFovDeg / 2 * M_PI / 180, f = (w / 2.0) / std::tan(half);
        for (int i = 0; i < N; i++) {
            double u = ((i + 0.5) / N * 2 - 1) * half;
            xs[i] = std::clamp((int)(w / 2.0 + f * std::tan(u)), 0, w - 1) * step + off;
        }
        for (int r = y0; r < y1; r += 2, rows++) {
            const uint8_t* row = y + r * stride;
            for (int i = 0; i < N; i++) p[i] += row[xs[i]];
        }
        double mean = 0, var = 0;
        for (int i = 0; i < N; i++) { p[i] /= std::max(rows, 1); mean += p[i]; }
        mean /= N;
        for (int i = 0; i < N; i++) { p[i] -= mean; var += p[i] * p[i]; }
        double sd = std::sqrt(var / N);
        if (sd > 1e-6) for (int i = 0; i < N; i++) p[i] /= sd;
        return sd;
    }

    // How far did the picture slide from a to b? Uses the middle of the row, where a rotation
    // shifts pixels most evenly. Sub-pixel via a parabola through the best match.
    static Shift shift(const double* a, const double* b) {
        double sad[2 * kMaxShift + 1], sum = 0, best = 1e30; int bi = 0;
        const int i0 = kMaxShift, i1 = N - kMaxShift;
        for (int s = -kMaxShift; s <= kMaxShift; s++) {
            double v = 0;
            for (int i = i0; i < i1; i++) v += std::fabs(a[i] - b[i + s]);
            sad[s + kMaxShift] = v; sum += v;
            if (v < best) { best = v; bi = s; }
        }
        double mean = sum / (2 * kMaxShift + 1);
        if (mean <= 1e-9 || best > mean * 0.8) return {0, false};   // no clear match
        double s = bi;
        if (bi > -kMaxShift && bi < kMaxShift) {
            double l = sad[bi + kMaxShift - 1], c = sad[bi + kMaxShift], r = sad[bi + kMaxShift + 1];
            double d = l - 2 * c + r;
            if (d > 1e-9) s += std::clamp(0.5 * (l - r) / d, -0.5, 0.5);
        }
        return {s, true};
    }

    static double corrAt(const double* a, const double* b, double shiftPx) {
        const int s = (int)std::lround(shiftPx);
        double ab = 0, aa = 0, bb = 0;
        for (int i = kMaxShift; i < N - kMaxShift; i++) { ab += a[i] * b[i + s]; aa += a[i] * a[i]; bb += b[i + s] * b[i + s]; }
        return ab / std::sqrt(std::max(aa * bb, 1e-12));
    }

    std::mutex mtx_;
    bool active_ = false, finished_ = false;
    Phase phase_ = Idle;
    double phaseStart_ = 0, lastT_ = 0;
    CalibResult result_;
    int frames_ = 0, restN_ = 0, trial_ = 0, dzStep_ = 1, dzHits_ = 0, level_ = 0, meanN_ = 0;
    double restSum_ = 0, noise_ = 0, latency_ = 0.1, trialStart_ = 0, trialLatency_ = 0, dz_ = 0, dzFirst_ = -1;
    double dzStart_ = 0, dzRefTime_ = 0, accSum_ = 0, accT_ = 0, earlyS_ = 0, earlyT_ = 0, lateS_ = 0, lateT_ = 0;
    double A_ = 0, refT_ = 0, profT_ = 0, lvlT0_ = 0, lvlTl_ = 0, lvlPx_ = 0, lvlOff_ = 0;
    bool trialSeen_ = false, haveProf_ = false, haveRef_ = false;
    double prof_[N], ref_[N], meanProf_[N];
    std::vector<double> latencies_, levels_, speed_;
};

// Settings survive restarts in a small text file.
inline bool saveConfig(const std::string& path, const Params& p) {
    std::ofstream f(path);
    if (!f) return false;
    f << "deg_per_count " << p.deg_per_count << "\nwmax_deg_s " << p.wmax_deg_s << "\ndeadzone " << p.deadzone
      << "\nwindow_ms " << p.window_ms << "\ny_scale " << p.y_scale << "\ninvert_y " << (p.invert_y ? 1 : 0) << "\naxis_pull_deg " << p.axis_pull_deg << "\ndiag_loss " << p.diag_loss << "\nvertical_ratio " << p.vertical_ratio << "\noverflow_ms " << p.overflow_ms << "\n";
    for (const CurvePoint& c : p.curve) f << "curve " << c.stick << " " << c.frac << "\n";
    return (bool)f;
}
inline bool loadConfig(const std::string& path, Params& p) {
    std::ifstream f(path);
    if (!f) return false;
    Params out = p; out.curve.clear();
    std::string k;
    while (f >> k) {
        if (k == "curve") { CurvePoint c; if (!(f >> c.stick >> c.frac)) return false; out.curve.push_back(c); continue; }
        double v; if (!(f >> v)) return false;
        if (k == "deg_per_count") out.deg_per_count = v; else if (k == "wmax_deg_s") out.wmax_deg_s = v;
        else if (k == "deadzone") out.deadzone = v; else if (k == "window_ms") out.window_ms = v;
        else if (k == "y_scale") out.y_scale = v; else if (k == "vertical_ratio") out.vertical_ratio = v; else if (k == "axis_pull_deg") out.axis_pull_deg = v; else if (k == "diag_loss") out.diag_loss = v; else if (k == "overflow_ms") out.overflow_ms = v; else if (k == "invert_y") out.invert_y = v != 0;
    }
    p = out;
    return true;
}

} // namespace m2s
