#!/usr/bin/env python3
# 0.14 stage 3 test (needs the app built on Linux, headless on Mesa llvmpipe, like tests/check_y2k_pictures.py):
#   check_effects_pictures.py <PhysicsLab binary> <work folder>
# Settings -> Theme effects Off (PL_EFFECTS=off is the same switch for tests; the saved setting is checked too) must remove every decoration of Frutiger Aero, Y2K Ice and
# Y2K Night and nothing else:
#   page      (Aero: clouds and bubbles are in the header: thousands of pixels differ on against off) with effects off the page in the header's left margin (column x = 8, rows 20..190, no content there) is the plain gradient: its second difference stays at most 2 of
#             255 per channel (a texture, a grid line, a cloud edge or a speck breaks that); with effects on, Ice and Night break it (grain, grid) and Aero's clouds change the column.
#   brackets  the corner bracket arm (row 3, columns 4..12) is the gradient with effects off (within 3) and a line (more than 20 different) with effects on (Y2K only).
#   inside    the inside of a floating window (below its title bar, 14 points in) is the same picture on and off: colours, gloss and fonts are unchanged (the tolerance of
#             check_window_pictures.py: at most 0.03 % of the pixels, none over 40).
#   saved     the setting in the settings file (effects=off) draws the same picture as the test switch.
#   others    Dark and Light have no decoration: the same picture on and off, to the pixel.
import os, struct, subprocess, sys, re

def run(binary, work, name, theme, extra, home_file=None):
    home = os.path.join(work, "home_" + name)
    os.makedirs(home, exist_ok=True)
    if home_file:
        d = os.path.join(home, ".local", "share", "PhysicsLab", "PhysicsLab"); os.makedirs(d, exist_ok=True)
        open(os.path.join(d, "settings.txt"), "w").write(home_file)
    env = {k: v for k, v in os.environ.items() if k != "XDG_DATA_HOME"}
    env.update(HOME=home, SDL_VIDEODRIVER="offscreen", PL_WINDOW="1000x640", PL_MOTION="on", PL_FIXED_DT="0.016", PL_DYK_FIRST="3", PL_DATE="2026-10-05",
               PL_EXIT_AFTER_FRAMES="40", PL_LOG_WINDOWS="1")
    if theme: env["PL_THEME"] = theme
    env.update(extra)
    shot = os.path.join(work, name + ".bmp"); env["PL_SCREENSHOT"] = shot
    r = subprocess.run([binary], env=env, cwd=work, capture_output=True, text=True, timeout=300)
    wins, bars = {}, {}
    for line in r.stderr.splitlines():
        m = re.match(r"PLWIN (.*) (-?\d+) (-?\d+) (\d+) (\d+) (docked|floating)$", line)
        if m: wins[m.group(1)] = (int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5)), m.group(6))
        m = re.match(r"PLTITLEBAR (.*) (-?\d+) (-?\d+) (\d+) (\d+) [0-9.]+$", line)
        if m: bars[m.group(1)] = (int(m.group(2)), int(m.group(3)), int(m.group(4)), int(m.group(5)))
    return shot, wins, bars

def load(path):
    d = open(path, "rb").read()
    off = struct.unpack("<I", d[10:14])[0]
    w, h = struct.unpack("<ii", d[18:26]); bpp = struct.unpack("<H", d[28:30])[0]
    bp = bpp // 8; stride = (w * bp + 3) // 4 * 4
    return w, abs(h), bp, [d[off:][(h - 1 - y if h > 0 else y) * stride:][:w * bp] for y in range(abs(h))]

def px(img, x, y): w, h, bp, rows = img; return tuple(rows[y][x * bp:x * bp + 3])
def diff(a, b, box=None):
    w, h, bp, ra = a; _, _, _, rb = b
    out = []
    for y in range(h):
        if ra[y] == rb[y]: continue
        for x in range(w):
            if box and not (box[0] <= x < box[2] and box[1] <= y < box[3]): continue
            d = max(abs(ra[y][x * bp + k] - rb[y][x * bp + k]) for k in range(3))
            if d: out.append(d)
    return out
