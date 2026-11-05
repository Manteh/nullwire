// sim3d.mm — 3D first-person sandbox where the camera is driven ONLY by a virtual DualSense
// right stick (fed by mouse_to_stick.h), the way Marvel Rivals would see it.
//
// Controller realism pipeline:
//   mouse -> m2s.tick() at the "send rate" -> [network latency queue] -> 8-bit stick ->
//   game: inner deadzone, linear curve, wmax deg/s -> camera
// So what you feel here (steps from a low send rate, added latency) is what you'd get via Remote Play.
//
// Build: ./build.sh   Run: ./sim3d
#import <Cocoa/Cocoa.h>
#import <SceneKit/SceneKit.h>
#import <GameController/GameController.h>
#include <mach/mach_time.h>
#include <deque>
#include <set>
#include "mouse_to_stick.h"

using namespace m2s;

static MouseToStick g_m;
static Stick g_wire;                    // stick as the "PS5" sees it (after latency + 8-bit)
static bool g_captured = false, g_usingGC = false;
static double g_idealYaw = 0, g_simYaw = 0;
static double g_yaw = 0, g_pitch = 0;   // camera, degrees
static int g_sendHz = 250, g_latencyMs = 0;
static int g_hits = 0, g_shots = 0;
static double g_posX = 0, g_posZ = 18, g_velY = 0, g_posY = 1.7;
static std::set<unichar> g_keys;

struct Sample { double t; Stick s; };
static std::deque<Sample> g_queue;

static double nowSec() {
    static mach_timebase_info_data_t tb; if (!tb.denom) mach_timebase_info(&tb);
    return (double)mach_absolute_time() * tb.numer / tb.denom * 1e-9;
}
static void feed(double dx, double dy) {   // dy: screen-down positive
    g_m.addDelta(dx, dy);
    g_idealYaw += dx * g_m.p.deg_per_count;
}

@interface PassHUD : NSView @end
@implementation PassHUD - (NSView*)hitTest:(NSPoint)p { return nil; } @end

@interface Crosshair : PassHUD @end
@implementation Crosshair
- (void)drawRect:(NSRect)r {
    [[NSColor colorWithRed:0.2 green:1 blue:0.4 alpha:0.95] setFill];
    CGFloat cx = NSMidX(self.bounds), cy = NSMidY(self.bounds);
    NSRectFill(NSMakeRect(cx-1, cy-10, 2, 6)); NSRectFill(NSMakeRect(cx-1, cy+4, 2, 6));
    NSRectFill(NSMakeRect(cx-10, cy-1, 6, 2)); NSRectFill(NSMakeRect(cx+4, cy-1, 6, 2));
}
@end

static NSImage* gridTexture() {
    NSImage* im = [[NSImage alloc] initWithSize:NSMakeSize(512, 512)];
    [im lockFocus];
    [[NSColor colorWithWhite:0.16 alpha:1] setFill]; NSRectFill(NSMakeRect(0, 0, 512, 512));
    [[NSColor colorWithWhite:0.32 alpha:1] setFill];
    NSRectFill(NSMakeRect(0, 0, 512, 3)); NSRectFill(NSMakeRect(0, 0, 3, 512));
    [im unlockFocus];
    return im;
}

@interface SimView : SCNView
@property SCNNode *camNode, *pitchNode;
@property NSMutableArray<SCNNode*>* targets;
@property NSTextField* hud;
@end

@implementation SimView
- (BOOL)acceptsFirstResponder { return YES; }

- (void)setCapture:(BOOL)on {
    if (on == g_captured) return;
    g_captured = on;
    CGAssociateMouseAndMouseCursorPosition(on ? false : true);
    if (on) CGDisplayHideCursor(kCGDirectMainDisplay); else CGDisplayShowCursor(kCGDirectMainDisplay);
}

- (void)respawn:(SCNNode*)t {
    double a = ((double)arc4random() / UINT32_MAX - 0.5) * 2.2;       // spread in front of spawn
    double d = 12 + (double)arc4random() / UINT32_MAX * 26;
    double h = 0.8 + (double)arc4random() / UINT32_MAX * 5;
    t.position = SCNVector3Make(sin(a) * d, h, 18 - cos(a) * d);
    [t removeAllActions];
    if (arc4random() % 2) {   // strafing target for tracking practice
        double w = 3 + arc4random() % 6, sp = 0.8 + (double)arc4random() / UINT32_MAX;
        SCNAction* m = [SCNAction sequence:@[[SCNAction moveByX:w y:0 z:0 duration:sp*1.2],
                                              [SCNAction moveByX:-2*w y:0 z:0 duration:sp*2.4],
                                              [SCNAction moveByX:w y:0 z:0 duration:sp*1.2]]];
        [t runAction:[SCNAction repeatActionForever:m]];
    }
}

