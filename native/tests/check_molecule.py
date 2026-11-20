#!/usr/bin/env python3
# 0.18 stage 1: the Molecule Lab in the REAL app (Linux, software OpenGL; NOT Windows, NOT a real GPU, NO timing: counts only).
#   Water, ball-and-stick, Dark theme, and benzene, space-filling, Light theme: pictures to look at (molecule_dark.bmp, molecule_light.bmp), the draw counts
#   (the whole molecule ONE instanced draw call), the CPK colour at an atom's middle, the bottom bar where the other labs have their toolbar;
#   a click on an atom and on a bond picks it (the panel shows it); the settings are saved when the app closes and read at the next start; a damaged settings line falls back.
#   rotation (0.53, its own part): the Rotation section for HCl has its three controls (level, isotope, spectrum), is reduced for H2 (no isotope, no spectrum choice),
#            is one line for water; the Rotation card's lines can be pointed at (the strongest line is named), and Turn turns the molecule at its slowed-down rate.
#   anharm (0.53.1, its own part): HCl's Vibrations show the tick box "Anharmonic (real bond)"; ticked, the level control and the Bond energy card appear; the
#            level goes to the last bound one and one above it, where the readout says dissociated; water has no tick box.
#   card (0.57.1, its own part): the Density cloud on this PC's renderer (Mesa llvmpipe here, NOT the owner's card): its start-up check against the
#            processor (every dot within its derived single-precision limit, molecule/mol_cloud.h), the benchmark's paths, the picture against the
#            processor's, and its guard: the process ended once inside each step (PL_GPU_DIE), the next start runs nothing of it and Settings, Performance
#            names the step; a changed shader tries again; Try again works in the same run.
#   check_molecule.py <PhysicsLab binary> <work folder> [rotation | anharm | card]
import os, sys, re
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, Bmp, inside, prefetch

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
        check(v["mol"] == "C6H6" and v["atoms"] == "12" and v["sticks"] == "0" and int(v["blobs"]) == 12, f"light: benzene space-filling, 12 balls (0.40: true spheres that draw their own outline; were 12 balls + 12 outline discs), no sticks ({v})")
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

def spheres(binary, work):
    """0.40: Space-filling balls are true spheres. Hydrogen chloride, seen as the lab first shows it: at pixels inside BOTH discs, the colour must be that of the
    sphere whose SURFACE is nearer there (H white or Cl green), even where the other ball's centre is nearer. Painting whole discs back to front by their centres
    (until 0.40) put one disc wholly over the other, so a ball jumped in front of its neighbour as the molecule turned."""
    import math
    r = run_app(binary, work, "molecule_hcl_fill", {"PL_MOLECULE": "HCl", "PL_MOLECULE_STYLE": "fill"}, frames=20)
    n = [x for x in r.notes if x.startswith("molatoms ")]
    check(bool(n), "the balls' places are reported")
    if not n: return
    balls = [tuple(map(float, t.split(","))) for t in n[-1].split()[1:]]
    check(len(balls) == 2, f"two balls ({len(balls)})")
    if len(balls) != 2: return
    b = Bmp(r.bmp)
    def zs(x, y, ball):
        bx, by, br, bz = ball
        q = (x - bx) ** 2 + (y - by) ** 2
        return bz + math.sqrt(br * br - q) if q < br * br else None
    want, wrong = 0, 0
    painter_front = 0 if balls[0][3] > balls[1][3] else 1
    tested = 0
    for y in range(0, b.h, 3):
        for x in range(0, b.w, 3):
            z = [zs(x + 0.5, y + 0.5, bl) for bl in balls]
            if z[0] is None or z[1] is None or abs(z[0] - z[1]) < 6: continue   # inside both, and not near the curve where they meet
            if min(math.hypot(x - bl[0], y - bl[1]) for bl in balls) < 14: continue  # (not on the element labels)
            front = 0 if z[0] > z[1] else 1
            if front == painter_front: continue                                   # only the pixels where the old drawing was wrong
            R, G, B = b.px(x, y)
            white = min(R, G, B) > 0.75 * max(R, G, B)
            tested += 1
            if white == (front == 0): want += 1
            else: wrong += 1
    check(tested >= 20, f"there are pixels where the nearer surface is not the nearer centre's ball ({tested})")
    check(wrong == 0, f"there the nearer SURFACE's colour is shown (right {want}, wrong {wrong})")

def rotnote(r):
    n = r.note("rot ")
    return dict(kv.split("=") for kv in n[-1].split()[1:]) if n else None
