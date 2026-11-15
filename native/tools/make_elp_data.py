#!/usr/bin/env python3
"""Download ELP2000-82B, measure it against the Horizons Moon table, truncate it by a measured rule, and write native/src/orbit/elp_data.inc.

Python 3, standard library only. Needs the internet the first time (cdsarc.cds.unistra.fr); the files are kept in a cache folder (not committed).
    python3 native/tools/make_elp_data.py [--cache DIR] [--ratio 1.5]
Run native/tools/make_moon_ref.py first: it writes the Horizons table this tool measures against (tests/reference/moon_table.inc).

Source: M. Chapront-Touze and J. Chapront, "ELP 2000-85: a semi-analytical lunar ephemeris adequate for historical times", Astron. Astrophys. 190, 342 (1988); the
series ELP2000-82B, CDS catalogue VI/79 (https://cdsarc.cds.unistra.fr/ftp/VI/79/): files ELP1..ELP36 and the Fortran subroutine elp82b.f (the ReadMe says the
arguments, constants and coordinate systems are in the notice elp82b.ps, "Lunar solution ELP 2000-82B", which ships with elp82b.f). EVERY constant below is read by this
tool from the text of elp82b.f; the series form, the arguments and the change of coordinates are the subroutine's, written here term by term. Nothing is from memory.

What the subroutine does (and this tool follows, with prec = 0, i.e. no truncation inside the series):
    longitude, latitude (arcsec) and distance (km) are sums of  A * T^p * sin(argument),  T = (JD_TDB - 2451545.0) / 36525,  p = 0, 1 or 2.
    files 1-3   main problem, argument = sum of 4 integers x the Delaunay arguments D, l', l, F (polynomials in T up to T^4); the distance uses cosine (sine of argument + pi/2);
                the amplitude is the file's coefficient plus the corrections of the fit to DE200/LE200 (delnu, delnp, delg, dele, delep: coefficients 2..6 of the file)
    files 4-9, 22-36  argument = phase + iz*zeta + the 4 Delaunay arguments (linear in T)
    files 10-15 planetary table 1: phase + 8 planetary mean longitudes + D, l, F;     files 16-21 planetary table 2: phase + 7 planetary longitudes + D, l', l, F
    longitude gets the mean longitude W1 (polynomial in T) added; the distance is scaled by a0/ath (the fit's change of the Moon's mean distance);
    the spherical coordinates become x, y, z in the mean dynamical ecliptic and inertial equinox of J2000 with the subroutine's precession-matrix polynomials p1..p5, q1..q5.
This tool merges all 36 files into ONE list of terms { coordinate, power of T, basis, 13 integer multipliers, phase, amplitude }, with the multipliers of
[D, l', l, F, zeta, Mercury, Venus, Earth, Mars, Jupiter, Saturn, Uranus, Neptune]; the C++ (src/orbit/moon_elp.cpp) evaluates that list the same way.

The truncation rule (measured, not assumed): all terms are used first and the error against Horizons (170 rows, km) is measured; then terms are dropped from the
smallest upward (size = amplitude x the largest |T|^p over 1800..2100, in arcsec; distance terms in km converted at the mean distance), one step of a factor sqrt(2) in
size at a time, as long as the worst error stays within RATIO (1.5) times the full-series worst error. The first step that breaks the limit stops the walk.
A second condition was added after the first, alone, was seen to leave 9.4 km between the kept and the full series at dates not in the table (the table has only 170
dates, most of them near 2026): at 300 seeded dates between 1800 and 2100 the kept series must stay within (RATIO - 1) x the full worst error of the full series, so by the triangle
inequality the 1.5 x limit also holds there.
"""
import array
import gzip
import math
import os
import random
import re
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                                   # native/
OUT = os.path.join(ROOT, "src", "orbit", "elp_data.inc")
OUT_PYCHECK = os.path.join(ROOT, "tests", "reference", "moon_elp_pycheck.inc")
TABLE = os.path.join(ROOT, "tests", "reference", "moon_table.inc")
BASE = "https://cdsarc.cds.unistra.fr/ftp/VI/79/"
# Records (lines, the first is a title) of ELP1..ELP36, from the table in the ReadMe of VI/79
RECORDS = [1024, 919, 705, 348, 317, 238, 15, 12, 9, 14329, 5234, 6632, 4385, 834, 1716, 171, 151, 115, 227, 189, 170,
           4, 3, 3, 7, 5, 6, 21, 13, 15, 12, 5, 11, 29, 14, 20]