- (void)fire {
    g_shots++;
    NSPoint c = NSMakePoint(NSMidX(self.bounds), NSMidY(self.bounds));
    NSArray<SCNHitTestResult*>* hits = [self hitTest:c options:@{SCNHitTestFirstFoundOnlyKey:@YES}];
    for (SCNHitTestResult* h in hits) {
        if ([self.targets containsObject:h.node]) {
            g_hits++;
            SCNNode* n = h.node;
            n.geometry.firstMaterial.emission.contents = [NSColor whiteColor];
            [SCNTransaction begin]; [SCNTransaction setAnimationDuration:0.15];
            n.geometry.firstMaterial.emission.contents = [NSColor blackColor]; [SCNTransaction commit];
            [self respawn:n];
            break;
        }
    }
}

- (void)mouseDown:(NSEvent*)e { if (!g_captured) [self setCapture:YES]; else [self fire]; }
- (void)mouseMoved:(NSEvent*)e { if (g_captured && !g_usingGC) feed(e.deltaX, e.deltaY); }
- (void)mouseDragged:(NSEvent*)e { [self mouseMoved:e]; }
- (void)keyUp:(NSEvent*)e { NSString* c = e.charactersIgnoringModifiers; if (c.length) g_keys.erase([c characterAtIndex:0]); }
- (void)keyDown:(NSEvent*)e {
    if (e.isARepeat) { /* repeats fine for tuning keys below */ }
    NSString* c = e.charactersIgnoringModifiers; unichar k = c.length ? [c characterAtIndex:0] : 0;
    Params& p = g_m.p;
    g_keys.insert(k);
    switch (k) {
        case 27: [self setCapture:NO]; break;
        case NSUpArrowFunctionKey:    p.deg_per_count *= 1.05; break;
        case NSDownArrowFunctionKey:  p.deg_per_count /= 1.05; break;
        case NSRightArrowFunctionKey: p.wmax_deg_s += 20; break;
        case NSLeftArrowFunctionKey:  p.wmax_deg_s = std::max(60.0, p.wmax_deg_s - 20); break;
        case '[': p.deadzone = std::max(0.0, p.deadzone - 0.01); break;
        case ']': p.deadzone = std::min(0.4, p.deadzone + 0.01); break;
        case ',': p.expo = std::max(0.5, p.expo - 0.05); break;
        case '.': p.expo = std::min(2.0, p.expo + 0.05); break;
        case 'f': p.window_ms = p.window_ms > 20 ? 16 : 32; break;
        case 'y': p.invert_y = !p.invert_y; break;
        case 'l': { int v[] = {0, 15, 30, 50, 80}; int i = 0; while (i < 4 && v[i] != g_latencyMs) i++; g_latencyMs = v[(i + 1) % 5]; break; }
        case 'p': { int v[] = {60, 120, 250, 500}; int i = 0; while (i < 3 && v[i] != g_sendHz) i++; g_sendHz = v[(i + 1) % 4]; break; }
        case 'r': g_idealYaw = g_simYaw = 0; g_hits = g_shots = 0; g_m.reset(); break;
    }
}

