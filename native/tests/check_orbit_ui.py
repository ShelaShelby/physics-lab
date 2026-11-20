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
#   search    20 named things are found, selected or opened with Enter; arrows move; recent choices only rank equal matches higher (5l: an empty box lists nothing)
#   links     in the reading view a link's underline is one line (no gap between two words of the same link)
#   airedge   0.50: just outside Earth's limb the picture is the sky (no blue rim of air)
#   nightside 0.50: Jupiter's night side is the sky's colour (no ambient light)
import datetime, glob, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, inside, overlap, Bmp, prefetch, old_run

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
             ("2012-06-06", utc_ms(2012, 6, 6, 1, 30), False), ("2020-12-21", utc_ms(2020, 12, 21, 12, 0), True), ("2061-07-28", utc_ms(2061, 7, 28, 12, 0), True)]
    # (0.39.3, on purpose: the transit jump lands at NASA's greatest transit, 01:29:36 UT, rounded to 01:30; it was 01:26)
    for date, ms, more in jumps:
        r = R.go(["click=top.chip"] + (["click=time.more"] if more else []) + [f"click=jump.{date}"]); s = state(r)
        check(not missing(r) and int(s["epochMs"]) == ms, f"history jump {date}{' (behind More)' if more else ''}: {s['epochMs']} vs {ms} {missing(r)}")
    r = R.go(["click=top.chip", "click=time.journey"]); s = state(r)
    check(not missing(r) and s["journey"] == "1", "Journey starts from the Time panel (2 clicks)")
    r = R.go(["click=top.chip", "click=time.journey", "click=top.stop"]); s = state(r)
    check(not missing(r) and s["journey"] == "0", "a running Journey is stopped by the Stop button in the chip: " + str(missing(r)))
    # --- speed ---
    r = R.go(["click=speed.slider@0.3"]); s = state(r)
    check(not missing(r) and abs(float(s["tspeed"]) - float(base["tspeed"])) > 0.05, f"Time speed slider: {base['tspeed']} -> {s['tspeed']}")
    # --- the Show panel ---
    for name, field in (("moons", "moons"), ("dwarfs", "dwarfs"), ("comets", "comets"), ("belts", "belts"), ("far", "far")):
        r = R.go([f"click=show.{name}"]); s = state(r)
        check(not missing(r) and s[field] != base[field], f"Show: {name} flips {base[field]} -> {s[field]} {missing(r)}")
    for pos, want in ((0.5, "1"), (0.95, "2")):
        r = R.go([f"click=show.sky@{pos}"]); s = state(r)
        check(not missing(r) and s["sky"] == want, f"Sky segment at {pos}: sky={s['sky']} (want {want})")
    for pos, want in ((0.35, "1"), (0.95, "2")):      # (0.35: the middle of "Lines"; in the 22 em Controls panel the three segments are as wide as their words need, so "Lines" is not the middle third any more)
        r = R.go([f"click=show.cons@{pos}"]); s = state(r)
        check(not missing(r) and s["cons"] == want, f"Constellations segment at {pos}: cons={s['cons']} (want {want})")
    r = R.go(["click=show.physics@0.8", "click=show.gr", "click=show.wobble"]); s = state(r)
    check(not missing(r) and s["real"] == "1" and s["gr"] != base["gr"] and s["wobble"] != base["wobble"], f"Planet physics Real + GR flips (0.29: on by default) + wobble: {base['gr']} -> {s['gr']} {s['wobble']} {missing(r)}")
    # --- scale ---
    r = R.go(["click=scale.zoom@0.2"]); s = state(r)
    check(not missing(r) and s["zoom"] != base["zoom"], f"Zoom slider: {base['zoom']} -> {s['zoom']}")
    r = R.go(["click=scale.sizes@0.8", "click=scale.trails@0.8"]); s = state(r)
    check(not missing(r) and s["trueScale"] == "0" and s["fullTrails"] == "1", f"Sizes Easy to see + Trails Full: {s['trueScale']} {s['fullTrails']} {missing(r)}")
    # --- 2D / 3D and the view options ---
    r = R.go(["click=tool.view.3d"]); s = state(r)
    check(not missing(r) and s["3D"] == "1", "3D button")
    r = R.go(["click=tool.view.3d", "click=view.pan", "click=view.spin"]); s = state(r)
    check(not missing(r) and s["pan"] == "1" and s["spin"] == "1", f"View options: Drag moves the view + Spin: {s['pan']} {s['spin']} {missing(r)}")
    r = R.go(["click=view.follow"], {"PL_SELECT": "Earth"}); s = state(r)
    check(not missing(r) and s["follow"] == "0", f"View options: Follow (Earth starts followed): {s['follow']} {missing(r)}")
    r = R.go(["click=scale.zoom@0.2", "click=view.reset"]); s = state(r)
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
    r = R.go(["click=top.menu", "click=menu.defaults"])
    check(not missing(r), "Menu > Put this lab's settings back is there: " + str(missing(r)))
    r = R.go(["click=top.menu", "click=menu.larger"])
    check(not missing(r) and r.window[3] > 1.0, f"Menu > text size A+ makes the text bigger (text scale {r.window[3]})")
    r = R.go(["click=top.learn"])
    check(not missing(r) and "toolbar" not in r.rects, "the Learn icon opens the Learn page")
    check("top.ach" not in r.rects and "topicon.ach" not in r.rects, "there is no Achievements icon (removed in 0.52.0)")
    r = R.go(["click=top.theme"])
    check(not missing(r), "the Theme icon is there")
    r = R.go(["click=top.settings"])
    check(not missing(r) and r.note("settingsnew"), "Settings opens, with the Facts and Performance sections (VSync, GPU rocks, Benchmark) when the new UI is on: " + str(r.note("settingsnew")))
    r = R.go(["click=top.tab.atom"])                                   # 0.38: the lab tabs replace the lab menu of the breadcrumb
    check(not missing(r) and any(t == "Atom Lab" for _, _, t in r.texts) and "top.tab.atom.on" in r.rects, "the Atom Lab tab switches to the Atom Lab")
    # --- the legend ---
    r = R.go(["click=card.legend"])
    need = ["The Sun", "Planets: each has its own colour", "Habitable zone (Kopparapu et al. 2014)", "Distance rings, labelled in AU", "The thing you picked", "The little axes: x red, y green, z blue", "A moon"]
    check(not missing(r) and all(any(t == n for _, _, t in r.texts) for n in need), "the Legend opens and lists what the picture draws: " + str([n for n in need if not any(t == n for _, _, t in r.texts)]))
    # --- the picture's own texts live in the readout and chip ---
    r = R.go([])
    # (0.22 stage 6: the readout draws a scale bar with its length, e.g. "3 AU", and the light time; the zoom factor is only in Controls › Zoom)
    import re as _re
    check(any(_re.fullmatch(r"(≈ )?[\d,.]+ (million )?(km|mi|AU)", t) for _, _, t in r.texts) and any(t.startswith("light takes") for _, _, t in r.texts) and not any(t.startswith("Zoom ×") for _, _, t in r.texts), "the readout shows the scale bar's length and the light time, and no longer repeats the zoom")
    # --- every name of INVENTORY.md's Go to list ---
    names = inventory_names()
    check(len(names) == 47, f"INVENTORY.md lists 47 Go to names ({len(names)})")
    cmds = []
    for n in names:      # (the filter box brings the name to the top of the list, so the click lands on a row that is in view; it also tests the filter)
        cmds += ["click=##pickfilter", f"text={n}", f"click=pick.{n}", "state"]
    script_parts = ["4:click=ctl.sec.pick"]       # (the Pick a body section starts folded; it stays open between the clicks)
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
    check(any("Real orbits from NASA/JPL data" in t for _, _, t in run_app(binary, work, "orb_pick_hint", {"PL_NEWUI": "on", "PL_SCRIPT": "10:click=ctl.sec.pick"}, frames=30).texts), "the pick list keeps the NASA/JPL words")

