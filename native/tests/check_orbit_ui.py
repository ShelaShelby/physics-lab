#!/usr/bin/env python3
# 0.15 stage 4: the Orbit Lab in the NEW UI, checked in the REAL app (Linux build, headless on Mesa llvmpipe; not Windows, not a GPU):
#   check_orbit_ui.py <PhysicsLab binary> <work folder> [part,part,...]
# WRITTEN BEFORE the stage 4 code was run in the app. Parts:
#   coverage  every Orbit Lab item of docs/ui-0.15/INVENTORY.md (section 1) is reached with real mouse clicks / keys in the new UI, within the clicks that file states, and does what
#             it should (the lab's state line changes as expected; dates are checked against Python's own datetime, not against the app). Includes: play / pause in the chip and
#             Space, Save picture in the menu and in search but NOT on the toolbar, no "Old panel" in the Orbit Lab, and ALL 47 "Go to" names that INVENTORY.md lists.
#   card      for 8 things (Sun, Earth, Jupiter, Pluto, Moon, Halley, the asteroid belt, Alpha Centauri) the lines of the new data sheet equal, word for word, the lines of the OLD
#             layout's card (both printed by the real app), and the big number is a piece of one of them.
#   time      the Time panel fits above the toolbar at 1280x800 and 1000x640 (its content needs no more room than it has) in the Live view; the Set manually view at 1280x800
#   facts     the fact line: visible by default, changes after the interval (a fixed 16 ms step, so the 20 s is counted in frames), stands still while hovered, a click opens the whole
#             fact, Settings Facts Off hides it, never overlaps the toolbar / readout / top bar / cards / panels, never over the selected body
#   search    20 named things are found, selected or opened with Enter; arrows move; recent choices come first
#   links     in the reading view a link's underline is one line (no gap between two words of the same link)
import datetime, glob, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, inside, overlap, Bmp

HERE = os.path.dirname(os.path.abspath(__file__))
fails = checks = 0

def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def seq(*cmds, start=10, step=6):
    out, f = [], start
    for c in cmds:
        out.append(f"{f}:{c}")
        f += step
    out.append(f"{f + 8}:state")
    return ";".join(out), f + 16

def state(r):
    if not r.states:
        return None
    d = {}
    for tok in r.states[-1].replace("|", " ").split():
        if "=" in tok:
            k, v = tok.split("=", 1)
            d[k] = v
    return d

def utc_ms(y, mo, d, h, mi):
    return int(datetime.datetime(y, mo, d, h, mi, tzinfo=datetime.timezone.utc).timestamp() * 1000)

class Runner:
    def __init__(self, binary, work, size="1280x800"):
        self.binary, self.work, self.size, self.n = binary, work, size, 0
        self.base = {}
    def go(self, cmds, env=None, size=None, theme="dark", extra_frames=0):
        self.n += 1
        script, frames = seq(*cmds)
        e = {"PL_NEWUI": "on", "PL_SCRIPT": script}
        e.update(env or {})
        return run_app(self.binary, self.work, f"orb{self.n}", e, frames=frames + extra_frames, size=size or self.size, theme=theme)
    def base_state(self, env=None):
        key = tuple(sorted((env or {}).items()))
        if key not in self.base:
            self.base[key] = state(self.go([], env))
        return self.base[key]

def missing(r):
    return r.scripted_missing()

def inventory_names():
    # the 47 names INVENTORY.md lists (it said 45 in stage 2: a miscount, found by this test) in its "Go to: tiles" row (the independent source of what the pick list must hold)
    text = open(os.path.join(HERE, "..", "docs", "ui-0.15", "INVENTORY.md"), encoding="utf-8").read()
    row = [ln for ln in text.splitlines() if ln.startswith("| Go to: tiles")][0]
    body = row.split("tiles — ", 1)[1].split(" (47 tiles")[0]
    names = []
    for part in body.split(";"):
        part = part.strip()
        names += [n.strip() for n in part.split(":", 1)[1].split(",")]
    return names

