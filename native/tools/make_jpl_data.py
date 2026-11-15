#!/usr/bin/env python3
"""Check the JPL Horizons files in native/data/jpl and write the two C++ include files made from them.

Python 3, standard library only. Run from anywhere:  python3 native/tools/make_jpl_data.py

Input : native/data/jpl/horizons-{sun,mercury,venus,emb,mars,jupiter,saturn,uranus,neptune}.txt
        (Horizons API vector tables, 301 rows: 1 January 12:00 TDB of every year 1800 to 2100, centre = Solar System Barycenter,
        ecliptic and mean equinox of J2000, km and km/s; see data/jpl/README.txt).
Output: native/tests/reference/jpl_table.inc  all rows of the 9 bodies, in metres and m/s (km x 1000), digits exactly as in the files
        native/src/orbit/jpl_epoch.inc        only the 2026-01-01 12:00 TDB row (Julian date 2461042.0 = kNBodyRefJD) of the 9 bodies

A file that is an error page, is incomplete, or does not say what is expected stops the tool with a message (nothing is written).
Files saved by hand may have Windows line endings or a byte-order mark: both are accepted.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                                   # native/
DATA = os.path.join(ROOT, "data", "jpl")
OUT_TABLE = os.path.join(ROOT, "tests", "reference", "jpl_table.inc")
OUT_EPOCH = os.path.join(ROOT, "src", "orbit", "jpl_epoch.inc")

# (file stem, text the "Target body name" line must contain) in the lab's body order: the Sun, then Mercury ... Neptune (Earth = Earth-Moon barycentre)
BODIES = [
    ("sun", "Sun (10)"), ("mercury", "Mercury Barycenter (199)"), ("venus", "Venus Barycenter (299)"),
    ("emb", "Earth-Moon Barycenter (3)"), ("mars", "Mars Barycenter (4)"), ("jupiter", "Jupiter Barycenter (5)"),
    ("saturn", "Saturn Barycenter (6)"), ("uranus", "Uranus Barycenter (7)"), ("neptune", "Neptune Barycenter (8)"),
]
FIRST_YEAR, LAST_YEAR = 1800, 2100
ROWS = LAST_YEAR - FIRST_YEAR + 1                              # 301
EPOCH_YEAR = 2026                                              # the lab's reference epoch, kNBodyRefJD = 2461042.0
EPOCH_JD = 2461042.0

NUM = r"[-+]?\d\.\d+E[-+]\d\d"
RE_JD = re.compile(r"^(\d+\.\d+) = A\.D\. (\d{4})-Jan-01 12:00:00\.0000 TDB\s*$")
RE_POS = re.compile(r"^\s*X =\s*(%s) Y =\s*(%s) Z =\s*(%s)\s*$" % (NUM, NUM, NUM))
RE_VEL = re.compile(r"^\s*VX=\s*(%s) VY=\s*(%s) VZ=\s*(%s)\s*$" % (NUM, NUM, NUM))


def die(msg):
    sys.exit("make_jpl_data.py: " + msg)


def gregorian_jd_noon(year, month=1, day=1):
    """Julian date of 12:00 on a Gregorian calendar date (integer arithmetic; Fliegel & Van Flandern 1968 form of the Julian day number)."""
    a = (14 - month) // 12
    y = year + 4800 - a
    m = month + 12 * a - 3
    return day + (153 * m + 2) // 5 + 365 * y + y // 4 - y // 100 + y // 400 - 32045   # the Julian day number is the JD at noon


def times_1000(s):
    """'-3.304561404094865E+07' (km) -> '-3.304561404094865E+10' (m): the digits stay, only the exponent grows by 3 (exact)."""
    mant, exp = s.upper().split("E")
    return "%sE%+03d" % (mant, int(exp) + 3)


def read_file(stem, target):
    path = os.path.join(DATA, "horizons-%s.txt" % stem)
    try:
        raw = open(path, "rb").read()
    except OSError as e:
        die("cannot read %s: %s" % (path, e))
    text = raw.decode("utf-8-sig", errors="strict")            # drops a BOM
    lines = text.replace("\r\n", "\n").replace("\r", "\n").split("\n")
    if "$$SOE" not in [l.strip() for l in lines] or "$$EOE" not in [l.strip() for l in lines]:
        die("%s: no $$SOE ... $$EOE data block (an error page, or the file is cut short)" % stem)
    soe = [l.strip() for l in lines].index("$$SOE")
    eoe = [l.strip() for l in lines].index("$$EOE")
    if eoe <= soe:
        die("%s: $$EOE comes before $$SOE" % stem)
    head = lines[:soe]
    flat = [re.sub(r"\s+", " ", l).strip() for l in head]

    def need(prefix, text_part):
        for l in flat:
            if l.startswith(prefix):
                if text_part not in l:
                    die("%s: header line '%s' should contain '%s'" % (stem, l, text_part))
                return
        die("%s: header line starting '%s' is missing" % (stem, prefix))

    need("Target body name:", target)
    need("Center body name:", "Solar System Barycenter (0)")
    need("Center-site name:", "BODY CENTER")
    need("Start time", "A.D. 1800-Jan-01 12:00:00.0000 TDB")
    need("Stop time", "A.D. 2100-Jan-01 12:00:00.0000 TDB")
    need("Step-size", "1 calendar years")
    need("Output units", "KM-S")
    need("Output type", "GEOMETRIC cartesian states")
    need("Output format", "2 (position and velocity)")
    need("Reference frame", "Ecliptic of J2000.0")

    body = [l for l in lines[soe + 1:eoe] if l.strip()]
    if len(body) != 3 * ROWS:
        die("%s: %d data lines, expected %d (%d rows of 3 lines)" % (stem, len(body), 3 * ROWS, ROWS))
    rows = []
    for i in range(ROWS):
        year = FIRST_YEAR + i
        m1, m2, m3 = RE_JD.match(body[3 * i]), RE_POS.match(body[3 * i + 1]), RE_VEL.match(body[3 * i + 2])
        if not (m1 and m2 and m3):
            die("%s: row %d (year %d) does not have the expected three lines:\n%s" % (stem, i, year, "\n".join(body[3 * i:3 * i + 3])))
        if int(m1.group(2)) != year:
            die("%s: row %d says year %s, expected %d" % (stem, i, m1.group(2), year))
        jd = float(m1.group(1))
        want = gregorian_jd_noon(year)
        if jd != want:
            die("%s: year %d has Julian date %s, the Gregorian formula gives %d" % (stem, year, m1.group(1), want))
        rows.append((m1.group(1), list(m2.groups()) + list(m3.groups())))
    return rows


def main():
    tables = [read_file(stem, target) for stem, target in BODIES]
    for stem_target, rows in zip(BODIES, tables):
        print("%-8s ok: %d rows, years %d..%d, Julian dates checked against the Gregorian formula" % (stem_target[0], len(rows), FIRST_YEAR, LAST_YEAR))

    k = EPOCH_YEAR - FIRST_YEAR
    for stem_target, rows in zip(BODIES, tables):
        if float(rows[k][0]) != EPOCH_JD:
            die("%s: the %d row has Julian date %s, expected %s" % (stem_target[0], EPOCH_YEAR, rows[k][0], EPOCH_JD))

    names = ", ".join(s for s, _ in BODIES)
    with open(OUT_TABLE, "w", newline="\n") as f:
        f.write("// GENERATED by native/tools/make_jpl_data.py from native/data/jpl/horizons-*.txt (NASA/JPL Horizons API, header {source: DE441}, downloaded 2026-10-03); do not edit.\n")
        f.write("// Initialiser for  double kJplTable[9][301][7]: bodies in the lab's order (%s; Earth = Earth-Moon barycentre),\n" % names)
        f.write("// then the rows for 1 January 12:00 TDB of the years 1800 .. 2100: { Julian date (TDB), x, y, z (m), vx, vy, vz (m/s) }.\n")
        f.write("// Centre = Solar System Barycenter, ecliptic and mean equinox of J2000. Metres = km x 1000: the digits are exactly the file's, only the exponent is raised by 3.\n{\n")
        for rows in tables:
            f.write("{\n")
            for jd, v in rows:
                f.write("{%s, %s},\n" % (jd, ", ".join(times_1000(s) for s in v)))
            f.write("},\n")
        f.write("}\n")
    with open(OUT_EPOCH, "w", newline="\n") as f:
        f.write("// GENERATED by native/tools/make_jpl_data.py from native/data/jpl/horizons-*.txt (NASA/JPL Horizons API, header {source: DE441}, downloaded 2026-10-03); do not edit.\n")
        f.write("// Initialiser for  double kJplEpoch[9][2][3]: the state at 2026-01-01 12:00 TDB (Julian date 2461042.0 = kNBodyRefJD) of the 9 bodies in the lab's order\n")
        f.write("// (%s; Earth = Earth-Moon barycentre): { {x, y, z} (m), {vx, vy, vz} (m/s) }. Centre = Solar System Barycenter, ecliptic and mean equinox of J2000.\n{\n" % names)
        for rows in tables:
            v = [times_1000(s) for s in rows[k][1]]
            f.write("{{%s}, {%s}},\n" % (", ".join(v[:3]), ", ".join(v[3:])))
        f.write("}\n")
    print("wrote %s (%d bytes)" % (OUT_TABLE, os.path.getsize(OUT_TABLE)))
    print("wrote %s (%d bytes)" % (OUT_EPOCH, os.path.getsize(OUT_EPOCH)))


if __name__ == "__main__":
    main()