# ------------------------------------------------------------------ the card
CARD_THINGS = [("Sun", "Sun"), ("Earth", "Earth"), ("Jupiter", "Jupiter"), ("Pluto", "Pluto"), ("Moon", "Moon"), ("Halley", "Halley"), ("Asteroid belt", "Asteroid belt"), ("Alpha Centauri", "Alpha Centauri")]

def part_card(binary, work):
    for tile, pick in CARD_THINGS:
        # the OLD layout: click the Go to tile; the picture prints the card it draws (PL_LOG_CARD)
        old = old_run("orbit_ui", f"card_old_{tile.replace(' ', '_')}", {"PL_LOG_CARD": "1", "PL_SCRIPT": f"10:click={tile}"}, frames=40, size="1000x2800")   # (saved from the last build with the old layout: tests/reference/old_ui_runs.json)
        old_cards = [ln[7:].split("\t") for ln in old.out.splitlines() if ln.startswith("PLCARD ")]
        new = run_app(binary, work, f"card_new_{tile.replace(' ', '_')}", {"PL_NEWUI": "on", "PL_SCRIPT": f"10:click=ctl.sec.pick;16:click=##pickfilter;22:text={pick};28:click=pick.{pick};40:click=card.all"}, frames=60)
        sheets = [ln[8:].split("\t") for ln in new.out.splitlines() if ln.startswith("PLSHEET ")]
        check(not missing(old) and not missing(new), f"{tile}: the clicks found their controls {missing(old)[:1]} {missing(new)[:1]}")
        check(len(old_cards) >= 1 and len(sheets) >= 1, f"{tile}: both layouts printed a card ({len(old_cards)} old, {len(sheets)} new)")
        if not old_cards or not sheets:
            continue
        o, n = old_cards[-1], sheets[-1]
        old_name = o[0].split(" · ")[0]
        check(n[0] == old_name, f"{tile}: the data sheet's name equals the old card's: {n[0]!r} vs {old_name!r}")
        n = [n[0]] + [x.replace(" × Earth (equatorial radius ", " × Earth (radius ") for x in n[1:]]   # (0.22 stage 6, on purpose: planet cards say "equatorial radius"; the saved old card says "radius")
        est = " (an estimate, not measured: a bare ball that reflects 30 % of the sunlight)"   # (0.25, on purpose: the estimated temperature says it is one; the saved old card says "(no atmosphere; assumes 30 % reflected)")
        n = [n[0]] + [x.replace("Temperature: about ", "Temperature: ").replace(est, " (no atmosphere; assumes 30 % reflected)") if x.endswith(est) else x for x in n[1:]]
        # (0.26 stage 3, on purpose: the audit added the moon card's "not the real place" line, and Alpha Centauri's distance is now
        #  Pourbaix & Boffin 2016's 4.39 ly (4.34 matched no source); the saved old card has neither change)
        n = [n[0]] + [x for x in n[1:] if not x.startswith("Its place on the orbit is not the real one")]
        n = [n[0]] + [x for x in n[1:] if not x.startswith("Shape: ")]
        n = [n[0]] + [x[:-len(" (mean)")] if x.startswith("Distance from ") and x.endswith(" (mean)") else x for x in n[1:]]   # (0.39.2, on purpose: a moon's distance says it is the mean; the saved old card does not)
        n = [n[0]] + [x.replace("Tipped past its side, so it turns backwards", "Upside down, so it turns backwards") for x in n[1:]]   # (0.39.3, on purpose: Pluto at 120° is not upside down; the saved old card says so)
        n = [n[0]] + [x.replace("Radius: 1,737.4 km", "Radius: 1737.4 km") for x in n[1:]]   # (0.39.3, on purpose: moon radii get thousands commas like every other km; the saved old card has none)
        n = [n[0]] + [re.sub(r"^Orbit: near-circle, [0-9.]+ to [0-9.]+ AU$", "Orbit: near-circle", x) for x in n[1:]]
        # (0.46.3, on purpose: a planet's card gives the gravity you would feel at the equator and the pull alone on its own line; the saved old card gives the pull, one decimal)
        pull = next((m.group(1) for x in n[1:] for m in [re.match(r"^Gravity alone ([0-9.]+) g there", x)] if m), None)
        if pull is not None:
            g = float(pull)
            n = [n[0]] + [re.sub(r"you would feel [0-9.]+ g at the equator$", f"gravity {g:.{3 if g < 0.1 else 1}f} g at the equator", x) for x in n[1:] if not x.startswith("Gravity alone ")]
        if tile == "Moon":   # (0.46.3, on purpose: the Moon's card gives its real phase and distance now (ELP2000-82B); the saved old card has neither)
            live = ("Phase now:", "It is ", "Distance from Earth right now:", "Real position and phase:", "The four main phases", "quarter) are exact", "gibbous) use", "98% lit")
            n = [n[0]] + [x for x in n[1:] if not x.startswith(live)]   # (0.46.6, on purpose: a near-circle gives its closest and farthest distance; the saved old card does not)
        n = [n[0]] + [x.replace("Dots here: the 1,800 largest known, at their real places (JPL)", "Dots here: 1,800 stand in for the millions") for x in n[1:]]   # (0.27, on purpose: the belt is real asteroids now)   # (0.27, on purpose: the real shapes add a Shape line; the saved old card has none)
        audit = {"4.39 light years (278,000 AU)": "4.34 light years (275,000 AU)", "left it 4.39 years": "left it 4.34 years"}
        if tile == "Alpha Centauri":
            for a, b in audit.items():
                n = [n[0]] + [x.replace(a, b) for x in n[1:]]
            n = [n[0]] + [x.replace("Voyager 1 would take about 78,000 years", "Voyager 1 would take about 77,000 years") for x in n[1:]]   # 277,600 AU at 3.561 AU/yr (16.882 km/s)
        # (0.48, on purpose: the review: the Sun's card says its patches are only spin markers; a far comet's card says no tails are drawn beyond 4 AU (a drawing rule)
        # instead of "frozen and dark, no tail", false as a rule; the saved old cards have neither)
        n = [n[0]] + [x for x in n[1:] if not x.startswith("The patches drawn on it are only markers")]
        # (0.49.4, on purpose: the outside review: the habitable zone comes from Kopparapu et al. 2014, 0.95 - 1.68 AU (was "rough", 0.95 - 1.67, no source), and the Sun's card
        # says it is drawn in its measured colour with the app's own darker edge; the saved old card has neither)
        n = [n[0]] + [x.replace("Habitable zone (Kopparapu et al. 2014): 0.95 – 1.68 AU", "Habitable zone (rough): 0.95 – 1.67 AU") for x in n[1:] if not x.startswith("Drawn in its measured colour (nearly white)")]
        n = [n[0]] + [x.replace("No tails drawn beyond 4 AU (a drawing rule: real comets can be active farther out)", "Far from the Sun: frozen and dark, no tail").replace("Tails drawn for show: their length and shape are not to scale", "Warmed by the Sun: glowing head and two tails") for x in n[1:]]
        if tile == "Halley":   # (0.39.3, on purpose: the note gives the real 1986 perihelion, 9 Feb (JPL), and the simple orbit's 8 Feb; the saved old card has neither)
            cut = lambda xs: next((i for i, x in enumerate(xs) if x.startswith("Seen from Earth")), len(xs))
            i = cut(n)
            tail = " ".join(n[i:]).replace("closest to the Sun on 9 Feb 1986 (JPL), next due 28 July 2061 (JPL’s prediction); this lab’s simple orbit puts these on 8 Feb 1986 and in January 2062.",
                                           "last in 1986, next due 28 July 2061 (JPL’s prediction; this lab’s simple orbit puts it in January 2062).")
            j = cut(o)
            n, o = n[:i] + [tail], o[:j] + [" ".join(o[j:])]
        # (0.52.1, on purpose: Simple draws Earth off the Earth-Moon barycentre too, and its card says so; the saved old card has no such line)
        n = [n[0]] + [x for x in n[1:] if not x.startswith("Earth is drawn off the Earth–Moon balance point")]
        check(n[1:] == o[1:], f"{tile}: the data sheet's lines equal the old card's lines word for word ({len(n) - 1} vs {len(o) - 1})" + ("" if n[1:] == o[1:] else f"\n  old {o[1:][:3]}\n  new {n[1:][:3]}"))
        keys = [t.replace("4.39 light years", "4.34 light years") for k, _, t in new.texts if k == "key"]   # (0.26 stage 3, the same on-purpose change)
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
    check(any(t.startswith("Source: ") and len(t) > 12 for _, _, t in rk.texts), "the open fact names its own source (0.40: 'Source: ...'; it said sources were not linked per fact)")
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
    ENVS = (("closed", {}), ("select", {"PL_SELECT": "Earth"}), ("nocontrols", {"PL_SCRIPT": "20:click=ctl.handle"}), ("time", {"PL_SCRIPT": "20:click=top.chip"}), ("card", {"PL_SELECT": "Earth", "PL_SCRIPT": "20:click=card.all"}))
    BODIES = ("Mercury", "Earth", "Mars", "Jupiter", "Saturn", "Neptune", "Pluto", "Ceres", "Halley", "Moon", "Sun", "Eris")
    SHORT = dict(FULL, PL_FACT_INTERVAL="2")   # 0.38.1: a 2 s interval (the app's own test switch): the 20 s timer itself is held by test_shell.inc (factTick at 16 ms steps)
    prefetch([((binary, work, "fact_real_a", SHORT), dict(frames=40, size="880x560")), ((binary, work, "fact_real_b", SHORT), dict(frames=200, size="880x560")), ((binary, work, "fact_real_c", SHORT), dict(frames=110, size="880x560"))] +
             [((binary, work, f"fact_ov_{n}_{sz}", dict({"PL_NEWUI": "on"}, **e)), dict(frames=60, size=sz)) for sz in ("1280x800", "880x560") for n, e in ENVS] +
             [((binary, work, f"fact_body_{b}", {"PL_NEWUI": "on", "PL_SCRIPT": f"10:click=ctl.sec.pick;16:click=##pickfilter;22:text={b};28:click=pick.{b}"}), dict(frames=150, size="1000x640")) for b in BODIES])
    # 0.38.1: the app's wiring of the interval, with a 2 s interval instead of the real 20 s (2,500 frames of software rendering, about 5 min of CPU, for what the unit test
    # test_shell.inc already proves at 16 ms steps): the fact changes after 3.2 s (interval + fade = 2.5 s) and not after 1.76 s
    ra = run_app(binary, work, "fact_real_a", SHORT, frames=40, size="880x560")
    rb = run_app(binary, work, "fact_real_b", SHORT, frames=200, size="880x560")
    rc = run_app(binary, work, "fact_real_c", SHORT, frames=110, size="880x560")
    check(factline(ra) and factline(rb) and factline(ra) != factline(rb), f"with a 2 s interval the fact has changed after 3.2 s: {factline(ra)!r} -> {factline(rb)!r}")
    check(factline(ra) == factline(rc), f"and has NOT changed after 1.76 s: {factline(ra)!r} -> {factline(rc)!r}")
    ra = run_app(binary, work, "fact_step_a", {"PL_NEWUI": "on"}, frames=70)
    # no overlap, any size, with panels open
    for size in ("1280x800", "880x560"):   # 0.38.1: 1000x640 dropped (between the two)
        for name, env in (("closed", {}), ("select", {"PL_SELECT": "Earth"}), ("nocontrols", {"PL_SCRIPT": "20:click=ctl.handle"}), ("time", {"PL_SCRIPT": "20:click=top.chip"}), ("card", {"PL_SELECT": "Earth", "PL_SCRIPT": "20:click=card.all"})):
            r = run_app(binary, work, f"fact_ov_{name}_{size}", dict({"PL_NEWUI": "on"}, **env), frames=60, size=size)
            fl = r.rects.get("factline")
            if fl is None:
                print(f"note: {name} {size}: the fact line is left out (something is in its place)")
                continue
            w, h = r.window[0], r.window[1]
            check(inside(fl, w, h), f"{name} {size}: the fact line is inside the window {fl}")
            for other, rc in r.rects.items():
                if other == "factline" or other.startswith("occ") or other.startswith("ringlabel.") or other.startswith("ringskip."):   # (the picture's own ring labels, logged since stage 5c, are not panels)
                    continue
                check(not overlap(fl, rc), f"{name} {size}: the fact line does not overlap {other} {fl} {rc}")
    # never over the selected body: for 12 bodies at their own places
    for pick in ("Mercury", "Earth", "Mars", "Jupiter", "Saturn", "Neptune", "Pluto", "Ceres", "Halley", "Moon", "Sun", "Eris"):
        r = run_app(binary, work, f"fact_body_{pick}", {"PL_NEWUI": "on", "PL_SCRIPT": f"10:click=ctl.sec.pick;16:click=##pickfilter;22:text={pick};28:click=pick.{pick}"}, frames=150, size="1000x640")
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
    themes = ("dark", "light", "black", "paper", "monochrome", "contrast")
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
    # 0.15 stage 5l (changed on purpose, owner's bug "the result stays after I delete the text"): the recent choices are NOT listed as results in an empty box any more (the thing just chosen came back
    # and looked stuck); they only rank equal matches higher (the unit test "recent:" in test_shell.inc checks the ranking). So a new search after a choice starts empty:
    r = R.go(["key=ctrl+k", "text=pluto", "key=enter", "key=ctrl+k"])
    check(r.note("searchcount") == ["searchcount 0"] and not r.note("searchhit"), "a new search after a choice starts empty (no recent result): " + str(r.note("searchhit")))
    r = R.go(["key=ctrl+k", "text=s", "key=enter", "key=ctrl+k", "text=s"])
    hit = r.note("searchhit")
    check(hit and hit[0] != "", "a query still works after a recent choice: " + str(hit))
    # Orbit Lab: no Old panel in search
    r = R.go(["key=ctrl+k", "text=old panel"])
    check(r.note("searchcount") == ["searchcount 0"], "search offers no Old panel in the Orbit Lab: " + str(r.note("searchcount")))

