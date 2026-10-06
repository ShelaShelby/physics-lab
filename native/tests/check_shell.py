#!/usr/bin/env python3
# 0.15 stage 3: the new UI, checked in the REAL app (Linux build, headless on Mesa llvmpipe; not Windows, not a GPU):
#   check_shell.py <PhysicsLab binary> <work folder>
# WRITTEN BEFORE the code was run in the app (the logic it tests is in app/shell.* with its own unit tests). What it checks, from what the app prints with PL_LOG_SHELL
# (every text the new UI draws with its size in pixels, every rectangle) and from scripted input (PL_SCRIPT: real mouse and key events):
#   sizes     every text the new UI draws, at every Text size (Small .. Extra large) and Windows scale 1.0, 1.25, 1.5, 2.0 (a stand-in: PL_SCALE), is exactly
#             max(floor, round(u * scale * text factor)) for its kind, so never under the floors 12 / 14 / 15 / 22 px (the expected values are written HERE, from DESIGN.md)
#   window    no panel, bar or card leaves the window, at the smallest window (880 x 560 units) and at 1280 x 800; the toolbar and the readout do not overlap
#   collapse  a collapsed panel keeps its title (the Legend, and the subject card after it is collapsed)
#   keys      Esc closes an open panel, then search; Ctrl+K opens search; Space plays / pauses (and is typing inside search)
#   shift     with a card or panel over the middle, the selected body is moved outside every open panel; with the shift switched off (PL_SHIFT=off) it is not (so the test can fail)
#   reading   About opens in the reading view: a column of at most 34 em, headings, a contents list, body lines of at most 75 characters (DejaVu Sans here, wider than Segoe UI)
#   old       the Old panel button opens today's side column as a floating window
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, inside, overlap

# From docs/ui-0.15/DESIGN.md section 1 (the owner's chosen design): kind -> (size in units, floor in px)
SCALE = {"caption": (13, 12), "body": (15, 14), "title": (17, 15), "key": (28, 22), "h1": (28, 22), "h2": (20, 16), "h3": (17, 15)}
# Text the old pages draw inside the reading view (About, guides ...) has many sizes of its own (17 pt card titles, 13 pt notes ...): only the floors are claimed for it, not an exact size
PAGE_FLOORS = {"pbody": 14, "pcaption": 12, "plabel": 12}
TF = {"small": 0.9, "normal": 1.0, "large": 1.15, "xl": 1.3}
EXERCISE = ("10:click=top.chip;14:key=escape;16:click=tool.speed;20:key=escape;22:click=tool.show;26:key=escape;28:click=tool.pick;32:key=escape;34:click=tool.scale;38:key=escape;"
            "40:click=tool.view.caret;44:key=escape;46:click=top.menu;50:key=escape;52:click=top.facts;56:key=escape;58:key=ctrl+k;62:text=ear;66:key=escape;68:click=top.lab;72:key=escape;"
            "74:click=card.legend;80:click=top.old")
fails = 0
checks = 0

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def expected(kind, dpi, tf):
    u, floor = SCALE[kind]
    return max(floor, int(u * dpi * tf + 0.5))

def part_sizes(binary, work, only_first_scale):
    for dpi in ((1.0,) if only_first_scale else (1.0, 1.25, 1.5, 2.0)):
        size = f"{int(round(1000 * dpi))}x{int(round(640 * dpi))}"
        for tfn, tf in TF.items():
            r = run_app(binary, work, f"sizes_{dpi}_{tfn}", {"PL_NEWUI": "on", "PL_SCALE": str(dpi), "PL_TEXTSIZE": tfn, "PL_SELECT": "Earth", "PL_SCRIPT": EXERCISE}, frames=90, size=size)
            tag = f" (scale {dpi}, text {tfn})"
            check(r.window is not None, "the app printed its PLSHELL report" + tag)
            if r.window is None:
                continue
            check(not r.scripted_missing(), "every scripted click found its control" + tag + " " + str(r.scripted_missing()[:2]))
            check(len(r.texts) >= 25, f"the new UI drew at least 25 different texts ({len(r.texts)})" + tag)
            kinds = {k for k, _, _ in r.texts}
            check({"caption", "body", "title"} <= kinds, "caption, body and title texts all appear" + tag)
            bad = [(k, px, t) for k, px, t in r.texts if k in SCALE and px != expected(k, dpi, tf)]
            check(not bad, "every text is exactly max(floor, round(u x scale x text size))" + tag + " " + str(bad[:3]))
            under = [(k, px, t) for k, px, t in r.texts if px < (SCALE[k][1] if k in SCALE else PAGE_FLOORS[k])]
            check(not under, "no text under its floor" + tag + " " + str(under[:3]))
            labels = [px for k, px, _ in r.texts if k == "plabel"]
            check(labels, "the picture drew labels" + tag)
            check(all(px >= max(12.0, 13 * dpi * tf - 0.01) for px in labels), "the picture's own labels are never under the Caption size (13 u, floor 12 px)" + tag + " " + str(sorted(set(labels))[:3]))
            w, h = r.window[0], r.window[1]
            for name, rect in r.rects.items():
                if name.startswith("occ"):
                    continue
                check(inside(rect, w, h), f"{name} stays inside the window {rect}" + tag)
            if "toolbar" in r.rects and "readout" in r.rects:
                check(not overlap(r.rects["toolbar"], r.rects["readout"]), "toolbar and readout do not overlap" + tag)