POWER = {7: 1, 8: 1, 9: 1, 13: 1, 14: 1, 15: 1, 19: 1, 20: 1, 21: 1, 25: 1, 26: 1, 27: 1, 34: 2, 35: 2, 36: 2}   # x t(2) or x t(3) in the subroutine
NDENSE = 300
MAXT = 2.0                                                     # |T| <= 2 over 1800..2100 (1800 is T = -2.0)


def die(msg):
    sys.exit("make_elp_data.py: " + msg)


# ---------------------------------------------------------------- download
def fetch(cache, name, gz=False):
    path = os.path.join(cache, name)
    if not os.path.exists(path):
        url = BASE + name + (".gz" if gz else "")
        try:
            data = urllib.request.urlopen(url, timeout=180).read()
        except Exception as e:                                  # noqa: BLE001
            die("cannot download %s (%s)" % (url, e))
        if gz:
            data = gzip.decompress(data)
        with open(path, "wb") as f:
            f.write(data)
    return open(path, "rb").read().decode("ascii").replace("\r", "")


# ---------------------------------------------------------------- constants from the Fortran text
def read_constants(src):
    """Evaluate the numeric assignments of elp82b.f (names listed here) in order; returns a dict. Fortran 'd' exponents and w(i,j) indexing are translated."""
    names = {"cpi", "cpi2", "pis2", "rad", "deg", "c1", "c2", "ath", "a0", "am", "alfa", "dtasm", "preces", "delnu", "dele", "delg", "delnp", "delep",
             "p1", "p2", "p3", "p4", "p5", "q1", "q2", "q3", "q4", "q5"}
    arrays = {"w", "eart", "peri", "p"}
    ns = {"w": {}, "eart": {}, "peri": {}, "p": {}}
    count = 0
    for line in src.split("\n"):
        if line[:1] in "*Cc" or not line.startswith("         "):
            continue                                            # comments; the assignments are inside the 'if (ideb.eq.0)' block (9 spaces)
        m = re.match(r"^\s+(\w+)(\((\d)(?:,(\d))?\))?=(.*)$", line)
        if not m:
            continue
        name, idx1, idx2, expr = m.group(1), m.group(3), m.group(4), m.group(5)
        if name not in names and name not in arrays:
            continue
        e = re.sub(r"(?<=[\d.])d(?=[+-]?\d)", "e", expr.replace(" ", ""))
        e = re.sub(r"\b(w|eart|peri|p)\((\d)(?:,(\d))?\)", lambda mm: "%s[%s]" % (mm.group(1), mm.group(2) + ("," + mm.group(3) if mm.group(3) else "")), e)
        try:
            val = eval(e, {"__builtins__": {}}, ns)             # noqa: S307 - the expression is numeric text from the Fortran file read above
        except Exception as ex:                                 # noqa: BLE001
            die("cannot evaluate Fortran line '%s' (%s)" % (line.strip(), ex))
        if name in arrays:
            ns[name][(int(idx1), int(idx2)) if idx2 else int(idx1)] = val
        else:
            ns[name] = val
        count += 1
    need = len(names) + 15 + 5 + 5 + 16
    if count != need:
        die("read %d constants from elp82b.f, expected %d" % (count, need))
    return ns


