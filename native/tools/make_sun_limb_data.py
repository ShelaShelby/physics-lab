# 0.85.0 (PLAN_REVIEW.md R2): the inputs of the Sun's measured limb darkening, written as src/render/sun_limb_data.inc. Only the standard library.
#   python3 native/tools/make_sun_limb_data.py <sources checkout> > native/src/render/sun_limb_data.inc
# - Neckel & Labs 1994 (Solar Physics 153, 91), Table I: P5(mu) = A0 + ... + A5 mu^5 and F/I for 30 wavelengths, read from this repository's extract
#   native/data/sun/neckel_labs_1994_SoPh153_91.extract.txt (its lines are checked against the saved typed file by tools/check_sources.py).
# - ASTM G173-03, the extraterrestrial spectrum (W m^-2 nm^-1), pvlib's copy: <sources>/data/sun/astm_g173_pvlib.csv, at 380, 385, ..., 780 nm
#   (the CIE grid of render/cie1931.h). The file's sha256 is checked against native/data/SOURCES.md and written into the output.
# The channel coefficients are made from these in C++ (render/sun_limb.h), so the tests can check every step.
import hashlib, os, re, sys

here = os.path.dirname(os.path.abspath(__file__))
src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(here, '..', '..', '..', 'physics-lab-sources')
ext = os.path.join(here, '..', 'data', 'sun', 'neckel_labs_1994_SoPh153_91.extract.txt')
csv = os.path.join(src, 'data', 'sun', 'astm_g173_pvlib.csv')

def sha(p): return hashlib.sha256(open(p, 'rb').read()).hexdigest()

manifest = open(os.path.join(here, '..', 'data', 'SOURCES.md'), encoding='utf-8').read()
csvSha = sha(csv)
assert csvSha in manifest, 'the ASTM file is not the one SOURCES.md lists'

rows = []
t = open(ext, encoding='utf-8').read()
table = t[t.index('#--- begin p. 98, Table I'):]
table = table[:table.index('#--- end')]
for line in table.splitlines():
    m = re.match(r'^(\d+\.\d+)\*? (\d+) (\d+) ' + ' '.join([r'(-?\d\.\d+)'] * 7) + r'$', line)
    if m: rows.append([float(m.group(1))] + [float(m.group(k)) for k in range(4, 11)])
assert len(rows) == 30, len(rows)

E = {}
for line in open(csv, encoding='utf-8'):
    p = line.strip().split(',')
    try: E[float(p[0])] = float(p[1])
    except (ValueError, IndexError): pass
grid = [380 + 5 * k for k in range(81)]
assert all(float(g) in E for g in grid)

print('// Made by tools/make_sun_limb_data.py (0.85.0). Do not edit by hand.')
print('// Neckel & Labs 1994, Solar Physics 153, 91, Table I (from native/data/sun/neckel_labs_1994_SoPh153_91.extract.txt, sha256 %s):' % sha(ext))
print('// lambda (nm), A0, A1, A2, A3, A4, A5, F/I. The 365.875 nm row is as printed (its A_n sum to 0.99800: REPORT 0.84.1); 380-780 nm does not use it.')
print('constexpr int kNl94N = 30;')
print('constexpr double kNl94[kNl94N][8] = {')
for r in rows: print('    {' + ', '.join(('%.3f' % r[0],) + tuple('%.5f' % v for v in r[1:7]) + ('%.4f' % r[7],)) + '},')
print('};')
print('// ASTM G173-03 extraterrestrial spectrum (W m^-2 nm^-1), pvlib copy data/sun/astm_g173_pvlib.csv (sha256 %s), at 380, 385, ..., 780 nm.' % csvSha)
print('constexpr int kSunEtrN = 81;')
print('constexpr double kSunEtrNm0 = 380, kSunEtrStepNm = 5;')
print('constexpr double kSunEtr[kSunEtrN] = {' + ', '.join('%g' % E[float(g)] for g in grid) + '};')
