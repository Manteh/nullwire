// m2s_track2d.h — measures how far the picture slid in both axes, for testing real camera motion.
// Coarse-to-fine block matching against a held keyframe. Ignores the HUD and the held weapon.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace m2s {

class Tracker2D {
public:
    static constexpr int W = 320, H = 180, CW = W / 4, CH = H / 4;

    void reset() { haveKey_ = false; totX_ = totY_ = offX_ = offY_ = velX_ = velY_ = 0; frames_ = bad_ = 0; }

    // Total slide in pixels of a 320-wide picture since reset.
    double x() const { return totX_ + offX_; }
    double y() const { return totY_ + offY_; }
    int frames() const { return frames_; }
    int bad() const { return bad_; }

    void feed(const uint8_t* src, int w, int h, int stride, int step = 1, int off = 0) {
        for (int y = 0; y < H; y++) {
            const uint8_t* r = src + (int)((y + 0.5) * h / H) * stride;
            for (int x = 0; x < W; x++) cur_[y * W + x] = r[(int)((x + 0.5) * w / W) * step + off];
        }
        for (int y = 0; y < CH; y++) for (int x = 0; x < CW; x++) {
            int t = 0;
            for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) t += cur_[(y * 4 + j) * W + x * 4 + i];
            curC_[y * CW + x] = (uint8_t)(t / 16);
        }
        frames_++;
        if (!haveKey_) { setKey(); haveKey_ = true; return; }

        // coarse search around where the motion so far predicts the picture will be
        int px = (int)std::lround((offX_ + velX_) / 4), py = (int)std::lround((offY_ + velY_) / 4);
        long best = -1; long sum = 0; int n = 0, bx = 0, by = 0;
        for (int dy = py - 12; dy <= py + 12; dy++) for (int dx = px - 16; dx <= px + 16; dx++) {
            long v = sadCoarse(dx, dy); sum += v; n++;
            if (best < 0 || v < best) { best = v; bx = dx; by = dy; }
        }
        if (n == 0 || best > (sum / n) * 0.75) { bad_++; return; }

        // fine search at full resolution, then sub-pixel
        long fb = -1; int fx = bx * 4, fy = by * 4;
        for (int dy = by * 4 - 4; dy <= by * 4 + 4; dy++) for (int dx = bx * 4 - 4; dx <= bx * 4 + 4; dx++) {
            long v = sadFine(dx, dy);
            if (fb < 0 || v < fb) { fb = v; fx = dx; fy = dy; }
        }
        double ox = fx + parabola(sadFine(fx - 1, fy), fb, sadFine(fx + 1, fy));
        double oy = fy + parabola(sadFine(fx, fy - 1), fb, sadFine(fx, fy + 1));
        velX_ = ox - offX_; velY_ = oy - offY_;
        offX_ = ox; offY_ = oy;
        if (std::hypot(ox, oy) >= 14) { totX_ += ox; totY_ += oy; offX_ = offY_ = 0; setKey(); }
    }

private:
    static bool masked(int x, int y, int w, int h) {   // HUD strips and the weapon in the lower right
        // Stay near the center: a turn slides the edges of a flat picture faster than the middle.
        return x < w * 28 / 100 || x > w * 72 / 100 || y < h * 9 / 100 || y > h * 78 / 100 || (x > w / 2 && y > h / 2);
    }
    void setKey() { std::copy(cur_, cur_ + W * H, key_); std::copy(curC_, curC_ + CW * CH, keyC_); }
    long sadCoarse(int dx, int dy) const {
        long t = 0; int n = 0;
        for (int y = 0; y < CH; y++) { int ny = y + dy; if (ny < 0 || ny >= CH) continue;
            for (int x = 0; x < CW; x++) { int nx = x + dx; if (nx < 0 || nx >= CW) continue;
                if (masked(x, y, CW, CH) || masked(nx, ny, CW, CH)) continue;
                t += std::abs(keyC_[y * CW + x] - curC_[ny * CW + nx]); n++; } }
        return n < CW * CH / 6 ? (1L << 40) : t * 4096 / n;
    }
    long sadFine(int dx, int dy) const {
        long t = 0; int n = 0;
        for (int y = 0; y < H; y += 2) { int ny = y + dy; if (ny < 0 || ny >= H) continue;
            for (int x = 0; x < W; x += 2) { int nx = x + dx; if (nx < 0 || nx >= W) continue;
                if (masked(x, y, W, H) || masked(nx, ny, W, H)) continue;
                t += std::abs(key_[y * W + x] - cur_[ny * W + nx]); n++; } }
        return n < W * H / 24 ? (1L << 40) : t * 4096 / n;
    }
    static double parabola(long l, long c, long r) {
        double d = (double)l - 2.0 * c + r;
        return d > 1e-9 ? std::clamp(0.5 * (l - r) / d, -0.5, 0.5) : 0;
    }
    uint8_t cur_[W * H], key_[W * H], curC_[CW * CH], keyC_[CW * CH];
    bool haveKey_ = false;
    double totX_ = 0, totY_ = 0, offX_ = 0, offY_ = 0, velX_ = 0, velY_ = 0;
    int frames_ = 0, bad_ = 0;
};

} // namespace m2s
