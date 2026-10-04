#!/usr/bin/env python3
"""Fetch the NIST ASD spectral lines the Atom Lab compares with, check them, and write the files the app is built from.

Python 3, standard library only. Run from anywhere:
    python3 native/tools/make_asd_data.py fetch    (needs network: physics.nist.gov; writes native/data/asd/*)
    python3 native/tools/make_asd_data.py build    (offline: reads native/data/asd/asd_lines.tsv and manifest.tsv, writes native/src/atom/asd_data.inc)

fetch : for every element Z = 1 .. 118 asks ASD's lines form (https://physics.nist.gov/cgi-bin/ASD/lines1.pl) twice:
          (a) the hydrogen-like ion of the element: "<symbol> <roman Z>" (H I, He II, ... Fe XXVI): ONE electron round a nucleus of charge Z, which is what the lab models
          (b) the neutral atom: "<symbol> I"
        and keeps a stated selection (SELECT_ION, SELECT_ATOM below; README.txt in data/asd says the same in words). The query is the one 0.10.6 stage 6 used for H I and He II
        (tests/test_atom.cpp, testSpectrumIndependent): format 3 (tab-delimited), VACUUM wavelengths (show_av = 3), observed and Ritz wavelengths, allowed and forbidden lines.
        Nothing is invented: a cell is exactly what ASD returned (without its quotation marks); a spectrum ASD does not have ("Input Error") or that has no line in the range is recorded as such in manifest.tsv.
build : checks the two data files and writes src/atom/asd_data.inc (C++ tables, wavelengths with the digits ASD gave).
"""
import math
import os
import re
import sys
import time
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                                     # native/
DATA = os.path.join(ROOT, "data", "asd")
LINES = os.path.join(DATA, "asd_lines.tsv")
MANIFEST = os.path.join(DATA, "manifest.tsv")
OUT_INC = os.path.join(ROOT, "src", "atom", "asd_data.inc")
ATOM_DATA = os.path.join(ROOT, "src", "atom", "atom_data.inc")    # the element symbols come from the lab's own table, not typed again here

URL = "https://physics.nist.gov/cgi-bin/ASD/lines1.pl"
LOW_NM_ION, UPP_NM_ION = 0.0001, 10000.0                         # wavelength range asked for the hydrogen-like ion (vacuum, nm): covers Lyman-alpha of the heaviest ion to Pfund-alpha of hydrogen
LOW_NM_ATOM, UPP_NM_ATOM = 100.0, 2500.0                         # the neutral atom: vacuum ultraviolet to the near infrared
# What is kept (the same words are in data/asd/README.txt and in About):
SELECT_ION = "lines of the hydrogen-like ion whose two levels both have principal quantum number n <= 6 (fine-structure components kept separately)"
ATOM_KEEP = 30
SELECT_ATOM = ("the %d strongest OBSERVED lines of the neutral atom in 100-2500 nm (vacuum) that ASD gives a numeric relative intensity for, ranked by that intensity (ASD says its relative intensities "
               "are qualitative, have no common scale and mean something only within one spectrum); fewer when ASD lists fewer" % ATOM_KEEP)
COLS = ["obs_wl_vac(nm)", "unc_obs_wl", "ritz_wl_vac(nm)", "unc_ritz_wl", "intens", "Aki(s^-1)", "Acc", "Ei(cm-1)", "Ek(cm-1)",
        "conf_i", "term_i", "J_i", "conf_k", "term_k", "J_k", "Type", "tp_ref", "line_ref"]


def die(msg):
    sys.exit("make_asd_data.py: " + msg)


def symbols():
    s = open(ATOM_DATA, encoding="utf-8").read()
    m = re.search(r"kSymbol\[119\] = \{([^}]*)\}", s)
    syms = re.findall(r'"([^"]*)"', m.group(1))
    if len(syms) != 119:
        die("could not read the 119 symbols from atom_data.inc")
    return syms


def roman(n):
    out = ""
    for v, r in ((100, "C"), (90, "XC"), (50, "L"), (40, "XL"), (10, "X"), (9, "IX"), (5, "V"), (4, "IV"), (1, "I")):
        while n >= v:
            out += r
            n -= v
    return out


def query(spectrum, low, upp):
    q = {"spectra": spectrum, "low_w": low, "upp_w": upp, "unit": 1, "format": 3, "line_out": 0, "en_unit": 0, "output": 0, "bibrefs": 1, "page_size": 15,
         "show_obs_wl": 1, "show_calc_wl": 1, "show_av": 3, "unc_out": 1, "order_out": 0, "max_low_enrg": "", "tsb_value": 0, "min_str": "", "A_out": 0, "intens_out": "on",
         "max_str": "", "allowed_out": 1, "forbid_out": 1, "min_accur": "", "min_intens": "", "conf_out": "on", "term_out": "on", "enrg_out": "on", "J_out": "on", "submit": "Retrieve Data"}
    url = URL + "?" + urllib.parse.urlencode(q)
    for attempt in range(4):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "PhysicsLab-native-data-tool/0.10.7 (one-off fetch of ASD lines; github.com/ShelaShelby/physics-lab)"})
            with urllib.request.urlopen(req, timeout=120) as r:
                return r.read().decode("latin-1")
        except Exception as e:                                   # network hiccup: wait and retry; give up loudly
            if attempt == 3:
                die("request failed for %s: %s" % (spectrum, e))
            time.sleep(2 ** (attempt + 1))


