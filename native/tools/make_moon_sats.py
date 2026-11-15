#!/usr/bin/env python3
"""0.27: the real places of the 13 big moons other than ours (owner's choice, 5 Nov 2026).

What it makes:
  src/orbit/moon_sats_data.inc        mean elements of a precessing ellipse for each moon, FITTED to JPL Horizons
  tests/reference/moon_sats_table.inc Horizons vectors at other dates (1 January 12:00 TDB of every 5th year 1805..2100,
                                      and a daily month in 2026), which tests/test_orbit.cpp compares the app with

Method (the same kind of model as JPL's own "planetary satellite mean elements", ssd.jpl.nasa.gov/sats/elem/: "the elements of a
precessing ellipse which has been fit in a least squares sense to the numerically integrated orbit"; JPL's table gives the periods to only
4-7 digits, too few for a fast moon over centuries, so the fit is done here, over 1800-2100, on Horizons vectors):
  in a fixed reference plane (the moon's Laplace plane where JPL's table gives its pole, else the planet's IAU 2015 equator),
  mean longitude L = L0 + n t + ndot t^2 / 2, longitude of periapsis w = w0 + wdot t, node O = O0 + Odot t, fixed a, e, i;
  t = days from J2000 (JD 2451545.0 TDB). Positions: planet-centred, ecliptic and mean equinox of J2000, km.
The fit uses 840 Horizons times per moon, none at a test date, in 25 windows from J2000 out to 1800 and 2100.

Run from native/:  python3 tools/make_moon_sats.py <cache folder>     (needs numpy, scipy; the replies are cached as received)
"""
import json, math, os, sys, time, urllib.parse, urllib.request

import numpy as np
from scipy.optimize import least_squares

API = "https://ssd.jpl.nasa.gov/api/horizons.api"
J2000 = 2451545.0
OBL = math.radians(84381.448 / 3600)   # obliquity of the ecliptic of J2000 that Horizons' "ECLIPTIC" frame uses (IAU 1976)

# name, Horizons id, centre, sidereal period (days, NASA fact sheets: the first guess only), reference pole RA, Dec (deg, ICRF), where the pole is from
MOONS = [
    ("Phobos", 401, 499, 0.31891, 317.7, 52.9, "JPL mean elements table, Laplace plane"),
    ("Deimos", 402, 499, 1.26244, 316.6, 53.5, "JPL mean elements table, Laplace plane"),
    ("Io", 501, 599, 1.769138, 268.1, 64.5, "JPL mean elements table, Laplace plane"),
    ("Europa", 502, 599, 3.551181, 268.1, 64.5, "JPL mean elements table, Laplace plane"),
    ("Ganymede", 503, 599, 7.154553, 268.2, 64.6, "JPL mean elements table, Laplace plane"),
    ("Callisto", 504, 599, 16.689017, 268.7, 64.8, "JPL mean elements table, Laplace plane"),
    ("Enceladus", 602, 699, 1.370218, 40.6, 83.5, "JPL mean elements table, Laplace plane"),
    ("Rhea", 605, 699, 4.5175, 40.6, 83.5, "JPL mean elements table, Laplace plane"),
    ("Titan", 606, 699, 15.945421, 36.4, 84.0, "JPL mean elements table, Laplace plane"),
    ("Titania", 703, 799, 8.705867, 257.311, -15.175, "Uranus's IAU 2015 pole (pck00011)"),
    ("Oberon", 704, 799, 13.463234, 257.311, -15.175, "Uranus's IAU 2015 pole (pck00011)"),
    ("Triton", 801, 899, 5.876854, 299.36, 43.46, "Neptune's IAU 2015 mean pole (pck00011, without its 0.70 / -0.51 deg wobble)"),
    ("Charon", 901, 999, 6.3872, 132.993, -6.163, "Pluto's IAU 2015 pole (pck00011)"),
]


