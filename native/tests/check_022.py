#!/usr/bin/env python3
# 0.22: the owner's look-through (13 findings) checked in the REAL app (Linux, software Vulkan; NOT Windows, NOT a real mouse). One part per fix that the unit tests cannot see:
#   molbar   the Molecule Lab's bottom bar has all three styles of the panel (Realistic too, in the Viewer only) and Reset view; Labels and Spin are in the panel only
#   centre   the Orbit Lab's scene (the Sun, nothing selected) is centred in the part of the window the Controls panel leaves free, and in the window when the panel is closed
#   axes     no ring label ("10 AU") is drawn on the x y z axes at the bottom right
#   start    the Orbit Lab starts with Neptune's orbit fitted to the free part (0.24; was zoom x0.16) and 1 s = 7 days (tspeed -0.92), where the inner planets are apart and a month does not fly by
#   words    the Atom Lab's view buttons carry their name next to the icon (wider than an icon-only button)
#   keys     1 2 3 open the labs, + and - zoom, R resets the view
#   facts    the Orbit and Atom Labs show only their own facts and the general ones (stepping through all of them in the Facts panel; the Molecule Lab shows the general ones, 0.22 stage 7)
#   density  0.22 stage 2: the Atom Lab draws the whole nucleus as its charge density by default (Nucleus look: Density), a smooth glow brightest in the middle; diving into a
#            nucleon shows the nucleons again
#   honest   0.22 stage 1: the new names and captions are on screen (Density glow, the Molecule Lab's Density style, "not an electron", the molecule "never stops")
#   followmoon  0.73.1: a clicked moon is FOLLOWED (the camera stays on the moon where it is drawn, not on its planet): Earth's Moon (the ELP place) and Io (the fitted place), 2D and 3D, with time
#            running fast; and "Slow time to watch moons" with a moon selected selects and follows the planet (the point of it is to see the moons go round)
#   camera   0.74.0: the Orbit Lab's camera never loses what it follows (an empty click, a right drag), a pick glides, a released turn coasts and stops, the wheel eases
#            and ends exactly; Reduce motion: no glides (it jumps), still never lost
#   depth    0.75.0: the 3D camera has real depth (a pinhole with a 45-degree field, orbit/camera.h): the Moon between the camera and Earth is drawn bigger, its radius
#            focal tan(asin(R / L)); a star and a far planet move together when the view turns (the sky through the same lens); the two gaps left by 0.74.0:
#            a click on a body while not following flies to it, ticking Follow glides (Reduce motion: both jump)
#   check_022.py <PhysicsLab binary> <work folder> <part>
import math, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, overlap, Bmp

fails = checks = 0
def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def state(r, key):
    st = [s for s in r.states]
    if not st:
        return None
    m = re.search(r"\b" + re.escape(key) + r"=(-?[\d.e+-]+)", st[-1])
    return float(m.group(1)) if m else None

def molbar(binary, work):
    r = run_app(binary, work, "molbar_view", {"PL_MOLECULE": "H2O", "PL_SCRIPT": "10:click=molecule.bar.real"}, frames=24)
    check(not r.scripted_missing(), f"viewer: the bar has a Realistic button ({r.scripted_missing()})")
    check(any("molecule H2O style=real" in n for n in r.notes), "viewer: a click on it shows the Realistic style")
    for gone in ("labels", "spin"):
        g = run_app(binary, work, "molbar_" + gone, {"PL_MOLECULE": "H2O", "PL_SCRIPT": f"10:click=molecule.bar.{gone}"}, frames=16)
        check(bool(g.scripted_missing()), f"the bar has no {gone} button any more (it is in the panel)")

def centre(binary, work):
    for sz in ("1280x800", "880x560"):
        r = run_app(binary, work, f"centre_{sz}", {}, frames=60, size=sz)
        sun, dock = r.rects.get("sun"), r.rects.get("dock.controls")
        check(sun is not None and dock is not None, f"{sz}: the Sun and the panel are drawn")
        if sun and dock:
            free = dock[0]                                   # the free part: left of the panel (the dock's x)
            check(abs(sun[0] + 1 - free / 2) < 3, f"{sz}: the Sun is in the middle of the free part ({sun[0] + 1:.1f}, want {free / 2:.1f})")
        c = run_app(binary, work, f"centre_closed_{sz}", {"PL_SCRIPT": "10:click=ctl.handle"}, frames=90, size=sz)
        sun = c.rects.get("sun")
        w = c.window[0] if c.window else 0
        check(sun is not None and "dock.controls" not in c.rects and abs(sun[0] + 1 - w / 2) < 3, f"{sz}: panel closed, the Sun is in the middle of the window ({sun}, want {w / 2})")

def axes(binary, work):
    for sz, zoom in (("1280x800", None), ("1280x800", "-0.4"), ("880x560", None), ("1000x640", "-1.2")):
        env = {"PL_ZOOM": zoom} if zoom else {}
        r = run_app(binary, work, f"axes_{sz}_{zoom}", env, frames=40, size=sz)
        ax = r.rects.get("axes")
        check(ax is not None, f"{sz} zoom {zoom}: the axes are drawn")
        if ax:
            hit = [k for k, v in r.rects.items() if k.startswith("ringlabel.") and overlap(v, ax)]
            check(not hit, f"{sz} zoom {zoom}: no ring label on the axes ({hit})")

