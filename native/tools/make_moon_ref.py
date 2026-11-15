#!/usr/bin/env python3
"""Download the Moon (and Sun) positions seen from the Earth's centre from JPL Horizons and write native/tests/reference/moon_table.inc.

Python 3, standard library only. Needs the internet (ssd.jpl.nasa.gov). Run from anywhere:  python3 native/tools/make_moon_ref.py

What is asked (Horizons API, https://ssd-api.jpl.nasa.gov/doc/horizons.html), the same method as make_jpl_data.py:
    COMMAND = '301' (Moon) and '10' (Sun)   CENTER = '500@399' (Earth's centre)   EPHEM_TYPE = VECTORS, VEC_TABLE = 1 (position only)
    VEC_CORR = NONE (geometric: no light time, no aberration)   REF_PLANE = ECLIPTIC (ecliptic and mean equinox of J2000)
    OUT_UNITS = KM-S   TIME_TYPE = TDB   CSV_FORMAT = YES.   Moon and Sun are DE441 (the header of every reply says so).
Three groups of dates, the same for Moon and Sun:
    group 0: every 5 years, 1 January 12:00 TDB, 1800 .. 2100 (61 rows)       -> the long-range error
    group 1: 60 daily steps from 2025-12-02 00:00 TDB                          -> two full lunar cycles (phases)
    group 2: 49 hourly steps, 2026-01-02 00:00 .. 2026-01-04 00:00 TDB          -> the full moon of 2026-01-03 to minutes
A reply that is an error page, is incomplete, or does not say what is expected stops the tool (nothing is written).
"""
import os
import re
import sys
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)                                   # native/
OUT = os.path.join(ROOT, "tests", "reference", "moon_table.inc")
API = "https://ssd.jpl.nasa.gov/api/horizons.api"

# (group, start, stop, step, number of rows)
GROUPS = [
    (0, "1800-01-01 12:00", "2100-01-01 12:00", "5 y", 61),
    (1, "2025-12-02 00:00", "2026-01-30 00:00", "1 d", 60),
    (2, "2026-01-02 00:00", "2026-01-04 00:00", "1 h", 49),
]
BODIES = [("301", "Moon (301)"), ("10", "Sun (10)")]

NUM = r"[-+]?\d\.\d+E[-+]\d\d"
RE_ROW = re.compile(r"^\s*(\d+\.\d+),\s*A\.D\. (\d{4}-\w{3}-\d\d \d\d:\d\d):\d\d\.\d+,\s*(%s),\s*(%s),\s*(%s),\s*$" % (NUM, NUM, NUM))


def die(msg):
    sys.exit("make_moon_ref.py: " + msg)


def query(command, start, stop, step):
    params = [("format", "text"), ("COMMAND", "'%s'" % command), ("CENTER", "'500@399'"), ("MAKE_EPHEM", "YES"), ("EPHEM_TYPE", "VECTORS"),
              ("START_TIME", "'%s'" % start), ("STOP_TIME", "'%s'" % stop), ("STEP_SIZE", "'%s'" % step), ("REF_PLANE", "ECLIPTIC"),
              ("OUT_UNITS", "KM-S"), ("VEC_TABLE", "1"), ("VEC_CORR", "NONE"), ("TIME_TYPE", "TDB"), ("CSV_FORMAT", "YES")]
    url = API + "?" + urllib.parse.urlencode(params, quote_via=urllib.parse.quote)
    try:
        return url, urllib.request.urlopen(url, timeout=120).read().decode("utf-8")
    except Exception as e:                                      # noqa: BLE001 - any network problem stops the tool
        die("Horizons not reachable (%s)" % e)


