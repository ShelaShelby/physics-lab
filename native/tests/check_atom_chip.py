#!/usr/bin/env python3
# 0.15 stage 5b: the Atom Lab's element chip and its dropdown (New UI On) in the real app.
#   check_atom_chip.py <PhysicsLab binary> <work folder> [card | classic | classicside | classicsideglow | classicslice | classicsliceglow]   (tests/TESTS.md)
#   card (0.57.0): the Atom Lab's two card programs (render/atom_card.h) on this PC's renderer (Mesa llvmpipe here, NOT the owner's card): their start-up
#   checks against the processor at the same seeds within the derived single-precision limits (atom/atom_card.h), the benchmark's paths, and their guards:
#   the process ended inside the first step (PL_GPU_DIE; 0.76.0: one step, git keeps the run of all five), the next start runs nothing of that program and Settings, Performance names the step; a changed
#   shader tries again; Try again works in the same run.
# References (nothing here is an earlier output of ours): the standard periodic table's places (H in column 1, He in 18, Fe in row 4 column 8, B in row 2 column 13; the lanthanides and
# actinides in two rows below), the spec's words ("1 proton", "0 neutrons"), the element names written in the famous list's own labels, and the app's own old setParam rule that a new element
# gets its usual isotope (the unit test in atom_tests holds the textbook mass numbers). Twelve elements across the table are clicked in the real app (0.76.0; git keeps the run of all 118).
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, inside, overlap, prefetch, Bmp

KEYS = ["view", "layer", "zoom", "zoomTo", "focus", "drag", "paused", "yaw", "pitch", "oyaw", "opitch", "ctilt", "czoom", "Z", "N", "usualN", "seed"]

def states(r):
    out = []
    for ln in r.states:
        pos = sorted((ln.find(k + "="), k) for k in KEYS)
        d = {}
        for n, (i, k) in enumerate(pos):
            j = pos[n + 1][0] if n + 1 < len(pos) else len(ln)
            d[k] = ln[i + len(k) + 1:j].strip()
        out.append(d)
    return out

CARD_STEPS = {"slice": ["compile the Slice's dots shader", "link the Slice's dots shader", "the Slice's first small run", "the Slice's check", "the Slice's first full use"],
              "jump": ["compile the jump's groups shader", "link the jump's groups shader", "the jump's first small run", "the jump's check", "the jump's first full use"]}
CARD_BENCH = {"PL_BENCH": "start", "PL_BENCH_QUIT": "1", "PL_FIXED_DT": "0", "PL_BENCH_FINISH": "1", "PL_BENCH_ONLY": "atom.slice_drag,atom.jump_all,atom.slice_super_drag",
              "PL_BENCH_WARMUP": "6.0", "PL_BENCH_MEASURE": "1.5"}   # (0.69.0: 6 s, was 1: on Vulkan the steps take their turns, one at a time, through the warm-up)

# 0.76.0: one part per scene (a run of all five took 12 min, over the 5-minute part limit). Each part runs the scene on the card and on the processor.
CLASSIC_SCENES = {"classic": "atom.all_classic", "classicside": "atom.side_all_classic", "classicsideglow": "atom.side_all_glow",
                  "classicslice": "atom.slice_all_drag", "classicsliceglow": "atom.slice_glow_ultra"}

