// Dumps what the real engine does in two situations, so the README figures are drawn from the code itself.
#include <cstdio>
#include "mouse_to_stick.h"
using namespace m2s;
int main() {
    const double dt = 0.004;
    { // a small, quick hand movement: 6 degrees in 60 ms, then the hand stops
        MouseToStick m; m.p.window_ms = 24; m.p.deadzone = 0.10;
        for (int i = 0; i < 70; i++) { if (i < 15) m.addDelta(6.0 / m.p.deg_per_count / 15, 0); printf("payout %.3f %.4f\n", i * dt * 1000, m.tick(dt).x); }
    }
    for (double overflow : {24.0, 300.0}) { // a big flick: 90 degrees in 150 ms
        MouseToStick m; m.p.window_ms = 24; m.p.deadzone = 0.10; m.p.overflow_ms = overflow;
        int n = 38; double ang = 0;
        for (int i = 0; i < 150; i++) {
            if (i < n) m.addDelta(90.0 / m.p.deg_per_count / n, 0);
            double x = m.tick(dt).x; MouseToStick::gameRate(m.p, {x, 0}, *new double, *new double);
            double r = x > m.p.deadzone ? (x - m.p.deadzone) / (1 - m.p.deadzone) * m.p.wmax_deg_s : 0; ang += r * dt;
            printf("flick%.0f %.3f %.4f %.4f\n", overflow, i * dt * 1000, x, ang);
        }
    }
}
