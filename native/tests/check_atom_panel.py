#!/usr/bin/env python3
# 0.15 stage 5d: the Atom Lab's view options in the Controls panel (New UI On), and the fact line's two lines.
#   check_atom_panel.py <PhysicsLab binary> <work folder> [part]      part: sections | facts | all (default)
# References (none is an earlier output of ours): the OLD side panel of the same build (New UI Off) is the reference for every control: both runs press the real control through the
# real input path (PL_SCRIPT) and print every Atom setting (PLASTATE); the new control must give the old control's value. Which sections show in which view: the old panel's own rules
# (atom_panel.cpp: Orbital choice and orientation in the cloud views but not the chart, n and Playback speed in the Orbit view, cloud style / density / superposition when the view is
# not Orbit, Slice only in Slice, Orbit style only in Orbit, scale / zoom / spin not on the chart, quark style and nucleon detail from zoom 4, nucleon edge from zoom 3, tilt and height
# only on the chart). The owner's brief for 5d: the order View, Orbital, Cloud, Orbit, Scale and motion, Nucleus, Chart; text >= 14 px; three window sizes. Fact line: at most two lines,
# "…" only when two are too short, clear of the toolbar, readout, panel, handle and top bar, with the longest fact of the app's list (PL_FACT_LONGEST), in both labs.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, inside, overlap, prefetch

SIZES = ("1280x800", "880x560", "1920x1080")
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
         ("atom.n@0.667", "old.n@0.667", "n", 0),
         ("atom.speed@0.483", "old.speed@0.483", "speed", 0),
         ("atom.scale=Fixed", "old.scale=Fixed", "scale", "s"),
         ("atom.zoom@0.62", "old.zoom@0.62", "zoom", 0.15),
         ("atom.style=Hard balls", "old.style=Hard balls", "style", "s"),
         ("atom.qstyle=Accurate", "old.qstyle=Accurate", "qstyle", "s"),
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
CHART = [("atom.tilt@0.5", "old.tilt@0.5", "tilt", 0.03),
         ("atom.cheight=Binding energy per nucleon", "old.cheight=Binding energy per nucleon", "cheight", "s")]
# the view each group needs: (new UI click, old click)
VIEWS = {"orbit": (None, None), "cloud": ("tool.cloud", "old.view=Cloud"), "slice": ("cloud.slice", "old.view=Slice"), "cloud3d": ("cloud.cloud3d", "old.view=Cloud 3D"),
         "chart": ("tool.chart", "old.view=Chart of nuclides")}
GROUPS = (("orbit", ORBIT), ("cloud", CLOUD), ("slice", SLICE), ("cloud3d", CLOUD3D), ("chart", CHART))
DEFAULTS = {}

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
    NEW = {"PL_NEWUI": "on", "PL_ATOM": "1"}
    OLD = {"PL_NEWUI": "off", "PL_ATOM": "1"}

    if part in ("sections", "all"):
        calls = []
        for view, items in GROUPS:
            s_old, f_old = group_script(view, items, False)
            calls.append(((binary, work, f"old_{view}", dict(OLD, PL_SCRIPT=s_old)), dict(frames=f_old, size=OLD_SIZE)))
            s_new, f_new = group_script(view, items, True)
            for sz in SIZES:
                calls.append(((binary, work, f"new_{view}_{sz}", dict(NEW, PL_SCRIPT=s_new)), dict(frames=f_new, size=sz)))
        prefetch(calls)
        # ---- 1. every control changes the same setting as the old one, to the same value, at three window sizes ----
        for view, items in GROUPS:
            s_old, f_old = group_script(view, items, False)
            ro = run_app(binary, work, f"old_{view}", dict(OLD, PL_SCRIPT=s_old), frames=f_old, size=OLD_SIZE)
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
                    if kind == "s":
                        check(after_n == after_o, f"{sz} {view}: {nc} gives {key}={after_n!r}, the old {oc} gives {after_o!r}")
                    else:
                        check(abs(float(after_n) - float(after_o)) <= kind, f"{sz} {view}: {nc} gives {key}={after_n}, the old {oc} gives {after_o} (allowed {kind})")
                    # nothing else changed by the click (the same as the old control, which may also reset dependent state: compare the whole line)
                    diff_n = {k for k in sn[i + 1] if sn[i + 1][k] != sn[i][k]}
                    diff_o = {k for k in so[i + 1] if so[i + 1][k] != so[i][k]}
                    check(diff_n == diff_o, f"{sz} {view}: {nc} changed the same settings as the old control: new {sorted(diff_n)} old {sorted(diff_o)}")
        # ---- 2. which sections show in which view (the old panel's rules), in the brief's order; each folds ----
        ORDER = ["view", "orbital", "cloud", "orbit", "scale", "nucleus", "chart"]
        WANT = {"orbit": {"view", "orbital", "orbit", "scale"}, "cloud": {"view", "orbital", "cloud", "scale"}, "chart": {"view", "cloud", "chart"},
                "nucleus": {"view", "orbital", "orbit", "scale", "nucleus"}}
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
                check("toolbar" in r.rects and "readout" in r.rects and "panel.controls" in r.rects and "handle" in r.rects, f"{lab} {sz}: toolbar, readout, panel and handle were all there to check against")
            # a click opens the whole fact
            r = run_app(binary, work, f"fact_open_{lab}", dict({"PL_NEWUI": "on", "PL_FACT_LONGEST": "1", "PL_SCRIPT": "16:click=fact.line"}, **({"PL_ATOM": "1"} if lab == "atom" else {})), frames=40, size="880x560")
            check(not r.scripted_missing() and any(k.startswith("panel.facts") for k in r.rects), f"{lab}: a click on the fact line opens the whole fact {[k for k in r.rects if k.startswith('panel.')]}")
    print(f"atom panel checks: {checks - fails} of {checks} passed")
    sys.exit(1 if fails else 0)

main()
