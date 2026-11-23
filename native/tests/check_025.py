#!/usr/bin/env python3
# 0.25, in the REAL app (Linux, software Vulkan; NOT Windows, NOT a real mouse): the "closest to reality" dots and each lab's "Most realistic" button (src/ui/realest.cpp).
#   orbit / atom / molecule: before the button, the lab's default picture shows dots and at least one of them is on a choice that is NOT the current one; after the
#   button, every dot drawn sits on the current choice, and the lab's state is the realistic one. The suite runs orbit (tests/TESTS.md).
#   check_025.py <PhysicsLab binary> <work folder> orbit|atom|molecule
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app

fails = checks = 0
def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def marks(r):
    """the dots drawn in the last frame: id -> 'on' (the dot's choice is the current one) / 'off'"""
    drawn = {k[5:] for k in r.rects if k.startswith("real.")}
    state = {}
    for n in r.note("realmark."):
        k, v = n[len("realmark."):].split()
        if k in drawn: state[k] = v
    return state

LABS = {"orbit": ({}, 4, "1280x800"), "atom": ({"PL_ATOM": "1"}, 3, "1280x1600"), "molecule": ({"PL_MOLECULE": "H2O"}, 1, "1280x800")}   # (0.46: the Atom Lab's Cloud rows, with their dots, are below the fold at 800 since Electrons All is the default)

def part(binary, work, lab):
    env, least, size = LABS[lab]
    before = marks(run_app(binary, work, lab + "_before", env, frames=20, size=size))
    check(len(before) >= least, f"{lab}: at least {least} dots in the default picture ({sorted(before)})")
    check("off" in before.values(), f"{lab}: some default choice is not the realistic one, and its dot is elsewhere ({before})")
    r = run_app(binary, work, lab + "_after", dict(env, PL_SCRIPT=f"8:click={lab}.realest"), frames=36, size=size)
    check(not r.scripted_missing(), f"{lab}: the Most realistic button is there to click ({r.scripted_missing()[:2]})")
    after = marks(r)
    check(len(after) >= least, f"{lab}: dots after the button ({sorted(after)})")
    check(all(v == "on" for v in after.values()), f"{lab}: after the button every dot is on the current choice ({after})")
    if lab == "molecule":
        check(any("Born" in t[2] or "Electron density" in t[2] for t in r.texts), "molecule: the Density style is on")
    if lab == "atom":
        v = r.note("viewsel ")
        check(v and v[-1].startswith("viewsel cloud3d"), f"atom: the picture is Cloud 3D ({v})")
        check(after.get("atom.elec.row0") == "on", f"atom: Most realistic sets Electrons to All, the real neutral atom (0.40; {after.get('atom.elec.row0')})")

if __name__ == "__main__":
    binary, work, p = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    part(binary, work, p)
    print(f"{p}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