# ------------------------------------------------------------------ links
def link_gaps(r):
    pieces = {}
    for ln in r.out.splitlines():
        m = re.match(r"PLLINK id=(\S+) y=(-?[\d.]+) x0=([\d.]+) x1=([\d.]+)", ln)
        if m:
            pieces.setdefault((m.group(1), m.group(2)), []).append((float(m.group(3)), float(m.group(4))))
    gaps = 0
    for key, segs in pieces.items():
        segs.sort()
        for a, b in zip(segs, segs[1:]):
            if 0.5 < b[0] - a[1] < 20:
                gaps += 1
    return pieces, gaps

def part_links(binary, work):
    r = run_app(binary, work, "links", {"PL_NEWUI": "on", "PL_OPEN": "about"}, frames=40, size="1280x800")
    first, gaps = link_gaps(r)
    print(f"links: {sum(len(v) for v in first.values())} underline pieces on the first screen of About")
    check(gaps == 0, f"no gap inside one link on one line ({gaps} gaps)")
    # the same for the lines further down: the sources part of About (0.39.3: this run used PL_SCROLL, which the app does not read, and checked nothing; now the wheel)
    wheel = ";".join(f"{12 + 2 * i}:wheel=-10" for i in range(12))
    r = run_app(binary, work, "links2", {"PL_NEWUI": "on", "PL_OPEN": "about", "PL_SCRIPT": "8:move=640,400;" + wheel}, frames=60, size="1280x800")
    lower, gaps = link_gaps(r)
    top0 = min(float(k[1]) for k in first)
    top1 = min((float(k[1]) for k in lower), default=top0)
    check(top1 < top0 - 1000, f"the wheel moved About down (the first link line from y={top0:.0f} to y={top1:.0f})")
    check(gaps == 0, f"further down About: no gap inside one link on one line ({gaps} gaps)")

