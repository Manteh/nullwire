// m2s_test.mm — live test harness for mouse_to_stick.h. No PS5 / Chiaki needed.
// Build: ./build.sh     Run: ./m2s_test
#import <Cocoa/Cocoa.h>
#import <GameController/GameController.h>
#include <mach/mach_time.h>
#include <vector>
#include "mouse_to_stick.h"

static m2s::MouseToStick g_m;
static m2s::Stick g_stick;
static double g_yaw = 0, g_pitch = 0;           // simulated camera from what the "game" would do
static double g_idealYaw = 0, g_idealPitch = 0; // what a perfect 1:1 mouse would have done
static double g_rawX = 0, g_rawY = 0;           // total counts
static bool g_captured = false, g_usingGC = false;
static double g_tickHz = 0;
static std::vector<NSPoint> g_trail;

static double nowSec() {
    static mach_timebase_info_data_t tb; if (!tb.denom) mach_timebase_info(&tb);
    return (double)mach_absolute_time() * tb.numer / tb.denom * 1e-9;
}
// dy here is "screen down positive" (NSEvent convention)
static void feed(double dx, double dy) {
    g_m.addDelta(dx, dy);
    g_rawX += dx; g_rawY += dy;
    g_idealYaw += dx * g_m.p.deg_per_count;
    g_idealPitch += -dy * g_m.p.deg_per_count * g_m.p.y_scale * (g_m.p.invert_y ? -1 : 1);
}

@interface StickView : NSView @end
@implementation StickView
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)isFlipped { return NO; }
- (void)setCapture:(BOOL)on {
    if (on == g_captured) return;
    g_captured = on;
    CGAssociateMouseAndMouseCursorPosition(on ? false : true);
    if (on) CGDisplayHideCursor(kCGDirectMainDisplay); else CGDisplayShowCursor(kCGDirectMainDisplay);
}
- (void)mouseDown:(NSEvent*)e { [self setCapture:YES]; }
- (void)mouseMoved:(NSEvent*)e { if (g_captured && !g_usingGC) feed(e.deltaX, e.deltaY); }
- (void)mouseDragged:(NSEvent*)e { [self mouseMoved:e]; }
- (void)keyDown:(NSEvent*)e {
    m2s::Params& p = g_m.p;
    NSString* c = e.charactersIgnoringModifiers;
    unichar k = c.length ? [c characterAtIndex:0] : 0;
    switch (k) {
        case 27: [self setCapture:NO]; break;                       // Esc
        case NSUpArrowFunctionKey:    p.deg_per_count *= 1.05; break;
        case NSDownArrowFunctionKey:  p.deg_per_count /= 1.05; break;
        case NSRightArrowFunctionKey: p.wmax_deg_s += 20; break;
        case NSLeftArrowFunctionKey:  p.wmax_deg_s = std::max(60.0, p.wmax_deg_s - 20); break;
        case '[': p.deadzone = std::max(0.0, p.deadzone - 0.01); break;
        case ']': p.deadzone = std::min(0.4, p.deadzone + 0.01); break;
        case ',': p.expo = std::max(0.5, p.expo - 0.05); break;
        case '.': p.expo = std::min(2.0, p.expo + 0.05); break;
        case 's': p.window_ms = p.window_ms > 20 ? 16 : 32; break;
        case 'q': p.quantize = !p.quantize; break;
        case 'y': p.invert_y = !p.invert_y; break;
        case 'r': g_yaw = g_pitch = g_idealYaw = g_idealPitch = g_rawX = g_rawY = 0; g_m.reset(); break;
    }
}
- (void)drawText:(NSString*)s at:(NSPoint)pt color:(NSColor*)col size:(CGFloat)sz {
    NSDictionary* a = @{NSFontAttributeName:[NSFont monospacedSystemFontOfSize:sz weight:NSFontWeightRegular],
                        NSForegroundColorAttributeName:col};
    [s drawAtPoint:pt withAttributes:a];
}
- (void)drawRect:(NSRect)r {
    [[NSColor colorWithWhite:0.08 alpha:1] setFill]; NSRectFill(self.bounds);
    const m2s::Params& p = g_m.p;
    NSPoint c = NSMakePoint(190, NSHeight(self.bounds) - 230); CGFloat R = 150;

    // stick gate, deadzone ring, trail, dot
    [[NSColor colorWithWhite:0.35 alpha:1] setStroke];
    NSBezierPath* gate = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(c.x-R, c.y-R, 2*R, 2*R)]; gate.lineWidth = 2; [gate stroke];
    [[NSColor colorWithRed:0.9 green:0.3 blue:0.3 alpha:0.8] setStroke];
    CGFloat dr = R * p.deadzone;
    NSBezierPath* dz = [NSBezierPath bezierPathWithOvalInRect:NSMakeRect(c.x-dr, c.y-dr, 2*dr, 2*dr)]; [dz stroke];
    for (size_t i = 0; i < g_trail.size(); i++) {
        CGFloat a = (CGFloat)(i + 1) / g_trail.size();
        [[NSColor colorWithRed:0.3 green:0.8 blue:1 alpha:a*0.5] setFill];
        NSPoint t = g_trail[i];
        NSRectFill(NSMakeRect(c.x + t.x*R - 2, c.y + t.y*R - 2, 4, 4));
    }
    [[NSColor colorWithRed:0.3 green:1 blue:0.5 alpha:1] setFill];
    [[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(c.x + g_stick.x*R - 7, c.y + g_stick.y*R - 7, 14, 14)] fill];

    NSColor* w = [NSColor whiteColor]; NSColor* g = [NSColor colorWithWhite:0.6 alpha:1];
    CGFloat X = 400, Y = NSHeight(self.bounds) - 40;
    [self drawText:(g_captured ? @"CAPTURED — Esc to release" : @"Click window to capture mouse") at:NSMakePoint(20, NSHeight(self.bounds)-30) color:(g_captured ? [NSColor greenColor] : [NSColor yellowColor]) size:13];
    [self drawText:[NSString stringWithFormat:@"input: %@   tick: %.0f Hz", g_usingGC ? @"GCMouse (raw, no accel)" : @"NSEvent (has OS accel!)", g_tickHz] at:NSMakePoint(X, Y) color:g size:12];
    double wx, wy; m2s::MouseToStick::gameRate(p, g_stick, wx, wy);
    NSArray* lines = @[
        [NSString stringWithFormat:@"stick        x %+.3f   y %+.3f   |r| %.3f", g_stick.x, g_stick.y, hypot(g_stick.x, g_stick.y)],
        [NSString stringWithFormat:@"game turn    yaw %+7.1f deg/s  pitch %+7.1f deg/s", wx, wy],
        [NSString stringWithFormat:@"backlog      x %+.2f deg   y %+.2f deg", g_m.backlogX(), g_m.backlogY()],
        @"",
        [NSString stringWithFormat:@"yaw   sim %+9.2f   ideal %+9.2f   err %+.2f deg", g_yaw, g_idealYaw, g_yaw - g_idealYaw],
        [NSString stringWithFormat:@"pitch sim %+9.2f   ideal %+9.2f   err %+.2f deg", g_pitch, g_idealPitch, g_pitch - g_idealPitch],
        [NSString stringWithFormat:@"raw counts  x %+.0f  y %+.0f", g_rawX, g_rawY],
        @"",
        [NSString stringWithFormat:@"[up/down]    sens        %.4f deg/count", p.deg_per_count],
        [NSString stringWithFormat:@"[left/right] wmax        %.0f deg/s at full stick  (CALIBRATE)", p.wmax_deg_s],
        [NSString stringWithFormat:@"[ ]          deadzone    %.2f  (match in-game inner deadzone)", p.deadzone],
        [NSString stringWithFormat:@", .          expo        %.2f  (1.00 = pure 1:1)", p.expo],
        [NSString stringWithFormat:@"s            window      %.0f ms", p.window_ms],
        [NSString stringWithFormat:@"q            8-bit quant %@", p.quantize ? @"on" : @"off"],
        [NSString stringWithFormat:@"y            invert Y    %@", p.invert_y ? @"on" : @"off"],
        @"r            reset counters",
    ];
    CGFloat ly = Y - 28;
    for (NSString* l in lines) { [self drawText:l at:NSMakePoint(X, ly) color:w size:13]; ly -= 20; }
    [self drawText:@"Test: move mouse a fixed distance, flick, then slow-creep. 'err' should stay ~0\n"
                    "(sim rotation tracks ideal mouse rotation). Large err = clamped at full tilt." at:NSMakePoint(20, 20) color:g size:12];
}
@end

