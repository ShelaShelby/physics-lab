#!/usr/bin/env python3
"""0.12 stage 2: bring the lab's half-life codes (src/atom/atom_data.inc, kNucLines) in line with NUBASE2020 (data/nubase/nubase_4.mas20.txt), the source the lab names for them.

Python 3, standard library only. Offline. Run once:  python3 native/tools/fix_halflives_0_12.py          (rewrites atom_data.inc; prints every change)
                                                      python3 native/tools/fix_halflives_0_12.py --check  (changes nothing; exit 1 if the file would change)

What it changes, and only this (everything else in the file is kept byte for byte):
 1. The 113 codes written "~x" (the lab's "approximate" mark, all superheavy: Z 103-118). NUBASE2020 gives each of them as a plain measured value with an uncertainty (no "#", no "~"): the lab's
    table had rounded them to one or two digits and flagged them approximate. They get NUBASE2020's own value.
 2. 16 more superheavy codes (Z 103-118) that differ from NUBASE2020's plain value by more than 0.5 % (up to a factor 2.8: an older evaluation).
 3. Kr-78 and Xe-124: the lab had half-lives (9.2e21 y and 1.8e22 y); NUBASE2020 and the mendeleev package (1.3.0, read 2026-10-04) both list them as stable with a lower limit, and the numbers are
    in neither: they become "S" and their decay entries are removed (stable nuclides have none).
A value is the file's number times the unit (s, m = 60 s, h = 3600 s, d = 86400 s, y = 365.25 d, SI prefixes), written the way the table writes numbers (1.25e-2).
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "data", "nubase", "nubase_4.mas20.txt")
INC = os.path.join(ROOT, "src", "atom", "atom_data.inc")
YEAR = 365.25 * 86400
UNIT = {"ys": 1e-24, "zs": 1e-21, "as": 1e-18, "fs": 1e-15, "ps": 1e-12, "ns": 1e-9, "us": 1e-6, "ms": 1e-3, "s": 1.0, "m": 60.0, "h": 3600.0, "d": 86400.0, "y": YEAR,
        "ky": 1e3 * YEAR, "My": 1e6 * YEAR, "Gy": 1e9 * YEAR, "Ty": 1e12 * YEAR, "Py": 1e15 * YEAR, "Ey": 1e18 * YEAR, "Zy": 1e21 * YEAR, "Yy": 1e24 * YEAR}


def nubase():
    rows = {}
    with open(SRC, encoding="ascii") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            zzzi = line[4:8]
            if zzzi[3] != "0":
                continue
            A, Z = int(line[0:3]), int(zzzi[:3])
            if Z == 0:
                continue
            rows[(Z, A - Z)] = (line[69:78].strip(), line[78:80].strip())
    return rows


def code_of(seconds):
    m, e = ("%.6e" % seconds).split("e")
    m = m.rstrip("0").rstrip(".")
    return "%se%d" % (m, int(e))


def main():
    check = "--check" in sys.argv
    nb = nubase()
    text = open(INC, encoding="utf-8").read()
    changes = []
    stable = []                                              # (Z, N) made stable
    start = text.index("static const char* const kNucLines[118] = {")
    end = text.index("};", start)
    block = text[start:end]
    out_lines = []
    for line in block.split("\n"):
        m = re.match(r'^(\s*")(\d+)\|(.*)(",?)$', line)
        if not m:
            out_lines.append(line)
            continue
        Z = int(m.group(2))
        parts = []
        for e in m.group(3).split(","):
            n, code = e.split(":")
            N = int(n)
            T, unit = nb[(Z, N)]
            new = code
            if T == "stbl" and code not in ("S", "U", "?"):
                new = "S"; stable.append((Z, N))
            elif re.fullmatch(r"[0-9.]+", T) and code not in ("S", "U", "?"):
                val = float(T) * UNIT[unit]
                old = float(code.lstrip("~"))
                if code.startswith("~") or abs(val - old) > 0.005 * val:
                    new = code_of(val)
            if new != code:
                changes.append((Z, N, code, new, "%s %s" % (T, unit)))
            parts.append("%s:%s" % (n, new))
        out_lines.append(m.group(1) + m.group(2) + "|" + ",".join(parts) + m.group(4))
    new_block = "\n".join(out_lines)
    text2 = text[:start] + new_block + text[end:]
    # the decay entries of the nuclides that became stable
    dstart = text2.index("static const char* const kDecayLines[118] = {")
    dend = text2.index("};", dstart)
    dblock = text2[dstart:dend]
    dl = dblock.split("\n")
    for Z, N in stable:
        for i, line in enumerate(dl):
            if line.startswith('    "%d|' % Z):
                head, rest = line.split("|", 1)
                ents = rest.rstrip('",').split(";")
                keep = [x for x in ents if not x.startswith("%d:" % N)]
                if len(keep) != len(ents) - 1:
                    sys.exit("decay entry of Z=%d N=%d not found exactly once" % (Z, N))
                tail = '",' if rest.endswith('",') else '"'
                dl[i] = head + "|" + ";".join(keep) + tail
    text2 = text2[:dstart] + "\n".join(dl) + text2[dend:]
    for c in changes:
        print("Z=%d N=%d  %s -> %s   (NUBASE2020: %s)" % c)
    print(len(changes), "codes changed;", len(stable), "made stable:", stable)
    if check:
        sys.exit(1 if text2 != text else 0)
    open(INC, "w", encoding="utf-8", newline="\n").write(text2)


if __name__ == "__main__":
    main()
