#!/usr/bin/env python3
"""Write the elements' melting points, boiling points and densities (src/atom/atom_data.inc, the kTempD block) from the saved mendeleev 1.3.0 tables.

Python 3, standard library only, offline:  python3 -I native/tools/make_temp_data.py
Source and provenance: native/data/elements/README.txt (the CRC Handbook of Chemistry and Physics, 97th ed. 2016 for the temperatures and
95th ed. 2014 for the densities, as stored in the mendeleev 1.3.0 database).
Rules (nothing else is changed or invented):
  - a row marked is_transition (a solid-solid change: grey tin at 286 K, rhombic sulfur at 368 K, vitreous selenium) is not a melting point and is skipped;
  - two forms left: phosphorus = white (the common form), sulfur = monoclinic (what yellow sulfur turns into before it melts at 388 K);
  - flag 's' = the row is a sublimation point (carbon, arsenic): the solid turns straight to gas at normal pressure, the value goes in 'boil';
    'p' = helium (no melting point at normal pressure); 'a' = phosphorus, sulfur (several forms); 'e' = a predicted melting point (the Wikipedia pages in
    data/elements/wiki say so: At, Fr, Fm, Md, No, Lr);
  - densities marked by mendeleev as Wikipedia estimates (At, Fr, Es to Og) are left out (NAN): only measured CRC densities are shown;
  - values are written as stored, rounded to 3 decimals (the stored floats carry binary noise such as 20.270999999999987).
"""
import csv, math, os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pl_sources   # the mendeleev tables are in the private sources repo (PL_SOURCES_DIR)

HERE = os.path.dirname(os.path.abspath(__file__))
INC = os.path.join(HERE, "..", "src", "atom", "atom_data.inc")
ESTIMATED_MELT = {85, 87, 100, 101, 102, 103}
NO_DENSITY = {85, 87} | set(range(99, 119))
PICK = {15: "white", 16: "monoclinic"}


def num(s):
    return None if s in ("", "None") else float(s)


def fmt(x):
    if x is None:
        return "NAN"
    s = "%.3f" % x
    return s.rstrip("0").rstrip(".")


def main():
    rows = {}
    for r in csv.DictReader(open(pl_sources.path("elements/mendeleev_1.3.0_phasetransitions.csv"))):
        if r["is_transition"] in ("1", "True"):
            continue
        rows.setdefault(int(r["atomic_number"]), []).append(r)
    dens = {int(r["atomic_number"]): num(r["density"]) for r in csv.DictReader(open(pl_sources.path("elements/mendeleev_1.3.0_density.csv")))}
    out = []
    for z in range(1, 119):
        rs = rows.get(z, [])
        if len(rs) > 1:
            rs = [r for r in rs if r["allotrope"] == PICK.get(z)]
            if len(rs) != 1:
                sys.exit("Z=%d: several forms and no rule picks one" % z)
        m = b = None
        flag = "0"
        if rs:
            r = rs[0]
            m, b = num(r["melting_point"]), num(r["boiling_point"])
            if r["is_sublimation_point"] in ("1", "True"):
                flag, m = "'s'", None
        if z == 2:
            flag = "'p'"
        if z in PICK:
            flag = "'a'"
        if z in ESTIMATED_MELT:
            flag = "'e'"
        d = None if z in NO_DENSITY else dens.get(z)
        out.append("    {%s, %s, %s, %s}," % (fmt(m), fmt(b), flag, "NAN" if d is None else "%.6g" % d))
    src = open(INC, encoding="utf-8").read()
    head = "static const TempData kTempD[119] = {{NAN, NAN, 0, NAN},\n"
    i = src.index(head) + len(head)
    j = src.index("};", i) + 2
    new = src[:i] + "\n".join(out) + "\n};" + src[j:]
    open(INC, "w", encoding="utf-8", newline="\n").write(new)
    print("wrote kTempD: 118 rows")


if __name__ == "__main__":
    main()