@interface AppDel : NSObject <NSApplicationDelegate> { StickView* v; double last; int frame; double hzAcc; int hzN; } @end
@implementation AppDel
- (void)bindMouse:(GCMouse*)m {
    g_usingGC = true;
    m.mouseInput.mouseMovedHandler = ^(GCMouseInput* mi, float dx, float dy) {
        if (g_captured) feed(dx, -dy);   // GCMouse: +y is up; harness convention is +y down
    };
}
- (void)applicationDidFinishLaunching:(NSNotification*)n {
    NSWindow* w = [[NSWindow alloc] initWithContentRect:NSMakeRect(200, 200, 960, 430)
        styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskMiniaturizable backing:NSBackingStoreBuffered defer:NO];
    w.title = @"mouse_to_stick test";
    v = [[StickView alloc] initWithFrame:w.contentView.bounds];
    w.contentView = v; [w makeFirstResponder:v]; w.acceptsMouseMovedEvents = YES;
    [w makeKeyAndOrderFront:nil]; [NSApp activateIgnoringOtherApps:YES];
    for (GCMouse* m in GCMouse.mice) [self bindMouse:m];
    [[NSNotificationCenter defaultCenter] addObserverForName:GCMouseDidConnectNotification object:nil queue:nil
        usingBlock:^(NSNotification* nn) { [self bindMouse:nn.object]; }];
    last = nowSec();
    [NSTimer scheduledTimerWithTimeInterval:1.0/250 repeats:YES block:^(NSTimer*) {
        double t = nowSec(), dt = t - self->last; self->last = t;
        g_stick = g_m.tick(dt);
        double wx, wy; m2s::MouseToStick::gameRate(g_m.p, g_stick, wx, wy);
        g_yaw += wx * dt; g_pitch += wy * dt;
        self->hzAcc += dt; self->hzN++;
        if (self->hzAcc >= 0.5) { g_tickHz = self->hzN / self->hzAcc; self->hzAcc = 0; self->hzN = 0; }
        g_trail.push_back(NSMakePoint(g_stick.x, g_stick.y)); if (g_trail.size() > 60) g_trail.erase(g_trail.begin());
        if (++self->frame % 4 == 0) self->v.needsDisplay = YES;
    }];
}
- (void)applicationWillTerminate:(NSNotification*)n { CGAssociateMouseAndMouseCursorPosition(true); CGDisplayShowCursor(kCGDirectMainDisplay); }
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)a { return YES; }
@end

int main(int argc, char** argv) {
    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        app.activationPolicy = NSApplicationActivationPolicyRegular;
        AppDel* d = [AppDel new]; app.delegate = d; [app run];
    }
    return 0;
}
