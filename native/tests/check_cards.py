#!/usr/bin/env python3
# 0.15 stage 5f part 2: the subject card, the legend and the data sheet (New UI On), checked in the REAL app (Linux, software OpenGL; not Windows, not a GPU).
#   check_cards.py <PhysicsLab binary> <work folder> [orbit | atom | all]
# What is checked, for each lab and each of 1280x800, 880x560 and 1920x1080 (DESIGN.md section 5: the subject card top right, to the left of the panel and its handle, never under them):
#   * the card exists, is inside the window, overlaps none of the top bar, toolbar, readout, fact line, Controls panel, handle, legend (and the Old panel);
#   * all card / sheet text is at least 14 px;
#   * the data sheet opens from "All values", closes with Esc and with its close button, is inside the window;
#   * the card follows the chosen body (Orbit) / element (Atom);
#   * Atom: the sheet shows the same strings as the OLD panel's element card, facts card, isotope line, decay history and hint line of the same build (the old panel prints them with
#     PL_LOG_CARD as PLATOMCARD lines; the new sheet prints PLSHEET), for hydrogen, iron, uranium-238 and oganesson (no melting or boiling point): independent of our new code, it is the old UI.
#   The Orbit sheet against the old card, word for word, is tested by orbit_ui_card (check_orbit_ui.py).
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, inside, overlap, prefetch, old_run

SIZES = ("1280x800", "880x560", "1920x1080")
fails = checks = 0

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def find(r, prefix):
    for k, v in r.rects.items():
        if k == prefix or k.startswith(prefix + "."):
            return v
    return None

def lines(r, tag):
    # "TAG\tline\tline ..." (the old panel's PLATOMCARD lines) or "TAG name\tline ..." (the sheet's PLSHEET)
    return [ln.split("\t")[1:] if "\t" in ln else [] for ln in r.out.splitlines() if ln == tag or ln.startswith(tag + "\t") or ln.startswith(tag + " ")]

def small_text(r):
    # "ctl" is the kind of the text drawn with the small-text floor (the Controls panel, the cards and the data sheet): the cards' small text is Caption, drawn at Body size (>= 14 px)
    return [(k, px, t) for k, px, t in r.texts if k == "ctl" and px < 14 - 0.01]

def geometry(r, lab, sz, what):
    card = find(r, f"card.{lab}.card")
    check(card is not None, f"{lab} {sz} {what}: the subject card is drawn")
    if card is None:
        return
    w, h = r.window[0], r.window[1]
    check(inside(card, w, h), f"{lab} {sz} {what}: the card is inside the window {card}")
    for other in ("topbar", "toolbar", "readout", "factline", "panel.controls", "handle", "panel.old", f"card.{lab}.legend"):
        o = find(r, other)
        if o is not None:
            check(not overlap(card, o), f"{lab} {sz} {what}: the card does not overlap {other} {card} {o}")
    body = [px for k, px, t in r.texts if k == "body"]
    check(body and abs(card[2] - 20 * body[0]) <= 1.5, f"{lab} {sz} {what}: the card is 20 em wide (DESIGN.md section 5): {card[2]:.1f} px, Body text {body[:1]} px")
    pc, hd = r.rects.get("panel.controls"), r.rects.get("handle")
    for o in (pc, hd):
        if o is not None:
            check(card[0] + card[2] <= o[0] + 0.6, f"{lab} {sz} {what}: the card is left of the panel / handle ({card[0] + card[2]:.0f} <= {o[0]:.0f})")
    check(not small_text(r), f"{lab} {sz} {what}: all text is at least 14 px: {small_text(r)[:3]}")
    check(any(k == "ctl" and t in ("from the Sun", "protons", "proton") for k, _, t in r.texts), f"{lab} {sz} {what}: the card's small caption is among the checked text")

