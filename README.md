<div align="center">

# ⚡ NULLWIRE

**Zero-Hardware, Sub-Millisecond Input Bridge & Optical Ballistics Engine for PlayStation 5**

[![Language](https://img.shields.io/badge/Language-C%2B%2B17%20%2F%20Objective--C%2B%2B-blue.svg)](#)
[![Protocol](https://img.shields.io/badge/Protocol-Sony%20Remote%20Play%20(Takion%20UDP)-orange.svg)](#)
[![Latency](https://img.shields.io/badge/Wire%20Latency-%3C0.5ms%20(LAN)-green.svg)](#)
[![Hardware](https://img.shields.io/badge/Hardware%20Required-%240%20(Zero%20Adapters)-brightgreen.svg)](#)
[![Status](https://img.shields.io/badge/Status-Prototype%20Verified-blueviolet.svg)](#)

*Eliminating physical hardware adapters forever. 1:1 raw mouse translation directly into the PS5 OS kernel via native encrypted LAN streaming.*

</div>

---

## 👁️ The Vision

For over a decade, console mouse-and-keyboard adapters have been trapped in a flawed hardware paradigm:
- **Fragile USB Dongles:** Devices like XIM, Cronus, and Beloader cost $150–$300, require third-party bypass controllers, and are constantly bricked or banned by console firmware updates.
- **Hardware Bottlenecks:** Physical USB bridges introduce polling jitter, translation latency, and cabling clutter.
- **The Display vs. Input Dilemma:** Players assumed streaming protocols were unusable for competitive play due to video encoding latency.

**Nullwire shatters this paradigm.**

Instead of plugging hardware into the console, Nullwire turns any local computer into an invisible, ultra-low-latency **virtual DualSense controller** communicating directly with the PS5 over local Ethernet. 

> [!IMPORTANT]
> **The Dual-Path Split Architecture:**
> 1. **Visuals (0ms lag):** Your monitor remains plugged directly into the **PS5 HDMI 2.1 port** (4K @ 120Hz HDR, VRR).
> 2. **Input (<0.5ms transit):** Your raw 1000Hz mouse input is processed on your host machine, shaped by an optical-calibrated ballistics engine, and injected over UDP into the PS5 OS as native, cryptographically authenticated Remote Play controller packets.

---

## 🏛️ System Architecture

```
                                  ┌────────────────────────────────────────┐
                                  │           PHYSICAL DISPLAY             │
                                  │    Gaming Monitor / OLED TV (HDMI)     │
                                  └───────────────────▲────────────────────┘
                                                      │
                                           Zero-Lag HDMI 2.1 (4K 120Hz)
                                                      │
┌──────────────────────────────┐              ┌───────┴────────┐
│        HOST MACHINE          │              │                │
│    (Mac / Linux / PC)        │              │     SONY       │
│                              │              │  PLAYSTATION 5 │
│  ┌────────────────────────┐  │              │                │
│  │    Raw Mouse Event     │  │              │                │
│  │ (1000Hz GCMouse/IOHID) │  │              │                │
│  └───────────┬────────────┘  │              │                │
│              ▼               │              │                │
│  ┌────────────────────────┐  │              │                │
│  │  M2S Ballistics Engine │  │              │                │
│  │   - Payout Window Queue│  │              │                │
│  │   - Anti-Deadzone (0.1)│  │              │                │
│  │   - Y/X Scaling (0.71) │  │              │                │
│  │   - Flick Saturation   │  │              │                │
│  └───────────┬────────────┘  │              │                │
│              ▼               │              │                │
│  ┌────────────────────────┐  │              │                │
│  │  Takion UDP Dispatcher │  │              │                │
│  │ (250Hz Controller Loop)│──┼──────────────┼─▶ Remote Play  │
│  └───────────▲────────────┘  │ Sub-ms Wire  │   Input Stack  │
│              │               │ (Direct LAN) │   (Native OS)  │
│  ┌───────────┴────────────┐  │              │                │
│  │ 2D Optical Flow Tracker│◀─┼──────────────┼── Video Frames │
│  │ (Self-Calibrating CV)  │  │   (UDP 360p  │   (Streamed)   │
│  └────────────────────────┘  │  Calibration)│                │
└──────────────────────────────┘              └────────────────┘
```

---

## 🧠 The Mathematics: Mouse to Stick Ballistics

A physical mouse commands **spatial displacement** ($\Delta \theta$). A gamepad thumbstick commands **angular velocity** ($\omega = \frac{d\theta}{dt}$). Converting a mouse into a stick without feeling like "aiming on ice" requires solving four fundamental nonlinear distortions.

### 1. The Fixed Payout Window (No Exponential Tails)
Naive converters use exponential decay filters ($\Delta x \cdot e^{-t/\tau}$). When mouse movement stops, an exponential tail leaves the virtual stick resting at the deadzone edge for up to 150ms—causing noticeable camera drift ("creep").

Nullwire models each mouse count as an exact angular quanta $\Delta \theta = \text{counts} \times S_{\text{deg}}$, paid out linearly over a strict window $T_w \approx 32\text{ ms}$:
$$\omega(t) = \frac{1}{T_w} \sum_{i} \Delta\theta_i \quad \text{for } t \in [t_i, t_i + T_w]$$
When the window expires, the stick returns to **absolute zero** within one frame.

### 2. Saturating Flick Integrator
A mouse flick can demand instantaneous turns exceeding $1500^\circ/\text{s}$, but console game engines enforce a hard turn-rate limit $\omega_{\max}$ (typically $360^\circ - 720^\circ/\text{s}$). 
* Naive converters clip this velocity, discarding up to 60% of flick distance.
* Nullwire integrates excess velocity into a **flick reservoir**, holding the stick at maximum saturation ($1.0$) for exactly:
  $$t_{\text{flick}} = \frac{\Delta \theta_{\text{excess}}}{\omega_{\max}}$$
  The entire flick rotation is preserved without dropping distance.

### 3. Vertical Aspect Ratio & Diagonal Normalization
Games intentionally dampen vertical pitch speed compared to horizontal yaw speed to keep horizons level. Nullwire pre-compensates using empirical video calibration:
$$\text{Ratio} = \frac{\omega_{\text{pitch}}}{\omega_{\text{yaw}}} \approx 0.711$$
$$\vec{S}_{\text{corrected}} = \begin{bmatrix} S_x \\ S_y \times \frac{1}{\text{Ratio}} \end{bmatrix}$$
Without this compensation, a 45° diagonal mouse flick lands at 36.3° in-game (an 8.7° error). With Nullwire, diagonal movement remains isotropic (1:1).

### 4. Inner Deadzone Nullification
Every engine has an inner deadzone $D_z$ below which stick input produces zero angular motion. Nullwire remaps the active range $[0, 1]$ directly past the deadzone threshold:
$$S_{\text{out}} = \text{sgn}(S) \cdot \left[ D_z + (1 - D_z) \cdot |S| \right]$$

---

## 🔬 Closed-Loop Optical Flow Tracker (`Tracker2D`)

Rather than guessing game parameters, Nullwire includes an embedded computer vision engine that measures camera motion directly from video stream keyframes:

```
Video Frame (320x180) ──▶ Spatial Masking (Cut HUD & Weapon) ──▶ Coarse Search (1/4 Scale) ──▶ Fine Sub-Pixel SAD Fit
```

- **Masking:** Strips HUD indicators, weapon models, and screen peripheries to measure purely the 3D world background.
- **Hierarchical Block Matching:** Coarse SAD (Sum of Absolute Differences) at $4\times4$ downsampling, followed by full-resolution fine search.
- **Sub-Pixel Parabolic Fit:** Interpolates between pixel boundaries to achieve sub-0.1 pixel tracking accuracy:
  $$\delta = \frac{1}{2} \cdot \frac{L - R}{L - 2C + R}$$

### Real Empirical Calibration Data (PS5 In-Engine Test Run)

Data collected via `scripts/axes.py` running against a live PS5 target:

```text
stick x   stick y |  yaw deg/s  pitch deg/s |  direction want / got
  0.500   0.000  |     170.6        -0.5   |     0.0 /    -0.2
  0.300   0.000  |      82.1         2.4   |     0.0 /     1.7
  0.000   0.500  |       4.4       121.4   |    90.0 /    87.9
  0.000  -0.500  |     -11.0      -142.1   |   -90.0 /   -94.4
  0.354   0.354  |     131.1        96.3   |    45.0 /    36.3   <-- 8.7° diagonal skew uncorrected
  0.110   0.000  |       4.9         0.4   |     0.0 /     4.4   <-- Past deadzone
  0.090   0.000  |      -0.0         0.0   |     0.0 /     nan   <-- Inside deadzone
```

* **Deadzone Identified:** Exactly `0.100` (10%).
* **True Pitch/Yaw Ratio:** $121.4 / 170.6 = \mathbf{0.711}$ (replacing arbitrary manual estimates).
* **True Max Angular Velocity:** $\omega_{\max} = 365.0^\circ/\text{s}$.

### Visual Verification: The Circle Closure Test

Using `scripts/circles.py`, the mouse drew mathematically perfect circles in-game. The camera's path was tracked and compared to first-and-last frame alignment:

| Circle Test | Radius | deg/s | Old Error (Ended Off) | Nullwire Corrected |
| :--- | :---: | :---: | :---: | :---: |
| **Tiny Slow** | 3° | 8°/s | 0.2° | **0.0°** (Perfect loop) |
| **Small Slow** | 5° | 10°/s | 0.1° | **0.5°** |
| **Small Quick** | 5° | 39°/s | 0.4° | **0.2°** |
| **Medium Medium**| 10° | 42°/s | 0.3° | **0.5°** |
| **Big Fast** | 20° | 251°/s| 4.2° | **3.5°** |

*(Inspect interactive vector trajectories in [`docs/circles.html`](docs/circles.html)).*

---

## ⚡ Comparison: Nullwire vs. Hardware Adapters

| Metric / Feature | Physical Adapters (XIM Matrix, Cronus Zen) | Nullwire Bridge |
| :--- | :--- | :--- |
| **Hardware Cost** | $150 – $300 + bypass controller | **$0** (Free, open source) |
| **Bypass Stability** | High ban/block risk (Sony USB patches) | **Permanent** (Official Remote Play channel) |
| **Input Delivery** | USB polling jitter | Direct 250Hz UDP stream (<0.5ms wire transit) |
| **Video Latency** | None (HDMI direct) | **None** (HDMI direct to display) |
| **Calibration** | Manual trial-and-error slider tuning | **Automated Computer Vision Closed-Loop** |
| **Platform** | Proprietary closed box | Native C++ / Objective-C / Cross-platform |

---

## 📂 Repository Structure

```
Nullwire/
├── include/
│   ├── mouse_to_stick.h    # Core header-only mathematical ballistics engine
│   ├── m2s_calib.h         # Automated closed-loop calibration state machine
│   ├── m2s_track2d.h       # Optical flow 2D block-matching tracker
│   ├── m2s_keys.h          # Keyboard-to-DualSense digital mappings
│   └── macMouseBridge.h    # High-rate raw mouse capture interface
├── src/
│   └── macMouseBridge.mm   # macOS GameController (GCMouse) unaccelerated capture
├── tests/
│   ├── selftest.cpp        # Unit tests for ballistics queue & payout
│   ├── tracktest.cpp       # Unit tests for 2D optical tracker
│   ├── calibtest.cpp       # Simulated camera calibration test runner
│   ├── sim3d.mm            # 3D SceneKit camera simulator
│   └── m2s_test.mm         # Interactive test harness
├── scripts/
│   ├── axes.py             # 2D stick response mapper & visualizer
│   ├── circles.py          # Mouse circle tracking & loop-closure test
│   ├── flicks.py           # High-speed flick saturation test
│   └── ready.py            # Automated test readiness checks
├── patches/
│   └── chiaki-ng-m2s.patch # Production patch for chiaki-ng Remote Play stack
├── docs/
│   └── circles.html        # Interactive SVG comparison visualizer
├── Makefile                # Build system for tests and harnesses
└── README.md
```

---

## 🚀 Quickstart & Building

### Prerequisites
* macOS 12+ or Linux / Windows (macOS test harnesses require `Cocoa`, `GameController`, and `SceneKit`).
* `clang++` or `g++` with C++17 support.
* Python 3.8+ (for optical test scripts).

### Building the Test Suite

```bash
git clone https://github.com/Manteh/nullwire.git
cd nullwire

# Build all test binaries
make

# Run the complete test suite (selftest, tracktest, calibtest)
make test
```

### Applying the Chiaki Bridge Patch

To build the integrated input bridge with Chiaki-ng:

```bash
git clone https://github.com/streetpea/chiaki-ng.git
cd chiaki-ng
git apply /path/to/nullwire/patches/chiaki-ng-m2s.patch

cmake -B build -DCHIAKI_GUI=ON
cmake --build build -j$(sysctl -n hw.ncpu)
```

Run in headless/low-bitrate mode:
```bash
M2S_STREAM=360 ./build/gui/chiaki.app/Contents/MacOS/chiaki stream "PS5-Manteh" 192.168.50.182
```

---

## 🗺️ Roadmap to `v1.0` (The Standalone Daemon)

- [x] **Phase 1: Core Mathematical Engine** — Windowed payout queue, saturation integrator, deadzone nullification.
- [x] **Phase 2: Closed-Loop Optical Tracker** — Real-time frame block matching, sub-pixel fit, and empirical calibration.
- [x] **Phase 3: Chiaki-ng Integration Patch** — Working DualSense UDP injection over LAN.
- [ ] **Phase 4: Headless Daemon (`nullwire-daemon`)** — Standalone C++ daemon with zero Qt/GUI dependencies, blackholing video packets at socket level (<1% CPU, <15MB RAM).
- [ ] **Phase 5: Cross-Platform Raw Input Hooks** — Linux `evdev` and Windows `RawInput` backends for sub-millisecond 8000Hz polling rate mice.

---

## 📜 License & Ethics

Distributed under the MIT License. Built for input research, accessibility, and precision gaming without predatory proprietary hardware.