# ------------------------------------------------------------------ coverage
def part_coverage(binary, work):
    R = Runner(binary, work)
    base = R.base_state()
    check(base is not None, "the base run printed a state line")
    # --- the top bar and the chip ---
    r = R.go(["click=top.play"]); s = state(r)
    check(not missing(r) and base["paused"] == "1" and s["paused"] == "0", f"play in the chip: paused {base['paused']} -> {s['paused']} {missing(r)}")
    r = R.go(["click=top.play", "click=top.play"]); s = state(r)
    check(s["paused"] == "1", "pause in the chip (two clicks give the start state back)")
    r = R.go(["key=space"]); s = state(r)
    check(s["paused"] == "0", "Space plays")
    r = R.go(["click=tool.play"]); s = state(r)
    check(len(missing(r)) == 1 and s["paused"] == base["paused"], "there is NO play button on the toolbar any more: " + str(missing(r)))
    r = R.go(["click=tool.save"], {"PL_SHOT_DIR": os.path.join(work, "shots_nosave")})
    check(len(missing(r)) == 1, "there is NO Save picture button on the toolbar any more: " + str(missing(r)))
    r = R.go(["click=top.today"]); s = state(r)
    check(base["jumped"] == "1" and s["jumped"] == "0", "Today (in the chip)")
    r = R.go(["click=top.chip", "click=time.mode@0.25"]); s = state(r)
    check(not missing(r) and s["jumped"] == "0" and s["paused"] == "0", f"Time panel: Live goes to now and runs: {s['jumped']} {s['paused']} {missing(r)}")
    r = R.go(["click=top.chip", "click=time.mode@0.9", "click=##shelldate", "text=1986-02-09", "click=time.go"]); s = state(r)
    check(not missing(r) and int(s["epochMs"]) == utc_ms(1986, 2, 9, 12, 0), f"Set manually: date field + Go gives 1986-02-09 12:00 UT {s['epochMs']} {missing(r)}")
    r = R.go(["click=top.chip", "click=time.mode@0.9", "click=##shelldate", "text=2003-08-27", "click=time.hour+", "click=time.hour+", "click=time.hour+", "click=time.minute+", "click=time.go"]); s = state(r)
    check(not missing(r) and int(s["epochMs"]) == utc_ms(2003, 8, 27, 15, 1), f"Set manually: hour + minute steppers: 2003-08-27 15:01 UT {s['epochMs']} {missing(r)}")
    r = R.go(["click=top.chip", "click=time.info"]);
    check(any("1800" in t for _, _, t in r.texts), "the (i) in the Time panel shows the help words (1800 to 2100)")
    jumps = [("1986-02-09", utc_ms(1986, 2, 9, 12, 0), False), ("1846-09-23", utc_ms(1846, 9, 23, 12, 0), False), ("2003-08-27", utc_ms(2003, 8, 27, 12, 0), False),
             ("2012-06-06", utc_ms(2012, 6, 6, 1, 26), False), ("2020-12-21", utc_ms(2020, 12, 21, 12, 0), True), ("2061-07-28", utc_ms(2061, 7, 28, 12, 0), True)]
    for date, ms, more in jumps:
        r = R.go(["click=top.chip"] + (["click=time.more"] if more else []) + [f"click=jump.{date}"]); s = state(r)
        check(not missing(r) and int(s["epochMs"]) == ms, f"history jump {date}{' (behind More)' if more else ''}: {s['epochMs']} vs {ms} {missing(r)}")
    r = R.go(["click=top.chip", "click=time.journey"]); s = state(r)
    check(not missing(r) and s["journey"] == "1", "Journey starts from the Time panel (2 clicks)")
    r = R.go(["click=top.chip", "click=time.journey", "click=top.stop"]); s = state(r)
    check(not missing(r) and s["journey"] == "0", "a running Journey is stopped by the Stop button in the chip: " + str(missing(r)))
    # --- speed ---
    r = R.go(["click=tool.speed", "click=speed.slider@0.3"]); s = state(r)
    check(not missing(r) and abs(float(s["tspeed"]) - float(base["tspeed"])) > 0.05, f"Time speed slider: {base['tspeed']} -> {s['tspeed']}")
    # --- the Show panel ---
    for name, field in (("moons", "moons"), ("dwarfs", "dwarfs"), ("comets", "comets"), ("belts", "belts"), ("far", "far")):
        r = R.go(["click=tool.show", f"click=show.{name}"]); s = state(r)
        check(not missing(r) and s[field] != base[field], f"Show: {name} flips {base[field]} -> {s[field]} {missing(r)}")
    for pos, want in ((0.5, "1"), (0.95, "2")):
        r = R.go(["click=tool.show", f"click=show.sky@{pos}"]); s = state(r)
        check(not missing(r) and s["sky"] == want, f"Sky segment at {pos}: sky={s['sky']} (want {want})")
    for pos, want in ((0.5, "1"), (0.95, "2")):
        r = R.go(["click=tool.show", f"click=show.cons@{pos}"]); s = state(r)
        check(not missing(r) and s["cons"] == want, f"Constellations segment at {pos}: cons={s['cons']} (want {want})")
    r = R.go(["click=tool.show", "click=show.physics@0.8", "click=show.gr", "click=show.wobble"]); s = state(r)
    check(not missing(r) and s["real"] == "1" and s["gr"] == "1" and s["wobble"] != base["wobble"], f"Planet physics Real + GR + wobble: {s['real']} {s['gr']} {s['wobble']} {missing(r)}")
    # --- scale ---
    r = R.go(["click=tool.scale", "click=scale.zoom@0.2"]); s = state(r)
    check(not missing(r) and s["zoom"] != base["zoom"], f"Zoom slider: {base['zoom']} -> {s['zoom']}")
    r = R.go(["click=tool.scale", "click=scale.sizes@0.8", "click=scale.trails@0.8"]); s = state(r)
    check(not missing(r) and s["trueScale"] == "0" and s["fullTrails"] == "1", f"Sizes Easy to see + Trails Full: {s['trueScale']} {s['fullTrails']} {missing(r)}")
    # --- 2D / 3D and the view options ---
    r = R.go(["click=tool.view.3d"]); s = state(r)
    check(not missing(r) and s["3D"] == "1", "3D button")
    r = R.go(["click=tool.view.3d", "click=tool.view.caret", "click=view.pan", "click=view.spin"]); s = state(r)
    check(not missing(r) and s["pan"] == "1" and s["spin"] == "1", f"View options: Drag moves the view + Spin: {s['pan']} {s['spin']} {missing(r)}")
    r = R.go(["click=tool.view.caret", "click=view.follow"], {"PL_SELECT": "Earth"}); s = state(r)
    check(not missing(r) and s["follow"] == "0", f"View options: Follow (Earth starts followed): {s['follow']} {missing(r)}")
    r = R.go(["click=tool.scale", "click=scale.zoom@0.2", "key=escape", "click=tool.view.caret", "click=view.reset"]); s = state(r)
    check(not missing(r) and float(s["zoom"]) == 0.0, f"Reset view puts the zoom back to 0: {s['zoom']} {missing(r)}")
    # --- the card's action links (the old column's "Slow time ..." buttons) ---
    r = R.go(["click=card.slowmoons"], {"PL_SELECT": "Earth"}); s = state(r)
    b2 = R.base_state({"PL_SELECT": "Earth"})
    check(not missing(r) and s["tspeed"] != b2["tspeed"] and s["follow"] == "1", f"Card link: Slow time to watch moons (Earth): {b2['tspeed']} -> {s['tspeed']} {missing(r)}")
    r = R.go(["click=card.slowspin"], {"PL_SELECT": "Mercury"}); s = state(r)
    b2 = R.base_state({"PL_SELECT": "Mercury"})
    check(not missing(r) and s["tspeed"] != b2["tspeed"], f"Card link: Slow time to watch it turn (Mercury): {b2['tspeed']} -> {s['tspeed']} {missing(r)}")
    # --- the Old panel is gone in the Orbit Lab ---
    r = R.go(["click=top.old"])
    check(len(missing(r)) == 1 and "panel.old" not in r.rects, "the Orbit Lab offers no Old panel: " + str(missing(r)))
    # --- the menu ---
    shots = os.path.join(work, "shots_menu")
    os.makedirs(shots, exist_ok=True)
    for f in glob.glob(os.path.join(shots, "*")): os.remove(f)
    r = R.go(["click=top.menu", "click=menu.save"], {"PL_SHOT_DIR": shots})
    check(not missing(r) and len(glob.glob(os.path.join(shots, "*.png"))) == 1, f"Save picture in the menu (2 clicks) made a PNG: {glob.glob(os.path.join(shots, '*'))} {missing(r)}")
    r = R.go(["click=top.menu", "click=menu.about"])
    check(not missing(r) and "reading.column" in r.rects, "Menu > About opens the reading view")
    r = R.go(["click=top.menu", "click=menu.history"])
    check(not missing(r) and "reading.column" in r.rects, "Menu > What's new opens the reading view")
    r = R.go(["click=top.menu", "click=menu.changelog"])
    check(not missing(r) and "reading.column" in r.rects, "Menu > Orbit Lab changelog opens the reading view")
    r = R.go(["click=top.menu", "click=menu.defaults"])
    check(not missing(r), "Menu > Put this lab's settings back is there: " + str(missing(r)))
    r = R.go(["click=top.menu", "click=menu.larger"])
    check(not missing(r) and r.window[3] > 1.0, f"Menu > text size A+ makes the text bigger (text scale {r.window[3]})")
    r = R.go(["click=top.learn"])
    check(not missing(r) and "toolbar" not in r.rects, "the Learn icon opens the Learn page")
    r = R.go(["click=top.ach"])
    check(not missing(r) and "toolbar" not in r.rects, "the Achievements icon opens its page")
    r = R.go(["click=top.theme"])
    check(not missing(r), "the Theme icon is there")
    r = R.go(["click=top.settings"])
    check(not missing(r) and r.note("settingsnew"), "Settings opens, with the Facts and Performance sections (VSync, GPU rocks, Benchmark) when the new UI is on: " + str(r.note("settingsnew")))
    r = R.go(["click=top.lab", "click=lab.atom"])
    check(not missing(r) and any(t == "Atom Lab" for _, _, t in r.texts), "the lab menu switches to the Atom Lab")
    # --- the legend ---
    r = R.go(["click=card.legend"])
    need = ["The Sun", "Planets: each has its own colour", "Habitable zone (rough)", "Distance rings, labelled in AU", "The thing you picked", "The little axes: x red, y green, z blue", "A moon"]
    check(not missing(r) and all(any(t == n for _, _, t in r.texts) for n in need), "the Legend opens and lists what the picture draws: " + str([n for n in need if not any(t == n for _, _, t in r.texts)]))
    # --- the picture's own texts live in the readout and chip ---
    r = R.go([])
    check(any(t.startswith("Scale") for _, _, t in r.texts) and any(t.startswith("light takes") for _, _, t in r.texts) and any(t.startswith("Zoom") for _, _, t in r.texts), "the readout shows zoom, scale and light time")
    # --- every name of INVENTORY.md's Go to list ---
    names = inventory_names()
    check(len(names) == 47, f"INVENTORY.md lists 47 Go to names ({len(names)})")
    cmds = []
    for n in names:      # (the filter box brings the name to the top of the list, so the click lands on a row that is in view; it also tests the filter)
        cmds += ["click=tool.pick", "click=##pickfilter", f"text={n}", f"click=pick.{n}", "state"]
    script_parts = []
    f = 10
    for c in cmds:
        script_parts.append(f"{f}:{c}")
        f += 6 if c == "state" else 5
    r = run_app(binary, work, "orb_pick_all", {"PL_NEWUI": "on", "PL_SCRIPT": ";".join(script_parts)}, frames=f + 10, size="1280x800")
    check(not missing(r), "all 47 Go to names are in the pick list: " + str(missing(r)[:3]))
    sels = []
    for line in r.states:
        m = re.search(r"sel=(\d+),(-?\d+),(-?\d+)", line)
        sels.append(m.groups() if m else None)
    check(len(sels) == 47 and all(x and x[0] != "0" for x in sels), f"each of the 47 clicks selected something ({len(sels)} states)")
    check(len(set(sels)) == 47, f"and 47 different things ({len(set(sels))})")
    check(any("Real orbits from NASA/JPL data" in t for _, _, t in run_app(binary, work, "orb_pick_hint", {"PL_NEWUI": "on", "PL_SCRIPT": "10:click=tool.pick"}, frames=30).texts), "the pick list keeps the NASA/JPL words")

