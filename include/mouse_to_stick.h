// mouse_to_stick.h — header-only, no dependencies. Portable (drops into chiaki-ng).
//
// Model: a stick commands a camera TURN RATE, a mouse commands a camera ANGLE. Assuming the game
// uses a linear response (Overwatch: Aim Technique = Linear Ramp, Aim Ease In = 0, Smoothing = 0):
//     rate = wmax * (|s| - dz) / (1 - dz)     for |s| > dz, otherwise 0
//
// So each mouse movement becomes an angle that is paid out at a constant rate over a short fixed
// window, then the stick returns to exactly zero:
//   1. counts -> angle:   angle = counts * deg_per_count
//   2. queue that angle and pay it evenly over window_ms (overlapping movements add up, so a
//      steady mouse speed gives a steady stick)
//   3. above full tilt, slow the payout down: the remainder stays queued, so flicks keep distance
//   4. add the game's inner deadzone back, so the game's rescale cancels it out
//
// The fixed window matters: an exponential payout leaves the stick resting at the deadzone edge for
// ~150 ms after the mouse stops, which turns any deadzone error into visible camera creep.
// Accuracy still depends on `deadzone` and `wmax_deg_s` matching the real game. Measure them.
#pragma once
#include <algorithm>
#include <cmath>
#include <deque>
#include <vector>

namespace m2s {

struct CurvePoint { double stick, frac; };   // measured: this stick value gives this fraction of full speed

struct Params {
    double deg_per_count = 0.06;    // mouse sensitivity: degrees of camera per mouse count
    double y_scale       = 1.0;     // mouse vertical relative to horizontal (1 = same, like a real mouse)
    double axis_pull_deg = 0;       // game pulls pushes toward the axes, growing steadily to this many degrees at full stick
    double diag_loss     = 0;       // game's speed loss on the diagonal, growing with the square of the push
    double vertical_ratio = 1.0;    // game's vertical speed relative to horizontal for the same stick
    double wmax_deg_s    = 360.0;   // camera deg/s at full stick in-game (measure with the test spin)
    double deadzone      = 0.10;    // game's inner deadzone, 0..1 (measure with the deadzone sweep)
    double overflow_ms   = 24.0;    // flick beyond full tilt: keep this much full-tilt turning, drop the rest
    double window_ms     = 32.0;    // payout window: longer = steadier, shorter = less delay
    double expo          = 1.0;     // output shaping; anything but 1.0 breaks 1:1 distance
    bool   quantize      = false;   // round to 8-bit steps with error carry (off: Chiaki sends 16-bit)
    bool   invert_y      = false;
    std::vector<CurvePoint> curve;  // measured response curve; empty = assume linear past the deadzone
};

struct Stick { double x = 0, y = 0; };   // each -1..1; +x right, +y up

class MouseToStick {
public:
    Params p;

    // Feed raw mouse counts (dy is screen-down positive). Thread safety is the caller's job.
    void addDelta(double dx, double dy) { acc_dx_ += dx; acc_dy_ += dy; }

    // Call at a fixed-ish rate (e.g. every 4 ms). dt in seconds. Returns the stick for this tick.
    Stick tick(double dt) {
        if (dt <= 1e-5) return last_;

        // 1: counts -> angle, queued for payout
        double ax = acc_dx_ * p.deg_per_count;
        double ay = acc_dy_ * p.deg_per_count * p.y_scale * (p.invert_y ? 1.0 : -1.0);
        acc_dx_ = acc_dy_ = 0;
        dropOverflowOnReversal(ax, ay);
        if (ax != 0 || ay != 0) q_.push_back({ax, ay, 1.0});
        limitQueue();

        // 2: pay each queued movement evenly over the window
        int n = std::max(1, (int)std::lround(p.window_ms * 1e-3 / dt));
        double frac = 1.0 / n, paid_x = 0, paid_y = 0;
        for (const Entry& e : q_) {
            double f = std::min(e.left, frac);
            paid_x += e.x * f; paid_y += e.y * f;
        }

        // 3: fraction of full tilt; above 1 the payout slows and the rest stays queued
        double nx = paid_x / dt / p.wmax_deg_s, ny = paid_y / dt / (p.wmax_deg_s * p.vertical_ratio);
        double mag = std::hypot(nx, ny);
        double scale = mag > 1.0 ? 1.0 / mag : 1.0;
        for (Entry& e : q_) e.left -= std::min(e.left, frac) * scale;
        while (!q_.empty() && q_.front().left < 1e-9) q_.pop_front();
        nx *= scale; ny *= scale; mag = std::min(mag, 1.0);

        // 4: add the deadzone back; no queued movement means exactly zero stick
        Stick s;
        if (mag > 1e-9) {
            double shaped = p.expo != 1.0 ? std::pow(mag, p.expo) : mag;
            double out = p.curve.size() >= 2 ? stickForFraction(shaped)
                                             : std::min(p.deadzone + (1.0 - p.deadzone) * shaped, 1.0);
            s.x = nx / mag * out; s.y = ny / mag * out;
            if (p.axis_pull_deg != 0 || p.diag_loss != 0) {
                // Aim away from the axes by what the game will pull back, and make up its diagonal loss.
                double th = std::atan2(std::fabs(s.y), std::fabs(s.x)), r2 = out * out;
                double loss = 1.0 - p.diag_loss * r2 * std::fabs(std::sin(2 * th));
                double aim = aimForDirection(th * 180 / M_PI, p.axis_pull_deg * out) * M_PI / 180;
                double r = std::min(out / std::max(loss, 0.5), 1.0);
                s.x = std::copysign(r * std::cos(aim), s.x); s.y = std::copysign(r * std::sin(aim), s.y);
            }
        }
        if (p.quantize) { s.x = quant(s.x, qerr_x_); s.y = quant(s.y, qerr_y_); }
        last_ = s;
        return s;
    }