class Const:
    def __init__(self, src):
        ns = read_constants(src)
        self.ns = ns
        w, eart, peri, p = ns["w"], ns["eart"], ns["peri"], ns["p"]
        self.W = [[w[(i, k)] for k in range(1, 6)] for i in (1, 2, 3)]
        self.EART = [eart[k] for k in range(1, 6)]
        self.PERI = [peri[k] for k in range(1, 6)]
        self.P = [[p[(i, 1)], p[(i, 2)]] for i in range(1, 9)]
        # Delaunay's arguments (the loop of the subroutine): D = w1-eart (+pi), l' = eart-peri, l = w1-w2, F = w1-w3, each a polynomial in T of 5 coefficients
        self.DEL = [[self.W[0][k] - self.EART[k] for k in range(5)], [self.EART[k] - self.PERI[k] for k in range(5)],
                    [self.W[0][k] - self.W[1][k] for k in range(5)], [self.W[0][k] - self.W[2][k] for k in range(5)]]
        self.DEL[0][0] += ns["cpi"]
        self.ZETA = [self.W[0][0], self.W[0][1] + ns["preces"]]


# ---------------------------------------------------------------- the series files
def parse_terms(cache, K):
    """All terms of ELP1..ELP36: list of (coord, tpow, basis, mult[13], phase_deg, amp)."""
    ns = K.ns
    terms = []
    for n in range(1, 37):
        text = fetch(cache, "ELP%d" % n).split("\n")
        lines = [l for l in text if l.strip()]
        if len(lines) != RECORDS[n - 1]:
            die("ELP%d has %d records, the ReadMe says %d" % (n, len(lines), RECORDS[n - 1]))
        coord = (n - 1) % 3                                     # iv = mod(ific-1,3)+1: 0 longitude, 1 latitude, 2 distance
        tpow = POWER.get(n, 0)
        for l in lines[1:]:
            if n <= 3:                                          # format (4i3,2x,f13.5,6(2x,f10.2))
                ilu = [int(l[3 * i:3 * i + 3]) for i in range(4)]
                c = [float(l[14:27])] + [float(l[27 + 12 * k + 2:27 + 12 * k + 12]) for k in range(6)]
                if n == 3:
                    c[0] = c[0] - 2.0 * c[0] * ns["delnu"] / 3.0
                tgv = c[1] + ns["dtasm"] * c[5]
                x = c[0] + tgv * (ns["delnp"] - ns["am"] * ns["delnu"]) + c[2] * ns["delg"] + c[3] * ns["dele"] + c[4] * ns["delep"]
                terms.append((coord, 0, 0, ilu + [0] * 9, 0.0, x))
            elif n <= 9 or n >= 22:                             # format (5i3,1x,f9.5,1x,f9.5,1x,f9.3): iz, ilu(4), phase (deg), amplitude, period
                iz = int(l[0:3])
                ilu = [int(l[3 + 3 * i:6 + 3 * i]) for i in range(4)]
                pha, x = float(l[16:25]), float(l[26:35])
                terms.append((coord, tpow, 1, ilu + [iz] + [0] * 8, pha, x))
            else:                                               # format (11i3,1x,f9.5,1x,f9.5,1x,f9.3)
                ip = [int(l[3 * i:3 * i + 3]) for i in range(11)]
                pha, x = float(l[34:43]), float(l[44:53])
                if n <= 15:                                     # table 1: 8 planets, then D, l, F
                    m = [ip[8], 0, ip[9], ip[10], 0] + ip[0:8]
                else:                                           # table 2: 7 planets (no Neptune), then D, l', l, F
                    m = [ip[7], ip[8], ip[9], ip[10], 0] + ip[0:7] + [0]
                terms.append((coord, tpow, 1, m, pha, x))
    return terms


