#!/usr/bin/env python3
# 0.22: the owner's look-through, checked in the REAL app (Linux, software Vulkan; NOT Windows, NOT a real mouse). tests/TESTS.md lists the parts kept and why.
#   keys     1 2 3 open the labs, + and - zoom, R resets the view
#   facts    the Orbit and Atom Labs show only their own facts and the general ones (stepping through all of them in the Facts panel)
#   factsrc  the Facts panel names the fact's own source; Show in About opens About at that entry
#   honest   the new names and captions are on screen (Density glow, the Molecule Lab's Density style, "not an electron", the molecule "never stops")
#   check_022.py <PhysicsLab binary> <work folder> <part>
import math, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app

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

def astate(r):
    v = [ln[9:] for ln in r.out.splitlines() if ln.startswith("PLASTATE ")]
    return dict(t.split("=", 1) for t in v[-1].split("\t") if "=" in t) if v else {}

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

def moonoff(line):
    m = re.search(r"moonoff=(-?[\d.]+),(-?[\d.]+) planetoff=(-?[\d.]+),(-?[\d.]+)", line)
    return tuple(float(v) for v in m.groups()) if m else None

def cam(line):
    # the state line's camera fields (0.74.0): sel kind and id, follow, zoom, yaw, pitch, where the selected thing was drawn (picture points, 9999: nothing), the view's picture key
    m = re.search(r"zoom=(-?[\d.e+-]+) \| sel=(\d+),(-?\d+),(-?\d+) follow=(\d)", line)
    t = re.search(r"\bt=(-?[\d.e+-]+)", line)
    c = re.search(r"cam=(-?[\d.]+),(-?[\d.]+) selxy=(-?[\d.]+),(-?[\d.]+) key=(\d+)", line)
    if not m or not c:
        return None
    return {"zoom": float(m.group(1)), "sel": (int(m.group(2)), int(m.group(3))), "follow": m.group(5) == "1", "yaw": float(c.group(1)), "pitch": float(c.group(2)),
            "x": float(c.group(3)), "y": float(c.group(4)), "key": c.group(5), "t": float(t.group(1)) if t else 0.0}

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

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"keys": keys, "facts": facts, "factsrc": factsrc, "honest": honest}[part](binary, work)
    print(f"{part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
