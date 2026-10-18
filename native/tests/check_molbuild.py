#!/usr/bin/env python3
# 0.20: the Molecule Lab builder and three layout fixes, in the REAL app (Linux, software OpenGL; NOT Windows, NOT a real mouse, NO timing: counts only). One part per check:
#   sketch    Build mode, the Sketch of an ethanol-like molecule (Dark, a picture to look at): draw counts (ONE instanced draw call), the formula, no open valence, no bottom bar
#   edit      a dragged O snaps and bonds to a C; a click on that bond makes it double; Ctrl+Z / Ctrl+Y undo and redo it; a picked atom is deleted with the Delete key; a click adds a C
#   refuse    an H dropped next to water's O is refused, and the reason "O already has 2 bonds" is shown (Dark, a picture to look at)
#   shape     the estimated 3D shape of water (Dark, a picture to look at): "estimated" note, one instanced draw call; "Show measured shape" jumps to the viewer's water
#   noshape   CH3 (one open valence) in the 3D view: no shape, and the reason (LIGHT, a picture to look at)
#   settings  the molbuild line: written after Build mode was used, read back at the next start; a damaged line falls back to an empty sketch; a Viewer-only run writes none
#   u238      layout fix a: the Atom Lab's info text no longer covers the nucleus in the Nucleus view (U-238, Th-232, Pb-208 at 1280x800); a click opens the folded text (nothing lost)
#   sheet     layout fix b: the Orbit Lab's "All values" sheet does not cover the Legend chip (1280x800 and 1920x1080)
#   molpan    layout fix c: the Molecule Lab's pan is limited (a stored pan far off and a long right-drag both leave the molecule in view); Reset view still centres it
#   check_molbuild.py <PhysicsLab binary> <work folder> <part>
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

ETHANOL = ("v1 mode=build view=sketch el=C atoms=C:-1.9:0.4/C:-0.7:-0.25/O:0.5:0.4/H:1.7:-0.25/H:-2.9:-0.3/H:-2.3:1.6/H:-1.2:1.45/H:-0.3:-1.45/H:-1.35:-1.35 "
           "bonds=0:1:1/1:2:1/2:3:1/0:4:1/0:5:1/0:6:1/1:7:1/1:8:1")
BASE = {"PL_NEWUI": "on", "PL_DRAWSTAT": "1"}

def stats(r):
    return [dict(re.findall(r"(\w+)=(\S+)", ln)) for ln in r.out.splitlines() if ln.startswith("PLMOLSTAT ")]
def sketch(r):
    last = {}
    for ln in r.out.splitlines():
        if ln.startswith("PLMOLSKETCH "):
            d = dict(re.findall(r"(\w+)=(\S+)", ln))
            last[int(d["i"])] = (float(d["x"]), float(d["y"]))
    return last
def atoms(r):
    last = {}
    for ln in r.out.splitlines():
        if ln.startswith("PLMOLATOM "):
            d = dict(re.findall(r"(\w+)=(\S+)", ln))
            last[int(d["i"])] = (float(d["x"]), float(d["y"]), float(d["r"]))
    return last
def mb(r):
    n = r.note("molbuild ")
    return dict(re.findall(r"(\w+)=(\S*)", n[-1].split(" msg=")[0])) | {"msg": n[-1].split(" msg=", 1)[1] if " msg=" in n[-1] else ""} if n else {}

def part_sketch(b, w):
    r = run_app(b, w, "molbuild_sketch", dict(BASE, PL_MOLBUILD=ETHANOL), frames=20)
    st = stats(r)
    check(bool(st), "sketch: draw counts printed")
    if st:
        v = st[-1]
        print("sketch:", v)
        check(v["mode"] == "sketch" and v["atoms"] == "9" and v["sticks"] == "8" and v["stubs"] == "0" and v["labels"] == "9", f"sketch: 9 atoms, 8 single bond lines, no open stub, 9 letters ({v})")
        check(int(v["blobs"]) == 8 + 9 * 2, f"sketch: every bond line and atom (outline + disc) is one blob ({v['blobs']})")
        check(int(v["calls"]) <= 3, f"sketch: one instanced draw call for the sketch ({v['calls']} calls in the frame)")
    m = mb(r)
    check(m.get("hill") == "C2H6O" and m.get("open") == "0" and m.get("name") == "-" and m.get("shape") == "estimated", f"sketch: C2H6O, complete, no name in our list (ethanol is not in it), a shape ({m})")
    check("moleculebar" not in r.rects, "sketch: no bottom bar (style, spin and zoom do nothing in the flat sketch)")
    p = sketch(r)
    if len(p) == 9 and os.path.exists(r.bmp):
        px = Bmp(r.bmp).px(int(p[2][0] + 11), int(p[2][1]))         # the O disc, right of its letter: CPK red
        check(px[0] > 150 and px[1] < 90 and px[2] < 90, f"sketch: O is CPK red {px}")