def second_diff(img, x, y0, y1):
    worst = 0
    for y in range(y0, y1 - 2):
        a, b, c = px(img, x, y), px(img, x, y + 1), px(img, x, y + 2)
        worst = max(worst, max(abs(a[k] - 2 * b[k] + c[k]) for k in range(3)))
    return worst

problems = 0
def report(ok, text):
    global problems
    print(("ok   " if ok else "FAIL ") + text)
    if not ok: problems += 1

def main():
    binary, work = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    os.makedirs(work, exist_ok=True)
    for theme in ("aero", "y2kice", "y2knight"):
        on, wins, bars = run(binary, work, f"orbit_{theme}_on", theme, {})
        off, _, _ = run(binary, work, f"orbit_{theme}_off", theme, {"PL_EFFECTS": "off"})
        a, b = load(on), load(off)
        report(len(diff(a, b)) > 1000, f"{theme}: effects off changes the picture ({len(diff(a, b))} pixels)")
        sd_off, sd_on = second_diff(b, 8, 20, 190), second_diff(a, 8, 20, 190)
        report(sd_off <= 2, f"{theme}: effects off, the page margin is the plain gradient (second difference {sd_off} <= 2)")
        if theme != "aero": report(sd_on > 2, f"{theme}: effects on, the margin has grain / grid (second difference {sd_on} > 2: the test can tell)")
        else:
            dh = len(diff(a, b, (0, 0, 1000, 208)))
            report(dh > 2000, f"{theme}: effects on, clouds and bubbles change the header ({dh} pixels differ from effects off)")
        if theme != "aero":
            bo = max(max(abs(px(b, x, 3)[k] - px(b, x, 5)[k]) for k in range(3)) for x in range(4, 13))     # off: the arm row against the rows two below it: the same gradient colour
            ba = max(max(abs(px(a, x, 3)[k] - px(a, x, 5)[k]) for k in range(3)) for x in range(4, 13))     # on: a line, very different from the rows below it
            report(bo <= 3 and ba > 20, f"{theme}: corner bracket arm (row 3, columns 4..12): off differs from the rows below by {bo} (<= 3, the gradient), on by {ba} (> 20, a line)")
        g_on, wg, bg = run(binary, work, f"guide_{theme}_on", theme, {"PL_LEARN": "1"})
        g_off, _, _ = run(binary, work, f"guide_{theme}_off", theme, {"PL_LEARN": "1", "PL_EFFECTS": "off"})
        win = [k for k, v in wg.items() if v[4] == "floating"][-1]
        x, y, w, h, _ = wg[win]; th = bg[win][3]
        box = (x + 14, y + th + 14, x + w - 14, y + h - 14)
        d = diff(load(g_on), load(g_off), box); area = (box[2] - box[0]) * (box[3] - box[1])
        report(len(d) <= 0.0003 * area and max(d, default=0) <= 40, f"{theme}: the window's inside is the same on and off: {len(d)} of {area} pixels differ (worst {max(d, default=0)}/255)")
        h_on, _, _ = run(binary, work, f"hover_{theme}_on", theme, {"PL_MOUSE": "766,256"})
        h_off, _, _ = run(binary, work, f"hover_{theme}_off", theme, {"PL_MOUSE": "766,256", "PL_EFFECTS": "off"})
        btn = (722, 245, 810, 267)
        dh = diff(load(h_on), load(h_off), btn)
        if theme != "aero": report(len(dh) > 30, f"{theme}: hovered button: the glint and brush lines are gone with effects off ({len(dh)} pixels in the button differ)")
        sv, _, _ = run(binary, work, f"saved_{theme}", None, {}, home_file=f"theme={theme}\neffects=off\n")
        report(len(diff(load(sv), b)) == 0, f"{theme}: the saved setting effects=off draws the same picture as the switch")
    for theme in ("dark", "light"):
        a, _, _ = run(binary, work, f"orbit_{theme}_on", theme, {})
        b, _, _ = run(binary, work, f"orbit_{theme}_off", theme, {"PL_EFFECTS": "off"})
        report(len(diff(load(a), load(b))) == 0, f"{theme}: no decoration, the same picture on and off, 0 differing pixels")
    print(f"{problems} problems")
    sys.exit(1 if problems else 0)
main()
