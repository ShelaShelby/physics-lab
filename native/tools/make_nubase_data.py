#!/usr/bin/env python3
"""Fetch NUBASE2020 and write the table the chart of nuclides (0.11 stage 2) takes its spins and its "what kind of half-life is this" flags from.

Python 3, standard library only. Run from anywhere:
    python3 native/tools/make_nubase_data.py fetch    (needs network: www-nds.iaea.org; writes native/data/nubase/nubase_4.mas20.txt)
    python3 native/tools/make_nubase_data.py build    (offline: reads that file and src/atom/atom_data.inc, writes src/atom/nubase_data.inc)

fetch : one plain GET of  https://www-nds.iaea.org/amdc/ame2020/nubase_4.mas20.txt  (AMDC / IAEA Nuclear Data Section). Saved exactly as received.
build : takes the GROUND STATES (the ZZZi column ends in 0; isomers, levels and resonances are left out) and writes, for each one except the free neutron (Z = 0, which is not a nuclide of an element):
        Z, N, the spin and parity text as NUBASE writes it (without its isospin part "T=..."), a one-letter kind of half-life, and two notes.
        Nothing is invented: a cell is exactly what the file says. The columns used are the ones the file's own header describes (column numbers 1-based):
          70-78 half-life ("stbl" = stable, "p-unst" = particle unstable, a trailing # = from systematics, a leading ~ > < as written), 79-80 its unit,
          82-88 its uncertainty (for "stbl" rows it holds a lower limit such as ">140Py"), 89-102 spin and parity (* directly measured, # from systematics, T= isospin),
          120-209 decay modes and intensities (IS = isotopic abundance).
        kind: S stable ("stbl"); M a plain half-life value; A an approximate one ("~..."); B only a limit (">..." or "<..."); E an estimate from systematics ("#");
              P "p-unst"; '-' no half-life field at all.
        lim  : for kind S, the text of the lower limit the file gives (empty if none); for kinds A and B, the half-life text as written with its unit.
        modes: for kind S only: the decay-mode entries of the file that carry a question mark ("A ?", "2B+ ?", "2B-=?"), joined with "; ". The file's header does not define the question mark.
"""
import hashlib
import os
import re
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                                     # native/
DATA = os.path.join(ROOT, "data", "nubase")
SRC = os.path.join(DATA, "nubase_4.mas20.txt")
LAB = os.path.join(ROOT, "src", "atom", "atom_data.inc")
OUT_INC = os.path.join(ROOT, "src", "atom", "nubase_data.inc")
URL = "https://www-nds.iaea.org/amdc/ame2020/nubase_4.mas20.txt"


def die(msg):
    sys.exit("make_nubase_data.py: " + msg)


def fetch():
    req = urllib.request.Request(URL, headers={"User-Agent": "PhysicsLab-native-data-tool"})
    with urllib.request.urlopen(req, timeout=120) as r:
        body = r.read()
        print("HTTP", r.status, r.headers.get("Content-Type"), "Last-Modified:", r.headers.get("Last-Modified"))
    os.makedirs(DATA, exist_ok=True)
    with open(SRC, "wb") as f:
        f.write(body)
    print("wrote", SRC, len(body), "bytes, %d lines, sha256 %s" % (body.count(b"\n"), hashlib.sha256(body).hexdigest()))


def lab_nuclides():
    t = open(LAB, encoding="utf-8").read()
    m = re.search(r"kNucLines\[118\] = \{(.*?)\};", t, re.S)
    out = set()
    for line in re.findall(r'"([^"]*)"', m.group(1)):
        z, rest = line.split("|")
        for e in rest.split(","):
            out.add((int(z), int(e.split(":")[0])))
    return out


def kind_of(T, lim_field):
    if T == "stbl":
        return "S"
    if T == "p-unst":
        return "P"
    if T == "":
        return "-"
    if "#" in T:
        return "E"
    if T[0] in "<>":
        return "B"
    if T[0] == "~":
        return "A"
    if re.fullmatch(r"[0-9.]+", T):
        return "M"
    die("half-life field not understood: %r" % T)


def build():
    rows = []
    with open(SRC, encoding="ascii") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("#") or not line.strip():
                continue
            zzzi = line[4:8]
            if zzzi[3] != "0":
                continue
            A, Z = int(line[0:3]), int(zzzi[:3])
            if Z == 0:
                continue                                         # the free neutron
            N = A - Z
            T, unit, dT = line[69:78].strip(), line[78:80].strip(), line[81:88].strip()
            jpi = line[88:102]
            if "T=" in jpi:
                jpi = jpi[: jpi.index("T=")]
            jpi = jpi.strip()
            k = kind_of(T, dT)
            lim = modes = ""
            if k == "S":
                lim = re.sub(r"^([<>~]?[0-9.]+)\s*([A-Za-z]+)$", r"\1 \2", dT)      # ">140Py" -> ">140 Py"
                modes = "; ".join(s.strip() for s in line[119:].split(";") if "?" in s)
            elif k in "AB":
                lim = (T + " " + unit).strip()
            rows.append((Z, N, jpi, k, lim, modes))
    rows.sort()
    if len({(r[0], r[1]) for r in rows}) != len(rows):
        die("a ground state appears twice")
    lab = lab_nuclides()
    got = {(r[0], r[1]) for r in rows}
    if got != lab:
        die("the ground states are not the lab's nuclides: %d only here, %d only in the lab" % (len(got - lab), len(lab - got)))
    for r in rows:
        for s in r[2:]:
            if '"' in s or "\\" in s:
                die("a character that needs escaping: %r" % (r,))
    with open(OUT_INC, "w", encoding="utf-8", newline="\n") as f:
        f.write("// Generated by native/tools/make_nubase_data.py from data/nubase/nubase_4.mas20.txt (NUBASE2020). Do not edit by hand.\n")
        f.write("// Citation, as the file gives it: F.G. Kondev, M. Wang, W.J. Huang, S. Naimi, and G. Audi, Chin. Phys. C45, 030001 (2021).\n")
        f.write("// %d ground states (the free neutron left out): Z, N, spin and parity as written, kind of half-life, lower limit or half-life text, decay modes marked with a question mark.\n" % len(rows))
        f.write("static const NubaseRow kNubase[] = {\n")
        for z, n, j, k, lim, modes in rows:
            f.write('    {%d, %d, "%s", \'%s\', "%s", "%s"},\n' % (z, n, j, k, lim, modes))
        f.write("};\n")
    kinds = {}
    for r in rows:
        kinds[r[3]] = kinds.get(r[3], 0) + 1
    print("wrote", OUT_INC, len(rows), "rows; kinds", dict(sorted(kinds.items())), "; spin blank", sum(1 for r in rows if not r[2]))


if __name__ == "__main__":
    if len(sys.argv) != 2 or sys.argv[1] not in ("fetch", "build"):
        die("usage: make_nubase_data.py fetch|build")
    fetch() if sys.argv[1] == "fetch" else build()