def cardnote(r):
    n = r.note("rotcard ")
    return dict(kv.split("=", 1) for kv in n[-1].split()[1:]) if n else None

def rotation(binary, work):
    import math
    hcl = run_app(binary, work, "rot_hcl", {"PL_MOLECULE": "HCl", "PL_SCRIPT": "8:click=molecule.rot.card"}, frames=20)
    # (a segmented control's parts are logged as seg.<id>...; the level's buttons by their words)
    has = lambda r, k: any(t[2] == "Most populated" for t in r.texts) if k == "molecule.rot.jup" else any(n.startswith("seg." + k + ".") for n in r.rects)
    check(not hcl.scripted_missing() and has(hcl, "molecule.rot.jup") and has(hcl, "molecule.rot.iso") and has(hcl, "molecule.rot.spec"), f"HCl: the Rotation section has the level, isotope and spectrum controls ({hcl.scripted_missing()})")
    c = cardnote(hcl)
    check(c is not None and int(c["lines"]) > 20 and c["hover"] == "-", f"HCl: the card draws the band's lines, none pointed at yet ({c})")
    rn = rotnote(hcl)
    check(rn is not None and rn["linear"] == "1" and rn["J"] == "3" and rn["iso"] == "4", f"HCl at 298 K: the level shown is the most populated, J = 3; the natural mix ({rn})")
    if c:
        x, y = float(c["x"]), float(c["y"])
        pt = run_app(binary, work, "rot_hover", {"PL_MOLECULE": "HCl", "PL_MOTION": "off", "PL_SCRIPT": f"8:click=molecule.rot.card;12:click=molecule.rot.turn;16:move={x},{y}"}, frames=40)
        c2, r2 = cardnote(pt), rotnote(pt)
        check(c2 is not None and c2["hover"] == c["strong"], f"pointing at the strongest line names it ({c['strong']}: {c2})")
        # the turn: J = 3 of H35Cl, 2 pi t / T on screen (T from the app's own formula, checked in rotation_tests); the click lands 2 frames after frame 12, 0.016 s a frame
        ok = r2 is not None and r2["on"] == "1" and 0 < float(r2["angle"]) < 2 * math.pi * 30 * 0.016 / 2.0
        check(ok, f"Turn: the molecule turns, slowed down ({r2})")
        check(differ_ok(hcl, pt), "Turn: the picture changes")
    h2 = run_app(binary, work, "rot_h2", {"PL_MOLECULE": "H2"}, frames=12)
    has = lambda r, k: any(t[2] == "Most populated" for t in r.texts) if k == "molecule.rot.jup" else any(n.startswith("seg." + k + ".") for n in r.rects)
    check(has(h2, "molecule.rot.jup") and not has(h2, "molecule.rot.iso") and not has(h2, "molecule.rot.spec"), "H2: the section has the level, and no isotope or spectrum choice")
    check(rotnote(h2) is not None and rotnote(h2)["J"] == "1", f"H2 at 298 K: J = 1 (ortho) holds most ({rotnote(h2)})")
    w = run_app(binary, work, "rot_h2o", {"PL_MOLECULE": "H2O"}, frames=12)
    check(rotnote(w) is not None and rotnote(w)["linear"] == "0" and any("non-linear molecules is not shown yet" in t[2] for t in w.texts) and not has(w, "molecule.rot.jup"),
          "water: one line, no controls")

def vibnote(r, i=-1):
    n = r.note("vib ")
    return dict(kv.split("=") for kv in n[i].split()[1:]) if n else None

def anharm(binary, work):
    hcl = run_app(binary, work, "anh_hcl", {"PL_MOLECULE": "HCl", "PL_SCRIPT": "6:click=molecule.vib.0;14:click=molecule.vib.anharm;22:click=molecule.vib.vtop;30:click=molecule.vib.vup"}, frames=38)
    check(not hcl.scripted_missing(), f"HCl: the mode, the tick box and the level buttons were there to click ({hcl.scripted_missing()})")
    check(any("Anharmonic (real bond)" in t[2] for t in hcl.texts), "HCl: the tick box is offered")
    check(any("above v = 27: dissociated" in t[2] for t in hcl.texts), "HCl: Last bound went to v = 27 = floor(we / 2wexe - 1/2), and + one above it")
    v = vibnote(hcl)
    check(v is not None and v["anh"] == "1" and v["lev"] == "28" and v["diss"] == "1" and any("Dissociated" in t[2] for t in hcl.texts), f"HCl: one above the last bound level the readout says dissociated ({v})")
    bc = hcl.note("bondcard ")
    check(bool(bc) and "mol=HCl" in bc[-1] and any(n.startswith("card.molecule.bond") for n in hcl.rects), f"HCl: the Bond energy card shows ({bc[-1] if bc else None})")
    w = run_app(binary, work, "anh_h2o", {"PL_MOLECULE": "H2O", "PL_SCRIPT": "6:click=molecule.vib.0"}, frames=12)
    wv = vibnote(w)
    check(wv is not None and wv["on"] == "1" and wv["anhbox"] == "0" and not any("Anharmonic (real bond)" in t[2] for t in w.texts) and not w.note("bondcard "), f"water: no tick box, no card ({wv})")