def start(binary, work):
    r = run_app(binary, work, "start", {"PL_SCRIPT": "5:state"}, frames=10)
    # 0.24 (the owner, 4 Nov 2026: "maybe the full solar system is at the view"): every start fits Neptune's whole orbit into the free part of the window (was a fixed
    # zoom of -0.8, x0.16, which left Neptune off the picture). Reference: Neptune's aphelion a (1 + e) from the JPL table (a0 30.06992276 AU, e0 0.00859048, typed
    # again here; the rates move it by < 0.001 % by 2026), IAU 1 AU; the picture's scale as orbit_view.cpp draws it (px = min(W, H) / 34 * 10^zoom / kSimLength)
    import math
    AU = 149597870700.0
    sim = AU / 100 ** (1 / 3)
    z = state(r, "zoom")
    W, H = 1280, 800
    rpx = min(W, H) / 34 * 10 ** z / sim * 30.06992276 * (1 + 0.00859048) * AU if z is not None else 0
    pc, tb, top = r.rects.get("panel.controls"), r.rects.get("toolbar"), r.rects.get("topbar")
    free = min(pc[0], tb[1] - top[3]) if pc and tb and top else 0
    check(free > 0 and 0.42 * free < rpx < 0.5 * free, f"the Orbit Lab starts with Neptune's whole orbit filling the free part (radius {rpx:.0f} px, free {free:.0f} px, zoom {z})")
    check(state(r, "tspeed") == -0.92, f"and at time speed -0.92 ({state(r, 'tspeed')})")
    check(any(t[2] == "1 s = 7.0 days" for t in r.texts), "the Time speed shows 1 s = 7.0 days")

def words(binary, work):
    r = run_app(binary, work, "words", {"PL_ATOM": "1"}, frames=20)
    reset = r.rects.get("toolbtn.tool.reset")
    check(reset is not None, "the toolbar's Reset view button is there (an icon alone)")
    for b in ("atom", "cloud", "cloud3d", "slice", "chart"):
        v = r.rects.get("toolbtn.tool." + b)
        check(v is not None and reset is not None and v[2] > reset[2] + 20, f"{b}: the button is wider than an icon alone (it carries its name) ({v}, icon {reset})")

def keys(binary, work):
    r = run_app(binary, work, "keys_2", {"PL_SCRIPT": "10:key=2"}, frames=20)
    check("toolbtn.tool.chart" in r.rects, "key 2 opens the Atom Lab")
    r = run_app(binary, work, "keys_3", {"PL_SCRIPT": "10:key=3"}, frames=20)
    check("moleculebar" in r.rects, "key 3 opens the Molecule Lab")
    r = run_app(binary, work, "keys_31", {"PL_SCRIPT": "10:key=3;14:key=1;18:state"}, frames=24)
    check("moleculebar" not in r.rects and r.rects.get("toolbtn.tool.reset") is not None and "toolbtn.tool.chart" not in r.rects, "key 1 opens the Orbit Lab again")
    r = run_app(binary, work, "keys_zoom", {"PL_SCRIPT": "8:state;10:key=plus;14:state;16:key=minus;18:key=minus;22:state"}, frames=26)
    z = [float(re.search(r"zoom=(-?[\d.e+-]+)", s).group(1)) for s in r.states]
    check(len(z) == 3 and z[1] > z[0] + 0.1 and z[2] < z[1] - 0.2, f"Orbit Lab: + zooms in, - zooms out ({z})")
    r = run_app(binary, work, "keys_reset", {"PL_ZOOM": "-2", "PL_SCRIPT": "10:key=r;14:state"}, frames=18)
    check(state(r, "zoom") == 0, f"Orbit Lab: R resets the view (zoom 0, as the Reset view button) ({state(r, 'zoom')})")
    r = run_app(binary, work, "keys_text", {"PL_SCRIPT": "10:key=ctrl+k;14:text=2;16:key=2;20:key=escape"}, frames=24)
    check("toolbtn.tool.chart" not in r.rects, "typing 2 in search does not switch the lab")

ORBIT_ONLY = ("Sunlight takes", "Venus takes 243", "The Moon drifts", "Sound cannot travel", "One light year", "The space station", "To leave Earth")
ATOM_ONLY = ("Carbon-14", "Iron-56", "Bismuth-209", "Uranium-238", "A proton weighs", "The nucleus is tens", "Water is densest", "Diamond and graphite")

def facts(binary, work):
    for lab, env, banned, wanted in (("orbit", {}, ATOM_ONLY, ORBIT_ONLY), ("atom", {"PL_ATOM": "1"}, ORBIT_ONLY, ATOM_ONLY)):
        script = "6:click=fact.line;" + ";".join(f"{10 + 3 * i}:click=facts.next" for i in range(30))
        r = run_app(binary, work, "facts_" + lab, dict(env, PL_SCRIPT=script), frames=10 + 3 * 30 + 4)
        shown = {t[2] for t in r.texts} | {n[9:] for n in r.notes if n.startswith("factline ")}   # (the Facts panel's text, every frame, and the fact line's)
        shown = {t for t in shown if t.startswith(ORBIT_ONLY + ATOM_ONLY + ("Lightning", "Absolute zero", "The speed of light"))}
        check(not r.scripted_missing() and len(shown) >= 3, f"{lab}: several facts were shown ({len(shown)}; {r.scripted_missing()[:2]})")
        bad = [t[:40] for t in shown if t.startswith(banned)]
        check(not bad, f"{lab}: no fact of another lab ({bad})")
        check(all(any(t.startswith(w) for t in shown) for w in wanted), f"{lab}: every fact of this lab comes round ({sorted(t[:20] for t in shown)})")

def factsrc(binary, work):
    """0.40: the Facts panel names the fact's own source, and Show in About opens About scrolled to that entry, highlighted"""
    r = run_app(binary, work, "factsrc", {"PL_SCRIPT": "6:click=fact.line;12:click=facts.source"}, frames=30)
    check(not r.scripted_missing(), f"the fact line and the Show in About button are there to click ({r.scripted_missing()[:2]})")
    src = [t[2] for t in r.texts if t[2].startswith("Source: ")]
    check(src and src[0] == "Source: NOAA National Weather Service: How hot is lightning?", f"the lightning fact names its source ({src[:1]})")
    focus = [ln for ln in r.out.splitlines() if ln.startswith("PLFOCUS ")]
    check(bool(focus), "About opened with the fact's source entry found and highlighted")
    if focus:
        top = float(focus[-1].split()[1].split("=")[1])
        check(0 < top < 800, f"About is scrolled so that the source entry is on screen (top {top:.0f} of 800)")

