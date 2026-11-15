import g2, re, time
HC = 219474.63136314
def levels(lv): return [float(m.group(2)) for m in re.finditer(r'^\s+\d+\s+\d+\s+(\S+)\s+[+-]\s+(-\d+\.\d+)', lv, re.M)]
L = [(4, '4s,4p,4d,4f'), (5, '5s,5p,5d,5f')]
r = {}
for nm, ref, j2, act0 in [('g', ['3d(6,*)4s(2,*)'], 8, '4s,3p,3d'), ('z7D5', ['3d(6,*)4s(1,*)4p(1,*)'], 10, '4s,4p,3d'), ('a5F5', ['3d(7,*)4s(1,*)'], 10, '4s,3p,3d')]:
    dt, out, lv = g2.layered('fecv_' + nm, 26, 56, 3, ref, j2, '1', L, act0)
    r[nm] = levels(lv)[0]; print(nm, out, '%.1f s' % dt, r[nm], flush=True)
print('a5F5 %.1f  z7D5 %.1f' % ((r['a5F5'] - r['g']) * HC, (r['z7D5'] - r['g']) * HC))
