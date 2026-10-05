#!/usr/bin/env python3
# 0.14 stage 2 test (needs the app built on Linux, run headless on Mesa llvmpipe, like tests/check_window_pictures.py):
#   check_y2k_pictures.py <PhysicsLab binary> <work folder>
# The Y2K themes (and Aero) add decoration that must never cover content, a title font that must stay on titles, and page textures whose strength was only modelled so far.
# Each check draws the SAME screen twice with one thing switched off by a test switch (PL_DECOR=off: no brackets, HUD rules or label; PL_TITLEFONT=off: the bold face for
# titles; PL_FX=off: no page textures) and compares the pictures:
#   decor      pixels may differ only in a 7 point band along the outer edge of a window (the brackets and rules are drawn 3 to 6 points in; every window has at least 8 points
#              of padding there) and in the title bar of a floating window (the tiny spec label). Content is anywhere else: it must be the same pixels (one level of 255 is
#              allowed, a rounding of the rasteriser; the first run of this test found 6 such pixels in the guide).
#   titlefont  the inside of a floating window (below its title bar, 14 points in from its edge) is the same picture with the title font and without: the BODY font is the same
#              and the bar keeps its height, so the text does not move. (The app name in the header changes the header's height, so the header is not compared.)
#   textures   the pixels of the page, with and without textures, differ by at most the model's bound: the sum of the textures (PageFx in app/theme_types.h) times 255, plus
#              2 for rounding; the strongest measured difference is printed (it is far under the bound under the glass).
#   glow       over the dark picture, the pixels 4 points outside a floating window's edge are brighter than the pixels 40 points outside it (the outer glow); Dark has none.
#   hover      a hovered button and a tooltip draw without a crash and change the picture.
import os, struct, subprocess, sys, re

def run(binary, work, name, theme, extra):
    env = dict(os.environ, HOME=os.path.join(work, "home"), SDL_VIDEODRIVER="offscreen", PL_WINDOW="1000x640", PL_MOTION="on", PL_FIXED_DT="0.016",
               PL_DYK_FIRST="3", PL_DATE="2026-10-05", PL_EXIT_AFTER_FRAMES="40", PL_THEME=theme, PL_LOG_WINDOWS="1")
    env.update(extra)
    shot = os.path.join(work, name + ".bmp")
    env["PL_SCREENSHOT"] = shot
    os.makedirs(env["HOME"], exist_ok=True)
    r = subprocess.run([binary], env=env, cwd=work, capture_output=True, text=True, timeout=300)
    wins, bars, texts = {}, {}, {}
    for line in r.stderr.splitlines():
        m = re.match(r"PLWIN (.*) (-?\d+) (-?\d+) (\d+) (\d+) (docked|floating)$", line)
        if m: wins[m.group(1)] = (int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5)), m.group(6))
        m = re.match(r"PLTITLEBAR (.*) (-?\d+) (-?\d+) (\d+) (\d+) [0-9.]+$", line)
        if m: bars[m.group(1)] = (int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5)))
        m = re.match(r"PLTITLETEXT (\S+) (-?\d+) (-?\d+) (\d+) (\d+)$", line)
        if m: texts[m.group(1)] = (int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5)))
    return shot, wins, bars, texts

def load(path):
    d = open(path, "rb").read()
    off = struct.unpack("<I", d[10:14])[0]
    w, h = struct.unpack("<ii", d[18:26]); bpp = struct.unpack("<H", d[28:30])[0]
    bp = bpp // 8; stride = (w * bp + 3) // 4 * 4
    px = d[off:]
    rows = [px[(h - 1 - y if h > 0 else y) * stride:][:w * bp] for y in range(abs(h))]
    return w, abs(h), bp, rows

def inside(x, y, rect, m=0):
    rx, ry, rw, rh = rect[:4]
    return rx - m <= x < rx + rw + m and ry - m <= y < ry + rh + m

def diff_pixels(a, b):
    w, h, bp, ra = a; _, _, _, rb = b
    out = []
    for y in range(h):
        if ra[y] == rb[y]: continue
        for x in range(w):
            if ra[y][x * bp:x * bp + 3] != rb[y][x * bp:x * bp + 3]:
                out.append((x, y, max(abs(ra[y][x * bp + k] - rb[y][x * bp + k]) for k in range(3))))
    return out

problems = 0
def report(ok, text):
    global problems
    print(("ok   " if ok else "FAIL ") + text)
    if not ok: problems += 1

def on_edge(x, y, wins, band):          # in the band along the outer edge of any window
    for r in wins.values():
        if inside(x, y, r, 0) and not inside(x, y, (r[0] + band, r[1] + band, r[2] - 2 * band, r[3] - 2 * band)): return True
    return False

