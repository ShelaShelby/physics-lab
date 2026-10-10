#!/usr/bin/env python3
# 0.15 stage 5p: the owner's bug list. Checked in the REAL app (Linux, software OpenGL, scripted events; NOT a real mouse, NOT Windows, NOT a real GPU).
#   check_5p.py <PhysicsLab binary> <work folder> <part>      parts: wheel, scroll, zoomout
# part wheel (#6): after a window is opened and closed again, the mouse wheel over the canvas must zoom exactly like it does with no window ever opened (the baseline).
# The reference is a scripted run with no window at all, not an earlier output of ours.
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, prefetch

fails = checks = 0

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def kv(r, nth):
    st = r.states
    if len(st) <= nth:
        return None
    return dict(p.split("=", 1) for p in st[nth].replace("| ", "").split() if "=" in p)

# how each window is opened and closed (the owner's case is "about": a click on the Physics Lab name in the top bar, then a click on the dimmed picture beside the page)
DIM = "clickxy=30,420"
CASES = {
    "about":    ({}, "10:click=top.physics", "20:" + DIM),
    "aboutesc": ({}, "10:click=top.physics", "20:key=escape"),
    "history":  ({"PL_OPEN": "history"}, "", "20:" + DIM),
    "log":      ({"PL_OPEN": "log"}, "", "20:" + DIM),
    "learn":    ({}, "10:click=top.learn", "20:click=top.learn"),
    "settings": ({}, "10:click=top.settings", "20:click=top.settings"),
    "theme":    ({}, "10:click=top.theme", "20:click=top.theme"),
    "search":   ({}, "10:click=top.search", "20:key=escape"),
    "labs":     ({}, "10:click=top.lab", "20:click=top.lab"),
}
LABS = (("orbit", {}), ("atom", {"PL_ATOM": "1"}))

def pscrolls(r):
    return [(float(ln.split()[1]), float(ln.split()[2]), float(ln.split()[3])) for ln in r.out.splitlines() if ln.startswith("PLSCROLL ")]

def script(open_, close_):
    return ";".join(s for s in (open_, close_, "16:pscroll", "30:move=500,450", "32:pscroll", "34:state", "36:wheel=3", "44:state") if s)

def part_wheel(binary, work):
    runs = []
    for lab, lenv in LABS:
        for name, (env, o, c) in dict(base=({}, "", ""), **CASES).items():
            runs.append(((binary, work, f"p_{lab}_{name}", dict(lenv, PL_NEWUI="on", PL_SCRIPT=script(o, c), **env)), {"frames": 50}))
    prefetch(runs)
    for lab, _ in LABS:
        res = {}
        for args, kw in runs:
            if args[2].startswith(f"p_{lab}_"):
                res[args[2][len(f"p_{lab}_"):]] = run_app(*args, **kw)
        def z(name, nth):
            d = kv(res[name], nth)
            return float(d["zoom"]) if d and "zoom" in d else None
        b0, b1 = z("base", 0), z("base", 1)
        check(b0 is not None and b1 is not None and abs(b1 - b0) > 1e-9, f"{lab}: baseline wheel changes the zoom ({b0} -> {b1})")
        for name in ("about", "aboutesc", "history", "log"):
            ps = pscrolls(res[name])
            check(len(ps) == 2 and ps[0][2] > 0, f"{lab}: '{name}' really opened a page at frame 16 (otherwise the test proves nothing) {ps}")
            check(len(ps) == 2 and ps[1][1] < 0, f"{lab}: '{name}': once closed, the app no longer says a page is shown (frame 32 reading {ps[1:] }; -1 means none)")
        check(pscrolls(res["base"])[0][1] < 0, f"{lab}: the baseline run opened no page")
        for name in CASES:
            a0, a1 = z(name, 0), z(name, 1)
            ok = a0 is not None and a1 is not None and abs((a1 - a0) - (b1 - b0)) < 1e-6 and abs(a0 - b0) < 1e-9
            check(ok, f"{lab}: after '{name}' opened and closed the wheel zooms like the baseline (zoom {a0} -> {a1}, baseline {b0} -> {b1})")
    print(f"check_5p wheel: {checks} checks, {fails} failed")

