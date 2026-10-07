#!/usr/bin/env python3
# 0.15 stage 5j / 5m: the look M1 (the New UI's look, always on since 5m; stage 5j tested M1..M3 with PL_LOOK), checked in the REAL app (Linux, software OpenGL; not Windows, not a real mouse, not a GPU).
#   check_5j.py <PhysicsLab binary> <work folder> m1
# The app starts under the look and really draws it (the Dark New UI picture differs from the same run with New UI Off, and the panel's pixels are M1's card colour #17171a), and under the look, at 1280x800 and 880x560:
#   the rect tests of stages 5g / 5h pass unchanged (toolbar, ring, centre) and no text is cut or overlapping in the Controls panel (5g panel), the Atom card (5h cards) and the data sheet (5h fit).
# A different font changes text widths: if one of these fails, the look (sizes, wrapping) is changed, never the test.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
look = sys.argv[3] if len(sys.argv) > 3 else ""
from shell_lib import run_app, Bmp
import check_5g, check_5h

SIZES = ("1280x800", "880x560")
fails = checks = 0

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def main(binary, work):
    check(look == "m1", "the look is m1 (the only one left)")
    on = run_app(binary, work, "lk_on", {"PL_NEWUI": "on", "PL_SELECT": "Earth"}, frames=40)
    off = run_app(binary, work, "lk_off", {"PL_NEWUI": "off", "PL_SELECT": "Earth"}, frames=40)
    check("panel.controls" in on.rects and "toolbar" in on.rects, f"look {look}: the app starts and draws the panel and the toolbar")
    a, b = open(on.bmp, "rb").read(), open(off.bmp, "rb").read()
    diff = sum(1 for i in range(54, min(len(a), len(b)), 4) if a[i:i + 3] != b[i:i + 3])
    check(len(a) == len(b) and diff > 0.01 * (len(a) // 4), f"look {look}: the New UI picture differs from the old UI's ({diff} pixels)")
    # PIXELS: inside the Controls panel, away from text, the colour is M1's card #17171a (23, 23, 26) (the panel is 0.95 opaque over the picture behind it, so allow the mix)
    pc = on.rects["panel.controls"]; bm = Bmp(on.bmp)
    inner = [bm.px(int(pc[0] + pc[2] - 6), int(pc[1] + pc[3] - 8 - k)) for k in range(0, 40, 4)]
    check(any(all(abs(c - t) <= 6 for c, t in zip(px, (23, 23, 26))) for px in inner), f"look {look}: the panel's empty strip is M1's card colour #17171a: {inner[:3]}")
    check_5g.SIZES = check_5h.SIZES = SIZES
    for mod, name in ((check_5g, "toolbar"), (check_5g, "ring"), (check_5h, "centre"), (check_5g, "panel"), (check_5h, "cards"), (check_5h, "fit")):
        before = mod.fails
        getattr(mod, "part_" + name)(binary, os.path.join(work, name))
        check(mod.fails == before, f"look {look}: the test {name} of 5g / 5h passes at {' and '.join(SIZES)}")

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
    print(f"{checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
