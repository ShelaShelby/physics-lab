#!/usr/bin/env python3
# 0.15 stage 5q: the owner's PC findings, part 1. Checked in the REAL app (Linux, software OpenGL, scripted events; NOT a real mouse, NOT Windows, NOT a real GPU).
#   check_5q.py <PhysicsLab binary> <work folder> <part>      parts: lit, theme, scrollbar, endkey
# Everything is measured on the PICTURE (the app's own pixels) with formulas written here (WCAG contrast), not read back from our colour constants.
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, prefetch, Bmp, overlap

fails = checks = 0

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def lin(c):
    c /= 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

def lum(rgb):
    r, g, b = (lin(v) for v in rgb)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b

def contrast(a, b):
    la, lb = lum(a), lum(b)
    return (max(la, lb) + 0.05) / (min(la, lb) + 0.05)

def chroma(p):
    return max(p) - min(p)

def dmax(a, b):
    return max(abs(x - y) for x, y in zip(a, b))

THEMES = "auto auto_light light dark black midnight solar paper sage contrast monochrome".split()

def theme_args(t):
    return ("auto", {"PL_LIGHT": "1"}) if t == "auto_light" else (t, {})

def best_icon(bmp, r, ref, inset=4):
    """the pixel of the icon area that contrasts most with the fill `ref` (the glyph's strongest pixel)"""
    best, bc = ref, 0
    for y in range(int(r[1]) + inset, int(r[1] + r[3]) - inset):
        for x in range(int(r[0]) + inset, int(r[0] + r[2]) - inset):
            p = bmp.px(x, y)
            c = contrast(p, ref)
            if c > bc:
                best, bc = p, c
    return best

def judge(tag, bmp, lit, unlit, outline_theme_idle_ring=False):
    """lit, unlit: button rects (x, y, w, h) of a lit button and a plain button of the same bar."""
    cx, cy = int(lit[0] + lit[2] / 2), int(lit[1] + lit[3] / 2)
    ux, uy = int(unlit[0] + unlit[2] / 2), int(unlit[1] + unlit[3] / 2)
    fill = bmp.px(int(lit[0]) + 3, cy)                                    # between the edge and the icon (the icon starts 5 px in on the 30 px top-bar buttons)
    ufill = bmp.px(int(unlit[0]) + 3, uy)
    check(dmax(fill, ufill) >= 10, f"{tag}: the lit fill {fill} differs from the bar {ufill} (largest channel difference {dmax(fill, ufill)}, at least 10)")
    check(dmax(fill, ufill) <= 80, f"{tag}: the lit fill {fill} is subtle against the bar {ufill} (difference {dmax(fill, ufill)}, at most 80)")
    check(chroma(fill) - chroma(ufill) <= 30, f"{tag}: the lit fill {fill} is not a coloured accent pill (colour spread {chroma(fill)} against the bar's {chroma(ufill)})")
    # no outline ring: the pixel row / column ON the edge and the one inside it lie between the fill and the colour just outside (a 1 px outline is darker or lighter than both)
    def between(p, a_, b_, tol=3):
        return all(min(a_[i], b_[i]) - tol <= p[i] <= max(a_[i], b_[i]) + tol for i in range(3))
    x0, y0, x1, y1 = int(lit[0]), int(lit[1]), int(lit[0] + lit[2]) - 1, int(lit[1] + lit[3]) - 1
    sides = [([(cx, y0), (cx, y0 + 1)], (cx, y0 - 2)), ([(cx, y1), (cx, y1 - 1)], (cx, y1 + 2)), ([(x0, cy), (x0 + 1, cy)], (x0 - 2, cy)), ([(x1, cy), (x1 - 1, cy)], (x1 + 2, cy))]
    ring = [[bmp.px(*q) for q in qs] + [bmp.px(*o)] for qs, o in sides]
    check(all(between(r_[0], fill, r_[2]) and between(r_[1], fill, r_[2]) for r_ in ring), f"{tag}: no outline: the edge pixels lie between the fill {fill} and the bar next to it {ring}")
    # no glow: 2..4 px above and below the button are the same colour as the same rows beside it (the unlit button's)
    out = []
    for k in (2, 3, 4):
        out.append((bmp.px(cx, int(lit[1]) - k), bmp.px(ux, int(unlit[1]) - k)))
        out.append((bmp.px(cx, int(lit[1] + lit[3]) + k - 1), bmp.px(ux, int(unlit[1] + unlit[3]) + k - 1)))
    check(all(dmax(a, b) <= 3 for a, b in out), f"{tag}: no glow: 2..4 px outside the button is the bar colour {out}")
    il, iu = best_icon(bmp, lit, fill), best_icon(bmp, unlit, ufill)
    check(contrast(il, fill) >= 4.5, f"{tag}: the lit icon {il} on its fill {fill}: contrast {contrast(il, fill):.2f} (at least 4.5)")
    check(contrast(il, iu) >= 3.0, f"{tag}: lit icon {il} against unlit icon {iu}: contrast {contrast(il, iu):.2f} (at least 3, without colour)")
    check(contrast(iu, ufill) >= 3.0, f"{tag}: the unlit icon {iu} is still readable on the bar {ufill}: contrast {contrast(iu, ufill):.2f} (at least 3)")

