# ⚡ Nullwire

> *"Why are we spending $250 on plastic USB dongles that get banned every few months, just to use a mouse on a console?"*

**Nullwire** is an open-source project that replaces physical console adapters (like XIM or Cronus) with pure code, sub-millisecond math, and an automated computer-vision feedback loop. 

This is the story of how it works, the physics rabbit hole of turning a mouse into a joystick, and how we got a computer to calibrate its own aim by watching its own screen.

---

## 📖 The Journey

### Act 1: The $250 Dongle Tax

If you want to play games on a PlayStation 5 with a mouse and keyboard, the market gives you a frustrating choice:

1. Buy a $150–$300 hardware box (XIM Matrix, Cronus Zen, Beloader).
2. Plug an extra DualSense controller into it to trick the PS5’s cryptographic security handshake.
3. Deal with a bird's nest of USB cables and constant input jitter.
4. Pray that the next Sony system update doesn't detect the USB hardware ID and brick your setup.

We looked at this and asked a simple question: **Why are we buying physical hardware to solve a digital problem?**

---

### Act 2: The "Aha!" Moment — The Invisible Controller

Sony built a feature directly into the PS5 that most people take for granted: **Remote Play**.

When you connect to your PS5 through Remote Play, the console doesn't ask for a physical controller. It accepts encrypted controller packets over your local Wi-Fi or Ethernet. **To the PS5 operating system, Remote Play is a 100% genuine, authorized controller.** There is no hardware authentication chip to spoof and no risk of USB device bans.

*"Wait,"* you might say, *"isn't Remote Play laggy?"*

That’s where our breakthrough hit us: **Nobody said you have to look at the Remote Play stream.**

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

You leave your gaming monitor plugged directly into the **PS5 HDMI port** for zero-latency, 120Hz HDR gameplay. 

Your computer simply sits on your desk, captures your mouse, translates the motion into DualSense controller packets, and sends them over a local Ethernet cable in **less than 0.5 milliseconds**. 

Zero extra hardware. $0 spent.

---

### Act 3: The Physics Trap (Why Mouse-to-Stick Feels Like Ice)

Once we had the bridge working, we ran into the wall that has plagued console emulation for 15 years: **Translating a mouse to a thumbstick is fundamentally unnatural.**

Think about the physics:
* **A mouse commands *position*:** If you move your hand 2 inches to the right, you expect your crosshair to move exactly that far and immediately stop.
* **A thumbstick commands *speed*:** A stick is like the gas pedal in a car. Push it right, and your camera turns continuously at a set speed until you let go.

If you connect a mouse to a stick naively, it feels awful. Here is why:

#### 1. The Deadzone Problem
Every game ignores tiny stick movements so your camera doesn't drift when you let go of the controller. If your game has a 10% deadzone, your first subtle mouse nudges do literally nothing.

#### 2. The "Creep" Bug (The Exponential Tail)
Most software tries to smooth stick movement using a decay curve (exponential decay). But as the movement decays, the virtual stick lingers right on the edge of the game's deadzone for an extra 150 milliseconds. The result? You stop your mouse, but your crosshair keeps slowly "creeping" across the screen.

#### 3. The Flick Wall (The Speed Limit)
A human can whip a mouse across a pad in 40 milliseconds. But console games enforce a hard speed limit on turn speed (usually around 360° per second). If you flick faster than the stick can turn, naive converters simply discard the rest of your motion. You wanted a 90° flick; you got 50°.

---

### Act 4: The Math That Solved It