def fetch(cache, cmd, centre, jds):
    key = f"{cmd}_{centre}_{jds[0]:.5f}_{len(jds)}.txt"
    path = os.path.join(cache, key)
    q = {"format": "text", "COMMAND": f"'{cmd}'", "CENTER": f"'500@{centre}'", "EPHEM_TYPE": "'VECTORS'", "TLIST": "'" + " ".join(f"{j:.6f}" for j in jds) + "'",
         "TLIST_TYPE": "'JD'", "TIME_TYPE": "'TDB'", "REF_PLANE": "'ECLIPTIC'", "REF_SYSTEM": "'ICRF'", "VEC_TABLE": "'1'", "VEC_CORR": "'NONE'",
         "OUT_UNITS": "'KM-S'", "CSV_FORMAT": "'YES'"}
    url = API + "?" + urllib.parse.urlencode(q)
    if not os.path.exists(path):
        for k in range(5):
            try:
                txt = urllib.request.urlopen(url, timeout=120).read().decode()
                break
            except Exception as e:  # network: retry
                print("retry", cmd, e, file=sys.stderr); time.sleep(2 ** k)
        else:
            raise SystemExit("Horizons unreachable")
        open(path, "w").write(txt)
    txt = open(path).read()
    rows = []
    soe, eoe = txt.index("$$SOE"), txt.index("$$EOE")
    for line in txt[soe + 5:eoe].strip().splitlines():
        f = [x.strip() for x in line.split(",")]
        rows.append((float(f[0]), float(f[2]), float(f[3]), float(f[4]), f[2], f[3], f[4]))
    src = txt[txt.find("{source:"):].split("}")[0] + "}" if "{source:" in txt else "?"
    date = next((l.split("/")[1].strip() for l in txt.splitlines() if l.startswith("Ephemeris /")), "?")
    return rows, url, src, date


def ref_to_ecl(ra, dec):
    """Columns: the reference frame's X (its node on the ICRF equator), Y, Z (the pole), in ecliptic J2000."""
    ra, dec = math.radians(ra), math.radians(dec)
    P = np.array([math.cos(dec) * math.cos(ra), math.cos(dec) * math.sin(ra), math.sin(dec)])
    X = np.cross([0.0, 0.0, 1.0], P); X /= np.linalg.norm(X)
    Y = np.cross(P, X)
    E = np.array([[1, 0, 0], [0, math.cos(OBL), math.sin(OBL)], [0, -math.sin(OBL), math.cos(OBL)]])   # equatorial -> ecliptic
    return E @ np.column_stack([X, Y, P])


def kepler(M, e):
    E = M + e * np.sin(M)
    for _ in range(8):
        E = E - (E - e * np.sin(E) - M) / (1 - e * np.cos(E))
    return E


def model(p, t, R):
    a, ec, es, wdot, sc, ss, Odot, L0, n, ndot = p
    e = min(math.hypot(ec, es), 0.5); w0 = math.atan2(es, ec)
    s = min(math.hypot(sc, ss), 1.0); inc = 2 * math.asin(s); O0 = math.atan2(ss, sc)
    L = L0 + n * t + 0.5 * ndot * t * t
    w = w0 + wdot * t; O = O0 + Odot * t
    M = L - w
    E = kepler(M, e)
    xo, yo = a * (np.cos(E) - e), a * math.sqrt(1 - e * e) * np.sin(E)
    om = w - O                     # argument of periapsis
    cu, su = np.cos(om), np.sin(om)
    x1, y1 = xo * cu - yo * su, xo * su + yo * cu
    ci, si = math.cos(inc), math.sin(inc)
    x2, y2, z2 = x1, y1 * ci, y1 * si
    cO, sO = np.cos(O), np.sin(O)
    xr, yr, zr = x2 * cO - y2 * sO, x2 * sO + y2 * cO, z2
    return (R @ np.vstack([xr, yr, zr])).T


def first_guess(t, xyz, R, P):
    loc = (R.T @ xyz.T).T
    h = np.cross(loc[0], loc[1]); h /= np.linalg.norm(h)
    inc = math.acos(max(-1, min(1, h[2]))); O = math.atan2(h[0], -h[1])
    k = np.argmin(abs(t))
    nodev = np.array([math.cos(O), math.sin(O), 0.0])
    u = math.atan2(np.dot(np.cross(nodev, loc[k]), h), np.dot(nodev, loc[k]))
    L0 = O + u - 2 * math.pi / P * t[k]
    return np.array([np.mean(np.linalg.norm(loc, axis=1)), 1e-3, 0.0, 0.0, math.sin(inc / 2) * math.cos(O), math.sin(inc / 2) * math.sin(O), 0.0, L0, 2 * math.pi / P, 0.0])


