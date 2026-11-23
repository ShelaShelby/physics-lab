#!/usr/bin/env python3
# 0.22 stage 7: the bug hunt of 4 Nov 2026, checked in the REAL app (Linux, software Vulkan; NOT Windows, NOT a real mouse). One part per fix:
#   scalebar  the Orbit scale bar's label is its real length: the light time under it is that length divided by c (IAU 2012: 1 AU = 149,597,870.7 km exactly;
#             SI: c = 299,792.458 km/s exactly; 1 mi = 1.609344 km exactly). Before: a round number of km or miles was labelled in rounded AU ("3 AU" over 3.34 AU, "light takes 28 minutes")
#   jumpcheck the Atom Lab's "Check a jump": the same shell twice asks for two shells; two shells of one n say so once (the same-n note was written twice)
#   search    search finds the Atom Lab's elements and the Molecule Lab's molecules, and offers every other lab (before: Orbit <-> Atom only)
#   space     Space in the Molecule Lab (no clock) leaves the other labs paused
#   factmol   the Molecule Lab shows the fact line too, clear of its style bar (it was always hidden under the bar at the normal text size)
#   dateenter Enter in the Time panel's date box jumps to that date, as Go does (it did nothing)
#   check_022s7.py <PhysicsLab binary> <work folder> <part>
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, overlap

fails = checks = 0
def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

AU_KM, C_KMS, MI_KM = 149597870.7, 299792.458, 1.609344

def light_words(sec):
    # how the app writes a light time (orbit_view.cpp): minutes / hours / days, two significant figures under 10 (0.39.2: 10 AU is 1.4 hours, it said "1 hour"), whole ones from 10; "1 minute" without the s
    def pl(v, u):
        n = f"{float(f'{v:.1e}'):g}" if v < 10 else f"{round(v):,}"
        return f"{n} {u}" + ("" if n == "1" else "s")
    if sec < 60: return pl(sec, "second")
    if sec < 3600: return pl(sec / 60, "minute")
    if sec < 86400: return pl(sec / 3600, "hour")
    return pl(sec / 86400, "day")

def scalebar(binary, work):
    seen = 0
    for imperial in (False, True):
        for zoom in ("-0.8", "-1.2", "-1.6", "-2.0", "-2.4", "0.4", "1.0"):
            env = {"PL_ZOOM": zoom}
            if imperial: env["PL_IMPERIAL"] = "1"
            r = run_app(binary, work, f"bar_{'mi' if imperial else 'km'}_{zoom}", env, frames=12)
            texts = [t[2] for t in r.texts]
            lab = [t for t in texts if re.fullmatch(r"(≈ )?[\d,.]+ (million )?(km|mi|AU)", t)]
            light = [t for t in texts if t.startswith("light takes ")]
            check(lab and light, f"zoom {zoom}: the readout has the bar's length and the light time ({lab[:1]}, {light[:1]})")
            if not (lab and light): continue
            m = re.fullmatch(r"(?:≈ )?([\d,.]+) (million )?(km|mi|AU)", lab[-1])
            v = float(m.group(1).replace(",", "")) * (1e6 if m.group(2) else 1)
            km = v * AU_KM if m.group(3) == "AU" else v * (MI_KM if m.group(3) == "mi" else 1)
            sec = km / C_KMS
            if sec < 1 or sec >= 86400 * 365.25: continue        # (ms and years are written another way)
            seen += 1
            check(light[-1] == "light takes " + light_words(sec), f"zoom {zoom}{' imperial' if imperial else ''}: \"{lab[-1]}\" means \"light takes {light_words(sec)}\", the readout says \"{light[-1]}\"")
    check(seen >= 8, f"enough zooms were compared ({seen})")