def part_lit(binary, work):
    runs = []
    for t in THEMES:
        th, env = theme_args(t)
        runs.append(((binary, work, f"q_lit_{t}", dict(env, PL_NEWUI="on", PL_ATOM="1", PL_OPEN="settings")), {"frames": 30, "theme": th}))
        runs.append(((binary, work, f"q_litorbit_{t}", dict(env, PL_NEWUI="on")), {"frames": 30, "theme": th}))
    prefetch(runs)
    for t in THEMES:
        th, env = theme_args(t)
        r = run_app(binary, work, f"q_lit_{t}", dict(env, PL_NEWUI="on", PL_ATOM="1", PL_OPEN="settings"), frames=30, theme=th)
        b = Bmp(r.bmp)
        on = [x for x in (r.note("toolon")[0][len("toolon "):].split(",") if r.note("toolon") else []) if x]
        btn = {k[len("toolbtn."):]: v for k, v in r.rects.items() if k.startswith("toolbtn.")}
        unlit = [k for k in btn if k not in on and not k.endswith(".caret") and k != "tool.more"]
        check(len(on) >= 1 and bool(unlit), f"{t}: the Atom toolbar has a lit and an unlit button ({on})")
        if on and unlit:
            judge(f"{t}: Atom toolbar {on[0]}", b, btn[on[0]], btn[unlit[0]])
        gear, srch = r.rects.get("topicon.settings"), r.rects.get("topicon.search")
        check(gear is not None and srch is not None, f"{t}: top bar has the gear and the search icon")
        if gear and srch:
            judge(f"{t}: top bar gear (Settings open)", b, gear, srch)
        # the Orbit toolbar's 2D / 3D segment: the chosen part is lit like the other toolbar buttons
        r = run_app(binary, work, f"q_litorbit_{t}", dict(env, PL_NEWUI="on"), frames=30, theme=th)
        b = Bmp(r.bmp)
        a2, a3 = r.rects.get("toolseg.tool.view.2d"), r.rects.get("toolseg.tool.view.3d")
        check(a2 is not None and a3 is not None, f"{t}: Orbit toolbar has the 2D / 3D segment")
        if a2 and a3:
            s2, s3 = b.px(int(a2[0]) + 5, int(a2[1] + a2[3] / 2)), b.px(int(a3[0]) + 5, int(a3[1] + a3[3] / 2))
            lit_s, unl_s = (s2, s3) if dmax(s2, b.px(int(a2[0]) - 6, int(a2[1] + a2[3] / 2))) > 0 else (s3, s2)
            check(chroma(lit_s) <= 30 + chroma(unl_s), f"{t}: Orbit toolbar: the lit part {lit_s} is not a coloured accent pill (the other part {unl_s})")
    # hover on a lit and on a plain button: still no coloured pill, no ring, no glow
    runs = []
    for t in THEMES:
        th, env = theme_args(t)
        runs.append(((binary, work, f"q_hov_{t}", dict(env, PL_NEWUI="on", PL_ATOM="1", PL_OPEN="settings", PL_SCRIPT="12:move=top.settings")), {"frames": 24, "theme": th}))
        runs.append(((binary, work, f"q_hovu_{t}", dict(env, PL_NEWUI="on", PL_ATOM="1", PL_OPEN="settings", PL_SCRIPT="12:move=top.search")), {"frames": 24, "theme": th}))
    prefetch(runs)
    for t in THEMES:
        th, env = theme_args(t)
        r = run_app(binary, work, f"q_hov_{t}", dict(env, PL_NEWUI="on", PL_ATOM="1", PL_OPEN="settings", PL_SCRIPT="12:move=top.settings"), frames=24, theme=th)
        gear, srch = r.rects.get("topicon.settings"), r.rects.get("topicon.search")
        if gear and srch:
            judge(f"{t}: top bar gear, pointer on it", Bmp(r.bmp), gear, srch)
        r = run_app(binary, work, f"q_hovu_{t}", dict(env, PL_NEWUI="on", PL_ATOM="1", PL_OPEN="settings", PL_SCRIPT="12:move=top.search"), frames=24, theme=th)
        b = Bmp(r.bmp)
        gear, srch = r.rects.get("topicon.settings"), r.rects.get("topicon.search")
        if gear and srch:
            sx, sy = int(srch[0]) + 5, int(srch[1] + srch[3] / 2)
            h = b.px(sx, sy)
            ach = r.rects.get("topicon.ach")
            bar = b.px(int(ach[0]) + 5, int(ach[1] + ach[3] / 2)) if ach else h
            check(chroma(h) - chroma(bar) <= 30, f"{t}: top bar search icon, pointer on it: the hover fill {h} is not a coloured accent pill (bar {bar})")
            cx = int(srch[0] + srch[2] / 2)
            hfill = b.px(int(srch[0]) + 3, int(srch[1] + srch[3] / 2))
            e = [b.px(cx, int(srch[1])), b.px(cx, int(srch[1]) + 1)]
            out = b.px(cx, int(srch[1]) - 2)
            check(t in ("contrast", "monochrome") or all(all(min(hfill[i], out[i]) - 3 <= p[i] <= max(hfill[i], out[i]) + 3 for i in range(3)) for p in e), f"{t}: hovered plain button has no ring {e} fill {hfill} outside {out}")
    print(f"check_5q lit: {checks} checks, {fails} failed")