# ------------------------------------------------------------------ the card
CARD_THINGS = [("Sun", "Sun"), ("Earth", "Earth"), ("Jupiter", "Jupiter"), ("Pluto", "Pluto"), ("Moon", "Moon"), ("Halley", "Halley"), ("Asteroid belt", "Asteroid belt"), ("Alpha Centauri", "Alpha Centauri")]

def part_card(binary, work):
    for tile, pick in CARD_THINGS:
        # the OLD layout: click the Go to tile; the picture prints the card it draws (PL_LOG_CARD)
        old = run_app(binary, work, f"card_old_{tile.replace(' ', '_')}", {"PL_NEWUI": "off", "PL_LOG_CARD": "1", "PL_SCRIPT": f"10:click={tile}"}, frames=40, size="1000x2800")
        old_cards = [ln[7:].split("\t") for ln in old.out.splitlines() if ln.startswith("PLCARD ")]
        new = run_app(binary, work, f"card_new_{tile.replace(' ', '_')}", {"PL_NEWUI": "on", "PL_SCRIPT": f"10:click=tool.pick;16:click=##pickfilter;22:text={pick};28:click=pick.{pick};40:click=card.all"}, frames=60)
        sheets = [ln[8:].split("\t") for ln in new.out.splitlines() if ln.startswith("PLSHEET ")]
        check(not missing(old) and not missing(new), f"{tile}: the clicks found their controls {missing(old)[:1]} {missing(new)[:1]}")
        check(len(old_cards) >= 1 and len(sheets) >= 1, f"{tile}: both layouts printed a card ({len(old_cards)} old, {len(sheets)} new)")
        if not old_cards or not sheets:
            continue
        o, n = old_cards[-1], sheets[-1]
        old_name = o[0].split(" · ")[0]
        check(n[0] == old_name, f"{tile}: the data sheet's name equals the old card's: {n[0]!r} vs {old_name!r}")
        check(n[1:] == o[1:], f"{tile}: the data sheet's lines equal the old card's lines word for word ({len(n) - 1} vs {len(o) - 1})" + ("" if n[1:] == o[1:] else f"\n  old {o[1:][:3]}\n  new {n[1:][:3]}"))
        keys = [t for k, _, t in new.texts if k == "key"]
        check(keys and any(keys[0] in line for line in o[1:]), f"{tile}: the big number {keys[:1]} is a piece of one of the old card's lines")
        check(any(k == "body" and t == "All values" for k, _, t in new.texts), f"{tile}: the card has its All values button")

