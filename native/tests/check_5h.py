#!/usr/bin/env python3
# 0.15 stage 5h: checked in the REAL app (Linux, software OpenGL; not Windows, not a real mouse, not a GPU).
#   check_5h.py <PhysicsLab binary> <work folder> <part>      parts: centre, infosize, cards, fit
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

TABS = ("spectrum", "lifetimes", "size", "temperature", "radial")
SUBJECTS = (("hydrogen", "Z=1;N=0"), ("helium4", "Z=2;N=2"), ("uranium238", "Z=92;N=146"), ("iron56", "Z=26;N=30"))

def card_lines(out, tag):
    # every "PLATOMCARD card.<tag>\tstring\tstring ..." the app printed (it prints a card's strings once per change); the first one is the card as it first showed
    got = [ln.split("\t")[1:] for ln in out.splitlines() if ln.startswith("PLATOMCARD card." + tag + "\t") or ln == "PLATOMCARD card." + tag]
    return got[0] if got else None

def part_cards(binary, work):
    # Part 2: the data sheet's tabs for the old panel's last cards show EXACTLY what the old panel shows (the strings and the numbers behind the two graphs), for 4 subjects in a cloud view
    # (all five cards) and in the Orbit view (no radial graph: the old panel has none there). The OLD run is the old layout (New UI Off): its panel prints its cards' strings with PL_LOG_CARD.
    seq = "10:click=card.all;" + ";".join(f"{16 + 8 * i}:click=data.tab.{t}" for i, t in enumerate(TABS))
    for nm, zn in SUBJECTS:
        for vn, view in (("cloud3d", "view=Cloud 3D;orb=2p"), ("orbit", "view=Orbit")):
            st = zn + ";" + view
            old = run_app(binary, work, f"cd_old_{nm}_{vn}", {"PL_ATOM": "1", "PL_NEWUI": "off", "PL_ATOM_SET": st, "PL_LOG_CARD": "1"}, frames=40)
            new = run_app(binary, work, f"cd_new_{nm}_{vn}", {"PL_ATOM": "1", "PL_NEWUI": "on", "PL_ATOM_SET": st, "PL_LOG_CARD": "1", "PL_SCRIPT": seq, "PL_LOG_SHELL": "1"}, frames=70)
            for t in TABS:
                tag = f"{nm} {vn} {t}"
                if t == "radial" and vn == "orbit":
                    check(card_lines(old.out, t) is None, f"{tag}: the old panel has no radial graph in the Orbit view")
                    check(card_lines(new.out, t) is None, f"{tag}: the sheet shows no radial card in the Orbit view")
                    continue
                o, n = card_lines(old.out, t), card_lines(new.out, t)
                check(o is not None and len(o) > 1, f"{tag}: the old panel printed its card ({None if o is None else len(o)} strings)")
                check(n is not None, f"{tag}: the sheet printed its card")
                if o is not None and n is not None:
                    check(o == n, f"{tag}: the sheet's strings and graph numbers equal the old panel's ({len(n)} vs {len(o)})" + ("" if o == n else f"\n  only sheet {[x for x in n if x not in o][:3]}\n  only old {[x for x in o if x not in n][:3]}"))
                if t in ("spectrum", "radial"):
                    check(o is not None and any(x.startswith("graph ") for x in o), f"{tag}: the old panel's graph numbers were captured")
    # tabs: each is there exactly when the old panel shows its card
    for vn, view, want_radial in (("cloud3d", "view=Cloud 3D;orb=2p", True), ("orbit", "view=Orbit", False), ("chart", "view=Chart of nuclides", False)):
        r = run_app(binary, work, f"cd_tabs_{vn}", {"PL_ATOM": "1", "PL_NEWUI": "on", "PL_ATOM_SET": "Z=92;N=146;" + view, "PL_SCRIPT": seq, "PL_LOG_SHELL": "1"}, frames=70)
        miss = " ".join(r.scripted_missing())
        for t in TABS:
            want = want_radial if t == "radial" else True
            check((("data.tab." + t) not in miss) == want, f"{vn}: the {t} tab is {'shown' if want else 'not shown'} ({miss!r})")

