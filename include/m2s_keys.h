// m2s_keys.h — turns held direction keys into a stick direction. Newest held key wins, and letting
// go falls back to whichever key is still down, so opposite keys never cancel you into standing still.
#pragma once
#include <cstdint>

namespace m2s {

class DirKeys {
public:
    enum { LeftNeg_X, LeftPos_X, LeftNeg_Y, LeftPos_Y, RightNeg_X, RightPos_X, RightNeg_Y, RightPos_Y, Count };
    void set(int key, bool down) { held_[key] = down; if (down) order_[key] = ++clock_; }
    // -1 if the negative key wins, +1 if the positive one does, 0 if neither is held.
    int axis(int neg, int pos) const {
        if (held_[neg] && held_[pos]) return order_[neg] > order_[pos] ? -1 : 1;
        return held_[neg] ? -1 : (held_[pos] ? 1 : 0);
    }
    void reset() { for (int i = 0; i < Count; i++) held_[i] = false; }
private:
    bool held_[Count] = {};
    uint64_t order_[Count] = {};
    uint64_t clock_ = 0;
};

} // namespace m2s