# ------------------------------------------------------------------ the Journey's caption (0.39.3)
def part_journey(binary, work):
    # it was placed at a fixed height in the middle of the whole window: it ran under the top bar, the Legend chip and the Controls panel
    for size, env in (("880x560", {"PL_TEXTSIZE": "xl"}), ("1280x800", {})):
        r = run_app(binary, work, f"journey_{size}", {"PL_NEWUI": "on", "PL_JOURNEY": "1", **env}, frames=60, size=size)
        cap = r.rects.get("journeycap")
        check(cap is not None, f"{size}: the Journey's caption is drawn")
        if not cap:
            continue
        W, H = (int(v) for v in size.split("x"))
        check(cap[0] >= 0 and cap[0] + cap[2] <= W, f"{size}: the caption is inside the window ({cap})")
        for name, box in r.rects.items():
            if name in ("topbar", "panel.controls", "toolbar", "readout") or name.startswith("card."):
                check(not overlap(cap, box), f"{size}: the caption is clear of {name} ({cap} / {box})")

# ------------------------------------------------------------------ the notes at the bottom left, zoomed far out (0.39.3)
def part_farnote(binary, work):
    # they were wrapped by a guessed width with a fixed line height: with Extra large text the lines ran into each other, under the readout and the panel
    # (and with Sagittarius A* picked, XL at 880x560: the masses line moved up under the subject card; now it is left out when there is no room)
    for size, env, must in (("880x560", {"PL_TEXTSIZE": "xl", "PL_ZOOM": "-6"}, True), ("1280x800", {"PL_ZOOM": "-6"}, True), ("880x560", {"PL_TEXTSIZE": "xl", "PL_SELECT": "far:sgra"}, False)):
        tag = size + (" sgra" if not must else "")
        r = run_app(binary, work, f"farnote_{tag.replace(' ', '_')}", {"PL_NEWUI": "on", **env}, frames=30, size=size)
        note = r.rects.get("farnote")
        if tag == "880x560":   # (0.39.3, the same run: the panel's keys line wrapped as "... zoom  ·  R" / "reset view": a key letter never ends a line)
            lone = [t for _, _, t in r.texts if any(k in t for k in ("Keys:", "zoom", "reset view", "switch lab")) and re.search(r" [A-Z0-9+]$", t)]
            check(any("reset view" in t for _, _, t in r.texts) and not lone, f"{tag}: no line ends in a lone key letter {lone[:1]}")
        if must:
            check(note is not None, f"{tag}: the far-out notes are drawn")
        if not note:
            continue
        size = tag
        for name, box in r.rects.items():
            if name in ("topbar", "panel.controls", "toolbar", "readout", "handle", "axes") or name.startswith("card."):
                check(not overlap(note, box), f"{size}: the far-out notes are clear of {name} ({note} / {box})")

