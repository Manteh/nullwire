#!/usr/bin/env python3
"""Draws every figure in the README as a small self-contained SVG (docs/figures/).

Charts use numbers that were measured on a real PS5 or dumped from the real engine (scripts/figdata.cpp).
Run:  clang++ -std=c++17 -O2 -Iinclude -o /tmp/figdata scripts/figdata.cpp && /tmp/figdata > /tmp/figdata.txt
      python3 scripts/make_figures.py /tmp/figdata.txt
"""
import math, os, sys, html

OUT = os.path.join(os.path.dirname(__file__), "..", "docs", "figures")
os.makedirs(OUT, exist_ok=True)
DATA = open(sys.argv[1] if len(sys.argv) > 1 else "/tmp/figdata.txt").read().splitlines()

STYLE = """
<style>
  .panel{fill:#fcfcfb}.ink{fill:#1a1a19}.mut{fill:#6b6a63}.grid{stroke:#e2e1da;stroke-width:1}.axis{stroke:#9b9a92;stroke-width:1}
  .a{stroke:#2a78d6}.b{stroke:#eb6834}.g{stroke:#8a897f}.af{fill:#2a78d6}.bf{fill:#eb6834}.gf{fill:#8a897f}
  .afade{fill:#2a78d6;opacity:.14}.bfade{fill:#eb6834;opacity:.14}.soft{fill:#efeee8}.line{fill:none;stroke-width:2.5;stroke-linecap:round;stroke-linejoin:round}
  text{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Helvetica,Arial,sans-serif}
  @media (prefers-color-scheme: dark){
    .panel{fill:#1a1a19}.ink{fill:#f0efec}.mut{fill:#c3c2b7}.grid{stroke:#34332f}.axis{stroke:#6b6a63}
    .a{stroke:#3987e5}.b{stroke:#d95926}.af{fill:#3987e5}.bf{fill:#d95926}.afade{fill:#3987e5}.bfade{fill:#d95926}.soft{fill:#262522}
  }
</style>"""


def svg(w, h, body, title):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}" role="img" aria-label="{html.escape(title)}">'
            f'<title>{html.escape(title)}</title>{STYLE}<rect class="panel" width="{w}" height="{h}" rx="14"/>{body}</svg>')


def t(x, y, s, cls="ink", size=13, weight=400, anchor="start", extra=""):
    return f'<text x="{x:.1f}" y="{y:.1f}" class="{cls}" font-size="{size}" font-weight="{weight}" text-anchor="{anchor}" {extra}>{html.escape(str(s))}</text>'


def heading(title, sub, w):
    return t(28, 40, title, size=20, weight=700) + t(28, 62, sub, "mut", 13)


class Chart:
    """A plain line/bar chart frame: left/top/right/bottom pixel box and numeric ranges."""
    def __init__(self, x0, y0, x1, y1, xr, yr):
        self.x0, self.y0, self.x1, self.y1, self.xr, self.yr = x0, y0, x1, y1, xr, yr
    def X(self, v): return self.x0 + (v - self.xr[0]) / (self.xr[1] - self.xr[0]) * (self.x1 - self.x0)
    def Y(self, v): return self.y1 - (v - self.yr[0]) / (self.yr[1] - self.yr[0]) * (self.y1 - self.y0)
    def frame(self, xt, yt, xlabel, ylabel, xfmt="{:g}", yfmt="{:g}"):
        s = ""
        for v in yt:
            s += f'<line class="grid" x1="{self.x0}" x2="{self.x1}" y1="{self.Y(v):.1f}" y2="{self.Y(v):.1f}"/>' + t(self.x0 - 10, self.Y(v) + 4, yfmt.format(v), "mut", 11, anchor="end")
        for v in xt:
            s += t(self.X(v), self.y1 + 20, xfmt.format(v), "mut", 11, anchor="middle")
        s += f'<line class="axis" x1="{self.x0}" x2="{self.x1}" y1="{self.y1}" y2="{self.y1}"/>'
        s += t((self.x0 + self.x1) / 2, self.y1 + 42, xlabel, "mut", 12, anchor="middle")
        s += t(self.x0 - 44, (self.y0 + self.y1) / 2, ylabel, "mut", 12, anchor="middle", extra=f'transform="rotate(-90 {self.x0 - 44} {(self.y0 + self.y1) / 2})"')
        return s
    def path(self, pts, cls, dash=""):
        d = " ".join(("M" if i == 0 else "L") + f"{self.X(x):.1f},{self.Y(y):.1f}" for i, (x, y) in enumerate(pts))
        da = f' stroke-dasharray="{dash}"' if dash else ""
        return f'<path class="line {cls}" d="{d}"{da}/>'
    def dots(self, pts, cls, r=4.5):
        return "".join(f'<circle class="{cls}" cx="{self.X(x):.1f}" cy="{self.Y(y):.1f}" r="{r}"/>' for x, y in pts)


