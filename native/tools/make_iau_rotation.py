# 0.50 (review item 10): the IAU WGCCRE rotation models of the moons the app draws with a map, read from NAIF's PCK text kernel pck00011.tpc
# (data/jpl/naif_pck00011.tpc, saved as received; its data are the 2015 report, Archinal et al. 2018, Celest. Mech. Dyn. Astr. 130, 22, as corrected,
# and the 2009 values where the 2015 report changed nothing). Only the \begindata blocks are read, as the SPICE reader does.
# Writes src/orbit/iau_rotation.inc:  python3 tools/make_iau_rotation.py > src/orbit/iau_rotation.inc
import os, re
here = os.path.dirname(os.path.abspath(__file__))
t = open(os.path.join(here, '..', 'data', 'jpl', 'naif_pck00011.tpc'), encoding='latin-1').read()
d = {}
for b in re.findall(r'\\begindata(.*?)(?:\\begintext|$)', t, re.S):
    for m in re.finditer(r'(BODY\w+)\s*=\s*\(([^)]*)\)', b):
        d[m.group(1)] = [float(v.replace('D', 'E')) for v in m.group(2).split()]
    for m in re.finditer(r'(BODY\w+)\s*=\s*([-+0-9.EeDd]+)\s*$', b, re.M):   # a scalar without brackets (BODY4_MAX_PHASE_DEGREE = 2)
        d[m.group(1)] = [float(m.group(2).replace('D', 'E'))]
BODIES = [('Io', 501), ('Europa', 502), ('Ganymede', 503), ('Callisto', 504), ('Rhea', 605), ('Triton', 801), ('Charon', 901), ('Phobos', 401)]
def arr(v): return '{' + ', '.join(repr(x) for x in v) + '}'
print('// Made by tools/make_iau_rotation.py (0.50) from data/jpl/naif_pck00011.tpc. Do not edit by hand.')
print('// pole RA, Dec: a + b T + c T^2 (deg, T Julian centuries TDB from J2000); W: a + b d + c d^2 (deg, d days); system angles: a + b T (+ c T^2), deg;')
print('// RA += sum ra_i sin(angle_i), Dec += sum dec_i cos(angle_i), W += sum pm_i sin(angle_i) (the SPICE convention).')
print('struct IauRotation { const char* name; int system; double ra[3], dec[3], pm[3]; int nra, ndec, npm; double nutRa[20], nutDec[20], nutPm[20]; };')
print('struct IauAngles { int system, degree, count; double coef[80]; };')
print('static const IauRotation kIauRotations[] = {')
for name, i in BODIES:
    g = lambda k: d.get('BODY%d_%s' % (i, k), [])
    ra, de, pm = (g('POLE_RA') + [0, 0, 0])[:3], (g('POLE_DEC') + [0, 0, 0])[:3], (g('PM') + [0, 0, 0])[:3]
    nr, nd, npm = g('NUT_PREC_RA'), g('NUT_PREC_DEC'), g('NUT_PREC_PM')
    assert len(nr) <= 20 and len(nd) <= 20 and len(npm) <= 20
    print('    {"%s", %d, %s, %s, %s, %d, %d, %d, %s, %s, %s},' % (name, i // 100, arr(ra), arr(de), arr(pm), len(nr), len(nd), len(npm), arr(nr or [0]), arr(nd or [0]), arr(npm or [0])))
print('};')
print('static const IauAngles kIauAngles[] = {')
for s in sorted({i // 100 for _, i in BODIES}):
    a = d.get('BODY%d_NUT_PREC_ANGLES' % s, [])
    deg = int(d.get('BODY%d_MAX_PHASE_DEGREE' % s, [1])[0])
    assert len(a) % (deg + 1) == 0 and len(a) <= 80
    print('    {%d, %d, %d, %s},' % (s, deg, len(a) // (deg + 1), arr(a or [0])))
print('};')
