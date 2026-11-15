#!/usr/bin/env python3
"""0.23 stage 1: the MEASURED vibrations of the 15 viewer molecules, from the NIST CCCBDB pages saved in data/molecules/vib/cccbdb/ (NOT run by the app).

Reads each page's "Vibrational levels" table (mode number, symmetry, fundamental, harmonic, its reference, the measured IR intensity with its
uncertainty and reference, the description) and pairs each measured mode with the calculated mode(s) of src/molecule/vib_data.inc:
  - a measured mode of symmetry E or Pi is 2 calculated modes, T is 3, the rest 1 (the degeneracy the symmetry label means);
  - among the calculated sets of that size, the pairing that makes the scaled calculated wavenumbers (x 0.9704, CCCBDB's factor for B3LYP/aug-cc-pVDZ)
    closest to the measured fundamentals (sum of |ln ratio|), with a penalty when CCCBDB's description says stretch and the calculated mode mostly bends,
    or the other way round (the stretch share is computed here from the calculated vector and the bonds, as src/molecule/vibration.cpp does);
  - every pairing is printed for checking by eye, and src/molecule/vib_measured.inc is written.
Usage: python3 make_vib_measured.py <cccbdb dir> <vib_data.inc> <xyz dir> <out .inc>      (numpy, scipy)
  (0.54.4: the pages are in the private sources repo: <cccbdb dir> = $PL_SOURCES_DIR/data/molecules/vib/cccbdb, or leave PL_SOURCES_DIR set and give any folder name)
"""
import glob, html, os, re, sys
import numpy as np
from scipy.optimize import linear_sum_assignment

# 0.41: HCN's CN stretch: CCCBDB lists 2089 (fundamental) and 2041 (harmonic) from Herzberg 1945, but 2089 is the LIQUID Raman band; the gas fundamental is
# 2096.85 cm^-1 (infrared, rated A: Shimanouchi 1972, NIST Chemistry WebBook, saved as cccbdb/webbook_HCN_vib_2026-11-08.html). The harmonic 2041, below the
# fundamental (unusual for a stretch: anharmonicity puts the fundamental below the harmonic), is left out.
CORRECT = {("HCN", 2): {"fund": 2096.85, "harm": None, "ref": "1972Shim"}}
SCALES = {"C6H6": 0.9704}; SCALE_TZ = 0.9675     # CCCBDB B3LYP/aug-cc-pVDZ (benzene) and B3LYP/aug-cc-pVTZ (the rest)

def table(path):
    t = open(path, errors="replace").read()
    i = t.find("Vibrational levels"); j = t.find("vibrational zero-point energy", i)
    rows = []
    for r in re.findall(r"<tr[^>]*>(.*?)</tr>", t[i:j], re.S | re.I):
        c = [html.unescape(re.sub(r"<[^>]+>", "", x)).replace("\xa0", " ").strip() for x in re.findall(r"<t[dh][^>]*>(.*?)</t[dh]>", r, re.S | re.I)]
        if c and c[0].isdigit(): rows.append(c)
    return rows

def num(s):
    try: return float(s)
    except ValueError: return None

def parse_row(c):
    # columns: mode, symmetry, fundamental, harmonic, reference, intensity, unc., reference, comment, description (blank cells kept as "")
    c = c + [""] * (10 - len(c))
    return dict(mode=int(c[0]), sym=c[1], fund=num(c[2]), harm=num(c[3]), ref=c[4], ir=num(c[5]), irunc=num(c[6]), irref=c[7], comment=c[8], desc=c[9])

def degen(sym):
    s = sym.strip()
    return 3 if s.startswith("T") else 2 if s.startswith("E") or s.startswith("Π") else 1

def read_calc(inc):
    calc = {}
    for l in open(inc):
        m = re.match(r'\{"(\w+)", ([-\d.]+), ([-\d.e]+), \{(.*)\}\},', l)
        if m: calc.setdefault(m.group(1), []).append((float(m.group(2)), float(m.group(3)), np.array([float(v) for v in m.group(4).split(",")])))
    for k in calc: calc[k].sort(key=lambda x: x[0])
    return calc

def read_xyz(path):
    L = open(path).read().splitlines(); n = int(L[0])
    return [l.split()[0] for l in L[2:2 + n]], np.array([[float(v) for v in l.split()[1:4]] for l in L[2:2 + n]])

MASS = {"H": 1.00782503223, "C": 12.0, "N": 14.00307400443, "O": 15.99491461957, "Cl": 34.968852682}
RCUT = {("C", "H"): 1.2, ("H", "O"): 1.1, ("H", "N"): 1.1, ("Cl", "H"): 1.4, ("H", "H"): 0.8, ("C", "C"): 1.6, ("C", "O"): 1.5, ("C", "N"): 1.3, ("N", "N"): 1.2, ("O", "O"): 1.4}

