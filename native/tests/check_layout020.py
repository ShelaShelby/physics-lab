#!/usr/bin/env python3
# 0.20: three layout fixes, in the REAL app (Linux, software OpenGL; NOT Windows, NOT a real mouse). Moved here unchanged from check_molbuild.py in 0.24, when the
# Molecule Lab's builder (and the rest of that file) was removed (the owner's decision of 4 Nov 2026). One part per check:
#   u238      layout fix a: the Atom Lab's info text no longer covers the nucleus in the Nucleus view (U-238, Th-232, Pb-208 at 1280x800); a click opens the folded text (nothing lost)
#   sheet     layout fix b: the Orbit Lab's "All values" sheet does not cover the Legend chip (1280x800 and 1920x1080)
#   molpan    layout fix c: the Molecule Lab's pan is limited (a stored pan far off and a long right-drag both leave the molecule in view); Reset view still centres it
#   check_layout020.py <PhysicsLab binary> <work folder> <part>
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, Bmp, overlap

fails = checks = 0
def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

BASE = {"PL_NEWUI": "on", "PL_DRAWSTAT": "1"}

def atoms(r):
    last = {}
    for ln in r.out.splitlines():
        if ln.startswith("PLMOLATOM "):
            d = dict(re.findall(r"(\w+)=(\S+)", ln))
            last[int(d["i"])] = (float(d["x"]), float(d["y"]), float(d["r"]))
    return last

def part_u238(b, w):
    opened = False
    for name, z, n in (("U-238", 92, 146), ("Th-232", 90, 142), ("Pb-208", 82, 126)):
        env = {"PL_NEWUI": "on", "PL_ATOM": "1", "PL_ATOM_SET": f"Z={z};N={n}", "PL_SCRIPT": "10:click=nucleus.nucleus;150:astate"}
        r = run_app(b, w, "u238_" + name, env, frames=160)
        box = [ln for ln in r.out.splitlines() if ln.startswith("PLATOMBOX ")]
        nuc = [dict(re.findall(r"(\w+)=(\S+)", ln)) for ln in r.out.splitlines() if ln.startswith("PLATOMNUC ")]
        check(bool(box) and bool(nuc), f"{name}: the info plate and the nucleus were reported")
        if not box or not nuc:
            continue
        x, y, iw, ih = (float(v) for v in re.search(r"info=([-\d.e]+),([-\d.e]+),([-\d.e]+),([-\d.e]+)", box[-1]).groups())
        ncx, ncy, nr = float(nuc[-1]["x"]), float(nuc[-1]["y"]), float(nuc[-1]["r"])
        qx, qy = min(max(ncx, x), x + iw), min(max(ncy, y), y + ih)
        dist = ((qx - ncx) ** 2 + (qy - ncy) ** 2) ** 0.5
        print(f"{name}: info plate {x},{y},{iw},{ih}; nucleus centre {ncx},{ncy} radius {nr}; folded {nuc[-1]['folded']}")
        check(nr > 0 and dist >= nr, f"{name}, Nucleus view 1280x800: the info text does not cover the nucleus (plate {x},{y},{iw}x{ih}; nucleus r {nr} at {ncx},{ncy})")
        # (0.46.1: the picture's free part now starts below the readout plate, so for U-238 and Pb-208 the whole text fits above the nucleus and nothing needs folding; then nothing
        # may be cut either (foldable 0). Th-232's longer text still folds, and the click test runs on the first atom that folds.)
        folded = int(nuc[-1]["folded"])
        check(folded > 0 or nuc[-1]["foldable"] == "0", f"{name}: the rest of the text is folded, not dropped ({nuc[-1]})")
        if folded > 0 and not opened:                                     # a click on the plate opens it: every row is there again
            opened = True
            env2 = dict(env, PL_SCRIPT=f"10:click=nucleus.nucleus;150:clickxy={x + 20:.0f},{y + ih - 8:.0f};156:astate")
            r2 = run_app(b, w, "u238_open", env2, frames=162)   # (named for the first atom it ran on)
            n2 = [dict(re.findall(r"(\w+)=(\S+)", ln)) for ln in r2.out.splitlines() if ln.startswith("PLATOMNUC ")]
            b2 = [ln for ln in r2.out.splitlines() if ln.startswith("PLATOMBOX ")]
            if n2 and b2:
                h2 = float(re.search(r"info=[-\d.e]+,[-\d.e]+,[-\d.e]+,([-\d.e]+)", b2[-1]).group(1))
                check(n2[-1]["open"] == "1" and n2[-1]["folded"] == "0" and h2 > ih + 18 * int(nuc[-1]["folded"]) - 1, f"{name}: a click opens the folded text, all rows shown ({n2[-1]}, height {ih} -> {h2})")
            else:
                check(False, "U-238: the opened state was reported")
    if not opened:   # 0.50: since 0.49 the three texts fit at 1280x800 (nothing folded, nothing dropped: checked above); the click test then runs at 880x560, where Th-232's folds
        env = {"PL_NEWUI": "on", "PL_ATOM": "1", "PL_ATOM_SET": "Z=90;N=142", "PL_SCRIPT": "10:click=nucleus.nucleus;150:astate"}
        r = run_app(b, w, "u238_small", env, frames=160, size="880x560")
        box = [ln for ln in r.out.splitlines() if ln.startswith("PLATOMBOX ")]
        nuc = [dict(re.findall(r"(\w+)=(\S+)", ln)) for ln in r.out.splitlines() if ln.startswith("PLATOMNUC ")]
        if box and nuc and int(nuc[-1]["folded"]) > 0:
            opened = True
            x, y, iw, ih = (float(v) for v in re.search(r"info=([-\d.e]+),([-\d.e]+),([-\d.e]+),([-\d.e]+)", box[-1]).groups())
            env2 = dict(env, PL_SCRIPT=f"10:click=nucleus.nucleus;150:clickxy={x + 20:.0f},{y + ih - 8:.0f};156:astate")
            r2 = run_app(b, w, "u238_small_open", env2, frames=162, size="880x560")
            n2 = [dict(re.findall(r"(\w+)=(\S+)", ln)) for ln in r2.out.splitlines() if ln.startswith("PLATOMNUC ")]
            b2 = [ln for ln in r2.out.splitlines() if ln.startswith("PLATOMBOX ")]
            ok = bool(n2 and b2)
            h2 = float(re.search(r"info=[-\d.e]+,[-\d.e]+,[-\d.e]+,([-\d.e]+)", b2[-1]).group(1)) if ok else 0
            check(ok and n2[-1]["open"] == "1" and n2[-1]["folded"] == "0" and h2 > ih, f"Th-232 at 880x560: a click opens the folded text, all rows shown ({n2[-1] if n2 else None}, height {ih} -> {h2})")
    check(opened, "at least one of U-238, Th-232, Pb-208 had folded text to open (0.46.1; 0.50: Th-232 at 880x560 if none at 1280x800)")