# ------------------------------------------------------------------ 0.50: lighting and the edge of air (pictures; dark theme, Stars Off so the sky is one colour)
def body_shot(binary, work, name, sel, zoom, extra=None):
    env = {"PL_NEWUI": "on", "PL_SELECT": sel, "PL_ZOOM": zoom, "PL_LOG_BODYPX": "1", "PL_SKYOFF": "1"}
    env.update(extra or {})
    r = run_app(binary, work, name, env, frames=40)
    ln = [l.split() for l in r.out.splitlines() if l.startswith("PLBODYPX " + sel + " ")]
    if not ln:
        return None, None
    x, y, rad, lx, ly, lz = map(float, ln[-1][2:8])
    return Bmp(r.bmp), (x, y, rad, lx, ly, lz)

def near(a, b, tol=4):
    return all(abs(p - q) <= tol for p, q in zip(a, b))

def part_airedge(binary, work):
    # review item 2: the blue ring from 0.96 R to 1.16 R round Earth (about 1,100-1,300 km on screen, on the night side too) had no source; the air's scale height is 8.5 km
    # (data/nssdc/earthfact.html), far under a pixel here. Earth about 147 px in radius: just outside the limb (1.07-1.12 R, outside the selection ring) the picture is the sky.
    import math
    bmp, at = body_shot(binary, work, "airedge", "Earth", "4.5")
    check(at is not None and at[2] > 100, f"Earth drawn big enough to look at its edge ({at})")
    if not at:
        return
    x, y, rad = at[:3]
    good = n = 0
    for k in range(72):
        t = 2 * math.pi * k / 72
        p15 = (int(round(x + 1.5 * rad * math.cos(t))), int(round(y + 1.5 * rad * math.sin(t))))
        if not (0 <= p15[0] < bmp.w and 0 <= p15[1] < bmp.h):
            continue
        sky = bmp.px(*p15)
        for f in (1.07, 1.09, 1.12):
            n += 1
            good += near(bmp.px(int(round(x + f * rad * math.cos(t))), int(round(y + f * rad * math.sin(t)))), sky)
    check(n >= 100 and good >= 0.9 * n, f"just outside Earth's limb the picture is the sky's colour: {good} of {n} samples (orbit lines and the label may cross a few)")

