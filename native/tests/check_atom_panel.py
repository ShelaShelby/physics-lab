#!/usr/bin/env python3
# 0.15 stage 5d: the Atom Lab's view options in the Controls panel (New UI On), and the fact line's two lines.
#   check_atom_panel.py <PhysicsLab binary> <work folder> [part]      part: sections | facts | marks | photons | place (0.47) | levels (0.47.3) | field (0.54.0) | vdots (0.54.1) | tidy | all (default)
# References (none is an earlier output of ours): the OLD side panel of the same build (New UI Off) is the reference for every control: both runs press the real control through the
# real input path (PL_SCRIPT) and print every Atom setting (PLASTATE); the new control must give the old control's value. Which sections show in which view: the old panel's own rules
# (atom_panel.cpp: Orbital choice and orientation in the cloud views but not the chart, n and Playback speed in the Orbit view, cloud style / density / superposition when the view is
# not Orbit, Slice only in Slice, Orbit style only in Orbit, scale / zoom / spin not on the chart, quark style and nucleon detail from zoom 4, nucleon edge from zoom 3, tilt and height
# only on the chart). The owner's brief for 5d: the order View, Orbital, Cloud, Orbit, Scale and motion, Nucleus, Chart; text >= 14 px; three window sizes. Fact line: at most two lines,
# "…" only when two are too short, clear of the toolbar, readout, panel, handle and top bar, with the longest fact of the app's list (PL_FACT_LONGEST), in both labs.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, inside, overlap, prefetch, old_run

SIZES = ("1280x800", "880x560")   # 0.38.1 (the owner: "cut what is not needed"): the smallest window and the usual laptop size; 1920x1080 added run time and found nothing the other two missed
OLD_SIZE = "1000x2800"           # the old side column shows everything without scrolling at this height (as in check_atom_toolbar.py)

def astates(r):
    out = []
    for ln in r.out.splitlines():
        if ln.startswith("PLASTATE "):
            d = {}
            for kv in ln[len("PLASTATE "):].split("\t"):
                if "=" in kv:
                    k, v = kv.split("=", 1)
                    d[k] = v
            out.append(d)
    return out

def script(steps, start=10, gap=8):
    # steps: list of "click=..." / "state" / ("wait", n). Returns (script, frames)
    parts, f = [], start
    for s in steps:
        if isinstance(s, tuple):
            f += s[1]
            continue
        parts.append(f"{f}:{s}")
        f += gap
    return ";".join(parts), f + 10

# (new control, old control, key, kind): kind "s" = the same string; a number = the largest allowed difference (sliders: the two sliders are not the same width, so a click at
# the same fraction can land a little apart; the fractions are picked in the middle of a step for the whole-number sliders, so those must agree exactly)
ORBIT = [("atom.ostyle=Bohr 1913", "old.ostyle=Bohr 1913", "ostyle", "s"),
         ("atom.n@0.667", "old.n@0.667", "n", ("is", "4")),   # (0.46: the slider is n = 1..6, was 1..4: two thirds along is 1 + 0.667 x 5 = 4.3 -> 4; the old click is kept only so the recording lines up)
         ("atom.speed@0.483", "old.speed@0.483", "speed", 0),
         ("atom.scale=Fixed", "old.scale=Fixed", "scale", "s"),
         ("atom.zoom@0.62", "old.zoom@0.62", "zoom", 0.15),
         ("atom.style=Nucleons", "old.style=Hard balls", "style", "s"),        # (0.43: Fuzzy and Hard balls became Nucleons; the old value is read through MIGRATE)
         ("atom.qstyle=Simple", "old.qstyle=Accurate", "qstyle", ("is", "Simple")),   # (0.43: Accurate is the default now, Classic is Simple; the old click is kept only so the recording lines up)
         ("atom.detail=+ Sea", "old.detail=+ Sea", "detail", "s")]
CLOUD = [("atom.orb=4f", "old.orb=4f", "orb", "s"),
         ("atom.fAx=xyz", "old.fAx=xyz", "fAx", "s"),
         ("atom.orb=2p", "old.orb=2p", "orb", "s"),
         ("atom.pAx=x", "old.pAx=x", "pAx", "s"),
         ("atom.cstyle=Classic", "old.cstyle=Classic", "cstyle", "s"),
         ("atom.cdens=Ultra", "old.cdens=Ultra", "cdens", "s"),
         ("atom.super=On", "old.super=On", "super", "s"),
         ("atom.orb2=3d", "old.orb2=3d", "orb2", "s"),
         ("atom.dAx2=xy", "old.dAx2=xy", "dAx2", "s"),
         ("atom.mix@0.3", "old.mix@0.3", "mix", 2),
         ("atom.speed@0.483", "old.speed@0.483", "speed", 0)]
SLICE = [("atom.cutAxis=Horizontal", "old.cutAxis=Horizontal", "cutAxis", "s"),
         ("atom.cut@0.75", "old.cut@0.75", "cut", 0.05)]
CLOUD3D = [("atom.spin", "⟳ Spin on/off", "spin", "s")]
CHART = [("atom.chart3d", "old.tilt@0.5", "tilt", ("is", 1.0)),   # 0.40: the 2D | 3D switch (was a Tilt slider): 3D is exactly tilt 1; the old (recorded) slider click is kept only so the recording lines up
         ("atom.cheight=Binding energy per nucleon", "old.cheight=Binding energy per nucleon", "cheight", "s")]
# the view each group needs: (new UI click, old click)
VIEWS = {"orbit": (None, None), "cloud": ("tool.cloud", "old.view=Cloud"), "slice": ("cloud.slice", "old.view=Slice"), "cloud3d": ("cloud.cloud3d", "old.view=Cloud 3D"),
         "chart": ("tool.chart", "old.view=Chart of nuclides")}