def fit(t, xyz, win, P):
    """win: the window number of each time, windows numbered from the one nearest J2000 outwards; added one at a time."""
    R = fit.R
    first = win == 0
    p = first_guess(t[first], xyz[first], R, P)
    free_rates = np.array([1, 1, 1, 0, 1, 1, 0, 1, 1, 0], bool)
    for m in range(win.max() + 1):
        idx = win <= m
        span = t[idx].max() - t[idx].min()
        mask = np.ones(10, bool) if span > 5 * 365.25 else free_rates
        def res(q):
            pp = p.copy(); pp[mask] = q
            return (model(pp, t[idx], R) - xyz[idx]).ravel()
        r = least_squares(res, p[mask], x_scale="jac", method="lm", max_nfev=20000)
        p[mask] = r.x
    return p


def main():
    cache = sys.argv[1]
    os.makedirs(cache, exist_ok=True)
    here = os.path.dirname(os.path.abspath(__file__))
    rng_offsets = [0.1371 + 0.6180339887 * k % 1 for k in range(50)]
    out_data, out_ref, urls = [], [], []
    test_jd = [2378497.0 + 1826.2 * 0 for _ in range(0)]
    # test dates: 1 January 12:00 TDB of 1805, 1810, ..., 2100 (as tests/reference/moon_table.inc) and daily 2026-01-01 .. 2026-01-30 00:00 TDB
    import datetime
    def jd_of(y, mo, d, h=0.0):
        a = (14 - mo) // 12; yy = y + 4800 - a; mm = mo + 12 * a - 3
        jdn = d + (153 * mm + 2) // 5 + 365 * yy + yy // 4 - yy // 100 + yy // 400 - 32045
        return jdn - 0.5 + h / 24
    test_jd = [jd_of(y, 1, 1, 12) for y in range(1805, 2101, 5)] + [jd_of(2026, 1, 1) + d for d in range(30)]   # (Horizons' Triton and Charon start on 2 Jan 1800)
    for name, cmd, centre, P, ra, dec, poleSrc in MOONS:
        R = ref_to_ecl(ra, dec)
        # 25 windows of 40 times across about 2.5 orbits, from J2000 outwards with growing gaps (so the phase carries from one to the next),
        # reaching 1800 and 2100; never at a test date (those are at 00:00 or 12:00 TDB; these start 0.137 days after)
        offs = [0.0, 0.03, -0.1, 0.4, -1.3, 3.1, -6.2, 11.7, -19.3, 28.1, -37.2, 47.4, -58.6, 66.3, -77.1, 83.2, -91.4, 95.1, -99.2, 99.4, -112.3, -127.9, -143.6, -158.2, -171.7, -184.4, -195.6, -198.9]   # years from J2000
        jds = []
        for o in offs:
            start = J2000 + 0.137 + o * 365.25 - (2.6 * P if o > 0 else 0)
            jds.append([start + 2.5 * P * (k + rng_offsets[k] * 0.5) / 40 for k in range(40)])
        rows, win = [], []
        for w, ws in enumerate(jds):
            r, url, src, date = fetch(cache, cmd, centre, ws)
            rows += r; win += [w] * len(r)
        t = np.array([r[0] - J2000 for r in rows]); xyz = np.array([[r[1], r[2], r[3]] for r in rows])
        # 0.52.1: a moon that goes round its reference pole backwards in nearly its plane (Uranus's moons about Uranus's IAU pole: inclination near 180 deg) left the
        # fit stuck at sin(i/2) = 1, the clamp in model() (Titania's i came out exactly pi, its node then meaningless). For those the pole is reversed, so the
        # inclination is small and the fit is free; the same plane, the same positions
        w0 = np.array(win) == 0
        g = first_guess(t[w0], xyz[w0], R, P)
        if 2 * math.asin(min(math.hypot(g[4], g[5]), 1.0)) > math.radians(170):
            ra, dec = round((ra + 180.0) % 360.0, 9), -dec
            R = ref_to_ecl(ra, dec)
            poleSrc += ", reversed (the moon goes round it backwards)"
        fit.R = R
        p = fit(t, xyz, np.array(win), P)
        err = np.linalg.norm(model(p, t, R) - xyz, axis=1)
        trow, turl, tsrc, tdate = [], [], None, None
        for k in range(0, len(test_jd), 50):
            r, url, src, date = fetch(cache, cmd, centre, test_jd[k:k + 50])
            trow += r; turl.append(url); tsrc, tdate = src, date
        tt = np.array([r[0] - J2000 for r in trow]); txyz = np.array([[r[1], r[2], r[3]] for r in trow])
        terr = np.linalg.norm(model(p, tt, R) - txyz, axis=1)
        print(f"{name:9s} fit rms {np.sqrt(np.mean(err**2)):9.1f} km  max {err.max():9.1f}  test max {terr.max():9.1f} km ({100 * terr.max() / p[0]:.2f} % of a)  src {tsrc}")
        out_data.append((name, cmd, centre, ra, dec, poleSrc, p, tsrc, err))
        out_ref.append((name, trow))
        urls += [(name, u) for u in turl]
    # ---- the app's data ----
    with open(os.path.join(here, "..", "src", "orbit", "moon_sats_data.inc"), "w") as f:
        f.write("// GENERATED by native/tools/make_moon_sats.py; do not edit. Mean elements of a precessing ellipse, fitted (least squares, 1,000 times per moon\n"
                "// in 25 windows over 1800-2100) to NASA/JPL Horizons planet-centred vectors (https://ssd.jpl.nasa.gov/api/horizons.api, retrieved 5 Nov 2026).\n"
                "// Reference plane: pole RA, Dec (ICRF, deg); its X axis is its node on the ICRF equator. t = days from J2000 (JD 2451545.0 TDB).\n"
                "// L = L0 + n t + ndot t^2 / 2 (mean longitude), periapsis longitude w0 + wdot t, node O0 + Odot t (all from the plane's X axis), rad and rad/day.\n"
                "// { name, Horizons id, pole RA, pole Dec, a (km), e, w0, wdot, i, O0, Odot, L0, n, ndot }\n")
        for name, cmd, centre, ra, dec, poleSrc, p, src, err in out_data:
            a, ec, es, wdot, sc, ss, Odot, L0, n, ndot = p
            e = math.hypot(ec, es); w0 = math.atan2(es, ec); inc = 2 * math.asin(min(math.hypot(sc, ss), 1)); O0 = math.atan2(ss, sc)
            f.write(f"    {{\"{name}\", {cmd}, {ra!r}, {dec!r}, {a:.10g}, {e:.10g}, {w0:.17g}, {wdot:.17g}, {inc:.17g}, {O0:.17g}, {Odot:.17g}, {L0:.17g}, {n:.17g}, {ndot:.17g}}},"
                    f"   // {src}; pole: {poleSrc}; fit rms {np.sqrt(np.mean(err**2)):.0f} km\n")
    # ---- the test's reference ----
    with open(os.path.join(here, "..", "tests", "reference", "moon_sats_table.inc"), "w") as f:
        f.write("// GENERATED by native/tools/make_moon_sats.py; do not edit. Source: NASA/JPL Horizons API, planet-centred vectors of 13 moons, ecliptic and mean equinox\n"
                "// of J2000, geometric, km (the digits exactly Horizons'). Dates: 1 January 12:00 TDB of every 5th year 1805..2100 (60) and daily 2026-01-01..30 00:00 TDB (30):\n"
                "// none of them is used by the fit. Queries (one per 50 dates; TLIST of Julian dates):\n")
        for name, u in urls:
            f.write(f"//   {name}: {u}\n")
        f.write("// { moon index (order of src/orbit/moon_sats_data.inc), Julian date (TDB), x, y, z (km) }\n{\n")
        for k, (name, rows) in enumerate(out_ref):
            for r in rows:
                f.write(f"{{{k}, {r[0]:.6f}, {r[4]}, {r[5]}, {r[6]}}},\n")
        f.write("}\n")


if __name__ == "__main__":
    main()
