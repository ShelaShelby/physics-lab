#!/usr/bin/env python3
# 0.15 stage 5t, 0.38: the Controls panel's look, in the REAL app (Linux, software Vulkan; NOT Windows, NOT a real GPU), on the picture's own pixels. tests/TESTS.md lists the parts kept and why.
#   grey   a selected segment is a soft accent tint with a 1 px accent edge (Monochrome / High contrast: the solid accent fill); a checked box is the accent fill
#          with its tick. Checked in four themes: the edge against the unselected face at least 3:1 (WCAG 1.4.11), the label on the fill at least 4.5:1, the tick
#          on the box at least 3:1, the tint is not grey (except Monochrome). The suite's one look test of the themes.
#   tiles  the Settings tile names are inside their tiles and the picture dims behind Settings
#   check_5t.py <PhysicsLab binary> <work folder> grey | tiles
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, Bmp

fails = checks = 0

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def lin(c):
    c /= 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

def lum(rgb):
    r, g, b = (lin(v) for v in rgb)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b

def contrast(a, b):
    la, lb = lum(a), lum(b)
    return (max(la, lb) + 0.05) / (min(la, lb) + 0.05)

THEMES = ("dark", "sage", "light", "monochrome")

def inside(r, pc):
    return pc[0] <= r[0] and r[0] + r[2] <= pc[0] + pc[2] and pc[1] <= r[1] and r[1] + r[3] <= pc[1] + pc[3]

def label_px(b, r, fill):
    """the pixel in the rectangle that differs most from the fill: the label's (or the check mark's) own colour"""
    x, y, w, h = (int(round(v)) for v in r)
    best = fill
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            p = b.px(xx, yy)
            if contrast(p, fill) > contrast(best, fill):
                best = p
    return best

def part_grey(binary, work):
    for t in THEMES:
        r = run_app(binary, work, f"t_grey_{t}", {"PL_NEWUI": "on", "PL_SCRIPT": "10:wheel=0"}, frames=24, theme=t, size="1920x1080")
        b = Bmp(r.bmp)
        pc = r.rects.get("panel.controls")
        check(pc is not None, f"{t}: the Controls panel is there")
        if not pc:
            continue
        sel = {k: v for k, v in r.rects.items() if k.startswith("seg.") and ".sel" in k and inside(v, pc)}
        off = {k: v for k, v in r.rects.items() if k.startswith("seg.") and ".off" in k and inside(v, pc)}
        box = {k: v for k, v in r.rects.items() if k.startswith("check.") and k.endswith(".box") and inside(v, pc)}
        check(len(sel) >= 1 and len(off) >= 1 and len(box) >= 1, f"{t}: found {len(sel)} selected segments, {len(off)} unselected, {len(box)} checked boxes in the panel")
        face = None
        if off:
            o = next(iter(off.values()))
            face = b.px(int(o[0] + 2), int(o[1] + 2))
        for k, v in sel.items():
            fp = b.px(int(v[0] + 4), int(v[1] + 4))                       # inside the tint, away from its edge and the label
            ep = b.px(int(v[0] + v[2] / 2), int(v[1]))                     # on the 1 px edge (the rectangle is the tint's own, its edge drawn on it)
            if t != "monochrome":
                check(max(fp) - min(fp) > 2, f"{t}: {k}: the fill {fp} is a tint of the accent, not grey")
            lp = label_px(b, v, fp)
            check(contrast(lp, fp) >= 4.5, f"{t}: {k}: the label {lp} on the fill {fp}: contrast {contrast(lp, fp):.2f} (at least 4.5)")
            if face:
                mark = ep if t != "monochrome" else fp
                check(contrast(mark, face) >= 3.0, f"{t}: {k}: the choice's mark {mark} against the unselected face {face}: contrast {contrast(mark, face):.2f} (at least 3)")
        # 0.38.1: the "closest to reality" mark on a chosen part keeps at least 3 px of the tint between it and the 1 px accent edge (it was 2 px and looked stuck to the edge).
        # The rule is our own design choice (the owner: "the mark sits right on its edge"), written here, not read from the app.
        dpi = r.window[2] if r.window else 1.0
        for k, v in sel.items():
            seg = k[len("seg."):k.rindex(".sel")]
            m = r.rects.get("real." + seg)
            if m and v[0] <= m[0] and m[0] + m[2] <= v[0] + v[2] and v[1] <= m[1] and m[1] + m[3] <= v[1] + v[3]:
                right, top = (v[0] + v[2]) - (m[0] + m[2]), m[1] - v[1]
                check(right >= 1 + 3 * dpi - 0.5 and top >= 1 + 3 * dpi - 0.5, f"{t}: {k}: the reality mark is 3 px inside the accent edge (right {right - 1:.1f}, top {top - 1:.1f} px)")
        for k, v in box.items():
            fp = b.px(int(v[0] + 2), int(v[1] + 2))
            lp = label_px(b, v, fp)
            check(contrast(lp, fp) >= 3.0, f"{t}: {k}: the tick {lp} on the box {fp}: contrast {contrast(lp, fp):.2f} (at least 3)")
            if face:
                check(contrast(fp, face) >= 3.0, f"{t}: {k}: the ticked box {fp} against the unticked face {face}: contrast {contrast(fp, face):.2f} (at least 3)")
    print(f"check_5t grey: {checks} checks, {fails} failed")