def part_edit(b, w):
    start = "v1 mode=build view=sketch el=C atoms=C:0:0/H:-1.3:0/O:2.6:1.6 bonds=0:1:1"
    r0 = run_app(b, w, "molbuild_edit0", dict(BASE, PL_MOLBUILD=start), frames=6)
    p = sketch(r0)
    check(len(p) == 3, f"edit: 3 atoms on screen ({p})")
    if len(p) != 3:
        return
    (cx, cy), (hx, hy), (ox, oy) = p[0], p[1], p[2]
    tx, ty = cx + 50, cy + 18                                        # the O let go about one bond from the C
    drag = f"6:drag={ox:.0f},{oy:.0f},{tx:.0f},{ty:.0f}"
    r1 = run_app(b, w, "molbuild_edit1", dict(BASE, PL_MOLBUILD=start, PL_SCRIPT=drag), frames=18)
    m1, p1 = mb(r1), sketch(r1)
    check(m1.get("bonds") == "2" and m1.get("orders") == "1,1", f"edit: the dropped O snapped and bonded to the C ({m1})")
    if len(p1) == 3:
        d = ((p1[2][0] - p1[0][0]) ** 2 + (p1[2][1] - p1[0][1]) ** 2) ** 0.5
        check(abs(d - 1.3 * 46) < 1.5, f"edit: it sits one sketch bond (1.3 x 46 = 59.8 points) from the C ({d:.1f})")
        mx, my = (p1[0][0] + p1[2][0]) / 2, (p1[0][1] + p1[2][1]) / 2
        seq = drag + f";20:clickxy={mx:.0f},{my:.0f}"
        r2 = run_app(b, w, "molbuild_edit2", dict(BASE, PL_MOLBUILD=start, PL_SCRIPT=seq), frames=28)
        check(mb(r2).get("orders") == "1,2", f"edit: a click on the C-O bond makes it double ({mb(r2)})")
        r3 = run_app(b, w, "molbuild_edit3", dict(BASE, PL_MOLBUILD=start, PL_SCRIPT=seq + ";28:key=ctrl+z"), frames=34)
        check(mb(r3).get("orders") == "1,1", f"edit: Ctrl+Z undoes it ({mb(r3)})")
        r4 = run_app(b, w, "molbuild_edit4", dict(BASE, PL_MOLBUILD=start, PL_SCRIPT=seq + ";28:key=ctrl+z;32:key=ctrl+y"), frames=38)
        check(mb(r4).get("orders") == "1,2", f"edit: Ctrl+Y redoes it ({mb(r4)})")
        r5 = run_app(b, w, "molbuild_edit5", dict(BASE, PL_MOLBUILD=start, PL_SCRIPT=f"6:clickxy={hx:.0f},{hy:.0f};12:key=delete;18:clickxy={cx:.0f},{cy + 140:.0f}"), frames=26)
        m5 = mb(r5)
        check(m5.get("atoms") == "3" and m5.get("bonds") == "0" and m5.get("hill") == "C2O", f"edit: the picked H deleted (its bond too), then a click on empty space added a C ({m5})")

def part_refuse(b, w):
    start = "v1 mode=build view=sketch el=H atoms=O:0:0/H:-1.05:0.75/H:1.05:0.75/H:0:-2.6 bonds=0:1:1/0:2:1"
    r0 = run_app(b, w, "molbuild_refuse0", dict(BASE, PL_MOLBUILD=start), frames=6)
    p = sketch(r0)
    check(len(p) == 4, "refuse: 4 atoms on screen")
    if len(p) != 4:
        return
    (ox, oy), (hx, hy) = p[0], p[3]
    r = run_app(b, w, "molbuild_refuse", dict(BASE, PL_MOLBUILD=start, PL_SCRIPT=f"6:drag={hx:.0f},{hy:.0f},{ox:.0f},{oy - 50:.0f}"), frames=20)
    m = mb(r)
    check(m.get("msg") == "O already has 2 bonds" and m.get("bonds") == "2", f"refuse: the H is not bonded, and the reason is shown ({m})")
    st = stats(r)
    check(bool(st) and st[-1]["stubs"] == "1", f"refuse: the lone H shows its one open stub ({st[-1] if st else None})")

def part_shape(b, w):
    water = "v1 mode=build view=3d el=H atoms=O:0:0/H:-1.05:0.75/H:1.05:0.75 bonds=0:1:1/0:2:1"
    r = run_app(b, w, "molbuild_shape", dict(BASE, PL_MOLBUILD=water), frames=20)
    st = stats(r)
    check(bool(st), "shape: draw counts printed")
    if st:
        v = st[-1]
        print("shape:", v)
        check(v["mode"] == "shape" and v["atoms"] == "3" and v["sticks"] == "4" and int(v["blobs"]) == 3 * 2 + 4 * 3, f"shape: water's estimated shape, 3 balls and 2 bonds x 2 halves ({v})")
        check(int(v["calls"]) <= 3, f"shape: one instanced draw call ({v['calls']})")
    m = mb(r)
    check(m.get("shape") == "estimated" and m.get("name") == "H2O" and m.get("view") == "3d", f"shape: estimated, named water ({m})")
    a = atoms(r)
    if len(a) == 3:                                                   # 109.47 degrees at the O, as drawn on screen (the view turns it, so only check it is a bent molecule)
        check(all(0 <= a[i][0] <= 1280 and 0 <= a[i][1] <= 800 for i in a), f"shape: all atoms in view ({a})")
    r2 = run_app(b, w, "molbuild_measured", dict(BASE, PL_MOLBUILD=water, PL_SCRIPT="8:click=molecule.showmeasured"), frames=16)
    st2 = stats(r2)
    check(bool(st2) and st2[-1]["mode"] == "viewer" and st2[-1]["mol"] == "H2O", f"shape: Show measured shape opens the viewer's water ({st2[-1] if st2 else None})")