def main():
    binary, work = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    os.makedirs(work, exist_ok=True)
    screens = {"orbit": {}, "guide": {"PL_LEARN": "1"}, "settings": {"PL_OPEN": "settings"}}
    # the picture area is not a window: leave it out of "outside every window" by giving it nothing; only windows count
    for theme in ("y2kice", "y2knight"):
        for sname, extra in screens.items():
            on, wins, bars, texts = run(binary, work, f"{sname}_{theme}_on", theme, extra)
            off, _, _, _ = run(binary, work, f"{sname}_{theme}_decoff", theme, dict(extra, PL_DECOR="off"))
            d = diff_pixels(load(on), load(off))
            bad = [p for p in d if p[2] > 1 and not (on_edge(p[0], p[1], wins, 7) or any(inside(p[0], p[1], b) for b in bars.values()))]
            report(len(d) > 0 and not bad, f"{theme} {sname}: decoration only in the 7 pt edge band and title bars: {len(d)} pixels differ, {len(bad)} outside" + (f" (first {bad[:3]})" if bad else ""))
            if sname != "orbit":                                                      # the floating windows: guide and Settings
                tf, wins2, bars2, _ = run(binary, work, f"{sname}_{theme}_titlefontoff", theme, dict(extra, PL_TITLEFONT="off"))
                win = [k for k, v in wins.items() if v[4] == "floating"][-1]
                x, y, w, h, _ = wins[win]
                th = bars[win][3]
                box = (x + 14, y + th + 14, x + w - 14, y + h - 14)
                da = diff_pixels(load(on), load(tf))
                bad = [p for p in da if box[0] <= p[0] < box[2] and box[1] <= p[1] < box[3]]
                area = (box[2] - box[0]) * (box[3] - box[1])
                worst = max((p[2] for p in bad), default=0)
                ok = len(bad) <= 0.0003 * area and worst <= 40       # the same tolerance as check_window_pictures.py: an anti-aliased letter edge rounds differently in two runs
                report(ok and wins2[win][:4] == wins[win][:4] and bars2[win][3] == th, f"{theme} {sname}: the window's inside is the same with the title font off: {len(bad)} of {area} pixels differ (worst {worst}/255); same window box and bar height")
        # textures: at most the model's bound
        on, wins, _, _ = run(binary, work, f"orbit_{theme}_fxon", theme, {})
        off, _, _, _ = run(binary, work, f"orbit_{theme}_fxoff", theme, {"PL_FX": "off"})
        d = diff_pixels(load(on), load(off))
        bound = {"y2kice": 0.08 + 0.12 + 0.10, "y2knight": 1 - (1 - (0.08 + 0.10 + 0.10)) * (1 - 0.45)}[theme] * 255 + 2   # PageFx numbers of the theme (see test_theme_stage2.inc)
        worst = max((p[2] for p in d), default=0)
        report(len(d) > 1000 and worst <= bound, f"{theme}: page textures change {len(d)} pixels by at most {worst} (the model's bound is {bound:.0f}/255)")
    # glow over the dark picture
    for theme, want in (("aero", True), ("y2kice", True), ("y2knight", True), ("dark", False)):
        pic, wins, bars, _ = run(binary, work, f"guide_{theme}_glow", theme, {"PL_LEARN": "1"})
        fl = [v for v in wins.values() if v[4] == "floating"][-1]
        w, h, bp, rows = load(pic)
        def col(x):   # mean brightness of a column at x over the middle 100 rows of the window's left side
            vals = [sum(rows[y][x * bp:x * bp + 3]) / 3 for y in range(fl[1] + fl[3] // 2 - 50, fl[1] + fl[3] // 2 + 50) if 0 <= x < w]
            return sum(vals) / len(vals)
        near, far = col(fl[0] - 4), col(fl[0] - 40)
        report((near > far + 5) == want, f"{theme}: outer glow over the dark picture: {near:.1f} 4 pt outside the edge against {far:.1f} 40 pt outside ({'brighter' if want else 'not brighter'} expected)")
    # hover and tooltip
    for theme in ("aero", "y2kice", "y2knight"):
        a, _, _, _ = run(binary, work, f"orbit_{theme}_nomouse", theme, {})
        b, _, _, _ = run(binary, work, f"orbit_{theme}_hover", theme, {"PL_MOUSE": "766,256"})
        d = diff_pixels(load(a), load(b))
        report(len(d) > 200, f"{theme}: a hovered button with its tooltip changes the picture ({len(d)} pixels)")
    print(f"{problems} problems")
    sys.exit(1 if problems else 0)
main()
