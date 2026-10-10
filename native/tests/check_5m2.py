#!/usr/bin/env python3
# 0.15 stage 5m part 2 / 5n. Checked in the REAL app (Linux, software OpenGL, scripted events; NOT a real mouse, NOT Windows).
#   check_5m2.py <PhysicsLab binary> <work folder> <part>      parts: layout
# The expected numbers are written here by hand (the owner's words), not read from the app's own code.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, overlap, inside

fails = checks = 0

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

ATOM_CHART = {"PL_ATOM": "1", "PL_ATOM_SET": "view=Chart of nuclides"}

def part_layout(binary, work):
    for sz in ("880x560", "1280x800"):
        W, H = (int(v) for v in sz.split("x"))
        for name, extra, leg, card in (("atom", dict(ATOM_CHART, PL_NEWUI="on"), "card.atom.legend", "card.atom.card"),
                                       ("orbit", {"PL_NEWUI": "on", "PL_SELECT": "Earth"}, "card.orbit.legend", "card.orbit.card")):
            closed = run_app(binary, work, f"m2_{name}_{sz}_closed", extra, frames=50, size=sz)
            opened = run_app(binary, work, f"m2_{name}_{sz}_open", dict(extra, PL_SCRIPT="10:click=card.legend"), frames=50, size=sz)
            h = opened.rects.get("handle") or closed.rects.get("handle")
            check(h is not None and h[2] >= 32 - 0.01 and h[3] >= 72 - 0.01, f"{name} {sz}: the handle is at least 32 x 72 px (is {h})")
            check(h is not None and inside(h, W, H), f"{name} {sz}: the handle is inside the window")
            lr = opened.rects.get(leg)
            check(lr is not None, f"{name} {sz}: the Legend opens")
            cr = [v for k, v in opened.rects.items() if k.startswith(card)]
            for c in cr:
                check(lr is None or not overlap(c, lr), f"{name} {sz}: the subject card {c} does not cover the open Legend {lr}")
                if h: check(not overlap(c, h), f"{name} {sz}: the subject card does not touch the handle")
            check(any(k.startswith(card) for k in closed.rects), f"{name} {sz}: with the Legend folded the subject card is shown")
            if sz == "1280x800":     # 330 (Legend) + 300 (card, 20 em) + margins fit beside the 330 px Controls panel in 1280 px: both are shown (arithmetic, not the app's)
                check(len(cr) == 1, f"{name} {sz}: room for both, so both are shown")

def centre_x(r):
    return r[0] + r[2] / 2

def part_tilt(binary, work):
    import re
    for sz in ("880x560", "1280x800"):
        W, H = (int(v) for v in sz.split("x"))
        ch = run_app(binary, work, f"m2_tilt_{sz}_chart", dict(ATOM_CHART, PL_NEWUI="on"), frames=50, size=sz)
        t, tb = ch.rects.get("tiltslider"), ch.rects.get("toolbar")
        check(t is not None and tb is not None, f"{sz}: the tilt slider is shown in the Chart view")
        if t and tb:
            check(abs(centre_x(t) - centre_x(tb)) <= 1.0, f"{sz}: the slider's centre x {centre_x(t):.1f} equals the toolbar's {centre_x(tb):.1f} within 1 px")
            check(inside(t, W, H), f"{sz}: the slider is inside the window")
            check(not overlap(t, tb), f"{sz}: the slider does not cover the toolbar")
            check(t[1] + t[3] <= tb[1], f"{sz}: the slider is above the toolbar")
            # behaviour: a click near the right end of the track sets a high tilt (the Controls panel's own "Tilt" row shows N %), at the left end ~0 %
            def tilt_after(tag, fx):
                x, y = int(t[0] + t[2] * fx), int(t[1] + t[3] / 2)
                x0 = int(t[0] + 66)                                    # the knob at the left end of the track (after "flat")
                r = run_app(binary, work, f"m2_tilt_{sz}_{tag}", dict(ATOM_CHART, PL_NEWUI="on", PL_SCRIPT=f"6:move={x0},{y};10:drag={x0},{y},{x},{y}"), frames=50, size=sz)
                vals = [int(m.group(1)) for _, _, tx in r.texts for m in [re.fullmatch(r"(\d+) %", tx.strip())] if m]
                return vals
            hi = tilt_after("hi", 0.85)
            check(any(v >= 60 for v in hi), f"{sz}: a drag to 85 % of the slider gives a high tilt in the Controls panel (saw {hi})")
        for name, extra in (("cloud3d", {"PL_ATOM": "1"}), ("slice", {"PL_ATOM": "1", "PL_ATOM_SET": "view=Slice"}), ("orbit", {})):
            r = run_app(binary, work, f"m2_tilt_{sz}_{name}", dict(extra, PL_NEWUI="on"), frames=40, size=sz)
            check("tiltslider" not in r.rects, f"{sz}: no tilt slider outside the Chart view ({name})")