def part_nightside(binary, work):
    # review item 3: the lit ball had a 12 % ambient floor, so Jupiter's night side showed its bands. Space gives no light from all round: a pixel whose surface faces
    # away from the Sun (cos i < -0.15, a margin for the planet's flattening and the smooth edge) inside 0.95 R is the sky's colour (the sky taken at 1.5 R on the night side).
    import math
    bmp, at = body_shot(binary, work, "nightside", "Jupiter", "3.4")
    check(at is not None and at[2] > 80, f"Jupiter drawn big enough ({at})")
    if not at:
        return
    x, y, rad, lx, ly, lz = at
    ln = math.sqrt(lx * lx + ly * ly + lz * lz) or 1
    lx, ly, lz = lx / ln, ly / ln, lz / ln
    skyp = (int(round(x - 1.5 * rad * lx / max(math.hypot(lx, ly), 1e-9))), int(round(y - 1.5 * rad * ly / max(math.hypot(lx, ly), 1e-9))))
    check(0 <= skyp[0] < bmp.w and 0 <= skyp[1] < bmp.h, f"sky sample on screen {skyp}")
    if not (0 <= skyp[0] < bmp.w and 0 <= skyp[1] < bmp.h):
        return
    sky = bmp.px(*skyp)
    good = n = 0
    step = max(1, int(rad / 25))
    for py in range(int(y - rad), int(y + rad) + 1, step):
        for px_ in range(int(x - rad), int(x + rad) + 1, step):
            u, v = (px_ - x) / rad, (py - y) / rad
            d2 = u * u + v * v
            if d2 > 0.95 ** 2 or not (0 <= px_ < bmp.w and 0 <= py < bmp.h):
                continue
            if u * lx + v * ly + math.sqrt(1 - d2) * lz < -0.15:
                n += 1
                good += near(bmp.px(px_, py), sky)
    check(n >= 50 and good >= 0.95 * n, f"Jupiter's night side is the sky's colour: {good} of {n} pixels (sky {sky}; the axis line and the label may cross a few)")

