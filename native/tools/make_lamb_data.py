#!/usr/bin/env python3
"""Make src/atom/lamb_data.inc from data/lamb (Yerokhin & Shabaev 2015, table tab:energy, rows LAMB and DIRAC; see data/lamb/README.txt).
Each level's shift (hc R_inf) is summed from the table's intervals: L(1s); L(2s) = L(1s) + [2s-1s]; L(2p1/2) = L(2s) + [2p1/2-2s]; L(2p3/2) = L(2p1/2) + [2p3/2-2p1/2].
0.86.0 (PLAN_REVIEW.md R7): the DIRAC row (the point-nucleus Dirac energy, their Eqs. (2)-(4)) as printed: 1s, 2s-1s, 2p3/2-2p1/2 (hc R_inf), each with its number
of printed decimals and its bracketed uncertainty in units of the last digit (0: none printed). The 2p1/2-2s column is 0 in Dirac theory (checked here).
Run from native/: python3 tools/make_lamb_data.py"""
import os, re, sys, tarfile
from decimal import Decimal
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pl_sources   # the paper's LaTeX source is in the private sources repo (PL_SOURCES_DIR)

src = tarfile.open(pl_sources.path("lamb/yerokhin_shabaev_1506.01885_source.tar.gz")).extractfile("Hlike8.tex").read().decode("latin-1")
start = src.index("\\label{tab:energy}")
end = src.index("\\end{longtable*}", start)
tab = src[start:end]

def num(s):  # "0.000002x4842735\,(43)" -> Decimal; the x only marks the decimal alignment, the bracket is the uncertainty
    s = s.split("\\,")[0].replace("x", "").strip()
    return Decimal(s)

def printed(s):  # (value as printed, decimals printed, bracketed uncertainty in units of the last digit or 0)
    v = s.split("\\,")[0].replace("x", "").strip()
    m = re.search(r"\((\d+)\)", s)
    return v, (len(v.split(".")[1]) if "." in v else 0), (int(m.group(1)) if m else 0)

rows = []
for blk in re.split(r"\\multicolumn\{5\}\{l\}\{", tab)[1:]:
    m = re.search(r"\$Z = (\d+),\\ \\mbox\{\\rm \$\^\{(\d+)\}\$([A-Za-z0-9]+) *\},\\ +R = ([0-9.]+)", blk)
    if not m:
        sys.exit("cannot read the block header: " + blk[:120])
    Z, A, sym, R = int(m.group(1)), int(m.group(2)), m.group(3), m.group(4)
    if sym == "0": sym = "O"   # the source writes oxygen-16 as "$^{16}$0" (a zero): a misprint of the symbol, the row is Z = 8
    lm = re.search(r"\nLAMB &([^\n]*)\\\\", blk)
    v = [num(x) for x in lm.group(1).split("&")]
    if len(v) != 4:
        sys.exit("LAMB row of Z = %d has %d values" % (Z, len(v)))
    l1s = v[0]; l2s = l1s + v[1]; l2p1 = l2s + v[2]; l2p3 = l2p1 + v[3]
    dm = re.search(r"\nDIRAC &([^\n]*)\\\\", blk)
    d = [printed(x) for x in dm.group(1).split("&")]
    if len(d) != 4 or Decimal(d[2][0]) != 0:
        sys.exit("DIRAC row of Z = %d: %s" % (Z, dm.group(1)))
    rows.append((Z, A, sym, R, l1s, l2s, l2p1, l2p3, d[0], d[1], d[3]))

if [r[0] for r in rows] != list(range(1, 111)):
    sys.exit("expected Z = 1..110, got %s" % [r[0] for r in rows])
with open("src/atom/lamb_data.inc", "w") as f:
    f.write("// Made by tools/make_lamb_data.py from data/lamb (Yerokhin & Shabaev, J. Phys. Chem. Ref. Data 44, 033103 (2015), table tab:energy, rows LAMB and DIRAC). Do not edit by hand.\n")
    f.write("// Z, the isotope's mass number, its symbol, its rms charge radius (fm) as the table gives it, and the Lamb shift of 1s, 2s, 2p1/2, 2p3/2 in hc R_inf;\n")
    f.write("// then (0.86.0) the DIRAC row as printed: 1s, 2s-1s, 2p3/2-2p1/2 in hc R_inf, each with {value, decimals printed, bracketed uncertainty in last digits}.\n")
    f.write("static const LambRow kLambRows[110] = {\n")
    for Z, A, sym, R, a, b, c, d, e1, e2, e3 in rows:
        f.write('    {%d, %d, "%s", %s, %s, %s, %s, %s, {{%s, %d, %d}, {%s, %d, %d}, {%s, %d, %d}}},\n' % ((Z, A, sym, R, a, b, c, d) + e1 + e2 + e3))
    f.write("};\n")
print("wrote", len(rows), "rows")
