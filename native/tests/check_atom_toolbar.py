#!/usr/bin/env python3
# 0.15 stage 5a: the Atom Lab's toolbar buttons (New UI On) give the same lab state as the old controls (New UI Off): the View row's buttons and the bar under the picture.
#   check_atom_toolbar.py <PhysicsLab binary> <work folder>
# Both runs press real controls through the real input path (PL_SCRIPT) and print the lab's state (PLSTATE) after each step. Nothing is compared with an earlier output of ours.
# The picture is a different size in the two layouts, so a zoom NUMBER cannot be equal; what must agree is the level the picture reaches ("layer") and the views and angles.
#   expected (INVENTORY.md section 2, the words of the old controls): the five views Orbit / Cloud / Cloud 3D / Slice / Chart of nuclides, the levels Atom / Nucleus / Nucleon, Reset view
#   gives the start angles (Cloud 3D yaw 0.6, pitch 0.35; the chart: tilt 0, zoom 1), Drag toggles rotate <-> move.
# It also checks the toolbar's own layout (one row, inside the window, over nothing of the top bar), Space and the chip's play button, and that the old bar is gone in the New UI.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, inside, overlap

def parse(line):
    d = {}
    for tok in line.split():
        if "=" in tok:
            k, v = tok.split("=", 1)
            d[k.replace("_", " ")] = v
    return d

def states(r):
    out = []
    for ln in r.states:
        # the view name has spaces ("Cloud 3D"): split on the known keys instead
        keys = ["view", "layer", "zoom", "zoomTo", "focus", "drag", "paused", "yaw", "pitch", "oyaw", "opitch", "ctilt", "czoom"]
        d, pos = {}, []
        for k in keys:
            i = ln.find(k + "=")
            pos.append((i, k))
        pos.sort()
        for n, (i, k) in enumerate(pos):
            j = pos[n + 1][0] if n + 1 < len(pos) else len(ln)
            d[k] = ln[i + len(k) + 1:j].strip()
        out.append(d)
    return out

# steps: (frame, what). A level change needs the zoom to settle (80 frames); a view change a few.
OLD_A = ("10:click=Cloud;16:state;20:click=Cloud 3D;32:state;36:click=Slice;48:state;52:click=Chart of nuclides;58:state;62:click=Orbit;68:state;"
         "72:click=⌕ Nucleus;162:state;166:click=⌕ Nucleon;262:state;266:click=⌕ Atom;366:state")
NEW_A = ("10:click=tool.cloud;16:state;20:click=tool.cloud.caret;26:click=cloud.cloud3d;32:state;36:click=tool.cloud.caret;42:click=cloud.slice;48:state;52:click=tool.chart;58:state;62:click=tool.atom;68:state;"
         "72:click=tool.nucleus;162:state;166:click=tool.nucleus.caret;172:click=nucleus.nucleon;262:state;266:click=tool.atom;366:state")
