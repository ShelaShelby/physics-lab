# 0.50 (review item 4): the colour of a star on an sRGB screen, as the colour of a black body at its colour temperature. Writes src/orbit/star_colours.inc.
#   - temperature from B-V: Ballesteros 2012, EPL 97, 34008, arXiv:1201.1809, eq. (14): T = 4600 (1 / (0.92 (B-V) + 1.7) + 1 / (0.92 (B-V) + 0.62)) K
#     (data/stars/ballesteros_2012_arxiv1201.1809.pdf, saved as received)
#   - Planck's law (CODATA 2018 h, c, k; exact) from 360 to 830 nm in 1 nm steps, under the CIE 1931 2-degree standard observer: the table in
#     data/stars/colour-science_v0.4.7_cmfs.py ("CIE 1931 2 Degree Standard Observer", CVRL's 1 nm table; saved as received), parsed here
#   - XYZ -> linear sRGB with the IEC 61966-2-1 matrix (D65), scaled so the largest channel is 1 (the same as the Sun, tools/sun_colour.py), clipped at 0,
#     then sRGB-encoded (IEC 61966-2-1 transfer function) and rounded to bytes. No white is mixed in.
# Only the standard library: python3 tools/star_colour.py > src/orbit/star_colours.inc
import math, os, re
here = os.path.dirname(os.path.abspath(__file__))
def cmf():
    t = open(os.path.join(here, '..', 'data', 'stars', 'colour-science_v0.4.7_cmfs.py'), encoding='utf-8').read()
    a = t.index('"CIE 1931 2 Degree Standard Observer": {', t.index('DATA_CMFS_STANDARD_OBSERVER'))
    b = t.index('}', a)
    rows = {int(m.group(1)): tuple(float(m.group(k)) for k in (2, 3, 4)) for m in re.finditer(r'(\d+): \(([-0-9.e+]+), ([-0-9.e+]+), ([-0-9.e+]+)\)', t[a:b])}
    assert len(rows) == 471 and min(rows) == 360 and max(rows) == 830
    assert rows[600][0] == 1.0622 and rows[555][1] == 1.0 and rows[450][2] == 1.77211   # spot values, as tools/sun_colour.py (0.49.4)
    return rows
CMF = cmf()
H, C, K = 6.62607015e-34, 299792458.0, 1.380649e-23
def planck(lam_nm, T):
    l = lam_nm * 1e-9
    return 2 * H * C * C / l ** 5 / math.expm1(H * C / (l * K * T))
def xyz(T):
    X = Y = Z = 0.0
    for lam, (x, y, z) in CMF.items():
        p = planck(lam, T); X += p * x; Y += p * y; Z += p * z
    return X, Y, Z
M = ((3.2406, -1.5372, -0.4986), (-0.9689, 1.8758, 0.0415), (0.0557, -0.2040, 1.0570))   # IEC 61966-2-1, XYZ (D65) -> linear sRGB
def enc(l): return 12.92 * l if l <= 0.0031308 else 1.055 * l ** (1 / 2.4) - 0.055
def srgb(T):
    X, Y, Z = xyz(T)
    lin = [max(0.0, r[0] * X + r[1] * Y + r[2] * Z) for r in M]
    m = max(lin)
    return [int(round(enc(v / m) * 255)) for v in lin]
def ballesteros(bv): return 4600 * (1 / (0.92 * bv + 1.7) + 1 / (0.92 * bv + 0.62))
if __name__ == '__main__':
    lo, hi, step = -0.40, 2.00, 0.01
    n = int(round((hi - lo) / step)) + 1
    print('// Made by tools/star_colour.py (0.50): a black body at the Ballesteros (2012) temperature of each B-V, under the CIE 1931 2-degree observer, to sRGB (largest channel 1).')
    print('// {B-V, T in K, R, G, B}; B-V from %.2f to %.2f in steps of %.2f.' % (lo, hi, step))
    print('static constexpr double kStarBvLo = %.2f, kStarBvStep = %.2f;' % (lo, step))
    print('static constexpr int kStarBvCount = %d;' % n)
    print('struct StarColourRow { double bv, T; unsigned char r, g, b; };')
    print('static const StarColourRow kStarColours[kStarBvCount] = {')
    for i in range(n):
        bv = lo + i * step; T = ballesteros(bv); c = srgb(T)
        print('    {%.2f, %.1f, %d, %d, %d},' % (bv, T, c[0], c[1], c[2]))
    print('};')