# ------------------------------------------------------------------ the Time panel
def part_time(binary, work):
    for size in ("1280x800", "1000x640"):
        for view, script in (("Live", "20:click=top.today;26:click=top.chip"), ("Set manually", "20:click=top.chip;26:click=time.mode@0.9")):
            # (PL_DATE makes the lab "jumped", so the first view is reached through Today)
            r = run_app(binary, work, f"time_{view[:3]}_{size}", {"PL_NEWUI": "on", "PL_SCRIPT": script}, frames=50, size=size)
            panel, tool = r.rects.get("panel.chip"), r.rects.get("toolbar")
            check(panel is not None and tool is not None, f"{view} {size}: the Time panel and the toolbar exist")
            if not panel or not tool:
                continue
            check(panel[1] + panel[3] <= tool[1] - 4 + 0.5, f"{view} {size}: the Time panel ends above the toolbar ({panel[1] + panel[3]:.0f} <= {tool[1] - 4:.0f})")
            need = [n for n in r.notes if n.startswith("panelneed chip")]
            m = re.search(r"need=(\d+) have=(\d+)", need[0]) if need else None
            fit = m and int(m.group(1)) <= int(m.group(2)) + 1
            if view == "Live" or size == "1280x800":
                check(fit, f"{view} {size}: the Time panel's content fits without scrolling ({need})")
            else:
                print(f"note: {view} {size}: the Time panel's content needs more than it has and scrolls inside ({need})") if not fit else None