# 2: the theme list (top bar) hangs below the top bar: it never covers a top-bar icon
def part_theme(binary, work):
    for sz in ("880x560", "1280x800", "2560x1369"):
        W, H = (int(v) for v in sz.split("x"))
        r = run_app(binary, work, f"q_theme_{sz}", {"PL_NEWUI": "on", "PL_SCRIPT": "10:click=top.theme"}, frames=24, size=sz)
        pop = r.rects.get("popup.themes")
        check(pop is not None, f"{sz}: the theme list opened")
        if not pop:
            continue
        icons = {k: v for k, v in r.rects.items() if k.startswith("topicon.")}
        check(len(icons) >= 6, f"{sz}: the top-bar icons are known ({sorted(icons)})")
        for k, v in icons.items():
            check(not overlap(pop, v), f"{sz}: the theme list {pop} does not cover {k} {v}")
        tb = r.rects.get("topbar")
        check(tb is not None and pop[1] >= tb[1] + tb[3] - 0.5, f"{sz}: the theme list starts below the top bar {pop} {tb}")
        check(pop[0] >= -0.5 and pop[0] + pop[2] <= W + 0.5 and pop[1] + pop[3] <= H + 0.5, f"{sz}: the theme list is inside the window {pop}")
    print(f"check_5q theme: {checks} checks, {fails} failed")

def thumbs(r):
    return r.rects.get("reading.scrollthumb"), r.rects.get("reading.scrollbar")

