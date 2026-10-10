#!/usr/bin/env python3
# 0.15 stage 5t: in the Controls panel a selected segment and a checked box are GREY like the sliders (no accent colour), their label / check mark is white or black with enough contrast.
# Checked in the REAL app (Linux, software OpenGL; NOT Windows, NOT a real GPU), on the picture's own pixels. Orbit Lab, New UI on, ONE size.
#   check_5t.py <PhysicsLab binary> <work folder> grey
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
        for k, v in list(sel.items()) + list(box.items()):
            fp = b.px(int(v[0] + 2), int(v[1] + 2))                       # a corner-free spot inside the fill, away from the label
            check(max(fp) - min(fp) <= 2, f"{t}: {k}: the fill {fp} is grey (r, g, b within 2)")
            lp = label_px(b, v, fp)
            check(contrast(lp, fp) >= 4.5, f"{t}: {k}: label / check mark {lp} on the fill {fp}: contrast {contrast(lp, fp):.2f} (at least 4.5)")
            if face:
                check(contrast(fp, face) >= 3.0, f"{t}: {k}: the fill {fp} against the unselected face {face}: contrast {contrast(fp, face):.2f} (at least 3)")
    print(f"check_5t grey: {checks} checks, {fails} failed")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"grey": part_grey}[part](binary, work)
    sys.exit(1 if fails else 0)
