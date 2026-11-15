#!/usr/bin/env python3
# 0.31: the nuclear ground states' spins and magnetic dipole moments for the Atom Lab's hyperfine structure (src/atom/hfs_data.inc), from the IAEA table saved in
# data/moments/magn_mom_recomm.csv (README.txt there). Run from the repository root:  python3 native/tools/make_hfs_data.py
# Rules (nothing is guessed): only ground states (level energy 0). A spin in brackets ("(3/2)-") is tentative in the table: kept, flagged. A sign in brackets ("(+)2.69")
# is assumed in the table: kept, flagged. No sign at all: the size is known, the sign not: flagged (the lab then gives the splitting's size only). Two values joined by
# "or" (two spins or two moments possible): left out. An asymmetric uncertainty ("0.32(+8/-20)"): the central value is kept, the uncertainty column says -1.
import csv, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pl_sources   # the IAEA table is in the private sources repo (PL_SOURCES_DIR)

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
SRC = pl_sources.path("moments/magn_mom_recomm.csv")
OUT = os.path.join(ROOT, "src", "atom", "hfs_data.inc")

def spin2(s):
    """'1/2+' -> (1, tentative False); '(3/2)-' -> (3, True); None if not one spin"""
    if " or " in s or "," in s: return None
    tent = "(" in s
    t = s.replace("(", "").replace(")", "").rstrip("+-").strip()
    m = re.fullmatch(r"(\d+)(/2)?", t)
    if not m: return None
    return (int(m.group(1)) if m.group(2) else 2 * int(m.group(1))), tent

def moment(s):
    """'+2.792847351(9)' -> (2.792847351, 9e-9, sign 1 known / 2 assumed / 0 unknown)"""
    if " or " in s: return None
    t = s.replace(" ", "")
    sign = 0
    if t.startswith("(+)") or t.startswith("(-)"): sign, neg, t = 2, t[1] == "-", t[3:]
    elif t[:1] in "+-": sign, neg, t = 1, t[0] == "-", t[1:]
    else: neg = False
    m = re.fullmatch(r"(\d+\.?\d*)(\((\d+)\)|\(\+\d+[/,]-\d+\))?", t)
    if not m: return None
    v = float(m.group(1))
    if m.group(3):
        dec = len(m.group(1).split(".")[1]) if "." in m.group(1) else 0
        unc = int(m.group(3)) * 10.0 ** (-dec)
    elif m.group(2): unc = -1.0
    else: unc = 0.0
    return (-v if neg else v), unc, sign

def main():
    rows, skipped = [], []
    for x in csv.DictReader(open(SRC, newline="", encoding="utf-8")):
        if x["energy [keV]"].strip() not in ("0", "0.0"): continue
        z, a = int(x["z"]), int(x["n.n+n.z"])
        sp, mo = spin2(x["spin"].strip()), moment(x["magnetic dipole [nm]"].strip())
        if sp is None or mo is None or sp[0] == 0:
            skipped.append(f"{z}-{a} spin '{x['spin']}' moment '{x['magnetic dipole [nm]']}'"); continue
        rows.append((z, a, sp[0], sp[1], mo[0], mo[1], mo[2]))
    rows.sort()
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("// Made by tools/make_hfs_data.py from data/moments/magn_mom_recomm.csv (IAEA recommended nuclear magnetic dipole moments, downloaded 6 Nov 2026). Do not edit.\n")
        f.write("// {Z, A, 2I, spin tentative, moment (nuclear magnetons), its uncertainty (-1: asymmetric, 0: none given), sign: 1 measured, 2 assumed, 0 unknown}\n")
        f.write(f"// {len(rows)} ground states; left out ({len(skipped)}): " + "; ".join(skipped) + "\n")
        for r in rows:
            f.write("{%d, %d, %d, %s, %.10g, %.3g, %d},\n" % (r[0], r[1], r[2], "true" if r[3] else "false", r[4], r[5], r[6]))
    print(len(rows), "kept,", len(skipped), "left out")

if __name__ == "__main__":
    sys.exit(main())
