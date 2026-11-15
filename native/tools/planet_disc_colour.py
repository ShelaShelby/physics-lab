# 0.50 (review item 8): a body's colour in sunlight from its measured geometric albedos, as LINEAR sRGB ratios r/g and b/g (the check the OPAL maps
# used, assets/maps/README.md). Standard library only; reads the saved tables as received:
#   - albedos: Mallama, Krobusek & Pavlov 2017, Icarus 282, 19, arXiv:1609.05048, Table 7 (data/planets/mallama_2017_arxiv1609.05048.pdf),
#     the 12 bands at the effective wavelengths of its Tables 1 and 2 (as tools/map_albedo_colour.py); interpolated linearly, flat outside
#   - CIE 1931 2-degree observer: data/stars/colour-science_v0.4.7_cmfs.py; illuminant D65: data/planets/colour-science_v0.4.7_illuminant_sds.py
#     (sunlight seen as white, as an sRGB screen's white); 360-780 nm in 5 nm steps; XYZ -> linear sRGB with the IEC 61966-2-1 matrix.
# python3 tools/planet_disc_colour.py Mercury
import os, re, sys
here = os.path.dirname(os.path.abspath(__file__)); data = os.path.join(here, '..', 'data')
def table(path, key, start_marker):
    t = open(path, encoding='utf-8').read()
    a = t.index(key, t.index(start_marker)); b = t.index('}', a)
    return t[a:b]
def cmf():
    blk = table(os.path.join(data, 'stars', 'colour-science_v0.4.7_cmfs.py'), '"CIE 1931 2 Degree Standard Observer": {', 'DATA_CMFS_STANDARD_OBSERVER')
    return {int(m.group(1)): tuple(float(m.group(k)) for k in (2, 3, 4)) for m in re.finditer(r'(\d+): \(([-0-9.e+]+), ([-0-9.e+]+), ([-0-9.e+]+)\)', blk)}
def d65():
    t = open(os.path.join(data, 'planets', 'colour-science_v0.4.7_illuminant_sds.py'), encoding='utf-8').read()
    a = t.index('"D65": {'); b = t.index('}', a)
    return {int(float(m.group(1))): float(m.group(2)) for m in re.finditer(r'([\d.]+): ([-0-9.e+]+),', t[a:b])}
WL = [360, 436, 549, 700, 900, 641, 798, 355, 469, 616, 748, 893]   # U B V R I Rc Ic u' g' r' i' z' (nm)
ALB = {'Mercury': [0.087, 0.105, 0.142, 0.172, 0.208, 0.158, 0.180, 0.095, 0.130, 0.169, 0.200, 0.237]}   # Table 7
M = ((3.2406, -1.5372, -0.4986), (-0.9689, 1.8758, 0.0415), (0.0557, -0.2040, 1.0570))
def interp(x, xs, ys):
    if x <= xs[0]: return ys[0]
    if x >= xs[-1]: return ys[-1]
    for i in range(len(xs) - 1):
        if xs[i] <= x <= xs[i + 1]: return ys[i] + (ys[i + 1] - ys[i]) * (x - xs[i]) / (xs[i + 1] - xs[i])
def linear_rgb(alb):
    o = sorted(range(len(WL)), key=lambda i: WL[i]); xs = [WL[i] for i in o]; ys = [alb[i] for i in o]
    c, s = cmf(), d65()
    X = Y = Z = N = 0.0
    for l in range(360, 781, 5):
        a = interp(l, xs, ys); x, y, z = c[l]; e = s[l]
        X += a * e * x; Y += a * e * y; Z += a * e * z; N += e * y
    X, Y, Z = X / N, Y / N, Z / N
    return [r[0] * X + r[1] * Y + r[2] * Z for r in M]
if __name__ == '__main__':
    rgb = linear_rgb(ALB[sys.argv[1]])
    print('%s linear sRGB %.4f %.4f %.4f  r/g %.4f  b/g %.4f' % (sys.argv[1], *rgb, rgb[0] / rgb[1], rgb[2] / rgb[1]))