def rows(kind):
    """Numbers from the lines that start with `kind` in the engine dump."""
    return [[float(x) for x in l.split()[1:]] for l in DATA if l.split()[0] == kind]


# ---------------------------------------------------------------------------------------------------------------------
# 1. Hero
# ---------------------------------------------------------------------------------------------------------------------
def hero():
    w, h = 1000, 350
    b = '<defs><linearGradient id="g" x1="0" x2="1"><stop offset="0" stop-color="#2a78d6"/><stop offset="1" stop-color="#eb6834"/></linearGradient></defs>'
    b += f'<rect x="0" y="0" width="{w}" height="6" fill="url(#g)" rx="3"/>'
    b += t(w / 2, 82, "Nullwire", "ink", 54, 800, "middle")
    b += t(w / 2, 118, "Teach a thumbstick to move like a mouse.", "mut", 20, 400, "middle")
    # mouse
    b += '<g transform="translate(120,160)"><rect class="soft" x="0" y="0" width="86" height="120" rx="40"/><rect class="a" x="0" y="0" width="86" height="120" rx="40" fill="none" stroke-width="3"/><line class="a" x1="43" y1="0" x2="43" y2="46" stroke-width="3"/><rect class="af" x="39" y="14" width="8" height="20" rx="4"/></g>'
    b += t(163, 304, "Your mouse", "ink", 14, 600, "middle") + t(163, 322, "says where to look", "mut", 12, anchor="middle")
    # arrow 1 + engine
    b += '<path class="g" d="M240,220 H330" stroke-width="3" fill="none" stroke-dasharray="2 7" stroke-linecap="round"/><path class="gf" d="M326,211 L344,220 L326,229 Z"/>'
    b += '<g transform="translate(352,170)"><rect class="soft" width="296" height="100" rx="16"/><rect fill="none" class="b" width="296" height="100" rx="16" stroke-width="3"/>'
    b += t(148, 42, "Ballistics engine", "ink", 18, 700, "middle") + t(148, 66, "turns positions into stick pushes", "mut", 13, anchor="middle") + t(148, 84, "and measures the game by watching it", "mut", 13, anchor="middle") + '</g>'
    # arrow 2 + stick
    b += '<path class="g" d="M664,220 H754" stroke-width="3" fill="none" stroke-dasharray="2 7" stroke-linecap="round"/><path class="gf" d="M750,211 L768,220 L750,229 Z"/>'
    b += '<g transform="translate(850,220)"><circle class="soft" r="62"/><circle class="b" r="62" fill="none" stroke-width="3"/><circle class="grid" r="42" fill="none" stroke-dasharray="3 5"/><circle class="bf" cx="22" cy="-18" r="24"/></g>'
    b += t(850, 304, "PS5 thumbstick", "ink", 14, 600, "middle") + t(850, 322, "over Remote Play, no hardware", "mut", 12, anchor="middle")
    open(os.path.join(OUT, "hero.svg"), "w").write(svg(w, h, b, "A mouse feeds the Nullwire engine, which drives a PS5 thumbstick over Remote Play"))