# 3: the About scrollbar keeps its width (12 px at 100 %) idle, hovered and while dragged
def part_scrollbar(binary, work):
    for sz in ("1280x800", "880x560"):
        base = run_app(binary, work, f"q_sb_idle_{sz}", {"PL_NEWUI": "on", "PL_OPEN": "about"}, frames=20, size=sz)
        th, tr = thumbs(base)
        check(th is not None, f"{sz}: About has a scrollbar")
        if not th:
            continue
        w0 = th[2]
        check(abs(w0 - 12.0) < 0.01, f"{sz}: idle width is 12 px ({w0})")
        x, y = th[0] + th[2] / 2, th[1] + th[3] / 2
        hov = run_app(binary, work, f"q_sb_hover_{sz}", {"PL_NEWUI": "on", "PL_OPEN": "about", "PL_SCRIPT": f"8:move={x:.0f},{y:.0f}"}, frames=20, size=sz)
        h, _ = thumbs(hov)
        check(h is not None and abs(h[2] - w0) < 0.01, f"{sz}: hovered width equals idle width ({h and h[2]} against {w0})")
        for fr in (12, 14, 15):                                         # the press is held at frames 11..15 of this script (drag at 10: press 10, moves 11..15, release 16)
            dr = run_app(binary, work, f"q_sb_drag{fr}_{sz}", {"PL_NEWUI": "on", "PL_OPEN": "about", "PL_SCRIPT": f"6:move={x:.0f},{y:.0f};10:drag={x:.0f},{y:.0f},{x:.0f},{y + 60:.0f}"}, frames=fr, size=sz)
            d, _ = thumbs(dr)
            check(d is not None and abs(d[2] - w0) < 0.01, f"{sz}: dragged width (frame {fr}) equals idle width ({d and d[2]} against {w0})")
            if fr >= 14: check(d is not None and d[1] > th[1] + 1, f"{sz}: frame {fr} really is mid-drag (the thumb moved down {d and d[1]} from {th[1]})")
        # the right edge does not move either (it widened to the left before: only the width counted, now the whole rectangle)
        check(h is not None and abs((h[0] + h[2]) - (th[0] + th[2])) < 0.01, f"{sz}: hovered right edge unchanged")
    print(f"check_5q scrollbar: {checks} checks, {fails} failed")

def pscrolls(r):
    return [(float(ln.split()[1]), float(ln.split()[2]), float(ln.split()[3])) for ln in r.out.splitlines() if ln.startswith("PLSCROLL ")]

# 4: ONE press of End reaches the true bottom (exactly), and Home the top
def part_endkey(binary, work):
    for sz in ("1280x800", "880x560", "2560x1369"):
        W, H = (int(v) for v in sz.split("x"))
        pre = run_app(binary, work, f"q_end_pre_{sz}", {"PL_NEWUI": "on", "PL_OPEN": "about"}, frames=20, size=sz)
        col = pre.rects.get("reading.column")
        check(col is not None, f"{sz}: About has a column")
        if not col:
            continue
        cx, cy = col[0] + col[2] / 2, col[1] + col[3] / 2
        # End pressed once at frame 12; read at 14 (just after), 20, 30 and 45; then Home; then End again after a wheel
        scr = f"6:move={cx:.0f},{cy:.0f};12:key=end;14:pscroll;20:pscroll;30:pscroll;45:pscroll;46:key=home;52:pscroll;54:key=end;70:pscroll"
        r = run_app(binary, work, f"q_end_{sz}", {"PL_NEWUI": "on", "PL_OPEN": "about", "PL_SCRIPT": scr}, frames=76, size=sz)
        v = pscrolls(r)
        check(len(v) == 6, f"{sz}: six scroll readings ({len(v)})")
        if len(v) != 6:
            continue
        check(v[0][2] > 0, f"{sz}: About is longer than the window (max {v[0][2]})")
        for i, name in ((1, "frame 20"), (2, "frame 30"), (3, "frame 45")):
            check(v[i][1] == v[i][2], f"{sz}: one End press: {name}: offset {v[i][1]} equals the maximum offset {v[i][2]} exactly")
        check(v[3][1] > 0, f"{sz}: End moved the page ({v[3][1]})")
        check(v[4][1] == 0, f"{sz}: Home (frame 46) reaches the exact top: {v[4][1]}")
        check(v[5][1] == v[5][2], f"{sz}: a second End (after Home) reaches the exact bottom: {v[5][1]} of {v[5][2]}")
    # Home: exact top after End
    for sz in ("1280x800",):
        pre = run_app(binary, work, f"q_end_pre_{sz}", {"PL_NEWUI": "on", "PL_OPEN": "about"}, frames=20, size=sz)
        col = pre.rects.get("reading.column")
        cx, cy = col[0] + col[2] / 2, col[1] + col[3] / 2
        r = run_app(binary, work, f"q_home_{sz}", {"PL_NEWUI": "on", "PL_OPEN": "about", "PL_SCRIPT": f"6:move={cx:.0f},{cy:.0f};12:key=end;30:key=home;34:pscroll;40:pscroll"}, frames=44, size=sz)
        v = pscrolls(r)
        check(len(v) == 2 and all(a[1] == 0 for a in v), f"{sz}: Home reaches the exact top and stays there ({v})")
    print(f"check_5q endkey: {checks} checks, {fails} failed")

