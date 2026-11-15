# 0.52.1: the app's one copy of the CIE 1931 2-degree standard observer and the CIE D65 spectrum (used by Earth's air in the Orbit Lab, render/shading.h,
# and by the spectral-line colours of the Atom Lab, atom_core.cpp lamColor). Writes src/render/cie1931_data.inc. Only the standard library:
#   python3 native/tools/make_cie_data.py > native/src/render/cie1931_data.inc
#   - CIE 1931 2-degree observer, 1 nm from 360 to 830 nm: data/stars/colour-science_v0.4.7_cmfs.py ("CIE 1931 2 Degree Standard Observer", saved as received;
#     the same table tools/star_colour.py and tools/make_rayleigh_data.py read).
#   - CIE D65, 5 nm from 380 to 780 nm: data/atmosphere/colour-science_v0.4.7_illuminants_sds.py ("D65", saved as received).
# The values are printed exactly as the files give them.
import os, re
here = os.path.dirname(os.path.abspath(__file__))
data = os.path.join(here, '..', 'data')

def table(path, start, key, pat):
    t = open(os.path.join(data, path), encoding='utf-8').read()
    a = t.index(key, t.index(start))
    b = t.index('}', a)
    return {int(m.group(1)): m.groups()[1:] for m in re.finditer(pat, t[a:b])}

C = table(os.path.join('stars', 'colour-science_v0.4.7_cmfs.py'), 'DATA_CMFS_STANDARD_OBSERVER', '"CIE 1931 2 Degree Standard Observer": {',
          r'(\d+): \(([-0-9.e+]+), ([-0-9.e+]+), ([-0-9.e+]+)\)')
D = table(os.path.join('atmosphere', 'colour-science_v0.4.7_illuminants_sds.py'), 'DATA_ILLUMINANTS_CIE', '"D65": {', r'(\d+): ([-0-9.e+]+),')
assert sorted(C) == list(range(360, 831)) and float(C[600][0]) == 1.0622 and float(C[555][1]) == 1.0 and float(C[450][2]) == 1.77211
assert all(l in D for l in range(380, 781, 5)) and float(D[560][0]) == 100.0
print('// Made by tools/make_cie_data.py (0.52.1). Do not edit by hand. CIE 1931 2-degree standard observer (xbar, ybar, zbar), 1 nm, 360-830 nm, and the')
print('// CIE D65 spectrum, 5 nm, 380-780 nm, both from colour-science 0.4.7 (data/stars, data/atmosphere; saved as received).')
print('inline constexpr int kCmfN = 471;')
print('inline constexpr double kCmfNm0 = 360, kCmfStepNm = 1;')
print('inline constexpr double kCmf[kCmfN][3] = {')
for l in range(360, 831): print('    {%s, %s, %s},' % C[l])
print('};')
print('inline constexpr int kD65N = 81;')
print('inline constexpr double kD65Nm0 = 380, kD65StepNm = 5;')
print('inline constexpr double kD65[kD65N] = {' + ', '.join(D[l][0] for l in range(380, 781, 5)) + '};')
