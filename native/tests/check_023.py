#!/usr/bin/env python3
# 0.23, in the REAL app (Linux, software Vulkan; NOT Windows, NOT a real mouse). tests/TESTS.md lists the parts kept and why.
#   orbits  the Orbit Lab: "Orbit paths" draws each planet's orbit (the picture changes when it is switched off) and is saved only when off; zoomed in on
#           Earth the picture is not tinted green (the habitable-zone band fades out)
#   check_023.py <PhysicsLab binary> <work folder> orbits
import math, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, Bmp

fails = checks = 0
def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def differ(a, b, x0, y0, x1, y1):
    A, B = Bmp(a.bmp), Bmp(b.bmp)
    return sum(1 for y in range(y0, y1, 2) for x in range(x0, x1, 2) if A.px(x, y) != B.px(x, y))

def orbits(binary, work):
    for f in ("s_on.txt", "s_off.txt"):                    # (fresh settings: a file left by the last run made the switch start off, and the test passed and failed in turn)
        if os.path.exists(os.path.join(work, f)): os.remove(os.path.join(work, f))
    on = run_app(binary, work, "orb_on", {"PL_SETTINGS": os.path.join(work, "s_on.txt")}, frames=20)
    off = run_app(binary, work, "orb_off", {"PL_SETTINGS": os.path.join(work, "s_off.txt"), "PL_SCRIPT": "8:click=show.orbits"}, frames=20)
    n = differ(on, off, 0, 60, 900, 740)
    check(n > 200, f"switching Orbit paths off changes the picture ({n} sampled pixels)")
    s_on = open(os.path.join(work, "s_on.txt")).read() if os.path.exists(os.path.join(work, "s_on.txt")) else ""
    s_off = open(os.path.join(work, "s_off.txt")).read() if os.path.exists(os.path.join(work, "s_off.txt")) else ""
    check("orbits" not in s_on and "orbits=0" in s_off.replace(" ", ""), "the setting is saved only when off")
    e = run_app(binary, work, "orb_earth", {"PL_SELECT": "Earth", "PL_ZOOM": "3"}, frames=20)
    b = Bmp(e.bmp)
    px = [b.px(x, y) for x in range(20, 560, 12) for y in range(80, 700, 12)]
    tint = sum(p[1] - (p[0] + p[2]) / 2 for p in px) / len(px)
    check(tint < 4, f"zoomed in on Earth the picture is not green (green excess {tint:.1f})")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"orbits": orbits}[part](binary, work)
    print(f"{part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
