// Checks the 2D tracker on a synthetic moving picture with a static HUD, swaying gun and noise.
#include <cstdio>
#include <cmath>
#include <random>
#include <vector>
#include "m2s_track2d.h"
using namespace m2s;
static double tex(double x, double y) {
    return 110 + 40 * std::sin(x * .031 + 1.3 * std::sin(y * .017)) + 30 * std::sin(y * .043 + x * .011) + 25 * std::sin(x * .083 - y * .067) + 14 * std::sin(x * .19 + y * .13);
}
int main() {
    const int w = 960, h = 540; std::vector<uint8_t> img(w * h); std::mt19937 g(3); std::normal_distribution<double> N(0, 1.5);
    int fails = 0; Tracker2D* t = new Tracker2D;
    auto run = [&](const char* name, double (*fx)(double), double (*fy)(double), int frames) {
        t->reset(); double cx = 0, cy = 0;
        for (int f = 0; f < frames; f++) {
            cx = fx(f / 60.0); cy = fy(f / 60.0);
            for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) img[y * w + x] = (uint8_t)std::clamp(tex(x - cx, y - cy) + N(g) * ((x % 7) == 0), 0.0, 255.0);
            for (int y = 500; y < 530; y++) for (int x = 30; x < 300; x++) img[y * w + x] = 240;                                  // HUD
            int sway = (int)(6 * std::sin(f * .1)); for (int y = 300; y < 540; y++) for (int x = 530 + sway; x < 800 + sway; x++) img[y * w + x] = 60;   // gun
            t->feed(img.data(), w, h, w);
        }
        double wantX = (cx - fx(0)) / 3, wantY = (cy - fy(0)) / 3;   // 960 wide -> 320 wide
        double tol = std::max(1.5, 0.007 * std::hypot(wantX, wantY));
        bool ok = std::fabs(t->x() - wantX) < tol && std::fabs(t->y() - wantY) < tol && t->bad() == 0;
        printf("%s %-22s x %8.2f want %8.2f | y %8.2f want %8.2f | lost %d\n", ok ? "PASS" : "FAIL", name, t->x(), wantX, t->y(), wantY, t->bad());
        if (!ok) fails++;
    };
    run("still", [](double) { return 0.; }, [](double) { return 0.; }, 60);
    run("slow creep", [](double s) { return s * 12; }, [](double s) { return -s * 7; }, 60);
    run("fast horizontal", [](double s) { return s * 2500; }, [](double) { return 0.; }, 40);
    run("fast diagonal", [](double s) { return s * 1800; }, [](double s) { return s * 1100; }, 40);
    run("chaotic curve", [](double s) { return 400 * std::sin(s * 5) + 200 * std::sin(s * 11.3); }, [](double s) { return 150 * std::sin(s * 7.1) + 90 * std::cos(s * 13); }, 120);
    printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails ? 1 : 0;
}