- (void)buildScene {
    SCNScene* s = [SCNScene scene];
    s.background.contents = [NSColor colorWithRed:0.45 green:0.6 blue:0.8 alpha:1];
    // floor
    SCNFloor* floor = [SCNFloor floor]; floor.reflectivity = 0;
    floor.firstMaterial.diffuse.contents = gridTexture();
    floor.firstMaterial.diffuse.wrapS = floor.firstMaterial.diffuse.wrapT = SCNWrapModeRepeat;
    floor.firstMaterial.diffuse.contentsTransform = SCNMatrix4MakeScale(60, 60, 1);
    floor.firstMaterial.lightingModelName = SCNLightingModelConstant;
    [s.rootNode addChildNode:[SCNNode nodeWithGeometry:floor]];
    // pillars as visual references to judge turn distance/speed
    for (int i = 0; i < 16; i++) {
        double a = i * M_PI * 2 / 16;
        SCNBox* b = [SCNBox boxWithWidth:1 height:6 length:1 chamferRadius:0];
        b.firstMaterial.diffuse.contents = (i % 4 == 0) ? [NSColor systemRedColor] : [NSColor colorWithWhite:0.75 alpha:1];
        SCNNode* n = [SCNNode nodeWithGeometry:b];
        n.position = SCNVector3Make(sin(a) * 45, 3, 18 - cos(a) * 45);
        [s.rootNode addChildNode:n];
    }
    // cardinal markers every 90 degrees from spawn heading
    SCNNode* ceil = [SCNNode nodeWithGeometry:[SCNBox boxWithWidth:0.3 height:0.3 length:0.3 chamferRadius:0]];
    ceil.position = SCNVector3Make(0, 8, -27); [s.rootNode addChildNode:ceil];
    // targets
    self.targets = [NSMutableArray array];
    for (int i = 0; i < 10; i++) {
        SCNSphere* sp = [SCNSphere sphereWithRadius:0.45];
        sp.firstMaterial.diffuse.contents = [NSColor systemOrangeColor];
        sp.firstMaterial.lightingModelName = SCNLightingModelConstant;
        SCNNode* t = [SCNNode nodeWithGeometry:sp];
        [self respawn:t]; [s.rootNode addChildNode:t]; [self.targets addObject:t];
    }
    SCNLight* L = [SCNLight light]; L.type = SCNLightTypeAmbient; L.intensity = 1000;
    SCNNode* ln = [SCNNode node]; ln.light = L; [s.rootNode addChildNode:ln];
    // camera rig: yaw node -> pitch node -> camera
    self.camNode = [SCNNode node];
    self.pitchNode = [SCNNode node];
    SCNCamera* cam = [SCNCamera camera];
    cam.projectionDirection = SCNCameraProjectionDirectionHorizontal;
    cam.fieldOfView = 100; cam.zNear = 0.05; cam.zFar = 300;
    SCNNode* cn = [SCNNode node]; cn.camera = cam;
    [self.pitchNode addChildNode:cn]; [self.camNode addChildNode:self.pitchNode];
    [s.rootNode addChildNode:self.camNode];
    self.scene = s; self.pointOfView = cn;
    self.preferredFramesPerSecond = 120;
    self.antialiasingMode = SCNAntialiasingModeMultisampling4X;
}

// one pass of the whole pipeline; dt = real elapsed since last call
- (void)step:(double)dt now:(double)now {
    static double lastSend = 0;
    Params& p = g_m.p;

    // controller "sends" at g_sendHz
    double period = 1.0 / g_sendHz;
    if (now - lastSend >= period) {
        double sdt = now - lastSend; lastSend = now;
        Stick s = g_m.tick(sdt);
        g_queue.push_back({now + g_latencyMs / 1000.0, s});
    }
    while (!g_queue.empty() && g_queue.front().t <= now) { g_wire = g_queue.front().s; g_queue.pop_front(); }

    // game: deadzone + linear + wmax
    static double lastFrame = 0, wx = 0, wy = 0;      // the game samples the stick once per frame (60 Hz)
    if (now - lastFrame >= 1.0 / 60) { lastFrame = now; MouseToStick::gameRate(p, g_wire, wx, wy); }
    g_yaw -= wx * dt; g_simYaw += wx * dt;
    g_pitch = std::max(-89.0, std::min(89.0, g_pitch + wy * dt));

    // WASD movement (keyboard stands in for left stick)
    double fwd = (g_keys.count('w') ? 1 : 0) - (g_keys.count('s') ? 1 : 0);
    double str = (g_keys.count('d') ? 1 : 0) - (g_keys.count('a') ? 1 : 0);
    double sp = 6.5;
    double yr = g_yaw * M_PI / 180;
    g_posX += (-sin(yr) * fwd + cos(yr) * str) * sp * dt;
    g_posZ += (-cos(yr) * fwd - sin(yr) * str) * sp * dt;
    if (g_keys.count(' ') && g_posY <= 1.701) g_velY = 6.5;
    g_velY -= 20 * dt; g_posY = std::max(1.7, g_posY + g_velY * dt); if (g_posY == 1.7) g_velY = 0;

    self.camNode.position = SCNVector3Make(g_posX, g_posY, g_posZ);
    self.camNode.eulerAngles = SCNVector3Make(0, g_yaw * M_PI / 180, 0);
    self.pitchNode.eulerAngles = SCNVector3Make(g_pitch * M_PI / 180, 0, 0);
}