def part_orbit(binary, work):
    base = {"PL_NEWUI": "on", "PL_SELECT": "Earth"}
    prefetch([((binary, work, f"cd_orbit_{sz}", base), dict(frames=60, size=sz)) for sz in SIZES] +
             [((binary, work, f"cd_orbit_sheet_{sz}", dict(base, PL_SCRIPT="20:click=card.all")), dict(frames=60, size=sz)) for sz in SIZES])
    for sz in SIZES:
        r = run_app(binary, work, f"cd_orbit_{sz}", base, frames=60, size=sz)
        geometry(r, "orbit", sz, "closed")
        lg = find(r, "card.orbit.legend")
        check(lg is not None and inside(lg, r.window[0], r.window[1]), f"orbit {sz}: the Legend chip is drawn inside the window {lg}")
        check(lg is not None and lg[0] < r.window[0] / 4 and lg[1] < 2 * r.rects["topbar"][3] + 40, f"orbit {sz}: the Legend is top left, under the top bar {lg}")
        check(any(k.startswith("card.orbit.legend.collapsed") for k in r.rects), f"orbit {sz}: the Legend starts collapsed (DESIGN.md section 5)")
        rs = run_app(binary, work, f"cd_orbit_sheet_{sz}", dict(base, PL_SCRIPT="20:click=card.all"), frames=60, size=sz)
        sh = rs.rects.get("panel.data")
        check(not rs.scripted_missing() and sh is not None and inside(sh, rs.window[0], rs.window[1]), f"orbit {sz}: All values opens the data sheet inside the window {sh} {rs.scripted_missing()[:2]}")
        check(sh is not None and sh[2] <= 0.8 * rs.window[0] + 0.6, f"orbit {sz}: the sheet is at most 80 % of the window wide")
        check(not small_text(rs), f"orbit {sz}: sheet text is at least 14 px: {small_text(rs)[:3]}")
    for how, script in (("Esc", "20:click=card.all;40:key=escape"), ("its close button", "20:click=card.all;40:click=data.close")):
        r = run_app(binary, work, f"cd_orbit_close", dict(base, PL_SCRIPT=script), frames=70, size="1280x800")
        check(not r.scripted_missing() and "panel.data" not in r.rects, f"orbit: the data sheet closes with {how}: {r.scripted_missing()[:2]} {[k for k in r.rects if 'data' in k]}")
    for body in ("Earth", "Jupiter", "Pluto"):
        r = run_app(binary, work, f"cd_orbit_{body}", {"PL_NEWUI": "on", "PL_SELECT": body}, frames=60)
        t = r.note("cardtitle orbit.card")
        check(t and body in t[0], f"orbit: the card follows the chosen body {body}: {t}")
    r = run_app(binary, work, "cd_orbit_pick", {"PL_NEWUI": "on", "PL_SELECT": "Earth", "PL_SCRIPT": "10:click=ctl.sec.pick;16:click=##pickfilter;22:text=Mars;28:click=pick.Mars"}, frames=100, size="1000x640")
    t = r.note("cardtitle orbit.card")
    check(not r.scripted_missing() and t and "Mars" in t[0], f"orbit: picking Mars in the list changes the card to Mars: {t} {r.scripted_missing()[:2]}")

# (stage 5g: the Orbit view is named in both runs: the New UI now starts in Cloud 3D and the old app in Orbit, and the hint line differs by view; the comparison needs the same view)
ELEMS = (("hydrogen", "Z=1;N=0;view=Orbit"), ("iron", "Z=26;N=30;view=Orbit"), ("uranium238", "Z=92;N=146;view=Orbit"), ("oganesson", "Z=118;N=176;view=Orbit"))