def part_chip(binary, work):
    # the chip keeps its place while time runs (its words change every frame), so the play and Today buttons inside it never slide under the pointer
    ra = run_app(binary, work, "chip_a", {"PL_NEWUI": "on", "PL_MOTION": "off", "PL_SCRIPT": "4:click=top.today"}, frames=20)
    rb = run_app(binary, work, "chip_b", {"PL_NEWUI": "on", "PL_MOTION": "off", "PL_SCRIPT": "4:click=top.today"}, frames=400)
    check("chip" in ra.rects and "chip" in rb.rects, "the chip is drawn")
    if "chip" in ra.rects and "chip" in rb.rects:
        check(ra.rects["chip"] == rb.rects["chip"], f"the chip has the same place and width after 20 and after 400 frames of running time: {ra.rects['chip']} {rb.rects['chip']}")
    # the date, in the same chip at another date, still fits
    rd = run_app(binary, work, "chip_d", {"PL_NEWUI": "on", "PL_MOTION": "off", "PL_DATE": "2099-09-30"}, frames=20)
    check("chip" in rd.rects and rd.rects["chip"][2] >= ra.rects["chip"][2] - 0.5, "a late date does not make the chip smaller")

# ------------------------------------------------------------------ the fact line
def factline(r):
    t = r.note("factline")
    return t[0][len("factline "):] if t else None

