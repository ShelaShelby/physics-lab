#!/usr/bin/env python3
"""0.10.5 stage 4: rescales the saved Simple-mode reference values from the web version's astronomical unit (149.6e9 m) to the IAU one (149,597,870,700 m, units.h).

WHY these values are no longer from the frozen web page: reference_values.h was made by the web's own JavaScript (make_reference.js, index.html, never changed) with the web's au.
The lab now uses the IAU au (owner decision, DECISIONS.md, 3 Oct 2026), and the web page cannot be asked for numbers with another au. In the Simple mode every length is a multiple of the au
(a_AU * au; the Sun's GM is built as 4 pi^2 au^3 / year^2, the softening and the crash radius are multiples of au^3 and au), so the whole run is the old one scaled by s = au_IAU / 149.6e9
with TIME UNCHANGED. What scales, kind by kind (checked, each one, by the tests passing at their old tolerances):
  positions (m)            x s          velocities (m/s)  x s   (v = sqrt(GM a) / r, GM ~ s^3, a and r ~ s)
  orbit sizes a (m)        x s          eccentricities, inclinations, periods (years), mean-motion based angles, times: unchanged (period = a_AU^1.5 years)
  barycentre (m)           x s          belt rocks' positions (m, also after 100 years: their mean motion sqrt(GM / a^3) does not change)
The six comet rows are NOT rescaled here: make_comet_refs.py recomputes them (with this au) from the JPL records.
NOT rescalable, so not touched here: anything that mixes the real GM values (nbody saved states, jpl "Real from the legacy elements start"); see tests/au_legacy.h.
Usage:  python3 tests/reference/rescale_au.py   (from native/; refuses to run twice)
"""
import re, sys

S = 149597870700.0 / 149.6e9
COMETS = {"Halley", "Hale–Bopp", "NEOWISE", "Encke", "67P", "Swift–Tuttle"}
MARK = "rescaled to the IAU au"
f = lambda x: "%.17g" % x


def block(src, start_pat):
    i0 = src.index(start_pat); i1 = src.index("};", i0)
    return i0, i1


path = "tests/reference/reference_values.h"
src = open(path, encoding="utf-8").read()
if MARK in src:
    sys.exit("already rescaled")
n = 0
for setname in ("kInitial", "kRunDefault", "kRunFast", "kRunRealTime"):
    i0, i1 = block(src, "inline const BodyState %s[] = {" % setname)
    def row(m):
        global n
        if m.group(1) in COMETS: return m.group(0)
        v = [float(x) * S for x in m.groups()[1:]]; n += 1
        return '  {"%s", %s},\n' % (m.group(1), ", ".join(f(x) for x in v))
    src = src[:i0] + re.sub(r'  \{"([^"]+)", ([^,]+), ([^,]+), ([^,]+), ([^,]+), ([^,]+), ([^,}]+)\},\n', row, src[i0:i1]) + src[i1:]
i0, i1 = block(src, "inline const OrbitShape kOrbits[] = {")
def orb(m):
    global n
    if m.group(1) in COMETS: return m.group(0)
    n += 1
    return '  {"%s", %s, %s, %s, %s},\n' % (m.group(1), f(float(m.group(2)) * S), m.group(3), m.group(4), m.group(5))
src = src[:i0] + re.sub(r'  \{"([^"]+)", ([^,]+), ([^,]+), ([^,]+), ([^,}]+)\},\n', orb, src[i0:i1]) + src[i1:]
m = re.search(r"constexpr double kBary\[3\] = \{([^}]*)\};", src)
src = src.replace(m.group(0), "constexpr double kBary[3] = {%s};" % ", ".join(f(float(x) * S) for x in m.group(1).split(","))); n += 3
i0, i1 = block(src, "inline const BeltRock kBeltRocks[] = {")
def rock(m):
    global n
    n += 3
    return '  {"%s", %s, %s, %s, %s, %s},\n' % (m.group(1), m.group(2), m.group(3), f(float(m.group(4)) * S), f(float(m.group(5)) * S), f(float(m.group(6)) * S))
src = src[:i0] + re.sub(r'  \{"(\w+)", (\d+), ([^,]+), ([^,]+), ([^,]+), ([^,}]+)\},\n', rock, src[i0:i1]) + src[i1:]
src = src.replace("\n", "\n// 0.10.5 stage 4: lengths and speeds %s (149597870700 / 149.6e9 = %.15f) by tests/reference/rescale_au.py; see its header for why and for what was rescaled.\n" % (MARK, S), 1)
open(path, "w", encoding="utf-8").write(src)

# test_jpl_saved.inc: only the Simple block (heliocentric positions from the elements, exact hex floats)
p2 = "tests/test_jpl_saved.inc"
s2 = open(p2, encoding="utf-8").read()
i0 = s2.index("static const double kSavedSimple[5][8][3] = {"); i1 = s2.index("};", i0)
def hexrow(m):
    global n
    n += 3
    return "{%s}," % ", ".join(float.hex(float.fromhex(x.strip()) * S) for x in m.group(1).split(","))
s2 = s2[:i0] + re.sub(r"\{(-?0x[^}]*)\},", hexrow, s2[i0:i1]) + s2[i1:]
s2 = s2.replace("\n", "\n// 0.10.5 stage 4: kSavedSimple (below) was rescaled by 149597870700 / 149.6e9 (tests/reference/rescale_au.py); kSavedReal is unchanged (see tests/au_legacy.h).\n", 1)
open(p2, "w", encoding="utf-8").write(s2)
print("numbers rescaled:", n)