# ---------------------------------------------------------------- evaluation
class Evaluator:
    """The subroutine's arithmetic for a list of dates (JD TDB): angles per date, term contributions, change of coordinates."""
    def __init__(self, K, jds):
        self.K, self.jds = K, jds
        d = K.ns
        self.T = [(jd - 2451545.0) / 36525.0 for jd in jds]
        self.angFull, self.angLin = [], []
        for T in self.T:
            tk = [T ** k for k in range(5)]
            full = [sum(K.DEL[i][k] * tk[k] for k in range(5)) for i in range(4)] + [0.0] * 9
            lin = [K.DEL[i][0] + K.DEL[i][1] * T for i in range(4)] + [K.ZETA[0] + K.ZETA[1] * T] + [K.P[i][0] + K.P[i][1] * T for i in range(8)]
            self.angFull.append(full)
            self.angLin.append(lin)
        self.d = d

    def contrib(self, term, di):
        coord, tpow, basis, m, pha, amp = term
        ang = (self.angLin if basis else self.angFull)[di]
        y = pha * self.d["deg"] if basis else 0.0
        for j in range(13):
            if m[j]:
                y += m[j] * ang[j]
        if not basis and coord == 2:
            y += self.d["pis2"]
        y = math.fmod(y, self.d["cpi2"])
        return amp * (self.T[di] ** tpow) * math.sin(y)

    def position(self, di, s):
        """s = [sum longitude (arcsec), sum latitude (arcsec), sum distance (km)] -> x, y, z in km (J2000 ecliptic)."""
        d, K, T = self.d, self.K, self.T[di]
        t = [T ** k for k in range(5)]
        r1 = s[0] / d["rad"] + sum(K.W[0][k] * t[k] for k in range(5))
        r2 = s[1] / d["rad"]
        r3 = s[2] * d["a0"] / d["ath"]
        x1 = r3 * math.cos(r2)
        x2 = x1 * math.sin(r1)
        x1 = x1 * math.cos(r1)
        x3 = r3 * math.sin(r2)
        pw = (d["p1"] + d["p2"] * t[1] + d["p3"] * t[2] + d["p4"] * t[3] + d["p5"] * t[4]) * t[1]
        qw = (d["q1"] + d["q2"] * t[1] + d["q3"] * t[2] + d["q4"] * t[3] + d["q5"] * t[4]) * t[1]
        ra = 2.0 * math.sqrt(1 - pw * pw - qw * qw)
        pwqw = 2.0 * pw * qw
        pw2 = 1 - 2.0 * pw * pw
        qw2 = 1 - 2.0 * qw * qw
        pw, qw = pw * ra, qw * ra
        return (pw2 * x1 + pwqw * x2 + pw * x3, pwqw * x1 + qw2 * x2 - qw * x3, -pw * x1 + qw * x2 + (pw2 + qw2 - 1) * x3)


def read_table():
    rows = []
    for l in open(TABLE).read().split("\n"):
        m = re.match(r"^\{(\d), ([\d.]+), ([^,]+), ([^,]+), ([^,]+), ([^,]+), ([^,]+), ([^}]+)\},$", l)
        if m:
            v = [float(g) for g in m.groups()]
            rows.append(v)
    if len(rows) != 170:
        die("%s: %d rows, expected 170 (run make_moon_ref.py first)" % (TABLE, len(rows)))
    return rows


def errors(ev, sums, rows):
    """Per row: (km, arcsec) of the ELP position (from the sums) against the Horizons Moon."""
    out = []
    for di, r in enumerate(rows):
        p = ev.position(di, [sums[0][di], sums[1][di], sums[2][di]])
        h = [r[2] / 1000.0, r[3] / 1000.0, r[4] / 1000.0]
        dk = math.sqrt(sum((p[i] - h[i]) ** 2 for i in range(3)))
        cx = [p[1] * h[2] - p[2] * h[1], p[2] * h[0] - p[0] * h[2], p[0] * h[1] - p[1] * h[0]]
        ang = math.atan2(math.sqrt(sum(c * c for c in cx)), sum(p[i] * h[i] for i in range(3)))
        out.append((dk, ang * 206264.80624709636))
    return out


