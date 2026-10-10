#!/usr/bin/env python3
# 0.15 stage 5l (bug-fix stage 2 of 2). Checked in the REAL app (Linux, software OpenGL, scripted events; NOT a real mouse, NOT Windows). The owner's PC showed bugs the cloud did not, so the cases copy the
# real order: the pointer arrives in one frame and the click comes many frames LATER; the text box is cleared by select-all + Backspace; results are also compared by PIXELS, not only by a logged number.
#   check_5l.py <PhysicsLab binary> <work folder> <part>      parts: search, legend, toolbar, popups
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, prefetch, Bmp

fails = checks = 0

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def only(default):                                       # (for "break it on purpose" runs: L5L_ONLY="1280x800,orbit" runs a part for those sizes / labs only; the real ctest runs set nothing)
    o = os.environ.get("L5L_ONLY")
    return [x for x in default if not o or str(x) in o.split(",")]

def script(*items):
    return ";".join(f"{f}:{c}" for f, c in items)

def region(bm, x0, y0, x1, y1):
    return [bm.px(x, y) for y in range(max(0, y0), min(bm.h, y1)) for x in range(max(0, x0), min(bm.w, x1))]

def diff_count(a, b, tol=24):
    return sum(1 for p, q in zip(a, b) if sum(abs(i - j) for i, j in zip(p, q)) > tol)

# ----------------------------------------------------------------------------------------------- search
# What each Settings row is called in search (the owner's words), per lab. The window's own list of drawn rows must be EXACTLY these (a row drawn but not listed here fails), and each one is typed
# whole (the label) and by one word of it. These labels are written here by hand on purpose: they are the independent expectation, the app builds its list from its own table.
ROWS_ALL = {"text": ("Text size (Settings)", "size"), "font": ("Font", "font"), "units": ("Units (metric or imperial)", "units"), "motion": ("Motion (reduced motion)", "motion"),
            "progress": ("Progress (start again)", "progress"), "developer": ("Developer (unlock everything)", "developer"),
            "newui": ("New UI (preview)", "preview"), "facts": ("Did you know facts", "did"), "perf": ("Performance", "performance"), "vsync": ("VSync (wait for the screen)", "vsync"),
            "frames": ("Frame times (fps)", "frame"), "benchmark": ("Benchmark (speed test)", "benchmark")}
ROWS_LAB = {0: {"gpurocks": ("GPU rocks (OpenGL 4.3)", "rocks")},
            1: {"gpuspin": ("GPU superposition and jumps (OpenGL 4.3)", "superposition"), "gpuoutline": ("GPU outline (OpenGL 4.3)", "outline"), "gpucolours": ("GPU colours (Atom Lab cloud dots)", "colours")}}
LAB_ENV = {0: {}, 1: {"PL_ATOM": "1", "PL_ATOM_SET": "view=Chart of nuclides"}}

def searches(r):
    out = []
    for ln in r.out.splitlines():
        m = re.match(r"PLSEARCH query=(.*?) hits=(.*)$", ln)
        if m:
            out.append((m.group(1), [h for h in m.group(2).split("|") if h]))
    return out

def setrows(r):
    out = []
    for ln in r.out.splitlines():
        m = re.match(r"PLSETROW (\S+) y=(-?[\d.]+) h=([\d.]+) winTop=(-?[\d.]+) winBottom=(-?[\d.]+)", ln)
        if m:
            out.append((m.group(1), *(float(v) for v in m.groups()[1:])))
    return out

