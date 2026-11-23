#!/usr/bin/env python3
# 0.15 stage 5f part 2: the Orbit Lab's subject card and its data sheet, in the REAL app (Linux, software Vulkan; not Windows, not a GPU). tests/TESTS.md lists the parts kept and why.
#   check_cards.py <PhysicsLab binary> <work folder> orbit
# What is checked at 1280x800 (0.76.0: one size; the Atom Lab's sheet against the old panel was deleted, git keeps it):
#   * the card exists, is inside the window, overlaps none of the top bar, toolbar, readout, fact line, Controls panel, handle, legend;
#   * all card / sheet text is at least 14 px;
#   * the data sheet opens from "All values", closes with Esc and with its close button, is inside the window;
#   * the card follows the chosen body.
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, inside, overlap, prefetch, old_run, old_strings_028

SIZES = ("1280x800",)   # 0.76.0: one size (the card opening and following its subject is the same at 880x560)
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
    ems = 17 if lab == "atom" else 20      # 0.22 (changed on purpose): the Atom Lab's card is 17 em wide (the Orbit Lab's keeps DESIGN.md section 5's 20 em)
    check(body and abs(card[2] - ems * body[0]) <= 1.5, f"{lab} {sz} {what}: the card is {ems} em wide: {card[2]:.1f} px, Body text {body[:1]} px")
    pc, hd = r.rects.get("panel.controls"), r.rects.get("handle")
    for o in (pc, hd):
        if o is not None:
            check(card[0] + card[2] <= o[0] + 0.6, f"{lab} {sz} {what}: the card is left of the panel / handle ({card[0] + card[2]:.0f} <= {o[0]:.0f})")
    check(not small_text(r), f"{lab} {sz} {what}: all text is at least 14 px: {small_text(r)[:3]}")
    check(any(k == "ctl" and (t in ("from the Sun",) or t.startswith("atomic number · ")) for k, _, t in r.texts), f"{lab} {sz} {what}: the card's small caption is among the checked text")   # (0.22 stage 6: "atomic number · 26 protons", was "protons")

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

def main():
    binary, work = sys.argv[1], sys.argv[2]
    part = sys.argv[3] if len(sys.argv) > 3 else "all"
    os.makedirs(work, exist_ok=True)
    if part in ("orbit", "all"):
        part_orbit(binary, work)
    print(f"card checks: {checks - fails} of {checks} passed")
    sys.exit(1 if fails else 0)

main()
