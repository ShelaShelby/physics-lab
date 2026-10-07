#!/usr/bin/env python3
# 0.15 stage 5g: the owner's bug list, checked in the REAL app (Linux, software OpenGL; not Windows, not a real mouse, not a GPU).
#   check_5g.py <PhysicsLab binary> <work folder> <part>      parts: pages, contents (items 1, 2), toolbar (item 3, 4a)
# pages: a wheel event (the same SDL event a mouse wheel makes, pushed by PL_SCRIPT "wheel=") over a Learn guide, the timeline, About and the history window moves the page
#   (the app logs "pagescroll y= max=": the scroll offset), 3 sizes, New UI On and Off; the keys PageDown / End / Home / arrows scroll them too; in the New UI a wheel over the dimmed picture
#   beside the column also scrolls; the Contents list (narrow window) can be opened and used three times in a row, each choice moving the page to its section.
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

def scroll(r):
    for n in r.note("pagescroll"):
        m = re.match(r"pagescroll y=(-?\d+) max=(-?\d+)", n)
        if m:
            return int(m.group(1)), int(m.group(2))
    return None

def part_pages(binary, work):
    pages = (("learn1", {"PL_LEARN": "1"}), ("timeline", {"PL_LEARN": "3"}), ("about", {"PL_OPEN": "about"}), ("history", {"PL_OPEN": "history"}))
    runs = []                                                      # (tag, name, env, size, what)
    for nu in ("off", "on"):
        for sz in SIZES:
            w, h = (int(v) for v in sz.split("x"))
            px, py = w // 2, h // 2
            for name, env in pages:
                base = dict(env, PL_NEWUI=nu)
                tag = f"{name} {sz} New UI {nu}"
                runs.append((tag, f"pg_{name}_{sz}_{nu}_0", dict(base), sz, "long"))
                runs.append((tag, f"pg_{name}_{sz}_{nu}_w", dict(base, PL_SCRIPT=f"8:move={px},{py};12:wheel=-3"), sz, "the wheel moved the page"))
                runs.append((tag, f"pg_{name}_{sz}_{nu}_k", dict(base, PL_SCRIPT=f"12:key=pagedown;14:key=pagedown"), sz, "PageDown scrolled the page"))
                if nu == "on":                                     # beside the reading column, over the dimmed picture
                    runs.append((tag, f"pg_{name}_{sz}_{nu}_dim", dict(base, PL_SCRIPT=f"8:move=12,{py};12:wheel=-3"), sz, "the wheel over the dimmed picture moved the page"))
    prefetch([((binary, work, n, e), {"frames": 24, "size": sz}) for _, n, e, sz, _ in runs])
    for tag, n, e, sz, what in runs:
        s = scroll(run_app(binary, work, n, e, frames=24, size=sz))
        if what == "long":
            check(s is not None and s[1] > 0, f"{tag}: the page is long enough to scroll {s}")
        else:
            check(s is not None and s[0] > 0, f"{tag}: {what} {s}")

def part_contents(binary, work):
    for sz in ("880x560", "1280x800"):
        for name, env in (("timeline", {"PL_LEARN": "3"}), ("learn1", {"PL_LEARN": "1"})):
            seq = "10:click=##readContents;16:click=##toc3;26:click=##readContents;32:click=##toc6;42:click=##readContents;48:click=##toc9"
            ys = []
            for f in (22, 38, 56):
                r = run_app(binary, work, f"ct_{name}_{sz}_{f}", dict(env, PL_NEWUI="on", PL_SCRIPT=seq), frames=f, size=sz)
                ys.append(scroll(r))
            if sz == "1280x800":                                   # a wide window has the contents rail, not the button: nothing to open
                continue
            check(all(y is not None for y in ys), f"{name} {sz}: scroll read {ys}")
            if all(y is not None for y in ys):
                check(ys[0][0] > 0 and ys[1][0] > ys[0][0] and ys[2][0] > ys[1][0], f"{name} {sz}: three Contents choices in a row each moved the view to its section {ys}")