OLD_B = "6:click=Cloud 3D;18:drag=300,300,420,360;28:state;32:click=↺ Reset view;42:state;46:click=✥ Drag: rotate;52:state;56:click=✥ Drag: move;62:state"
NEW_B = "6:click=tool.cloud.caret;12:click=cloud.cloud3d;18:drag=300,300,420,360;28:state;32:click=tool.reset;42:state;46:click=tool.drag;52:state;56:click=tool.drag;62:state"
OLD_C = "6:move=300,300;8:wheel=3;20:state;24:click=Reset tilt, zoom and position;34:state"
NEW_C = "6:move=500,400;8:wheel=3;20:state;24:click=tool.reset;34:state"
NEW_D = "10:click=top.play;18:state;22:key=space;30:state;34:click=top.play;42:state"

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
    base = {"PL_ATOM": "1"}
    def go(name, newui, script, extra=None, frames=376, size="1000x2800"):
        env = dict(base, PL_NEWUI=newui, PL_SCRIPT=script)
        env.update(extra or {})
        r = run_app(binary, work, name, env, frames=frames, size=size)
        check(not r.scripted_missing(), f"{name}: every scripted click found its control: {r.scripted_missing()}")
        return r
    # ---- A: the views and the levels ----
    carbon = {"PL_ATOM_SET": "Z=6;N=6"}      # carbon-12: a nucleus of 12 nucleons (hydrogen's is one nucleon: its "Nucleus level" is already the nucleon level)
    ro, rn = go("atom_old_a", "off", OLD_A, carbon), go("atom_new_a", "on", NEW_A, carbon, size="1000x700")
    so, sn = states(ro), states(rn)
    check(len(so) == 8 and len(sn) == 8, f"A: eight states each ({len(so)}, {len(sn)})")
    if len(so) == 8 and len(sn) == 8:
        want = ["Cloud", "Cloud 3D", "Slice", "Chart of nuclides", "Orbit"]
        for i, w in enumerate(want):
            check(so[i]["view"] == w, f"A old: step {i} view {w}: {so[i]['view']}")
            check(sn[i]["view"] == w, f"A new: step {i} view {w}: {sn[i]['view']}")
        check(so[5]["layer"] == "Nucleus" and sn[5]["layer"] == "Nucleus", f"A: Nucleus level: old {so[5]['layer']} new {sn[5]['layer']}")
        check(so[6]["layer"] in ("Nucleon", "Quarks") and sn[6]["layer"] == so[6]["layer"], f"A: Nucleon level: old {so[6]['layer']} new {sn[6]['layer']}")
        check(float(sn[5]["zoom"]) > 3 and float(so[5]["zoom"]) > 3, f"A: Nucleus is zoomed in: old {so[5]['zoom']} new {sn[5]['zoom']}")
        check(float(sn[6]["zoom"]) > float(sn[5]["zoom"]), f"A: Nucleon is further in than Nucleus: {sn[5]['zoom']} -> {sn[6]['zoom']}")
        check(int(sn[6]["focus"]) >= 0 and int(so[6]["focus"]) >= 0, "A: a nucleon is followed at the Nucleon level")
        check(so[7]["layer"] == "Atom" and sn[7]["layer"] == "Atom", f"A: the Atom button zooms out: old {so[7]['layer']} new {sn[7]['layer']}")
        check(float(sn[7]["zoom"]) == 0 and float(so[7]["zoom"]) == 0 and sn[7]["focus"] == "-1", f"A: Atom is zoom 0 with no nucleon: new {sn[7]['zoom']} {sn[7]['focus']}")
    # ---- B: Reset view and Drag in Cloud 3D ----
    rob, rnb = go("atom_old_b", "off", OLD_B, frames=72), go("atom_new_b", "on", NEW_B, frames=72, size="1000x700")
    sob, snb = states(rob), states(rnb)
    if len(sob) == 4 and len(snb) == 4:
        for name, s in (("old", sob), ("new", snb)):
            check(s[0]["view"] == "Cloud 3D", f"B {name}: Cloud 3D")
            check((float(s[0]["yaw"]), float(s[0]["pitch"])) != (0.6, 0.35), f"B {name}: the drag turned the cloud ({s[0]['yaw']}, {s[0]['pitch']})")
            check((float(s[1]["yaw"]), float(s[1]["pitch"])) == (0.6, 0.35), f"B {name}: Reset view gives yaw 0.6, pitch 0.35 ({s[1]['yaw']}, {s[1]['pitch']})")
            check(s[1]["drag"] == "rotate" and s[2]["drag"] == "move" and s[3]["drag"] == "rotate", f"B {name}: Drag toggles rotate, move, rotate ({s[1]['drag']}, {s[2]['drag']}, {s[3]['drag']})")
    else:
        check(False, f"B: four states each ({len(sob)}, {len(snb)})")
    # ---- C: the chart's Reset ----
    cenv = {"PL_ATOM_SET": "view=Chart of nuclides"}
    roc, rnc = go("atom_old_c", "off", OLD_C, cenv, frames=40), go("atom_new_c", "on", NEW_C, cenv, frames=40, size="1000x700")
    soc, snc = states(roc), states(rnc)
    if len(soc) == 2 and len(snc) == 2:
        for name, s in (("old", soc), ("new", snc)):
            check(s[0]["view"] == "Chart of nuclides" and float(s[0]["czoom"]) > 1, f"C {name}: the wheel zoomed the chart ({s[0]['czoom']})")
            check(float(s[1]["czoom"]) == 1 and float(s[1]["ctilt"]) == 0, f"C {name}: Reset gives zoom 1 and tilt 0 ({s[1]['czoom']}, {s[1]['ctilt']})")
    else:
        check(False, f"C: two states each ({len(soc)}, {len(snc)})")
    # ---- D: play / pause in the element chip, and Space ----
    rd = go("atom_new_d", "on", NEW_D, frames=50, size="1000x700")
    sd = states(rd)
    if len(sd) == 3:
        p = [s["paused"] for s in sd]
        check(p[0] != p[1] and p[1] != p[2], f"D: the chip's play button and Space both toggle pause: {p}")
        check("top.play" not in str(rd.rects), "D: (no toolbar play button)")
    else:
        check(False, f"D: three states ({len(sd)})")
    # ---- layout: the toolbar is one row inside the window, the picture fills the window, the old bar is gone ----
    rl = run_app(binary, work, "atom_layout", {"PL_ATOM": "1", "PL_NEWUI": "on"}, frames=20, size="1280x800")
    tb = rl.rects.get("toolbar")
    check(tb is not None and inside(tb, 1280, 800), f"layout: the toolbar is inside the window: {tb}")
    if tb:
        check(tb[3] <= 44, f"layout: the toolbar is one row ({tb[3]})")
        check(not overlap(tb, rl.rects["topbar"]), "layout: the toolbar does not touch the top bar")
        check(abs(tb[0] + tb[2] / 2 - 640) <= 2, f"layout: the toolbar is centred ({tb[0] + tb[2] / 2})")
    check(not any(k.startswith("panel.old") for k in rl.rects), "layout: the Old panel starts closed")
    print(f"{checks} checks, {fails} failed")
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
