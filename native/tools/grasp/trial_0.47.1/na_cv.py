import g2, re
HC = 219474.63136314
def levels(lv): return [(m.group(1), float(m.group(2))) for m in re.finditer(r'^\s+\d+\s+\d+\s+(\S+)\s+[+-]\s+(-\d+\.\d+)', lv, re.M)]
L = [(4, '4s,4p,4d,4f'), (5, '5s,5p,5d,5f,5g'), (6, '6s,6p,6d,6f,6g')]
r = {}
for nm, ref, j2, root in [('s', ['2s(2,i)2p(6,5)3s(1,*)'], 1, '1'), ('p', ['2s(2,i)2p(6,5)3p(1,*)'], 1, '1'), ('p3', ['2s(2,i)2p(6,5)3p(1,*)'], 3, '1')]:
    act0 = '3s,2p' if nm == 's' else '2s,3p'
    dt, out, lv = g2.layered('nacv_' + nm, 11, 23, 1, ref, j2, root, L, act0)
    r[nm] = levels(lv)[0][1]; print(nm, out, '%.1f s' % dt, r[nm], flush=True)
print('3p1/2 %.1f  3p3/2 %.1f cm-1' % ((r['p'] - r['s']) * HC, (r['p3'] - r['s']) * HC))