MOL_STEPS = ["compile the Density cloud's shaders", "link the Density cloud's shaders", "the Density cloud's first small run", "the Density cloud's check", "the Density cloud's first full use"]
MOL_BENCH = {"PL_GRAPHICS": "opengl", "PL_BENCH": "start", "PL_BENCH_QUIT": "1", "PL_FIXED_DT": "0", "PL_BENCH_FINISH": "1", "PL_BENCH_ONLY": "molecule.density,molecule.vib_density",
             "PL_BENCH_WARMUP": "1.0", "PL_BENCH_MEASURE": "1.5"}

def card(binary, work):
    def folder(name):
        d = os.path.join(work, name); os.makedirs(d, exist_ok=True)
        for f in os.listdir(d): os.remove(os.path.join(d, f))
        return d
    def read(path):
        return open(path, encoding="utf-8").read() if os.path.exists(path) else ""
    def lines_of(log):
        return "\n".join(ln for ln in log.splitlines() if ln.startswith("[molcloud] "))
    BIG = dict(frames=100000, size="880x560", timeout=1500)
    dirs = {"ok": folder("mc_ok")}
    calls = [((binary, work, "mc_ok", dict(MOL_BENCH, PL_SETTINGS=os.path.join(dirs["ok"], "settings.txt"))), BIG)]
    for i, st in enumerate(MOL_STEPS):
        d = folder(f"mc_die{i}")
        dirs[i] = d
        calls.append(((binary, work, f"mc_die{i}", dict(MOL_BENCH, PL_SETTINGS=os.path.join(d, "settings.txt"), PL_GPU_DIE=st)), BIG))
    # the picture: water and benzene in the Density style, cloud and surface, the card's cloud and the processor's (PL_MOL_CARD=0), the same frames
    look = {"PL_MOLECULE_STYLE": "real", "PL_DRAWSTAT": "1", "PL_GRAPHICS": "opengl"}   # (0.63.0: OpenGL's card program and guard records)
    PICS = [("H2O", "dark"), ("C6H6", "dark"), ("H2O", "light")]   # (the light page: the darker dot colour, drawn over instead of added)
    pics = [((binary, work, f"mc_pic_{m}_{th}_{w}", dict(look, PL_MOLECULE=m, PL_THEME=th, **({"PL_MOL_CARD": "0"} if w == "cpu" else {}))), dict(frames=90, theme=th)) for m, th in PICS for w in ("card", "cpu")]   # (90 frames: the steps take a frame each, the read-backs a few more on a busy PC)
    prefetch(calls + pics)
    results = [run_app(*a, **kw) for a, kw in calls]
    ok = results[0]
    out = ok.out
    # 1. the start-up check passed, every case within its derived limits
    check("PLMOLCARD state=1" in out, "card: the Density cloud's program passes its start-up check here")
    cases = [ln for ln in out.splitlines() if ln.startswith("PLMOLCARDCASE ")]
    def case_ok(ln):
        m = re.search(r"(\d+) drawn by one side only \((\d+) within the rounding at the picture's edge\); (\d+) beyond their limit", ln)
        return m and m.group(1) == m.group(2) and m.group(3) == "0"
    check(len(cases) == 12 and all(case_ok(ln) for ln in cases), f"card: 6 cameras x (the lab's own dots, a box beyond them): every dot within its derived limit, every one-sided dot at the picture's edge {cases}")
    # 2. the benchmark says which path ran and why
    rep = out[out.find("PLBENCH-RESULT-BEGIN"):out.find("PLBENCH-RESULT-END")]
    paths = [ln.strip() for ln in rep.splitlines() if ln.strip().startswith("Path:")]
    card_paths = [p for p in paths if "the Density cloud's dots: the graphics card (its start-up check passed" in p]
    check(len(card_paths) == 2, f"card: the benchmark's report names the card for both scenes, with the check {paths}")
    # 3. the log: every step in order with its time, the whole path succeeded, kept "ok", no marker left
    log = lines_of(read(os.path.join(dirs["ok"], "gpu-paths-log.txt")))
    st = read(os.path.join(dirs["ok"], "settings.txt"))
    at, missing = 0, []
    for w in [x for s_ in MOL_STEPS for x in ("begin " + s_, "end " + s_)] + ["the whole path succeeded"]:
        k = log.find(w, at)
        if k < 0: missing.append(w)
        else: at = k
    timed = all(re.search(r"end " + re.escape(s_) + r"[^\n]* in \d+ ms", log) for s_ in MOL_STEPS[:2])
    check(not missing and timed, f"card: the log names every step in order, the compile and link with their times (missing: {missing})")
    check("gpumolcloud=ok | molcloud " in st and not os.path.exists(os.path.join(dirs["ok"], "gpu-molcloud-step.txt")), "card: kept as ok in the settings file, no marker left")
    # 4. the process ended inside each step: the marker names it; the next start runs nothing of it, the processor is used, Settings names the step
    for i, s_ in enumerate(MOL_STEPS):
        marker = read(os.path.join(dirs[i], "gpu-molcloud-step.txt"))
        check(f"step={s_}\n" in marker and "version=molcloud " in marker, f"card: the process ended inside '{s_}': the marker names the step ({marker!r})")
    after = [((binary, work, f"mc_after{i}", {"PL_GRAPHICS": "opengl", "PL_MOLECULE": "C6H6", "PL_MOLECULE_STYLE": "real", "PL_SETTINGS": os.path.join(dirs[i], "settings.txt"), "PL_OPEN": "settings", "PL_DRAWSTAT": "1"}),
              dict(frames=30, size="880x560", timeout=900)) for i in range(len(MOL_STEPS))]
    prefetch(after)
    for i, s_ in enumerate(MOL_STEPS):
        a, kw = after[i]
        r = run_app(*a, **kw)
        lg = lines_of(read(os.path.join(dirs[i], "gpu-paths-log.txt")))
        said = [n for n in r.notes if n.startswith("molcloud ") and "stopped the app last time" in n and f"'{s_}'" in n]
        st_ = stats(r)
        check("PLMOLCARD state=-1" in r.out and not any(("begin " + x) in lg for x in MOL_STEPS) and said and st_ and all(x.get("card") == "0" for x in st_) and int(st_[-1]["dots"]) > 50000,
              f"card: after '{s_}' nothing of the program runs, the processor draws the cloud, Settings names the step (said {bool(said)})")
        txt = read(os.path.join(dirs[i], "settings.txt"))
        check("gpumolcloud=off | molcloud " in txt and f"| {s_}\n" in txt and not os.path.exists(os.path.join(dirs[i], "gpu-molcloud-step.txt")), f"card: the settings file keeps it off with the step '{s_}', the marker gone")
    # 5. a changed shader tries again (and passes here); Try again passes in the same run
    sf = os.path.join(dirs[0], "settings.txt")
    sf2 = os.path.join(dirs[0], "settings_shader.txt")
    with open(sf2, "w", encoding="utf-8") as f: f.write(read(sf))
    r5 = run_app(binary, work, "mc_shader", dict(MOL_BENCH, PL_SETTINGS=sf2, PL_GPU_MOLSHADER="molcloud 0000000000000000"), frames=100000, size="880x560", timeout=1500)
    check("PLMOLCARD state=1" in r5.out, "card: a changed shader tries again (and passes here)")
    sf = os.path.join(dirs[1], "settings.txt")
    r4 = run_app(binary, work, "mc_again", {"PL_GRAPHICS": "opengl", "PL_MOLECULE": "H2O", "PL_MOLECULE_STYLE": "real", "PL_SETTINGS": sf, "PL_OPEN": "settings:gpumolcloud", "PL_SCRIPT": "14:click=gpumolcloud.again"}, frames=60, size="880x560", timeout=900)
    check(not r4.scripted_missing() and "PLMOLCARD state=1" in r4.out and "gpumolcloud=ok" in read(sf), f"card: Try again: the program is made, checked and used again in the same run ({r4.scripted_missing()})")
    # 6. the picture: the card's cloud against the processor's. The dots themselves are checked against their derived limits above; here a sanity bound on
    #    the whole picture (a wrong exposure, a missing or misplaced set of dots or the wrong colour gives tens of levels): every frame drawn by the card
    #    once its check passed, and the mean difference per channel under one level of 255
    for k, (m, th) in enumerate(PICS):
        rc, rp = run_app(*pics[2 * k][0], **pics[2 * k][1]), run_app(*pics[2 * k + 1][0], **pics[2 * k + 1][1])
        m = m + " " + th
        sc, sp = stats(rc), stats(rp)
        check(sc and sc[-1].get("card") == "1" and sp and sp[-1].get("card") == "0" and abs(int(sc[-1]["dots"]) - int(sp[-1]["dots"])) <= 5,
              f"card: {m}: the card drew the last frames, the processor the other run, the same dots on the picture ({sc[-1] if sc else None} / {sp[-1] if sp else None})")
        A, B = Bmp(rc.bmp), Bmp(rp.bmp)
        tot = n = big = 0
        for y in range(0, A.h, 2):
            for x in range(0, A.w, 2):
                a, b = A.px(x, y), B.px(x, y)
                dd = max(abs(a[i] - b[i]) for i in range(3))
                tot += sum(abs(a[i] - b[i]) for i in range(3)); n += 3; big += dd > 8
        check(n and tot / n < 1.0 and big < 0.001 * n / 3, f"card: {m}: the card's picture is the processor's (mean difference {tot / max(n, 1):.3f} of 255 per channel, {big} pixels more than 8 levels off)")