def part_search(binary, work):
    for sz in ("880x560", "1280x800"):
        for lab in (0, 1):
            rows = dict(ROWS_ALL, **ROWS_LAB[lab])
            env = dict(LAB_ENV[lab], PL_NEWUI="on")
            # pass 1: the Settings window open, no search: which rows it draws
            r0 = run_app(binary, work, f"l_rows_{lab}_{sz}", dict(env, PL_OPEN="settings"), frames=20, size=sz)
            drawn = [n for n in r0.note("settingrows")]
            ids = drawn[0].split(" ", 1)[1].split(",") if drawn and " " in drawn[0] else []
            check(set(ids) == set(rows), f"{sz} lab {lab}: the Settings window draws exactly the rows the test expects: drawn-only {sorted(set(ids) - set(rows))}, expected-only {sorted(set(rows) - set(ids))}")
            # pass 2: one run, every row: its word, then its whole label, Enter; the next row starts at once (Settings stays open, so the scroll has to travel)
            order = list(reversed(list(rows)))
            items, f = [], 10
            for rid in order:
                label, word = rows[rid]
                items += [(f, "key=ctrl+k"), (f + 3, "text=" + word), (f + 6, "key=escape"), (f + 14, "key=ctrl+k"), (f + 17, "text=" + label), (f + 20, "key=enter")]
                f += 38
            r = run_app(binary, work, f"l_search_{lab}_{sz}", dict(env, PL_SCRIPT=script(*items)), frames=f + 10, size=sz)
            ss = searches(r)
            goto = setrows(r)
            check(len(goto) == len(order), f"{sz} lab {lab}: every row opened Settings at its row ({len(goto)} of {len(order)})")
            for k, rid in enumerate(order):
                label, word = rows[rid]
                lab_hits = [h for q, h in ss if q == label]
                word_hits = [h for q, h in ss if q == word]
                check(any(label in h for h in lab_hits), f"{sz} lab {lab}: typing the label of '{rid}' ({label!r}) finds it: {lab_hits[:1]}")
                check(any(h and h[0] == label for h in lab_hits), f"{sz} lab {lab}: the label of '{rid}' is the FIRST result")
                check(any(label in h for h in word_hits), f"{sz} lab {lab}: typing the word {word!r} finds '{rid}': {[x[:4] for x in word_hits[:1]]}")
                if k < len(goto):
                    g = goto[k]
                    check(g[0] == rid, f"{sz} lab {lab}: row {k} opened is '{rid}' (got {g[0]})")
                    check(g[3] <= g[1] and g[1] + g[2] <= g[4] + 1, f"{sz} lab {lab}: '{rid}' is inside the Settings window when it opens (row y {g[1]:.0f}..{g[1] + g[2]:.0f}, window {g[3]:.0f}..{g[4]:.0f})")
    part_search_follow(binary, work)
    part_highlight(binary, work)

def band(r0, bm):
    p = r0.rects.get("panel.search")
    if not p:
        return None
    x0, y0, w, h = int(p[0]), int(p[1]), int(p[2]), int(p[3])
    return (x0 + 4, y0 + int(h * 0.45), x0 + w - 4, y0 + h - 2), p