- (void)updateHUD {
    Params& p = g_m.p;
    double wx, wy; MouseToStick::gameRate(p, g_wire, wx, wy);
    self.hud.stringValue = [NSString stringWithFormat:
        @"%@\n"
        @"stick x %+.2f y %+.2f   turn %+.0f/%+.0f deg/s   input %@\n"
        @"yaw: controller-sim %+.1f  vs ideal mouse %+.1f  (err %+.2f deg)\n"
        @"hits %d / shots %d (%.0f%%)\n\n"
        @"WASD move  Space jump  Click shoot  Esc release  R reset\n"
        @"sens %.4f deg/count [up/dn]   wmax %.0f deg/s [l/r]   deadzone %.2f [ ]   expo %.2f [, .]\n"
        @"send rate %d Hz [P]   added latency %d ms [L]   window %@ [F]   invertY %@ [Y]",
        g_captured ? @"CAPTURED" : @"CLICK TO CAPTURE MOUSE",
        g_wire.x, g_wire.y, wx, wy, g_usingGC ? @"raw" : @"NSEvent(accel!)",
        g_simYaw, g_idealYaw, g_simYaw - g_idealYaw,
        g_hits, g_shots, g_shots ? 100.0 * g_hits / g_shots : 0.0,
        p.deg_per_count, p.wmax_deg_s, p.deadzone, p.expo, g_sendHz, g_latencyMs, [NSString stringWithFormat:@"%.0fms", p.window_ms],
        p.invert_y ? @"on" : @"off"];
}
@end

@interface AppDel : NSObject <NSApplicationDelegate> { SimView* v; double last; int fr; } @end
@implementation AppDel
- (void)bindMouse:(GCMouse*)m {
    g_usingGC = true;
    m.mouseInput.mouseMovedHandler = ^(GCMouseInput* mi, float dx, float dy) { if (g_captured) feed(dx, -dy); };
}
- (void)applicationDidFinishLaunching:(NSNotification*)n {
    NSWindow* w = [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, 1280, 760)
        styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable backing:NSBackingStoreBuffered defer:NO];
    w.title = @"mouse -> DualSense stick 3D sim";
    v = [[SimView alloc] initWithFrame:w.contentView.bounds];
    v.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [v buildScene];
    w.contentView = v;
    Crosshair* ch = [[Crosshair alloc] initWithFrame:v.bounds]; ch.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [v addSubview:ch];
    NSTextField* hud = [NSTextField labelWithString:@""];
    hud.font = [NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightMedium];
    hud.textColor = [NSColor whiteColor]; hud.backgroundColor = [NSColor colorWithWhite:0 alpha:0.55]; hud.drawsBackground = YES;
    hud.frame = NSMakeRect(8, 8, 760, 175); hud.autoresizingMask = NSViewMaxXMargin | NSViewMaxYMargin;
    // labels on top of SCNView: keep it from eating clicks
    PassHUD* holder = [[PassHUD alloc] initWithFrame:v.bounds]; holder.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [holder addSubview:hud]; [v addSubview:holder];
    v.hud = hud;
    [w makeFirstResponder:v]; w.acceptsMouseMovedEvents = YES;
    [w makeKeyAndOrderFront:nil]; [NSApp activateIgnoringOtherApps:YES];
    for (GCMouse* m in GCMouse.mice) [self bindMouse:m];
    [[NSNotificationCenter defaultCenter] addObserverForName:GCMouseDidConnectNotification object:nil queue:nil
        usingBlock:^(NSNotification* nn) { [self bindMouse:nn.object]; }];
    last = nowSec();
    [NSTimer scheduledTimerWithTimeInterval:1.0/1000 repeats:YES block:^(NSTimer*) {
        double t = nowSec(), dt = t - self->last; self->last = t;
        [self->v step:dt now:t];
        if (++self->fr % 16 == 0) [self->v updateHUD];
    }];
}
- (void)applicationWillTerminate:(NSNotification*)n { CGAssociateMouseAndMouseCursorPosition(true); CGDisplayShowCursor(kCGDirectMainDisplay); }
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)a { return YES; }
@end

int main() {
    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        app.activationPolicy = NSApplicationActivationPolicyRegular;
        AppDel* d = [AppDel new]; app.delegate = d; [app run];
    }
}
