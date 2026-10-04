#!/usr/bin/env python3
"""Fetch the recommended nuclear electric quadrupole moments the Atom Lab draws nucleus shapes from, check them, and write the file the app is built from.

Python 3, standard library only. Run from anywhere:
    python3 native/tools/make_quadrupole_data.py fetch    (needs network: www-nds.iaea.org; writes native/data/quadrupole/elec_mom_recomm.csv)
    python3 native/tools/make_quadrupole_data.py build    (offline: reads that CSV, writes native/src/atom/quadrupole_data.inc)

fetch : one plain GET of  https://www-nds.iaea.org/nuclearmoments/elec_mom_recomm.csv  (IAEA Nuclear Data Section). Saved exactly as received. The directory page around it
        (www-nds.iaea.org/nuclearmoments/) is behind a bot challenge no script can pass; this one static file is not (0.10.9 stage 2).
build : checks the file and writes src/atom/quadrupole_data.inc: ONE ROW PER GROUND-STATE ROW of the file (energy [keV] = 0). Isomer rows (energy other than 0, 556 of them) are NOT in the table: the lab
        draws ground-state nuclei, and a moment measured in an excited isomer says nothing about the ground state. Nothing is invented and nothing is repaired: a cell is exactly what the file says.

The columns (names as in the file's header): z, n.n+n.z, symbol, energy [keV], halflife, spin, electric quadrupole [b], method, description, nsr, journal, indc.
THE SECOND COLUMN IS THE MASS NUMBER A, NOT A NEUTRON COUNT (its header "n.n+n.z" is misleading). How that was established (this tool re-checks it on every build and stops if it ever fails):
  every ground-state row's (Z, A - Z) is a nuclide in the lab's NUBASE-derived table (atom_data.inc), 599 of 599; read as a neutron count, (Z, column) is a known nuclide for only 53 of 599;
  and the rows whose half-life says "stable" are the stable isotopes of the table (76 of 77 under A; the 77th, Eu-151, is in the table with a half-life of 1.45e26 s; 0 of 77 under N).
Value cells (units: barns, 1 b = 100 fm^2 exactly): a signed number "+0.0400(3)" (the digits in brackets are the uncertainty in the last digits), an UNSIGNED one "0.0132(3)" (the file gives no sign),
a tentative sign "(-)0.0333(5)", and a few that are not a single clean number ("x or y", "[x]", and five with typing errors such as "+0.70)15)"): those are kept in the table as rows that say so and are never used.
Spin cell: "3/2-" is a firm spin; "(3/2-)" a tentative one; "9(-)" a firm spin with a tentative parity; the rest ("3- or 4-", "(2)-" ...) is not used.
One defect of the file is handled by name (KNOWN_DEFECTS): K-41 appears twice as a ground state, once "stable" (+0.734(7) b: the stable isotope, as the table says) and once with a half-life of 181 ms and
-0.15(11) b, which cannot be K-41 (it is stable); the second row is left out.
"""
import csv
import hashlib
import math
import os
import re
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                                     # native/
DATA = os.path.join(ROOT, "data", "quadrupole")
CSV_PATH = os.path.join(DATA, "elec_mom_recomm.csv")
OUT_INC = os.path.join(ROOT, "src", "atom", "quadrupole_data.inc")
ATOM_DATA = os.path.join(ROOT, "src", "atom", "atom_data.inc")
URL = "https://www-nds.iaea.org/nuclearmoments/elec_mom_recomm.csv"
HEADER = ["z", "n.n+n.z", "symbol", "energy [keV]", "halflife", "spin", "electric quadrupole [b]", "method", "description", "nsr", "journal", "indc"]
KNOWN_DEFECTS = {("19", "41", "181 ms"): "a second ground-state row for K-41 whose half-life (181 ms) is not K-41's (stable): left out; the stable row is kept"}


def die(msg):
    sys.exit("make_quadrupole_data.py: " + msg)


def fetch():
    req = urllib.request.Request(URL, headers={"User-Agent": "PhysicsLab-native-data-tool"})
    with urllib.request.urlopen(req, timeout=120) as r:
        body = r.read()
        print("HTTP", r.status, r.headers.get("Content-Type"), "Last-Modified:", r.headers.get("Last-Modified"))
    os.makedirs(DATA, exist_ok=True)
    with open(CSV_PATH, "wb") as f:
        f.write(body)
    print("wrote", CSV_PATH, len(body), "bytes, %d lines, sha256 %s" % (body.count(b"\n"), hashlib.sha256(body).hexdigest()))


def lab_tables():
    s = open(ATOM_DATA, encoding="utf-8").read()
    syms = re.findall(r'"([^"]*)"', re.search(r"kSymbol\[119\] = \{([^}]*)\}", s).group(1))
    body = s[s.index("kNucLines[118]"):]
    body = body[:body.index("};")]
    tab = {}
    for m in re.finditer(r'"(\d+)\|([^"]*)"', body):
        tab[int(m.group(1))] = {int(e.split(":", 1)[0]): e.split(":", 1)[1] for e in m.group(2).split(",")}
    return syms, tab


RE_S = re.compile(r"^([+-])(\d+\.?\d*)\((\d+)\)$")
RE_U = re.compile(r"^(\d+\.?\d*)\((\d+)\)$")
RE_T = re.compile(r"^\(([+-])\)(\d+\.?\d*)\((\d+)\)$")
RE_SPIN = re.compile(r"^(\d+)(/2)?\s?(\([+-]\)|[+-])$")