# ---------------------------------------------------------------------------------------------------------------------
# 2. Position vs speed
# ---------------------------------------------------------------------------------------------------------------------
def position_vs_speed():
    w, h = 1000, 400
    b = heading("A mouse says where. A stick says how fast.", "Turning one into the other is the whole problem.", w)
    cols = [(40, "Mouse: position", "a", "Move it 2 cm and stop.\nThe camera turns 40° and stops."), (520, "Stick: speed", "b", "Hold it right.\nThe camera keeps turning until you let go.")]
    for x, title, c, text in cols:
        b += f'<g transform="translate({x},92)"><rect class="soft" width="440" height="270" rx="14"/>'
        b += t(24, 38, title, "ink", 17, 700)
        for i, line in enumerate(text.split("\n")): b += t(24, 64 + i * 20, line, "mut", 13)
        b += "</g>"
    # mouse panel: a ruler and a camera arc
    g = '<g transform="translate(40,92)">'
    g += '<line class="axis" x1="40" x2="400" y1="190" y2="190"/>'
    for i in range(0, 10): g += f'<line class="axis" x1="{40 + i * 40}" x2="{40 + i * 40}" y1="190" y2="{198 if i % 2 else 204}"/>'
    g += '<rect class="afade" x="40" y="168" width="160" height="22" rx="11"/><circle class="af" cx="200" cy="179" r="10"/>'
    g += t(120, 230, "hand moves 2 cm", "mut", 12, anchor="middle")
    g += '<path class="a" d="M300,150 A60,60 0 0 1 360,100" fill="none" stroke-width="3" stroke-dasharray="3 5"/>' + t(358, 172, "= 40° turned", "ink", 13, 700, "end") + '</g>'
    # stick panel: stick position and speed bar over time
    g += '<g transform="translate(520,92)">'
    g += '<circle class="soft" cx="90" cy="176" r="50"/><circle class="b" cx="90" cy="176" r="50" fill="none" stroke-width="2.5"/><circle class="bf" cx="112" cy="176" r="19"/>' + t(90, 248, "hold right", "mut", 12, anchor="middle")
    g += '<line class="axis" x1="200" x2="410" y1="206" y2="206"/><path class="bfade" d="M220,206 V150 H360 V206 Z"/><path class="b" d="M220,206 V150 H360 V206" fill="none" stroke-width="3"/>'
    g += t(290, 140, "camera speed", "mut", 12, anchor="middle") + t(290, 232, "turning the whole time you hold", "mut", 12, anchor="middle") + '</g>'
    b += g + t(500, 386, "Nullwire works out how hard to push, and for how long, so the camera ends up exactly where the mouse said.", "ink", 13, 600, "middle")
    open(os.path.join(OUT, "position-vs-speed.svg"), "w").write(svg(w, h, b, "A mouse commands a position while a thumbstick commands a turning speed"))


