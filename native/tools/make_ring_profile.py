# 0.51.2: Saturn's rings as Cassini measured them. Writes src/orbit/saturn_profile.inc. Only the standard library:
#   python3 native/tools/make_ring_profile.py > native/src/orbit/saturn_profile.inc
# Source: NASA PDS Ring-Moon Systems Node, volume COUVIS_8001, UVIS_HSP_2008_231_BETCEN_I_TAU10KM (.LBL and .TAB saved as received in data/rings/,
# from https://pds-rings.seti.org/holdings/volumes/COUVIS_8xxx/COUVIS_8001/data/): the Cassini UVIS High Speed Photometer's occultation of the star
# beta Centauri on 18 Aug 2008 (day 231, orbit 81, ingress), 110-190 nm, binned to 10 km of ring-plane radius, 72,833 to 151,675 km; the ring seen at
# 66.7 deg elevation, so even the B ring is measured up to the "highest detectable opacity" 10.73 (Colwell et al. 2010, Icarus 205, 1).
# Columns used: 1 ring radius (km), 5 normal optical depth, 6 maximum detectable normal optical depth (25 % counting error).
# Kept: tau as given, except that noise below 0 is set to 0 and a value above the maximum detectable is set to that maximum (a lower bound: 2 bins).
import os
here = os.path.dirname(os.path.abspath(__file__))
f = os.path.join(here, '..', 'data', 'rings', 'UVIS_HSP_2008_231_BETCEN_I_TAU10KM.TAB')
rows = [l.split(',') for l in open(f)]
assert len(rows) == 7884                                  # the label's ROWS
r = [float(x[0]) for x in rows]; t = [float(x[4]) for x in rows]; tm = [float(x[5]) for x in rows]
assert abs(r[0] - 72833.00) < 1e-6 and abs(r[-1] - 151675.00) < 0.01 and all(b > a for a, b in zip(r, r[1:]))
tau = [min(max(a, 0.0), m) for a, m in zip(t, tm)]
print('// Made by tools/make_ring_profile.py (0.51.2) from data/rings/UVIS_HSP_2008_231_BETCEN_I_TAU10KM.TAB (Cassini UVIS, beta Cen, 18 Aug 2008). Do not edit by hand.')
print('constexpr int kSatProfileN = %d;' % len(r))
print('constexpr float kSatProfileKm[kSatProfileN] = {' + ', '.join('%.2f' % v for v in r) + '};')
print('constexpr float kSatProfileTau[kSatProfileN] = {' + ', '.join('%.4g' % v for v in tau) + '};')