def px_stats(bm, rect):
    x0, y0, x1, y1 = int(rect[0]) + 2, int(rect[1]) + 2, int(rect[0] + rect[2]) - 2, int(rect[1] + rect[3]) - 2
    n = card = blue = 0
    for y in range(max(0, y0), min(bm.h, y1), 2):
        for x in range(max(0, x0), min(bm.w, x1), 2):
            r, g, b = bm.px(x, y)
            n += 1
            if (r, g, b) == (23, 23, 26): card += 1               # M1's card colour #17171a
            if b > 200 and r < 160 and b - r > 60: blue += 1       # a blue like the old accent #6c9cff
    return n, card, blue

def part_windows(binary, work):
    from shell_lib import Bmp
    for sz in ("880x560", "1280x800"):
        W, H = (int(v) for v in sz.split("x"))
        for name, extra, rect in (("settings", {"PL_OPEN": "settings"}, "win.settings"), ("about", {"PL_OPEN": "about"}, "reading.column"), ("learn", {"PL_LEARN": "1"}, "reading.column"),
                                  ("history", {"PL_OPEN": "history"}, "reading.column")):
            r = run_app(binary, work, f"m2_win_{name}_{sz}", dict(extra, PL_NEWUI="on"), frames=40, size=sz)
            wr = r.rects.get(rect)
            check(wr is not None, f"{name} {sz}: the window / page is there ({rect})")
            if not wr: continue
            check(inside(wr, W, H), f"{name} {sz}: it lies inside the app window ({wr})")
            n, card, blue = px_stats(Bmp(r.bmp), wr)
            check(card >= 0.4 * n, f"{name} {sz}: PIXELS: M1's flat card colour #17171a fills the window ({card} of {n} samples)")
            if name == "settings":
                check(blue == 0, f"{name} {sz}: PIXELS: no blue left in the window ({blue} blue samples)")

def part_popups(binary, work):
    import re
    for sz in ("880x560", "1280x800"):
        W, H = (int(v) for v in sz.split("x"))
        for lab, extra in (("orbit", {}), ("atom", {"PL_ATOM": "1"})):
            r = run_app(binary, work, f"m2_pop_{lab}_{sz}_facts", dict(extra, PL_NEWUI="on", PL_SCRIPT="10:click=fact.line"), frames=50, size=sz)
            f = r.rects.get("panel.facts")
            check(f is not None and inside(f, W, H), f"{lab} {sz}: the Facts panel is inside the window ({f})")
            need = [n for n in r.notes if n.startswith("panelneed facts")]
            m = re.search(r"need=(\d+) have=(\d+)", need[-1]) if need else None
            check(m is not None and int(m.group(1)) <= int(m.group(2)) + 1, f"{lab} {sz}: the Facts panel is tall enough for its content ({need[-1] if need else 'no note'})")

PARTS = {"layout": part_layout, "tilt": part_tilt, "windows": part_windows, "popups": part_popups}

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    PARTS[part](binary, work)
    print(f"{checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