def part_atom(binary, work):
    NEW, OLD = {"PL_NEWUI": "on", "PL_ATOM": "1"}, {"PL_NEWUI": "off", "PL_ATOM": "1", "PL_LOG_CARD": "1"}
    U = {"PL_ATOM_SET": "Z=92;N=146"}
    calls = [((binary, work, f"cd_atom_{sz}", dict(NEW, **U)), dict(frames=60, size=sz)) for sz in SIZES]
    calls += [((binary, work, f"cd_atom_sheet_{sz}", dict(NEW, PL_SCRIPT="20:click=card.all", **U)), dict(frames=60, size=sz)) for sz in SIZES]
    for name, st in ELEMS:
        calls.append(((binary, work, f"cd_atom_new_{name}", dict(NEW, PL_ATOM_SET=st, PL_LOG_SHELL="1", PL_SCRIPT="20:click=card.all")), dict(frames=60, size="1280x800")))
    prefetch(calls)
    for sz in SIZES:
        r = run_app(binary, work, f"cd_atom_{sz}", dict(NEW, **U), frames=60, size=sz)
        geometry(r, "atom", sz, "closed")
        rs = run_app(binary, work, f"cd_atom_sheet_{sz}", dict(NEW, PL_SCRIPT="20:click=card.all", **U), frames=60, size=sz)
        sh = rs.rects.get("panel.data")
        check(not rs.scripted_missing() and sh is not None and inside(sh, rs.window[0], rs.window[1]), f"atom {sz}: All values opens the data sheet inside the window {sh} {rs.scripted_missing()[:2]}")
        check(sh is not None and sh[2] <= 0.8 * rs.window[0] + 0.6, f"atom {sz}: the sheet is at most 80 % of the window wide")
        check(not small_text(rs), f"atom {sz}: sheet text is at least 14 px: {small_text(rs)[:3]}")
    for how, script in (("Esc", "20:click=card.all;40:key=escape"), ("its close button", "20:click=card.all;40:click=data.close")):
        r = run_app(binary, work, "cd_atom_close", dict(NEW, PL_SCRIPT=script, **U), frames=70)
        check(not r.scripted_missing() and "panel.data" not in r.rects, f"atom: the data sheet closes with {how}: {r.scripted_missing()[:2]}")
    # the card follows the chosen element: set at the start, and by the periodic table in the chip's dropdown
    for name, st in ELEMS:
        r = run_app(binary, work, f"cd_atom_new_{name}", dict(NEW, PL_ATOM_SET=st, PL_LOG_SHELL="1", PL_SCRIPT="20:click=card.all"), frames=60)
        t = r.note("cardtitle atom.card")
        want = {"hydrogen": "Hydrogen-1", "iron": "Iron-56", "uranium238": "Uranium-238", "oganesson": "Oganesson-294"}[name]
        check(t and want in t[0], f"atom: the card is about {want}: {t}")
    r = run_app(binary, work, "cd_atom_pick", dict(NEW, PL_SCRIPT="10:click=top.chip;20:click=pt.26"), frames=60)
    t = r.note("cardtitle atom.card")
    check(not r.scripted_missing() and t and "Iron" in t[0], f"atom: choosing iron in the periodic table changes the card: {t} {r.scripted_missing()[:2]}")
    # the sheet shows what the old panel showed (the old panel's strings, saved from the last build that had the old UI: tests/reference/old_ui_runs.json)
    for name, st in ELEMS:
        ro = old_run("cards", f"cd_atom_old_{name}", dict(OLD, PL_ATOM_SET=st), frames=40, size="1000x2800")
        old = []
        for tag in ("hint", "isotope", "element", "facts", "decay"):
            m = lines(ro, f"PLATOMCARD {tag}")
            check(len(m) >= 1, f"atom {name}: the old panel printed its {tag}")
            if m:
                old += m[-1]
        rn = run_app(binary, work, f"cd_atom_new_{name}", dict(NEW, PL_ATOM_SET=st, PL_LOG_SHELL="1", PL_SCRIPT="20:click=card.all"), frames=60)
        sheet = lines(rn, "PLSHEET")
        check(len(sheet) >= 1, f"atom {name}: the new sheet printed its lines")
        if sheet and old:
            new = sheet[-1]               # (the sheet line is "name TAB line TAB line ...": the name is not a field here)
            check(sorted(new) == sorted(old), f"atom {name}: the sheet's lines equal the old panel's strings ({len(new)} vs {len(old)})" + ("" if sorted(new) == sorted(old) else f"\n  only new {sorted(set(new) - set(old))[:3]}\n  only old {sorted(set(old) - set(new))[:3]}"))
    # the Legend (chart view): a collapsed chip top left; it holds the chart's colours with counts; the picture no longer draws the same legend under the panel (the old picture does)
    CH = {"PL_ATOM_SET": "Z=92;N=146;view=Chart of nuclides"}
    def drawn_legend(r):
        for ln in r.out.splitlines():
            if ln.startswith("PLASTATE "):
                for kv in ln[9:].split("\t"):
                    if kv.startswith("drawnLegend="):
                        return kv.split("=", 1)[1]
        return None
    for sz in SIZES:
        r = run_app(binary, work, f"cd_atom_leg_{sz}", dict(NEW, PL_SCRIPT="30:astate", **CH), frames=50, size=sz)
        lg, cd = find(r, "card.atom.legend"), find(r, "card.atom.card")
        check(lg is not None and any(k == "card.atom.legend.collapsed" for k in r.rects), f"atom {sz} chart: the Legend chip is there, collapsed (DESIGN.md section 5) {lg}")
        if lg is not None:
            check(inside(lg, r.window[0], r.window[1]) and lg[0] < r.window[0] / 4, f"atom {sz} chart: the Legend is inside the window, top left {lg}")
            for other in ("topbar", "card.atom.card", "toolbar", "readout", "factline"):
                o = find(r, other)
                if o is not None:
                    check(not overlap(lg, o), f"atom {sz} chart: the Legend does not overlap {other} {lg} {o}")
        check(drawn_legend(r) == "0", f"atom {sz} chart: the picture does not draw its own legend while the Legend chip has it: {drawn_legend(r)}")
    ro = old_run("cards", "cd_atom_leg_old", dict({"PL_NEWUI": "off", "PL_ATOM": "1", "PL_SCRIPT": "30:astate"}, **CH), frames=50, size="1000x800")
    check(drawn_legend(ro) == "1", f"atom old UI (saved run): the picture drew its legend itself: {drawn_legend(ro)}")
    for sz in ("1280x800", "1920x1080"):
        r = run_app(binary, work, f"cd_atom_legopen_{sz}", dict(NEW, PL_SCRIPT="20:click=card.legend", **CH), frames=60, size=sz)
        lg = find(r, "card.atom.legend")
        n = r.note("atomlegend")
        check(not r.scripted_missing() and lg is not None and not any(k.endswith(".collapsed") and "legend" in k for k in r.rects) and inside(lg, r.window[0], r.window[1]), f"atom {sz}: clicking the chip opens the Legend inside the window {lg} {r.scripted_missing()[:2]}")
        check(n and "rows=8" in n[0] and "ends=(" in n[0], f"atom {sz}: the open Legend lists the chart's 8 colours, each with its count: {n}")
        check(not small_text(r), f"atom {sz}: Legend text is at least 14 px: {small_text(r)[:3]}")
    r = run_app(binary, work, "cd_atom_legnone", dict(NEW, **U), frames=40)
    check(find(r, "card.atom.legend") is None, "atom: no Legend chip in the Orbit view (nothing in it to list)")

    # More / Less info: the old button's words and action (the lab starts with the picture's extra lines shown at 1280 wide)
    r = run_app(binary, work, "cd_atom_more0", dict(NEW, **U), frames=40)
    r1 = run_app(binary, work, "cd_atom_more1", dict(NEW, PL_SCRIPT="20:click=card.more", **U), frames=60)
    w0 = {t for k, _, t in r.texts if t in ("Less info", "More info")}
    w1 = {t for k, _, t in r1.texts if t in ("Less info", "More info")}
    # (the texts are collected over all frames of a run: before the click only the first words, after it both)
    check(not r1.scripted_missing() and w0 == {"Less info"} and w1 == {"Less info", "More info"}, f"atom: the card's More / Less info button flips its words when clicked: {w0} -> {w1} {r1.scripted_missing()[:2]}")

def main():
    binary, work = sys.argv[1], sys.argv[2]
    part = sys.argv[3] if len(sys.argv) > 3 else "all"
    os.makedirs(work, exist_ok=True)
    if part in ("orbit", "all"):
        part_orbit(binary, work)
    if part in ("atom", "all"):
        part_atom(binary, work)
    print(f"card checks: {checks - fails} of {checks} passed")
    sys.exit(1 if fails else 0)

main()
