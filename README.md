# ⚡ Nullwire

> *"Why are we spending $250 on plastic USB dongles that get banned every few months, just to use a mouse on a console?"*

**Nullwire** is an open-source project that replaces physical console adapters (like XIM or Cronus) with pure code, sub-millisecond math, and an automated computer-vision feedback loop. 

It turns any computer into an invisible, ultra-low-latency **virtual DualSense controller** that talks directly to your PlayStation 5 over local Ethernet. 

This is the story of what we built, what it feels like to play, what it beats, and how we plan to make it the most precise console input system on the planet.

---

## 🎯 What This Allows Us to Play Like

If you’ve ever played a shooter on PC (like *Counter-Strike*, *Valorant*, or *Apex Legends*), you know the feeling: your crosshair is an extension of your arm. You don’t think about turning; you just flick, and the crosshair snaps onto a head.

When you try using existing mouse converters on console, you immediately hit the "console wall":
* Your crosshair feels heavy, like you're aiming through cold molasses.
* Diagonal sweeps curve unnaturally into flattened ellipses.
* Micro-adjustments for sniper headshots get swallowed by stubborn deadzones.
* If you whip your mouse for a quick 180° turn, the game cuts you off halfway through because you exceeded the stick's speed limit.

**Nullwire changes the game completely.**

```
   ┌────────────────────────────────────────────────────────┐
   │                    YOUR TV / MONITOR                   │
   │               (Zero Display Lag • 4K @ 120Hz)          │
   └───────────────────────────▲────────────────────────────┘
                               │
                     Direct PS5 HDMI Cable
                               │
   ┌───────────────────────────┴────────────┐
   │             PLAYSTATION 5              │
   └───────────────────────────▲────────────┘
                               │
               Local LAN Cable │ (< 0.5ms packet transit)
                               │
   ┌───────────────────────────┴────────────┐
   │              YOUR COMPUTER             │
   │       (Mac / Linux / Windows PC)       │
   │                                        │
   │  • Mouse plugged in here               │
   │  • Translates mouse deltas to sticks   │
   │  • Fires UDP packets over LAN to PS5   │
   └────────────────────────────────────────┘
```

Because Nullwire mathematically pre-compensates for the console engine's hidden biases:
* **Instant 180° & 360° Flicks:** Our *Flick Reservoir* holds the stick at maximum speed for the exact duration of your swipe. A 180° flick lands at 180° every single time.
* **Pixel-Perfect Micro-Adjustments:** The inner deadzone is canceled out on the very first sub-millimeter mouse tick. Tiny crosshair nudges feel crisp and instantaneous.
* **True 1:1 Diagonals:** We measure and invert the game’s vertical sensitivity dampening, so a 45° diagonal mouse flick travels in a true 45° straight line—not an awkward curve.
* **Zero Display Lag:** You aren't playing on a compressed video stream. Your eyes are locked onto your gaming monitor or OLED TV via HDMI at full 120Hz.

---

## 🥊 What This Beats

| The Alternative | Why It Fails | How Nullwire Beats It |
| :--- | :--- | :--- |
| **Physical Adapters**<br>*(XIM Matrix, Cronus Zen, Beloader)* | Cost $150–$300. Require bypass controllers. Cable nests across your desk. Regularly banned or bricked by Sony firmware updates. | **$0 cost. Zero hardware.** Works over native Remote Play, an official Sony feature that cannot be banned without breaking the PlayStation Portal. |
| **Native Game Mouse Support**<br>*(COD, Fortnite)* | Only supported in a handful of games. Often forces you into PC cross-play lobbies against PC cheaters. Typically has high input lag and uncustomizable acceleration. | **Universal compatibility.** Works in *any* PS5 game (*Overwatch, Apex, Destiny 2, Helldivers 2, GTA*) because the console sees a standard DualSense controller. |
| **Standard Remote Play**<br>*(Chiaki, PS Remote Play App)* | Designed to be played on a small laptop screen with 40–80ms of video encoding lag and compressed visuals. | **The Dual-Path Split.** Visuals stay on your 0ms HDMI display; only lightweight input packets travel over Ethernet in $< 0.5\text{ ms}$. |

---

## 📍 Where We Are Right Now

We aren't just theorizing; we have already built the engine and confirmed it on real hardware:

- [x] **The Mathematical Ballistics Core (`mouse_to_stick.h`):**
  - **Fixed Payout Window:** Movement is paid out over a calibrated 32ms window, snapping the stick to absolute zero when you stop to eliminate post-aim camera drift.
  - **Flick Overflow Reservoir:** Capped turns are banked and sustained over time so flicks retain full distance.
  - **Deadzone Cancellation & Quantization:** Instant deadzone clearing with sub-step error carry.

- [x] **The Optical Feedback Tracker (`m2s_track2d.h`):**
  - An embedded computer-vision engine that watches the incoming video frames from the PS5 at 60fps.
  - Masks out the HUD and weapon model to isolate the 3D scene.
  - Uses coarse-to-fine block matching with sub-pixel parabolic interpolation to measure real camera yaw and pitch down to fractions of a pixel.

- [x] **The Ground Truth Confirmed on PS5:**
  - Ran automated calibration sweeps on a live PS5 running over LAN.
  - **Deadzone discovered:** Exactly `0.100` (10%).
  - **Vertical dampening revealed:** The game engine dampens vertical pitch to `0.711` of horizontal yaw (disproving our initial guess of 0.8).
  - **The Diagonal Trap caught:** Uncalibrated 45° mouse input resulted in an in-game camera angle of `36.3°` (an 8.7° error).

