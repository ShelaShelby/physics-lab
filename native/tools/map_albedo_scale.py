# 0.77.0: one brightness factor per map, so the app's shaded ball, seen fully lit, has the body's measured geometric albedo (V band).
# Usage: python3 native/tools/map_albedo_scale.py [native/assets/maps]   (needs numpy and Pillow; its output is saved as tools/map_albedo_scale.txt)
#
# The app's light (render/shading.h): a pixel's linear value is rho * f, rho the map's linear colour and f the light factor.
# Lambert: f = cos i. Fully lit (phase 0) cos i = cos e = mu, and the disc's mean of rho * mu, over the disc's area (dA_disc = mu dA),
# averaged over every direction the ball can be seen from, is (2/3) <rho> (<rho>: the map's mean over the ball's surface).
# Proof: the average over view directions of max(0, cos)^2 is 1/6, and the disc has area pi for a unit ball: (1/pi) <rho> 4 pi / 6 = 2/3 <rho>.
# For a uniform ball this is the textbook result p = (2/3) A (Lambert sphere): Russell 1916, ApJ 43, 173, and e.g. Hapke, Theory of Reflectance
# and Emittance Spectroscopy (2nd ed. 2012), the Lambert sphere's geometric albedo 2/3.
# The Moon (uLunar, McEwen's function): at phase 0, g = 1 and f = 2 cos i / (cos i + cos e) = 1, the disc is flat: p = <rho>.
# The V band is taken as the map's luminance Y (linear sRGB, Rec. 709 / IEC 61966-2-1 weights 0.2126, 0.7152, 0.0722). Y and V both peak near 550 nm.
# A factor that pushes a pixel above 1 in a channel: the pixel is divided by its largest channel (its hue kept, its brightness clipped; kPlanetFS does
# the same, render/shading.h albedoScaled). The clip loses light, so the factor is then raised (bisection) until the clipped ball gives p again.
# Where no factor can (p above the ceiling of the clipped map; any Lambert ball is at most 2/3), the plain factor p / (g <Y>) is kept and the ball is darker
# than measured: the shortfall is printed and stated in the app.
# Earth: the measured albedo is of the whole planet, clouds and air included. Blue Marble is the ground alone, so its factor is 1 (Earth's check is the
# test of its full disc with clouds and air, tests/test_orbit.cpp).
import sys, os
import numpy as np
from PIL import Image

D = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), '..', 'assets', 'maps')
# body: (map file, geometric albedo V, source, law)
P = {
    'Mercury':  ('mercury',  0.142, 'Mallama et al. 2017 Table 7 V', 'lambert'),
    'Earth':    ('earth',    0.434, 'Mallama et al. 2017 Table 7 V', 'lambert'),
    'Mars':     ('mars',     0.170, 'Mallama et al. 2017 Table 7 V', 'lambert'),
    'Jupiter':  ('jupiter',  0.538, 'Mallama et al. 2017 Table 7 V', 'lambert'),
    'Saturn':   ('saturn',   0.499, 'Mallama et al. 2017 Table 7 V', 'lambert'),
    'Uranus':   ('uranus',   0.488, 'Mallama et al. 2017 Table 7 V', 'lambert'),
    'Neptune':  ('neptune',  0.442, 'Mallama et al. 2017 Table 7 V', 'lambert'),
    'Moon':     ('moon',     0.12,  'NSSDC Moon fact sheet', 'lunar'),
    'Pluto':    ('pluto',    0.52,  'NSSDC Pluto fact sheet', 'lambert'),
    'Charon':   ('charon',   0.42,  'NSSDC Pluto fact sheet (Charon)', 'lambert'),
    'Io':       ('io',       0.62,  'NSSDC Jovian satellite fact sheet', 'lambert'),
    'Europa':   ('europa',   0.68,  'NSSDC Jovian satellite fact sheet', 'lambert'),
    'Ganymede': ('ganymede', 0.44,  'NSSDC Jovian satellite fact sheet', 'lambert'),
    'Callisto': ('callisto', 0.19,  'NSSDC Jovian satellite fact sheet', 'lambert'),
    'Rhea':     ('rhea',     0.7,   'NSSDC Saturnian satellite fact sheet', 'lambert'),
    'Triton':   ('triton',   0.72,  'NSSDC Neptunian satellite fact sheet', 'lambert'),
    'Phobos':   ('phobos',   0.07,  'NSSDC Mars fact sheet (Phobos)', 'lambert'),
}
W709 = np.array([0.2126, 0.7152, 0.0722])

# 0.88.0: Pluto's bytes are LINEAR light, not sRGB: USGS's page says "The original 32bit pixel values were linearly stretched to 8bit (1-255) using the input range of
# 0.03344 to 0.99981" (saved data/maps/usgs_pluto_new_horizons_lorri_mvic_global_mosaic_300m_2026-11-23.html), so a byte is proportional to the measured value
# (render/planet_maps.h MapInfo::linear). Until 0.87.x it was decoded as sRGB, which made Pluto's map look darker in the middle and its mean 0.161 instead of 0.397.
LINEAR = {'pluto'}

def decode(c, linear=False):
    c = c / 255.0
    if linear: return c
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)

def solve(lin, w, p, g):
    W = lin.shape[1]
    def pk(k):
        c = lin * k
        mx = np.maximum(c.max(-1, keepdims=True), 1.0)
        return g * float((((c / mx) @ W709) * w).sum() / (w.sum() * W)), float((mx > 1).mean() * 100)
    m = pk(1.0)[0] / g
    k0 = p / (g * m)
    ceil = pk(1e9)[0]
    if ceil < p:
        return m, k0, k0, pk(k0)[0], pk(k0)[1], ceil
    lo, hi = min(k0, 1.0) * 0.5, k0
    while pk(hi)[0] < p: hi *= 1.5
    for _ in range(60):
        mid = 0.5 * (lo + hi)
        if pk(mid)[0] < p: lo = mid
        else: hi = mid
    return m, k0, hi, pk(hi)[0], pk(hi)[1], ceil

print('body       map mean Y  albedo p  law      plain k  factor   drawn p  clipped px %  ceiling  source')
for body, (f, p, src, law) in P.items():
    im = np.asarray(Image.open(os.path.join(D, f + '.jpg')).convert('RGB'), dtype=np.float64)
    H = im.shape[0]
    lin = decode(im, f in LINEAR)
    lat = 90 - (np.arange(H) + 0.5) * 180 / H
    w = np.cos(np.radians(lat))[:, None]
    g = 2.0 / 3.0 if law == 'lambert' else 1.0
    m, k0, k, pd, clip, ceil = solve(lin, w, p, g)
    if body == 'Earth': k, pd, clip = 1.0, g * m, 0.0
    print(f'{body:9s} {m:10.4f} {p:9.3f}  {law:7s} {k0:8.4f} {k:8.4f} {pd:8.4f} {clip:12.2f} {ceil:8.3f}  {src}')
