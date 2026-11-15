# 0.51.2: Earth's air in the Orbit Lab: the Rayleigh optical depth of the whole air column and the colour weights that turn a spectrum into linear sRGB.
# Writes src/render/rayleigh_data.inc. Only the standard library: python3 native/tools/make_rayleigh_data.py > native/src/render/rayleigh_data.inc
#   - Rayleigh optical depth tau(lambda) of the air above sea level: Bodhaine, Wood, Dutton & Slusser (1999), "On Rayleigh optical depth calculations",
#     J. Atmos. Oceanic Technol. 16, 1854: tau = sigma(lambda) P A / (m_a g), sigma = 24 pi^3 (n^2 - 1)^2 / (lambda^4 N_s^2 (n^2 + 2)^2) F_air,
#     n from Peck & Reeder (1972) with the CO2 term, F_air (King factor) from Bates (1984) depolarisations, g from List (1968).
#     The paper itself could not be reached from the session (journals.ametsoc.org refused); the formulas are ported from colour-science 0.4.7's
#     implementation of that paper (data/atmosphere/colour-science_v0.4.7_rayleigh.py, saved as received), and the port is checked against that
#     file's own example: tau(555 nm) = 0.0936290 at the equator (asserted below).
#     The app's values: P = 101325 Pa (sea level), 288.15 K, CO2 300 ppm, latitude 45 deg (g = 980.6 cm/s^2), colour-science's defaults apart from the latitude.
#   - the CIE 1931 2-degree observer: data/stars/colour-science_v0.4.7_cmfs.py (as tools/star_colour.py), sampled every 5 nm.
#   - the CIE D65 spectrum: data/atmosphere/colour-science_v0.4.7_illuminants_sds.py ("D65", 5 nm steps; saved as received). D65 is the white the
#     app's planet maps are balanced to (sRGB's white), so sunlight is taken as D65: a grey surface stays grey.
#   - XYZ -> linear sRGB: IEC 61966-2-1 matrix (as tools/star_colour.py).
# 0.52.1: the weights below are no longer written (the app makes them from cie1931_data.inc, tools/make_cie_data.py, in render/shading.h airWeight);
# they are still computed here as a check of that formula. Weights w_c(lambda) = sum_j M[c][j] D65(lambda) cmf_j(lambda) / sum D65 ybar: a spectrum f(lambda) (relative to sunlight) has linear sRGB channel
# c = sum_k w_c[k] f(lambda_k). Each channel's weights are scaled to sum to exactly 1 (they sum to 1 within 0.2 % before; the scale makes f = 1, white light, (1, 1, 1)).
import math, os, re
here = os.path.dirname(os.path.abspath(__file__))
data = os.path.join(here, '..', 'data')

def cmf():
    t = open(os.path.join(data, 'stars', 'colour-science_v0.4.7_cmfs.py'), encoding='utf-8').read()
    a = t.index('"CIE 1931 2 Degree Standard Observer": {', t.index('DATA_CMFS_STANDARD_OBSERVER'))
    b = t.index('}', a)
    rows = {int(m.group(1)): tuple(float(m.group(k)) for k in (2, 3, 4)) for m in re.finditer(r'(\d+): \(([-0-9.e+]+), ([-0-9.e+]+), ([-0-9.e+]+)\)', t[a:b])}
    assert rows[600][0] == 1.0622 and rows[555][1] == 1.0 and rows[450][2] == 1.77211
    return rows

def d65():
    t = open(os.path.join(data, 'atmosphere', 'colour-science_v0.4.7_illuminants_sds.py'), encoding='utf-8').read()
    a = t.index('"D65": {', t.index('DATA_ILLUMINANTS_CIE'))
    b = t.index('}', a)
    rows = {int(m.group(1)): float(m.group(2)) for m in re.finditer(r'(\d+): ([-0-9.e+]+),', t[a:b])}
    assert rows[560] == 100.0 and rows[300] == 0.0341   # D65 is 100 at 560 nm by definition; the table's first row
    return rows

AVOGADRO = 6.02214179e23   # the value colour-science's formulas use (CODATA 2006), kept so the port reproduces its example

def peck1972(wl_um):
    return (8060.51 + 2480990 / (132.274 - wl_um ** -2) + 17455.7 / (39.32957 - wl_um ** -2)) / 1e8 + 1

def n_air(wl_um, co2_ppm):
    c = co2_ppm * 1e-6
    return (1 + 0.54 * (c - 300e-6)) * (peck1972(wl_um) - 1) + 1

def f_air(wl_um, co2_ppm):
    n2 = 1.034 + 3.17e-4 / wl_um ** 2
    o2 = 1.096 + 1.385e-3 / wl_um ** 2 + 1.448e-4 / wl_um ** 4
    c = co2_ppm * 1e-4
    return (78.084 * n2 + 20.946 * o2 + 0.934 * 1 + c * 1.15) / (78.084 + 20.946 + 0.934 + c)

def tau(lam_nm, co2_ppm=300, T=288.15, P_pa=101325, lat_deg=0.0):
    wl_cm = lam_nm * 1e-7
    wl_um = lam_nm * 1e-3
    Ns = (AVOGADRO / 22.4141) * (273.15 / T) / 1000          # molecules per cm^3
    n = n_air(wl_um, co2_ppm)
    sigma = 24 * math.pi ** 3 * (n * n - 1) ** 2 / (wl_cm ** 4 * Ns ** 2 * (n * n + 2) ** 2) * f_air(wl_um, co2_ppm)
    ma = 15.0556 * co2_ppm * 1e-6 + 28.9595
    c2 = math.cos(2 * math.radians(lat_deg))
    g = 980.6160 * (1 - 0.0026373 * c2 + 0.0000059 * c2 * c2)  # sea level
    return sigma * (P_pa * 10) * AVOGADRO / (ma * g)

M = ((3.2406, -1.5372, -0.4986), (-0.9689, 1.8758, 0.0415), (0.0557, -0.2040, 1.0570))

if __name__ == '__main__':
    t555 = tau(555)
    assert 0.0936290 <= t555 < 0.0936291, t555                # colour-science's own example (rayleigh.py, rayleigh_optical_depth docstring: 0.0936290...)
    C, D = cmf(), d65()
    lams = list(range(380, 781, 5))
    taus = [tau(l, lat_deg=45.0) for l in lams]
    ysum = sum(D[l] * C[l][1] for l in lams)
    w = [[sum(M[c][j] * D[l] * C[l][j] for j in range(3)) / ysum for l in lams] for c in range(3)]
    sums = [sum(r) for r in w]
    assert all(abs(s - 1) < 2e-3 for s in sums), sums
    w = [[v / s for v in r] for r, s in zip(w, sums)]
    print('// Made by tools/make_rayleigh_data.py (0.51.2). Earth\'s air: Rayleigh optical depth of the whole column at sea level (Bodhaine et al. 1999, through')
    print('// colour-science 0.4.7\'s implementation; 101325 Pa, 288.15 K, CO2 300 ppm, latitude 45 deg), every 5 nm from 380 to 780 nm. Do not edit by hand.')
    print('// (0.52.1: the colour weights are made in render/shading.h from the app\'s one CIE table, cie1931_data.inc.)')
    print('constexpr int kAirN = %d;' % len(lams))
    print('constexpr double kAirLambdaNm0 = 380, kAirLambdaStepNm = 5;')
    print('constexpr double kAirTau[kAirN] = {' + ', '.join('%.7g' % v for v in taus) + '};')
