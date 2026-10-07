#!/usr/bin/env python3
# 0.15 stage 5i: checked in the REAL app (Linux, software OpenGL; not Windows, not a real mouse, not a GPU).
#   check_5i.py <PhysicsLab binary> <work folder> <part>      parts: sheet  (the look tests moved to check_5j.py in stage 5j)
# sheet (Part 1): the data sheet ends above the bottom toolbar (>= 4 px gap), is inside the window, solid and clear of the Controls panel, at 3 window sizes,
#   for the 3 normal tabs (Values / Decay / This view) and the 5 Atom card tabs, and for the Orbit Lab's sheet. The rects are the app's own (window pixels = points at scale 1).
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, prefetch, inside, overlap

SIZES = ("1280x800", "880x560", "1920x1080")
TABS = ("values", "decay", "hint", "spectrum", "lifetimes", "size", "temperature", "radial")
GAP = 4
fails = checks = 0

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def part_sheet(binary, work):
    cases = []                                                  # (tag, size, run_app args, frames)
    for sz in SIZES:
        for t in TABS:
            env = {"PL_ATOM": "1", "PL_NEWUI": "on", "PL_ATOM_SET": "Z=92;N=146;view=Cloud 3D;orb=2p", "PL_SCRIPT": f"10:click=card.all;16:click=data.tab.{t}", "PL_LOG_SHELL": "1"}
            cases.append((f"atom {t} {sz}", sz, (binary, work, f"sh_{t}_{sz}", env), 44))
        env = {"PL_NEWUI": "on", "PL_SELECT": "Earth", "PL_SCRIPT": "20:click=card.all", "PL_LOG_SHELL": "1"}
        cases.append((f"orbit {sz}", sz, (binary, work, f"sh_orbit_{sz}", env), 60))
    prefetch([(c[2], {"frames": c[3], "size": c[1]}) for c in cases])
    for tag, sz, a, frames in cases:
        r = run_app(*a, frames=frames, size=sz)
        w, h = (int(v) for v in sz.split("x"))
        sheet, tb, panel = r.rects.get("panel.data"), r.rects.get("toolbar"), r.rects.get("panel.controls")
        check(sheet is not None, f"{tag}: the sheet is open")
        check(tb is not None, f"{tag}: the toolbar is drawn")
        if sheet is None or tb is None:
            continue
        check(sheet[1] + sheet[3] <= tb[1] - GAP + 1e-6, f"{tag}: the sheet bottom {sheet[1] + sheet[3]:.0f} is at least {GAP} px above the toolbar top {tb[1]:.0f}")
        check(inside(sheet, w, h), f"{tag}: the sheet is inside the window {sheet}")
        check(any(n.startswith("datasheet solid") for n in r.notes), f"{tag}: the sheet is drawn solid")
        if panel is not None:
            check(not overlap(sheet, panel), f"{tag}: the sheet is clear of the Controls panel {sheet} {panel}")

if __name__ == "__main__":
    binary, work = sys.argv[1], sys.argv[2]
    part = sys.argv[3] if len(sys.argv) > 3 else "all"
    for p, f in (("sheet", part_sheet),):
        if part in (p, "all"):
            f(binary, work)
    print(f"{checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
