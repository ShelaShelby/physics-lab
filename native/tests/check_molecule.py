#!/usr/bin/env python3
# 0.18 stage 1: the Molecule Lab in the REAL app (Linux, software OpenGL; NOT Windows, NOT a real GPU, NO timing: counts only).
#   Water, ball-and-stick, Dark theme, and benzene, space-filling, Light theme: pictures to look at (molecule_dark.bmp, molecule_light.bmp), the draw counts
#   (the whole molecule ONE instanced draw call), the CPK colour at an atom's middle, the bottom bar where the other labs have their toolbar;
#   a click on an atom and on a bond picks it (the panel shows it); the settings are saved when the app closes and read at the next start; a damaged settings line falls back.
#   check_molecule.py <PhysicsLab binary> <work folder>
import os, sys, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, Bmp, inside

fails = checks = 0
def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def stats(r):
    return [dict((k, v) for k, v in re.findall(r"(\w+)=(\S+)", ln)) for ln in r.out.splitlines() if ln.startswith("PLMOLSTAT ")]
def atoms(r):
    last = {}
    for ln in r.out.splitlines():
        if ln.startswith("PLMOLATOM "):
            d = dict(re.findall(r"(\w+)=(\S+)", ln))
            last[int(d["i"])] = (float(d["x"]), float(d["y"]), float(d["r"]))
    return last
def notes(r, prefix):
    return [n for n in r.notes if n.startswith(prefix)]

def main(binary, work):
    base = {"PL_NEWUI": "on", "PL_DRAWSTAT": "1"}
    # 1. water, ball-and-stick, Dark
    r = run_app(binary, work, "molecule_dark", dict(base, PL_MOLECULE="H2O", PL_SETTINGS=os.path.join(work, "s_dark.txt")), frames=20, theme="dark")
    st = stats(r)
    check(len(st) >= 10, "dark: the draw counts were printed")
    if st:
        v = st[-1]
        print("dark:", v)
        check(v["mol"] == "H2O" and v["atoms"] == "3" and v["sticks"] == "4", f"dark: water, 3 atoms, 2 bonds x 2 halves ({v})")
        check(int(v["blobs"]) == 3 * 2 + 4 * 3, f"dark: every ball (outline + ball) and stick half (outline, core, light) is one blob ({v['blobs']})")
        check(int(v["calls"]) <= 3, f"dark: the molecule is one instanced draw call ({v['calls']} calls in the frame)")
    a = atoms(r)
    check(len(a) == 3, "dark: 3 atoms on screen")
    bar = r.rects.get("moleculebar")
    check(bar is not None and inside(bar, 1280, 800) and bar[1] + bar[3] > 800 - 80, f"dark: the bottom bar is on screen, at the bottom ({bar})")
    if a and r.bmp and os.path.exists(r.bmp):
        b = Bmp(r.bmp)
        x, y, rad = a[0]
        px = b.px(int(x + rad * 0.25), int(y + rad * 0.25))       # oxygen, a little below right of the highlight: CPK red
        check(px[0] > 120 and px[1] < 80 and px[2] < 80, f"dark: oxygen is red {px}")
    check(bool(notes(r, "molecule H2O style=ball labels=1")), "dark: the bar's state")
    # 2. benzene, space-filling, Light
    r2 = run_app(binary, work, "molecule_light", dict(base, PL_MOLECULE="C6H6", PL_MOLECULE_STYLE="fill", PL_SETTINGS=os.path.join(work, "s_light.txt")), frames=20, theme="light")
    st2 = stats(r2)
    if st2:
        v = st2[-1]
        print("light:", v)
        check(v["mol"] == "C6H6" and v["atoms"] == "12" and v["sticks"] == "0" and int(v["blobs"]) == 24, f"light: benzene space-filling, 12 balls, no sticks ({v})")
        check(int(v["calls"]) <= 3, f"light: one instanced draw call for the molecule ({v['calls']})")
    else:
        check(False, "light: the draw counts were printed")
    # 3. clicks: an atom, then the middle of a bond (water's first bond, O to H)
    if len(a) == 3:
        ox, oy, orad = a[0]
        hx, hy, hrad = a[1]
        mx, my = (ox + hx) / 2, (oy + hy) / 2
        sel = []
        for nm, xy in (("molecule_click_atom", (hx, hy)), ("molecule_click_bond", (mx, my))):
            r3 = run_app(binary, work, nm, dict(base, PL_MOLECULE="H2O", PL_SCRIPT=f"6:clickxy={xy[0]:.0f},{xy[1]:.0f}"), frames=12)
            s3 = [n.split("sel=")[1] for n in notes(r3, "molecule H2O")]
            sel.append(s3[-1] if s3 else None)
        check(sel[0] == "1,-1", f"a click on H1 picks atom 1 ({sel[0]})")
        check(sel[1] == "-1,0", f"a click on the middle of the O-H bond picks bond 0 ({sel[1]})")
    # 4. settings: chosen in one run, there at the next start; a damaged line falls back to water
    sf = os.path.join(work, "s_keep.txt")
    if os.path.exists(sf):
        os.remove(sf)
    run_app(binary, work, "molecule_save", dict(base, PL_MOLECULE="H2O", PL_SETTINGS=sf, PL_SCRIPT="5:click=molecule.pick.NH3;9:click=molecule.bar.fill"), frames=14)
    txt = open(sf).read() if os.path.exists(sf) else ""
    check("lab=Molecule Lab" in txt and re.search(r"^molecule=v1 mol=NH3 style=fill ", txt, re.M) is not None, "the settings file has the lab, ammonia and space-filling")
    r4 = run_app(binary, work, "molecule_load", dict(base, PL_SETTINGS=sf), frames=8)
    st4 = stats(r4)
    check(bool(st4) and st4[-1]["mol"] == "NH3" and bool(notes(r4, "molecule NH3 style=fill")), "the next start opens ammonia, space-filling")
    bad = os.path.join(work, "s_bad.txt")
    open(bad, "w").write("lab=Molecule Lab\nmolecule=v1 mol=C60 style=wire zoom=nan pan=1e400,2 rot=0,0,0,0,0,0,0,0,0\n")
    r5 = run_app(binary, work, "molecule_bad", dict(base, PL_SETTINGS=bad), frames=8)
    st5 = stats(r5)
    check(bool(st5) and st5[-1]["mol"] == "H2O" and bool(notes(r5, "molecule H2O style=ball")), "damaged settings: water, ball-and-stick (the defaults), no crash")

if __name__ == "__main__":
    binary, work = sys.argv[1], sys.argv[2]
    os.makedirs(work, exist_ok=True)
    main(binary, work)
    print(f"check_molecule: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