def part_facts(binary, work):
    FULL = {"PL_NEWUI": "on", "PL_MOTION": "off"}                # (this test harness runs with Motion Reduced by default: there the line never changes by itself; Full motion here)
    r1 = run_app(binary, work, "fact_a", dict(FULL, PL_FACT_INTERVAL="2"), frames=70)
    r2 = run_app(binary, work, "fact_b", dict(FULL, PL_FACT_INTERVAL="2"), frames=240)
    f1, f2 = factline(r1), factline(r2)
    check(f1 is not None and "factline" in r1.rects, "the fact line is visible by default")
    a = [n for n in r1.notes if n.startswith("factalpha")]
    check(a and float(a[0].split()[1]) == 1.0, "and fully visible after a second: " + str(a))
    check(f1 and f2 and f1 != f2, f"with a 2 s interval the fact changes after 3.8 s: {f1!r} -> {f2!r}")
    # Motion Reduced: no automatic change, the text is simply there
    rr = run_app(binary, work, "fact_reduced", {"PL_NEWUI": "on", "PL_MOTION": "on", "PL_FACT_INTERVAL": "2"}, frames=300)
    rr0 = run_app(binary, work, "fact_reduced0", {"PL_NEWUI": "on", "PL_MOTION": "on", "PL_FACT_INTERVAL": "2"}, frames=40)
    check(factline(rr) is not None and factline(rr) == factline(rr0), "with Motion Reduced the fact does not change by itself")
    # hover: the pointer on the line stops the clock
    rh = run_app(binary, work, "fact_hover", dict(FULL, PL_FACT_INTERVAL="2", PL_SCRIPT="30:move=fact.line"), frames=400)
    rh0 = run_app(binary, work, "fact_hover0", dict(FULL, PL_FACT_INTERVAL="2", PL_SCRIPT="30:move=fact.line"), frames=50)
    check(not missing(rh) and factline(rh) == factline(rh0), f"hovering stops the change (400 frames, 2 s interval): {factline(rh0)!r} -> {factline(rh)!r}")
    ah = [n for n in rh.notes if n.startswith("factalpha")]
    check(ah and float(ah[0].split()[1]) == 1.0, "and the text stays fully visible: " + str(ah))
    # a click opens the whole fact, with its source line
    rk = run_app(binary, work, "fact_click", {"PL_NEWUI": "on", "PL_SCRIPT": "40:click=fact.line"}, frames=70)
    check(not missing(rk) and "panelopen facts" in rk.notes, "a click on the line opens the whole fact: " + str(rk.note("panelopen")))
    check(any("source" in t for _, _, t in rk.texts), "the open fact says what is known about its source")
    check("panel.facts" in rk.rects and inside(rk.rects["panel.facts"], 1280, 800), "and the panel is inside the window")
    # the arrows step (on hover)
    rs = run_app(binary, work, "fact_step", {"PL_NEWUI": "on", "PL_SCRIPT": "30:move=fact.line;36:click=fact.next"}, frames=70)
    ra = run_app(binary, work, "fact_step_a", {"PL_NEWUI": "on"}, frames=70)
    check(not missing(rs) and factline(rs) != factline(ra), f"the arrow steps to the next fact: {factline(ra)!r} -> {factline(rs)!r}")
    # off
    ro = run_app(binary, work, "fact_off", {"PL_NEWUI": "on", "PL_FACTS": "off"}, frames=70)
    check("factline" not in ro.rects and factline(ro) is None, "Settings > Facts Off hides the line")
    rt = run_app(binary, work, "fact_tile", {"PL_NEWUI": "on", "PL_SCRIPT": "20:click=top.settings;30:move=640,400;34:wheel=-40;40:click=facts-off"}, frames=90)
    check(not missing(rt) and "factline" not in rt.rects, "the Facts Off tile in Settings hides it: " + str(missing(rt)))

def part_facts_long(binary, work):
    FULL = {"PL_NEWUI": "on", "PL_MOTION": "off"}
    # the real interval: about 20 s = 1282 frames of 16 ms
    ra = run_app(binary, work, "fact_real_a", FULL, frames=90, size="880x560")
    rb = run_app(binary, work, "fact_real_b", FULL, frames=1330, size="880x560")
    rc = run_app(binary, work, "fact_real_c", FULL, frames=1200, size="880x560")
    check(factline(ra) and factline(rb) and factline(ra) != factline(rb), f"with the real 20 s interval the fact has changed after 21.3 s: {factline(ra)!r} -> {factline(rb)!r}")
    check(factline(ra) == factline(rc), f"and has NOT changed after 19.2 s: {factline(ra)!r} -> {factline(rc)!r}")
    ra = run_app(binary, work, "fact_step_a", {"PL_NEWUI": "on"}, frames=70)
    # no overlap, any size, with panels open
    for size in ("1280x800", "1000x640", "880x560"):
        for name, env in (("closed", {}), ("select", {"PL_SELECT": "Earth"}), ("show", {"PL_SCRIPT": "20:click=tool.show"}), ("time", {"PL_SCRIPT": "20:click=top.chip"}), ("card", {"PL_SELECT": "Earth", "PL_SCRIPT": "20:click=card.all"})):
            r = run_app(binary, work, f"fact_ov_{name}_{size}", dict({"PL_NEWUI": "on"}, **env), frames=60, size=size)
            fl = r.rects.get("factline")
            if fl is None:
                print(f"note: {name} {size}: the fact line is left out (something is in its place)")
                continue
            w, h = r.window[0], r.window[1]
            check(inside(fl, w, h), f"{name} {size}: the fact line is inside the window {fl}")
            for other, rc in r.rects.items():
                if other == "factline" or other.startswith("occ"):
                    continue
                check(not overlap(fl, rc), f"{name} {size}: the fact line does not overlap {other} {fl} {rc}")
    # never over the selected body: for 12 bodies at their own places
    for pick in ("Mercury", "Earth", "Mars", "Jupiter", "Saturn", "Neptune", "Pluto", "Ceres", "Halley", "Moon", "Sun", "Eris"):
        r = run_app(binary, work, f"fact_body_{pick}", {"PL_NEWUI": "on", "PL_SCRIPT": f"10:click=tool.pick;16:click=##pickfilter;22:text={pick};28:click=pick.{pick}"}, frames=150, size="1000x640")
        fl = r.rects.get("factline")
        b = [n for n in r.note("body")]
        if fl and b:
            _, x, y, rad = b[0].split()
            x, y, rad = float(x), float(y), float(rad)
            hit = fl[0] - rad < x < fl[0] + fl[2] + rad and fl[1] - rad < y < fl[1] + fl[3] + rad
            check(not hit, f"{pick}: the fact line {fl} is not over the selected body ({x:.0f},{y:.0f} r={rad:.0f})")


