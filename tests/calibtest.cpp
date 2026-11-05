// Runs the calibration against simulated games with known answers, using rendered pictures only.
#include <cstdio>
#include <cmath>
#include <deque>
#include <functional>
#include <random>
#include <vector>
#include "m2s_calib.h"
using namespace m2s;

struct Game {
    const char* name;
    double dz, wmax;
    std::function<double(double)> curve;   // 0..1 past the deadzone -> fraction of wmax
    double rampSec = 0;                    // >0: speed builds up while the stick is held
    double inputDelay = 0.03, videoDelay = 0.03;
    bool delaySpikes = false;
    double fov = 103;
    bool blank = false;
    double frac(double stick) const { if (stick <= dz) return 0; return curve((stick - dz) / (1 - dz)); }
    double rate(double stick) const { return frac(stick) * wmax; }
};

struct Pano {   // a varied 360 degree background
    std::vector<double> v; 
    Pano() { std::mt19937 g(11); std::normal_distribution<double> N(0, 1); int M = 3600; v.resize(M);
        for (double w : {6., 18., 60., 200.}) { std::vector<double> t(M); for (auto& x : t) x = N(g);
            int k = (int)w; for (int i = 0; i < M; i++) { double s = 0; for (int j = 0; j < k; j += std::max(1, k / 16)) s += t[(i + j) % M]; v[i] += s / std::sqrt(k) * 7; } } }
    double at(double deg) const { int M = v.size(); double x = std::fmod(deg, 360.0); if (x < 0) x += 360; double f = x * 10; int i = (int)f; double a = f - i; return 110 + v[i % M] * (1 - a) + v[(i + 1) % M] * a; }
};

static void render(const Pano& pa, double yaw, double t, std::vector<uint8_t>& img, int w, int h, std::mt19937& g, double fov, bool blank) {
    if (blank) { std::fill(img.begin(), img.end(), 90); return; }
    std::normal_distribution<double> N(0, 2.0);
    double f = (w / 2.0) / std::tan(fov / 2 * M_PI / 180);
    std::vector<uint8_t> row(w);
    for (int x = 0; x < w; x++) row[x] = (uint8_t)std::clamp(pa.at(yaw + std::atan((x - w / 2.0) / f) * 180 / M_PI), 0.0, 255.0);
    int cx = w / 2;
    for (int y = 0; y < h; y++) {
        uint8_t* r = &img[y * w];
        std::copy(row.begin(), row.end(), r);
        for (int x = 0; x < w; x += 8) { double n = N(g); r[x] = (uint8_t)std::clamp(r[x] + n, 0.0, 255.0); }
    }
    // static HUD inside the measured band: crosshair and an ability icon
    for (int y = h * 45 / 100; y < h * 55 / 100; y++) { for (int x = cx - 2; x < cx + 2; x++) img[y * w + x] = 255; for (int x = 20; x < 70; x++) img[y * w + x] = 230; }
    // a target walking around in the world
    double tdeg = 40 + 8 * std::sin(t * 1.3), dyaw = std::remainder(tdeg - yaw, 360.0);
    if (std::fabs(dyaw) < 45) { int tx = (int)(w / 2.0 + f * std::tan(dyaw * M_PI / 180)); for (int y = h * 35 / 100; y < h * 60 / 100; y++) for (int x = std::max(0, tx - 10); x < std::min(w, tx + 10); x++) img[y * w + x] = 200; }
}

static int fails = 0;
static void chk(const char* what, bool ok, double got, double want) { printf("   %s %-34s got %8.3f want %8.3f\n", ok ? "PASS" : "FAIL", what, got, want); if (!ok) fails++; }

static CalibResult runCalib(const Game& G, const Pano& pa) {
    AutoCalib c; std::mt19937 g(5); std::uniform_real_distribution<double> U(0, 1);
    const int w = 640, h = 360; std::vector<uint8_t> img(w * h);
    struct S { double at, v; }; std::deque<S> in; struct Y { double at, yaw; double t; }; std::deque<Y> vid;
    double t = 0, yaw = 0, cur = 0, heldFor = 0, nextFrame = 0, lastTick = 0, spikeUntil = 0; const double dt = 0.001;
    c.start(0);
    for (; t < 200; t += dt) {
        if (t - lastTick >= 0.004) { lastTick = t; double d = G.inputDelay; if (G.delaySpikes && t < spikeUntil) d += spikeUntil - t; in.push_back({t + d, c.stick(t)}); if (!c.active()) break; }
        if (G.delaySpikes && U(g) < 0.0004) spikeUntil = t + 0.06;
        while (!in.empty() && in.front().at <= t) { double v = in.front().v; if (std::fabs(v - cur) > 1e-6 && v == 0) heldFor = 0; cur = v; in.pop_front(); }
        if (t >= nextFrame) {                       // the game updates at 60 Hz
            nextFrame += 1 / 60.0; double r = G.rate(cur);
            if (G.rampSec > 0) { heldFor = cur > G.dz ? heldFor + 1 / 60.0 : 0; r *= std::min(1.0, 0.3 + 0.7 * heldFor / G.rampSec); }
            yaw += r / 60.0; vid.push_back({t + G.videoDelay, yaw, t});
        }
        while (!vid.empty() && vid.front().at <= t) { render(pa, vid.front().yaw, vid.front().t, img, w, h, g, G.fov, G.blank); c.onFrame(img.data(), w, h, w, t); vid.pop_front(); }
    }
    CalibResult r; c.takeFinished(r); printf("   finished after %.0f s simulated\n", t);
    return r;
}