def part_scroll(binary, work):
    # #2: the About page scrolls by the wheel and by PageUp / PageDown / Home / End, and its scrollbar is at least 12 px wide, draggable, inside the window and inside the page
    for sz in ("1280x800", "880x560"):
        W, H = (int(v) for v in sz.split("x"))
        pre = run_app(binary, work, f"p_scroll_pre_{sz}", {"PL_NEWUI": "on", "PL_OPEN": "about"}, frames=20, size=sz)
        col, th, tr = pre.rects.get("reading.column"), pre.rects.get("reading.scrollthumb"), pre.rects.get("reading.scrollbar")
        check(col is not None and th is not None and tr is not None, f"{sz}: About has a column and a scrollbar")
        if not (col and th and tr):
            continue
        check(th[2] >= 12 - 0.01, f"{sz}: the visible scrollbar is at least 12 px wide ({th[2]:.1f})")
        check(th[0] >= col[0] - 0.6 and th[0] + th[2] <= col[0] + col[2] + 0.6 and th[1] >= col[1] - 0.6 and th[1] + th[3] <= col[1] + col[3] + 0.6, f"{sz}: the scrollbar is inside the page {th} {col}")
        check(th[0] >= 0 and th[0] + th[2] <= W and th[1] >= 0 and th[1] + th[3] <= H, f"{sz}: the scrollbar is inside the window")
        cx, cy = col[0] + col[2] / 2, col[1] + col[3] / 2
        scr = f"6:move={cx:.0f},{cy:.0f};10:pscroll;12:wheel=-3;16:pscroll;18:key=pagedown;22:pscroll;24:key=pageup;28:pscroll;30:key=end;32:key=end;36:key=end;40:pscroll;42:key=home;46:pscroll"
        r = run_app(binary, work, f"p_scroll_keys_{sz}", {"PL_NEWUI": "on", "PL_OPEN": "about", "PL_SCRIPT": scr}, frames=52, size=sz)
        v = pscrolls(r)
        check(len(v) == 6, f"{sz}: six scroll readings ({len(v)})")
        if len(v) == 6:
            y0, y1, y2, y3, y4, y5 = (a[1] for a in v)
            mx = v[0][2]
            check(mx > 0, f"{sz}: About is longer than the window (max {mx})")
            check(y1 > y0, f"{sz}: the wheel moved the page ({y0} -> {y1})")
            check(y2 > y1, f"{sz}: PageDown moved it further ({y1} -> {y2})")
            check(y3 < y2, f"{sz}: PageUp moved it back ({y2} -> {y3})")
            check(abs(y4 - v[4][2]) < 1.0, f"{sz}: End (pressed three times: the page grows by about 1 % while its bottom is first drawn) reaches the bottom ({y4} of {v[4][2]})")
            check(y5 == 0, f"{sz}: Home reaches the top ({y5})")
        # drag of the thumb moves the page
        x, y0 = th[0] + th[2] / 2, th[1] + th[3] / 2
        r = run_app(binary, work, f"p_scroll_drag_{sz}", {"PL_NEWUI": "on", "PL_OPEN": "about", "PL_SCRIPT": f"6:move={x:.0f},{y0:.0f};10:drag={x:.0f},{y0:.0f},{x:.0f},{y0 + 60:.0f};24:pscroll"}, frames=30, size=sz)
        v = pscrolls(r)
        check(len(v) == 1 and v[0][1] > 0, f"{sz}: dragging the scrollbar moves the page ({v})")
    print(f"check_5p scroll: {checks} checks, {fails} failed")