# ------------------------------------------------------------------ the fact line's pixels
def lum(c):
    def lin(v):
        v /= 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    return 0.2126 * lin(c[0]) + 0.7152 * lin(c[1]) + 0.0722 * lin(c[2])

def wcag(a, b):
    la, lb = lum(a), lum(b)
    hi, lo = max(la, lb), min(la, lb)
    return (hi + 0.05) / (lo + 0.05)

def part_readable(binary, work):
    # In the real picture the fact line is found by DIFFERENCE: the same picture with the line and with Settings > Facts Off; the pixels that differ are the letters and their shadow. The
    # lightest and the darkest of them must be 4.5:1 apart (WCAG 2.x, the formula written here in Python), in every theme, over the dark space picture AND over a BRIGHT one (the
    # Sun's glow). The shadow is what keeps the letters readable over white: without it the changed pixels would be the letters alone and this fails.
    themes = ("dark", "light", "black", "paper", "monochrome", "aero", "y2kice", "y2knight", "contrast")
    for theme in themes:
        for name, env in (("space", {}), ("bright", {"PL_ZOOM": "2.2"})):
            on = run_app(binary, work, f"read_{theme}_{name}_on", dict({"PL_NEWUI": "on", "PL_FACTS": "on"}, **env), frames=50, size="1280x800", theme=theme)
            off = run_app(binary, work, f"read_{theme}_{name}_off", dict({"PL_NEWUI": "on", "PL_FACTS": "off"}, **env), frames=50, size="1280x800", theme=theme)
            fl = on.rects.get("factline")
            if fl is None:
                print(f"note: {theme} {name}: no fact line this time"); continue
            b1, b0 = Bmp(on.bmp), Bmp(off.bmp)
            x0, x1, y0, y1 = int(fl[0]), int(fl[0] + fl[2] - 60), int(fl[1]), int(fl[1] + fl[3])
            changed = [b1.px(x, y) for y in range(y0, y1) for x in range(x0, x1) if b1.px(x, y) != b0.px(x, y)]
            check(len(changed) > 200, f"{theme} ({name}): the line drew {len(changed)} pixels of its own")
            if len(changed) > 200:
                lo = min(changed, key=lum); hi = max(changed, key=lum)
                check(wcag(lo, hi) >= 4.5, f"{theme} ({name}): the letters and their shadow are {wcag(lo, hi):.1f}:1 apart (need 4.5) {lo} {hi}")

