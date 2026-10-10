#!/usr/bin/env python3
# 0.15 stage 5r: pills (the Atom toolbar and the chart tilt pill) are a flat fill: no lighter block behind the buttons, no 1 px outline (except Monochrome / High contrast).
# Checked in the REAL app (Linux, software OpenGL; NOT Windows, NOT a real GPU), on the picture's own pixels.
#   check_5r.py <PhysicsLab binary> <work folder> pill
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

def dmax(a, b):
    return max(abs(x - y) for x, y in zip(a, b))

def between(p, a_, b_, tol=3):
    return all(min(a_[i], b_[i]) - tol <= p[i] <= max(a_[i], b_[i]) + tol for i in range(3))

THEMES = ("dark", "light", "sage", "monochrome")
OUTLINE = ("monochrome", "contrast")
ATOM_CHART = {"PL_ATOM": "1", "PL_ATOM_SET": "view=Chart of nuclides", "PL_NEWUI": "on"}

def judge(tag, theme, bmp, r):
    x, y, w, h = (int(round(v)) for v in r)
    R = h / 2.0
    cx = x + w // 2
    # a) the interior is one colour: samples in the round end caps (x + 1 R ... 1.5 R), the middle, the other end, on the top and the bottom row inside the pill, in the gaps above / below the buttons
    xs = [x + int(R * k) for k in (1.0, 1.2, 1.5)] + [cx, x + w // 3, x + w - int(R * 1.5), x + w - int(R * 1.2), x + w - int(R * 1.0) - 1]
    pts = [bmp.px(px, py) for py in (y + 2, y + h - 3) for px in xs]
    ref = pts[3]
    worst = max(dmax(p, ref) for p in pts)
    check(len(pts) >= 6 and worst <= 2, f"{theme}: {tag}: the interior is one flat colour (largest difference {worst} of 255 over {len(pts)} points, at most 2): {pts}")
    # c) the fill differs a little from the page behind it
    out_top = bmp.px(cx, y - 2)
    d = dmax(ref, out_top)
    check(4 <= d <= 40, f"{theme}: {tag}: fill {ref} against the page {out_top} differs by {d} (4..40)")
    # b) the outline: gone, except in Monochrome / High contrast, where it must stay
    edge = [bmp.px(cx, y), bmp.px(cx, y + 1)]
    if theme in OUTLINE:
        check(dmax(edge[0], ref) >= 20, f"{theme}: {tag}: the outline stays: edge pixel {edge[0]} against the fill {ref}")
    else:
        check(all(between(e, ref, out_top) for e in edge), f"{theme}: {tag}: no outline: edge pixels {edge} lie between the fill {ref} and the page {out_top}")

def part_pill(binary, work):
    for t in THEMES:
        r = run_app(binary, work, f"r_pill_{t}", dict(ATOM_CHART), frames=40, theme=t)
        b = Bmp(r.bmp)
        for name, tag in (("toolbar", "toolbar"), ("tiltslider", "2D/3D pill")):
            rc = r.rects.get(name)
            check(rc is not None, f"{t}: {tag} found")
            if rc:
                judge(tag, t, b, rc)
    print(f"check_5r pill: {checks} checks, {fails} failed")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"pill": part_pill}[part](binary, work)
    sys.exit(1 if fails else 0)
