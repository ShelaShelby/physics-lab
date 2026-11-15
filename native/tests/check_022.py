#!/usr/bin/env python3
# 0.22: the owner's look-through (13 findings) checked in the REAL app (Linux, software OpenGL; NOT Windows, NOT a real mouse). One part per fix that the unit tests cannot see:
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
#   check_022.py <PhysicsLab binary> <work folder> <part>
import os, re, sys
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

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"molbar": molbar, "centre": centre, "axes": axes, "start": start, "words": words, "keys": keys, "facts": facts, "factsrc": factsrc, "spectrum": spectrum, "density": density, "honest": honest, "stage6": stage6}[part](binary, work)
    print(f"{part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