def part_fit(binary, work):
    tall_seen = {}
    # Part 2: the sheet stays solid, inside the window, out from under the Controls panel and scrolls inside itself (wheel, PageDown) when a tab is taller than the window; no text cut by "..."
    for sz in SIZES:
        for t in TABS:
            w, h = (int(v) for v in sz.split("x"))
            r0 = run_app(binary, work, f"ft_{t}_{sz}_0", {"PL_ATOM": "1", "PL_NEWUI": "on", "PL_ATOM_SET": "Z=92;N=146;view=Cloud 3D;orb=2p", "PL_SCRIPT": f"10:click=card.all;16:click=data.tab.{t}", "PL_LOG_SHELL": "1"}, frames=44, size=sz)
            sheet, panel = r0.rects.get("panel.data"), r0.rects.get("panel.controls")
            check(sheet is not None, f"{t} {sz}: the sheet is open")
            if sheet is None:
                continue
            check(inside(sheet, w, h), f"{t} {sz}: the sheet is inside the window {sheet}")
            if panel is not None:
                check(not overlap(sheet, panel), f"{t} {sz}: the sheet is out from under the Controls panel {sheet} {panel}")
            check(any(n.startswith("datasheet solid") for n in r0.notes), f"{t} {sz}: the sheet is drawn solid")
            m = re.search(r"panelneed data need=(\d+) have=(\d+)", r0.out)
            tall = bool(m) and int(m.group(1)) > int(m.group(2)) + 1
            tall_seen[sz] = tall_seen.get(sz, 0) + (1 if tall else 0)
            if tall:                                               # taller than its place: it scrolls inside itself
                cx, cy = int(sheet[0] + sheet[2] / 2), int(sheet[1] + sheet[3] / 2)
                for how, scr in (("wheel", f"10:click=card.all;16:click=data.tab.{t};30:move={cx},{cy};34:wheel=-3"), ("PageDown", f"10:click=card.all;16:click=data.tab.{t};34:key=pagedown;38:key=pagedown")):
                    r1 = run_app(binary, work, f"ft_{t}_{sz}_{how}", {"PL_ATOM": "1", "PL_NEWUI": "on", "PL_ATOM_SET": "Z=92;N=146;view=Cloud 3D;orb=2p", "PL_SCRIPT": scr, "PL_LOG_SHELL": "1"}, frames=50, size=sz)
                    ys = [int(mm.group(1)) for n in r1.notes for mm in [re.match(r"sheetscroll y=(\d+)", n)] if mm]
                    check(bool(ys) and max(ys) > 0, f"{t} {sz}: {how} scrolled the sheet {ys[-3:]}")
            fact = " ".join(r0.note("factline"))                  # 5l: the fact line ends its second line with "…" BY DESIGN when two lines are too short (a click opens the whole fact); with the toolbar centred under the picture it now sits above it in these views, so its words are logged here. Every other text must still be whole.
            cut = [tx for k, px, tx in r0.texts if "…" in tx and not tx.endswith(" …") and tx.rstrip("…") not in fact]
            check(not cut, f"{t} {sz}: no text cut with an ellipsis {cut}")
    check(tall_seen.get("880x560", 0) >= 3, f"at 880x560 at least three tabs are taller than the sheet, so the scrolling was really exercised {tall_seen}")

if __name__ == "__main__":
    binary, work = sys.argv[1], sys.argv[2]
    part = sys.argv[3] if len(sys.argv) > 3 else "all"
    for p, f in (("centre", part_centre), ("infosize", part_infosize), ("cards", part_cards), ("fit", part_fit)):
        if part in (p, "all"):
            f(binary, work)
    print(f"{checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
