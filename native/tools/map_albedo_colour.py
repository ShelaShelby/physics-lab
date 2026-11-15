# 0.26: the measured colours of the planets (src/render/planet_maps.cpp measuredColour; Venus is drawn in it, the others are checks).
# A planet's colour in sunlight from its measured geometric albedos (Mallama, Krobusek & Pavlov 2017, Icarus 282, 19, arXiv:1609.05048,
# Table 7): all 12 bands together, Johnson U B V R I, Cousins Rc Ic and Sloan u' g' r' i' z', at the effective wavelengths of the paper's
# Tables 1 and 2; the albedo spectrum is interpolated linearly between them (held flat outside), turned into CIE 1931 XYZ under
# illuminant D65 (sunlight seen as white, as an sRGB screen's white), then sRGB. Needs numpy and colour-science.
import numpy as np, colour
WL = [360, 436, 549, 700, 900, 641, 798, 355, 469, 616, 748, 893]   # U B V R I Rc Ic u' g' r' i' z' (nm)
A = {   # Table 7: U B V R I Rc Ic | u' g' r' i' z'
    'Mercury': [0.087, 0.105, 0.142, 0.172, 0.208, 0.158, 0.180, 0.095, 0.130, 0.169, 0.200, 0.237],
    'Venus':   [0.348, 0.658, 0.689, 0.708, 0.584, 0.658, 0.640, 0.326, 0.664, 0.712, 0.708, 0.630],
    'Earth':   [0.688, 0.512, 0.434, 0.418, 0.430, 0.392, 0.396, 0.722, 0.497, 0.388, 0.393, 0.412],
    'Mars':    [0.060, 0.088, 0.170, 0.288, 0.330, 0.250, 0.285, 0.061, 0.111, 0.245, 0.298, 0.325],
    'Jupiter': [0.358, 0.443, 0.538, 0.495, 0.321, 0.513, 0.389, 0.377, 0.509, 0.575, 0.456, 0.348],
    'Saturn':  [0.203, 0.339, 0.499, 0.568, 0.423, 0.646, 0.543, 0.209, 0.436, 0.618, 0.601, 0.450],
    'Uranus':  [0.502, 0.561, 0.488, 0.202, 0.079, 0.264, 0.089, 0.541, 0.559, 0.355, 0.120, 0.056],
    'Neptune': [0.578, 0.562, 0.442, 0.181, 0.067, 0.226, 0.072, 0.564, 0.533, 0.307, 0.101, 0.048]}
shape = colour.SpectralShape(360, 780, 5); g5 = np.arange(360, 781, 5.0)
cm = colour.MSDS_CMFS['CIE 1931 2 Degree Standard Observer'].copy().align(shape).values
d6 = colour.SDS_ILLUMINANTS['D65'].copy().align(shape).values
o = np.argsort(WL)
for name, a in A.items():
    s = np.interp(g5, np.array(WL, float)[o], np.array(a)[o])
    XYZ = (s[:, None] * d6[:, None] * cm).sum(0) / (d6 * cm[:, 1]).sum()
    rgb = np.round(np.clip(colour.XYZ_to_sRGB(XYZ), 0, 1) * 255).astype(int)
    print(f'{{"{name}", {rgb[0]}, {rgb[1]}, {rgb[2]}}},   // #{"".join(f"{v:02x}" for v in rgb)}  Y {XYZ[1]:.3f}')
