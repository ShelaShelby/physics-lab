#!/usr/bin/env python3
"""Build the Atom Lab's many-electron tables (0.39) from the files in native/data/hf/, as received. Python 3, standard library only, offline:
    python3 native/tools/make_hf_data.py        (reads native/data/hf/*, writes native/src/atom/hf_data.inc)

Sources (downloaded 6 Nov 2026, kept unchanged in native/data/hf/; README.txt there says the same):
  koga/light_neutral, koga/light_cation : T. Koga, K. Kanayama, S. Watanabe and A. J. Thakkar, "Analytical Hartree-Fock wave functions subject to cusp and asymptotic
                                          constraints: He to Xe, Li+ to Cs+, H- to I-", Int. J. Quantum Chem. 71, 491-497 (1999). File k99light.zip from
                                          https://ajit.ext.unb.ca/download.htm (A. J. Thakkar's research data page).
  koga/heavy_neutral                    : T. Koga, K. Kanayama, T. Watanabe, T. Imai and A. J. Thakkar, "Analytical Hartree-Fock wave functions for the atoms Cs to Lr",
                                          Theor. Chem. Acc. 104, 411-413 (2000). File k00heavy.zip from the same page.
  nist_ie_all.tsv                       : NIST Atomic Spectra Database, ver. 5.12 (Kramida, Ralchenko, Reader and NIST ASD Team, 2024, doi:10.18434/T4W30F), ionization energies of
                                          every spectrum H to Og (ground shells, ground level, ionization energy with its NIST marks, uncertainty, references), query:
                                          https://physics.nist.gov/cgi-bin/ASD/ie.pl?spectra=H-Og&units=1&format=3&order=0&at_num_out=on&sp_name_out=on&ion_charge_out=on&el_name_out=on
                                          &seq_out=on&shells_out=on&level_out=on&ion_conf_out=on&e_out=0&unc_out=on&biblio=on&submit=Retrieve+Data

Each Koga orbital is a sum of NORMALISED Slater-type functions c_k N_k r^(n_k - 1) exp(-zeta_k r), N_k = (2 zeta_k)^(n_k + 1/2) / sqrt((2 n_k)!) (atomic units). That convention is
not assumed blindly: the build checks every orbital's norm (1 within 2e-6, the 7 digits of the coefficients) and the orthogonality of orbitals of one l (within 2e-6), and stops otherwise.
The occupations come from each file's first line (e.g. "K(2)L(8)3S(2)3P(6)4S(2)3D(6), 5D"); they must add up to the number of electrons.
"""
import math
import os
import re
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pl_sources   # the Koga et al. files are in the private sources repo (PL_SOURCES_DIR)

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
DATA = os.path.join(ROOT, "data", "hf")
OUT = os.path.join(ROOT, "src", "atom", "hf_data.inc")
LETTERS = "SPDF"
CORES = {"K": "1S(2)", "L": "2S(2)2P(6)", "M": "3S(2)3P(6)3D(10)", "[HE]": "1S(2)", "[NE]": "1S(2)2S(2)2P(6)",
         "[AR]": "1S(2)2S(2)2P(6)3S(2)3P(6)", "[KR]": "1S(2)2S(2)2P(6)3S(2)3P(6)3D(10)4S(2)4P(6)",
         "[XE]": "1S(2)2S(2)2P(6)3S(2)3P(6)3D(10)4S(2)4P(6)4D(10)5S(2)5P(6)",
         "[RN]": "1S(2)2S(2)2P(6)3S(2)3P(6)3D(10)4S(2)4P(6)4D(10)5S(2)5P(6)4F(14)5D(10)6S(2)6P(6)"}
SYMBOLS = None


def symbols():
    """The element symbols from the lab's own table (atom_data.inc), not typed again here."""
    txt = open(os.path.join(ROOT, "src", "atom", "atom_data.inc"), encoding="utf-8").read()
    m = re.search(r"kSymbol\[119\]\s*=\s*\{(.*?)\};", txt, re.S)
    syms = re.findall(r'"([A-Za-z]*)"', m.group(1)) if m else []
    out = {z: s for z, s in enumerate(syms) if z > 0}
    if len(out) < 118:
        sys.exit(f"could not read 118 symbols from atom_data.inc (got {len(out)})")
    return out