GROUPS = (("orbit", ORBIT), ("cloud", CLOUD), ("slice", SLICE), ("cloud3d", CLOUD3D), ("chart", CHART))
DEFAULTS = {}
# 0.43 (the owner, 8 Nov 2026): the old style names map to the new ones as the app migrates saved settings (AtomLab::migrateChoice)
MIGRATE = {("style", "Fuzzy"): "Nucleons", ("style", "Hard balls"): "Nucleons", ("qstyle", "Cinematic"): "Accurate", ("qstyle", "Classic"): "Simple"}

def group_script(view, items, new):
    steps = []
    v = VIEWS[view][0 if new else 1]
    if v:
        steps += ["click=" + v, ("wait", 12)]
    steps.append("astate")
    for nc, oc, key, kind in items:
        steps.append("click=" + (nc if new else oc))
        steps.append(("wait", 6))
        steps.append("astate")
    return script(steps)

def main():
    binary, work = sys.argv[1], sys.argv[2]
    part = sys.argv[3] if len(sys.argv) > 3 else "all"
    os.makedirs(work, exist_ok=True)
    fails = checks = 0
    def check(ok, what):
        nonlocal fails, checks
        checks += 1
        if not ok:
            fails += 1
            print("FAIL", what)
    # (stage 5g: the Orbit view is named in both runs: the New UI now starts in Cloud 3D, the old app in Orbit, and these tests start from the same view)
    NEW = {"PL_NEWUI": "on", "PL_ATOM": "1", "PL_ATOM_SET": "view=Orbit"}
    OLD = {"PL_NEWUI": "off", "PL_ATOM": "1", "PL_ATOM_SET": "view=Orbit"}

    if part in ("sections", "all"):
        calls = []
        for view, items in GROUPS:
            s_old, f_old = group_script(view, items, False)
            s_new, f_new = group_script(view, items, True)
            for sz in SIZES:
                calls.append(((binary, work, f"new_{view}_{sz}", dict(NEW, PL_SCRIPT=s_new)), dict(frames=f_new, size=sz)))
        prefetch(calls)
        # ---- 1. every control changes the same setting as the old one, to the same value, at three window sizes ----
        for view, items in GROUPS:
            s_old, f_old = group_script(view, items, False)
            ro = old_run("atom_panel", f"old_{view}", dict(OLD, PL_SCRIPT=s_old), frames=f_old, size=OLD_SIZE)
            so = astates(ro)
            check(not ro.scripted_missing() and len(so) == len(items) + 1, f"old {view}: every old control was found and clicked: {ro.scripted_missing()[:3]} ({len(so)} states)")
            s_new, f_new = group_script(view, items, True)
            for sz in SIZES:
                rn = run_app(binary, work, f"new_{view}_{sz}", dict(NEW, PL_SCRIPT=s_new), frames=f_new, size=sz)
                sn = astates(rn)
                check(not rn.scripted_missing() and len(sn) == len(items) + 1, f"{sz} {view}: every new control was found and clicked: {rn.scripted_missing()[:3]} ({len(sn)} states)")
                if len(sn) != len(items) + 1 or len(so) != len(items) + 1:
                    continue
                for i, (nc, oc, key, kind) in enumerate(items):
                    before_n, after_n, after_o = sn[i][key], sn[i + 1][key], so[i + 1][key]
                    check(after_n != before_n, f"{sz} {view}: {nc} changed {key} ({before_n} -> {after_n})")
                    if isinstance(kind, tuple):                    # (0.40: a control with no old twin: its own value)
                        check(after_n == kind[1] if isinstance(kind[1], str) else abs(float(after_n) - kind[1]) < 1e-9, f"{sz} {view}: {nc} gives {key}={after_n} (want {kind[1]})")
                        continue
                    if kind == "s":
                        after_o = MIGRATE.get((key, after_o), after_o)
                        check(after_n == after_o, f"{sz} {view}: {nc} gives {key}={after_n!r}, the old {oc} gives {after_o!r}")
                    else:
                        check(abs(float(after_n) - float(after_o)) <= kind, f"{sz} {view}: {nc} gives {key}={after_n}, the old {oc} gives {after_o} (allowed {kind})")
                    # nothing else changed by the click (the same as the old control, which may also reset dependent state: compare the whole line)
                    diff_n = {k for k in sn[i + 1] if sn[i + 1][k] != sn[i][k]}
                    diff_o = {k for k in so[i + 1] if so[i + 1][k] != so[i][k]}
                    check(diff_n == diff_o, f"{sz} {view}: {nc} changed the same settings as the old control: new {sorted(diff_n)} old {sorted(diff_o)}")
        # ---- 2. which sections show in which view (the old panel's rules), in the brief's order; each folds ----
        # (stage 5e added Marks: the old panel's outline and phase rows when the view is not Orbit, its chain block on the chart; and Photons: emit / absorb / auto off the chart,
        # Check a jump always, as the old Lifetimes card)
        ORDER = ["view", "orbital", "cloud", "orbit", "scale", "nucleus", "chart", "marks", "photons", "field"]   # 0.54.0: Magnetic field, last, off the chart
        # 0.16 stage 2: the Nucleus section is always shown off the chart, at any zoom (it used to need zoom 3)
        WANT = {"orbit": {"view", "orbital", "orbit", "scale", "nucleus", "photons", "field"}, "cloud": {"view", "orbital", "cloud", "scale", "nucleus", "marks", "photons", "field"}, "chart": {"view", "chart", "marks", "photons"},   # (stage 5f: the Cloud section is left out on the chart: its rows do nothing there; the old panel still shows them)
                "nucleus": {"view", "orbital", "orbit", "scale", "nucleus", "photons", "field"}}
        SCR = {"orbit": "", "cloud": "10:click=tool.cloud", "chart": "10:click=tool.chart", "nucleus": "10:click=atom.zoom@0.62"}
        for name, want in WANT.items():
            r = run_app(binary, work, f"sec_{name}", dict(NEW, PL_SCRIPT=SCR[name]), frames=40)
            n = r.note("ctlsections")
            got = n[0].split()[1:] if n else []
            check(set(got) == want, f"{name}: the sections shown are {got}, the old panel's rules give {sorted(want)}")
            check(got == [k for k in ORDER if k in got], f"{name}: in the order {ORDER}: {got}")
        def need(r):
            n = r.note("panelneed controls")
            return float(n[0].split("need=")[1].split()[0]) if n else -1
        for key in ("orbital", "cloud", "scale"):
            r0 = run_app(binary, work, f"fold0_{key}", dict(NEW, PL_SCRIPT="10:click=tool.cloud"), frames=40)
            r1 = run_app(binary, work, f"fold1_{key}", dict(NEW, PL_SCRIPT=f"10:click=tool.cloud;24:click=ctl.sec.{key}"), frames=50)
            check(not r1.scripted_missing() and 0 < need(r1) < need(r0) - 40, f"folding {key} makes the panel shorter ({need(r0):.0f} -> {need(r1):.0f})")
        # ---- 3. the 5c.1 panel rules in the Cloud view (the most sections): one header edge and width, text >= 14 px ----
        for sz in SIZES:
            r = run_app(binary, work, f"rules_{sz}", dict(NEW, PL_SCRIPT="10:click=tool.cloud"), frames=40, size=sz)
            heads = [v for k, v in r.rects.items() if k.startswith("ctlhead.")]
            pc = r.rects.get("panel.controls")
            check(len(heads) >= 2 and all(abs(v[0] - heads[0][0]) < 0.01 and abs(v[2] - heads[0][2]) < 0.01 for v in heads), f"{sz}: every header in view has one left edge and width {heads[:3]}")
            check(pc is not None and inside(pc, r.window[0], r.window[1]), f"{sz}: the panel is inside the window {pc}")
            small = [(px, t) for k, px, t in r.texts if k == "ctl" and px < 14 - 0.01]
            check(any(k == "ctl" for k, _, _ in r.texts) and not small, f"{sz}: all panel text is at least 14 px: {small[:3]}")

    if part in ("marks", "all"):
        # ---- 5. (stage 5e) the marks: the same settings as the old rows, and the picture draws the same thing (in Bohr radii and counts, so the two picture sizes do not matter) ----
        DRAWN = ("drawnOutline", "drawnOrb", "drawnMass", "drawnReach", "drawnSegs", "drawnGroups", "drawnWheel", "drawnChainNodes", "drawnChainEdges")
        MARKS = [("atom.outline=On", "old.outline=On", "outline", "s"), ("atom.phase=Wheel", "old.phase=Wheel", "phase", "s")]
        CHAIN = [("atom.walk", "old.walk", "walkSteps", "s"), ("atom.walkauto", "old.walkauto", "walkAuto", "s"), ("atom.walkauto", "old.walkauto", "walkAuto", "s"),
                 ("atom.walkrestart", "old.walkrestart", "walkSteps", "s"), ("atom.chain", "old.chain", "chain", "s")]
        U238 = {"PL_ATOM_SET": "Z=92;N=146;view=Chart of nuclides"}
        runs = []
        for view, items, extra in (("cloud", MARKS, {}), ("slice", MARKS, {}), ("chart", CHAIN, U238)):
            steps_n, steps_o = [], []
            for new, steps in ((True, steps_n), (False, steps_o)):
                v = VIEWS[view][0 if new else 1] if view != "chart" else None
                if v:
                    steps += ["click=" + v, ("wait", 12)]
                steps.append("astate")
                for nc, oc, key, kind in items:
                    steps += ["click=" + (nc if new else oc), ("wait", 30), "astate"]
            runs.append((view, items, extra, script(steps_n), script(steps_o)))
        prefetch([((binary, work, f"marks_new_{v}_{sz}", dict(NEW, PL_SCRIPT=sn[0], **ex)), dict(frames=sn[1], size=sz)) for v, it, ex, sn, so in runs for sz in SIZES])
        for view, items, extra, sn_, so_ in runs:
            so = astates(old_run("atom_panel", f"marks_old_{view}", dict(OLD, PL_SCRIPT=so_[0], **extra), frames=so_[1], size=OLD_SIZE))
            for sz in SIZES:
                rn = run_app(binary, work, f"marks_new_{view}_{sz}", dict(NEW, PL_SCRIPT=sn_[0], **extra), frames=sn_[1], size=sz)
                sn = astates(rn)
                check(not rn.scripted_missing() and len(sn) == len(items) + 1 and len(so) == len(items) + 1, f"{sz} marks {view}: all clicked, all states read: {rn.scripted_missing()[:3]} ({len(sn)}, {len(so)})")
                if len(sn) != len(items) + 1 or len(so) != len(items) + 1:
                    continue
                for i, (nc, oc, key, kind) in enumerate(items):
                    check(sn[i + 1][key] != sn[i][key], f"{sz} marks {view}: {nc} changed {key} ({sn[i][key]} -> {sn[i + 1][key]})")
                    if nc == "atom.walkrestart":
                        # (the old panel's Restart walk could not be clicked by the harness: at the centre and at the left edge of its rect the click changed nothing, see REPORT.md;
                        # the reference here is what walkRestart does (atom_sim.cpp): the walk starts again from the chosen nuclide, 0 steps, as before the first step)
                        check(sn[i + 1][key] == "0" == sn[0][key], f"{sz} marks {view}: Restart walk goes back to 0 steps ({sn[i + 1][key]})")
                        continue
                    check(sn[i + 1][key] == so[i + 1][key], f"{sz} marks {view}: {nc} gives {key}={sn[i + 1][key]!r}, the old {oc} gives {so[i + 1][key]!r}")
                    if key != "walkSteps":            # (a walk step is a random draw: the drawn chain is the same, the walker may stand elsewhere)
                        dn = {k: sn[i + 1][k] for k in DRAWN}; do = {k: so[i + 1][k] for k in DRAWN}
                        check(dn == do, f"{sz} marks {view}: after {nc} the picture drew the same marks as the old one: new {dn} old {do}")
                last = sn[-1]
                if view in ("cloud", "slice"):
                    # independent: the outline is the 90 % region (its definition; outline.h), and Wheel colours by phase
                    check(last["drawnOutline"] == ("side" if view == "cloud" else "slice") and abs(float(last["drawnMass"]) - 0.9) < 0.02 and float(last["drawnSegs"]) > 0,
                          f"{sz} marks {view}: the 90 % outline is drawn round about 90 % of the probability: {last['drawnOutline']} mass {last['drawnMass']} segs {last['drawnSegs']}")
                    check(last["drawnWheel"] == "1" and sn[0]["drawnWheel"] == "0", f"{sz} marks {view}: the phase colours went from Sign to the Wheel")
                else:
                    # independent: the uranium series from U-238 to Pb-206 has 15 members on its main path (U-238, Th-234, Pa-234, U-234, Th-230, Ra-226, Rn-222, Po-218, Pb-214,
                    # Bi-214, Po-214, Pb-210, Bi-210, Po-210, Pb-206; e.g. the NNDC chart / any nuclear physics text): the chain drawn has at least these; hiding it draws none
                    check(int(sn[0]["drawnChainNodes"]) >= 15, f"{sz} chain: U-238's chain drawn with at least the 15 members of the uranium series ({sn[0]['drawnChainNodes']})")
                    check(last["chain"] == "0" and last["drawnChainNodes"] == "-1", f"{sz} chain: hidden after the check box ({last['chain']}, {last['drawnChainNodes']})")

    if part in ("photons", "all"):
        # ---- 6. (stage 5e) photons: emit / absorb / auto do what the old buttons do (the hydrogen atom starts in n = 1: nothing below it to emit to) ----
        P1 = (["astate", "click=atom.emit", ("wait", 4), "astate", "click=atom.absorb", ("wait", 4), "astate"], ["astate", "click=old.emit", ("wait", 4), "astate", "click=old.absorb", ("wait", 4), "astate"])
        P2 = (["click=atom.n@1.0", ("wait", 8), "astate", "click=atom.emit", ("wait", 4), "astate"], ["click=old.n@1.0", ("wait", 8), "astate", "click=old.emit", ("wait", 4), "astate"])
        P3 = (["astate", "click=atom.auto", ("wait", 4), "astate", "click=atom.auto", ("wait", 4), "astate"], ["astate", "click=old.auto", ("wait", 4), "astate", "click=old.auto", ("wait", 4), "astate"])
        def kind(st):
            l = st.get("last", "")
            return l.split("  ·  ")[0], st.get("pending"), st.get("auto"), st.get("n")
        for name, (sn_, so_) in (("p1", P1), ("p2", P2), ("p3", P3)):
            a, fa = script(sn_); b, fb = script(so_)
            so = astates(old_run("atom_panel", f"ph_old_{name}", dict(OLD, PL_SCRIPT=b), frames=fb, size=OLD_SIZE))
            for sz in SIZES:
                rn = run_app(binary, work, f"ph_new_{name}_{sz}", dict(NEW, PL_SCRIPT=a), frames=fa, size=sz)
                sn = astates(rn)
                check(not rn.scripted_missing() and len(sn) == len(so) and len(sn) > 1, f"{sz} {name}: clicked and read ({rn.scripted_missing()[:2]}, {len(sn)} vs {len(so)})")
                same = (lambda k: k[:3]) if name == "p2" else (lambda k: k)          # (0.46: the slider's top is n = 6, the old one's 4: p2 compares the photon state, not n)
                check([same(kind(x)) for x in sn] == [same(kind(x)) for x in so], f"{sz} {name}: the same photon state as the old buttons: new {[kind(x) for x in sn]} old {[kind(x) for x in so]}")
                if name == "p1" and len(sn) == 3:
                    check(kind(sn[1])[0] == "" and kind(sn[1])[1] == "none", f"{sz}: emit from n = 1 does nothing (no lower level) {kind(sn[1])}")
                    check(kind(sn[2])[0] == "last photon: absorbed" and kind(sn[2])[1] == "absorb", f"{sz}: absorb starts an absorbed photon {kind(sn[2])}")
                if name == "p2" and len(sn) == 2:
                    check(sn[0]["n"] == "6" and kind(sn[1])[0] == "last photon: emitted", f"{sz}: from n = 6 (the slider's top since 0.46) emit starts an emitted photon {kind(sn[1])}")
                if name == "p3" and len(sn) == 3:
                    check([x["auto"] for x in sn] == ["0", "1", "0"], f"{sz}: Auto photons toggles on and off {[x['auto'] for x in sn]}")
        # ---- 7. check a jump: two hydrogen cases of the existing atom_tests (NIST ASD values): 2p -> 1s A = 6.265e8 /s, AAA, 100 %; 2s -> 1s not allowed, only M1 with A = 2.495e-6 /s ----
        for name, new_s, old_s, want in (("2p1s", "", "", ("6.265×10⁸", "AAA", "100 %")), ("2s1s", "10:click=atom.jfrom=2s", "8:move=820,1400;10:wheel=-60;24:click=old.jfrom=2s", ("M1", "2.495"))):
            ro = old_run("atom_panel", f"jc_old_{name}", dict(OLD, PL_SCRIPT=old_s), frames=40, size=OLD_SIZE)      # (the old card is below the window's bottom: the old column is scrolled first)
            jo = ro.note("jumpcheck")
            check(not ro.scripted_missing(), f"old check a jump {name}: clicked {ro.scripted_missing()[:2]}")
            for sz in SIZES:
                rn = run_app(binary, work, f"jc_new_{name}_{sz}", dict(NEW, PL_SCRIPT=new_s), frames=30, size=sz)
                jn = rn.note("jumpcheck")
                check(not rn.scripted_missing() and jn and jo and jn == jo, f"{sz} check a jump {name}: the same lines as the old card: new {jn[:1]} old {jo[:1]}")
                check(jn and all(w in jn[0] for w in want), f"{sz} check a jump {name}: has the NIST values {want}: {jn[:1]}")

    if part in ("dirac", "all"):
        # ---- 6a. (0.49.3, outside review) the Dirac cloud of a one-electron ion: uranium (Z = 92) opens as the Dirac state (Cloud model Auto); a tap on Non-relativistic gives the old orbital ----
        for sz in SIZES:
            def last_row(tag, scr, frames):
                r = run_app(binary, work, f"dirac_{tag}_{sz}", dict(NEW, PL_ATOM_SET="Z=92;N=146;elec=Hydrogen-like;view=Cloud 3D;orb=2p", PL_SCRIPT=scr), frames=frames, size=sz)
                rows = r.note("inforows")
                return r, (" ".join(rows[-1].split(" | ")[1:]) if rows else "")
            r1, auto = last_row("auto", "20:astate", 25)
            r2, nonrel = last_row("nonrel", "20:click=atom.hrel=Non-relativistic;60:astate", 65)
            check(not r1.scripted_missing() and "2p₃/₂" in auto and "Dirac + Lamb shift" in auto and "lined up" in auto,
                  f"{sz} uranium one-electron: the Dirac state 2p3/2, lined up, with its Dirac + Lamb level ({r1.scripted_missing()[:1]}, {auto[:160]!r})")
            check(not r2.scripted_missing() and "2p₃/₂" not in nonrel and "2pz orbital" in nonrel,
                  f"{sz} uranium: a tap on Cloud model Non-relativistic gives the non-relativistic 2pz orbital ({r2.scripted_missing()[:1]}, {nonrel[:160]!r})")
    if part in ("place", "all"):
        # ---- 6b. (0.47) "Put the outer electron in": lithium with every electron, a tap on 3p puts the outer electron there (no photon); the readout says so ----
        for sz in SIZES:
            r = run_app(binary, work, f"ph_place_{sz}", dict(NEW, PL_ATOM_SET="Z=3;N=4;elec=All;view=Cloud 3D", PL_SCRIPT="12:click=atom.place=3p;60:astate"), frames=70, size=sz)
            rows = r.note("inforows")[-1].split(" | ")[1:] if r.note("inforows") else []
            text = " ".join(rows)
            st = astates(r)
            check(not r.scripted_missing() and "outer electron: 3p" in text, f"{sz} lithium: a tap on 3p puts the outer electron in 3p ({r.scripted_missing()[:1]}, {text[:120]!r})")
            check(bool(st) and st[-1].get("pending") == "none", f"{sz} lithium: placing the electron starts no photon ({st[-1].get('pending') if st else None})")
    if part in ("levels", "all"):
        # ---- 6c. (0.47.3) a level atom: magnesium with every electron: a tap on the J=1 button of 3s3p 1P puts the atom in that level (no photon); the round switch;
        #      oganesson (nothing measured): Emit / Absorb greyed with the D5 sentence (PLAN_JUMPS.md section 7) ----
        sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
        import make_level_data as LD
        mg = LD.atom("Mg")["levels"]
        p1 = next(i for i, L in enumerate(mg) if L["conf"] == "3s.3p" and L["term"] == "1P*" and L["J2"] == 2)
        for sz in SIZES:
            r = run_app(binary, work, f"lv_mg_{sz}", dict(NEW, PL_ATOM_SET="Z=12;N=12;elec=All;view=Cloud 3D", PL_SCRIPT=f"12:click=atom.level={p1};60:astate;64:click=atom.shape=round;110:astate"), frames=120, size=sz)
            rows = r.note("inforows")[-1].split(" | ")[1:] if r.note("inforows") else []
            text = " ".join(rows)
            st = astates(r)
            check(not r.scripted_missing() and "level: 3s3p ¹P° J=1" in text, f"{sz} magnesium: a tap on 3s3p 1P J=1 puts the atom there ({r.scripted_missing()[:1]}, {text[:120]!r})")
            check(bool(st) and st[0].get("pending") == "none", f"{sz} magnesium: placing starts no photon ({st[0].get('pending') if st else None})")
            check("drawn round" in text, f"{sz} magnesium: the Round switch draws the free atom and the readout says so ({text[-160:]!r})")
            # (0.47.4, D4) strontium in 5s6s 3S J=1: its jump to 5s6p 3P J=0 has no A in NIST; the panel shows this app's own, marked as computed
            sr = LD.atom("Sr")["levels"]
            s6 = next(i for i, L in enumerate(sr) if L["conf"] == "5s.6s" and L["term"] == "3S" and L["J2"] == 2)
            r = run_app(binary, work, f"lv_sr_{sz}", dict(NEW, PL_ATOM_SET="Z=38;N=50;elec=All;view=Cloud 3D", PL_SCRIPT=f"12:click=atom.level={s6};40:astate"), frames=45, size=sz)
            ctl = [t for k, px, t in r.texts if k == "ctl"]   # (the panel wraps the caption over several text lines; the lines are not kept in reading order)
            own = any(t.startswith("absorb: A = 7.7 × 10⁶ /s, computed by this app") for t in ctl) and any(t.startswith("(GRASP2018), not measured") for t in ctl)
            check(not r.scripted_missing() and own, f"{sz} strontium 5s6s 3S1: the jump to 5s6p 3P0, which has no A in NIST, shows the app's own, marked computed ({r.scripted_missing()[:1]}, {own})")
            r = run_app(binary, work, f"lv_og_{sz}", dict(NEW, PL_ATOM_SET="Z=118;elec=All;view=Cloud 3D"), frames=30, size=sz)
            nj = r.note("nojump")
            check(bool(nj) and "nobody has measured light from this atom yet" in nj[-1], f"{sz} oganesson: the D5 sentence under greyed Emit / Absorb ({nj[-1:] if nj else None})")
    if part in ("field", "all"):
        # ---- 6d. (0.54.0) the Magnetic field section (atom/zeeman.h). One size (the layout does not change the result). Sodium with every electron, the data sheet's Spectrum tab open:
        #      field 0 changes nothing (the readout, every card string and the settings as without the setting); at 1 T sodium D2 (the default line of the ground level) has
        #      6 components, all seen across the field and 4 along it (pi is not seen along the field); mercury 6s6p 3P1 in 1 T: the readout gives the LS g and NIST's measured g ----
        sz = "1280x800"
        SET = "Z=11;N=12;elec=All;view=Cloud 3D"
        SCR = "10:click=card.all;16:click=data.tab.spectrum;24:click=ctl.sec.field"
        def cards(r):
            return [l for l in r.out.splitlines() if l.startswith("PLATOMCARD")]
        def rows(r):
            n = r.note("inforows")
            return n[-1] if n else ""
        r0 = run_app(binary, work, "fld_none", dict(NEW, PL_LOG_CARD="1", PL_ATOM_SET=SET, PL_SCRIPT=SCR), frames=40, size=sz)
        r1 = run_app(binary, work, "fld_zero", dict(NEW, PL_LOG_CARD="1", PL_ATOM_SET=SET + ";bfield=0", PL_SCRIPT=SCR), frames=40, size=sz)
        secs = r0.note("ctlsections")
        check(bool(secs) and "field" in secs[-1].split(), f"field: the Magnetic field section is shown ({secs[-1:]})")
        check(not r0.scripted_missing() and not r1.scripted_missing(), f"field: the script ran ({r0.scripted_missing()[:2]} {r1.scripted_missing()[:2]})")
        check(rows(r0) != "" and rows(r0) == rows(r1), "field 0: the readout is as without the setting")
        check(cards(r0) != [] and cards(r0) == cards(r1), "field 0: every card string is as without the setting")
        check(astates(r0)[-1:] == astates(r1)[-1:], "field 0: every Atom setting is as without it")
        check(not any("zeemancard" in l for l in cards(r1)) and not r1.note("zeeman kind=1 line=5 B=1"), "field 0: no split line is drawn")
        for look, want in (("Across", 6), ("Along", 4)):
            r = run_app(binary, work, f"fld_na_{look}", dict(NEW, PL_LOG_CARD="1", PL_ATOM_SET=SET + f";bfield=1;blook={look}", PL_SCRIPT=SCR), frames=40, size=sz)
            z = [n for n in r.notes if n.startswith("zeeman ")]
            zc = [l for l in cards(r) if "zeemancard" in l]
            ok = bool(z) and "comps=6 " in z[-1] and f"seen={want} " in z[-1] and "3p ²P°3/2 → 3s ²S1/2" in z[-1] and bool(zc) and f"comps=6 seen={want} " in zc[-1]
            check(ok, f"field: sodium D2 at 1 T shows {want} components seen {look.lower()} the field ({z[-1:]}, {zc[-1:]})")
        sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
        import make_level_data as LD
        hg = LD.atom("Hg")["levels"]
        p31 = next(i for i, L in enumerate(hg) if L["conf"] == "5d10.6s.6p" and L["term"] == "3P*" and L["J2"] == 2)
        r = run_app(binary, work, "fld_hg", dict(NEW, PL_ATOM_SET="Z=80;N=122;elec=All;view=Cloud 3D;bfield=1", PL_SCRIPT=f"12:click=atom.level={p31};40:astate"), frames=45, size=sz)
        t = rows(r).replace(" | ", " ")   # (the readout's wrapped rows)
        check(not r.scripted_missing() and "g (LS) = 1.50116" in t and "g (measured, NIST ASD) = 1.486094" in t, f"field: mercury 6s6p 3P1's card gives the LS g and NIST's measured g ({t[-220:]!r})")
    if part in ("vdots", "all"):
        # ---- 6e. (0.54.1) "Outer electron's dots: Easier to see / True share". One theme, one size (the layout does not change the result). Sodium, every electron, Cloud 3D: the
        #      choice is there (a tap on True share is taken), the readout drops "drawn ... denser" and says 1 in 11, faint; iron (no jumps) and the Slice view: not there;
        #      Most realistic picks True share (realest.cpp). References: the electron count (1 outer electron of 11) and the app's own default (Easier to see, as before 0.54.1) ----
        sz = "1280x800"
        def rows(r):
            n = r.note("inforows")
            return n[-1].replace(" | ", " ") if n else ""
        SET = "Z=11;N=12;elec=All;view=Cloud 3D"
        r0 = run_app(binary, work, "vd_na_easy", dict(NEW, PL_ATOM_SET=SET, PL_SCRIPT="30:astate"), frames=35, size=sz)
        r1 = run_app(binary, work, "vd_na_true", dict(NEW, PL_ATOM_SET=SET, PL_SCRIPT="12:click=atom.vdots=True share;40:astate"), frames=45, size=sz)
        st0, st1 = astates(r0), astates(r1)
        check(bool(st0) and st0[-1].get("vdots") == "Easier to see" and "× denser than its true share" in rows(r0), f"vdots: sodium starts with Easier to see, the readout says drawn denser ({st0[-1].get('vdots') if st0 else None})")
        check(not r1.scripted_missing() and bool(st1) and st1[-1].get("vdots") == "True share", f"vdots: sodium shows the choice and a tap on True share is taken ({r1.scripted_missing()[:1]})")
        t1 = rows(r1)
        check("denser than its true share" not in t1 and "its dots at their true share, 1 in 11, so its cloud and its jumps are faint" in t1, f"vdots: with True share the readout says 1 in 11, faint, and not denser ({t1[:240]!r})")
        for tag, st_ in (("iron", "Z=26;N=30;elec=All;view=Cloud 3D"), ("slice", "Z=11;N=12;elec=All;view=Slice")):
            r = run_app(binary, work, f"vd_{tag}", dict(NEW, PL_ATOM_SET=st_, PL_SCRIPT="12:click=atom.vdots=True share;30:astate"), frames=35, size=sz)
            st = astates(r)
            check(bool(r.scripted_missing()) and bool(st) and st[-1].get("vdots") == "Easier to see", f"vdots: {tag}: the choice is not shown (nothing to tap; {r.scripted_missing()[:1]})")
        r = run_app(binary, work, "vd_real", dict(NEW, PL_ATOM_SET=SET, PL_SCRIPT="10:click=atom.realest;40:astate"), frames=45, size=sz)
        st = astates(r)
        check(not r.scripted_missing() and bool(st) and st[-1].get("vdots") == "True share", f"vdots: Most realistic picks True share ({r.scripted_missing()[:1]}, {st[-1].get('vdots') if st else None})")
    if part in ("tidy", "all"):
        # ---- 8. (stage 5f part 1) the View selector, the folded notes, the chart without Cloud rows ----
        # References: the OLD panel's View rows (old.view=...) for Cloud, Cloud 3D, Slice, Chart and Atom (= the old "Orbit"); the toolbar buttons (already compared with the old bar by atom_toolbar)
        # for Nucleus and Nucleon. The highlighted part is read from the note "viewsel" of the last frame.
        SEL = [("view.atom", "atom", "old.view=Orbit"), ("nucleus.nucleus", "nucleus", "⌕ Nucleus"), ("nucleus.nucleon", "nucleon", "nucleus.nucleon"),
               ("cloud.cloud", "cloud", "old.view=Cloud"), ("cloud.cloud3d", "cloud3d", "old.view=Cloud 3D"), ("cloud.slice", "slice", "old.view=Slice"), ("view.chart", "chart", "old.view=Chart of nuclides")]
        W = 100                          # frames to let the zoom animation finish
        C = {"PL_ATOM_SET": "Z=6;N=6;view=Orbit;elec=Hydrogen-like"}   # (0.46: the zoom is relative to the atom drawn; the recorded old runs are one electron) carbon: its nucleus has several nucleons, so "Nucleus" is a different layer from "Nucleon" (hydrogen's nucleus is one proton)
        def sel_script(click, pre=None):
            steps = (["click=" + pre, ("wait", 12)] if pre else []) + ["click=" + click, ("wait", W), "astate"]
            return script(steps)
        calls = []
        for name, short, ref in SEL:
            pre_new = "nucleus.nucleus" if short == "nucleon" else "tool.cloud" if short == "atom" else None
            sn_, fn_ = sel_script(name, pre_new)
            for sz in SIZES:
                calls.append(((binary, work, f"sel_{short}_{sz}", dict(NEW, PL_SCRIPT=sn_, **C)), dict(frames=fn_, size=sz)))
            if ref.startswith(("old.", "⌕")):
                pre_old = "old.view=Cloud" if short == "atom" else None
                so_, fo_ = sel_script(ref, pre_old)
            else:
                sr_, fr_ = sel_script(ref, pre_new)
                calls.append(((binary, work, f"sel_ref_{short}", dict(NEW, PL_SCRIPT=sr_, **C)), dict(frames=fr_, size="1280x800")))
        prefetch(calls)
        for name, short, ref in SEL:
            pre_new = "nucleus.nucleus" if short == "nucleon" else "tool.cloud" if short == "atom" else None
            sn_, fn_ = sel_script(name, pre_new)
            if ref.startswith(("old.", "⌕")):
                so_, fo_ = sel_script(ref, "old.view=Cloud" if short == "atom" else None)
                rr = old_run("atom_panel", f"sel_old_{short}", dict(OLD, PL_SCRIPT=so_, **C), frames=fo_, size=OLD_SIZE)
            else:
                sr_, fr_ = sel_script(ref, pre_new)
                rr = run_app(binary, work, f"sel_ref_{short}", dict(NEW, PL_SCRIPT=sr_, **C), frames=fr_, size="1280x800")
            st_ref = astates(rr)
            for sz in SIZES:
                rn = run_app(binary, work, f"sel_{short}_{sz}", dict(NEW, PL_SCRIPT=sn_, **C), frames=fn_, size=sz)
                st = astates(rn)
                check(not rn.scripted_missing() and len(st) == 1 and len(st_ref) == 1, f"{sz} selector {name}: found, clicked, read ({rn.scripted_missing()[:2]}, {len(st)}, {len(st_ref)})")
                if len(st) == 1 and len(st_ref) == 1:
                    check(st[0]["view"] == st_ref[0]["view"] and abs(float(st[0]["zoom"]) - float(st_ref[0]["zoom"])) <= 0.15,
                          f"{sz} selector {name}: same view and zoom as {ref}: new {st[0]['view']} {st[0]['zoom']}, ref {st_ref[0]['view']} {st_ref[0]['zoom']}")
                n = rn.note("viewsel")
                # (0.22 stage 6: two rows, the picture and the zoom; the note is "viewsel <picture> zoom=<level>")
                want = {"atom": "orbit zoom=atom", "nucleus": "orbit zoom=nucleus", "nucleon": "orbit zoom=nucleon"}.get(short, short + " zoom=atom")
                check(n == ["viewsel " + want], f"{sz} selector {name}: the highlighted parts are {want!r}: {n}")
                small = [(px, t) for k, px, t in rn.texts if k == "ctl" and px < 14 - 0.01]
                check(not small, f"{sz} selector {name}: all panel text is at least 14 px: {small[:3]}")
        # the selector is one compact control: two rows of segmented buttons, inside the panel, one under the other
        for sz in SIZES:
            r = run_app(binary, work, f"seldraw_{sz}", dict(NEW), frames=30, size=sz)
            pc, r1, r2 = r.rects.get("panel.controls"), r.rects.get("atom.viewsel1"), r.rects.get("atom.viewsel2")
            check(pc and r1 and r2 and all(inside(q, r.window[0], r.window[1]) and q[0] >= pc[0] - 0.6 and q[0] + q[2] <= pc[0] + pc[2] + 0.6 for q in (r1, r2)), f"{sz}: the two rows of view buttons are inside the panel {r1} {r2} {pc}")
            check(r1 and r2 and r2[1] >= r1[1] + r1[3] - 0.6 and abs(r1[2] - r2[2]) < 0.6, f"{sz}: the rows sit one under the other and are equally wide")
        # notes: closed by default (Orbit, Cloud, Chart views), opened by their row, closed by it again; the same words
        for view, pre, keys in (("orbit", "", ["view", "orbital", "jump"]), ("cloud", "10:click=tool.cloud", ["view", "orbital", "phase", "jump"]), ("chart", "10:click=tool.chart", ["view", "chart"])):
            r = run_app(binary, work, f"notes0_{view}", dict(NEW, PL_SCRIPT=pre), frames=40)
            n = r.note("atomnotes")
            check(n and all(f"{k}=closed" in n[0] for k in keys) and "=open" not in n[0], f"{view}: every note fold is closed by default and the folds {keys} are there: {n}")
            for k in keys:
                steps = ([pre] if pre else []) + [f"24:click=atom.notes.{k}"]
                r1 = run_app(binary, work, f"notes1_{view}_{k}", dict(NEW, PL_SCRIPT=";".join(steps)), frames=50)
                n1 = r1.note("atomnotes")
                check(not r1.scripted_missing() and n1 and f"{k}=open" in n1[0], f"{view}: the row 'Notes about this choice' of {k} opens it: {n1} {r1.scripted_missing()[:2]}")
                texts = [t for kk, _, t in r1.texts if kk == "ctlnote"]
                check(any(len(t) > 20 for t in texts), f"{view}: its note text is shown once open ({len(texts)} note texts)")
            r2 = run_app(binary, work, f"notes2_{view}", dict(NEW, PL_SCRIPT=(pre + ";" if pre else "") + f"24:click=atom.notes.{keys[0]};40:click=atom.notes.{keys[0]}"), frames=60)
            n2 = r2.note("atomnotes")
            check(n2 and f"{keys[0]}=closed" in n2[0], f"{view}: clicking the row again closes it: {n2}")
            r3 = run_app(binary, work, f"notes3_{view}", dict(NEW, PL_SCRIPT=pre), frames=40)
            check(not [t for kk, _, t in r3.texts if kk == "ctlnote"], f"{view}: no note text is drawn while the folds are closed")
        # the chart: no Cloud rows (they do nothing there); the other views keep them
        for sz in SIZES:
            r = run_app(binary, work, f"chartnocloud_{sz}", dict(NEW, PL_SCRIPT="10:click=tool.chart;20:click=atom.cstyle=Classic;24:click=atom.cdens=Ultra;28:click=atom.super=On"), frames=50, size=sz)
            check(len(r.scripted_missing()) == 3, f"{sz} chart: Cloud style, density and superposition are not in the panel (all three clicks found nothing): {r.scripted_missing()}")
            n = r.note("ctlsections")
            check(n and "cloud" not in n[0].split()[1:], f"{sz} chart: no Cloud section {n}")
        # panel text >= 14 px in Orbit and Chart views at three sizes (the Cloud view is in part 3 of the sections check)
        for view, pre in (("orbit", ""), ("chart", "10:click=tool.chart")):
            for sz in SIZES:
                r = run_app(binary, work, f"txt_{view}_{sz}", dict(NEW, PL_SCRIPT=pre), frames=40, size=sz)
                small = [(px, t) for k, px, t in r.texts if k in ("ctl", "ctlnote") and px < 14 - 0.01]
                check(any(k == "ctl" for k, _, _ in r.texts) and not small, f"{sz} {view}: all panel text is at least 14 px: {small[:3]}")

    if part in ("facts", "all"):
        # ---- 4. the fact line: the longest fact, both labs, three sizes ----
        calls = [((binary, work, f"fact_{lab}_{sz}", dict({"PL_NEWUI": "on", "PL_FACT_LONGEST": "1"}, **({"PL_ATOM": "1"} if lab == "atom" else {}))), dict(frames=40, size=sz))
                 for lab in ("orbit", "atom") for sz in SIZES]
        prefetch(calls)
        for lab in ("orbit", "atom"):
            for sz in SIZES:
                env = dict({"PL_NEWUI": "on", "PL_FACT_LONGEST": "1"}, **({"PL_ATOM": "1"} if lab == "atom" else {}))
                r = run_app(binary, work, f"fact_{lab}_{sz}", env, frames=40, size=sz)
                fl = r.rects.get("factline")
                check(fl is not None, f"{lab} {sz}: the fact line is shown with the longest fact")
                if fl is None:
                    continue
                n = r.note("factlines")
                lines = int(n[0].split()[1]) if n else 0
                text = r.note("factline ")[0][len("factline "):] if r.note("factline ") else ""
                check(1 <= lines <= 2, f"{lab} {sz}: at most two lines ({lines})")
                check(lines == 2, f"{lab} {sz}: the longest fact wraps to two lines")
                cut = n and n[0].endswith(" cut")
                check(cut == text.endswith("…"), f"{lab} {sz}: '…' only when cut ({n})")
                w, h = r.window[0], r.window[1]
                check(inside(fl, w, h), f"{lab} {sz}: inside the window {fl}")
                for other in ("toolbar", "readout", "panel.controls", "handle", "topbar"):
                    if other in r.rects:
                        check(not overlap(fl, r.rects[other]), f"{lab} {sz}: clear of {other} {fl} {r.rects[other]}")
                check("toolbar" in r.rects and ("readout" in r.rects or lab == "atom") and "panel.controls" in r.rects and "handle" in r.rects, f"{lab} {sz}: toolbar, readout (Orbit only: 0.22 stage 6, the Atom Lab's readout is the fps counter, off by default), panel and handle were all there to check against")
            # a click opens the whole fact
            r = run_app(binary, work, f"fact_open_{lab}", dict({"PL_NEWUI": "on", "PL_FACT_LONGEST": "1", "PL_SCRIPT": "16:click=fact.line"}, **({"PL_ATOM": "1"} if lab == "atom" else {})), frames=40, size="880x560")
            check(not r.scripted_missing() and any(k.startswith("panel.facts") for k in r.rects), f"{lab}: a click on the fact line opens the whole fact {[k for k in r.rects if k.startswith('panel.')]}")
    print(f"atom panel checks: {checks - fails} of {checks} passed")
    sys.exit(1 if fails else 0)

main()