def part_toolbar(binary, work):
    # item 3 (stage 5g): REPLACED in stage 5l by centring each lab's toolbar on its scene (see below); the optional buttons (Centre, Drag) still join at the right end
    # item 4a: Cloud 3D and Slice have their own buttons beside Cloud
    views = (("orbit", {}), ("a_orbit", {"PL_ATOM": "1", "PL_ATOM_SET": "view=Orbit"}), ("a_cloud", {"PL_ATOM": "1", "PL_ATOM_SET": "view=Cloud;orb=2p"}), ("a_cloud3d", {"PL_ATOM": "1", "PL_ATOM_SET": "view=Cloud 3D;orb=2p"}),
             ("a_slice", {"PL_ATOM": "1", "PL_ATOM_SET": "view=Slice;orb=2p"}), ("a_chart", {"PL_ATOM": "1", "PL_ATOM_SET": "view=Chart of nuclides"}),
             ("a_zoom", {"PL_ATOM": "1", "PL_ATOM_SET": "view=Cloud 3D;orb=2p;zoom=2.5"}))
    calls = [((binary, work, f"tb_{n}_{sz}", dict(e, PL_NEWUI="on")), {"frames": 24, "size": sz}) for sz in SIZES for n, e in views]
    prefetch(calls)
    for sz in SIZES:
        first, edge, slots = {}, {}, {}
        for n, e in views:
            r = run_app(binary, work, f"tb_{n}_{sz}", dict(e, PL_NEWUI="on"), frames=24, size=sz)
            btn = {k[len("toolbtn."):]: v for k, v in r.rects.items() if k.startswith("toolbtn.")}
            tb = r.rects.get("toolbar")
            check(bool(btn) and tb is not None, f"{n} {sz}: the toolbar and its buttons are logged")
            if not btn or tb is None:
                continue
            first[n] = min(v[0] for v in btn.values())
            for k, v in btn.items():
                if n != "orbit":
                    slots.setdefault(k, {})[n] = v[0]
            if n != "orbit":
                for want in ("tool.cloud", "tool.cloud3d", "tool.slice"):
                    check(want in btn, f"{n} {sz}: the toolbar has {want}")
        # 5l (changed on purpose, owner's request): the stage 5g rule "the first button / every button has the same x in every view and lab" is gone. Each lab's toolbar is now CENTRED on its scene (see
        # DECISIONS.md, stage 5l; tests/check_5l.py part toolbar), so the Orbit Lab's shorter toolbar is no longer off to the left, and the Atom buttons shift when Drag or Centre appear.
        check("tool.drag" in slots and "tool.centre" in slots, f"{sz}: the optional buttons (Drag, Centre) appear in some view")
        if "tool.centre" in slots and "tool.drag" in slots:
            check(slots["tool.centre"]["a_zoom"] > slots["tool.drag"]["a_zoom"] > slots["tool.reset"]["a_zoom"], f"{sz}: the optional buttons are at the right end, after Reset")

def part_default(binary, work):
    # item 4b: New UI On, fresh settings: the Atom Lab opens in Cloud 3D; New UI Off: the old default (Orbit); a view named by the test (PL_ATOM_SET) still wins
    for nu, want in (("on", "Cloud 3D"), ("off", "Orbit")):
        r = run_app(binary, work, f"def_{nu}", {"PL_ATOM": "1", "PL_NEWUI": nu, "PL_SCRIPT": "10:astate"}, frames=20)
        v = [t.split("=", 1)[1] for ln in r.out.splitlines() if ln.startswith("PLASTATE ") for t in ln[9:].split("\t") if t.startswith("view=")]
        check(v == [want], f"New UI {nu}: a fresh Atom Lab opens in {want}: {v}")
    r = run_app(binary, work, "def_on_set", {"PL_ATOM": "1", "PL_NEWUI": "on", "PL_ATOM_SET": "view=Chart of nuclides", "PL_SCRIPT": "10:astate"}, frames=20)
    check("view=Chart of nuclides" in r.out, "New UI on: a view that is set is kept")

def part_ring(binary, work):
    # item 5: the phase ring never overlaps the subject card, the Legend chip, the Controls panel or the top bar, in any view, at the three sizes (the ring box is what atom_view draws, in points = pixels at scale 1)
    views = (("cloud3d", "view=Cloud 3D;orb=2p;phase=Wheel"), ("slice", "view=Slice;orb=3d;phase=Wheel"), ("cloud", "view=Cloud;orb=2p;phase=Wheel"))
    for sz in SIZES:
        for n, st in views:
            for card_open in (True, False):
                env = {"PL_ATOM": "1", "PL_NEWUI": "on", "PL_ATOM_SET": st, "PL_SCRIPT": ("" if card_open else "6:click=card.card.collapse;") + "20:astate", "PL_LOG_SHELL": "1"}
                r = run_app(binary, work, f"ring_{n}_{sz}_{int(card_open)}", env, frames=30, size=sz)
                tag = f"{n} {sz} card {'open' if card_open else 'collapsed'}"
                m = re.search(r"PLATOMBOX .*ring=([-\d.e+]+),([-\d.e+]+),([-\d.e+]+),([-\d.e+]+)", r.out)
                check(m is not None and float(m.group(3)) > 0, f"{tag}: the ring is drawn")
                if not m or float(m.group(3)) <= 0:
                    continue
                ring = tuple(float(m.group(i)) for i in range(1, 5))
                w, h = r.window[0], r.window[1]
                check(inside(ring, w, h), f"{tag}: the ring is inside the window {ring}")
                cardr = r.rects.get("card.atom.card" + ("" if card_open else ".collapsed"))
                check(cardr is not None, f"{tag}: the card is {'open' if card_open else 'collapsed'}")
                for other in ("card", "card.atom.legend", "panel.controls", "handle", "topbar", "toolbar", "readout", "factline"):
                    o = cardr if other == "card" else r.rects.get(other)
                    if o is not None:
                        check(not overlap(ring, o), f"{tag}: the ring does not overlap {other} {ring} {o}")