def part_rings(binary, work):
    # 0.64.0: Saturn's rings per pixel on the card (Vulkan): the start-up check passes here (three views, each at most 1 % of the ring pixels over 8 levels
    # from the processor's triangles), the benchmark's Path names the card; made to fail on purpose (PL_RINGS_CARDFAIL), the processor's triangles are kept
    # and the Path says why. Then the two pictures (card, processor) of the same moment differ in under 2 % of the pixels by more than 8 levels.
    import re
    BENCH = {"PL_GRAPHICS": "vulkan", "PL_BENCH": "start", "PL_BENCH_QUIT": "1", "PL_FIXED_DT": "0.016", "PL_BENCH_FINISH": "1", "PL_BENCH_ONLY": "orbit.saturn",
             "PL_BENCH_WARMUP": "0.3", "PL_BENCH_MEASURE": "2.0", "PL_BENCH_SHOT": "1.5"}
    runs = {}
    for tag, extra in (("card", {}), ("cpu", {"PL_RINGS_CARDFAIL": "1"})):
        d = os.path.join(work, "rings_" + tag); os.makedirs(d, exist_ok=True)
        for f in os.listdir(d):
            if f.endswith(".bmp"): os.remove(os.path.join(d, f))
        runs[tag] = run_app(binary, d, "rings_" + tag, dict(BENCH, **extra), frames=100000, size="1280x800", timeout=900)
    out = runs["card"].out
    cases = [ln for ln in out.splitlines() if ln.startswith("PLRINGCARDCASE ")]
    pct = [float(m.group(1)) for m in (re.search(r"\(([\d.]+) %, limit 1 %\)", ln) for ln in cases) if m]
    check("PLRINGCARD state=1" in out and len(pct) == 3 and all(p <= 1.0 for p in pct), f"rings: the start-up check passes, three views within 1 % ({cases})")
    rep = out[out.find("PLBENCH-RESULT-BEGIN"):out.find("PLBENCH-RESULT-END")]
    path = [ln.strip() for ln in rep.splitlines() if ln.strip().startswith("Path:")]
    check(any("Saturn's rings: the graphics card (Vulkan; its start-up check passed" in p for p in path), f"rings: the benchmark's Path names the card ({path})")
    out2 = runs["cpu"].out
    rep2 = out2[out2.find("PLBENCH-RESULT-BEGIN"):out2.find("PLBENCH-RESULT-END")]
    path2 = [ln.strip() for ln in rep2.splitlines() if ln.strip().startswith("Path:")]
    check("PLRINGCARD state=-1" in out2 and any("Saturn's rings: the processor, because the check was made to fail" in p for p in path2), f"rings: a failed check keeps the processor's triangles and says why ({path2})")
    try:
        a, b = Bmp(os.path.join(work, "rings_card", "orbit.saturn.bmp")), Bmp(os.path.join(work, "rings_cpu", "orbit.saturn.bmp"))
        n = over = 0
        for y in range(0, a.h, 2):
            for x in range(0, a.w, 2):
                n += 1
                over += max(abs(p - q) for p, q in zip(a.px(x, y)[:3], b.px(x, y)[:3])) > 8
        check(n > 0 and over <= 0.02 * n, f"rings: the card's picture and the processor's of the same moment: {over} of {n} pixels over 8 levels")
    except Exception as e:
        check(False, f"rings: both pictures saved ({e})")