def part_search_follow(binary, work):
    # the list follows the text, in the real order: type -> result; select all + Backspace -> nothing and the hint; type another -> replaced; choose -> reopen shows nothing from before
    sz = "1280x800"
    env = {"PL_NEWUI": "on"}
    def go(name, items, frames):
        return run_app(binary, work, name, dict(env, PL_SCRIPT=script(*items)), frames=frames, size=sz)
    empty = go("l_f_empty", [(10, "key=ctrl+k")], 30)
    typed = go("l_f_moo", [(10, "key=ctrl+k"), (14, "text=moo")], 30)
    cleared = go("l_f_clear", [(10, "key=ctrl+k"), (14, "text=moo"), (24, "key=ctrl+a"), (28, "key=backspace")], 44)
    check(typed.note("searchcount") and int(typed.note("searchcount")[0].split()[1]) > 0, "typing 'moo' gives results: " + str(typed.note("searchcount")))
    check(cleared.note("searchcount") == ["searchcount 0"], "select all + Backspace: no results left: " + str(cleared.note("searchcount")))
    check(not cleared.note("searchhit"), "select all + Backspace: no top hit is kept: " + str(cleared.note("searchhit")))
    check(not any(q == "" and h for q, h in searches(cleared)), "an empty box never lists results")
    be = band(empty, Bmp(empty.bmp))
    if be:
        (x0, y0, x1, y1), pe = be
        pc = cleared.rects.get("panel.search")
        check(pc is not None and abs(pc[3] - pe[3]) < 1.5, f"the box is as small again after clearing as when fresh (height {pc and pc[3]} vs {pe[3]})")
        check(diff_count(region(Bmp(empty.bmp), x0, y0, x1, y1), region(Bmp(cleared.bmp), x0, y0, x1, y1)) == 0, "PIXELS: below the text box, a cleared search looks exactly like a fresh one (no old result)")
        check(diff_count(region(Bmp(empty.bmp), x0, y0, x1, y1), region(Bmp(typed.bmp), x0, y0, x1, y1)) > 200, "PIXELS: with 'moo' typed the area below the box does show results (the comparison can see them)")
    # Ctrl+K again with the box open starts a new search: the old words are gone (before 5l they came back in front of the new ones: "moopluto")
    again2 = go("l_f_ctrlk", [(10, "key=ctrl+k"), (14, "text=moo"), (20, "key=ctrl+k"), (26, "text=pluto")], 40)
    qs = [q for q, _ in searches(again2)]
    check("pluto" in qs and "moopluto" not in qs, "Ctrl+K with the box open clears the old text: " + str(qs))
    # typing something else replaces the results
    other = go("l_f_other", [(10, "key=ctrl+k"), (14, "text=moo"), (24, "key=ctrl+a"), (28, "text=pluto")], 44)
    hits = [h for q, h in searches(other) if q == "pluto"]
    check(hits and hits[-1] and hits[-1][0] == "Pluto" and not any("moo" in x.lower() for x in hits[-1][:1]), "typing 'pluto' after 'moo' replaces the results: " + str(hits[-1:] if hits else hits))
    # choose a result, reopen: nothing from before is listed (before 5l: the recent choice came back as a result)
    again = go("l_f_again", [(10, "key=ctrl+k"), (14, "text=moo"), (18, "key=enter"), (30, "key=ctrl+k")], 46)
    check(again.note("searchcount") == ["searchcount 0"], "after choosing a result, a new search starts empty: " + str(again.note("searchcount")))
    if be:
        (x0, y0, x1, y1), _ = be
        check(diff_count(region(Bmp(typed.bmp), x0, y0, x1, y1), region(Bmp(again.bmp), x0, y0, x1, y1)) > 200, "PIXELS: a new search after a choice shows no result rows (the box below the text is not the one with results)")
    # the owner's two cases: the Did you know facts setting, and a Moon fact
    for what, q in (("did you know", "did you know"), ("a Moon fact", "the moon")):
        a = go("l_f_dyk_" + q.replace(" ", ""), [(10, "key=ctrl+k"), (14, "text=" + q)], 30)
        hs = [h for qq, h in searches(a) if qq == q]
        check(hs and hs[-1], f"'{q}' finds something ({what}): {hs[-1:] if hs else hs}")
        if q == "did you know":
            check(hs and "Did you know facts" in hs[-1], "'did you know' finds the Settings row 'Did you know facts': " + str(hs[-1:] if hs else hs))
        b = go("l_f_dykc_" + q.replace(" ", ""), [(10, "key=ctrl+k"), (14, "text=" + q), (24, "key=enter"), (40, "key=ctrl+k"), (44, "text=" + q), (54, "key=ctrl+a"), (58, "key=backspace")], 72)
        check(b.note("searchcount") == ["searchcount 0"] and not b.note("searchhit"), f"{what}: choose it, search again, delete the text: no result stays: {b.note('searchcount')}")
        ref = go("l_f_ref_" + q.replace(" ", ""), [(10, "key=ctrl+k"), (14, "text=" + q), (24, "key=enter"), (40, "key=ctrl+k")], 72)      # the same scene, search just opened, nothing typed
        pr = ref.rects.get("panel.search")
        if pr:
            x0, y0, x1, y1 = int(pr[0]) + 4, int(pr[1] + pr[3] * 0.45), int(pr[0] + pr[2]) - 4, int(pr[1] + pr[3]) - 2
            check(diff_count(region(Bmp(ref.bmp), x0, y0, x1, y1), region(Bmp(b.bmp), x0, y0, x1, y1)) == 0, f"PIXELS ({what}): after deleting the text the search area looks like a search just opened (no old result)")
        else:
            check(False, f"({what}) the reference search panel is not open")
    # Esc and a click away close the box (the click comes many frames after the pointer: the pointer moves at frame 20, the click is at frame 30)
    esc = go("l_f_esc", [(10, "key=ctrl+k"), (14, "text=moo"), (24, "key=escape")], 34)
    check("panel.search" not in esc.rects, "Esc closes the search box")
    away = go("l_f_away", [(10, "key=ctrl+k"), (14, "text=moo"), (20, "move=300,300"), (30, "clickxy=300,300")], 44)
    check("panel.search" not in away.rects, "a click on the picture closes the search box (pointer moved 10 frames before the click)")
    bmA, bmE = Bmp(away.bmp), Bmp(go("l_f_none", [], 44).bmp)
    check(diff_count(region(bmA, 400, 60, 880, 400), region(bmE, 400, 60, 880, 400)) < 300, "PIXELS: after the click away, no search panel is drawn on the picture")

