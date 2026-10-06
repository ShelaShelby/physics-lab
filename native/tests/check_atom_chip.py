#!/usr/bin/env python3
# 0.15 stage 5b: the Atom Lab's element chip and its dropdown (New UI On) in the real app.
#   check_atom_chip.py <PhysicsLab binary> <work folder>
# References (nothing here is an earlier output of ours): the standard periodic table's places (H in column 1, He in 18, Fe in row 4 column 8, B in row 2 column 13; the lanthanides and
# actinides in two rows below), the spec's words ("1 proton", "0 neutrons"), the element names written in the famous list's own labels, and the app's own old setParam rule that a new element
# gets its usual isotope (the unit test in atom_tests holds the textbook mass numbers). All 118 elements are clicked in the real app.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, inside, overlap

KEYS = ["view", "layer", "zoom", "zoomTo", "focus", "drag", "paused", "yaw", "pitch", "oyaw", "opitch", "ctilt", "czoom", "Z", "N", "usualN", "seed", "big"]

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

# standard periodic table: (Z, row, column) for a few; lanthanide row 9, actinide row 10 in the app's own numbering
PLACES = [(1, 1, 1), (2, 1, 18), (5, 2, 13), (6, 2, 14), (11, 3, 1), (26, 4, 8), (36, 4, 18), (57, 9, 3), (58, 9, 4), (71, 9, 17), (89, 10, 3), (91, 10, 5), (103, 10, 17), (118, 7, 18)]   # (Z, row, column); rows 9 and 10: lanthanides La-Lu and actinides Ac-Lr, 15 each, under group 3 to group 17 (column 2 holds the * and ** marks)

def main():
    binary, work = sys.argv[1], sys.argv[2]
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
    check("U · Uranium-238" in chip_texts and "92 protons · 146 neutrons" in chip_texts, f"A: the chip says U · Uranium-238 / 92 protons · 146 neutrons")

    # ---- B: every one of the 118 elements can be reached by a click ----
    step = 5
    script = ";".join(f"{10 + step * (z - 1)}:click=pt.{z};{14 + step * (z - 1)}:state" for z in range(1, 119))
    script = "6:click=top.chip;" + script
    rb = go("chip_b", script, frames=10 + step * 119 + 10)
    sb = states(rb)
    check(not rb.scripted_missing(), f"B: every scripted click found its cell {rb.scripted_missing()[:3]}")
    check(len(sb) == 118, f"B: 118 states ({len(sb)})")
    if len(sb) == 118:
        wrong = [z + 1 for z in range(118) if sb[z]["Z"] != str(z + 1)]
        check(not wrong, f"B: clicking element Z gives Z (wrong: {wrong[:5]})")
        wrongN = [z + 1 for z in range(118) if sb[z]["N"] != sb[z]["usualN"]]
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
    # ---- D: Esc, a click outside and a second click on the chip close it ----
    for name, scr in (("esc", "10:click=top.chip;20:key=escape"), ("outside", "10:click=top.chip;20:clickxy=100,640"), ("again", "10:click=top.chip;20:click=top.chip")):
        rd = go("chip_d_" + name, scr, frames=34)
        check("panel.chip" not in rd.rects, f"D: {name} closes the dropdown")
    ro = go("chip_d_open", "10:click=top.chip", frames=34)
    check("panel.chip" in ro.rects, "D: (control) it is open without the closing action")

    # ---- E: the words, singular and plural ----
    for z, n, words in ((1, 0, "1 proton · 0 neutrons"), (1, 1, "1 proton · 1 neutron"), (2, 1, "2 protons · 1 neutron")):
        re_ = go(f"chip_e_{z}_{n}", "", {"PL_ATOM_SET": f"Z={z};N={n}"}, frames=20)
        check(words in [t for _, _, t in re_.texts], f"E: Z={z} N={n} says '{words}'")

    # ---- F: Build another and Reshuffle live in the Nucleus panel, in the nucleus view only; no longer in the Old panel ----
    rf = go("chip_f", "70:click=tool.nucleus.caret;76:click=nucleus.reshuffle;82:state", {"PL_ATOM_SET": "Z=6;N=6", "PL_ATOM_DO": "nucleus"}, frames=96)
    sf = states(rf)
    check(not rf.scripted_missing() and len(sf) == 1 and sf[0]["seed"] == "1", f"F: Reshuffle in the Nucleus panel reshuffles once {sf[0]['seed'] if sf else None} {rf.scripted_missing()}")
    rg = go("chip_g", "20:click=tool.nucleus.caret;26:click=nucleus.reshuffle", {"PL_ATOM_SET": "Z=6;N=6"}, frames=40)
    check(any("nucleus.reshuffle" in l for l in rg.scripted_missing()), "F: in the atom view the Reshuffle button is not offered")
    rh = go("chip_h", "10:click=top.old;20:click=Reshuffle nucleons", {"PL_ATOM_SET": "Z=6;N=6", "PL_ATOM_DO": "nucleus"}, frames=40)
    check(any("Reshuffle nucleons" in l for l in rh.scripted_missing()), "F: the Old panel no longer has Reshuffle (New UI On)")
    roff = run_app(binary, work, "chip_off", {"PL_ATOM": "1", "PL_NEWUI": "off", "PL_ATOM_SET": "Z=6;N=6", "PL_ATOM_DO": "nucleus", "PL_SCRIPT": "40:click=Reshuffle nucleons;50:state"}, frames=60, size="1000x2800")
    soff = states(roff)
    check(not roff.scripted_missing() and soff and soff[0]["seed"] == "1", f"F: with New UI Off the old panel still has Reshuffle: {roff.scripted_missing()}")

    # ---- G: Bigger table still works (the old panel's button and the large window) ----
    rbg = go("chip_big", "10:click=top.old;20:click=⤢ Bigger table;30:state", {"PL_ATOM_SET": "Z=6;N=6"}, frames=40)
    sbg = states(rbg)
    check(not rbg.scripted_missing() and sbg and sbg[0]["big"] == "1", f"G: Bigger table opens the large table window {rbg.scripted_missing()}")
    print(f"{checks} checks, {fails} failed")
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
