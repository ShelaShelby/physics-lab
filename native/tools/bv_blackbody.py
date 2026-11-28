#!/usr/bin/env python3
"""0.97.0 (the outside review of 28 Nov 2026, second, part 2): is Ballesteros 2012 eq. (14) a black body's colour temperature, and how far is it from a
star's effective temperature?

  python3 native/tools/bv_blackbody.py <checkout of ShelaShelby/physics-lab-sources>

Inputs (all saved in the sources repo, data/stars/):
  - the Johnson B and V passbands of M. S. Bessell, PASP 102, 1181 (1990), Table 2 (columns B and V), as the SVO Filter Profile Service serves them
    (Generic/Bessell.B, Generic/Bessell.V: the same numbers, every 10 nm; SVO lists them as energy-counter responses);
  - the SVO's Vega zero points of the same two filters (6.13268e-9 and 3.62708e-9 erg/cm2/s/A: B - V = 0 for Vega);
  - E. Mamajek, "A Modern Mean Dwarf Stellar Color and Effective Temperature Sequence", version 2022.04.16 (Pecaut & Mamajek 2013, ApJS 208, 9, Table 5):
    each dwarf's B-V and Teff.
Synthetic colour of a black body at T: B - V = -2.5 log10(<F>_B / <F>_V) + 2.5 log10(ZP_B / ZP_V), <F> = int F_lambda S dlambda / int S dlambda
(the energy-counter mean, linear between the 10 nm points). The black body's colour temperature for a B - V is the T whose synthetic colour is that B - V.
Prints, for each dwarf with a B - V: Teff, eq. (14)'s T, the synthetic black body's T, and eq. (14) / Teff. Python 3, standard library only."""
import math, os, re, sys

H, C, K = 6.62607015e-34, 299792458.0, 1.380649e-23   # CODATA 2022 (exact)


def passband(path):
    pts = []
    for line in open(path, encoding="utf-8"):
        f = line.split()
        if len(f) == 2:
            pts.append((float(f[0]) * 1e-10, float(f[1])))   # angstrom -> m
    return pts


def mean_flux(T, band, n=20):
    num = den = 0.0
    for (l0, s0), (l1, s1) in zip(band, band[1:]):
        for i in range(n):
            l = l0 + (l1 - l0) * (i + 0.5) / n
            s = s0 + (s1 - s0) * (i + 0.5) / n
            b = 2 * H * C * C / l ** 5 / math.expm1(H * C / (l * K * T))
            num += b * s
            den += s
    return num / den


def bv_black(T, B, V, zpB, zpV):
    return -2.5 * math.log10(mean_flux(T, B) / mean_flux(T, V)) + 2.5 * math.log10(zpB / zpV)


def t_of_bv(bv, B, V, zpB, zpV):
    lo, hi = 1500.0, 1e7          # B - V falls as T rises
    for _ in range(200):
        mid = math.sqrt(lo * hi)
        if bv_black(mid, B, V, zpB, zpV) > bv:
            lo = mid
        else:
            hi = mid
    return math.sqrt(lo * hi)


def eq14(bv):
    return 4600 * (1 / (0.92 * bv + 1.7) + 1 / (0.92 * bv + 0.62))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    d = os.path.join(sys.argv[1], "data", "stars")
    B = passband(os.path.join(d, "svo_generic_bessell_B_2026-11-28.dat"))
    V = passband(os.path.join(d, "svo_generic_bessell_V_2026-11-28.dat"))
    zpB, zpV = 6.13268e-9, 3.62708e-9
    print("black body T (K) -> synthetic B-V -> eq. (14) T")
    for T in (3000, 4000, 5000, 5772, 7000, 10000, 15000, 20000, 31400, 40000):
        bv = bv_black(T, B, V, zpB, zpV)
        print("  %6d  %+.3f  %7.0f  (%+.1f %%)" % (T, bv, eq14(bv), (eq14(bv) / T - 1) * 100))
    print("SpT     Teff   B-V    eq14   bb(syn)  eq14/Teff")
    for line in open(os.path.join(d, "EEM_dwarf_UBVIJHK_colors_Teff_2026-11-28.txt"), encoding="utf-8"):
        f = line.split()
        if len(f) < 9 or line.startswith("#") or not re.match(r"^[OBAFGKM]\d", f[0]):
            continue
        try:
            teff, bv = float(f[1]), float(f[8])
        except ValueError:
            continue
        print("%-6s %6.0f %+.3f %7.0f %7.0f   %.3f" % (f[0], teff, bv, eq14(bv), t_of_bv(bv, B, V, zpB, zpV), eq14(bv) / teff))
    return 0


if __name__ == "__main__":
    sys.exit(main())