# ---------------------------------------------------------------------------------------------------------------------
# 3. Payout window (from the real engine)
# ---------------------------------------------------------------------------------------------------------------------
def payout():
    new = [(r[0], r[1]) for r in rows("payout")]
    dt, WMAX, DZ = 0.004, 365.0, 0.10
    old, bl = [], 0.0
    for i in range(70):    # the earlier approach: pay the queued angle out as a fading push
        d = (6.0 / 15) if i < 15 else 0.0
        bl += d; n = max(-1, min(1, bl / max(dt, 0.016) / WMAX)); bl -= n * WMAX * dt
        old.append((i * dt * 1000, DZ + (1 - DZ) * n if n > 1e-9 else 0))
    creep = lambda pts: max([x for x, y in pts if x > 60 and (y - DZ) / (1 - DZ) * WMAX > 3.0] or [60]) - 60   # time the camera is still turning faster than 3 deg/s
    off_old = creep(old); off_new = max([x for x, y in new if x > 60 and y > DZ] or [60]) - 60
    w, h = 1000, 440; c = Chart(110, 116, 940, 340, (0, 280), (0, 0.4))
    b = heading("Stop the push cleanly, or the camera keeps creeping.", "A 6° hand movement, then the hand stops. How long does the stick stay pushed?", w)
    b += c.frame([0, 50, 100, 150, 200, 250], [0, 0.1, 0.2, 0.3, 0.4], "time since the hand started moving (ms)", "stick push", "{:g}", "{:.1f}")
    b += f'<rect class="soft" x="{c.X(0):.1f}" y="{c.Y(DZ):.1f}" width="{c.X(280) - c.X(0):.1f}" height="{c.Y(0) - c.Y(DZ):.1f}" opacity=".7"/>' + t(c.x1 - 8, c.Y(DZ / 2) + 4, "below the line, the game ignores the stick (deadzone)", "mut", 11, anchor="end")
    b += f'<line class="grid" x1="{c.X(60):.1f}" x2="{c.X(60):.1f}" y1="{c.y0}" y2="{c.y1}" stroke-dasharray="3 4"/>' + t(c.X(60) + 6, c.y0 + 12, "hand stops", "mut", 11)
    b += c.path(old, "b") + c.path(new, "a")
    b += t(c.X(112), c.Y(0.375), f"Old way: still creeping ≈{off_old:.0f} ms after you stop", "ink", 12.5, 700)
    b += t(c.X(112), c.Y(0.375) + 17, "and the stick stays above the deadzone far longer", "mut", 12)
    b += t(c.X(112), c.Y(0.265), f"New way: back to zero ≈{off_new:.0f} ms after the hand stops", "ink", 12.5, 700)
    b += f'<circle class="bf" cx="{c.X(30):.1f}" cy="{c.y1 + 62}" r="5"/>' + t(c.X(30) + 12, c.y1 + 66, "old: fading push", "ink", 12)
    b += f'<circle class="af" cx="{c.X(130):.1f}" cy="{c.y1 + 62}" r="5"/>' + t(c.X(130) + 12, c.y1 + 66, "new: fixed 24 ms payout window", "ink", 12)
    open(os.path.join(OUT, "payout-window.svg"), "w").write(svg(w, h + 20, b, "The new payout window returns the stick to zero soon after the hand stops, the old fading push lingers"))


# ---------------------------------------------------------------------------------------------------------------------
# 4. Flick reservoir (from the real engine)
# ---------------------------------------------------------------------------------------------------------------------
def flick():
    short = [(r[0], r[2]) for r in rows("flick24")]
    long_ = [(r[0], r[2]) for r in rows("flick300")]
    ideal = [(ms, 90.0 * min(1, ms / 150.0)) for ms in range(0, 601, 4)]
    w, h = 1000, 460; c = Chart(110, 120, 940, 350, (0, 600), (0, 100))
    b = heading("A fast flick asks for more than a stick can give.", "A 90° flick in 150 ms. The stick tops out near 365°/s, so the rest has to be saved and paid out.", w)
    b += c.frame([0, 100, 200, 300, 400, 500, 600], [0, 25, 50, 75, 100], "time (ms)", "camera turned (degrees)")
    b += f'<line class="grid" x1="{c.x0}" x2="{c.x1}" y1="{c.Y(90):.1f}" y2="{c.Y(90):.1f}" stroke-dasharray="4 4"/>' + t(c.x1, c.Y(90) - 8, "target: 90°", "mut", 11, anchor="end")
    b += c.path(ideal, "g", "6 5") + c.path(short, "b") + c.path(long_, "a")
    sa = short[-1][1]; la = long_[-1][1]
    b += t(c.X(590), c.Y(la) + 22, f"reservoir kept: lands at {la:.0f}°", "ink", 12.5, 700, "end")
    b += t(c.X(590), c.Y(sa) - 10, f"short queue: stops at ≈{sa:.0f}°", "ink", 12.5, 700, "end")
    b += t(c.X(70), c.Y(60), "the hand finished", "mut", 11.5, anchor="middle") + t(c.X(70), c.Y(60) + 15, "at 150 ms", "mut", 11.5, anchor="middle")
    b += f'<circle class="gf" cx="{c.X(20):.1f}" cy="{c.y1 + 62}" r="5"/>' + t(c.X(20) + 12, c.y1 + 66, "the hand (mouse)", "ink", 12)
    b += f'<circle class="af" cx="{c.X(170):.1f}" cy="{c.y1 + 62}" r="5"/>' + t(c.X(170) + 12, c.y1 + 66, "camera, large reservoir", "ink", 12)
    b += f'<circle class="bf" cx="{c.X(370):.1f}" cy="{c.y1 + 62}" r="5"/>' + t(c.X(370) + 12, c.y1 + 66, "camera, small reservoir (today's default)", "ink", 12)
    b += t(500, 438, "Simulated from the real engine. A measured run on the PS5 showed the same pattern: the small reservoir stopped near 52° of a 90° flick.", "mut", 11.5, anchor="middle")
    open(os.path.join(OUT, "flick-reservoir.svg"), "w").write(svg(w, h + 20, b, "A 90 degree flick lands at 90 degrees with a large reservoir and stops short with a small one"))