def classic_part(binary, work, scene):
    # 0.64.0: the Classic Cloud 3D dots from the card (Vulkan): the start-up check passes here (every dot's group, band and place, the squares in the
    # processor's order), the benchmark's Path names the card; made to fail on purpose (PL_CLASSIC_CARDFAIL), the processor's squares are kept and the
    # Path says why. The two pictures (card, processor) of the same moment differ in under 1 % of the pixels by more than 8 levels.
    # 0.65.0: the side view with every electron (Classic and Glow) and the Slice's dots (every electron, Classic; one state, Glow) from the same program:
    # their Paths name the card, the same picture rule.
    os.makedirs(work, exist_ok=True)
    fails = checks = 0
    def check(ok, what):
        nonlocal fails, checks
        checks += 1
        if not ok:
            fails += 1
            print("FAIL", what)
    B = {"PL_BENCH": "start", "PL_BENCH_QUIT": "1", "PL_FIXED_DT": "0.016", "PL_BENCH_FINISH": "1", "PL_BENCH_ONLY": scene,
         "PL_BENCH_WARMUP": "0.3", "PL_BENCH_MEASURE": "2.0", "PL_BENCH_SHOT": "1.5"}
    runs = {}
    for tag, extra in (("card", {}), ("cpu", {"PL_CLASSIC_CARDFAIL": "1"})):
        d = os.path.join(work, "classic_" + tag); os.makedirs(d, exist_ok=True)
        for f in os.listdir(d):
            if f.endswith(".bmp"): os.remove(os.path.join(d, f))
        runs[tag] = run_app(binary, d, "classic_" + tag, dict(B, **extra), frames=100000, size="1280x800", timeout=900)
    def paths(out):
        rep = out[out.find("PLBENCH-RESULT-BEGIN"):out.find("PLBENCH-RESULT-END")]
        return [ln.strip() for ln in rep.splitlines() if ln.strip().startswith("Path:")]
    out = runs["card"].out
    case = [ln for ln in out.splitlines() if ln.startswith("PLCLASSICCARDCASE ")]
    check("PLCLASSICCARD state=1" in out and len(case) == 1 and re.search(r"0 in another group, \d+ in another band \((\d+) at", case[0]) and " 0 places beyond" in case[0] and "0 keys out of order, 0 squares wrong" in case[0],
          f"classic: the start-up check passes ({case})")
    check(not case or ("| the side view, Glow " in case[0] and "0 beyond their limit" in case[0] and "| the Slice, Glow " in case[0]), f"classic: the side view's and the Slice's checks passed ({case})")
    if scene == "atom.all_classic":
        check(any("the Classic dots: the graphics card (Vulkan; its start-up check passed" in p for p in paths(out)), f"classic: the benchmark's Path names the card ({paths(out)})")
        out2 = runs["cpu"].out
        check("PLCLASSICCARD state=-1" in out2 and any("the Classic dots: the processor, because the check was made to fail" in p for p in paths(out2)), f"classic: a failed check keeps the processor's squares and says why ({paths(out2)})")
    elif scene.startswith("atom.side"):
        check(sum("the side view's dots: the graphics card (Vulkan; its start-up check passed" in p for p in paths(out)) == 1, f"classic: the side view's Path names the card ({paths(out)})")
    else:
        check(sum("drawn: the graphics card (Vulkan; its start-up check passed" in p for p in paths(out)) == 1, f"classic: the Slice's Path names the card ({paths(out)})")
    for scene in (scene,):
        try:
            a, b = Bmp(os.path.join(work, "classic_card", scene + ".bmp")), Bmp(os.path.join(work, "classic_cpu", scene + ".bmp"))
            n = over = 0
            for y in range(0, a.h, 2):
                for x in range(0, a.w, 2):
                    n += 1
                    over += max(abs(p - q) for p, q in zip(a.px(x, y)[:3], b.px(x, y)[:3])) > 8
            print(f"classic: {scene}: {over} of {n} pixels over 8 levels")
            check(n > 0 and over <= 0.01 * n, f"classic: {scene}: the card's picture and the processor's of the same moment: {over} of {n} pixels over 8 levels")
        except Exception as e:
            check(False, f"classic: {scene}: both pictures saved ({e})")
    print(f"{checks} checks, {fails} failed")
    return 1 if fails else 0

