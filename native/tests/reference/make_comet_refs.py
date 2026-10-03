#!/usr/bin/env python3
"""0.10.5 stage 3: rewrites the six comet rows of reference/reference_values.h (initial positions, the three runs, the orbit shapes).
Run it AFTER make_reference.js (which makes the file from the web code and knows only the old comet elements):
    node tests/reference/make_reference.js > tests/reference/reference_values.h   (as before)
    python3 tests/reference/make_comet_refs.py tests/reference/reference_values.h
The new rows come from an independent Python Kepler solver (same formulas as the lab: Newton's method on Kepler's equation, rotation by Omega, i, omega, speed sqrt(GM a)/r with the lab's
GM = 4 pi^2 au^3 / year^2, au = 149.6e9 m, year = 365.25 d) applied to the elements in tests/reference/comet_table.inc, i.e. the JPL records, not to the lab's C++ code.
"""
import math, re, sys

path = sys.argv[1]
src = open(path, encoding="utf-8").read()
inc = open(path.replace("reference_values.h", "comet_table.inc"), encoding="utf-8").read()
AU, YEAR = 149.6e9, 365.25 * 86400.0
GM = 4 * math.pi ** 2 * AU ** 3 / YEAR ** 2
rec = {}
for m in re.finditer(r'\{"([^"]+)", "[^"]*", "[^"]*", ([^,]+), ([^,]+), ([^,]+), ([^,]+), ([^,]+), ([^,]+), ([^,]+), ([^,\n]+),', inc):
    n = m.group(1); v = [float(x) for x in m.groups()[2:]]
    rec[n] = dict(q=v[0], e=v[1], i=v[2], om=v[3], w=v[4], tp=v[5], per=v[6])


def state(r, jd):
    a = r["q"] / (1 - r["e"]) * AU
    e = r["e"]
    M = (((jd - r["tp"]) / r["per"] * 360.0) % 360.0 + 540.0) % 360.0 - 180.0
    M = math.radians(M)
    E = (math.pi if M >= 0 else -math.pi) if e > 0.8 else M + e * math.sin(M)
    for _ in range(100):
        dE = (M - (E - e * math.sin(E))) / (1 - e * math.cos(E)); E += dE
        if abs(dE) < 1e-14: break
    rr = a * (1 - e * math.cos(E)); q = math.sqrt(1 - e * e); kv = math.sqrt(GM * a) / rr
    P = (a * (math.cos(E) - e), a * q * math.sin(E)); V = (-kv * math.sin(E), kv * q * math.cos(E))
    I, O, W = (math.radians(r[k]) for k in ("i", "om", "w"))
    co, so, cO, sO, cI, sI = math.cos(W), math.sin(W), math.cos(O), math.sin(O), math.cos(I), math.sin(I)
    rot = lambda X, Y: ((co * cO - so * sO * cI) * X + (-so * cO - co * sO * cI) * Y, (co * sO + so * cO * cI) * X + (-so * sO + co * cO * cI) * Y, so * sI * X + co * sI * Y)
    return rot(*P), rot(*V)


epoch_ms = int(re.search(r"kEpochMs = (\d+)LL", src).group(1))
jd0 = epoch_ms / 86400000.0 + 2440587.5
f = lambda x: "%.17g" % x
for name in rec:
    # the three runs: the simulated time of each set sets the date
    for setname, tvar in (("kInitial", None), ("kRunDefault", "kRunDefaultSimTime"), ("kRunFast", "kRunFastSimTime"), ("kRunRealTime", "kRunRealTimeSimTime")):
        t = 0.0 if tvar is None else float(re.search(tvar + r" = ([\d.e+-]+);", src).group(1))
        (x, y, z), (vx, vy, vz) = state(rec[name], jd0 + t / 86400.0)
        i0 = src.index("inline const BodyState %s[] = {" % setname); i1 = src.index("};", i0)
        block = src[i0:i1]
        new, n = re.subn(r'  \{"%s", [^\n]*\},\n' % re.escape(name), '  {"%s", %s, %s, %s, %s, %s, %s},\n' % (name, f(x), f(y), f(z), f(vx), f(vy), f(vz)), block)
        assert n == 1, (name, setname)
        src = src[:i0] + new + src[i1:]
    r = rec[name]; a = r["q"] / (1 - r["e"]) * AU
    period = 2 * math.pi * math.sqrt(a ** 3 / GM) / YEAR
    i0 = src.index("inline const OrbitShape kOrbits[] = {"); i1 = src.index("};", i0)
    new, n = re.subn(r'  \{"%s", [^\n]*\},\n' % re.escape(name), '  {"%s", %s, %s, %s, %s},\n' % (name, f(a), f(r["e"]), f(r["i"]), f(period)), src[i0:i1])
    assert n == 1, name
    src = src[:i0] + new + src[i1:]
note = "// 0.10.5 stage 3: the six comet rows are NOT from the web code any more; make_comet_refs.py rewrote them from the JPL records in comet_table.inc with an independent Python Kepler solver.\n"
if note not in src:
    src = src.replace("\n", "\n" + note, 1)
open(path, "w", encoding="utf-8").write(src)
print("comet rows rewritten")