def parse(text):
    """ASD's tab-delimited reply -> (status, rows). status: 'ok', 'none' (ASD has no such spectrum), 'empty' (no line in the range). rows: dicts keyed by COLS."""
    if "Input Error" in text[:600] or text.lstrip().startswith("<!DOCTYPE"):
        return "none", []
    lines = [l for l in text.replace("\r", "").split("\n") if l.strip()]
    if not lines or "ritz_wl_vac(nm)" not in lines[0]:
        return "empty", []
    head = [h.strip() for h in lines[0].split("\t")]
    rows = []
    for l in lines[1:]:
        cells = [c.strip().strip('"') for c in l.split("\t")]
        if len(cells) < len(head) - 1:
            continue
        row = {c: "" for c in COLS}
        for h, c in zip(head, cells):
            if h in row:
                row[h] = c
        rows.append(row)
    return ("ok" if rows else "empty"), rows


def level_n(conf):
    m = re.match(r"^(\d+)", conf)
    return int(m.group(1)) if m else None


def num(s):
    m = re.match(r"^\s*[\[\(]?([-+]?\d+\.?\d*(?:[eE][-+]?\d+)?)", s)
    return float(m.group(1)) if m else None


def wavelength(row):
    """the wavelength of a line: the observed one when ASD gives it, else the Ritz one (as the lab's comparison does)"""
    o = num(row["obs_wl_vac(nm)"])
    return (o, True) if o is not None else (num(row["ritz_wl_vac(nm)"]), False)


def select_ion(rows):
    out = []
    for r in rows:
        ni, nk = level_n(r["conf_i"]), level_n(r["conf_k"])
        if ni is None or nk is None or ni > 6 or nk > 6:
            continue
        w, _ = wavelength(r)
        if w is None or w <= 0:
            continue
        out.append(r)
    return out


def select_atom(rows):
    cand = []
    for i, r in enumerate(rows):
        if num(r["obs_wl_vac(nm)"]) is None:
            continue
        inten = num(r["intens"])
        if inten is None or inten <= 0:
            continue
        cand.append((-inten, num(r["obs_wl_vac(nm)"]), i, r))   # strongest first; ties by wavelength, then ASD's own order
    cand.sort(key=lambda t: (t[0], t[1], t[2]))
    keep = [t[3] for t in cand[:ATOM_KEEP]]
    keep.sort(key=lambda r: num(r["obs_wl_vac(nm)"]))
    return keep


def fetch():
    syms = symbols()
    os.makedirs(DATA, exist_ok=True)
    cache = os.environ.get("ASD_CACHE")                            # optional: keep ASD's raw replies here (not in the repo)
    if cache:
        os.makedirs(cache, exist_ok=True)
    when = time.strftime("%Y-%m-%d", time.gmtime())
    with open(LINES, "w", encoding="utf-8", newline="\n") as fl, open(MANIFEST, "w", encoding="utf-8", newline="\n") as fm:
        fl.write("# NIST Atomic Spectra Database lines form, retrieved %s (UTC) by native/tools/make_asd_data.py; see README.txt. Cells are ASD's own text, quotation marks removed.\n" % when)
        fl.write("Z\tkind\tspectrum\t" + "\t".join(COLS) + "\n")
        fm.write("# Z\tkind\tspectrum\tstatus\tASD lines returned in the range\tlines kept\n")
        for Z in range(1, 119):
            for kind, spec, lo, hi in (("ion", "%s %s" % (syms[Z], roman(Z)), LOW_NM_ION, UPP_NM_ION), ("atom", "%s I" % syms[Z], LOW_NM_ATOM, UPP_NM_ATOM)):
                text = None
                cpath = os.path.join(cache, "%03d_%s.txt" % (Z, kind)) if cache else None
                if cpath and os.path.exists(cpath):
                    text = open(cpath, encoding="latin-1").read()
                if text is None:
                    text = query(spec, lo, hi)
                    if cpath:
                        open(cpath, "w", encoding="latin-1", newline="").write(text)
                    time.sleep(0.4)                                # be gentle with NIST's server
                status, rows = parse(text)
                keep = select_ion(rows) if kind == "ion" else select_atom(rows)
                if status == "ok" and not keep:
                    status = "empty"
                for r in keep:
                    fl.write("%d\t%s\t%s\t%s\n" % (Z, kind, spec, "\t".join(r[c] for c in COLS)))
                fm.write("%d\t%s\t%s\t%s\t%d\t%d\n" % (Z, kind, spec, status, len(rows), len(keep)))
                print("%3d %-5s %-9s %-6s returned %5d kept %3d" % (Z, kind, spec, status, len(rows), len(keep)), flush=True)


