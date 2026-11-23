#!/usr/bin/env python3
# 0.15 stage 5m part 2 / 5n. Checked in the REAL app (Linux, software Vulkan, scripted events; NOT a real mouse, NOT Windows).
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
        check(t is not None and tb is not None, f"{sz}: the 2D | 3D switch is shown in the Chart view")
        if t and tb:
            check(abs(centre_x(t) - centre_x(tb)) <= 1.0, f"{sz}: the slider's centre x {centre_x(t):.1f} equals the toolbar's {centre_x(tb):.1f} within 1 px")
            check(inside(t, W, H), f"{sz}: the slider is inside the window")
            check(not overlap(t, tb), f"{sz}: the slider does not cover the toolbar")
            check(t[1] + t[3] <= tb[1], f"{sz}: the slider is above the toolbar")
            # 0.40: the pill is a 2D | 3D switch (it was a Tilt slider). 3D, then a drag across the picture turns the chart round, a long drag down tips it only
            # to 85 degrees (5 above the floor: never under it), a long drag up only to looking straight down
            def cam(tag, script):
                r = run_app(binary, work, f"m2_cam_{sz}_{tag}", dict(ATOM_CHART, PL_NEWUI="on", PL_SCRIPT=script), frames=40, size=sz)
                n = [x for x in r.notes if x.startswith("chartcam ")]
                check(not r.scripted_missing() and bool(n), f"{sz} {tag}: the switch is there and the camera is reported ({r.scripted_missing()[:1]})")
                return dict(kv.split("=") for kv in n[-1].split()[1:]) if n else {}
            cx, cy = int(W * 0.2), int(H * 0.5)              # (left of the cards and the panel)
            c0 = cam("3d", "8:click=chart.3d")
            check(float(c0.get("tilt", -1)) == 1, f"{sz}: 3D is on after clicking it ({c0})")
            c1 = cam("turn", f"8:click=chart.3d;14:drag={cx},{cy},{cx - 200},{cy}")
            check(c1 and abs(float(c1["yaw"]) - float(c0.get("yaw", 0))) > 20, f"{sz}: a drag across turns the chart ({c0.get('yaw')} -> {c1.get('yaw')})")
            lo, hi = int(H * 0.25), int(H * 0.72)            # (above the toolbar and the 2D | 3D pill at every size)
            c2 = cam("side", f"8:click=chart.3d;14:drag={cx},{hi},{cx},{lo};20:drag={cx},{hi},{cx},{lo};26:drag={cx},{hi},{cx},{lo}")   # drag up: towards the side view
            c3 = cam("top", f"8:click=chart.3d;14:drag={cx},{lo},{cx},{hi};20:drag={cx},{lo},{cx},{hi}")                               # drag down: towards straight down
            check(c2 and float(c2["pitch"]) == 85, f"{sz}: dragging up tips the chart only to 5 degrees above its floor, never under it ({c2.get('pitch')})")
            check(c3 and float(c3["pitch"]) == 0, f"{sz}: dragging down stops at looking straight down ({c3.get('pitch')})")
            c4 = cam("back", f"8:click=chart.3d;14:drag={cx},{cy},{cx - 200},{cy};20:click=chart.2d")
            check(c4 and float(c4["tilt"]) == 0, f"{sz}: 2D brings back the flat chart ({c4})")
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
            if (r, g, b) == (22, 23, 26): card += 1               # the 0.38 look's Dark card colour #16171a (M1's was #17171a; changed on purpose)
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
            check(card >= 0.4 * n, f"{name} {sz}: PIXELS: the flat card colour #16171a fills the window ({card} of {n} samples)")
            if name == "settings":
                # 0.38 (the owner's new look; reference changed on purpose): blue only marks the chosen tiles (their 1 px accent edge), never a filled block (it was "no blue at all")
                check(blue <= 0.02 * n, f"{name} {sz}: PIXELS: blue only on the chosen tiles' edges ({blue} blue samples of {n}, at most 2 %)")

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