def stretch_share(syms, x, L):
    n = len(syms); m = np.array([MASS[s] for s in syms]); d = L.reshape(n, 3) / np.sqrt(m)[:, None]
    S = B = 0.0
    for a in range(n):
        for b in range(a + 1, n):
            key = tuple(sorted((syms[a], syms[b])))
            r = np.linalg.norm(x[b] - x[a])
            if r > RCUT.get(key, 0): continue
            u = (x[b] - x[a]) / r; rel = d[b] - d[a]; par = rel @ u
            S += par * par; B += rel @ rel - par * par
    return S / (S + B) if S + B > 0 else 0

# ---- symmetry (convention-free): which of the operations below map the molecule onto itself (the app's frame is the principal axes), and each mode's character ----
OPS = {"i": -np.eye(3), "C2x": np.diag([1, -1, -1]), "C2y": np.diag([-1, 1, -1]), "C2z": np.diag([-1, -1, 1]),
       "s_yz": np.diag([-1, 1, 1]), "s_xz": np.diag([1, -1, 1]), "s_xy": np.diag([1, 1, -1])}
def rot(axis, ang):
    a = axis / np.linalg.norm(axis); K = np.array([[0, -a[2], a[1]], [a[2], 0, -a[0]], [-a[1], a[0], 0]])
    return np.eye(3) + np.sin(ang) * K + (1 - np.cos(ang)) * K @ K
def candidates(x):
    """the axis operations above, and (for the symmetric tops: benzene, NH3, CH4) rotations by 2 pi / n about each axis, two-fold axes through each atom
    (perpendicular to an axis) and mirror planes containing an axis and an atom"""
    c = dict(OPS)
    for ai, a in enumerate(np.eye(3)):
        for n in (3, 4, 6): c[f"C{n}_{ai}"] = rot(a, 2 * np.pi / n)
        for j, r in enumerate(x):
            p = r - (r @ a) * a
            if np.linalg.norm(p) < 1e-3: continue
            u = p / np.linalg.norm(p); nrm = np.cross(a, u)
            c[f"C2u_{ai}_{j}"] = rot(u, np.pi)
            c[f"sv_{ai}_{j}"] = np.eye(3) - 2 * np.outer(nrm, nrm)
            w = rot(a, np.pi / 6) @ u                        # (and the planes between two atoms, benzene's sigma_d)
            n2 = np.cross(a, w); c[f"sd_{ai}_{j}"] = np.eye(3) - 2 * np.outer(n2, n2); c[f"C2d_{ai}_{j}"] = rot(w, np.pi)
        for j, r in enumerate(x):                            # CH4: three-fold axes along the bonds
            if np.linalg.norm(r) > 1e-3: c[f"C3b_{j}"] = rot(r, 2 * np.pi / 3)
    return c
def symmetry_ops(syms, x):
    found = {}
    for name, R in candidates(x).items():
        y = x @ R.T; perm = []
        for a in range(len(syms)):
            d = [np.linalg.norm(y[a] - x[b]) if syms[b] == syms[a] else 9 for b in range(len(syms))]
            b = int(np.argmin(d))
            if d[b] > 1e-4: break
            perm.append(b)
        else: found[name] = (R, perm)
    return found
def character(L, n, R, perm):
    v = L.reshape(n, 3); w = np.zeros_like(v)
    for a in range(n): w[perm[a]] = R @ v[a]          # the moved vector field: atom a's arrow, turned, now sits on atom perm[a]
    return float((v * w).sum())
def calc_symmetry(syms, x, L, ops):
    n = len(syms); ch = {k: character(L, n, R, p) for k, (R, p) in ops.items()}
    total = all(c > 0.99 for c in ch.values())
    parity = (1 if ch["i"] > 0.99 else -1 if ch["i"] < -0.99 else 0) if "i" in ch else 0
    return total, parity
def label_symmetry(sym):
    s = sym.replace("+", "").strip()
    total = s in ("A1", "Ag", "A'", "Σg", "Σ", "A1g", "A")
    parity = 1 if "g" in s[1:] else -1 if "u" in s[1:] else 0
    return total, parity