def jumpcheck(binary, work):
    def lines(frm, to):
        r = run_app(binary, work, f"jump_{frm}_{to}", {"PL_ATOM": "1", "PL_ATOM_SET": f"jfrom={frm};jto={to}"}, frames=12)
        n = r.note("jumpcheck")
        return n[-1].split(" | ")[1:] if n else None
    same = lines("2p", "2p")
    check(same is not None and "Choose two different shells." in same, f"2p to 2p: asks for two different shells ({same})")
    one_n = lines("2s", "2p")
    noteN = [l for l in (one_n or []) if "same n" in l and "fine-structure" in l]
    check(one_n is not None and len(noteN) == 1, f"2s to 2p: the same-n note is written once ({len(noteN)}: {one_n})")
    ok = lines("2p", "1s")
    check(ok is not None and not any("Choose two" in l for l in ok) and len(ok) >= 2, f"2p to 1s: the jump lines are listed ({ok[:2] if ok else ok})")

def search(binary, work):
    def q(name, env, word):
        r = run_app(binary, work, "search_" + name, dict(env, PL_SCRIPT=f"10:key=ctrl+k;16:text={word};24:key=enter;30:state"), frames=34)
        hits = [ln for ln in r.out.splitlines() if ln.startswith("PLSEARCH ")]
        return r, hits[-1] if hits else ""
    r, h = q("iron", {"PL_ATOM": "1"}, "iron")
    check("hits=Iron (Fe, Z = 26)|" in h, f"Atom Lab: \"iron\" finds the element first ({h[:90]})")
    st = r.states[-1] if r.states else ""
    check(" Z=26 " in st and " N=30 " in st, f"Atom Lab: Enter loads iron-56 ({st[:120]})")
    r, h = q("benz", {"PL_MOLECULE": "H2O"}, "benz")
    check("Benzene" in h, f"Molecule Lab: \"benz\" finds benzene ({h[:90]})")
    check(any("C₆H₆" in t[2] and "Benzene" in t[2] for t in r.texts), "Molecule Lab: Enter shows benzene (the chip)")
    r, h = q("labs", {"PL_MOLECULE": "H2O"}, "go to")
    check("Go to the Orbit Lab" in h and "Go to the Atom Lab" in h and "Go to the Molecule Lab" not in h, f"Molecule Lab: search offers the two other labs ({h[:90]})")
    r, h = q("labs_orbit", {}, "go to")
    check("Go to the Atom Lab" in h and "Go to the Molecule Lab" in h, f"Orbit Lab: search offers the two other labs ({h[:90]})")

def space(binary, work):
    r = run_app(binary, work, "space_mol", {"PL_SCRIPT": "4:state;6:key=3;10:key=space;14:key=1;20:state"}, frames=24)
    st = r.states
    check(len(st) >= 2 and "paused=1" in st[0] and "paused=1" in st[-1], f"Space in the Molecule Lab leaves the Orbit Lab paused ({[s[s.find('paused'):s.find('paused') + 8] for s in st]})")
    o = run_app(binary, work, "space_orbit", {"PL_SCRIPT": "10:key=space;14:state"}, frames=18)
    check(o.states and "paused=0" in o.states[-1], "Space in the Orbit Lab still plays")

def factmol(binary, work):
    r = run_app(binary, work, "factmol", {"PL_MOLECULE": "H2O"}, frames=20)
    f, b = r.rects.get("factline"), r.rects.get("moleculebar")
    check(f is not None and b is not None, f"Molecule Lab: the fact line is shown ({f}) with the style bar ({b})")
    if f and b: check(not overlap(f, b), f"Molecule Lab: the fact line is clear of the style bar ({f} / {b})")

def dateenter(binary, work):
    import datetime
    want = int(datetime.datetime(1986, 2, 9, 12, 0, tzinfo=datetime.timezone.utc).timestamp() * 1000)
    r = run_app(binary, work, "dateenter", {"PL_SCRIPT": "8:click=top.chip;14:click=time.mode@0.9;20:click=##shelldate;24:text=1986-02-09;28:key=enter;34:state"}, frames=38)
    m = re.search(r"epochMs=(-?\d+)", r.states[-1]) if r.states else None
    check(m is not None and abs(int(m.group(1)) - want) < 60000 and not r.scripted_missing(), f"Enter in the date box jumps to 1986-02-09 12:00 UT ({m.group(1) if m else None}, want {want}; {r.scripted_missing()[:1]})")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"scalebar": scalebar, "jumpcheck": jumpcheck, "search": search, "space": space, "factmol": factmol, "dateenter": dateenter}[part](binary, work)
    print(f"{part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
