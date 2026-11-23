#!/usr/bin/env python3
# 0.22 stage 7: the bug hunt of 4 Nov 2026, in the REAL app (Linux, software Vulkan; NOT Windows, NOT a real mouse). tests/TESTS.md lists the parts kept and why.
#   search    search finds the Atom Lab's elements and the Molecule Lab's molecules, and offers every other lab
#   check_022s7.py <PhysicsLab binary> <work folder> search
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

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"search": search}[part](binary, work)
    print(f"{part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