def stats(errs):
    km = sorted(e[0] for e in errs)
    a = sorted(e[1] for e in errs)
    n = len(km)
    return dict(worst_km=km[-1], worst_as=a[-1], rms_km=math.sqrt(sum(x * x for x in km) / n), rms_as=math.sqrt(sum(x * x for x in a) / n),
                med_km=km[n // 2], med_as=a[n // 2])


def fmt(x):
    return repr(float(x))


def main():
    args = sys.argv[1:]
    cache = os.path.join(ROOT, "data", "elp_cache")
    ratio = 1.5
    if "--cache" in args:
        cache = args[args.index("--cache") + 1]
    if "--ratio" in args:
        ratio = float(args[args.index("--ratio") + 1])
    os.makedirs(cache, exist_ok=True)
    with open(os.path.join(cache, ".gitignore"), "w") as f:
        f.write("*\n")                                          # the downloaded files are not committed
    src = fetch(cache, "elp82b.f", gz=True)
    K = Const(src)
    terms = parse_terms(cache, K)
    print("read %d terms from ELP1..ELP36 (record counts equal the ReadMe's)" % len(terms))

    rows = read_table()
    N = len(rows)
    rnd = random.Random(20261003)                               # 300 seeded dates, 1800..2100, NOT in the table: where the truncated series is compared with the full one
    dense = [2378497.0 + rnd.random() * (2488070.0 - 2378497.0) for _ in range(NDENSE)]
    jds = [r[1] for r in rows] + dense                          # table rows first, so the row index is the date index
    M = len(jds)
    ev = Evaluator(K, jds)
    # contribution of every term at every table date (array of doubles per term)
    km_per_arcsec = K.ns["a0"] * math.pi / 648000.0
    size = []
    for t in terms:
        s = abs(t[5]) * MAXT ** t[1]
        size.append(s / km_per_arcsec if t[0] == 2 else s)     # arcsec (distance terms converted at the mean distance)
    order = sorted(range(len(terms)), key=lambda i: -size[i])  # descending size, ties by input order (sorted() is stable)
    terms = [terms[i] for i in order]
    size = [size[i] for i in order]

    # sweep from the smallest thresholds: first all terms (the full series), then drop the smallest by steps of sqrt(2) in size
    contrib = []
    for t in terms:
        contrib.append(array.array("d", [ev.contrib(t, di) for di in range(M)]))
    full_sums = [[0.0] * M for _ in range(3)]
    for t, c in zip(terms, contrib):
        row = full_sums[t[0]]
        for di in range(M):
            row[di] += c[di]
    full = stats(errors(ev, full_sums, rows))
    print("FULL series (%d terms): worst %.3f km (%.4f arcsec), rms %.3f km (%.4f arcsec), median %.3f km (%.4f arcsec)" % (
        len(terms), full["worst_km"], full["worst_as"], full["rms_km"], full["rms_as"], full["med_km"], full["med_as"]))
    for g, name in enumerate(["yearly 1800-2100", "daily 2025-12-02..2026-01-30", "hourly 2026-01-02..04"]):
        idx = [i for i, r in enumerate(rows) if int(r[0]) == g]
        e = errors(ev, full_sums, rows)
        print("   full, %-30s worst %.3f km (%.4f arcsec)" % (name, max(e[i][0] for i in idx), max(e[i][1] for i in idx)))

    # build cumulative sums in descending size and evaluate at thresholds, from the smallest threshold up
    thr, k = [], 0
    smallest = size[-1]
    x = smallest
    while x < size[0]:
        thr.append(x)
        x *= math.sqrt(2.0)
    # number of terms kept for threshold th: those with size >= th
    cum = [[0.0] * M for _ in range(3)]
    keep_counts = []
    prefix_stats = {}
    # evaluate prefix sums at the needed counts
    counts = sorted(set(sum(1 for s in size if s >= th) for th in thr))
    ci = 0
    for n_terms in range(1, len(terms) + 1):
        t, c = terms[n_terms - 1], contrib[n_terms - 1]
        row = cum[t[0]]
        for di in range(M):
            row[di] += c[di]
        if ci < len(counts) and n_terms == counts[ci]:
            prefix_stats[n_terms] = stats(errors(ev, cum, rows))
            dd = 0.0
            for di in range(N, M):                              # kept terms against ALL terms at the unseen dates
                a = ev.position(di, [full_sums[0][di], full_sums[1][di], full_sums[2][di]])
                b = ev.position(di, [cum[0][di], cum[1][di], cum[2][di]])
                dd = max(dd, math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(3))))
            prefix_stats[n_terms]["dense_km"] = dd
            ci += 1
    print("\n  size threshold (arcsec)   terms kept   worst km   worst arcsec   rms km   kept-vs-full at 300 unseen dates (km)   dropped-size bound (arcsec, sum of |A| x 2^p)")
    chosen = len(terms)
    chosen_table_only = len(terms)
    limit = ratio * full["worst_km"]
    dense_limit = (ratio - 1.0) * full["worst_km"]
    stopped = stopped_table = False
    rows_out = []
    for th in thr:
        n_terms = sum(1 for s in size if s >= th)
        st = prefix_stats[n_terms]
        bound = sum(size[n_terms:])
        rows_out.append((th, n_terms, st, bound))
    for th, n_terms, st, bound in rows_out:                     # thresholds ascending = most terms first
        print("  %12.3e %14d %12.3f %12.4f %10.3f %14.3f %24.4e" % (th, n_terms, st["worst_km"], st["worst_as"], st["rms_km"], st["dense_km"], bound))
        if not stopped_table:
            if st["worst_km"] <= limit:
                chosen_table_only = n_terms
            else:
                stopped_table = True
        if not stopped:
            if st["worst_km"] <= limit and st["dense_km"] <= dense_limit:
                chosen = n_terms
            else:
                stopped = True
    kept = terms[:chosen]
    st = prefix_stats.get(chosen) or full
    bound = sum(size[chosen:])
    print("\nRULE: worst error on the table within %.2f x the full %.3f km = %.3f km AND kept-vs-full at 300 unseen dates within %.3f km: keep %d of %d terms (size >= %.3e arcsec)" % (
        ratio, full["worst_km"], limit, dense_limit, chosen, len(terms), size[chosen - 1]))
    print("TRUNCATED (%d terms): worst %.3f km (%.4f arcsec), rms %.3f km (%.4f arcsec), median %.3f km (%.4f arcsec); dropped terms' size bound %.4e arcsec-equivalent" % (
        chosen, st["worst_km"], st["worst_as"], st["rms_km"], st["rms_as"], st["med_km"], st["med_as"], bound))
    by_coord = [sum(1 for t in kept if t[0] == c) for c in range(3)]
    print("kept per coordinate (longitude, latitude, distance): %s" % by_coord)

    print("the rule on the table alone would keep %d terms (kept-vs-full at the unseen dates there: %.3f km, above the %.3f km the second condition allows)" % (
        chosen_table_only, prefix_stats[chosen_table_only]["dense_km"], dense_limit))
    worst = st["dense_km"] if "dense_km" in st else 0.0
    print("truncated against full at %d seeded dates not in the table: worst difference %.3f km" % (NDENSE, worst))

    # write elp_data.inc
    with open(OUT, "w", newline="\n") as f:
        f.write("// GENERATED by native/tools/make_elp_data.py; do not edit. Included by src/orbit/moon_elp.cpp (which defines struct ElpTerm).\n")
        f.write("// Source: ELP2000-82B, M. Chapront-Touze and J. Chapront, Astron. Astrophys. 190, 342 (1988); CDS catalogue VI/79 (https://cdsarc.cds.unistra.fr/ftp/VI/79/): the files ELP1..ELP36 and\n")
        f.write("// elp82b.f. Every constant below is read from elp82b.f by the tool; the amplitudes of ELP1..3 already include the subroutine's corrections (fit to DE200/LE200).\n")
        f.write("// Terms: all %d terms of the 36 files, sorted by size, the %d largest kept. The rule (measured against the Horizons Moon table, tests/reference/moon_table.inc): the worst\n" % (len(terms), chosen))
        f.write("// error stays within %.2f x the full series' (%.3f km): full series worst %.3f km (%.4f arcsec), rms %.3f km; kept series worst %.3f km (%.4f arcsec), rms %.3f km.\n" % (
            ratio, full["worst_km"], full["worst_km"], full["worst_as"], full["rms_km"], st["worst_km"], st["worst_as"], st["rms_km"]))
        f.write("// Dropped terms: sizes (|amplitude| x the largest |T|^power over 1800..2100, in arcsec) below %.3e; their summed size is %.4e arcsec (about %.3f km). The kept series differs from\n" % (size[chosen - 1], bound, bound * km_per_arcsec))
        f.write("// the full series by at most %.3f km at 300 dates not in the table.\n" % worst)
        f.write("// Units: amplitudes in arcsec (longitude, latitude) and km (distance). Multipliers: D, l', l, F, zeta, Mercury, Venus, Earth, Mars, Jupiter, Saturn, Uranus, Neptune.\n")
        f.write("static const double kElpW[3][5] = {%s};   // W1..W3 (arcsec -> rad) polynomials in T\n" % ", ".join("{%s}" % ", ".join(fmt(v) for v in r) for r in K.W))
        f.write("static const double kElpEart[5] = {%s}, kElpPeri[5] = {%s};\n" % (", ".join(fmt(v) for v in K.EART), ", ".join(fmt(v) for v in K.PERI)))
        f.write("static const double kElpP[8][2] = {%s};   // planetary mean longitudes (Mercury .. Neptune), constant and rate\n" % ", ".join("{%s, %s}" % (fmt(a), fmt(b)) for a, b in K.P))
        scal = ["cpi", "rad", "deg", "ath", "a0", "preces", "p1", "p2", "p3", "p4", "p5", "q1", "q2", "q3", "q4", "q5"]
        f.write("static const double %s;\n" % ", ".join("kElp_%s = %s" % (n, fmt(K.ns[n])) for n in scal))
        f.write("static const int kElpTermCount = %d;\n" % chosen)
        f.write("static const ElpTerm kElpTerms[%d] = {\n" % chosen)
        for coord, tpow, basis, m, pha, amp in kept:
            f.write("{%d,%d,%d,{%s},%s,%s},\n" % (coord, tpow, basis, ",".join(str(v) for v in m), fmt(pha), fmt(amp)))
        f.write("};\n")
    print("wrote %s (%d bytes)" % (OUT, os.path.getsize(OUT)))

    # the Python port's values for the C++ port test
    ev3 = Evaluator(K, [2378497.0, 2400000.5, 2415020.5, 2430000.0, 2444239.5, 2451545.0, 2455197.5, 2459000.5, 2461042.0, 2461043.75, 2461044.0,
                        2462000.25, 2470000.0, 2480000.5, 2488070.0, 2451544.0, 2451547.3, 2441000.125, 2433282.5, 2458849.5, 2461100.0, 2475000.0, 2465000.7, 2390000.9])
    s3 = [[0.0] * len(ev3.jds) for _ in range(3)]
    for t in kept:
        for di in range(len(ev3.jds)):
            s3[t[0]][di] += ev3.contrib(t, di)
    with open(OUT_PYCHECK, "w", newline="\n") as f:
        f.write("// GENERATED by native/tools/make_elp_data.py; do not edit. The Python port's position of the Moon from the SAME %d terms as src/orbit/elp_data.inc (not Horizons),\n" % chosen)
        f.write("// to check the C++ port: { JD (TDB), x, y, z (m) }, geocentric, ecliptic and equinox of J2000.\n{\n")
        for di, jd in enumerate(ev3.jds):
            p = ev3.position(di, [s3[0][di], s3[1][di], s3[2][di]])
            f.write("{%s, %s, %s, %s},\n" % (fmt(jd), fmt(p[0] * 1000.0), fmt(p[1] * 1000.0), fmt(p[2] * 1000.0)))
        f.write("}\n")
    print("wrote %s" % OUT_PYCHECK)


if __name__ == "__main__":
    main()