def part_highlight(binary, work):
    # search opens Settings at the row, scrolled into view and highlighted for a moment (compared by pixels while it is lit and 130 frames later)
    sz = "880x560"
    env = {"PL_NEWUI": "on"}
    seq = [(10, "key=ctrl+k"), (14, "text=benchmark"), (20, "key=enter")]
    lit = run_app(binary, work, "l_hl_lit", dict(env, PL_SCRIPT=script(*seq)), frames=40, size=sz)
    off = run_app(binary, work, "l_hl_off", dict(env, PL_SCRIPT=script(*seq)), frames=200, size=sz)
    rl, ro = lit.rects.get("setting.benchmark"), off.rects.get("setting.benchmark")
    check(rl is not None and ro is not None, "the Benchmark row's place is logged")
    if rl and ro:
        check(abs(rl[1] - ro[1]) < 2, f"the row has stopped moving by the time the highlight has gone ({rl[1]:.0f} vs {ro[1]:.0f})")
        x0, y0, x1, y1 = int(rl[0]) + 20, int(rl[1]) + 2, int(rl[0] + rl[2]) - 20, int(rl[1] + rl[3]) - 2
        n = diff_count(region(Bmp(lit.bmp), x0, y0, x1, y1), region(Bmp(off.bmp), x0, y0, x1, y1), tol=12)
        total = max(1, (x1 - x0) * (y1 - y0))
        check(n > 0.5 * total, f"PIXELS: the row is tinted while the highlight lasts ({n} of {total} pixels differ from the same place later)")

# ----------------------------------------------------------------------------------------------- legend
def free_point(r, sz):
    W, H = (int(v) for v in sz.split("x"))
    occ = [v for k, v in r.rects.items() if re.fullmatch(r"occ\d+", k)] + [v for k, v in r.rects.items() if k.startswith("card.")]
    for fy in (0.55, 0.45, 0.65, 0.35, 0.75):
        for fx in (0.5, 0.4, 0.3, 0.6, 0.2, 0.7):
            x, y = int(W * fx), int(H * fy)
            if not any(v[0] - 8 <= x <= v[0] + v[2] + 8 and v[1] - 8 <= y <= v[1] + v[3] + 8 for v in occ):
                return f"{x},{y}"
    return None

def only_cases(cases):
    o = os.environ.get("L5L_ONLY")
    return [c for c in cases if not o or c[0] in o.split(",")]