- [x] **Loop-Closure Circle Verification:**
  - Ran closed-loop tests drawing exact circles. Calibrated Nullwire brought circle closure error down to **0.0° – 0.2°** (see [`docs/circles.html`](file:///Users/Manteh/Nullwire/docs/circles.html)).

- [x] **Integrated Chiaki Patch:**
  - A clean 41KB patch ([`patches/chiaki-ng-m2s.patch`](file:///Users/Manteh/Nullwire/patches/chiaki-ng-m2s.patch)) for `chiaki-ng` that adds the input bridge, unaccelerated macOS mouse capture (`GCMouse`), and UDP packet pacing.

---

## 🚀 The Master Plan: How We Make It God-Tier

Here is the exact roadmap to take Nullwire from an impressive research prototype to the ultimate input engine:

### 1. The Headless Standalone Daemon (`nullwire-daemon`)
Right now, Nullwire runs inside a modified build of `chiaki-ng`. While functional, Chiaki still initializes video streams and GUI layers.
* **The Plan:** Extract Chiaki’s low-level session handshake and UDP crypto into a lean, standalone C++ binary.
* **The Result:** No Qt interface. No video decoding. No audio pipeline. It runs quietly in your terminal, consuming **< 15MB of RAM** and **< 0.5% CPU**, dedicated 100% to polling your mouse and dispatching packets.

### 2. 1000Hz – 8000Hz Raw Hardware Polling
Standard desktop windows process mouse events through operating system event queues, introducing micro-jitter.
* **The Plan:** Hook directly into kernel-level raw input interfaces:
  * **macOS:** `IOHIDManager` / `GCMouse` direct driver access.
  * **Windows:** `RawInput` API bypassing Windows mouse acceleration.
  * **Linux:** Direct `/dev/input/event*` (`evdev`) capture.
* **The Result:** True sub-millisecond polling capable of ingesting high-polling-rate gaming mice (1000Hz, 4000Hz, 8000Hz) with zero OS filtering.

### 3. One-Click Auto-Calibration (Zero Manual Sliders)
Traditional converters make you spend weeks tweaking sensitivity curves, deadzones, and acceleration sliders.
* **The Plan:** When you launch a new game, you activate **Auto-Calib Mode**. 
* Nullwire automatically takes the stick, executes a 10-second test sequence (settle, latency pulse, deadzone sweep, full-speed spin), and uses its computer-vision tracker to measure the game's exact profile.
* It automatically saves the profile to disk. You never have to guess a setting again.

### 4. Anti-Loss Packet Pacing & Jitter Buffering
UDP packets over local Wi-Fi or home switches can occasionally drop or arrive out of order. If a "stick back to zero" packet gets dropped, your crosshair can jerk.
* **The Plan:** Implement redundant packet bursts (sending high-priority flick stops and stick-zero updates across micro-spaced intervals) and sequence-stamped state history.
* **The Result:** Rock-solid transmission consistency even over busy home Wi-Fi networks.

### 5. Automated Aim Curve Inversion
Some console games force non-linear response curves (Dual-Zone, Exponential, or Dynamic curves) that distort mouse motion.
* **The Plan:** The calibration engine builds a high-resolution 64-point Lookup Table (LUT) of the game's actual velocity curve $f(\text{stick}) \to \text{deg/s}$, and mathematically inverts it ($f^{-1}$).
* **The Result:** Even if the game’s controller curve is wildly non-linear, your mouse output feels 100% linear, 1:1, and raw.

---

## 📂 Repository Tour

```
Nullwire/
├── include/
│   ├── mouse_to_stick.h    # Header-only ballistics model (payout queue, flick reservoir)
│   ├── m2s_track2d.h       # 2D optical block-matching tracker with sub-pixel fit
│   ├── m2s_calib.h         # Automated calibration state machine
│   ├── m2s_keys.h          # Keyboard to DualSense digital mapping
│   └── macMouseBridge.h    # Low-level raw mouse capture interface
├── src/
│   └── macMouseBridge.mm   # macOS GameController (GCMouse) unaccelerated capture
├── tests/
│   ├── selftest.cpp        # Unit tests for queue payout, deadzones, and flicks
│   ├── tracktest.cpp       # Unit tests for 2D optical block matching
│   ├── calibtest.cpp       # Simulation suite testing calibration against game curves
│   ├── sim3d.mm            # 3D SceneKit camera simulator
│   └── m2s_test.mm         # Interactive test harness
├── scripts/
│   ├── axes.py             # Measures real yaw/pitch speeds for all stick directions
│   ├── circles.py          # Draws perfect circles and measures camera loop closure
│   ├── flicks.py           # Tests high-speed flick saturation and queue depth
│   └── ready.py            # Diagnostic pre-flight checks
├── patches/
│   └── chiaki-ng-m2s.patch # Production patch for chiaki-ng Remote Play stack
├── docs/
│   └── circles.html        # Interactive SVG comparison visualizer
├── Makefile                # Complete build system for tests and harnesses
└── README.md
```

---

## ⚡ Try the Tests Today

You can compile and run the full mathematical and optical test suite right now on your machine (no PS5 needed):

```bash
git clone https://github.com/Manteh/nullwire.git
cd nullwire

# Build all test binaries
make

# Run the complete test suite
make test
```

You’ll see the engine test itself against simulated games (Linear, Dual-Zone, Exponential, and games with delay spikes) and verify that the calibration engine converges on the exact deadzone and turn rate every time.

---

## 🤝 Philosophy & License

Nullwire is distributed under the **MIT License**. It was born from the belief that precision input and accessibility do not belong behind a $250 hardware paywall. Built with curiosity, open-source code, and math.