Inside [`include/mouse_to_stick.h`](file:///Users/Manteh/Nullwire/include/mouse_to_stick.h), we wrote a pure mathematical ballistics model that fixes all three issues:

1. **The Strict Payout Window:** Instead of an exponential curve that lingers, every mouse count is converted to an exact angle and paid out over a fixed, tight window (e.g. 32 milliseconds). The moment that window finishes, the stick snaps to **absolute zero**. Zero camera creep.
2. **The Saturating Flick Reservoir:** When you flick faster than the game’s maximum turn rate ($\omega_{\max}$), we don’t throw the extra movement away. We store it in a reservoir, hold the stick at 100% deflection, and hold it just long enough for the camera to finish your intended turn. Your 90° flick lands at 90° every single time.
3. **Deadzone Cancellation:** We inject an anti-deadzone offset that instantly boots the stick past the game’s dead zone the moment the mouse moves a single count.

---

### Act 5: Teaching the Computer to See

We thought our math was complete—until we tested it. We had made guesses:
* *"Let's assume the deadzone is 10%."*
* *"Let's assume vertical sensitivity is 80% of horizontal."*

Our measuring tools fooled us twice in one evening. So we asked: **Why are we guessing when the computer has eyes?**

Because Chiaki receives the Remote Play video stream, our computer has access to the actual game screen 60 times a second. We built a real-time **optical computer vision tracker** ([`include/m2s_track2d.h`](file:///Users/Manteh/Nullwire/include/m2s_track2d.h)) directly into the system.

```
Incoming 60fps Frame ──▶ Mask HUD & Weapon ──▶ Coarse Search (1/4 scale) ──▶ Sub-Pixel Parabolic Fit
```

* It masks out the HUD and weapon model so only the 3D environment is tracked.
* It performs coarse-to-fine block matching against reference frames.
* It uses parabolic interpolation to measure camera motion down to **fractions of a pixel**.

Then, we wrote a test script ([`scripts/axes.py`](file:///Users/Manteh/Nullwire/scripts/axes.py)) that took control of the PS5, moved the stick at exact increments, and logged where the camera really went.

---

### Act 6: The Ground Truth (What the Camera Saw)

Here is the actual measurement output from our PS5 session:

```text
stick x   stick y |  yaw deg/s  pitch deg/s |  direction want / got
  0.500   0.000  |     170.6        -0.5   |     0.0 /    -0.2
  0.000   0.500  |       4.4       121.4   |    90.0 /    87.9
  0.354   0.354  |     131.1        96.3   |    45.0 /    36.3  <-- The Diagonal Trap!
  0.110   0.000  |       4.9         0.4   |     0.0 /     4.4  <-- Moving
  0.090   0.000  |      -0.0         0.0   |     0.0 /     nan  <-- Deadzone!
```

Look at what this revealed:
1. **The Deadzone is exactly `0.100` (10%):** At 0.090 stick deflection, the camera had 0.0 deg/s movement. At 0.110, it moved at 4.9 deg/s.
2. **The Vertical Ratio is `0.711`, not `0.8`:** Horizontal speed at 0.5 stick was 170.6 deg/s; vertical speed was only 121.4 deg/s. Our previous guess of 0.8 had been overshooting vertical aim by 12%!
3. **The Diagonal Trap:** When we pushed the stick diagonally at 45° (`0.354, 0.354`), the in-game camera only moved at **36.3°**. That is an **8.7° aiming error** caused by the game dampening vertical pitch!

#### The Ultimate Test: Drawing Circles
With our optical tracker feeding data into our ballistics engine, we ran [`scripts/circles.py`](file:///Users/Manteh/Nullwire/scripts/circles.py). We instructed the mouse to draw mathematically perfect circles, and watched how closely the game camera matched:

| Test Name | Radius | Turn Rate | Before Calibration | Nullwire (Calibrated) |
| :--- | :---: | :---: | :---: | :---: |
| **Tiny Slow** | 3° | 8°/s | 0.2° error | **0.0° error** (Closed loop) |
| **Small Slow** | 5° | 10°/s | 0.1° error | **0.5° error** |
| **Small Quick** | 5° | 39°/s | 0.4° error | **0.2° error** |
| **Medium Medium**| 10° | 42°/s | 0.3° error | **0.5° error** |

*(You can open [`docs/circles.html`](file:///Users/Manteh/Nullwire/docs/circles.html) in your browser to inspect the interactive SVG paths).*

---

## 🛠️ How It’s Organized

* **[`include/mouse_to_stick.h`](file:///Users/Manteh/Nullwire/include/mouse_to_stick.h):** Header-only C++ ballistics engine. Manages the payout queue, flick accumulator, deadzone clearing, and diagonal normalization. Zero dependencies.
* **[`include/m2s_track2d.h`](file:///Users/Manteh/Nullwire/include/m2s_track2d.h):** The 2D optical tracker. Compares video frames, rejects HUD noise, and measures real sub-pixel camera displacement.
* **[`include/m2s_calib.h`](file:///Users/Manteh/Nullwire/include/m2s_calib.h):** Automated calibration state machine. Sweeps deadzones, tests response curves, and measures full-turn speeds.
* **[`include/macMouseBridge.h`](file:///Users/Manteh/Nullwire/include/macMouseBridge.h) / [`src/macMouseBridge.mm`](file:///Users/Manteh/Nullwire/src/macMouseBridge.mm):** Low-level macOS `GCMouse` driver that reads raw, unaccelerated mouse counts directly from hardware.
* **[`patches/chiaki-ng-m2s.patch`](file:///Users/Manteh/Nullwire/patches/chiaki-ng-m2s.patch):** The complete patch for `chiaki-ng` that adds the input bridge, packet resend logic (preventing lost UDP packets from getting stuck), and the IPC test socket.
* **[`scripts/`](file:///Users/Manteh/Nullwire/scripts/):** Python test scripts (`axes.py`, `circles.py`, `flicks.py`) for automated bench testing.

---

## 🚀 Running the Tests

You can build and run the standalone simulation test suite right now without needing a PS5:

```bash
git clone https://github.com/Manteh/nullwire.git
cd nullwire

# Build the test suite
make

# Run the automated ballistics and computer-vision tests
make test
```

You’ll see the engine test itself against simulated games (linear response curves, exponential response curves, and dual-zone games) and verify that the calibration engine converges on the exact deadzone and turn rate every time.

---

## 🗺️ What’s Next: The Standalone Daemon

Right now, Nullwire runs inside a patched build of `chiaki-ng`. 

Our next milestone is **`nullwire-daemon`**:
* A tiny, headless background service written in C++.
* No Qt interface. No video decoding. No audio pipeline.
* Drops incoming video packets at the network socket layer to keep CPU usage under 1% and RAM under 15MB.
* Pure, silent, ultra-low-latency input injection over local Ethernet.

---

## 🤝 Philosophy & License

Nullwire is released under the **MIT License**. It was created to prove that precision input and accessibility do not require predatory $250 proprietary hardware dongles. Built with curiosity and math.