def card_part(binary, work):
    os.makedirs(work, exist_ok=True)
    fails = checks = 0
    def check(ok, what):
        nonlocal fails, checks
        checks += 1
        if not ok:
            fails += 1
            print("FAIL", what)
    def folder(name):
        d = os.path.join(work, name); os.makedirs(d, exist_ok=True)
        for f in os.listdir(d): os.remove(os.path.join(d, f))
        return d
    def read(path):
        return open(path, encoding="utf-8").read() if os.path.exists(path) else ""
    def lines_of(log, which):
        return "\n".join(ln for ln in log.splitlines() if ln.startswith(f"[vulkan] [{which}] "))   # (Vulkan's guards)
    # one normal run of both scenes, the steps of every die run and the runs after them, in parallel
    BIG = dict(frames=100000, size="880x560", timeout=1500)
    dirs = {"ok": folder("card_ok")}
    calls = [((binary, work, "card_ok", dict(CARD_BENCH, PL_SETTINGS=os.path.join(dirs["ok"], "settings.txt"))), BIG)]
    for which, steps in CARD_STEPS.items():
        for i, st in enumerate(steps[:1]):   # (0.76.0: one step per program: the guard is the same for every step; git keeps the runs of all five)
            d = folder(f"card_die_{which}{i}")
            dirs[(which, i)] = d
            calls.append(((binary, work, f"card_die_{which}{i}", dict(CARD_BENCH, PL_SETTINGS=os.path.join(d, "settings.txt"), PL_GPU_DIE=st)), BIG))
    prefetch(calls)
    results = [run_app(*a, **kw) for a, kw in calls]
    ok = results[0]
    out = ok.out
    # 1. the start-up checks passed, every case within its limits
    check("PLATOMCARD slice state=1" in out and "PLATOMCARD jump state=1" in out, "card: both programs pass their start-up check here")
    cases = [ln for ln in out.splitlines() if ln.startswith("PLATOMCARDCASE ")]
    sl = [ln for ln in cases if ln.startswith("PLATOMCARDCASE slice")]
    jp = [ln for ln in cases if ln.startswith("PLATOMCARDCASE jump")]
    def slice_ok(ln):
        m = re.search(r"(\d+) answered differently \((\d+) within the rounding there\); points (\d+) beyond their limit", ln)
        return m and m.group(1) == m.group(2) and m.group(3) == "0"
    check(len(sl) == 7 and sum("(a superposition's state)" in ln for ln in sl) == 2 and all(slice_ok(ln) for ln in sl), f"card: the Slice's dots at 7 planes (0.64.0: two of a superposition's states), the same seeds: every point within its derived limit, every differing answer within the rounding at its margin {sl}")
    check(len(jp) == 3 and all(re.search(r": 0 of \d+ groups differ", ln) for ln in jp), f"card: the jump's groups equal the processor's exactly {jp}")
    # 2. the benchmark says which path ran and why
    rep = out[out.find("PLBENCH-RESULT-BEGIN"):out.find("PLBENCH-RESULT-END")]
    paths = [ln.strip() for ln in rep.splitlines() if ln.strip().startswith("Path:")]
    check(any("the Slice's new dots: the graphics card (its start-up check passed" in p for p in paths) and any("drawn from a buffer on the graphics card; the jump's groups: the graphics card (its start-up check passed" in p for p in paths),
          f"card: the benchmark's report names the card for both scenes, with the check {paths}")
    # 0.64.0: the Slice of a superposition (atom.slice_super_drag) from the card too
    check(sum("the Slice's new dots: the graphics card (its start-up check passed" in p for p in paths) >= 2, f"card: atom.slice_super_drag's Path names the graphics card {paths}")
    # 3. the log: every step in order with its time, the whole path succeeded, kept "ok", no marker left
    log = read(os.path.join(dirs["ok"], "gpu-paths-log.txt"))
    st = read(os.path.join(dirs["ok"], "settings.txt"))
    for which, steps in CARD_STEPS.items():
        part = lines_of(log, which)
        at, missing = 0, []
        for w in [x for s_ in steps for x in ("begin " + s_, "end " + s_)] + ["the whole path succeeded"]:
            k = part.find(w, at)
            if k < 0: missing.append(w)
            else: at = k
        timed = all(re.search(r"end " + re.escape(s_) + r"[^\n]* in \d+ ms", part) for s_ in steps[:2])
        check(not missing and timed, f"card: {which}: the log names every step in order, the compile and link with their times (missing: {missing})")
        check(f"gpuvk{which}=ok | vulkan {which} " in st and not os.path.exists(os.path.join(dirs["ok"], f"gpu-vk{which}-step.txt")), f"card: {which}: kept as ok in the settings file, no marker left")
    # 4. the process ended inside each step: the marker names it; the next start runs nothing of that program, the processor is used, Settings names the step
    after = []
    for which, steps in CARD_STEPS.items():
        for i, s_ in enumerate(steps[:1]):
            d = dirs[(which, i)]
            after.append(((binary, work, f"card_after_{which}{i}", {"PL_ATOM": "1", "PL_SETTINGS": os.path.join(d, "settings.txt"), "PL_OPEN": "settings", "PL_ATOM_SET": "Z=92;view=Slice" if which == "slice" else "Z=88"}),
                          dict(frames=40, size="880x560", timeout=900)))
    for which, steps in CARD_STEPS.items():
        for i, s_ in enumerate(steps[:1]):
            mk = os.path.join(dirs[(which, i)], f"gpu-vk{which}-step.txt")
            marker = read(mk)
            check(f"step={s_}\n" in marker and f"version=vulkan {which} " in marker, f"card: {which}: the process ended inside '{s_}': the marker names the step ({marker!r})")
    prefetch(after)
    for k, (which, steps) in enumerate(CARD_STEPS.items()):
        for i, s_ in enumerate(steps[:1]):
            a, kw = after[k]
            r = run_app(*a, **kw)
            d = dirs[(which, i)]
            lg = lines_of(read(os.path.join(d, "gpu-paths-log.txt")), which)
            said = [n for n in r.notes if n.startswith(f"atom{which} ") and "stopped the app last time" in n and f"'{s_}'" in n]
            check(f"PLATOMCARD {which} state=-1" in r.out and not any(("begin " + x) in lg for x in steps) and said,
                  f"card: {which}: after '{s_}' nothing of the program runs, the processor is used, Settings names the step (said {bool(said)})")
            txt = read(os.path.join(d, "settings.txt"))
            check(f"gpuvk{which}=off | vulkan {which} " in txt and (f"| {s_}\n" in txt or f"| {s_} | app " in txt) and not os.path.exists(os.path.join(d, f"gpu-vk{which}-step.txt")), f"card: {which}: the settings file keeps it off with the step '{s_}', the marker gone")
    # 5. a changed shader tries again (and passes here); Try again passes in the same run
    for which in CARD_STEPS:
        sf = os.path.join(dirs[(which, 0)], "settings.txt")
        sf2 = os.path.join(dirs[(which, 0)], "settings_shader.txt")
        with open(sf2, "w", encoding="utf-8") as f: f.write(read(sf))
        r5 = run_app(binary, work, f"card_shader_{which}", dict(CARD_BENCH, PL_SETTINGS=sf2, **{f"PL_GPU_{which.upper()}SHADER": f"{which} 0000000000000000"}), frames=100000, size="880x560", timeout=1500)
        check(f"PLATOMCARD {which} state=1" in r5.out, f"card: {which}: a changed shader tries again (and passes here)")
    sf = os.path.join(dirs[("slice", 0)], "settings.txt")
    r4 = run_app(binary, work, "card_again", {"PL_ATOM": "1", "PL_SETTINGS": sf, "PL_OPEN": "settings:gpuslice", "PL_ATOM_SET": "Z=92;view=Slice", "PL_SCRIPT": "14:click=gpuslice.again"}, frames=90, size="880x560", timeout=900)
    # (the cut does not move here, so no new dots are asked for: its first full use waits; the settings file no longer keeps it off)
    check(not r4.scripted_missing() and "PLATOMCARD slice state=1" in r4.out and "gpuvkslice=off" not in read(sf), f"card: Try again: the Slice's program is made and checked again in the same run ({r4.scripted_missing()})")
    print(f"{checks} checks, {fails} failed")
    return 1 if fails else 0