def halo_rect(r, name):
    """a panel's own rectangle (x, y, w, h) from its 'halo name=... a=x0,y0,x1,y1' line"""
    for n in r.notes:
        if n.startswith("halo name=" + name + " "):
            x0, y0, x1, y1 = (float(v) for v in n.split(" a=")[1].split()[0].split(","))
            return (x0, y0, x1 - x0, y1 - y0)
    return None

def mean_lum(b, x0, y0, x1, y1):
    tot = n = 0
    for yy in range(int(y0), int(y1), 2):
        for xx in range(int(x0), int(x1), 2):
            tot += lum(b.px(xx, yy)); n += 1
    return tot / max(1, n)

def part_tiles(binary, work):
    """0.38.1: every name in a Settings tile stays inside its tile (6 px in from each side, so at least 2 px clear of the 1 px edge): "Theme default" was wider than its tile in Paper;
    and the picture behind Settings is dimmed as behind About and the guides (60 % black: at most half as bright as without Settings)"""
    for t in ("dark", "light", "paper", "contrast", "monochrome"):
        r = run_app(binary, work, f"t_tiles_{t}", {"PL_NEWUI": "on", "PL_OPEN": "settings"}, frames=24, theme=t, size="1000x640")
        tiles = {k[len("tile."):]: v for k, v in r.rects.items() if k.startswith("tile.")}
        names = {k: v for k, v in r.rects.items() if k.startswith("tilename.")}
        check(len(tiles) >= 9 and len(names) >= 9, f"{t}: found {len(tiles)} tiles and {len(names)} tile names in Settings")
        for k, v in names.items():
            tid = k[len("tilename."):k.rindex(".")]
            tv = tiles.get(tid)
            ok = tv is not None and v[0] >= tv[0] + 2 and v[0] + v[2] <= tv[0] + tv[2] - 2 and v[1] >= tv[1] and v[1] + v[3] <= tv[1] + tv[3]
            check(ok, f"{t}: {k} {v} lies inside its tile {tv} with 2 px to spare")
        win = r.rects.get("win.settings")
        r0 = run_app(binary, work, f"t_tiles0_{t}", {"PL_NEWUI": "on"}, frames=24, theme=t, size="1000x640")
        if win and win[0] > 140:
            box = (win[0] - 130, win[1] + 100, win[0] - 10, win[1] + 300)
            dim, plain = mean_lum(Bmp(r.bmp), *box), mean_lum(Bmp(r0.bmp), *box)
            check(plain > 0.002 and dim <= 0.5 * plain, f"{t}: the picture beside Settings is dimmed: luminance {dim:.4f} with Settings, {plain:.4f} without")
        else:
            check(False, f"{t}: Settings is open with picture to its left {win}")
    print(f"check_5t tiles: {checks} checks, {fails} failed")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"grey": part_grey, "tiles": part_tiles}[part](binary, work)
    sys.exit(1 if fails else 0)