def main():
    cdir, inc, xyzdir, out = sys.argv[1:5]
    if not glob.glob(os.path.join(cdir, "cccbdb_exp2x_*_2026-11-04.html")):   # the saved pages are in the private sources repo
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import pl_sources
        cdir = pl_sources.path("molecules/vib/cccbdb")   # PL_SOURCES_DIR=<sources checkout>; stops with a message when it is not set
    calc = read_calc(inc)
    lines = []
    for path in sorted(glob.glob(os.path.join(cdir, "cccbdb_exp2x_*_2026-11-04.html"))):
        mid = os.path.basename(path).split("_")[2]
        meas = [parse_row(c) for c in table(path)]
        for r in meas:                                           # corrections, each with its saved source (README.txt)
            if (mid, r["mode"]) in CORRECT: r.update(CORRECT[(mid, r["mode"])])
        cm = calc.get(mid, [])
        SCALE = SCALES.get(mid, SCALE_TZ)
        if not cm: print(mid + ": no calculated modes, skipped"); continue
        syms, x = read_xyz(os.path.join(xyzdir, mid + ".xyz"))
        # the calculated degenerate sets (to 1 cm^-1, as vibration.cpp)
        sets = []
        for k, (nu, A, L) in enumerate(cm):
            if sets and abs(cm[sets[-1][0]][0] - nu) < 1.0: sets[-1].append(k)
            else: sets.append([k])
        ops = symmetry_ops(syms, x)
        pairs = []
        for g in (1, 2, 3):
            M = [r for r in meas if degen(r["sym"]) == g]
            C = [s for s in sets if len(s) == g]
            if len(M) != len(C): raise SystemExit(f"{mid}: {len(M)} measured modes of degeneracy {g}, {len(C)} calculated sets")
            if not M: continue
            cost = np.zeros((len(M), len(C)))
            for i, r in enumerate(M):
                for j, s in enumerate(C):
                    nu = cm[s[0]][0]
                    cost[i, j] = abs(np.log(SCALE * abs(nu) / r["fund"]))
                    if g == 1:                                   # symmetry must agree (degenerate sets are never totally symmetric; their parity is checked below)
                        ct, cp = calc_symmetry(syms, x, cm[s[0]][2], ops); lt, lp = label_symmetry(r["sym"])
                        if ct != lt or (cp and lp and cp != lp): cost[i, j] += 10
                    elif "i" in ops:
                        lt, lp = label_symmetry(r["sym"]); _, cp = calc_symmetry(syms, x, cm[s[0]][2], ops)
                        if cp and lp and cp != lp: cost[i, j] += 10
                    sh = stretch_share(syms, x, cm[s[0]][2]); d = r["desc"].lower()
                    if re.search(r"str", d) and sh < 0.5: cost[i, j] += 0.15
                    if re.search(r"bend|deform|sciss|wag|rock|twist|torsion", d) and sh > 0.5: cost[i, j] += 0.15
            ri, ci = linear_sum_assignment(cost)
            for i, j in zip(ri, ci):
                if cost[i, j] >= 10: raise SystemExit(f"{mid}: no pairing agrees with the symmetry for measured mode {M[i]['mode']} {M[i]['sym']}")
                pairs.append((M[i], C[j]))
        pairs.sort(key=lambda p: p[0]["mode"])
        for r, s in pairs:
            nu = cm[s[0]][0]
            print(f"{mid:6s} nu{r['mode']:<2d} {r['sym']:4s} measured {r['fund']:7.1f}  calc {nu:8.1f} x{SCALE} = {SCALE * nu:7.1f} ({100 * (SCALE * nu / r['fund'] - 1):+5.1f} %)  modes {s}  {r['desc']}  IR {r['ir']}")
            def q(v): return "NAN" if v is None else repr(v)
            esc = lambda v: v.replace("\\", "\\\\").replace('"', '\\"')
            lines.append(f'{{"{mid}", {r["mode"]}, "{esc(r["sym"])}", {q(r["fund"])}, {q(r["harm"])}, "{r["ref"]}", {q(r["ir"])}, {q(r["irunc"])}, "{esc(r["irref"])}", "{esc(r["desc"])}", {s[0]}, {len(s)}}},')
    with open(out, "w") as fo:
        fo.write("// Made by tools/make_vib_measured.py from the NIST CCCBDB pages saved in data/molecules/vib/cccbdb/ (read 4 Nov 2026). MEASURED values, as CCCBDB lists them,\n")
        fo.write("// except the corrections in CORRECT of the tool (HCN's CN stretch: Shimanouchi 1972, NIST WebBook, the gas band; CCCBDB gives the liquid's).\n")
        fo.write("// id, CCCBDB mode number, symmetry, fundamental (cm^-1), harmonic (cm^-1, NAN: not listed), reference, IR intensity (km/mol, NAN: not listed), its uncertainty, its reference,\n")
        fo.write("// description, the first calculated mode it is paired with (index in vib::modes), how many (the degeneracy)\n")
        fo.write("\n".join(lines) + "\n")
    print("wrote", out, len(lines), "measured modes")

if __name__ == "__main__":
    main()