def spectrum(binary, work):
    """0.40: the Photons section's spectrum strip. Hydrogen: a click on the Balmer-alpha line makes the electron jump 3 -> 2 and the line is marked as made
    (the Rydberg formula with CODATA 2022 R = 10 973 731.568 157 /m and m_p/m_e = 1836.152 673 426 gives 656.4696 nm in vacuum). Iron with Electrons All: the
    neutral atom's measured lines (NIST ASD), none clickable. 0.46: sodium with Electrons All: the outer electron's lines, a click on D2 emits it."""
    r = run_app(binary, work, "spec_h", {"PL_ATOM": "1", "PL_SCRIPT": "10:click=atom.line.3-2"}, frames=60)
    n = [x for x in r.notes if x.startswith("specstrip ")]
    check(not r.scripted_missing() and bool(n), f"hydrogen: the strip and its Balmer-alpha line are there ({r.scripted_missing()[:1]})")
    seen = [float(v) for v in n[-1].split("seen=")[1].split(",") if v] if n else []
    check(any(abs(v - 656.470) < 0.01 for v in seen), f"hydrogen: the click made the 3 -> 2 jump, Balmer-alpha 656.470 nm ({seen})")
    r = run_app(binary, work, "spec_fe", {"PL_ATOM": "1", "PL_ATOM_SET": "Z=26;N=30;elec=All"}, frames=30)
    n = [x for x in r.notes if x.startswith("specstrip ")]
    check(bool(n) and "mode=atom" in n[-1] and not any(k.startswith("atom.line.") for k in r.rects), f"iron, every electron: the measured lines of Fe I, none to click ({n[-1:] if n else None})")
    # 0.46: sodium, every electron: the outer electron's measured lines (NIST ASD, data/asd/alkali/NaI_lines.tsv: D2 3s 1/2 - 3p 3/2 at 589.1583264 nm, vacuum);
    # a click on D2 puts the outer electron in 3p and it emits exactly that line
    r = run_app(binary, work, "spec_na", {"PL_ATOM": "1", "PL_ATOM_SET": "Z=11;N=12;elec=All", "PL_SCRIPT": "10:click=atom.vline.589.158"}, frames=60)
    n = [x for x in r.notes if x.startswith("specstrip ")]
    seen = [float(v) for v in n[-1].split("seen=")[1].split(",") if v] if n else []
    check(bool(n) and "mode=valence" in n[-1] and not r.scripted_missing(), f"sodium, every electron: the outer electron's lines, clickable ({n[-1:] if n else None}, {r.scripted_missing()[:1]})")
    check(any(abs(v - 589.158) < 0.0005 for v in seen), f"sodium: the click on D2 emitted 589.158 nm ({seen})")

def astate(r):
    v = [ln[9:] for ln in r.out.splitlines() if ln.startswith("PLASTATE ")]
    return dict(t.split("=", 1) for t in v[-1].split("\t") if "=" in t) if v else {}