def part_sheet(b, w):
    for sz in ("1280x800", "1920x1080"):
        r = run_app(b, w, "sheet_" + sz, {"PL_NEWUI": "on", "PL_SELECT": "Earth", "PL_SCRIPT": "20:click=card.all"}, frames=40, size=sz)
        sheet, chip = r.rects.get("panel.data"), r.rects.get("card.orbit.legend.collapsed")
        check(sheet is not None and chip is not None, f"{sz}: the sheet and the Legend chip are drawn ({sheet}, {chip})")
        if sheet and chip:
            check(not overlap(sheet, chip), f"{sz}: the All values sheet {sheet} does not cover the Legend chip {chip}")
            card = r.rects.get("card.orbit.card")
            check(card is None or not overlap(sheet, card), f"{sz}: nor the subject card {card}")
            W, H = (int(v) for v in sz.split("x"))
            check(sheet[1] + sheet[3] <= H and sheet[3] >= 200, f"{sz}: the sheet stays on screen and tall enough to read ({sheet})")
    # 0.39.3 night hunt: below the Legend chip the sheet kept its bottom and lost its height: the Moon's last line was cut (need 267, have 237) though the room below was free
    r = run_app(b, w, "sheet_moon", {"PL_NEWUI": "on", "PL_SELECT": "moon:Earth", "PL_ZOOM": "1", "PL_SCRIPT": "30:click=card.all"}, frames=60, size="1280x800")
    need = [tuple(float(x) for x in re.findall(r"need=([\d.]+) have=([\d.]+)", n)[0]) for n in r.note("panelneed data")]
    check(bool(need) and need[-1][0] <= need[-1][1] + 1, f"1280x800, the Moon: the All values sheet shows every line without scrolling (need, have: {need[-1:]})")

def part_molpan(b, w):
    def centre(r):
        a = atoms(r)
        if not a:
            return None
        return (sum(v[0] for v in a.values()) / len(a), sum(v[1] for v in a.values()) / len(a), a)
    sf = os.path.join(w, "s_pan.txt")
    open(sf, "w").write("lab=Molecule Lab\nmolecule=v1 mol=H2O style=ball labels=1 spin=0 zoom=1 pan=500,400 rot=1,0,0,0,1,0,0,0,1\n")
    r = run_app(b, w, "molpan_stored", dict(BASE, PL_SETTINGS=sf), frames=10)
    c = centre(r)
    check(c is not None and all(0 <= v[0] <= 1280 and 0 <= v[1] <= 800 for v in c[2].values()), f"a stored pan of (500, 400) A: the molecule is still in view ({c[2] if c else None})")
    r0 = run_app(b, w, "molpan_ref", dict(BASE, PL_MOLECULE="H2O"), frames=10)
    c0 = centre(r0)
    rd = run_app(b, w, "molpan_drag", dict(BASE, PL_MOLECULE="H2O", PL_SCRIPT="6:rdrag=400,400,1270,790;14:rdrag=400,400,1270,790;22:rdrag=400,400,1270,790"), frames=32)
    cd = centre(rd)
    if c0 and cd:
        # 1.3 molecule radii (kPanLimitRadii), in points: the radius is what fits 0.48 of the free area's smaller side at zoom 1 (molecule.cpp fitScale)
        dist = ((cd[0] - c0[0]) ** 2 + (cd[1] - c0[1]) ** 2) ** 0.5
        print(f"molpan: centre {c0[:2]} -> after three long right-drags {cd[:2]}, {dist:.0f} points")
        check(all(0 <= v[0] <= 1280 and 0 <= v[1] <= 800 for v in cd[2].values()), f"three long right-drags: every atom still in view ({cd[2]})")
        check(dist > 50, f"the drags did move it ({dist:.0f} points)")
        rr = run_app(b, w, "molpan_reset", dict(BASE, PL_MOLECULE="H2O", PL_SCRIPT="6:rdrag=400,400,1270,790;14:click=molecule.bar.reset"), frames=20)
        cr = centre(rr)
        check(cr is not None and abs(cr[0] - c0[0]) < 0.5 and abs(cr[1] - c0[1]) < 0.5, f"Reset view centres it again ({cr[:2] if cr else None} vs {c0[:2]})")
    else:
        check(False, "molpan: atoms reported")


if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"u238": part_u238, "sheet": part_sheet, "molpan": part_molpan}[part](binary, work)
    print(f"{part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