def parse_value(v):
    """-> (kind, Q in barns, uncertainty in barns); kind S signed, U unsigned (no sign in the file), T tentative sign, X not a single clean number."""
    v = v.strip()
    for kind, rx in (("S", RE_S), ("T", RE_T), ("U", RE_U)):
        m = rx.match(v)
        if m:
            g = m.groups()
            sign, num, unc = (g[0], g[1], g[2]) if kind != "U" else ("+", g[0], g[1])
            dec = len(num.split(".")[1]) if "." in num else 0
            q = float(num) * (1 if sign == "+" else -1) if kind != "U" else float(num)
            return kind, q, int(unc) * 10.0 ** (-dec)
    return "X", float("nan"), float("nan")


def parse_spin(s):
    """-> (2 x spin, firm) ; firm spin only when the cell is one clean value (the parity may be tentative)."""
    m = RE_SPIN.match(s.strip())
    if not m:
        return -1, 0
    return (int(m.group(1)) if m.group(2) else 2 * int(m.group(1))), 1


def cstr(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def build():
    with open(CSV_PATH, newline="", encoding="ascii") as f:
        rd = csv.reader(f)
        head = next(rd)
        if head != HEADER:
            die("unexpected header: %r" % head)
        rows = list(rd)
    syms, tab = lab_tables()
    for ln, r in enumerate(rows, start=2):
        if len(r) != 12:
            die("line %d: %d cells" % (ln, len(r)))
    ground = [r for r in rows if r[3].strip() == "0"]
    isomer = len(rows) - len(ground)
    # the second column is A: every ground row must be a known nuclide as (Z, A - Z), and the symbol must be the element's
    asN = known = stab = stab_ok = 0
    for r in ground:
        z, a = int(r[0]), int(r[1])
        if syms[z] != r[2].strip():
            die("symbol %s does not match Z = %d" % (r[2], z))
        if (a - z) not in tab[z]:
            die("%s-%d is not a nuclide in the lab's table: the second column is not the mass number" % (r[2], a))
        known += 1
        if a in tab[z]:
            asN += 1
        if r[4].strip() == "stable":
            stab += 1
            stab_ok += tab[z].get(a - z) == "S"
    print("second column = mass number A: %d of %d ground rows are nuclides as (Z, A-Z); as (Z, column): %d; 'stable' rows on stable isotopes: %d of %d" % (known, len(ground), asN, stab_ok, stab))
    seen, out, dropped = {}, [], []
    for r in ground:
        key = (r[0], r[1], r[4].strip())
        if key in KNOWN_DEFECTS:
            dropped.append(key)
            continue
        z, a = int(r[0]), int(r[1])
        if (z, a) in seen:
            die("two ground rows for %s-%d that are not a known defect" % (r[2], a))
        seen[(z, a)] = 1
        kind, q, unc = parse_value(r[6])
        s2, firm = parse_spin(r[5])
        out.append((z, a, s2, firm, kind, q, unc, r[6].strip(), r[5].strip(), r[7].strip(), r[9].strip(), 1 if r[4].strip() == "stable" else 0))
    out.sort()

    def num(x):
        return "NAN" if math.isnan(x) else repr(x)

    lines = ["// GENERATED by native/tools/make_quadrupole_data.py from data/quadrupole/elec_mom_recomm.csv (IAEA Nuclear Data Section, see data/quadrupole/README.txt). Do not edit.",
             "// One row per GROUND-STATE row of the file (energy 0), sorted by Z then A: Z, A (the file's second column is the mass number), 2 x spin (-1: not one clean value), spin firm, value kind",
             "// (S signed, U unsigned: no sign in the file, T tentative sign, X not a single clean number), Q in barns and its uncertainty in barns (NAN for X), the value text as the file has it, the spin text as the file has it,",
             "// the method, the NSR key, 1 when the file's half-life says stable. The file has %d rows: %d ground states (%d here: %d left out as a known defect), %d isomer rows (not here)." % (len(rows), len(ground), len(out), len(dropped), isomer),
             "static const int kQuadFileRows = %d, kQuadIsomerRows = %d;" % (len(rows), isomer),
             "static const QuadRow kQuadRows[] = {"]
    for z, a, s2, firm, kind, q, unc, raw, spin, method, nsr, stable in out:
        lines.append("    {%d, %d, %d, %d, '%s', %d, %s, %s, %s, %s, %s, %s}," % (z, a, s2, firm, kind, stable, num(q), num(unc), cstr(raw), cstr(spin), cstr(method), cstr(nsr)))
    lines.append("};")
    with open(OUT_INC, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines) + "\n")
    kinds = {k: sum(1 for o in out if o[4] == k) for k in "SUTX"}
    print("wrote", OUT_INC, "%d rows (ground states of the file, %d left out): signed %d, unsigned %d, tentative sign %d, not clean %d; %d isomer rows not used" % (len(out), len(dropped), kinds["S"], kinds["U"], kinds["T"], kinds["X"], isomer))


if __name__ == "__main__":
    if len(sys.argv) != 2 or sys.argv[1] not in ("fetch", "build"):
        die("usage: make_quadrupole_data.py fetch | build")
    fetch() if sys.argv[1] == "fetch" else build()