def parse_koga(path):
    lines = open(path, encoding="latin-1").read().splitlines()
    head = lines[0].strip()
    m = re.match(r"([A-Z]+\+?)\s+(.+?),\s*(\S+)\s*$", head)
    if not m:
        sys.exit(f"{path}: unexpected first line {head!r}")
    name, conf, term = m.groups()
    occ = {}
    rest = conf
    # expand K(2) L(8) M(18) and [XE] style cores
    rest = re.sub(r"\b([KLM])\((\d+)\)", lambda q: CORES[q.group(1)], rest)
    for core in ("[HE]", "[NE]", "[AR]", "[KR]", "[XE]", "[RN]"):
        rest = rest.replace(core, CORES[core])
    for nl, k in re.findall(r"(\d[SPDF])\((\d+)\)", rest):
        occ[nl] = occ.get(nl, 0) + int(k)
    energy = float(re.search(r"\bE =\s*(-?[\d.]+)", "\n".join(lines)).group(1))
    orbs = {}
    i = 0
    while i < len(lines):
        mm = re.match(r"\s+([SPDF])\s+((?:\d[SPDF]\s*)+)$", lines[i])
        if mm:
            names = mm.group(2).split()
            eps = [float(x) for x in lines[i + 1].split()[1:]]
            i += 2                                               # the orbital energies
            if i < len(lines) and "CUSP" in lines[i]:
                i += 1                                           # (the light files give the cusp ratio of each orbital; the heavy ones do not)
            basis = []
            while i < len(lines) and re.match(r"\s+\d[SPDF]\s+[\d.]+", lines[i]):
                p = lines[i].split()
                basis.append((int(p[0][0]), float(p[1]), [float(x) for x in p[2:]]))
                i += 1
            for k, nm in enumerate(names):
                orbs[nm] = (eps[k], [(n, z, c[k]) for n, z, c in basis])
            continue
        i += 1
    return dict(name=name, conf=conf, term=term, occ=occ, energy=energy, orbs=orbs)


def norm(n, z):
    return (2 * z) ** (n + 0.5) / math.sqrt(math.factorial(2 * n))


def overlap(a, b):
    s = 0.0
    for n1, z1, c1 in a:
        for n2, z2, c2 in b:
            k = n1 + n2
            s += c1 * c2 * norm(n1, z1) * norm(n2, z2) * math.factorial(k) / (z1 + z2) ** (k + 1)
    return s


def koga_files():
    out = []
    for sub, charge in (("light_neutral", 0), ("light_cation", 1), ("heavy_neutral", 0)):
        d = pl_sources.path(os.path.join("hf", "koga", sub))
        for f in sorted(os.listdir(d)):
            out.append((os.path.join(d, f), charge, f))
    return out