    void reset() { acc_dx_ = acc_dy_ = qerr_x_ = qerr_y_ = 0; q_.clear(); last_ = {}; }

    // What a linear game would do with a given stick: camera deg/s per axis (simulation and tests).
    static void gameRate(const Params& p, Stick s, double& wx, double& wy) {
        double r = std::hypot(s.x, s.y);
        if (r <= p.deadzone) { wx = wy = 0; return; }
        double m = (r - p.deadzone) / (1.0 - p.deadzone);
        wx = s.x / r * m * p.wmax_deg_s;
        wy = s.y / r * m * p.wmax_deg_s * p.vertical_ratio;
    }

    double backlogX() const { double v = 0; for (const Entry& e : q_) v += e.x * e.left; return v; }
    double backlogY() const { double v = 0; for (const Entry& e : q_) v += e.y * e.left; return v; }

private:
public:
    // How the game bends a stick direction: strong pushes lose A degrees between 15 and 30 degrees from
    // an axis, fading to none on the axes and exactly on the diagonal. Measured, first quadrant, degrees.
    static double gameDirection(double aim, double A) {
        static const double k[7] = {0, 15, 30, 45, 60, 75, 90}, d[7] = {0, -1, -1, 0, 1, 1, 0};
        for (int i = 1; i < 7; i++) if (aim <= k[i]) {
            double u = (aim - k[i - 1]) / 15;
            return aim + A * (d[i - 1] + (d[i] - d[i - 1]) * u);
        }
        return 90;
    }
    // The stick direction to send so the game ends up going `want`.
    static double aimForDirection(double want, double A) {
        A = std::min(A, 14.0);    // keeps the map one-to-one
        double lo = 0, hi = 90;
        for (int i = 0; i < 30; i++) { double m = (lo + hi) / 2; if (gameDirection(m, A) < want) lo = m; else hi = m; }
        return (lo + hi) / 2;
    }
private:
    // Inverse of the measured curve: which stick value gives this fraction of full speed?
    double stickForFraction(double f) const {
        const auto& c = p.curve;
        if (f <= c.front().frac) return c.front().stick;
        for (size_t i = 1; i < c.size(); i++) {
            if (f <= c[i].frac) {
                double span = c[i].frac - c[i - 1].frac;
                double a = span > 1e-9 ? (f - c[i - 1].frac) / span : 1.0;
                return c[i - 1].stick + (c[i].stick - c[i - 1].stick) * a;
            }
        }
        return c.back().stick;
    }

    struct Entry { double x, y, left; };   // queued angle (deg) and the fraction still unpaid

    // Your hand always wins: reversing against queued flick overflow discards it, so the stick
    // never keeps pushing the old way. Normal slow aim (queue within one window) is left alone.
    void dropOverflowOnReversal(double ax, double ay) {
        double normal = p.wmax_deg_s * p.window_ms * 1e-3;
        double bx = backlogX(), by = backlogY();
        if (ax * bx < 0 && std::fabs(bx) > normal) for (Entry& e : q_) e.x = 0;
        if (ay * by < 0 && std::fabs(by) > normal) for (Entry& e : q_) e.y = 0;
    }
    // Anything beyond a short burst of full tilt is dropped: a stick physically can't turn faster.
    void limitQueue() {
        double cap = p.wmax_deg_s * (p.window_ms + p.overflow_ms) * 1e-3;
        double m = std::hypot(backlogX(), backlogY());
        if (m > cap) {
            double k = cap / m;
            for (Entry& e : q_) { e.x *= k; e.y *= k; }
        }
    }
    static double quant(double v, double& err) {
        double scaled = (v + 1.0) * 127.5 + err;      // 0..255
        double q = std::round(scaled);
        err = scaled - q;
        q = std::clamp(q, 0.0, 255.0);
        return q / 127.5 - 1.0;
    }
    double acc_dx_ = 0, acc_dy_ = 0, qerr_x_ = 0, qerr_y_ = 0;
    std::deque<Entry> q_;
    Stick last_;
};

} // namespace m2s