# ---------------------------------------------------------------------------------------------------------------------
# 5. Measured game response
# ---------------------------------------------------------------------------------------------------------------------
def response():
    pts = [(0.329, 25.6), (0.463, 39.9), (0.597, 55.2), (0.731, 76.2), (0.821, 80.1), (0.893, 88.9), (0.928, 91.6), (0.964, 96.4), (1.0, 100.0)]
    line = [(0.1, 0), (1.0, 100)]
    w, h = 1000, 450; c = Chart(110, 116, 940, 350, (0, 1.0), (0, 100))
    b = heading("What the game does with the stick.", "Measured on a real PS5: how far the stick is pushed, and how fast the camera turns.", w)
    b += c.frame([0, 0.2, 0.4, 0.6, 0.8, 1.0], [0, 25, 50, 75, 100], "stick push (0 = centered, 1 = all the way)", "camera speed (% of the maximum)", "{:g}", "{:g}%")
    b += f'<rect class="soft" x="{c.X(0):.1f}" y="{c.y0}" width="{c.X(0.103) - c.X(0):.1f}" height="{c.y1 - c.y0}" opacity=".8"/>' + t(c.X(0.0515), c.y0 + 18, "deadzone", "mut", 11, anchor="middle") + t(c.X(0.0515), c.y0 + 33, "≈0.10", "mut", 11, anchor="middle")
    b += c.path(line, "g", "6 5") + c.path(pts, "a") + c.dots(pts, "af")
    b += t(c.X(0.18), c.Y(52), "a straight line", "mut", 12) + t(c.X(0.18), c.Y(52) + 16, "(dashed) is almost exactly", "mut", 12) + t(c.X(0.18), c.Y(52) + 32, "what the game does", "mut", 12)
    b += t(c.X(0.86), c.Y(62), "measured turn speed", "ink", 12.5, 700, "middle") + t(c.X(0.86), c.Y(62) + 16, "at full push ≈ 365°/s", "ink", 12.5, 700, "middle")
    b += f'<circle class="af" cx="{c.X(0.33):.1f}" cy="{c.y1 + 62}" r="5"/>' + t(c.X(0.33) + 12, c.y1 + 66, "measured (each dot is a timed turn)", "ink", 12)
    b += f'<line class="g" x1="{c.X(0.66):.1f}" x2="{c.X(0.66) + 28:.1f}" y1="{c.y1 + 62}" y2="{c.y1 + 62}" stroke-width="2.5" stroke-dasharray="6 5"/>' + t(c.X(0.66) + 36, c.y1 + 66, "a perfect straight line", "ink", 12)
    open(os.path.join(OUT, "game-response.svg"), "w").write(svg(w, h + 20, b, "Measured camera speed against stick push follows a straight line past the deadzone"))