def main():
    syms = symbols()
    by_sym = {s.upper(): z for z, s in syms.items()}
    atoms = []
    for path, charge, fname in koga_files():
        a = parse_koga(path)
        stem = re.sub(r"\.cat$", "", re.sub(r"^\d+", "", fname)).upper()
        Z = by_sym.get(stem)
        if Z is None:
            sys.exit(f"{path}: unknown element {stem}")
        a["Z"], a["charge"] = Z, charge
        ne = sum(a["occ"].values())
        if ne != Z - charge:
            sys.exit(f"{path}: occupations add up to {ne}, not {Z - charge} ({a['conf']})")
        for nl, k in a["occ"].items():
            if k and nl not in a["orbs"]:
                sys.exit(f"{path}: {nl} is occupied but has no orbital in the file")
        for nl, (eps, b) in a["orbs"].items():
            if abs(overlap(b, b) - 1) > 2e-6:
                sys.exit(f"{path}: {nl} norm {overlap(b, b)}")
        for l in LETTERS:
            same = [nl for nl in a["orbs"] if nl[1] == l]
            for i in range(len(same)):
                for j in range(i + 1, len(same)):
                    o = overlap(a["orbs"][same[i]][1], a["orbs"][same[j]][1])
                    if abs(o) > 2e-6:
                        sys.exit(f"{path}: {same[i]}.{same[j]} overlap {o}")
        atoms.append(a)
    atoms.sort(key=lambda a: (a["Z"], a["charge"]))
    seen = {(a["Z"], a["charge"]) for a in atoms}
    if not all((z, 0) in seen for z in range(2, 104)):
        sys.exit("a neutral atom He..Lr is missing: " + str([z for z in range(2, 104) if (z, 0) not in seen]))

    # NIST: the neutral atom and the 1+ ion of every element, as given
    nist = {}
    rows = open(os.path.join(DATA, "nist_ie_all.tsv"), encoding="utf-8").read().splitlines()
    for ln in rows[1:]:
        c = [x.strip().strip('"') for x in ln.split("\t")]
        if len(c) < 13 or not c[0].isdigit():
            continue
        z, q = int(c[0]), int(c[2].replace("+", ""))
        if q in (0, 1):
            nist[(z, q)] = dict(sp=c[1], shells=c[5], level=c[6], prefix=c[8], ie=c[9], suffix=c[10], unc=c[11], refs=c[12])
    if len([k for k in nist if k[1] == 0]) < 100:
        sys.exit("fewer than 100 neutral atoms read from nist_ie_all.tsv")

    def cstr(s):
        return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'

    out = ["// GENERATED by native/tools/make_hf_data.py from native/data/hf/ (sources: see that script and data/hf/README.txt). Do not edit by hand.",
           "// Koga et al. analytical Hartree-Fock orbitals (Int. J. Quantum Chem. 71, 491 (1999); Theor. Chem. Acc. 104, 411 (2000)): normalised Slater-type functions",
           "// c N r^(n-1) e^(-zeta r), atomic units; NIST ASD ver. 5.12 ground shells, ground levels and ionization energies (doi:10.18434/T4W30F).", ""]
    sto, orbrec, atomrec = [], [], []
    for a in atoms:
        first = len(orbrec)
        order = sorted(a["orbs"], key=lambda nl: (LETTERS.index(nl[1]), int(nl[0])))
        for nl in order:
            eps, b = a["orbs"][nl]
            s0 = len(sto)
            for n, z, c in b:
                sto.append(f"{{{n}, {z!r}, {c!r}}}")
            orbrec.append(f"{{{int(nl[0])}, {LETTERS.index(nl[1])}, {a['occ'].get(nl, 0)}, {eps!r}, {s0}, {len(b)}}}")
        atomrec.append(f"{{{a['Z']}, {a['charge']}, {cstr(a['conf'])}, {cstr(a['term'])}, {a['energy']!r}, {first}, {len(orbrec) - first}}}")
    out.append(f"static const HfSto kHfSto[{len(sto)}] = {{")
    out += ["    " + ", ".join(sto[i:i + 4]) + "," for i in range(0, len(sto), 4)]
    out.append("};")
    out.append(f"static const HfOrbitalRec kHfOrb[{len(orbrec)}] = {{")
    out += ["    " + r + "," for r in orbrec]
    out.append("};")
    out.append(f"static const HfAtomRec kHfAtom[{len(atomrec)}] = {{")
    out += ["    " + r + "," for r in atomrec]
    out.append("};")
    keys = sorted(nist)
    out.append(f"static const NistIeRec kNistIe[{len(keys)}] = {{")
    for k in keys:
        r = nist[k]
        out.append(f"    {{{k[0]}, {k[1]}, {cstr(r['sp'])}, {cstr(r['shells'])}, {cstr(r['level'])}, {cstr(r['prefix'])}, {cstr(r['ie'])}, {cstr(r['unc'])}, {cstr(r['refs'])}}},")
    out.append("};")
    open(OUT, "w", encoding="utf-8").write("\n".join(out) + "\n")
    print(f"{len(atoms)} Hartree-Fock atoms and ions ({sum(1 for a in atoms if a['charge'] == 0)} neutral), {len(orbrec)} orbitals, {len(sto)} basis functions; "
          f"{len(keys)} NIST rows -> {os.path.relpath(OUT, ROOT)}")


if __name__ == "__main__":
    main()
