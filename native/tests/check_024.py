#!/usr/bin/env python3
# 0.24, checked in the REAL app (Linux, software Vulkan; NOT Windows, NOT a real mouse). The physics of the vibrating density is in tests/test_density.cpp.
#   densvib  the Density style vibrates for water: a picked mode moves the cloud and the surface (the picture differs from the still one), its two ends differ from each
#            other, and the panel says how it is made; a molecule with no vibrating density says so and does not move
#   densfit  the Density style's surface fits the picture: nothing of it under the top bar or off the free part (it ran off the top before), for water and benzene
#   nobuild  the Molecule Lab has no Build mode any more (the owner's decision of 4 Nov 2026): no Mode switch, no build entry in search
#   check_024.py <PhysicsLab binary> <work folder> <part>
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from shell_lib import run_app, Bmp

fails = checks = 0
def check(ok, what):
    global fails, checks
    checks += 1
    if not ok:
        fails += 1
        print("FAIL", what)

def differ(a, b, x0, y0, x1, y1):
    A, B = Bmp(a.bmp), Bmp(b.bmp)
    return sum(1 for y in range(y0, y1, 2) for x in range(x0, x1, 2) if A.px(x, y) != B.px(x, y))

def densvib(binary, work):
    base = {"PL_MOLECULE": "H2O", "PL_MOLECULE_STYLE": "real"}
    still = run_app(binary, work, "dv_still", base, frames=24)
    plus = run_app(binary, work, "dv_plus", dict(base, PL_SCRIPT="8:click=molecule.vib.0"), frames=24)               # Reduced motion: held at the + end
    n = differ(still, plus, 0, 60, 900, 740)
    check(n > 300, f"water, Density: the bend moves the cloud and the surface ({n} sampled pixels differ)")
    check(any("Born" in t[2] for t in plus.texts), "the panel says the cloud follows the atoms (Born–Oppenheimer) and how it is made")
    real = run_app(binary, work, "dv_real", dict(base, PL_SCRIPT="8:click=molecule.vib.0;16:click=molecule.vib.real"), frames=30)
    check(0 < differ(still, real, 0, 60, 900, 740) < n, "Real size moves the cloud less than 3 times bigger")
    o = run_app(binary, work, "dv_co2", {"PL_MOLECULE": "CO2", "PL_MOLECULE_STYLE": "real"}, frames=20)
    check(any("only water vibrates so far" in t[2] for t in o.texts), "CO2, Density: the panel says only water vibrates in this style so far")

def densfit(binary, work):
    for mol in ("H2O", "C6H6"):
        r = run_app(binary, work, "df_" + mol, {"PL_MOLECULE": mol, "PL_MOLECULE_STYLE": "real", "PL_THEME": "dark"}, frames=24)
        b = Bmp(r.bmp)
        top, pc = r.rects.get("topbar"), r.rects.get("panel.controls")
        if not top or not pc:
            check(False, f"{mol}: the top bar and the panel are logged"); continue
        bg = b.px(20, int(top[3]) + 30)
        lit = lambda y: sum(1 for x in range(40, int(pc[0]) - 60, 3) if sum(abs(b.px(x, y)[i] - bg[i]) for i in range(3)) > 40)
        first = next((y for y in range(int(top[3]) + 1, b.h // 2) if lit(y) >= 3), None)          # the surface's top edge
        check(first is not None and first > top[3] + 12, f"{mol}, Density: the surface starts clearly below the top bar (top edge at y {first}, the bar ends at {top[3]:.0f})")

def nobuild(binary, work):
    r = run_app(binary, work, "nb", {"PL_MOLECULE": "H2O", "PL_SCRIPT": "8:click=molecule.mode.build;14:key=ctrl+k;20:text=build"}, frames=28)
    check(bool(r.scripted_missing()), "there is no Build switch to click")
    hits = [ln for ln in r.out.splitlines() if ln.startswith("PLSEARCH ")]
    check(hits and "Build" not in hits[-1], f"search offers nothing to build ({hits[-1][:80] if hits else 'no search'})")

if __name__ == "__main__":
    binary, work, part = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(work, exist_ok=True)
    {"densvib": densvib, "densfit": densfit, "nobuild": nobuild}[part](binary, work)
    print(f"{part}: {checks} checks, {fails} failed")
    sys.exit(1 if fails else 0)
