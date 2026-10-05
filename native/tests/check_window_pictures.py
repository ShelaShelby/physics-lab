#!/usr/bin/env python3
# 0.14 stage 1.1 test (needs the app built on Linux, run headless on Mesa llvmpipe, like tests/check_theme_pictures.sh):
#   check_window_pictures.py <PhysicsLab binary> <work folder>
# A floating window that holds text must hide what is behind it. The owner saw the Learn cards and the Solar System readout through the guide window in
# Frutiger Aero. For each theme (Aero and two old ones: Dark, Light) the SAME window is drawn over two different backdrops and the inside of the window
# (inset 14 px from its edge, where the soft corner and shadow are) must be the same picture, within a stated tolerance:
#   guide     the Orbit Lab guide over the Orbit Lab picture       against   over the Learn tab
#   changelog "Orbit Lab changelog" window over the picture        against   over the Learn tab
#   layering  the Settings window over the picture while an overlay card (Earth selected: its facts card, bottom right of the picture) sits behind it, against no card:
#             the window must be on top. (The guide itself shows the selected body in its own card, so it cannot be used here.)
#   docked    the side panel after 40 frames against after 70 (the picture next to it has moved): a docked panel is over the page only
# Tolerance: at most 0.03 % of the pixels in the region may differ, none by more than 40 of 255 in any channel. Measured: 14 to 15 pixels, worst 29: the anti-aliased
# edge of one letter (the text atlas rounds a glyph's edge column differently in two runs). Content showing through a window is thousands of pixels (Aero before the fix: see REPORT.md).
import os, struct, subprocess, sys, re

def run(binary, work, name, theme, extra):
    env = dict(os.environ, HOME=os.path.join(work, "home"), SDL_VIDEODRIVER="offscreen", PL_WINDOW="1000x640", PL_MOTION="on", PL_FIXED_DT="0.016",
               PL_DYK_FIRST="3", PL_DATE="2026-10-05", PL_EXIT_AFTER_FRAMES="40", PL_THEME=theme, PL_LOG_WINDOWS="1")
    env.update(extra)
    shot = os.path.join(work, name + ".bmp")
    env["PL_SCREENSHOT"] = shot
    os.makedirs(env["HOME"], exist_ok=True)
    r = subprocess.run([binary], env=env, cwd=work, capture_output=True, text=True, timeout=300)
    rects = {}
    for line in r.stderr.splitlines():
        m = re.match(r"PLWIN (.*) (-?\d+) (-?\d+) (\d+) (\d+) (docked|floating)$", line)
        if m: rects[m.group(1)] = (int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5)), m.group(6))
    return shot, rects

def load(path):
    d = open(path, "rb").read()
    off = struct.unpack("<I", d[10:14])[0]
    w, h = struct.unpack("<ii", d[18:26]); bpp = struct.unpack("<H", d[28:30])[0]
    assert bpp in (24, 32), bpp
    bp = bpp // 8; stride = (w * bp + 3) // 4 * 4
    return w, abs(h), d[off:], stride, bp, h > 0

def region_diff(a, b, box):
    wa, ha, da, sa, bp, bu = a; wb, hb, db, sb, _, _ = b
    x0, y0, x1, y1 = box
    n = bad = worst = 0
    for y in range(y0, y1):
        ry = (ha - 1 - y) if bu else y
        ra = da[ry * sa + x0 * bp: ry * sa + x1 * bp]; rb = db[ry * sb + x0 * bp: ry * sb + x1 * bp]
        n += x1 - x0
        if ra == rb: continue
        for i in range(0, len(ra), bp):
            d = max(abs(ra[i + k] - rb[i + k]) for k in range(3))
            if d: bad += 1; worst = max(worst, d)
    return n, bad, worst

def inset(rect, m=14):
    x, y, w, h = rect[:4]
    return (max(0, x + m), max(0, y + m), min(1000, x + w - m), min(640, y + h - m))

def main():
    binary, work = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    os.makedirs(work, exist_ok=True)
    problems = 0
    def check(label, a, b, box):
        nonlocal problems
        n, bad, worst = region_diff(load(a), load(b), box)
        ok = n > 0 and bad <= 0.0003 * n and worst <= 40
        print(f"{'ok  ' if ok else 'FAIL'} {label}: {bad} of {n} pixels differ (worst {worst}/255)")
        if not ok: problems += 1
    for theme in ("aero", "y2kice", "y2knight", "dark", "light"):
        guide_a, ra = run(binary, work, f"guide_pic_{theme}", theme, {"PL_LEARN": "1"})
        guide_b, rb = run(binary, work, f"guide_learn_{theme}", theme, {"PL_LEARN": "1", "PL_PANE": "1"})
        win = [k for k, v in ra.items() if v[4] == "floating"]
        assert win, "no floating window logged: " + str(ra)
        box = inset(ra[win[-1]])
        check(f"{theme}: guide over the picture / over the Learn cards", guide_a, guide_b, box)
        hist_a, ha = run(binary, work, f"hist_pic_{theme}", theme, {"PL_OPEN": "history"})
        hist_b, _ = run(binary, work, f"hist_learn_{theme}", theme, {"PL_OPEN": "history", "PL_PANE": "1"})
        fw = [k for k, v in ha.items() if v[4] == "floating"]
        assert fw, "no changelog window logged"
        check(f"{theme}: changelog over the picture / over the Learn cards", hist_a, hist_b, inset(ha[fw[-1]]))
        set_a, rs = run(binary, work, f"settings_nocard_{theme}", theme, {"PL_OPEN": "settings"})
        set_b, _ = run(binary, work, f"settings_card_{theme}", theme, {"PL_OPEN": "settings", "PL_SELECT": "Earth"})
        sw = [k for k, v in rs.items() if v[4] == "floating"]
        assert sw, "no Settings window logged"
        check(f"{theme}: layering, Settings on top of the Earth card / no card", set_b, set_a, inset(rs[sw[-1]]))
        dk_a, rd = run(binary, work, f"dock_a_{theme}", theme, {})
        dk_b, _ = run(binary, work, f"dock_b_{theme}", theme, {"PL_EXIT_AFTER_FRAMES": "70"})
        dock = [v for k, v in rd.items() if k == "Controls"]
        assert dock, "no Controls panel logged: " + str(rd)
        check(f"{theme}: side panel after 40 / after 70 frames (the planets move behind it)", dk_a, dk_b, inset(dock[0], 10))
    print(f"{problems} problems")
    sys.exit(1 if problems else 0)
main()