def part_panel(binary, work):
    # item 8: a Controls panel section that was just opened scrolls into view: with "Pick a body" opened last, its first row is inside the panel's visible rectangle
    for sz in SIZES:
        r = run_app(binary, work, f"panel_pick_{sz}", {"PL_NEWUI": "on", "PL_SCRIPT": "10:click=ctl.sec.pick"}, frames=50, size=sz)
        pc = r.rects.get("panel.controls")
        nn = r.note("pickfirst")
        row = tuple(float(v) for v in nn[0].split()[1:5]) if nn else None
        check(not r.scripted_missing(), f"{sz}: the Pick a body header was found {r.scripted_missing()}")
        check(pc is not None and row is not None, f"{sz}: the panel and the first Pick row are logged {pc} {row}")
        if pc and row:
            check(row[1] >= pc[1] - 0.6 and row[1] + row[3] <= pc[1] + pc[3] + 0.6, f"{sz}: the first Pick row is inside the panel's visible rectangle {row} {pc}")

def part_atomtext(binary, work):
    # item 10: the Atom info text (top left, "More info") stops before the subject card and the Controls panel (and the phase ring), at the three sizes, in a wide-text view and with the ring
    # item 11: the data sheet has a solid background and never cuts text with "..." (the only "..." is the one in the isotope line's SOURCE string: "... and other decay modes", atom_core.cpp)
    for sz in SIZES:
        for n, st in (("cloud3d", "Z=92;N=146;view=Cloud 3D;orb=2p;phase=Wheel"), ("orbit", "Z=92;N=146;view=Orbit"), ("chart", "Z=92;N=146;view=Chart of nuclides")):
            r = run_app(binary, work, f"info_{n}_{sz}", {"PL_ATOM": "1", "PL_NEWUI": "on", "PL_ATOM_SET": st, "PL_SCRIPT": "6:click=card.more;20:astate"}, frames=30, size=sz)
            m = re.search(r"PLATOMBOX info=([-\d.e+]+),([-\d.e+]+),([-\d.e+]+),([-\d.e+]+)", r.out)
            tag = f"{n} {sz}"
            if not m or float(m.group(3)) <= 0:
                check(n == "chart" or False, f"{tag}: the info text is drawn")
                continue
            info = tuple(float(m.group(i)) for i in range(1, 5))
            card = r.rects.get("card.atom.card")
            check(card is not None, f"{tag}: the subject card is there")
            for other in ("card.atom.card", "panel.controls", "handle"):
                o = r.rects.get(other)
                if o is not None:
                    check(not overlap(info, o), f"{tag}: the info text does not run under {other} {info} {o}")
            rm = re.search(r"PLATOMBOX .*ring=([-\d.e+]+),([-\d.e+]+),([-\d.e+]+),([-\d.e+]+)", r.out)
            if rm and float(rm.group(3)) > 0:
                ring = tuple(float(rm.group(i)) for i in range(1, 5))
                check(not overlap(info, ring), f"{tag}: the info text does not run under the phase ring {info} {ring}")
    for el, nm in (("Z=92;N=146", "u238"), ("Z=26;N=30", "iron")):
        r = run_app(binary, work, f"sheet_{nm}", {"PL_ATOM": "1", "PL_NEWUI": "on", "PL_ATOM_SET": el, "PL_SCRIPT": "10:click=card.all", "PL_LOG_SHELL": "1"}, frames=40)
        check(any(n.startswith("datasheet solid") for n in r.notes), f"{nm}: the data sheet is drawn solid")
        cut = [t for k, px, t in r.texts if "…" in t]
        for t in cut:
            check(t.endswith(" …") and nm == "u238", f"{nm}: a text with an ellipsis must be the isotope line's own source ellipsis, not a cut: {t!r}")
        if nm == "iron":
            check(not cut, f"{nm}: no ellipsis anywhere in the sheet {cut}")

if __name__ == "__main__":
    binary, work = sys.argv[1], sys.argv[2]
    part = sys.argv[3] if len(sys.argv) > 3 else "all"
    for p, f in (("pages", part_pages), ("contents", part_contents), ("toolbar", part_toolbar), ("default", part_default), ("ring", part_ring), ("panel", part_panel), ("atomtext", part_atomtext)):
        if part in (p, "all"):
            f(binary, work)
    print(f"{checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
