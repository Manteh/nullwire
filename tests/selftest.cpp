// Headless sanity test: simulated camera rotation must track ideal mouse rotation.
#include <cstdio>
#include <cmath>
#include "mouse_to_stick.h"
#include "m2s_keys.h"
using namespace m2s;

static int fails = 0;
static void check(const char* name, bool ok, double a, double b) {
    printf("%s %-34s sim=%9.3f ideal=%9.3f\n", ok ? "PASS" : "FAIL", name, a, b);
    if (!ok) fails++;
}

// Run a mouse path (counts per tick for n ticks) at 250Hz and return simulated yaw.
static double run(MouseToStick& m, double countsPerTick, int ticks, int tailTicks, double& ideal) {
    const double dt = 0.004; double yaw = 0; ideal = 0;
    for (int i = 0; i < ticks + tailTicks; i++) {
        if (i < ticks) { m.addDelta(countsPerTick, 0); ideal += countsPerTick * m.p.deg_per_count; }
        Stick s = m.tick(dt);
        double wx, wy; MouseToStick::gameRate(m.p, s, wx, wy);
        yaw += wx * dt;
    }
    return yaw;
}

int main() {
    double ideal;
    { MouseToStick m; double y = run(m, 1, 250, 50, ideal);        check("slow creep 1 count/tick", std::fabs(y - ideal) < 0.05, y, ideal); }
    { MouseToStick m; double y = run(m, 10, 100, 50, ideal);       check("medium sweep 10 count/tick", std::fabs(y - ideal) < 0.1, y, ideal); }
    { MouseToStick m; double y = run(m, 100, 3, 100, ideal);       check("flick within the overflow cap", std::fabs(y - ideal) < 0.2, y, ideal); }
    { MouseToStick m; for (int i = 0; i < 15; i++) { m.addDelta(160, 0); m.tick(0.004); }   // hard flick right
      int leftAt = -1; for (int i = 0; i < 100 && leftAt < 0; i++) { m.addDelta(-12, 0); if (m.tick(0.004).x < 0) leftAt = i; }
      check("reversal after flick answers (ms)", leftAt >= 0 && leftAt * 4 <= 8, leftAt * 4.0, 8); }
    { MouseToStick m; for (int i = 0; i < 15; i++) { m.addDelta(160, 0); m.tick(0.004); }   // hard flick, then hand stops
      int on = 0; for (int i = 0; i < 200; i++) if (m.tick(0.004).x != 0) on++;
      check("stick stops soon after a flick (ms)", on * 4 <= 64, on * 4.0, 64); }
    { MouseToStick m; m.p.quantize = true; double y = run(m, 2, 250, 50, ideal); check("8-bit quantized", std::fabs(y - ideal) < 0.3, y, ideal); }
    { MouseToStick m; m.p.window_ms = 48; double y = run(m, 10, 100, 200, ideal); check("longer window", std::fabs(y - ideal) < 0.1, y, ideal); }
    { MouseToStick m; m.p.window_ms = 16; m.addDelta(50, 0); m.tick(0.004); int on = 1; for (int i = 0; i < 100; i++) { Stick s = m.tick(0.004); if (s.x != 0) on++; }
      check("stick stops within the window (ms)", on * 4 <= 20, on * 4.0, 16); }
    { MouseToStick m; double yaw = 0, last = 0; const double T = 1.0 / 60; double t = 0, next = T; Stick s; double ideal2 = 0;
      for (int i = 0; i < 2000; i++) { if (i % 50 < 12) { m.addDelta(8, 0); ideal2 += 8 * m.p.deg_per_count; } s = m.tick(0.004); t += 0.004;
        while (t >= next) { next += T; double wx, wy; MouseToStick::gameRate(m.p, s, wx, wy); yaw += wx * T; } }
      (void)last; check("stop-go, game samples at 60 Hz", std::fabs(yaw - ideal2) < ideal2 * 0.03, yaw, ideal2); }
    { MouseToStick m; Stick s = m.tick(0.004); check("idle = zero stick", s.x == 0 && s.y == 0, s.x, 0); }
    { MouseToStick m; m.addDelta(1, 0); Stick s = m.tick(0.004);
      check("1-count nudge clears deadzone", s.x > m.p.deadzone, s.x, m.p.deadzone); }
    { MouseToStick m; m.addDelta(5000, 5000); Stick s = m.tick(0.004);
      check("diagonal stays in unit circle", std::hypot(s.x, s.y) <= 1.0 + 1e-9, std::hypot(s.x, s.y), 1.0); }
    { // A game that pulls strong pushes toward the axes: the correction must land on the asked direction.
      for (double ang : {15., 30., 60., 75.}) { MouseToStick m; m.p.deadzone = 0.1; m.p.axis_pull_deg = 10.7; m.p.diag_loss = 0.06;
        double a = ang * M_PI / 180, c = 40 * std::cos(a), sn = 40 * std::sin(a); Stick s;
        for (int i = 0; i < 12; i++) { m.addDelta(c, -sn); s = m.tick(0.004); }
        double th = std::atan2(s.y, s.x), r = std::hypot(s.x, s.y), got = MouseToStick::gameDirection(th * 180 / M_PI, 10.7 * r) * M_PI / 180;
        char nm[64]; snprintf(nm, 64, "axis pull corrected, %.0f deg", ang); check(nm, std::fabs(got * 180 / M_PI - ang) < 1.0, got * 180 / M_PI, ang); } }
    { DirKeys k; const int A = DirKeys::LeftNeg_X, D = DirKeys::LeftPos_X, W = DirKeys::LeftNeg_Y, S = DirKeys::LeftPos_Y;
      k.set(D, true);  check("hold d: moves right", k.axis(A, D) == 1, k.axis(A, D), 1);
      k.set(A, true);  check("add a: newest wins, left", k.axis(A, D) == -1, k.axis(A, D), -1);
      k.set(A, false); check("release a: d still works (the bug)", k.axis(A, D) == 1, k.axis(A, D), 1);
      k.set(A, true);  k.set(D, false); check("release d: a still works", k.axis(A, D) == -1, k.axis(A, D), -1);
      k.set(A, false); check("release all: stops", k.axis(A, D) == 0, k.axis(A, D), 0);
      k.set(W, true); k.set(D, true); k.set(D, false); check("w+d, release d: still walking", k.axis(W, S) == -1, k.axis(W, S), -1);
      k.set(S, true); k.set(W, false); check("w to s roll: goes back", k.axis(W, S) == 1, k.axis(W, S), 1); }
    printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
    return fails ? 1 : 0;
}