def density(binary, work):
    from shell_lib import Bmp
    r = run_app(binary, work, "density_u238", {"PL_ATOM": "1", "PL_ATOM_SET": "Z=92;N=146", "PL_ATOM_DO": "nucleus", "PL_SCRIPT": "30:state;31:astate"}, frames=36)
    dn = [ln for ln in r.out.splitlines() if ln.startswith("PLATOMDENSITY ")]
    st = (r.states[-1] if r.states else "") + (" density=" + dn[-1].split()[1] if dn else "")
    check("density=1" in st and "layer=Nucleus" in st, f"U-238, the nucleus in view: drawn as its charge density by default ({st[:160]})")
    b = Bmp(r.bmp)
    if "density=1" in st:
        box = [ln for ln in r.out.splitlines() if ln.startswith("PLATOMBOX ")]
        m = re.search(r"centre=(-?[\d.]+),(-?[\d.]+)", box[-1]) if box else None
        x0, y0 = (int(float(m.group(1))), int(float(m.group(2)))) if m else (b.w // 2, b.h // 2)
        mid = b.px(x0, y0)
        check(mid[0] > mid[1] + 40 and mid[0] > 120, f"the middle of the nucleus glows in the protons' colour ({mid} at {x0},{y0})")
    f = run_app(binary, work, "density_focus", {"PL_ATOM": "1", "PL_ATOM_SET": "Z=92;N=146", "PL_ATOM_DO": "nucleon", "PL_SCRIPT": "60:state"}, frames=66)
    dn = [ln for ln in f.out.splitlines() if ln.startswith("PLATOMDENSITY ")]
    st = (f.states[-1] if f.states else "") + (" density=" + dn[-1].split()[1] if dn else "")
    check("density=0" in st and bool(dn), f"dived into a nucleon: the nucleons are drawn, not the density ({st[:160]})")
    o = run_app(binary, work, "density_nucleons", {"PL_ATOM": "1", "PL_ATOM_SET": "Z=92;N=146;style=Nucleons", "PL_ATOM_DO": "nucleus", "PL_SCRIPT": "30:state"}, frames=36)
    check("PLATOMDENSITY 0" in o.out, "Nucleus look Nucleons: the nucleons, as before")

def honest(binary, work):
    a = run_app(binary, work, "honest_atom", {"PL_ATOM": "1"}, frames=20)
    words = " ".join(t[2] for t in a.texts)
    check("Density glow" in words, "Atom Lab: the cloud style is called Density glow")
    check("not an electron" in words, "Atom Lab: the cloud style's note says a dot is not an electron")
    check("Nucleus look" in words, "Atom Lab: the nucleus row is called Nucleus look")
    labels = [x[2] for x in a.texts]
    o = run_app(binary, work, "honest_pz", {"PL_ATOM": "1", "PL_ATOM_SET": "orb=2p"}, frames=20)
    check("pz" in [x[2] for x in o.texts] and "pz · along z" not in [x[2] for x in o.texts], "Atom Lab, 2p: the orientation button says pz (its note \"along z (up)\" is below it)")
    check(all(w in labels for w in ("Density", "Nucleons")) and not any(w in labels for w in ("Fuzzy", "Hard balls")), f"Atom Lab: the two Nucleus look buttons say just Density, Nucleons (0.43; their notes are below, not on the button) ({[x for x in labels if 'snapshot' in x or 'balls' in x][:3]})")
    m = run_app(binary, work, "honest_mol", {"PL_MOLECULE": "H2O"}, frames=20)
    words = " ".join(t[2] for t in m.texts)
    check("Density" in [t[2] for t in m.texts] and "Realistic" not in [t[2] for t in m.texts], "Molecule Lab: the style is called Density (not Realistic)")
    check("never stops" in words, "Molecule Lab: the panel says a real molecule never stops moving")

def stage6(binary, work):
    # 0.22 stage 6: the owner's second look-through, in the real app
    # 1. Reduced motion (these checks run with it): the Atom Lab starts paused, and its cloud is still drawn (it was empty until Play)
    r = run_app(binary, work, "s6_cloud", {"PL_ATOM": "1", "PL_PANELS": "", "PL_SCRIPT": "30:astate"}, frames=40)
    m = re.search(r"PLATOMBOX .*?\bcentre=([-\d.e+]+),([-\d.e+]+)", r.out)
    check(m is not None, "the Atom picture's centre is logged")
    if m:
        b = Bmp(r.bmp); cx, cy = int(float(m.group(1))), int(float(m.group(2)))
        lum = lambda p: 0.2126 * p[0] + 0.7152 * p[1] + 0.0722 * p[2]
        mid = sum(lum(b.px(cx + dx, cy + dy)) for dx in range(-30, 31, 3) for dy in range(-30, 31, 3)) / 441
        bg = sum(lum(b.px(20 + dx, 400 + dy)) for dx in range(0, 30, 3) for dy in range(0, 30, 3)) / 100
        check(mid > bg + 25, f"paused at the start (Reduced motion), the cloud is drawn: the middle is brighter than the background ({mid:.0f} vs {bg:.0f})")
    # 2. the element card folds itself on a narrow window, stays open on a wide one (no stored choice)
    n = run_app(binary, work, "s6_narrow", {"PL_ATOM": "1", "PL_PANELS": ""}, frames=20, size="880x560")
    w = run_app(binary, work, "s6_wide", {"PL_ATOM": "1", "PL_PANELS": ""}, frames=20, size="1280x800")
    check("card.atom.card.collapsed" in n.rects, f"880x560: the element card starts folded ({sorted(k for k in n.rects if k.startswith('card.'))})")
    check("card.atom.card" in w.rects, f"1280x800: the element card starts open ({sorted(k for k in w.rects if k.startswith('card.'))})")
    # 3. no fps counter, no two-lab swap button, the Orbit readout without the zoom
    o = run_app(binary, work, "s6_orbit", {}, frames=20)
    texts = [t[2] for t in o.texts] + [t[2] for t in w.texts]
    check(not any(t.endswith(" fps") for t in texts), "no fps counter on the picture (it is off by default)")
    check("topicon.swap" not in o.rects and "topicon.search" in o.rects, "the top bar has no swap button")
    check(not any(t.startswith("Zoom ×") for t in [x[2] for x in o.texts]), "the Orbit readout no longer repeats the zoom")

def moonoff(line):
    m = re.search(r"moonoff=(-?[\d.]+),(-?[\d.]+) planetoff=(-?[\d.]+),(-?[\d.]+)", line)
    return tuple(float(v) for v in m.groups()) if m else None

def followmoon(binary, work):
    # 1280x800, dark: the result does not depend on the size or the theme. The moon is picked from Pick a body; moonoff / planetoff in the state line are where the moon and its planet were DRAWN last frame,
    # in points from the middle the followed thing is held at (orbit_view.h drawnSel; 9999 = no moon drawn). Followed moon: moonoff stays at 0; its planet circles it (planetoff moves).
    # (0.73.0's build: the planet was followed, moonoff went round the planet's circle: tests (a)-(c) fail there.)
    cases = (("Moon", "", "Earth's Moon (the ELP place), 2D"), ("Moon", "1", "Earth's Moon, 3D"), ("Io", "1", "Io (the fitted place), 3D"), ("Io", "", "Io, 2D"))
    for name, d3, what in cases:
        parts = ["4:key=space", "14:click=ctl.sec.pick", "20:click=##pickfilter", f"26:text={name}", f"32:click=pick.{name}"] + [f"{f}:state" for f in range(44, 104, 3)]   # (space: the checks start paused; the lab then runs at its start speed, 7 days a second: Io goes round in 10 frames, the Moon in about 4 s)
        env = {"PL_NEWUI": "on", "PL_SCRIPT": ";".join(parts)}
        if d3:
            env["PL_3D"] = "1"
        r = run_app(binary, work, f"followmoon_{name}_{d3 or '2d'}", env, frames=112)
        offs = [moonoff(x) for x in r.states[3:]]           # (the first states are while the flight settles)
        check(len(r.states) >= 15 and all(offs), f"{what}: the state lines carry moonoff ({len(r.states)} lines)")
        if not offs or not all(offs):
            continue
        tsp = state(r, "tspeed")
        worst = max(max(abs(o[0]), abs(o[1])) for o in offs)
        planet = max(max(abs(o[2]), abs(o[3])) for o in offs)
        moved = max(abs(o[2] - offs[0][2]) + abs(o[3] - offs[0][3]) for o in offs)
        check(worst <= 2.0, f"{what}: the moon stays within 2 points of the middle ({worst:.2f}, tspeed {tsp})")
        check(planet > 10 and moved > 10, f"{what}: its planet moves away from the middle and round it (furthest {planet:.1f}, moved {moved:.1f} points)")
    # (d) "Slow time to watch moons" with a moon selected: the planet is selected and followed
    r = run_app(binary, work, "followmoon_slow", {"PL_NEWUI": "on", "PL_SCRIPT": "4:key=space;14:click=ctl.sec.pick;20:click=##pickfilter;26:text=Io;32:click=pick.Io;48:state;54:click=card.slowmoons;74:state"}, frames=84)
    check(len(r.states) == 2, f"slow moons: two state lines ({len(r.states)})")
    if len(r.states) == 2:
        a, b = (re.search(r"sel=(\d+),(\d+),(\d+) follow=(\d)", x) for x in r.states)
        check(a and b and a.group(1) == "4" and b.group(1) == "3" and a.group(2) == b.group(2) and b.group(4) == "1",
              f"slow moons with Io selected: the moon (kind 4) becomes its planet (Body, kind 3), followed ({a and a.group(0)} -> {b and b.group(0)})")

def cam(line):
    # the state line's camera fields (0.74.0): sel kind and id, follow, zoom, yaw, pitch, where the selected thing was drawn (picture points, 9999: nothing), the view's picture key
    m = re.search(r"zoom=(-?[\d.e+-]+) \| sel=(\d+),(-?\d+),(-?\d+) follow=(\d)", line)
    t = re.search(r"\bt=(-?[\d.e+-]+)", line)
    c = re.search(r"cam=(-?[\d.]+),(-?[\d.]+) selxy=(-?[\d.]+),(-?[\d.]+) key=(\d+)", line)
    if not m or not c:
        return None
    return {"zoom": float(m.group(1)), "sel": (int(m.group(2)), int(m.group(3))), "follow": m.group(5) == "1", "yaw": float(c.group(1)), "pitch": float(c.group(2)),
            "x": float(c.group(3)), "y": float(c.group(4)), "key": c.group(5), "t": float(t.group(1)) if t else 0.0}

def camera(binary, work):
    # 1280x800, dark, 2D unless said (the camera's rules do not depend on the size or the theme). PL_FIXED_DT 0.016 (shell_lib): a glide of 0.8 s is 50 frames, 1 s is 62 frames.
    # Time runs at the start speed (1 s = 7 days: Earth moves about 2 degrees of its orbit a second, many points at the picked zoom); (d) pauses it (space) to read the kept picture's key.
    # Before 0.74.0 an empty click let go of Earth (sel None, selxy 9999), a right drag let go (Earth drifts away), a pick jumped in one frame and a released turn stopped dead.
    for motion in ("off", "on"):                             # (PL_MOTION is Settings → Reduce motion: "off" moves, "on" jumps; shell_lib's default is "on")
        reduced = motion == "on"
        tag = " (Reduce motion)" if reduced else ""
        pick = ("4:key=space;" if reduced else "") + "14:click=ctl.sec.pick;20:click=pick.Earth"   # (Reduce motion pauses the lab when it opens: space runs it; without, time runs from the start)
        env = {"PL_NEWUI": "on", "PL_MOTION": motion}
        # (a) following Earth, time running: a left click on empty space (the bottom left of the picture) changes nothing for the camera
        r = run_app(binary, work, f"camera_a_{motion}", dict(env, PL_SCRIPT=pick + ";100:state;102:clickxy=60,600;" + ";".join(f"{f}:state" for f in range(106, 170, 2))), frames=176)
        cs = [cam(x) for x in r.states]
        check(len(cs) == 33 and all(cs), f"(a){tag}: 33 state lines with the camera fields ({len(cs)})")
        if len(cs) == 33 and all(cs):
            held, earth = cs[0], cs[0]["sel"]
            check(cs[-1]["t"] > held["t"], f"(a){tag}: time runs ({held['t']} -> {cs[-1]['t']})")
            check(held["follow"] and held["sel"][0] == 3, f"(a){tag}: Earth is picked and followed before the click ({held['sel']}, follow {held['follow']})")
            gone = [c for c in cs[1:] if c["sel"] != earth or not c["follow"]]
            worst = max(math.hypot(c["x"] - held["x"], c["y"] - held["y"]) for c in cs[1:])
            check(not gone and worst <= 2.0, f"(a){tag}: after an empty click Earth stays selected, followed and within 2 points of where it was held for 64 frames (lost {len(gone)}, furthest {worst:.1f})")
        # (b) a right drag by (100, 40) while following Earth: the view slides, Earth stays followed and keeps that offset while time runs
        r = run_app(binary, work, f"camera_b_{motion}", dict(env, PL_SCRIPT=pick + ";100:state;102:rdrag=500,400,600,440;" + ";".join(f"{f}:state" for f in range(112, 176, 2))), frames=182)
        cs = [cam(x) for x in r.states]
        check(len(cs) == 33 and all(cs), f"(b){tag}: 33 state lines with the camera fields ({len(cs)})")
        if len(cs) == 33 and all(cs):
            held, at = cs[0], cs[1]
            check(abs(at["x"] - held["x"] - 100) <= 2 and abs(at["y"] - held["y"] - 40) <= 2, f"(b){tag}: the drag moved Earth by (100, 40) ({at['x'] - held['x']:.1f}, {at['y'] - held['y']:.1f})")
            worst = max(math.hypot(c["x"] - at["x"], c["y"] - at["y"]) for c in cs[1:])
            check(all(c["follow"] and c["sel"] == held["sel"] for c in cs) and worst <= 2.0, f"(b){tag}: Earth stays followed at that offset for 64 frames (furthest {worst:.1f})")
        # (c) from Earth, a pick of Mars: the frames after, Mars is on its way (not jumped); within 1 s it is held where Earth was
        # (e) the wheel, one notch up at the middle: the zoom eases to +0.07 within 0.3 s (19 frames) and ends exactly there (Reduce motion: in one frame)
        r = run_app(binary, work, f"camera_ce_{motion}", dict(env, PL_SCRIPT=pick + ";100:state;102:click=pick.Mars;" + ";".join(f"{f}:state" for f in range(103, 175)) +
                                                                ";180:move=500,400;182:state;184:wheel=1;" + ";".join(f"{f}:state" for f in range(184, 214))), frames=220)
        cs = [cam(x) for x in r.states]
        check(len(cs) == 104 and all(cs), f"(c){tag}: 104 state lines with the camera fields ({len(cs)})")
        if len(cs) == 104 and all(cs):
            held = cs[0]
            mars = [c for c in cs[1:73] if c["sel"] != held["sel"]]
            check(len(mars) >= 60 and all(c["follow"] for c in mars), f"(c){tag}: Mars is picked and followed ({len(mars)} frames)")
            if len(mars) >= 60:
                d = [math.hypot(c["x"] - held["x"], c["y"] - held["y"]) for c in mars]
                first = next((i for i, v in enumerate(d) if v <= 2.0), None)
                if not reduced:
                    check(d[0] > 20 and d[1] > 20 and d[2] > 20 and d[2] <= d[0], f"(c): the first frames Mars is on its way, not jumped ({d[0]:.1f}, {d[1]:.1f}, {d[2]:.1f} points from the held place)")
                    check(first is not None and first <= 62 and max(d[first:]) <= 2.0, f"(c): Mars is held within 2 points after at most 1 s (from frame {first}, then furthest {max(d[first:]) if first is not None else -1:.1f})")
                else:
                    check(first is not None and first <= 2 and max(d[first:]) <= 2.0, f"(c){tag}: Mars is held within 2 points at once (from frame {first})")
            z0, zs = cs[73]["zoom"], [c["zoom"] for c in cs[74:]]
            want = z0 + 0.07
            i0 = next((i for i, z in enumerate(zs) if z != z0), None)            # the first frame the wheel has changed the zoom
            hit = next((i for i, z in enumerate(zs) if abs(z - want) < 1.1e-5), None)   # (the state line has 6 digits)
            check(i0 is not None and i0 <= 3, f"(e){tag}: the wheel changes the zoom ({z0:.5f} -> {zs[-1]:.5f})")
            if i0 is not None and not reduced:
                check(z0 < zs[i0] < want - 0.001, f"(e): the zoom eases (the first changed frame is on the way: {z0:.5f} -> {zs[i0]:.5f}, want {want:.5f})")
                check(hit is not None and hit - i0 <= 19 and all(z == zs[hit] for z in zs[hit:]), f"(e): the zoom reaches {want:.5f} within 0.3 s and stays exactly there ({hit} - {i0} frames, last {zs[-1]:.6f})")
            elif i0 is not None:
                check(hit == i0 and all(z == zs[hit] for z in zs[hit:]), f"(e){tag}: the zoom jumps to {want:.5f} in one frame (frame {hit}, first change {i0})")
    # (d) 3D, paused, nothing selected, Reduce motion off: a turning drag; after the release the turn coasts a few frames, then stops; within 1 s the view's picture key stays the same for 30 frames
    r = run_app(binary, work, "camera_d", {"PL_NEWUI": "on", "PL_MOTION": "off", "PL_3D": "1", "PL_SCRIPT": "4:key=space;60:drag=400,420,700,420;" + ";".join(f"{f}:state" for f in range(66, 140))}, frames=146)
    cs = [cam(x) for x in r.states]
    check(len(cs) == 74 and all(cs), f"(d): 74 state lines with the camera fields ({len(cs)})")
    if len(cs) == 74 and all(cs):
        yaws = [c["yaw"] for c in cs]                       # (the release is on frame 66, the first state line)
        check(all(yaws[i + 1] > yaws[i] + 1e-4 for i in range(3)), f"(d): the turn keeps a little momentum after the release ({', '.join(f'{y:.4f}' for y in yaws[:5])})")
        run = next((i for i in range(len(cs) - 29) if len({c["key"] for c in cs[i:i + 30]}) == 1), None)
        check(run is not None and run <= 62, f"(d): the turn stops and the picture key stays the same for 30 frames, starting within 1 s (frame {run}; yaw {yaws[0]:.4f} -> {yaws[-1]:.4f})")

# ---- 0.75.0: real depth ----
KSIM = 1.495978707e11 / 100 ** (1 / 3)              # orbit/units.h kSimLength (1 AU over the cube root of 100)
def axes3(yaw, pitch):
    # the 3D view's axes in space (orbit_view.cpp's turn: yaw + pi about z, then -pitch): right, up, forward (the camera looks along forward)
    ya, pa = yaw + math.pi, -pitch
    ca, sa, cb, sb = math.cos(ya), math.sin(ya), math.cos(pa), math.sin(pa)
    return (ca, -sa, 0.0), (sa * cb, ca * cb, -sb), (-sa * sb, -ca * sb, -cb)
def turn_for(fwd):
    # the yaw and pitch whose forward axis is fwd (a unit vector): towards the viewer t = -fwd = (sa sb, ca sb, cb), sb < 0 (pitch > 0)
    t = [-v for v in fwd]
    cb = t[2]; sb = -math.sqrt(max(0.0, 1 - cb * cb))
    return math.atan2(t[0] / sb, t[1] / sb) - math.pi, -math.atan2(sb, cb)
def comb(a, b, c, x, y, z):
    return tuple(a[i] * x + b[i] * y + c[i] * z for i in range(3))
def dotp(a, b):
    return sum(a[i] * b[i] for i in range(3))
def norm(v):
    n = math.sqrt(dotp(v, v)); return tuple(x / n for x in v)
def logged(r, tag, name):
    return [l.split() for l in r.out.splitlines() if l.startswith(tag + " " + name + " ")]

def depth(binary, work):
    # 1280x800, dark, 3D, True scale (the start's). The geometry is worked out here from the 45-degree field (focal = 400 / tan 22.5 deg) and the zoom (points per metre at the
    # target: min(W, H) / 34 x 10^zoom / kSimLength; the camera's distance d = focal / px), not from the app's camera code. PLMOONPX / PLBODYPX (PL_LOG_BODYPX) give each
    # moon's and body's place, drawn radius and camera-space place; PLSTARPX (PL_LOG_STARS) the bright stars' places. Before 0.75.0 the camera sat 90 sim lengths away
    # (nearly flat: a moon's radius at true scale was R px at any depth) and the sky had its own lens (100 degrees to the corner): (a) and (b) fail there.
    F, W, H, Z = 400 / math.tan(math.radians(22.5)), 1280.0, 800.0, 3.3
    px = min(W, H) / 34 * 10 ** Z / KSIM
    d = F / px
    pick = "14:click=ctl.sec.pick;20:click=pick.Earth"
    env = {"PL_NEWUI": "on", "PL_3D": "1", "PL_LOG_BODYPX": "1"}
    # (a) following Earth at zoom 3.3 (d = 6.6e8 m, beyond the Moon), paused: where the Moon is from Earth; then the camera put on the line from Earth through the Moon
    r = run_app(binary, work, "depth_probe", dict(env, PL_SCRIPT=pick + ";40:view=3.3,0.6,1.0;46:state"), frames=50)
    mo, ea = logged(r, "PLMOONPX", "Moon"), logged(r, "PLBODYPX", "Earth")
    check(mo and ea and len(mo[-1]) >= 12 and len(ea[-1]) >= 11, f"(a) the probe logs the Moon's and Earth's places ({len(mo)}, {len(ea)})")
    if mo and ea and len(mo[-1]) >= 12 and len(ea[-1]) >= 11:
        X, Y, Zc, R = (float(v) for v in (mo[-1][5], mo[-1][6], mo[-1][7], mo[-1][8]))
        eX, eY, eZ = (float(v) for v in ea[-1][8:11])
        check(abs(eX) < 1e-3 * d and abs(eY) < 1e-3 * d and abs(eZ - d) < 1e-6 * d, f"(a) Earth is on the camera's axis at the zoom's distance d = focal / px ({eX:.4g}, {eY:.4g}, {eZ:.6g} against {d:.6g} m)")
        right, up, fwd = axes3(0.6, 1.0)
        m = comb(right, up, fwd, X - eX, Y - eY, Zc - eZ)    # the Moon from Earth, in space
        dm = math.sqrt(dotp(m, m))
        yaw2, pitch2 = turn_for(norm(tuple(-v for v in m)))  # looking back at Earth along the Moon's direction: the Moon between the camera and Earth
        r2 = run_app(binary, work, "depth_moon", dict(env, PL_SCRIPT=pick + f";40:view=3.3,{yaw2:.9f},{pitch2:.9f};46:state"), frames=50)
        mo2 = logged(r2, "PLMOONPX", "Moon")
        check(mo2 and len(mo2[-1]) >= 12, f"(a) the Moon is drawn in front of Earth ({len(mo2)} lines)")
        if mo2 and len(mo2[-1]) >= 12:
            rad, Z2 = float(mo2[-1][4]), float(mo2[-1][7])
            L = d - dm                                        # its distance from the camera, from the probe's geometry
            want, atEarth = F * math.tan(math.asin(R / L)), F * R / d
            check(abs(R - 1737.4e3) < 1e3 and dm > 3.5e8 and dm < 4.1e8, f"(a) the Moon (radius {R / 1e3:.1f} km) is {dm / 1e3:,.0f} km from Earth (its real distance at True scale)")
            check(abs(Z2 - L) < 1e-6 * d, f"(a) the Moon is {L:.6g} m in front of the camera, between it and Earth at {d:.6g} m (logged {Z2:.6g})")
            check(abs(rad - want) <= 1.0, f"(a) the Moon's radius on the picture is focal tan(asin(R / L)) within 1 point ({rad:.2f} against {want:.2f})")
            check(rad > atEarth + 1.0, f"(a) the Moon is drawn bigger than it would be at Earth's distance ({rad:.2f} against {atEarth:.2f} points)")
    # (b) a star and a far planet move together: aimed at Neptune from beside Earth, the view turns from 15 degrees one side of it to 15 degrees the other (30 degrees); the bright
    #     star nearest Neptune and Neptune each land where the one pinhole camera puts their directions (from the first view, turned by the second view's axes) within 1 point
    r = run_app(binary, work, "depth_nprobe", dict(env, PL_SCRIPT=pick + ";40:view=3.3,0.6,1.0;46:state"), frames=50)
    ne, ea = logged(r, "PLBODYPX", "Neptune"), logged(r, "PLBODYPX", "Earth")
    check(ne and ea and len(ne[-1]) >= 11 and len(ea[-1]) >= 11, f"(b) the probe logs Neptune's and Earth's places ({len(ne)})")
    if ne and ea and len(ne[-1]) >= 11 and len(ea[-1]) >= 11:
        right, up, fwd = axes3(0.6, 1.0)
        v = norm(comb(right, up, fwd, *(float(x) - float(y) for x, y in zip(ne[-1][8:11], ea[-1][8:11]))))   # Neptune from Earth (the camera is 6.6e8 m from Earth)
        yc, pc = turn_for(v)
        ya, yb = yc - math.radians(15), yc + math.radians(15)
        r2 = run_app(binary, work, "depth_turn", dict(env, PL_LOG_STARS="1", PL_SCRIPT=pick + f";40:view=3.3,{ya:.9f},{pc:.9f};46:state;50:view=3.3,{yb:.9f},{pc:.9f};56:state"), frames=60)
        frames, cur = [], {"stars": {}, "nep": None, "pp": None}
        for l in r2.out.splitlines():                         # one picture's lines: the stars (the sky is drawn first), the bodies, the moons; a state line ends the ones we want
            f = l.split()
            if l.startswith("PLSTARPX "):
                if cur["nep"] is not None: cur = {"stars": {}, "nep": None, "pp": cur["pp"]}
                cur["stars"][int(f[1])] = (float(f[2]), float(f[3]))
            elif l.startswith("PLBODYPX Neptune "): cur["nep"] = (float(f[2]), float(f[3]))
            elif l.startswith("PLMOONPX ") and len(f) >= 12: cur["pp"] = (float(f[10]), float(f[11])); cur["F"] = float(f[9])
            elif l.startswith("PLSTATE "): frames.append(cur)
        check(len(frames) == 2 and all(fr["nep"] and fr["pp"] and fr["stars"] for fr in frames), f"(b) both views logged Neptune, the stars and the camera ({len(frames)})")
        if len(frames) == 2 and all(fr["nep"] and fr["pp"] and fr["stars"] for fr in frames):
            A, B = frames
            check(abs(A.get("F", 0) - F) < 1e-6 * F, f"(b) the focal length is 400 / tan(22.5 degrees) = {F:.4f} points ({A.get('F')})")
            ra, ua, fa = axes3(ya, pc)
            rb, ub, fb = axes3(yb, pc)
            def moved(p):                                     # where a direction seen at p in view A is seen in view B (the camera's own turn only)
                x, y = (p[0] - A["pp"][0]) / F, -(p[1] - A["pp"][1]) / F
                w = comb(ra, ua, fa, x, y, 1.0)
                zx, zy, zz = dotp(w, rb), dotp(w, ub), dotp(w, fb)
                return (B["pp"][0] + F * zx / zz, B["pp"][1] - F * zy / zz)
            near = sorted((math.hypot(p[0] - A["nep"][0], p[1] - A["nep"][1]), i) for i, p in A["stars"].items() if i in B["stars"])
            check(near and near[0][0] < 100, f"(b) a star near Neptune (6 degrees or less) ({near[0][0]:.0f} points apart)" if near else "(b) a star near Neptune")
            if near:
                i = near[0][1]
                ps, pn = moved(A["stars"][i]), moved(A["nep"])
                es = math.hypot(B["stars"][i][0] - ps[0], B["stars"][i][1] - ps[1])
                en = math.hypot(B["nep"][0] - pn[0], B["nep"][1] - pn[1])
                sep0 = (A["stars"][i][0] - A["nep"][0], A["stars"][i][1] - A["nep"][1])
                sep1 = (B["stars"][i][0] - B["nep"][0], B["stars"][i][1] - B["nep"][1])
                want = (ps[0] - pn[0], ps[1] - pn[1])
                print(f"  (b) star {i} {near[0][0]:.1f} points from Neptune; after the 30-degree turn: the star {es:.3f}, Neptune {en:.3f} points from the lens's places; separation off by {math.hypot(sep1[0] - want[0], sep1[1] - want[1]):.3f}")
                check(es <= 1.0, f"(b) the star (number {i}) moves as the camera turns, within 1 point ({es:.2f})")
                check(en <= 1.0, f"(b) Neptune moves as the camera turns, within 1 point ({en:.2f})")
                check(math.hypot(sep1[0] - want[0], sep1[1] - want[1]) <= 1.0, f"(b) the star and Neptune move together: their separation ({sep0[0]:.1f}, {sep0[1]:.1f}) -> ({sep1[0]:.1f}, {sep1[1]:.1f}) points, the lens's ({want[0]:.1f}, {want[1]:.1f})")
    # (c) the two gaps of 0.74.0, Reduce motion off (a glide) and on (at once). (c) nothing followed: a click on Neptune flies to it. (d) Earth selected, not followed, the
    #     view moved off it: ticking Follow glides to it.
    for motion in ("off", "on"):
        reduced, tag = motion == "on", " (Reduce motion)" if motion == "on" else ""
        e2 = {"PL_NEWUI": "on", "PL_MOTION": motion, "PL_LOG_BODYPX": "1"}
        run0 = "" if not reduced else "4:key=space;"
        pr = run_app(binary, work, f"depth_cprobe_{motion}", dict(e2, PL_SCRIPT=run0 + "40:state"), frames=44)
        ma = logged(pr, "PLBODYPX", "Neptune")
        check(bool(ma), f"(c){tag} Neptune is on the picture ({len(ma)})")
        if not ma:
            continue
        mx, my = float(ma[-1][2]), float(ma[-1][3])
        # (d): Earth selected and followed (PL_SELECT), Follow ticked off, the view moved away by a drag (200, 50), then Follow ticked on again
        for case, script in (("c", run0 + f"40:state;42:clickxy={mx:.1f},{my:.1f}"), ("d", run0 + "24:click=view.follow;30:drag=500,400,700,450;40:state;42:click=view.follow")):
            ee = dict(e2, PL_SELECT="Earth") if case == "d" else e2
            rr = run_app(binary, work, f"depth_{case}_{motion}", dict(ee, PL_SCRIPT=script + ";" + ";".join(f"{f}:state" for f in range(44, 130))), frames=134)
            cs = [cam(x) for x in rr.states]
            ok = len(cs) == 87 and all(cs)
            check(ok, f"({case}){tag}: 87 state lines with the camera fields ({len(cs)})")
            if not ok:
                continue
            what = "a click on Neptune while nothing is followed" if case == "c" else "ticking Follow with Earth selected"
            check(not cs[0]["follow"] and all(c["follow"] for c in cs[-40:]) and cs[-1]["sel"][0] == 3, f"({case}){tag}: {what}: it is followed after ({cs[0]['follow']} -> {cs[-1]['follow']}, sel {cs[-1]['sel']})")
            held = (cs[-1]["x"], cs[-1]["y"])
            dd = [math.hypot(c["x"] - held[0], c["y"] - held[1]) for c in cs[1:]]
            first = next((i for i, v in enumerate(dd) if v <= 2.0), None)
            if not reduced:
                check(dd[1] > 20 and dd[2] > 20 and dd[3] <= dd[1], f"({case}): {what} glides: the first frames it is on its way ({dd[1]:.1f}, {dd[2]:.1f}, {dd[3]:.1f} points from the held place)")
                check(first is not None and first <= 64 and max(dd[first:]) <= 2.0, f"({case}): ... and it is held within 2 points after at most 1 s (from frame {first})")
            else:
                check(first is not None and first <= 3 and max(dd[first:]) <= 2.0, f"({case}){tag}: {what} jumps: held within 2 points at once (from frame {first})")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"molbar": molbar, "centre": centre, "axes": axes, "start": start, "words": words, "keys": keys, "facts": facts, "factsrc": factsrc, "spectrum": spectrum, "density": density, "honest": honest, "stage6": stage6, "followmoon": followmoon, "camera": camera, "depth": depth}[part](binary, work)
    print(f"{part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