# standard periodic table: (Z, row, column) for a few; lanthanide row 9, actinide row 10 in the app's own numbering
PLACES = [(1, 1, 1), (2, 1, 18), (5, 2, 13), (6, 2, 14), (11, 3, 1), (26, 4, 8), (36, 4, 18), (57, 9, 3), (58, 9, 4), (71, 9, 17), (89, 10, 3), (91, 10, 5), (103, 10, 17), (118, 7, 18)]   # (Z, row, column); rows 9 and 10: lanthanides La-Lu and actinides Ac-Lr, 15 each, under group 3 to group 17 (column 2 holds the * and ** marks)

def main():
    binary, work = sys.argv[1], sys.argv[2]
    if len(sys.argv) > 3 and sys.argv[3] == "card":
        return card_part(binary, work)
    if len(sys.argv) > 3 and sys.argv[3] in CLASSIC_SCENES:
        return classic_part(binary, work, CLASSIC_SCENES[sys.argv[3]])
    os.makedirs(work, exist_ok=True)
    fails = checks = 0
    def check(ok, what):
        nonlocal fails, checks
        checks += 1
        if not ok:
            fails += 1
            print("FAIL", what)
    base = {"PL_ATOM": "1", "PL_NEWUI": "on"}
    def go(name, script, extra=None, frames=60, size="1280x800"):
        r = run_app(binary, work, name, dict(base, PL_SCRIPT=script, **(extra or {})), frames=frames, size=size)
        return r

    # ---- A: the dropdown opens under the chip, never over it, 118 cells of at least 28 u in the table's places ----
    r = go("chip_a", "10:click=top.chip", {"PL_ATOM_SET": "Z=92"})
    check(not r.scripted_missing(), f"A: the chip can be clicked {r.scripted_missing()}")
    panel, chip = r.rects.get("panel.chip"), r.rects.get("chip")
    check(panel is not None and chip is not None, "A: the dropdown is open and the chip is on screen")
    dpi = r.window[2] if r.window else 1.0
    if panel and chip:
        check(not overlap(panel, chip), f"A: the dropdown does not cover the chip: panel {panel} chip {chip}")
        check(panel[1] >= chip[1] + chip[3] - 0.5, "A: the dropdown starts below the chip")
        check(inside(panel, r.window[0], r.window[1]), f"A: the dropdown is inside the window {panel}")
        cells = {z: r.rects.get(f"pt.{z}") for z in range(1, 119)}
        check(all(cells.values()), "A: all 118 element cells are drawn")
        if all(cells.values()):
            check(min(c[2] for c in cells.values()) >= 28 * dpi - 0.5, f"A: every cell is at least 28 u wide (smallest {min(c[2] for c in cells.values()):.1f} px, 28 u = {28 * dpi:.1f})")
            check(all(inside(c, r.window[0], r.window[1]) and c[0] >= panel[0] and c[0] + c[2] <= panel[0] + panel[2] + 0.5 for c in cells.values()), "A: every cell is inside the dropdown")
            zs = sorted(cells)
            clash = sum(1 for i in zs for j in zs if i < j and overlap(cells[i], cells[j]))
            check(clash == 0, f"A: no two cells overlap ({clash})")
            x0 = cells[1][0]
            xstep = (cells[2][0] - cells[1][0]) / 17.0           # He sits in column 18, H in column 1
            ystep = cells[3][1] - cells[1][1]                    # Li is one row below H
            for z, row, c in PLACES:
                okx = abs(cells[z][0] - (x0 + (c - 1) * xstep)) < 1.0
                if row <= 7: oky = abs(cells[z][1] - (cells[1][1] + (row - 1) * ystep)) < 1.0
                else: oky = cells[z][1] > cells[118][1] + 0.5 * ystep and abs(cells[z][1] - cells[57 if row == 9 else 89][1]) < 1.0
                check(okx and oky, f"A: element {z} sits in the standard place (row {row}, column {c})")
            check(cells[58][1] < cells[91][1] and abs(cells[58][1] - cells[71][1]) < 1 and abs(cells[91][1] - cells[103][1]) < 1, "A: lanthanides and actinides are two rows below the table")
    chip_texts = [t for _, _, t in r.texts]
    # 0.38 (the owner's new look): the symbol moved into the element's tile at the chip's left, so the words are "Uranium-238" (they were "U · Uranium-238"); reference changed on purpose
    check("Uranium-238" in chip_texts and "U" in chip_texts and "92 protons · 146 neutrons" in chip_texts, f"A: the chip shows the tile U and says Uranium-238 / 92 protons · 146 neutrons")

    # ---- B: elements in every part of the table can be reached by a click (0.76.0: 12 of the 118, the corners, each block and both rows below; git keeps the run of all) ----
    step = 5
    ZS = (1, 2, 3, 10, 21, 26, 36, 57, 71, 89, 103, 118)
    script = ";".join(f"{10 + step * k}:click=pt.{z};{14 + step * k}:state" for k, z in enumerate(ZS))
    script = "6:click=top.chip;" + script
    rb = go("chip_b", script, frames=10 + step * (len(ZS) + 1) + 10)
    sb = states(rb)
    check(not rb.scripted_missing(), f"B: every scripted click found its cell {rb.scripted_missing()[:3]}")
    check(len(sb) == len(ZS), f"B: {len(ZS)} states ({len(sb)})")
    if len(sb) == len(ZS):
        wrong = [z for k, z in enumerate(ZS) if sb[k]["Z"] != str(z)]
        check(not wrong, f"B: clicking element Z gives Z (wrong: {wrong[:5]})")
        wrongN = [z for k, z in enumerate(ZS) if sb[k]["N"] != sb[k]["usualN"]]
        check(not wrongN, f"B: the neutrons are the usual isotope's (wrong: {wrongN[:5]})")

    # ---- C: the steppers, the sliders, usual and the famous isotopes ----
    sc = ("6:click=top.chip;10:click=chip.z.inc;14:state;16:click=chip.z.dec;18:click=chip.z.dec;22:state;"
          "24:click=chip.n.inc;28:state;30:click=chip.n.dec;32:click=chip.n.dec;36:state;38:click=chip.usual;42:state;"
          "44:click=fam.2;48:state;50:click=chip.z.slider@0.5;54:state;56:click=chip.n.slider@0;60:state")
    rc = go("chip_c", sc, {"PL_ATOM_SET": "Z=6;N=6"}, frames=70)
    s = states(rc)
    check(not rc.scripted_missing(), f"C: every control was found {rc.scripted_missing()}")
    if len(s) == 8:
        check((s[0]["Z"], s[0]["N"]) == ("7", "7"), f"C: Protons + from carbon-12 gives nitrogen-14 (Z 7, N 7): {s[0]['Z']} {s[0]['N']}")
        check((s[1]["Z"], s[1]["N"]) == ("5", "6"), f"C: Protons - twice gives boron-11 (Z 5, N 6): {s[1]['Z']} {s[1]['N']}")
        check((s[2]["Z"], s[2]["N"]) == ("5", "7"), f"C: Neutrons + gives boron-12: {s[2]['N']}")
        check((s[3]["Z"], s[3]["N"]) == ("5", "5"), f"C: Neutrons - twice gives boron-10: {s[3]['N']}")
        check(s[4]["N"] == s[4]["usualN"] == "6", f"C: usual gives boron-11 again: {s[4]['N']}")
        check((s[5]["Z"], s[5]["N"]) == ("2", "1"), f"C: the famous list's Helium-3 sets 2 protons, 1 neutron: {s[5]['Z']} {s[5]['N']}")
        check(55 <= int(s[6]["Z"]) <= 63, f"C: the Protons slider in the middle gives about Z 60: {s[6]['Z']}")
        check(int(s[6]["N"]) == int(s[6]["usualN"]), "C: ... with that element's usual neutrons")
        check(int(s[7]["N"]) < int(s[6]["N"]) and int(s[7]["N"]) >= 0, f"C: the Neutrons slider at its left end gives fewer neutrons: {s[6]['N']} -> {s[7]['N']}")
    else:
        check(False, f"C: eight states ({len(s)})")
    # ---- D: Esc, a click outside and a second click on the chip close it (stage 5f: the dropdown steps aside from the Atom card, so it now covers the left of the picture; the outside click is at 770,600, between it and the Controls panel) ----
    for name, scr in (("esc", "10:click=top.chip;20:key=escape"), ("outside", "10:click=top.chip;20:clickxy=770,600"), ("again", "10:click=top.chip;20:click=top.chip")):
        rd = go("chip_d_" + name, scr, frames=34)
        check("panel.chip" not in rd.rects, f"D: {name} closes the dropdown")
    ro = go("chip_d_open", "10:click=top.chip", frames=34)
    check("panel.chip" in ro.rects, "D: (control) it is open without the closing action")

    # ---- E: the words, singular and plural ----
    for z, n, words in ((1, 0, "1 proton · 0 neutrons"), (1, 1, "1 proton · 1 neutron"), (2, 1, "2 protons · 1 neutron")):
        re_ = go(f"chip_e_{z}_{n}", "", {"PL_ATOM_SET": f"Z={z};N={n}"}, frames=20)
        check(words in [t for _, _, t in re_.texts], f"E: Z={z} N={n} says '{words}'")

    # ---- E2 (0.38.1): the element's tile in the chip writes its atomic number at the Caption size of docs/ui-0.15/DESIGN.md (13 u, floor 12 px), never smaller
    #      (0.38 drew it at 80 % of Caption, about 10 px, and outside the text log). Reference: DESIGN.md section 1, the type scale.
    for tsz in ("small", "normal"):
        rt = go(f"chip_tile_{tsz}", "", {"PL_ATOM_SET": "Z=92", "PL_TEXTSIZE": tsz}, frames=20)
        if rt.window:
            want = max(12, int(13 * rt.window[2] * rt.window[3] + 0.5))
            got = [(k, px) for k, px, t in rt.texts if t == "92"]
            check(("caption", float(want)) in got, f"E2: text size {tsz}: the tile's '92' is a Caption text of {want} px: {got}")
        else:
            check(False, f"E2: text size {tsz}: the app printed its window line")

    # ---- F: Build another and Reshuffle live in the Nucleus panel, in the nucleus view only ----
    rf = go("chip_f", "76:click=nucleus.reshuffle;82:state", {"PL_ATOM_SET": "Z=6;N=6;view=Orbit", "PL_ATOM_DO": "nucleus"}, frames=96)
    sf = states(rf)
    check(not rf.scripted_missing() and len(sf) == 1 and sf[0]["seed"] == "1", f"F: Reshuffle in the Nucleus panel reshuffles once {sf[0]['seed'] if sf else None} {rf.scripted_missing()}")
    rg = go("chip_g", "26:click=nucleus.reshuffle", {"PL_ATOM_SET": "Z=6;N=6;view=Orbit"}, frames=40)
    check(any("nucleus.reshuffle" in l for l in rg.scripted_missing()), "F: in the atom view the Reshuffle button is not offered")
    print(f"{checks} checks, {fails} failed")
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