def part_cardlines(binary, work):
    # 0.68.0: the Orbit Lab's trails, orbits and distance rings on the card (Vulkan): its start-up check passes here (three cases against the processor's
    # LinePen, orbit/lines_check.h), and the pictures of three views (the benchmark's orbit.heaviest, orbit.real_fast, and orbit.earth's close-up), card
    # against processor (PL_NO_LINES_CARD), multisampling off, differ in at most 1 % of the pixels by more than 8 levels (the Vulkan picture limit). Made to
    # fail on purpose (PL_LINES_CARDFAIL) the processor's lines are drawn: that picture is the processor's.
    def over(x, y):
        px, py = x.pixels(), y.pixels()
        st = x.bpp
        return sum(1 for i in range(0, min(len(px), len(py)) - st + 1, st) if max(abs(px[i] - py[i]), abs(px[i + 1] - py[i + 1]), abs(px[i + 2] - py[i + 2])) > 8)
    for sc in ("orbit.heaviest", "orbit.real_fast", "orbit.earth"):
        env = {"PL_GRAPHICS": "vulkan", "PL_NO_MSAA": "1", "PL_BENCH_SETUP": sc}
        card = run_app(binary, work, "lines_card_" + sc, env, frames=60)
        cpu = run_app(binary, work, "lines_cpu_" + sc, dict(env, PL_NO_LINES_CARD="1"), frames=60)
        if sc == "orbit.heaviest":
            cases = [ln for ln in card.out.splitlines() if ln.startswith("PLLINESCASE ")]
            check("PLLINESCARD state=1" in card.out and len(cases) == 3, f"lines: the start-up check passes, three cases ({cases})")
            bad = run_app(binary, work, "lines_fail", dict(env, PL_LINES_CARDFAIL="1"), frames=60)
            check("PLLINESCARD state=-1" in bad.out, "lines: a failed check leaves the processor's lines")
            o2 = over(Bmp(bad.bmp), Bmp(cpu.bmp))   # (two runs of the processor's own picture already differ in a pixel or two here: at most 0.01 %)
            check(o2 <= 100, f"lines: the failed check's picture is the processor's ({o2} pixels over 8 levels)")
        a, b = Bmp(card.bmp), Bmp(cpu.bmp)
        o = over(a, b)
        print(f"  {sc}: card against processor, {o} pixels over 8 levels ({100 * o / max(1, a.w * a.h):.3f} %)")
        check((a.w, a.h) == (b.w, b.h) and a.w > 0 and o <= 0.01 * a.w * a.h, f"lines, {sc}: the card's picture is the processor's within the limit ({o} pixels over 8 levels)")

def main():
    binary, work = sys.argv[1], sys.argv[2]
    only = set(sys.argv[3].split(",")) if len(sys.argv) > 3 else None
    want = lambda n: only is None or n in only
    os.makedirs(work, exist_ok=True)
    for name, fn in (("coverage", part_coverage), ("card", part_card), ("time", part_time), ("chip", part_chip), ("facts", part_facts), ("facts_long", part_facts_long), ("readable", part_readable), ("search", part_search), ("links", part_links), ("journey", part_journey), ("farnote", part_farnote), ("airedge", part_airedge), ("nightside", part_nightside), ("rings", part_rings), ("cardlines", part_cardlines)):
        if want(name):
            fn(binary, work)
    print(f"{checks} checks, {fails} failed")
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
