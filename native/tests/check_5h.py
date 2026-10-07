#!/usr/bin/env python3
# 0.15 stage 5h: checked in the REAL app (Linux, software OpenGL; not Windows, not a real mouse, not a GPU).
#   check_5h.py <PhysicsLab binary> <work folder> <part>      parts: centre, infosize
# centre (Part 1, item 1): the Atom Lab's drawing centre and its nucleus label ("Uranium-238 · 92p, 146n") are never under the subject card, the Controls panel, the top bar, the toolbar or
#   the fact line, at 3 window sizes, card open / collapsed, panel open / closed, in the views that draw the nucleus label (rects are the app's own, in window pixels = points at scale 1).
# infosize (Part 1, item 2): the Atom info text is drawn at 14 px or more in the New UI (measured from the size the picture asks the font for: PLATOMBOX infopx) and still 13 px with the New UI Off.
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, prefetch, inside, overlap

SIZES = ("1280x800", "880x560", "1920x1080")
fails = checks = 0

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

NUM = r"([-\d.e+]+)"

def box(out, key, n):
    m = re.search(r"PLATOMBOX .*?\b" + key + "=" + ",".join([NUM] * n), out)
    return tuple(float(m.group(i + 1)) for i in range(n)) if m else None

def part_centre(binary, work):
    cases = []
    for sz in SIZES:
        for vn, st in (("cloud3d", "Z=92;N=146;view=Cloud 3D;orb=2p"), ("orbit", "Z=92;N=146;view=Orbit"), ("slice", "Z=92;N=146;view=Slice;orb=3d")):
            for card_open in (True, False):
                for panel_open in (True, False):
                    script = ("" if card_open else "6:click=card.card.collapse;") + ("" if panel_open else "8:click=ctl.handle;") + "40:astate"
                    cases.append((f"{vn} {sz} card {'open' if card_open else 'collapsed'} panel {'open' if panel_open else 'closed'}", sz, card_open,
                                  (binary, work, f"ctr_{vn}_{sz}_{int(card_open)}{int(panel_open)}", {"PL_ATOM": "1", "PL_NEWUI": "on", "PL_ATOM_SET": st, "PL_SCRIPT": script, "PL_LOG_SHELL": "1"})))
    prefetch([(c[3], {"frames": 50, "size": c[1]}) for c in cases])
    for tag, sz, card_open, a in cases:
        r = run_app(*a, frames=50, size=sz)
        centre, label = box(r.out, "centre", 2), box(r.out, "label", 4)
        check(centre is not None, f"{tag}: the drawing centre is logged")
        if centre is None:
            continue
        w, h = r.window[0], r.window[1]
        check(0 <= centre[0] <= w and 0 <= centre[1] <= h, f"{tag}: the centre is inside the window {centre}")
        cardr = r.rects.get("card.atom.card" + ("" if card_open else ".collapsed"))
        check(cardr is not None, f"{tag}: the card is {'open' if card_open else 'collapsed'}")
        for other in ("card", "panel.controls", "topbar", "toolbar", "factline", "handle"):
            o = cardr if other == "card" else r.rects.get(other)
            if o is None:
                continue
            check(not (o[0] <= centre[0] <= o[0] + o[2] and o[1] <= centre[1] <= o[1] + o[3]), f"{tag}: the drawing centre {centre} is not under {other} {o}")
            if label is not None and label[2] > 0:
                check(not overlap(label, o), f"{tag}: the nucleus label {label} is not under {other} {o}")
        if label is not None and label[2] > 0:
            check(inside(label, w, h), f"{tag}: the nucleus label is inside the window {label}")
        elif not tag.startswith("orbit"):
            check(False, f"{tag}: the nucleus label is drawn {label}")

def part_infosize(binary, work):
    for nu, want_min, want_max in (("on", 14, 40), ("off", 13, 13)):
        for sz in SIZES:
            r = run_app(binary, work, f"isz_{nu}_{sz}", {"PL_ATOM": "1", "PL_NEWUI": nu, "PL_ATOM_SET": "Z=92;N=146;view=Orbit", "PL_SCRIPT": "20:astate"}, frames=30, size=sz)
            px = box(r.out, "infopx", 1)
            check(px is not None and want_min - 1e-6 <= px[0] <= want_max + 1e-6, f"New UI {nu} {sz}: the info text's font size is {want_min}..{want_max} px: {px}")

if __name__ == "__main__":
    binary, work = sys.argv[1], sys.argv[2]
    part = sys.argv[3] if len(sys.argv) > 3 else "all"
    for p, f in (("centre", part_centre), ("infosize", part_infosize)):
        if part in (p, "all"):
            f(binary, work)
    print(f"{checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