def parse(text, target, start, rows_expected, label):
    lines = text.replace("\r\n", "\n").split("\n")
    flat = [re.sub(r"\s+", " ", l).strip() for l in lines]
    if "$$SOE" not in flat or "$$EOE" not in flat:
        die("%s: no $$SOE ... $$EOE block (error page?):\n%s" % (label, text[:600]))
    head = flat[:flat.index("$$SOE")]

    def need(prefix, part):
        for l in head:
            if l.startswith(prefix):
                if part not in l:
                    die("%s: header line '%s' should contain '%s'" % (label, l, part))
                return
        die("%s: header line starting '%s' is missing" % (label, prefix))
    need("Target body name:", target)
    need("Target body name:", "{source: DE441}")
    need("Center body name:", "Earth (399)")
    need("Center-site name:", "BODY CENTER")
    need("Output units", "KM-S")
    need("Output type", "GEOMETRIC cartesian states")
    need("Reference frame", "Ecliptic of J2000.0")
    body = [l for l in lines[flat.index("$$SOE") + 1:flat.index("$$EOE")] if l.strip()]
    if len(body) != rows_expected:
        die("%s: %d rows, expected %d" % (label, len(body), rows_expected))
    rows = []
    for l in body:
        m = RE_ROW.match(l)
        if not m:
            die("%s: row not understood: %s" % (label, l))
        rows.append((m.group(1), m.group(2), [m.group(3), m.group(4), m.group(5)]))
    m = re.search(r"API_USER \w+ (\w+) +(\d+) [\d:]+ (\d{4})", text)
    return rows, (m.group(3) + "-" + m.group(1) + "-" + m.group(2)) if m else "unknown"


def times_1000(s):
    """km -> m: the digits stay, only the exponent grows by 3 (exact)."""
    mant, exp = s.upper().split("E")
    return "%sE%+03d" % (mant, int(exp) + 3)


def main():
    out_rows, urls, got = [], [], set()
    for group, start, stop, step, n in GROUPS:
        per_body = []
        for command, target in BODIES:
            url, text = query(command, start, stop, step)
            rows, got_date = parse(text, target, start, n, "group %d %s" % (group, target))
            per_body.append(rows)
            urls.append(url)
            got.add(got_date)
            print("group %d %-10s ok: %d rows %s .. %s" % (group, target, len(rows), rows[0][1], rows[-1][1]))
        for mr, sr in zip(*per_body):
            if mr[0] != sr[0]:
                die("group %d: Moon and Sun rows have different Julian dates (%s, %s)" % (group, mr[0], sr[0]))
            out_rows.append((group, mr[0], [times_1000(v) for v in mr[2]], [times_1000(v) for v in sr[2]]))
    if len(got) != 1:
        die("replies carry different retrieval dates: %s" % got)
    date = got.pop()
    with open(OUT, "w", newline="\n") as f:
        f.write("// GENERATED by native/tools/make_moon_ref.py; do not edit. Source: NASA/JPL Horizons API (https://ssd.jpl.nasa.gov/api/horizons.api), Moon and Sun {source: DE441}.\n")
        f.write("// Retrieved (date in the reply header): %s. Query for each group and body, as made (one URL each):\n" % date)
        for u in urls:
            f.write("//   %s\n" % u)
        f.write("// Initialiser for  double kMoonTable[170][8]: { group, Julian date (TDB), Moon x, y, z (m), Sun x, y, z (m) }, both seen from the Earth's centre (500@399),\n")
        f.write("// ecliptic and mean equinox of J2000, geometric (no light time). Metres = km x 1000: the digits are exactly Horizons', only the exponent is raised by 3.\n")
        f.write("// group 0: every 5 years 1800..2100 at 1 January 12:00 TDB (61 rows); group 1: daily from 2025-12-02 00:00 TDB (60 rows); group 2: hourly 2026-01-02 00:00 .. 2026-01-04 00:00 TDB (49 rows).\n{\n")
        for g, jd, m, s in out_rows:
            f.write("{%d, %s, %s, %s},\n" % (g, jd, ", ".join(m), ", ".join(s)))
        f.write("}\n")
    print("wrote %s (%d rows, %d bytes)" % (OUT, len(out_rows), os.path.getsize(OUT)))


if __name__ == "__main__":
    main()