def cardsurface(binary, work):
    # 0.68.0: the Density surface on the card (Vulkan): its start-up check passes (three surfaces against the processor's, molecule/surface_check.h); the
    # pictures of molecule.density's and molecule.vib_density's set-ups (benzene; water vibrating), card against processor (PL_NO_SURFACE_CARD),
    # multisampling off, differ in at most 1 % of the pixels by more than 8 levels; made to fail on purpose (PL_SURFACE_CARDFAIL) the processor's triangles
    # are drawn
    def over(x, y):
        px, py = x.pixels(), y.pixels()
        st = x.bpp
        return sum(1 for i in range(0, min(len(px), len(py)) - st + 1, st) if max(abs(px[i] - py[i]), abs(px[i + 1] - py[i + 1]), abs(px[i + 2] - py[i + 2])) > 8)
    for sc in ("molecule.density", "molecule.vib_density"):
        env = {"PL_GRAPHICS": "vulkan", "PL_NO_MSAA": "1", "PL_BENCH_SETUP": sc}
        c = run_app(binary, work, "surf_card_" + sc, env, frames=60)
        p = run_app(binary, work, "surf_cpu_" + sc, dict(env, PL_NO_SURFACE_CARD="1"), frames=60)
        if sc == "molecule.density":
            cases = [ln for ln in c.out.splitlines() if ln.startswith("PLSURFACECASE ")]
            check("PLSURFACECARD state=1" in c.out and len(cases) == 3, f"surface: the start-up check passes, three cases ({cases})")
            bad = run_app(binary, work, "surf_fail", dict(env, PL_SURFACE_CARDFAIL="1"), frames=60)
            check("PLSURFACECARD state=-1" in bad.out, "surface: a failed check leaves the processor's triangles")
            o2 = over(Bmp(bad.bmp), Bmp(p.bmp))
            check(o2 <= 100, f"surface: the failed check's picture is the processor's ({o2} pixels over 8 levels; two runs of the processor's own may differ in a few)")
        a, b = Bmp(c.bmp), Bmp(p.bmp)
        o = over(a, b)
        print(f"  {sc}: card against processor, {o} pixels over 8 levels ({100 * o / max(1, a.w * a.h):.3f} %)")
        check((a.w, a.h) == (b.w, b.h) and a.w > 0 and o <= 0.01 * a.w * a.h, f"surface, {sc}: the card's picture is the processor's within the limit ({o} pixels over 8 levels)")

def differ_ok(a, b):
    A, B = Bmp(a.bmp), Bmp(b.bmp)
    return sum(1 for y in range(60, 740, 3) for x in range(0, 900, 3) if A.px(x, y) != B.px(x, y)) > 30

if __name__ == "__main__":
    binary, work = sys.argv[1], sys.argv[2]
    os.makedirs(work, exist_ok=True)
    if len(sys.argv) > 3 and sys.argv[3] == "rotation":
        rotation(binary, work)
    elif len(sys.argv) > 3 and sys.argv[3] == "anharm":
        anharm(binary, work)
    elif len(sys.argv) > 3 and sys.argv[3] == "card":
        card(binary, work)
    elif len(sys.argv) > 3 and sys.argv[3] == "cardsurface":
        cardsurface(binary, work)
    else:
        main(binary, work)
        spheres(binary, work)
    print(f"check_molecule: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