# ---------------------------------------------------------------------------------------------- part 2
# 5: the Element picker (top bar chip) in the Atom Lab HOVERS: opening it moves nothing (atom, toolbar, FPS pill, scale bar: pixel for pixel the same outside the picker)
def box(r, key):
    m = re.search(key + r"=([-\d.e+]+),([-\d.e+]+),([-\d.e+]+),([-\d.e+]+)", r.out.split("PLATOMBOX")[-1])
    return tuple(float(m.group(i)) for i in range(1, 5)) if m else (0, 0, 0, 0)

def grow(rc, k):
    return (rc[0] - k, rc[1] - k, rc[2] + 2 * k, rc[3] + 2 * k)

def part_picker(binary, work):
    for sz in ("880x560", "1280x800", "2560x1369"):
        W, H = (int(v) for v in sz.split("x"))
        base = {"PL_NEWUI": "on", "PL_ATOM": "1"}
        runs = [((binary, work, f"q_pk_closed_{sz}", dict(base, PL_SCRIPT="30:astate")), {"frames": 40, "size": sz}),
                ((binary, work, f"q_pk_closed2_{sz}", dict(base, PL_SCRIPT="30:astate")), {"frames": 40, "size": sz}),
                ((binary, work, f"q_pk_open_{sz}", dict(base, PL_SCRIPT="8:click=top.chip;30:astate")), {"frames": 40, "size": sz})]
        prefetch(runs)
        c, c2, o = (run_app(*a, **k) for a, k in runs)
        dpi = c.window[2]
        pk = o.rects.get("panel.chip")
        check(pk is not None, f"{sz}: the picker opened")
        if not pk:
            continue
        bc, b2, bo = Bmp(c.bmp), Bmp(c2.bmp), Bmp(o.bmp)
        halo = 16 * dpi
        skip = [grow(pk, halo)]
        for name in ("chip",):
            skip.append(grow(o.rects[name], 4)) if name in o.rects else None
        info = tuple(v * dpi for v in box(c, "info"))
        if info[2] > 0:
            skip.append(grow(info, 4))
        card = c.rects.get("card.atom.card")
        if card and overlap(card, pk):                                   # the subject card is left out while the picker would cover it: its place may differ
            skip.append(grow(card, 2))
        def diff(a, b, skips):
            n, first = 0, None
            for y in range(H):
                for x in range(W):
                    if any(r[0] <= x < r[0] + r[2] and r[1] <= y < r[1] + r[3] for r in skips):
                        continue
                    if a.px(x, y) != b.px(x, y):
                        n += 1
                        first = first or (x, y)
            return n, first
        n0, f0 = diff(bc, b2, [])
        check(n0 == 0, f"{sz}: the test itself is steady: two closed runs differ in {n0} pixels (first {f0})")
        n, f = diff(bc, bo, skip)
        check(n == 0, f"{sz}: PIXELS: closed against open, outside the picker, the chip and the info text: {n} pixels differ (first {f}); picker {pk}")
        # nothing was moved (numbers): the atom's centre, the toolbar, the free rectangle
        for key in ("centre", "free"):
            m1 = re.search(key + r"=([-\d.e+]+),([-\d.e+]+)", c.out.split("PLATOMBOX")[-1]); m2 = re.search(key + r"=([-\d.e+]+),([-\d.e+]+)", o.out.split("PLATOMBOX")[-1])
            check(m1 and m2 and m1.groups() == m2.groups(), f"{sz}: the atom's {key} is the same closed and open ({m1 and m1.groups()} {m2 and m2.groups()})")
        check(c.rects.get("toolbar") == o.rects.get("toolbar"), f"{sz}: the toolbar is where it was ({c.rects.get('toolbar')} {o.rects.get('toolbar')})")
        # the picker must not cover the scale bar's label or the subject card's text
        sc = tuple(v * dpi for v in box(o, "scale"))
        check(sc[2] > 0 and not overlap(pk, sc), f"{sz}: the picker {pk} does not cover the scale bar and its label {sc}")
        oc = o.rects.get("card.atom.card")
        check(oc is None or not overlap(pk, oc), f"{sz}: the picker does not cover the subject card {oc} {pk}")
        inf = tuple(v * dpi for v in box(o, "info"))
        check(inf[2] == 0, f"{sz}: the Atom info text is hidden while the picker is open {inf}")
        for other in ("toolbar", "readout", "factline"):
            r_ = o.rects.get(other)
            check(r_ is None or not overlap(pk, r_), f"{sz}: the picker does not cover the {other} {pk} {r_}")
        order = o.note("worder")[0].split()[1:] if o.note("worder") else []
        later = [w for w in order if w.startswith("##card_") or w in ("##shellControls", "##shellHandle", "##shellTool")]
        check("##panel_chip" in order and all(order.index("##panel_chip") > order.index(w) for w in later), f"{sz}: the picker is on top of the cards, the Controls panel and the toolbar ({order[-5:]})")
    # it closes cleanly and the wheel works afterwards
    sz = "1280x800"
    zs = []
    for tag, scr in (("base", "20:move=500,450;24:state;26:wheel=3;30:state"), ("pick", "8:click=top.chip;14:key=escape;20:move=500,450;24:state;26:wheel=3;30:state")):
        r = run_app(binary, work, f"q_pk_wheel_{tag}", {"PL_NEWUI": "on", "PL_ATOM": "1", "PL_SCRIPT": scr}, frames=34, size=sz)
        d = [dict(p.split("=", 1) for p in st.replace("| ", "").split() if "=" in p) for st in r.states]
        zs.append((float(d[0]["zoom"]), float(d[1]["zoom"])) if len(d) == 2 else None)
        if tag == "pick":
            check("panel.chip" not in r.rects, "the picker closed on Escape")
    check(zs[0] and zs[1] and abs(zs[0][1] - zs[0][0]) > 1e-9 and zs[0] == zs[1], f"after the picker was opened and closed the wheel zooms like the baseline {zs}")
    print(f"check_5q picker: {checks} checks, {fails} failed")