# #3: the Atom Lab zooms out twice as far as before in every view. The old limit was the scale 1 (log10 zoom 0; the chart's zoom 1 = the whole chart fitted), so twice as far is the scale 1/2:
# log10 zoom = -log10(2) = -0.30103, chart zoom 0.5. (Independent of our code: it is the arithmetic of "half the picture size".)
import math
NEW_MIN = -math.log10(2.0)

def part_zoomout(binary, work):
    views = ("Orbit", "Cloud", "Cloud 3D", "Slice", "Chart of nuclides")
    runs = []
    for v in views:
        key = "czoom" if v.startswith("Chart") else "zoom"
        # many notches out, again more notches out, then ONE notch in; the pointer is over the picture, clear of the cards
        scr = "20:move=500,450;24:wheel=-30;30:state;31:astate;34:wheel=-5;40:state;42:wheel=1;48:state"
        env = {"PL_NEWUI": "on", "PL_ATOM": "1", "PL_ATOM_SET": f"view={v};zoom=3", "PL_SCRIPT": scr}
        runs.append(((binary, work, "p_zoom_" + v.replace(" ", "_"), env), {"frames": 54}))
    # a setting asked for from outside (PL_ATOM_SET zoom=-5, the same clamp as a saved setting uses) lands on the new limit too
    runs.append(((binary, work, "p_zoom_set", {"PL_NEWUI": "on", "PL_ATOM": "1", "PL_ATOM_SET": "view=Cloud 3D;zoom=-5", "PL_SCRIPT": "20:state"}), {"frames": 30}))
    prefetch(runs)
    for (args, kw), v in zip(runs, list(views) + ["set"]):
        r = run_app(*args, **kw)
        if v == "set":
            d = kv(r, 0)
            check(d is not None and abs(float(d["zoom"]) - NEW_MIN) < 1e-5, f"zoom=-5 from outside is cut at {NEW_MIN:.5f} ({d and d.get('zoom')})")
            continue
        key = "czoom" if v.startswith("Chart") else "zoom"
        want = 0.5 if key == "czoom" else NEW_MIN
        vals = [kv(r, i) for i in range(3)]
        check(all(d is not None for d in vals), f"{v}: three state readings")
        if not all(d is not None for d in vals):
            continue
        z1, z2, z3 = (float(d[key]) for d in vals)
        check(abs(z1 - want) < 1e-5, f"{v}: many notches out stop exactly at twice the old zoom-out: {key} {z1} (want {want:.5f})")
        check(abs(z2 - want) < 1e-5, f"{v}: more notches out do not go further: {key} {z2}")
        check(z3 > want + 0.05, f"{v}: one notch in leaves the limit: {key} {z3}")
        pa = [ln for ln in r.out.splitlines() if ln.startswith("PLATOMBOX")]
        if pa and not v.startswith("Chart"):
            m = dict(p.split("=", 1) for p in pa[0].split()[1:])
            ring = [float(x) for x in m["ring"].split(",")]
            lab = [float(x) for x in m["label"].split(",")]
            cx = [float(x) for x in m["centre"].split(",")]
            check(0 <= cx[0] <= 1280 and 0 <= cx[1] <= 800, f"{v}: the atom's centre {cx} stays in the window at the limit")
            check(lab[0] >= 0 and lab[1] >= 0 and lab[0] + lab[2] <= 1280 and lab[1] + lab[3] <= 800, f"{v}: the nucleus label {lab} stays in the window at the limit")
            check(ring[2] == 0 or (ring[0] >= 0 and ring[1] >= 0 and ring[0] + ring[2] <= 1280 and ring[1] + ring[3] <= 800), f"{v}: the phase ring {ring} stays in the window at the limit")
    print(f"check_5p zoomout: {checks} checks, {fails} failed")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"wheel": part_wheel, "scroll": part_scroll, "zoomout": part_zoomout}[part](binary, work)
    sys.exit(1 if fails else 0)