# ---------------------------------------------------------------------------------------------------------------------
# 6. Direction pull
# ---------------------------------------------------------------------------------------------------------------------
def pull():
    ang = [0, 15, 30, 45, 60, 75, 90]
    s = {"gentle push (0.3)": [0.8, 12.3, 28.4, 45.5, 62.9, 78.5, 89.6], "medium push (0.6)": [0.9, 9.4, 24.2, 44.8, 65.8, 83.3, 90.1], "hard push (0.9)": [0.8, 6.1, 22.1, 48.2, 71.3, 85.1, 89.9]}
    w, h = 1000, 470; c = Chart(110, 116, 940, 360, (0, 90), (-12, 14))
    b = heading("The game quietly bends your direction.", "How far the camera's direction was from the stick's, at three push strengths. Zero is perfect.", w)
    b += c.frame([0, 15, 30, 45, 60, 75, 90], [-12, -8, -4, 0, 4, 8, 12], "direction you pushed the stick (degrees from straight right)", "camera direction minus stick direction", "{:g}°", "{:+g}°").replace(">+0°<", ">0°<")
    b += f'<line class="axis" x1="{c.x0}" x2="{c.x1}" y1="{c.Y(0):.1f}" y2="{c.Y(0):.1f}"/>'
    b += c.path([(a, v - a) for a, v in zip(ang, s["gentle push (0.3)"])], "a", "") + c.dots([(a, v - a) for a, v in zip(ang, s["gentle push (0.3)"])], "af")
    b += c.path([(a, v - a) for a, v in zip(ang, s["hard push (0.9)"])], "b") + c.dots([(a, v - a) for a, v in zip(ang, s["hard push (0.9)"])], "bf")
    b += t(c.X(44), c.Y(-5), "Hard pushes are pulled toward straight left/right", "ink", 12.5, 700) + t(c.X(44), c.Y(-5) + 17, "(about 9° at 15° and 30°) and toward straight up/down", "mut", 12) + t(c.X(44), c.Y(-5) + 33, "(about 10° at 60° and 75°). That squares off circles.", "mut", 12)
    b += t(c.X(2), c.Y(11.5), "Gentle pushes barely bend:", "ink", 12.5, 700) + t(c.X(2), c.Y(11.5) + 17, "within about 3.5°", "mut", 12)
    b += f'<circle class="af" cx="{c.X(8):.1f}" cy="{c.y1 + 62}" r="5"/>' + t(c.X(8) + 12, c.y1 + 66, "gentle push (0.3)", "ink", 12)
    b += f'<circle class="bf" cx="{c.X(34):.1f}" cy="{c.y1 + 62}" r="5"/>' + t(c.X(34) + 12, c.y1 + 66, "hard push (0.9)", "ink", 12)
    b += t(500, 452, "Nullwire aims a few degrees the other way, so the camera lands in the direction your hand actually moved.", "mut", 11.5, anchor="middle")
    open(os.path.join(OUT, "direction-pull.svg"), "w").write(svg(w, h + 20, b, "At hard pushes the game bends the camera direction by several degrees, gentle pushes barely bend it"))