def part_noshape(b, w):
    ch3 = "v1 mode=build view=3d el=H atoms=C:0:0/H:-1.2:0.6/H:1.2:0.6/H:0:-1.3 bonds=0:1:1/0:2:1/0:3:1"
    r = run_app(b, w, "molbuild_noshape_light", dict(BASE, PL_MOLBUILD=ch3), frames=20, theme="light")
    st = stats(r)
    check(bool(st) and st[-1]["mode"] == "noshape" and st[-1]["atoms"] == "0", f"noshape: no shape drawn ({st[-1] if st else None})")
    m = mb(r)
    check(m.get("shape") == "none" and m.get("open") == "1", f"noshape: 1 open valence, no shape ({m})")
    check(any("open valence" in t for _, _, t in r.texts), "noshape: the panel says why (open valence)")

def part_settings(b, w):
    sf = os.path.join(w, "s_build.txt")
    if os.path.exists(sf):
        os.remove(sf)
    run_app(b, w, "molbuild_save", dict(BASE, PL_MOLBUILD=ETHANOL, PL_SETTINGS=sf), frames=60)   # (the file is written 0.25 s after the last change: enough frames for that)
    txt = open(sf).read() if os.path.exists(sf) else ""
    line = [ln for ln in txt.splitlines() if ln.startswith("molbuild=")]
    check(len(line) == 1 and line[0] == "molbuild=" + ETHANOL, f"settings: the molbuild line written as built ({line})")
    r = run_app(b, w, "molbuild_load", dict(BASE, PL_SETTINGS=sf), frames=10)
    m = mb(r)
    check(m.get("atoms") == "9" and m.get("hill") == "C2H6O", f"settings: the next start opens the same sketch ({m})")
    bad = os.path.join(w, "s_bad.txt")
    open(bad, "w").write("lab=Molecule Lab\nmolbuild=v1 mode=build view=3d atoms=C:0:0/Xx:1:0 bonds=0:1:1\n")
    r2 = run_app(b, w, "molbuild_bad", dict(BASE, PL_SETTINGS=bad), frames=10)
    st = stats(r2)
    check(bool(st) and st[-1]["mode"] == "viewer", f"settings: a damaged line falls back to the Viewer and an empty sketch, no crash ({st[-1] if st else None})")
    old = os.path.join(w, "s_old.txt")
    open(old, "w").write("lab=Molecule Lab\nmolecule=v1 mol=NH3 style=ball labels=1 spin=0 zoom=1 pan=0,0 rot=1,0,0,0,1,0,0,0,1\n")
    run_app(b, w, "molbuild_viewer_only", dict(BASE, PL_SETTINGS=old), frames=60)
    t2 = open(old).read()
    check(t2.startswith("# Physics Lab") and "molecule=v1 mol=NH3" in t2 and "molbuild=" not in t2, "settings: a run that never opened Build mode rewrites the file without a molbuild line")

def part_u238(b, w):
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
        check(int(nuc[-1]["folded"]) > 0, f"{name}: the rest of the text is folded, not dropped ({nuc[-1]})")
        if name == "U-238":                                               # a click on the plate opens it: every row is there again
            env2 = dict(env, PL_SCRIPT=f"10:click=nucleus.nucleus;150:clickxy={x + 20:.0f},{y + ih - 8:.0f};156:astate")
            r2 = run_app(b, w, "u238_open", env2, frames=162)
            n2 = [dict(re.findall(r"(\w+)=(\S+)", ln)) for ln in r2.out.splitlines() if ln.startswith("PLATOMNUC ")]
            b2 = [ln for ln in r2.out.splitlines() if ln.startswith("PLATOMBOX ")]
            if n2 and b2:
                h2 = float(re.search(r"info=[-\d.e]+,[-\d.e]+,[-\d.e]+,([-\d.e]+)", b2[-1]).group(1))
                check(n2[-1]["open"] == "1" and n2[-1]["folded"] == "0" and h2 > ih + 18 * int(nuc[-1]["folded"]) - 1, f"U-238: a click opens the folded text, all rows shown ({n2[-1]}, height {ih} -> {h2})")
            else:
                check(False, "U-238: the opened state was reported")

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
        # 1.5 molecule radii, in points: the radius is what fits 0.42 of the free area's smaller side at zoom 1 (molecule.cpp fitScale)
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
    {"sketch": part_sketch, "edit": part_edit, "refuse": part_refuse, "shape": part_shape, "noshape": part_noshape, "settings": part_settings,
     "u238": part_u238, "sheet": part_sheet, "molpan": part_molpan}[part](binary, work)
    print(f"check_molbuild {part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