// Hand makes stop-go movements; returns mean error per movement in degrees.
static double aimError(const Game& G, const Params& base, double* total = nullptr) {
    MouseToStick m; m.p = base; std::mt19937 g(9); std::uniform_real_distribution<double> U(0, 1); const double dt = 0.004; double err = 0; int N = 300;
    for (int b = 0; b < N; b++) { m.reset(); int len = 3 + (int)(U(g) * 40); double cps = 100 + U(g) * U(g) * 2500; double yaw = 0, ideal = 0, nf = U(g) / 60, t = 0, s = 0;
        for (int i = 0; i < 180; i++) { t += dt; if (i < len) { double c = cps * dt; m.addDelta(c, 0); ideal += c * m.p.deg_per_count; } s = m.tick(dt).x; while (t >= nf) { nf += 1 / 60.0; yaw += (s >= 0 ? 1 : -1) * G.rate(std::fabs(s)) / 60; } }
        err += std::fabs(yaw - ideal); }
    return err / N;
}

int main() {
    Pano pa;
    auto lin = [](double x) { return x; };
    auto dual = [](double x) { return x < 0.9 ? 0.25 * x / 0.9 : 0.25 + 0.75 * (x - 0.9) / 0.1; };
    auto expo = [](double x) { return (std::exp(3 * x) - 1) / (std::exp(3.0) - 1); };
    std::vector<Game> games = {
        {"Linear, deadzone 0.05, 400 deg/s", 0.05, 400, lin},
        {"Linear, no deadzone, 720 deg/s", 0.0, 720, lin},
        {"Dual Zone, deadzone 0.10, 500 deg/s", 0.10, 500, dual},
        {"Exponential, deadzone 0.15, 300 deg/s", 0.15, 300, expo},
        {"Linear with delay spikes", 0.05, 400, lin, 0, 0.03, 0.03, true},
        {"Linear, view is really 85 degrees", 0.05, 400, lin, 0, 0.03, 0.03, false, 85},
        {"Dual Zone, slow, 150 deg/s", 0.08, 150, dual},
    };
    for (const Game& G : games) {
        printf("%s\n", G.name);
        CalibResult r = runCalib(G, pa);
        if (!r.ok) { printf("   FAIL calibration failed: %s\n", r.message.c_str()); fails++; continue; }
        chk("deadzone", std::fabs(r.deadzone - G.dz) <= 0.015, r.deadzone, G.dz);
        chk("full stick deg/s (within 5%)", std::fabs(r.wmax_deg_s - G.wmax) <= G.wmax * 0.05, r.wmax_deg_s, G.wmax);
        chk("latency ms (within 40)", std::fabs(r.latency_ms - (G.inputDelay + G.videoDelay) * 1000) <= 40, r.latency_ms, (G.inputDelay + G.videoDelay) * 1000);
        double worst = 0; for (const auto& c : r.curve) worst = std::max(worst, std::fabs(c.frac - G.frac(c.stick)));
        chk("curve worst error (max 0.07)", worst <= 0.07, worst, 0);
        Params naive; naive.deadzone = 0.10; naive.wmax_deg_s = 360; Params cal; cal.deadzone = r.deadzone; cal.wmax_deg_s = r.wmax_deg_s; cal.curve = r.curve;
        double e0 = aimError(G, naive), e1 = aimError(G, cal);
        printf("   aim error per movement: uncalibrated %.2f deg, calibrated %.2f deg\n", e0, e1);
        chk("calibration improves aim", e1 < e0 * 0.5 || e1 < 0.5, e1, e0);
        chk("no false ramp warning", !r.ramp_detected, r.ramp_ratio, 1);
    }
    { Game G{"Ramp-up game", 0.05, 400, lin, 0.5}; printf("%s\n", G.name); CalibResult r = runCalib(G, pa); chk("ramp detected", r.ok && r.ramp_detected, r.ramp_ratio, 1.5); }
    { Game G{"Blank picture", 0.05, 400, lin}; G.blank = true; printf("%s\n", G.name); CalibResult r = runCalib(G, pa); printf("   message: %s\n", r.message.c_str()); chk("fails cleanly with a reason", !r.ok && !r.message.empty(), r.ok, 0); }
    { Params p; p.deadzone = 0.07; p.wmax_deg_s = 512; p.window_ms = 24; p.curve = {{0.07, 0}, {1.0, 1}}; bool s = saveConfig("/tmp/m2s_test.conf", p); Params q; bool l = loadConfig("/tmp/m2s_test.conf", q);
      printf("Settings file\n"); chk("save, load and match", s && l && q.deadzone == 0.07 && q.wmax_deg_s == 512 && q.window_ms == 24 && q.curve.size() == 2, q.wmax_deg_s, 512); }
    printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails ? 1 : 0;
}
