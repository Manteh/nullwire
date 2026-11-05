#import <Cocoa/Cocoa.h>
#import <GameController/GameController.h>
#include <atomic>
#include <mutex>
#include <sys/time.h>
#include "macMouseBridge.h"

static std::mutex g_mtx;
static double g_dx = 0, g_dy = 0;
static std::atomic<unsigned> g_buttons{0};
static std::atomic<unsigned> g_events{0};
static std::atomic<bool> g_captured{false};
static std::atomic<bool> g_ignoreReal{false};   // test channel owns the input: real mouse is ignored
static bool g_started = false;
static std::atomic<bool> g_gcActive{false};   // true once GCMouse actually delivers deltas

static void setBit(unsigned bit, bool down) {
    if (down) g_buttons |= (1u << bit); else g_buttons &= ~(1u << bit);
}
static void bind(GCMouse *m) {
    GCMouseInput *mi = m.mouseInput;
    mi.mouseMovedHandler = ^(GCMouseInput *, float dx, float dy) {
        if (!g_captured || g_ignoreReal) return;
        if (!g_gcActive) fprintf(stderr, "[m2s] input path: GCMouse (raw, unaccelerated) is delivering\n");
        g_gcActive = true;
        g_events++;
        {   // a mouse that was moving and suddenly goes quiet, then resumes: log it, a person can't stop that sharply often
            static double last = 0; static int run = 0;
            timeval tv; gettimeofday(&tv, nullptr); double now = tv.tv_sec + tv.tv_usec * 1e-6, gap = now - last;
            if (run >= 40 && gap > 0.12 && gap < 2.0) fprintf(stderr, "[m2s] MOUSE QUIET %.0f ms, resumed at %.3f\n", gap * 1000, now);
            run = gap < 0.03 ? run + 1 : 0; last = now;
        }
        std::lock_guard<std::mutex> l(g_mtx);
        g_dx += dx; g_dy += -dy;       // GCMouse +y is up; flip to screen-down
    };
    mi.leftButton.pressedChangedHandler   = ^(GCControllerButtonInput *, float, BOOL p) { setBit(0, p && g_captured && !g_ignoreReal); };
    mi.rightButton.pressedChangedHandler  = ^(GCControllerButtonInput *, float, BOOL p) { setBit(1, p && g_captured && !g_ignoreReal); };
    mi.middleButton.pressedChangedHandler = ^(GCControllerButtonInput *, float, BOOL p) { setBit(2, p && g_captured && !g_ignoreReal); };
    NSArray<GCControllerButtonInput *> *aux = mi.auxiliaryButtons;
    if (aux.count > 0) aux[0].pressedChangedHandler = ^(GCControllerButtonInput *, float, BOOL p) { setBit(3, p && g_captured && !g_ignoreReal); };
    if (aux.count > 1) aux[1].pressedChangedHandler = ^(GCControllerButtonInput *, float, BOOL p) { setBit(4, p && g_captured && !g_ignoreReal); };
}

namespace MacMouse {
void start() {
    if (g_started) return;
    g_started = true;
    fprintf(stderr, "[m2s] GCMouse devices at start: %lu\n", (unsigned long)GCMouse.mice.count);
    for (GCMouse *m in GCMouse.mice) bind(m);
    // Fallback / primary: AppKit events (works for unbundled builds). Swallow mouse events while captured
    // so Chiaki doesn't treat clicks as touchpad presses.
    NSEventMask mask = NSEventMaskMouseMoved | NSEventMaskLeftMouseDragged | NSEventMaskRightMouseDragged | NSEventMaskOtherMouseDragged |
        NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp | NSEventMaskRightMouseDown | NSEventMaskRightMouseUp |
        NSEventMaskOtherMouseDown | NSEventMaskOtherMouseUp;
    [NSEvent addLocalMonitorForEventsMatchingMask:mask handler:^NSEvent *(NSEvent *e) {
        if (!g_captured || g_ignoreReal) return e;
        switch (e.type) {
            case NSEventTypeMouseMoved: case NSEventTypeLeftMouseDragged: case NSEventTypeRightMouseDragged: case NSEventTypeOtherMouseDragged:
                if (!g_gcActive) { static bool said = false; if (!said) { said = true; fprintf(stderr, "[m2s] input path: NSEvent (macOS-accelerated!)\n"); }
                    g_events++; std::lock_guard<std::mutex> l(g_mtx); g_dx += e.deltaX; g_dy += e.deltaY; }
                break;
            case NSEventTypeLeftMouseDown:  setBit(0, true);  break;
            case NSEventTypeLeftMouseUp:    setBit(0, false); break;
            case NSEventTypeRightMouseDown: setBit(1, true);  break;
            case NSEventTypeRightMouseUp:   setBit(1, false); break;
            case NSEventTypeOtherMouseDown: setBit(e.buttonNumber == 2 ? 2 : (e.buttonNumber == 3 ? 3 : 4), true);  break;
            case NSEventTypeOtherMouseUp:   setBit(e.buttonNumber == 2 ? 2 : (e.buttonNumber == 3 ? 3 : 4), false); break;
            default: break;
        }
        return nil;
    }];
    [[NSNotificationCenter defaultCenter] addObserverForName:GCMouseDidConnectNotification object:nil queue:nil
        usingBlock:^(NSNotification *n) { bind(n.object); }];
}
void setCapture(bool on) {
    if (on == g_captured) return;
    g_captured = on;
    dispatch_async(dispatch_get_main_queue(), ^{
        CGAssociateMouseAndMouseCursorPosition(on ? false : true);
        if (on) CGDisplayHideCursor(kCGDirectMainDisplay); else CGDisplayShowCursor(kCGDirectMainDisplay);
    });
    if (!on) { g_buttons = 0; std::lock_guard<std::mutex> l(g_mtx); g_dx = g_dy = 0; }
}
bool captured() { return g_captured; }
void takeDeltas(double &dx, double &dy) {
    std::lock_guard<std::mutex> l(g_mtx);
    dx = g_dx; dy = g_dy; g_dx = g_dy = 0;
}
void setHeadlessCapture(bool on) { g_ignoreReal = on; g_captured = on; if (on) g_buttons = 0; }
void inject(double dx, double dy) { std::lock_guard<std::mutex> l(g_mtx); g_dx += dx; g_dy += dy; }
void setTestButtons(unsigned bits) { g_buttons = bits; }
void beep(int times) {
    for (int i = 0; i < times; i++)
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(i * 0.35 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{ NSBeep(); });
}
unsigned takeEventCount() { return g_events.exchange(0); }
unsigned buttons() { return g_buttons; }
}