# ---------------------------------------------------------------------------------------------------------------------
# 7. Circle accuracy
# ---------------------------------------------------------------------------------------------------------------------
def circles():
    names = ["3° circle\n8°/s", "5° circle\n10°/s", "5° circle\n39°/s", "10° circle\n42°/s", "20° circle\n105°/s", "10° circle\n209°/s", "20° circle\n251°/s", "30° circle\n314°/s"]
    old = [0.2, 0.1, 0.4, 0.3, 0.5, 7.5, 4.2, 2.3]; new = [0.0, 0.5, 0.2, 0.5, 0.5, 9.7, 3.5, 1.6]
    w, h = 1000, 480; c = Chart(110, 140, 950, 360, (0, 8), (0, 12))
    b = heading("Drawing circles with the mouse.", "After a whole circle, how far was the camera from where it started? Lower is better.", w)
    b += c.frame([], [0, 3, 6, 9, 12], "", "degrees off after the circle", "{:g}", "{:g}°")
    bw = 26
    for i, n in enumerate(names):
        cx = c.X(i + 0.5)
        for j, (v, cl) in enumerate(((old[i], "bf"), (new[i], "af"))):
            x = cx + (j - 1) * bw - (0 if j else 0) + (0 if j else 0)
            x = cx - bw - 1 + j * (bw + 2)
            hh = max(v / 12 * (c.y1 - c.y0), 1.5)
            b += f'<rect class="{cl}" x="{x:.1f}" y="{c.y1 - hh:.1f}" width="{bw}" height="{hh:.1f}" rx="4"/>'
            b += t(x + bw / 2, c.y1 - hh - 6, f"{v:.1f}", "ink", 11.5, 600, "middle")
        for k, line in enumerate(n.split("\n")): b += t(cx, c.y1 + 20 + k * 16, line, "mut", 11.5, anchor="middle")
    b += f'<rect class="soft" x="{c.X(0.05):.1f}" y="{c.y0 - 6}" width="{c.X(4.95) - c.X(0.05):.1f}" height="26" rx="8"/>' + t((c.X(0.05) + c.X(4.95)) / 2, c.y0 + 12, "slow to medium: about half a degree or better", "ink", 12, 700, "middle")
    b += f'<rect class="soft" x="{c.X(5.05):.1f}" y="{c.y0 - 6}" width="{c.X(7.95) - c.X(5.05):.1f}" height="26" rx="8"/>' + t((c.X(5.05) + c.X(7.95)) / 2, c.y0 + 12, "fast: a few degrees, sometimes more", "ink", 12, 700, "middle")
    b += f'<rect class="bf" x="{c.X(1.3):.1f}" y="{c.y1 + 62}" width="14" height="14" rx="3"/>' + t(c.X(1.3) + 22, c.y1 + 74, "first settings", "ink", 12)
    b += f'<rect class="af" x="{c.X(3.2):.1f}" y="{c.y1 + 62}" width="14" height="14" rx="3"/>' + t(c.X(3.2) + 22, c.y1 + 74, "with the measured corrections", "ink", 12)
    b += t(500, 470, "Camera position compared from the first and last screenshot, two runs each. At fast speeds the runs vary more.", "mut", 11.5, anchor="middle")
    open(os.path.join(OUT, "circle-accuracy.svg"), "w").write(svg(w, h + 10, b, "After drawing circles, slow and medium ones end within about half a degree, fast ones end a few degrees off"))


# ---------------------------------------------------------------------------------------------------------------------
# 8. Wi-Fi scan
# ---------------------------------------------------------------------------------------------------------------------
def wifi():
    w, h = 1000, 420; c = Chart(110, 110, 500, 320, (0, 2), (0, 24))
    b = heading("The one thing that hurts: Wi-Fi hiccups.", "Aiming error with the network quiet, and while the Mac scanned for Wi-Fi. 12 runs each.", w)
    b += c.frame([], [0, 6, 12, 18, 24], "", "degrees off at the end", "{:g}", "{:g}°")
    for i, (lab, v, cl) in enumerate((("quiet network", 1.18, "af"), ("during a Wi-Fi scan", 20.65, "bf"))):
        x = c.X(i + 0.5) - 55; hh = v / 24 * (c.y1 - c.y0)
        b += f'<rect class="{cl}" x="{x:.1f}" y="{c.y1 - hh:.1f}" width="110" height="{hh:.1f}" rx="6"/>' + t(x + 55, c.y1 - hh - 10, f"{v:.1f}°", "ink", 16, 700, "middle") + t(x + 55, c.y1 + 22, lab, "mut", 12.5, anchor="middle")
    b += '<g transform="translate(580,110)"><rect class="soft" width="360" height="232" rx="14"/>'
    b += t(22, 36, "Why a late packet matters", "ink", 15, 700)
    for i, line in enumerate(["A stick sets a camera SPEED.", "If an update arrives late, the camera", "keeps turning at the old speed until it", "finally shows up.", "", "Faster resends and priority tags didn't", "help: the packets are late, not lost.", "Wired Ethernet is the planned fix."]):
        b += t(22, 64 + i * 19, line, "mut", 12.5)
    b += "</g>" + t(500, 392, "Measured: pings jumped from a median of 3.6 ms to about 20 ms (worst around 190 ms) while a scan ran.", "mut", 11.5, anchor="middle")
    open(os.path.join(OUT, "wifi-scan.svg"), "w").write(svg(w, h, b, "A Wi-Fi scan raises aiming error from about one degree to about twenty degrees"))