def c_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def build():
    man, rows = {}, {}
    for l in open(MANIFEST, encoding="utf-8"):
        if l.startswith("#") or not l.strip():
            continue
        z, kind, spec, status, ret, kept = l.rstrip("\n").split("\t")
        man[(int(z), kind)] = (spec, status, int(ret), int(kept))
    if len(man) != 236:
        die("manifest.tsv should have 236 entries (118 elements x ion, atom)")
    with open(LINES, encoding="utf-8") as f:
        f.readline()
        head = f.readline().rstrip("\n").split("\t")
        if head != ["Z", "kind", "spectrum"] + COLS:
            die("asd_lines.tsv header is not what was expected")
        for l in f:
            c = l.rstrip("\n").split("\t")
            if len(c) != len(head):
                die("a line of asd_lines.tsv has %d cells, not %d: %r" % (len(c), len(head), l[:80]))
            rows.setdefault((int(c[0]), c[1]), []).append(dict(zip(head, c)))
    for k, (spec, status, ret, kept) in man.items():
        if len(rows.get(k, [])) != kept:
            die("%s: manifest says %d kept lines, the data file has %d" % (spec, kept, len(rows.get(k, []))))
    allrows = []
    out = ["// GENERATED by native/tools/make_asd_data.py from data/asd (NIST Atomic Spectra Database, see data/asd/README.txt). Do not edit.\n",
           "// Columns of kAsdLines: wavelength nm (vacuum; the observed one when ASD gives it, else the Ritz one), observed?, its uncertainty nm (NaN when ASD gives none), the Ritz wavelength nm when ASD gives one (NaN when not), relative intensity as ASD wrote it (\"\" when none),\n",
           "// n of the lower and the upper level (0 when the configuration has no leading n), 1 when ASD gives J of both levels (a resolved fine-structure component; 0 for an unresolved n-n line), the transition as text.\n"]
    for z in range(1, 119):
        for kind in ("ion", "atom"):
            for r in rows.get((z, kind), []):
                w, obs = wavelength(r)
                if w is None or not (w > 0):
                    die("a line of %s has no positive wavelength" % man[(z, kind)][0])
                unc = num(r["unc_obs_wl"] if obs else r["unc_ritz_wl"])
                trans = "%s %s %s - %s %s %s" % (r["conf_i"], r["term_i"], r["J_i"], r["conf_k"], r["term_k"], r["J_k"])
                allrows.append((z, kind, w, obs, unc, num(r["ritz_wl_vac(nm)"]), r["intens"], level_n(r["conf_i"]) or 0, level_n(r["conf_k"]) or 0, 1 if (r["J_i"] and r["J_k"]) else 0, re.sub(r"\s+", " ", trans).strip()))
    out.append("static const AsdLine kAsdLines[] = {\n")
    for (z, kind, w, obs, unc, ritz, inten, ni, nk, res, trans) in allrows:
        out.append("    {%r, %d, %s, %s, %s, %d, %d, %d, %s},\n" % (w, 1 if obs else 0, ("%r" % unc) if unc is not None else "NAN", ("%r" % ritz) if ritz is not None else "NAN", c_str(inten), ni, nk, res, c_str(trans)))
    out.append("};\n")
    out.append("// Per element and kind: {spectrum name, status (0 lines, 1 ASD has no such spectrum, 2 no line in the range or selection), ASD lines returned, first index, count}\n")
    out.append("static const AsdSet kAsdSets[118][2] = {\n")
    idx = 0
    for z in range(1, 119):
        out.append("    {")
        for kind in ("ion", "atom"):
            spec, status, ret, kept = man[(z, kind)]
            st = {"ok": 0, "none": 1, "empty": 2}[status]
            out.append("{%s, %d, %d, %d, %d}%s" % (c_str(spec), st, ret, idx, kept, ", " if kind == "ion" else ""))
            idx += kept
        out.append("},\n")
    out.append("};\n")
    out.append("static const char* const kAsdSelIon = %s;\nstatic const char* const kAsdSelAtom = %s;\n" % (c_str(SELECT_ION), c_str(SELECT_ATOM)))
    out.append("static const double kAsdRanges[4] = {%r, %r, %r, %r};   // nm: ion low, ion high, atom low, atom high (the query's wavelength range)\n" % (LOW_NM_ION, UPP_NM_ION, LOW_NM_ATOM, UPP_NM_ATOM))
    if idx != len(allrows):
        die("internal: line count mismatch")
    open(OUT_INC, "w", encoding="utf-8", newline="\n").write("".join(out))
    print("wrote %s: %d lines" % (OUT_INC, len(allrows)))


if __name__ == "__main__":
    if len(sys.argv) != 2 or sys.argv[1] not in ("fetch", "build"):
        sys.exit(__doc__)
    fetch() if sys.argv[1] == "fetch" else build()