# ------------------------------------------------------------------ search
def part_search(binary, work):
    R = Runner(binary, work)
    base = R.base_state()
    shots = os.path.join(work, "shots_search")
    os.makedirs(shots, exist_ok=True)
    for f in glob.glob(os.path.join(shots, "*")): os.remove(f)
    def q(text, env=None, extra=()):
        return R.go(["key=ctrl+k", f"text={text}", "key=enter"] + list(extra), env)
    def sel(s): return tuple(int(x) for x in re.search(r"sel=(\d+),(-?\d+),(-?\d+)", "sel=" + s["sel"] if "sel" in s else "").groups()) if "sel" in s else None
    def selected(r):
        m = re.search(r"sel=(\d+),(-?\d+),(-?\d+)", r.states[-1])
        return tuple(int(x) for x in m.groups())
    found = 0
    # 1-7: bodies of every kind
    for text, kind in (("earth", 3), ("pluto", 3), ("halley", 3), ("asteroid", 5), ("sirius", 6), ("voyager 1", 6), ("moon", 4)):
        r = q(text); k = selected(r)
        ok = not missing(r) and k[0] == kind
        found += ok
        check(ok, f"search '{text}' + Enter selects it (kind {k[0]}, want {kind})")
    # 8-9: history dates
    for text, ms in (("neptune found", utc_ms(1846, 9, 23, 12, 0)), ("mars closest", utc_ms(2003, 8, 27, 12, 0))):
        r = q(text); s = state(r)
        ok = int(s["epochMs"]) == ms
        found += ok
        check(ok, f"search '{text}' + Enter jumps to the date {s['epochMs']} vs {ms}")
    # 10-18: settings
    for text, field, want in (("dwarf planets", "dwarfs", "0"), ("planet physics: real", "real", "1"), ("3d view", "3D", "1"), ("trails: full", "fullTrails", "1"), ("constellations: lines and names", "cons", "2"),
                              ("stars: off", "sky", "2"), ("sizes: easy", "trueScale", "0"), ("journey", "journey", "1"), ("far-away", "far", "0")):
        r = q(text); s = state(r)
        ok = s[field] == want
        found += ok
        check(ok, f"search '{text}' + Enter sets {field}={want} (got {s[field]})")
    # 19: Save picture, 20: About, 21: Settings, 22: a fact, 23: text size
    r = q("save picture", {"PL_SHOT_DIR": shots})
    ok = len(glob.glob(os.path.join(shots, "*.png"))) == 1
    found += ok
    check(ok, "search 'save picture' + Enter saves a picture: " + str(glob.glob(os.path.join(shots, "*"))))
    r = q("about physics")
    ok = "reading.column" in r.rects
    found += ok
    check(ok, "search 'about physics' + Enter opens About")
    r = q("settings")
    ok = bool(r.note("settingsnew"))
    found += ok
    check(ok, "search 'settings' + Enter opens Settings (the first hit is the Settings window, not a setting): " + str(r.note("searchhit")))
    r = q("sunlight takes")
    ok = "panelopen facts" in r.notes and any("8 minutes" in t for _, _, t in r.texts)
    found += ok
    check(ok, "search 'sunlight takes' finds the fact and opens it")
    r = q("text size")
    ok = "panelopen menu" in r.notes
    found += ok
    check(ok, "search 'text size' opens the menu's text size")
    check(found >= 20, f"at least 20 named things were found, selected or opened by search ({found})")
    # arrows: the second hit
    r = R.go(["key=ctrl+k", "text=moon", "key=down", "key=enter"]); s = state(r)
    check(s["moons"] == "0", f"arrow Down + Enter takes the second hit of 'moon' (Moons: turn off): moons={s['moons']}")
    # recent choices come first
    r = R.go(["key=ctrl+k", "text=pluto", "key=enter", "key=ctrl+k"])
    check(r.note("searchhit") and r.note("searchhit")[0] == "searchhit Pluto", "the search box lists the recent choice first: " + str(r.note("searchhit")))
    r = R.go(["key=ctrl+k", "text=s", "key=enter", "key=ctrl+k", "text=s"])
    hit = r.note("searchhit")
    check(hit and hit[0] != "", "a query still works after a recent choice: " + str(hit))
    # Orbit Lab: no Old panel in search
    r = R.go(["key=ctrl+k", "text=old panel"])
    check(r.note("searchcount") == ["searchcount 0"], "search offers no Old panel in the Orbit Lab: " + str(r.note("searchcount")))

# ------------------------------------------------------------------ links
def part_links(binary, work):
    r = run_app(binary, work, "links", {"PL_NEWUI": "on", "PL_OPEN": "about", "PL_SCRIPT": ";".join(f"{20 + 6 * i}:clickxy=640,{700 if i % 2 else 100}" for i in range(0))}, frames=40, size="1280x800")
    pieces = {}
    for ln in r.out.splitlines():
        m = re.match(r"PLLINK id=(\S+) y=([\d.]+) x0=([\d.]+) x1=([\d.]+)", ln)
        if m:
            pieces.setdefault((m.group(1), m.group(2)), []).append((float(m.group(3)), float(m.group(4))))
    print(f"links: {sum(len(v) for v in pieces.values())} underline pieces on the first screen of About")
    gaps = 0
    for key, segs in pieces.items():
        segs.sort()
        for a, b in zip(segs, segs[1:]):
            if 0.5 < b[0] - a[1] < 20:
                gaps += 1
    check(gaps == 0, f"no gap inside one link on one line ({gaps} gaps)")
    # the same for the lines further down: the sources part of About
    r = run_app(binary, work, "links2", {"PL_NEWUI": "on", "PL_OPEN": "about", "PL_SCROLL": "60000"}, frames=40, size="1280x800")

def main():
    binary, work = sys.argv[1], sys.argv[2]
    only = set(sys.argv[3].split(",")) if len(sys.argv) > 3 else None
    want = lambda n: only is None or n in only
    os.makedirs(work, exist_ok=True)
    for name, fn in (("coverage", part_coverage), ("card", part_card), ("time", part_time), ("chip", part_chip), ("facts", part_facts), ("facts_long", part_facts_long), ("readable", part_readable), ("search", part_search), ("links", part_links)):
        if want(name):
            fn(binary, work)
    print(f"{checks} checks, {fails} failed")
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