def part_legend(binary, work):
    cases = (("orbit", {}, "card.orbit.legend"), ("atomchart", {"PL_ATOM": "1", "PL_ATOM_SET": "view=Chart of nuclides"}, "card.atom.legend"))
    for sz in only(("880x560", "1280x800")):
        for name, extra, rect in only_cases(cases):
            env = dict(extra, PL_NEWUI="on")
            def go(tag, items, frames):
                return run_app(binary, work, f"l_lg_{name}_{sz}_{tag}", dict(env, PL_SCRIPT=script(*items)), frames=frames, size=sz)
            def is_open(r):
                return rect in r.rects and rect + ".collapsed" not in r.rects
            def is_closed(r):
                return rect + ".collapsed" in r.rects
            closed0 = go("closed", [], 50)
            opened = go("open", [(10, "click=card.legend")], 50)
            check(is_closed(closed0), f"{name} {sz}: the Legend starts folded")
            check(is_open(opened), f"{name} {sz}: a click on the Legend title opens it")
            pr = opened.rects.get(rect)
            pc = closed0.rects.get(rect + ".collapsed")
            if not (pr and pc):
                continue
            ri = (int(pr[0]), int(pr[1]), int(pr[0] + pr[2]), int(pr[1] + pr[3]))
            clickp = free_point(opened, sz)                                           # a spot of the picture that no bar, panel or card covers
            check(clickp is not None, f"{name} {sz}: a free spot of the picture exists for the click")
            if clickp is None:
                continue
            # outside click on the picture: the pointer arrives at frame 24, the click is at frame 34 (the real order), then again: open, outside click
            out1 = go("out1", [(10, "click=card.legend"), (24, "move=" + clickp), (34, "clickxy=" + clickp)], 52)
            check(is_closed(out1), f"{name} {sz}: a click on the picture closes the Legend (pointer {10} frames before the click)")
            ctl = go("ctl", [(24, "move=" + clickp), (34, "clickxy=" + clickp)], 52)                 # the same pointer and click, Legend never opened (the click itself may change the picture)
            same = diff_count(region(Bmp(out1.bmp), *ri), region(Bmp(ctl.bmp), *ri))
            check(same == 0, f"{name} {sz}: PIXELS: after the click the Legend's place looks like the never-opened one ({same} pixels differ)")
            openpx = diff_count(region(Bmp(opened.bmp), *ri), region(Bmp(closed0.bmp), *ri))
            check(openpx > 300, f"{name} {sz}: PIXELS: the open Legend looks different from the folded one ({openpx}), so the comparison can see it")
            # click inside: stays open (twice)
            ix, iy = int(pr[0] + pr[2] / 2), int(pr[1] + pr[3] * 0.6)
            ins = go("in", [(10, "click=card.legend"), (24, "move=%d,%d" % (ix, iy)), (34, "clickxy=%d,%d" % (ix, iy)), (44, "clickxy=%d,%d" % (ix + 3, iy + 2))], 60)
            check(is_open(ins), f"{name} {sz}: a click inside the Legend keeps it open (twice)")
            # open, outside, open again, outside; then open, top bar click
            rep = go("rep", [(10, "click=card.legend"), (22, "clickxy=" + clickp), (34, "click=card.legend"), (46, "clickxy=" + clickp), (58, "click=card.legend")], 72)
            check(is_open(rep), f"{name} {sz}: open - outside - open - outside - open ends open")
            rep2 = go("rep2", [(10, "click=card.legend"), (22, "clickxy=" + clickp), (34, "click=card.legend"), (46, "clickxy=" + clickp)], 62)
            check(is_closed(rep2), f"{name} {sz}: open - outside - open - outside ends folded")
            bar = go("bar", [(10, "click=card.legend"), (24, "click=top.theme")], 46)
            check(is_closed(bar), f"{name} {sz}: a click in the top bar closes the Legend")
            win = go("win", [(8, "click=top.settings"), (20, "click=card.legend"), (36, "clickxy=%d,%d" % (int(sz.split('x')[0]) // 2, int(sz.split('x')[1]) // 2))], 56)
            check("win.settings" in win.rects and is_closed(win), f"{name} {sz}: a click in another window (Settings) closes the Legend")
            esc = go("esc", [(10, "click=card.legend"), (24, "key=escape")], 40)
            check(is_closed(esc), f"{name} {sz}: Esc closes the Legend")
            other = [v for k, v in opened.rects.items() if k.startswith("card.") and k != rect and ".collapsed" not in k and v[0] < pr[0] + pr[2] and pr[0] < v[0] + v[2] and v[1] < pr[1] + pr[3] and pr[1] < v[1] + v[3]]
            if other:
                print(f"NOTE {name} {sz}: another card lies over the open Legend here (its arrow is under it): the arrow check is skipped; a click on that card closes the Legend, checked below")
                over = go("overcard", [(10, "click=card.legend"), (24, "move=%d,%d" % (other[0][0] + other[0][2] / 2, other[0][1] + other[0][3] / 2)), (34, "clickxy=%d,%d" % (other[0][0] + other[0][2] / 2, other[0][1] + other[0][3] / 2))], 52)
                check(is_closed(over), f"{name} {sz}: a click on the card lying over the Legend closes the Legend")
            else:
                arrow = go("arrow", [(10, "click=card.legend"), (24, "click=card.legend.collapse")], 40)
                check(is_closed(arrow), f"{name} {sz}: its own arrow still closes it")

# ----------------------------------------------------------------------------------------------- toolbar
def lit(r):
    n = r.note("toolon")
    return [x for x in (n[0][len("toolon "):].split(",") if n else []) if x]

def platom(r):                                           # where the Atom picture drew its centre (points), from the `astate` verb; the chart has no single centre: the middle of the free part, free=x,w
    for ln in r.out.splitlines():
        m = re.search(r"PLATOMBOX .* centre=([-\d.e+]+),([-\d.e+]+) .* free=([-\d.e+]+),([-\d.e+]+)", ln)
        if m:
            return float(m.group(1)), float(m.group(2)), float(m.group(3)) + float(m.group(4)) / 2
    return None

def part_toolbar(binary, work):
    # (a) one view, one lit button. Atom Lab: every view; the lit buttons come from the toolbar itself, and are checked again by PIXELS (a lit button has the accent colour on its face)
    atom_views = (("Orbit", ["tool.atom"]), ("Cloud", ["tool.cloud"]), ("Cloud 3D", ["tool.cloud3d"]), ("Slice", ["tool.slice"]), ("Chart of nuclides", ["tool.chart"]))
    for sz in ("880x560", "1280x800"):
        for view, want in atom_views:
            r = run_app(binary, work, f"l_tb_{view.replace(' ', '')}_{sz}", {"PL_NEWUI": "on", "PL_ATOM": "1", "PL_ATOM_SET": "view=" + view}, frames=30, size=sz)
            check(lit(r) == want, f"{sz} Atom view {view}: the lit toolbar buttons are exactly {want}, got {lit(r)}")
            bm = Bmp(r.bmp)
            btn = {k[len("toolbtn."):]: v for k, v in r.rects.items() if k.startswith("toolbtn.")}
            def face(v):
                return bm.px(int(v[0]) + 3, int(v[1] + v[3] / 2))
            tool = [k for k in btn if k.startswith("tool.") and k not in ("tool.reset", "tool.drag", "tool.centre")]
            base = [face(btn[k]) for k in tool if k not in want]
            if base and want and want[0] in btn:
                spread = max(sum(abs(a - b) for a, b in zip(base[0], c)) for c in base)
                gap = sum(abs(a - b) for a, b in zip(face(btn[want[0]]), base[0]))
                check(spread <= 30 and gap > 60, f"{sz} Atom view {view}: PIXELS: the lit button's face differs from the others' (by {gap}; the others differ among themselves by {spread})")
        # Orbit Lab: the 2D / 3D pair is one segmented control: one of the two is chosen, no button is lit
        for v3 in ("2D", "3D"):
            r = run_app(binary, work, f"l_tb_orbit_{v3}_{sz}", {"PL_NEWUI": "on", "PL_SCRIPT": "8:click=tool.view." + v3.lower()}, frames=24, size=sz)
            check(lit(r) == [], f"{sz} Orbit {v3}: no toolbar button is lit as well ({lit(r)})")
    # (d) no see-through rectangle inside the pill: along a row just under the pill's top, the colour does not jump where the (square) top-light gradient used to end, 10 px inside the pill's right end
    for lab, extra in ((0, {}), (1, {"PL_ATOM": "1", "PL_ATOM_SET": "view=Cloud 3D"})):
        r = run_app(binary, work, f"l_tb_rect_{lab}", dict(extra, PL_NEWUI="on"), frames=30, size="1280x800")
        tb = r.rects["toolbar"]
        bm = Bmp(r.bmp)
        for dy in (10, 11, 12):
            xe = int(tb[0] + tb[2])                       # the right end: the last button has no face, and the old gradient's square end was 10 px inside the pill's end
            a_, b_ = bm.px(xe - 9, int(tb[1]) + dy), bm.px(xe - 11, int(tb[1]) + dy)
            check(max(abs(p - q) for p, q in zip(a_, b_)) <= 4, f"lab {lab}: PIXELS: no rectangle edge inside the toolbar pill's end (row {dy}: {a_} vs {b_})")
    # (b) the Nucleus button is gone; the same function is in Controls > View and works
    r = run_app(binary, work, "l_tb_nonuc", {"PL_NEWUI": "on", "PL_ATOM": "1", "PL_SCRIPT": "6:state;10:click=nucleus.nucleus;50:state"}, frames=56, size="1280x800")
    check("toolbtn.tool.nucleus" not in r.rects and "tool.nucleus" not in "".join(r.note("toolon")), "the bottom toolbar has no Nucleus button")
    zs = [float(re.search(r"zoom=([\d.e+-]+)", x).group(1)) for x in r.states if "zoom=" in x]
    check(len(zs) == 2 and zs[0] < 2 <= zs[1] - 1 + 1 and zs[1] > zs[0] * 2, f"Controls > View > Nucleus zooms in to the nucleus (zoom {zs})")
    # (c) centred on the scene
    for sz in only(("880x560", "1109x882", "1920x1080")):
        for lab, views in ((0, (("2D", "tool.view.2d"), ("3D", "tool.view.3d"))), (1, [(v, None) for v, _ in atom_views])):
            for ctl in ("open", "closed"):
                for view, click in views:
                    env = {"PL_NEWUI": "on"}
                    items = []
                    if lab == 1:
                        env.update(PL_ATOM="1", PL_ATOM_SET="view=" + view)
                    else:
                        items.append((6, "click=" + click))
                    if ctl == "closed":
                        items.append((12, "click=ctl.close"))
                    items.append((50, "astate"))
                    r = run_app(binary, work, f"l_tbc_{lab}_{view.replace(' ', '')}_{ctl}_{sz}", dict(env, PL_SCRIPT=script(*items)), frames=60, size=sz)
                    W, H, dpi, ts = r.window
                    tb, dock = r.rects.get("toolbar"), r.rects.get("panel.controls")
                    if not tb:
                        check(False, f"lab {lab} {view} {ctl} {sz}: no toolbar rect")
                        continue
                    if lab == 0:
                        sx = W / 2.0                      # the Sun is drawn at W / 2 (orbit_view.cpp)
                    else:
                        pc = platom(r)
                        sx = ((pc[2] if view == "Chart of nuclides" else pc[0]) * dpi) if pc else None
                    cx = tb[0] + tb[2] / 2
                    tag = f"lab {lab} {view} controls {ctl} {sz}"
                    if sx is None:
                        check(False, f"{tag}: the scene's centre was not reported")
                        continue
                    if abs(cx - sx) > 1.5 and ((ctl == "open" and dock) or tb[0] <= 14 * dpi):
                        # centred as far as the room allows: pushed against the Controls panel (its left edge minus the gap), or against the window's left edge when the scene's centre is nearer to it than half the toolbar
                        ok = (dock and tb[0] + tb[2] <= dock[0] and tb[0] + tb[2] >= dock[0] - 16 * dpi) or tb[0] <= 14 * dpi
                        check(ok, f"{tag}: toolbar centre {cx:.1f} vs scene centre {sx:.1f}: allowed only if pushed against the Controls panel or the window edge (toolbar {tb[0]:.1f}..{tb[0] + tb[2]:.1f}, panel {dock and dock[0]})")
                        print(f"NOTE {tag}: not exactly centred: pushed against the {'window edge' if tb[0] <= 14 * dpi else 'Controls panel'} (toolbar centre {cx:.1f}, scene centre {sx:.1f})")
                    else:
                        check(abs(cx - sx) <= 1.5, f"{tag}: the toolbar's centre ({cx:.1f}) is the scene's centre ({sx:.1f})")

# ----------------------------------------------------------------------------------------------- popups
def part_popups(binary, work):
    for sz in ("880x560", "1280x800"):
        for lab, extra in ((0, {}), (1, {"PL_ATOM": "1"})):
            for name, click, panel in (("labs", "top.lab", "labmenu"), ("menu", "top.menu", "menu"), ("chip", "top.chip", "chip")):
                r = run_app(binary, work, f"l_pop_{name}_{lab}_{sz}", dict(extra, PL_NEWUI="on", PL_SCRIPT=script((10, "click=" + click))), frames=30, size=sz)
                n = [x for x in r.notes if x.startswith("panelneed " + panel + " ")]
                m = re.search(r"need=(\d+) have=(\d+) scroll=(\d+)", n[0]) if n else None
                if not m:
                    check(False, f"{sz} lab {lab} {name}: no size line ({n})")
                    continue
                need, have, sc = (int(v) for v in m.groups())
                room_limited = need > have + 1                    # it truly does not fit the room: a scrollbar is allowed then
                check(room_limited or (sc == 0 and abs(need - have) <= 2), f"{sz} lab {lab} {name}: the popup fits its content (need {need}, have {have}, scrollable range {sc})")
                if room_limited:
                    print(f"NOTE {sz} lab {lab} {name}: does not fit the room (need {need}, have {have}): scrolls, as allowed")
    for sz in ("880x560", "1280x800"):
        r = run_app(binary, work, f"l_pop_theme_{sz}", {"PL_NEWUI": "on", "PL_SCRIPT": script((10, "click=top.theme"))}, frames=30, size=sz)
        n = r.note("popupscroll themes")
        check(n and n[0].endswith("max=0"), f"{sz} the Theme list has nothing to scroll: {n}")
        r = run_app(binary, work, f"l_pop_contents_{sz}", {"PL_NEWUI": "on", "PL_OPEN": "about", "PL_SCRIPT": script((10, "click=##readContents"))}, frames=30, size=sz)
        n = r.note("popupscroll contents")
        if n:
            m = re.search(r"max=(\d+) need=(\d+) have=(\d+)", n[0])
            mx, need, have = (int(v) for v in m.groups())
            check((mx == 0 and abs(need - have) <= 2) or need > have + 1, f"{sz} the Contents dropdown fits its list (need {need}, have {have}, scroll {mx})")
        else:
            print(f"NOTE {sz}: no Contents dropdown (the page has a Contents rail at this size)")
    # the Labs dropdown is opaque and in front of everything: its pixels do not depend on what is behind it; the window order has it last
    for sz in ("880x560", "1280x800"):
        for lab, extra in ((0, {}), (1, {"PL_ATOM": "1", "PL_ATOM_SET": "view=Chart of nuclides"})):
            runs = []
            for k, bgscript in enumerate(("", "6:click=tool.view.3d" if lab == 0 else "6:click=tool.cloud")):
                sc = (bgscript + ";" if bgscript else "") + "14:click=top.lab"
                runs.append(run_app(binary, work, f"l_lab_{lab}_{sz}_{k}", dict(extra, PL_NEWUI="on", PL_SCRIPT=sc), frames=34, size=sz))
            a, b = runs
            pr = a.rects.get("panel.labmenu")
            check(pr is not None and b.rects.get("panel.labmenu") == pr, f"{sz} lab {lab}: the Labs dropdown opens at the same place over both pictures")
            order = [x for x in a.note("worder")[0].split()[1:]] if a.note("worder") else []
            wl = "##panel_labmenu"
            check(wl in order and order.index(wl) > max([order.index(w) for w in order if w.startswith("##card_") or w in ("##shellControls", "##shellHandle", "##shellTool")] or [-1]), f"{sz} lab {lab}: the Labs dropdown is in front of the cards, the Controls panel and the toolbar ({order[-4:]})")
            if pr:
                x0, y0, x1, y1 = int(pr[0]) + 4, int(pr[1]) + 4, int(pr[0] + pr[2]) - 4, int(pr[1] + pr[3]) - 4   # 5q: inset 4 (was 3: 3 px in reaches the anti-aliased edge of the radius-10 corner; the owner approved this one change)
                d = diff_count(region(Bmp(a.bmp), x0, y0, x1, y1), region(Bmp(b.bmp), x0, y0, x1, y1), tol=6)
                check(d == 0, f"{sz} lab {lab}: PIXELS: nothing shows through the Labs dropdown ({d} pixels depend on the picture behind it)")

PARTS = {"search": part_search, "legend": part_legend, "toolbar": part_toolbar, "popups": part_popups, "follow": lambda b, w: part_search_follow(b, w), "highlight": lambda b, w: part_highlight(b, w)}

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    PARTS[part](binary, work)
    print(f"{checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