def part_reading_sizes(binary, work):
    # the reading view at every Text size and Windows scale: its headings are exact, its page text never under the floors
    for dpi in (1.0, 1.25, 1.5, 2.0):
        for tfn, tf in TF.items():
            r = run_app(binary, work, f"read_{dpi}_{tfn}", {"PL_NEWUI": "on", "PL_SCALE": str(dpi), "PL_TEXTSIZE": tfn, "PL_OPEN": "about"}, frames=30, size=f"{int(round(1000 * dpi))}x{int(round(700 * dpi))}")
            tag = f" (reading view, scale {dpi}, text {tfn})"
            check(any(k == "pbody" for k, _, _ in r.texts) and any(k == "h1" for k, _, _ in r.texts), "the reading view drew its title and its page text" + tag)
            bad = [(k, px, t) for k, px, t in r.texts if k in SCALE and px != expected(k, dpi, tf)]
            check(not bad, "its headings and card texts are exactly on the scale" + tag + " " + str(bad[:3]))
            under = [(k, px, t) for k, px, t in r.texts if px < (SCALE[k][1] if k in SCALE else PAGE_FLOORS[k])]
            check(not under, "no page text under its floor" + tag + " " + str(under[:3]))
            col = r.rects.get("reading.column")
            check(col is not None and inside(col, r.window[0], r.window[1]), "the reading column stays inside the window" + tag)

def part_small(binary, work):
    # the smallest window: 880 x 560 units, at 100 % and at 150 %
    for dpi, size in ((1.0, "880x560"), (1.5, "1320x840")):
        for tfn in ("small", "xl"):
            r = run_app(binary, work, f"small_{dpi}_{tfn}", {"PL_NEWUI": "on", "PL_SCALE": str(dpi), "PL_TEXTSIZE": tfn, "PL_SELECT": "Earth", "PL_SCRIPT": EXERCISE}, frames=90, size=size)
            tag = f" (smallest window, scale {dpi}, text {tfn})"
            w, h = r.window[0], r.window[1]
            for name, rect in r.rects.items():
                if not name.startswith("occ"):
                    check(inside(rect, w, h), f"{name} stays inside the smallest window {rect}" + tag)
            check(not overlap(r.rects["toolbar"], r.rects["readout"]), "toolbar and readout do not overlap" + tag)

def part_collapse(binary, work):
    r = run_app(binary, work, "collapse", {"PL_NEWUI": "on", "PL_SELECT": "Earth", "PL_SCRIPT": "10:click=card.card.collapse"}, frames=40)
    check("card.orbit.legend.collapsed" in r.rects, "the Legend starts collapsed")
    check(any(k == "title" and t == "Legend" for k, _, t in r.texts), "the collapsed Legend still shows its title")
    check("card.orbit.card.collapsed" in r.rects, "the subject card collapses when its button is pressed: " + str([k for k in r.rects if k.startswith("card")]))
    check(any(k == "title" and t == "Earth" for k, _, t in r.texts), "the collapsed subject card still shows its title (Earth)")
    cr = r.rects.get("card.orbit.card.collapsed")
    if cr:
        check(cr[3] <= 40 and cr[2] <= 200, f"a collapsed card is small ({cr})")

def part_keys(binary, work):
    def panels(r): return [n for n in r.notes if n.startswith("panelopen")]
    r = run_app(binary, work, "k_open", {"PL_NEWUI": "on", "PL_SCRIPT": "10:click=tool.show"}, frames=30)
    check(panels(r) == ["panelopen show"], "a click on Show opens its panel: " + str(panels(r)))
    r = run_app(binary, work, "k_esc", {"PL_NEWUI": "on", "PL_SCRIPT": "10:click=tool.show;20:key=escape"}, frames=30)
    check(panels(r) == [], "Esc closes the open panel: " + str(panels(r)))
    r = run_app(binary, work, "k_one", {"PL_NEWUI": "on", "PL_SCRIPT": "10:click=tool.show;20:click=tool.speed"}, frames=34)
    check(panels(r) == ["panelopen speed"], "one dropdown at a time: " + str(panels(r)))
    r = run_app(binary, work, "k_ctrlk", {"PL_NEWUI": "on", "PL_SCRIPT": "10:key=ctrl+k"}, frames=30)
    check("panel.search" in r.rects, "Ctrl+K opens search")
    r = run_app(binary, work, "k_ctrlk_esc", {"PL_NEWUI": "on", "PL_SCRIPT": "10:key=ctrl+k;20:key=escape"}, frames=30)
    check("panel.search" not in r.rects, "Esc closes search")
    r = run_app(binary, work, "k_ctrlk_over_panel", {"PL_NEWUI": "on", "PL_SCRIPT": "10:click=tool.show;20:key=ctrl+k"}, frames=34)
    check("panel.search" in r.rects and panels(r) == [], "Ctrl+K closes an open dropdown and opens search")
    r = run_app(binary, work, "k_space_none", {"PL_NEWUI": "on", "PL_SCRIPT": "30:state"}, frames=34)
    r2 = run_app(binary, work, "k_space", {"PL_NEWUI": "on", "PL_SCRIPT": "10:key=space;30:state"}, frames=34)
    r3 = run_app(binary, work, "k_space_search", {"PL_NEWUI": "on", "PL_SCRIPT": "10:key=ctrl+k;14:key=space;30:state"}, frames=34)
    paused = lambda x: x.states[-1].split("paused=")[1].split()[0] if x.states else "?"
    check(paused(r) == "1", "the lab starts paused here (reduced motion): " + str(r.states))
    check(paused(r2) == "0", "Space plays the paused lab: " + str(r2.states))
    check(paused(r3) == "1", "Space inside search is typing, not play: " + str(r3.states))

