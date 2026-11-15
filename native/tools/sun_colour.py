# 0.49.4: the Sun's colour on an sRGB screen from its measured spectrum (data/sun/README.txt). Needs numpy and colour-science.
# Prints the bytes for src/orbit/sun_facts.h kSunSrgb and the chromaticity the test (tests/test_orbit.cpp, testSun0494) checks against.
import csv, os, sys, numpy as np, colour
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pl_sources   # the two ASTM G173 copies are in the private sources repo (PL_SOURCES_DIR)
here = os.path.dirname(os.path.abspath(__file__))
def etr(name, skip):
    rows = [r for r in csv.reader(open(pl_sources.path('sun/' + name)))][skip:]
    return np.array([float(r[0]) for r in rows]), np.array([float(r[1]) for r in rows])
w, s = etr('astm_g173_pvlib.csv', 2)
w2, s2 = etr('astm_g173_solcore.csv', 2)
assert np.array_equal(w, w2) and np.array_equal(s, s2), 'the two copies of ASTM G173 differ'
print('ETR total over the file, W/m^2: %.1f' % np.trapezoid(s, w))
cm = colour.MSDS_CMFS['CIE 1931 2 Degree Standard Observer'].copy().align(colour.SpectralShape(360, 830, 1))
g = np.arange(360, 831, 1.0)
XYZ = (np.interp(g, w, s)[:, None] * cm.values).sum(0)
xy = XYZ[:2] / XYZ.sum()
lin = colour.XYZ_to_RGB(XYZ / XYZ[1], 'sRGB', apply_cctf_encoding=False)
lin = lin / lin.max()
rgb = np.round(colour.models.eotf_inverse_sRGB(np.clip(lin, 0, 1)) * 255).astype(int)
print('xy = %.4f, %.4f  CCT (McCamy) %.0f K' % (xy[0], xy[1], colour.xy_to_CCT(xy, method='McCamy 1992')))
print('kSunSrgb = {%d, %d, %d}' % tuple(rgb))