# ---------------------------------------------------------------------------------------------------------------------
# 9. Calibration stages
# ---------------------------------------------------------------------------------------------------------------------
def calib():
    w, h = 1000, 420
    stages = [("1", "Settle", "Hold still. Measure how much the picture wobbles on its own.", "≈ 1 s"),
              ("2", "Latency", "Tap the stick. Time how long until the picture moves.", "≈ 5 s"),
              ("3", "Deadzone", "Raise the push 1% at a time until the camera starts to move.", "≈ 5 to 20 s"),
              ("4", "Response curve", "Hold 12 different pushes and measure the speed of each.", "≈ 15 s"),
              ("5", "Ramp check", "Does the speed keep building while held? (ease-in / smoothing)", "≈ 2 s"),
              ("6", "Full turn", "Hold full push until the view repeats: that's 360° and the real °/s.", "≈ 1 s")]
    b = heading("The game gets measured by watching it.", "Press one key. About 30 seconds later it knows the deadzone, curve, turn speed and delay.", w)
    for i, (n, name, desc, tm) in enumerate(stages):
        x = 28 + i * 160; y = 100
        b += f'<g transform="translate({x},{y})"><rect class="soft" width="148" height="240" rx="14"/><circle class="{"af" if i % 2 == 0 else "bf"}" cx="30" cy="34" r="16"/>' + t(30, 40, n, "ink", 14, 800, "middle", 'fill="#fff" style="fill:#fff"')
        b += t(18, 82, name, "ink", 15, 700)
        words, line, ls = desc.split(), "", []
        for wd in words:
            if len(line + " " + wd) > 19: ls.append(line); line = wd
            else: line = (line + " " + wd).strip()
        ls.append(line)
        for k, l in enumerate(ls): b += t(18, 108 + k * 18, l, "mut", 12.5)
        b += t(18, 224, tm, "ink", 12, 700) + "</g>"
        if i < 5: b += f'<path class="gf" d="M{x + 150},220 l8,5 l-8,5 z"/>'
    b += t(500, 386, "Everything runs on the game's own video. If something is wrong (a blank wall, smoothing on), it stops and says why.", "mut", 12.5, anchor="middle")
    open(os.path.join(OUT, "calibration-stages.svg"), "w").write(svg(w, h, b, "The six stages of automatic calibration"))


# ---------------------------------------------------------------------------------------------------------------------
# 10. Scorecard
# ---------------------------------------------------------------------------------------------------------------------
def scorecard():
    w, h = 1000, 330
    cards = [("0.10", "deadzone", "measured, repeatable", "af"), ("365°/s", "full-stick turn speed", "from a real 360° turn", "bf"), ("≈ 0.2°", "tracking error", "per ¼ second, random paths up to 120°/s", "af"),
             ("≤ 0.5°", "circle closure", "slow to medium circles", "bf"), ("250/s", "stick updates", "own high-priority thread", "af"), ("≈ 80 ms", "stick → picture", "on the computer, video included", "bf")]
    b = heading("The numbers so far.", "All measured on one real PS5. Where it falls short, the README says so.", w)
    for i, (big, lab, sub, cl) in enumerate(cards):
        x = 28 + (i % 3) * 318; y = 92 + (i // 3) * 112
        b += f'<g transform="translate({x},{y})"><rect class="soft" width="300" height="98" rx="14"/><rect class="{cl}" x="0" y="0" width="6" height="98" rx="3"/>' + t(24, 46, big, "ink", 30, 800) + t(24, 68, lab, "ink", 13.5, 700) + t(24, 86, sub, "mut", 11.5) + "</g>"
    open(os.path.join(OUT, "scorecard.svg"), "w").write(svg(w, h, b, "Headline measured numbers"))


if __name__ == "__main__":
    for f in (hero, position_vs_speed, payout, flick, response, pull, circles, wifi, calib, scorecard): f()
    print("wrote", len(os.listdir(OUT)), "figures to", os.path.abspath(OUT))
