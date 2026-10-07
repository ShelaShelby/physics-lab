#!/usr/bin/env python3
# 0.15 stage 5j: the look options M1..M3 (PL_LOOK=m1|m2|m3), checked in the REAL app (Linux, software OpenGL; not Windows, not a real mouse, not a GPU).
#   check_5j.py <PhysicsLab binary> <work folder> m1|m2|m3
# The app starts under the look, really draws it (the picture differs from the same run with PL_LOOK empty), and under the look, at 1280x800 and 880x560:
#   the rect tests of stages 5g / 5h pass unchanged (toolbar, ring, centre) and no text is cut or overlapping in the Controls panel (5g panel), the Atom card (5h cards) and the data sheet (5h fit).
# A different font changes text widths: if one of these fails, the look (sizes, wrapping) is changed, never the test.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
look = sys.argv[3] if len(sys.argv) > 3 else ""
os.environ["LOOK_TEST"] = look                                  # shell_lib.run_app puts it in PL_LOOK for every app start of the rect tests below
from shell_lib import run_app
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
    check(look in ("m1", "m2", "m3"), "the look is m1, m2 or m3")
    on = run_app(binary, work, "lk_on", {"PL_NEWUI": "on", "PL_SELECT": "Earth"}, frames=40)
    off = run_app(binary, work, "lk_off", {"PL_NEWUI": "on", "PL_SELECT": "Earth", "PL_LOOK": ""}, frames=40)
    check("panel.controls" in on.rects and "toolbar" in on.rects, f"look {look}: the app starts and draws the panel and the toolbar")
    a, b = open(on.bmp, "rb").read(), open(off.bmp, "rb").read()
    diff = sum(1 for i in range(54, min(len(a), len(b)), 4) if a[i:i + 3] != b[i:i + 3])
    check(len(a) == len(b) and diff > 0.01 * (len(a) // 4), f"look {look}: the picture differs from today's ({diff} pixels)")
    check_5g.SIZES = check_5h.SIZES = SIZES
    for mod, name in ((check_5g, "toolbar"), (check_5g, "ring"), (check_5h, "centre"), (check_5g, "panel"), (check_5h, "cards"), (check_5h, "fit")):
        before = mod.fails
        getattr(mod, "part_" + name)(binary, os.path.join(work, name))
        check(mod.fails == before, f"look {look}: the test {name} of 5g / 5h passes at {' and '.join(SIZES)}")

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
    print(f"{checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