def part_shift(binary, work):
    # 1000 x 640: the Time panel covers the middle where Earth is
    r = run_app(binary, work, "shift_on", {"PL_NEWUI": "on", "PL_SELECT": "Earth", "PL_SCRIPT": "20:click=top.chip"}, frames=100, size="1000x640")
    r0 = run_app(binary, work, "shift_off", {"PL_NEWUI": "on", "PL_SELECT": "Earth", "PL_SHIFT": "off", "PL_SCRIPT": "20:click=top.chip"}, frames=100, size="1000x640")
    def body(res):
        for n in res.note("body"):
            _, x, y, rad = n.split()
            return float(x), float(y), float(rad)
    def hit(res, b):
        x, y, rad = b
        return [n for n, rc in res.rects.items() if n.startswith("occ") and rc[0] - rad < x < rc[0] + rc[2] + rad and rc[1] - rad < y < rc[1] + rc[3] + rad]
    b_on, b_off = body(r), body(r0)
    check(b_on is not None and b_off is not None, "the selected body's place is printed")
    if b_on and b_off:
        check(hit(r0, b_off), f"without the shift Earth {b_off} is under an open panel (so the test can fail): {hit(r0, b_off)}")
        check(not hit(r, b_on), f"with the shift Earth {b_on} is outside every open panel and card: {hit(r, b_on)}")
        check(abs(b_on[0] - 500) > 100, f"Earth really moved from the middle ({b_on})")

def part_reading(binary, work):
    r = run_app(binary, work, "reading", {"PL_NEWUI": "on", "PL_OPEN": "about"}, frames=60)
    col = r.rects.get("reading.column")
    check(col is not None, "About opens in the reading view")
    if col:
        check(col[2] <= 34 * 15 + 0.6, f"the column is at most 34 em ({col[2]} px at body 15)")
        check(abs((col[0] + col[2] / 2) - 640) < 1.0, "the column is centred")
    check(any(k == "h1" and "About" in t for k, _, t in r.texts), "the page has its H1 title")
    check(any(k == "h3" for k, _, t in r.texts), "the page has headings")
    toc = r.note("toc")
    check(toc and int(toc[0].split()[1]) >= 4, "the contents list has the page's headings: " + str(toc))
    rm = r.note("readmax")
    check(rm and int(rm[0].split()[1]) <= 75, "no line of body text is longer than 75 characters: " + str(rm))
    r = run_app(binary, work, "reading_wide", {"PL_NEWUI": "on", "PL_OPEN": "about"}, frames=60, size="2000x900")
    check("reading.column" in r.rects and r.note("toc") and "reading.toc" in r.rects, "a wide window shows the reading view with its contents rail")

def part_old(binary, work):
    r = run_app(binary, work, "old", {"PL_NEWUI": "on", "PL_SCRIPT": "10:click=top.old"}, frames=40)
    check("panel.old" in r.rects, "the Old panel button opens today's side panel as a floating window")
    r = run_app(binary, work, "off", {"PL_NEWUI": "off"}, frames=30)
    check(r.window is None or not r.texts, "with New UI off the new UI draws nothing")

def main():
    binary, work = sys.argv[1], sys.argv[2]
    only = set(sys.argv[3].split(",")) if len(sys.argv) > 3 else None       # a third argument runs only some parts (for the perturbation runs): sizes1 sizes reading_sizes small collapse keys shift reading old
    want = lambda name: only is None or name in only
    os.makedirs(work, exist_ok=True)
    if want("sizes") or want("sizes1"):
        part_sizes(binary, work, not want("sizes"))
    if want("reading_sizes"):
        part_reading_sizes(binary, work)
    if want("small"):
        part_small(binary, work)
    if want("collapse"):
        part_collapse(binary, work)
    if want("keys"):
        part_keys(binary, work)
    if want("shift"):
        part_shift(binary, work)
    if want("reading"):
        part_reading(binary, work)
    if want("old"):
        part_old(binary, work)
    print(f"{checks} checks, {fails} failed")
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
