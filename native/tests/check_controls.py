#!/usr/bin/env python3
# 0.15 stage 5c: the Controls panel (one docked panel on the right, both labs) and the two bug fixes (ring labels under the bars; the readout) in the real app.
#   check_controls.py <PhysicsLab binary> <work folder>
# References (none is an earlier output of ours): the owner's decision of 6 Oct 2026 (DECISIONS.md: all simulation options in ONE docked panel on the right, about 22 em wide, under the top bar,
# the bottom toolbar and the readout left of it; the toolbar keeps one-click buttons only), DESIGN.md (Body 15 u, 14 px floor), plain geometry, and the WCAG 2.x contrast formula written out
# here. That each moved control changes the same state as the old one is checked by check_orbit_ui.py / check_shell_state.py (the old side panel as the reference).
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, inside, overlap, Bmp, prefetch

def lum(c):
    f = lambda v: v / 12.92 if v <= 0.03928 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = (f(x / 255.0) for x in c)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b

def wcag(a, b):
    x, y = lum(a), lum(b)
    return (max(x, y) + 0.05) / (min(x, y) + 0.05)

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
    NEW = {"PL_NEWUI": "on"}
    def go(name, script, extra=None, frames=60, size="1280x800", theme="dark", atom=False):
        env = dict(NEW, PL_SCRIPT=script, **(extra or {}))
        if atom:
            env["PL_ATOM"] = "1"
        return run_app(binary, work, name, env, frames=frames, size=size, theme=theme)
    FAR = "10:click=scale.zoom@0.0"
    UP = "30:drag=640,600,640,200"            # moves the picture up: the Sun goes to the window's top edge, so ring labels fall into the top bar
    prefetch([((binary, work, f"c_geo_{sz}", dict(NEW, PL_SELECT="Earth")), dict(frames=80, size=sz)) for sz in ("1280x800", "880x560")] +
             [((binary, work, f"c_ring_{sz}", dict(NEW, PL_SCRIPT=FAR + ";" + UP)), dict(frames=90, size=sz)) for sz in ("1280x800", "880x560", "1920x1080")] +
             [((binary, work, f"c_ro_{t}", dict(NEW)), dict(frames=40, size="1280x800", theme=t)) for t in ("dark", "light", "aero", "black", "paper")])

    # ---- 1. the panel's place (Orbit Lab), at two window sizes: inside the window, under the top bar, about 22 em wide, clear of the toolbar and the readout ----
    for sz in ("1280x800", "880x560"):
        r = run_app(binary, work, f"c_geo_{sz}", dict(NEW, PL_SELECT="Earth"), frames=80, size=sz)
        w, h = r.window[0], r.window[1]
        pc, tb, ro, top = r.rects.get("panel.controls"), r.rects.get("toolbar"), r.rects.get("readout"), r.rects.get("topbar")
        check(pc is not None, f"{sz}: the Controls panel is open by default")
        if pc is None or tb is None or ro is None:
            continue
        check(inside(pc, w, h), f"{sz}: the panel is inside the window {pc}")
        check(pc[1] >= top[3] and not overlap(pc, top), f"{sz}: below the top bar {pc}")
        check(not overlap(pc, tb), f"{sz}: the toolbar is clear of it")
        check(not overlap(pc, ro), f"{sz}: the readout is clear of it")
        check(abs(pc[2] - 22 * 15) <= 1.5 or pc[2] <= 0.45 * w + 1, f"{sz}: about 22 em wide (330 px at 100 %): {pc[2]}")
        check(pc[1] + pc[3] >= h - 13 and pc[1] + pc[3] <= h - 11, f"{sz}: full height, 12 u above the window's bottom edge {pc}")
        occ = [v for k, v in r.rects.items() if k.startswith("occ")]
        check(any(all(abs(a - b) < 1 for a, b in zip(o, pc)) for o in occ), f"{sz}: it is one of the occluders the centre shift keeps the selected body out from under")
        b = r.note("body")
        if b:
            _, x, y, rad = b[0].split()
            x, y, rad = float(x), float(y), float(rad)
            check(x + rad <= pc[0] + 1 or y < pc[1] or y > pc[1] + pc[3], f"{sz}: the selected Earth is not under the panel (x {x:.0f} r {rad:.0f}, panel from {pc[0]:.0f})")
        nm = [n for n in r.note("panelneed controls")]
        check(len(nm) == 1, f"{sz}: the panel reports its content height")
        check(any(k == "readout" and abs(px - 15) < 0.01 for k, px, _ in r.texts), f"{sz}: the readout text is Body size (15 px at 100 %): {[(k, px) for k, px, _ in r.texts if k == 'readout']}")

    # ---- 2. the toolbar: one-click buttons only, a Controls button opens and closes the panel; no carets left ----
    r = go("c_tb", "10:click=tool.show;12:click=tool.pick;14:click=tool.scale;16:click=tool.speed;18:click=tool.view.caret;20:click=tool.view.3d;26:click=tool.reset;34:click=tool.controls;50:click=tool.controls", frames=40)
    miss = r.scripted_missing()
    check(len(miss) == 5, f"the toolbar no longer has Show / Pick / Scale / Time speed / view carets ({len(miss)} missing): {miss}")
    for nm in ("tool.view.3d", "tool.reset", "tool.controls"):
        check(nm not in " ".join(miss), f"{nm} is on the toolbar")
    rc = go("c_tb_closed", "10:click=tool.controls", frames=40)
    check("panel.controls" not in rc.rects, "the Controls button closes the panel")
    rc = go("c_tb_reopen", "10:click=tool.controls;24:click=tool.controls", frames=40)
    check("panel.controls" in rc.rects, "and a second click opens it again")
    rc = go("c_x", "10:click=ctl.close", frames=40)
    check("panel.controls" not in rc.rects and not rc.scripted_missing(), "the panel's own ✕ closes it")
    ro_open = run_app(binary, work, "c_geo_1280x800", dict(NEW, PL_SELECT="Earth"), frames=80)
    tb_o, tb_c = ro_open.rects["toolbar"], rc.rects["toolbar"]
    check(tb_c[0] + tb_c[2] / 2 > tb_o[0] + tb_o[2] / 2 + 100, f"with the panel closed the toolbar is centred in the whole window (centre {tb_c[0] + tb_c[2] / 2:.0f} vs {tb_o[0] + tb_o[2] / 2:.0f} with it open)")
    check(tb_o[0] + tb_o[2] / 2 < ro_open.rects["panel.controls"][0] / 2 + 2 and abs(tb_o[0] + tb_o[2] / 2 - ro_open.rects["panel.controls"][0] / 2) < 2, "with it open the toolbar is centred left of the panel")
    # the closed state is kept like the cards' state (the settings file): a second run in the same home keeps it
    # (not tested here: every start uses a fresh home)

    # ---- 3. every moved control is reachable in the panel (ids are the old panels' ids), at a big and at the smallest window ----
    SHOW = "show.moons;show.dwarfs;show.comets;show.belts;show.far;show.sky@0.5;show.cons@0.5;show.physics@0.9;show.gr;show.wobble;show.notes".split(";")
    for sz in ("1280x800", "880x560"):
        cmds = ["speed.slider@0.5"] + SHOW + ["scale.zoom@0.5", "scale.sizes@0.9", "scale.trails@0.9", "tool.view.3d", "view.pan", "view.spin", "view.follow", "view.reset", "ctl.sec.pick", "##pickfilter", "text=Titan", "pick.Titan"]
        script = ";".join(f"{10 + 8 * i}:click={c}" if not c.startswith("text=") else f"{10 + 8 * i}:{c}" for i, c in enumerate(cmds)) + f";{10 + 8 * len(cmds) + 6}:state"
        r = go(f"c_reach_{sz}", script, {"PL_SELECT": "Earth"}, frames=10 + 8 * len(cmds) + 14, size=sz)
        check(not r.scripted_missing(), f"{sz}: all {len(cmds)} moved controls were found and clicked: {r.scripted_missing()[:4]}")
        st = r.states[-1] if r.states else ""
        check("sel=" in st and "sel=0," not in st, f"{sz}: Pick a body: Titan was picked in the panel ({st[:80]})")
    # folding: a folded section's controls are not drawn, the others stay
    def need(r):
        n = r.note("panelneed controls")
        return float(n[0].split("need=")[1].split()[0]) if n else -1
    r0 = go("c_nofold", "", frames=30)
    r = go("c_fold", "10:click=ctl.sec.show", frames=40)
    check(need(r0) > 0 and need(r) > 0 and need(r) < need(r0) - 150, f"folding the Show section makes the panel's content shorter ({need(r0):.0f} -> {need(r):.0f} px)")
    r = go("c_fold2", "10:click=ctl.sec.show;20:click=ctl.sec.show", frames=40)
    check(abs(need(r) - need(r0)) < 1, f"and unfolding brings it back ({need(r):.0f} vs {need(r0):.0f})")
    # Esc does not close the docked panel; Ctrl+K search can still open a section (Time speed etc.)
    r = go("c_esc", "10:key=escape", frames=30)
    check("panel.controls" in r.rects, "Esc leaves the docked panel open")
    r = go("c_search", "10:key=ctrl+k;16:text=Famous comets;24:key=enter;40:click=show.comets", frames=60)
    check(not r.scripted_missing() and "panel.controls" in r.rects, f"search still finds a moved control and the panel stays open: {r.scripted_missing()[:2]}")

    # ---- 4. bug fix A: no ring label under the top bar, the toolbar or the panel (a far zoom-out with the Sun moved to the top edge) ----
    for sz in ("1280x800", "880x560", "1920x1080"):
        r = run_app(binary, work, f"c_ring_{sz}", dict(NEW, PL_SCRIPT=FAR + ";" + UP), frames=90, size=sz)
        drawn = {k: v for k, v in r.rects.items() if k.startswith("ringlabel.")}
        skipped = {k: v for k, v in r.rects.items() if k.startswith("ringskip.")}
        top, tb, pc = r.rects["topbar"], r.rects["toolbar"], r.rects.get("panel.controls")
        if sz != "880x560":       # (the smallest window has little room left of the panel and above the toolbar: all its labels may be skipped; the rings stay)
            check(len(drawn) > 0, f"{sz}: some ring labels are drawn ({len(drawn)})")
        check(len(skipped) > 0, f"{sz}: and some are skipped (the case: the Sun at the top edge puts labels into the bars) ({len(skipped)})")
        check(all(not overlap(v, top) for v in drawn.values()), f"{sz}: no drawn ring label touches the top bar: {[k for k, v in drawn.items() if overlap(v, top)]}")
        check(all(not overlap(v, tb) for v in drawn.values()), f"{sz}: none touches the toolbar")
        check(pc is not None and all(not overlap(v, pc) for v in drawn.values()), f"{sz}: none is under the Controls panel")
        check(any(overlap(v, top) or (pc and overlap(v, pc)) or overlap(v, tb) or v[1] < 0 for v in skipped.values()), f"{sz}: the skipped ones really were in a covered band")

    # ---- 5. bug fix B: the readout is Body size on a dark pill; the measured contrast of its text on its pill, per theme ----
    SKY_OFF = "10:click=show.sky@0.95"              # a plain light background behind the pill in the light themes: the hard case
    for theme, sky in [(t, k) for t in ("dark", "light", "aero", "black", "paper") for k in ("space", "plain")]:
        r = run_app(binary, work, f"c_ro_{theme}" if sky == "space" else f"c_ro_{theme}_plain", dict(NEW) if sky == "space" else dict(NEW, PL_SCRIPT=SKY_OFF), frames=40, size="1280x800", theme=theme)
        ro = r.rects["readout"]
        b = Bmp(r.bmp)
        x0, y0, x1, y1 = int(ro[0]), int(ro[1]), int(ro[0] + ro[2]), int(ro[1] + ro[3])
        pill = b.px(x0 + 3, y0 + int(ro[3] / 2))          # the pill's own colour at its left padding (no text there)
        best = max(((wcag(b.px(x, y), pill), b.px(x, y)) for y in range(y0 + 4, y1 - 4) for x in range(x0 + 8, x1 - 8)), key=lambda t: t[0])
        print(f"READOUT {theme} on {sky}: pill {pill} text {best[1]} contrast {best[0]:.2f}")
        check(best[0] >= 4.5, f"{theme} on {sky}: the readout text reads 4.5:1 on its pill, measured from the picture: {best[0]:.2f}")
        check(lum(pill) < 0.3, f"{theme} on {sky}: the pill is darker than the picture it sits on ({pill})")

    # ---- 6. the Atom Lab: the View section, no carets, the Old panel left of the panel ----
    r = go("c_atom", "10:click=tool.nucleus.caret;12:click=tool.cloud.caret;16:click=nucleus.nucleon;30:click=cloud.cloud3d;40:click=cloud.slice;50:click=tool.old", frames=60, atom=True)
    miss = " ".join(r.scripted_missing())
    check("tool.nucleus.caret" in miss and "tool.cloud.caret" in miss, "Atom: the Nucleus and Cloud buttons have no caret any more")
    check("nucleus.nucleon" not in miss and "cloud.cloud3d" not in miss and "cloud.slice" not in miss, f"Atom: Nucleon, Cloud 3D and Slice are in the View section: {miss}")
    pc = r.rects.get("panel.controls")
    check(pc is not None and inside(pc, r.window[0], r.window[1]) and pc[1] >= r.rects["topbar"][3], f"Atom: the panel is inside the window, under the top bar {pc}")
    check(not overlap(pc, r.rects["toolbar"]) and not overlap(pc, r.rects["readout"]), "Atom: the toolbar and the readout are clear of it")
    r = go("c_atom_old", "10:click=top.old", frames=40, atom=True)
    po, pc = r.rects.get("panel.old"), r.rects.get("panel.controls")
    check(po is not None and pc is not None and po[0] + po[2] <= pc[0] + 0.5, f"Atom: the Old panel sits left of the Controls panel {po} {pc}")
    r = go("c_atom_tb", "", frames=30, atom=True)
    check("tool.controls" in " ".join(r.notes) or "toolbar" in r.rects, "Atom: the toolbar is drawn")
    print(f"controls panel checks: {checks - fails} of {checks} passed")
    sys.exit(1 if fails else 0)

main()