# 6: the sliders of the Controls panel are grey in every theme (no accent colour): the filled part and the knob have almost no colour (largest channel spread <= 30 of 255)
def part_sliders(binary, work):
    runs = []
    for t in THEMES:
        th, env = theme_args(t)
        for lab, e2 in (("atom", {"PL_ATOM": "1"}), ("orbit", {})):
            runs.append(((binary, work, f"q_sl_{lab}_{t}", dict(env, **e2, PL_NEWUI="on", PL_SCRIPT="10:wheel=0")), {"frames": 24, "theme": th, "size": "1920x1080"}))
    prefetch(runs)
    for t in THEMES:
        th, env = theme_args(t)
        for lab, e2 in (("atom", {"PL_ATOM": "1"}), ("orbit", {})):
            r = run_app(binary, work, f"q_sl_{lab}_{t}", dict(env, **e2, PL_NEWUI="on", PL_SCRIPT="10:wheel=0"), frames=24, theme=th, size="1920x1080")
            b = Bmp(r.bmp)
            fills = {k[:-5]: v for k, v in r.rects.items() if k.startswith("slider.") and k.endswith(".fill")}
            tested = 0
            for k, f in fills.items():
                kn = r.rects.get(k + ".knob")
                pc = r.rects.get("panel.controls")                          # a slider scrolled out of the panel is clipped away: its rectangle says nothing about pixels
                if f[2] < 8 or not kn or not pc or not (pc[0] <= kn[0] and kn[0] + kn[2] <= pc[0] + pc[2] and pc[1] <= kn[1] and kn[1] + kn[3] <= pc[1] + pc[3]):
                    continue
                fp = b.px(int(f[0] + f[2] / 2), int(f[1] + f[3] / 2))
                kp = b.px(int(kn[0] + kn[2] / 2), int(kn[1] + kn[3] / 2))
                tested += 1
                check(chroma(fp) <= 30, f"{lab} {t}: {k}: the filled part {fp} is grey (colour spread {chroma(fp)})")
                check(chroma(kp) <= 30, f"{lab} {t}: {k}: the knob {kp} is grey (colour spread {chroma(kp)})")
                trk = b.px(int(f[0] + f[2] + 3 + (kn[2] / 2)), int(f[1] + f[3] / 2))
                check(contrast(fp, trk) >= 1.3 or dmax(fp, trk) >= 20, f"{lab} {t}: {k}: the filled part {fp} still shows against the empty track {trk}")
            check(tested >= 1, f"{lab} {t}: at least one slider with a filled part was found ({len(fills)} sliders)")
    print(f"check_5q sliders: {checks} checks, {fails} failed")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"lit": part_lit, "theme": part_theme, "scrollbar": part_scrollbar, "endkey": part_endkey, "picker": part_picker, "sliders": part_sliders}[part](binary, work)
    sys.exit(1 if fails else 0)
