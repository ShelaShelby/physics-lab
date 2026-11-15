import g2, re
HC = 219474.63136314
def levels(lv): return [(m.group(1), float(m.group(2))) for m in re.finditer(r'^\s+\d+\s+\d+\s+(\S+)\s+[+-]\s+(-\d+\.\d+)', lv, re.M)]
L = [(4, '4s,4p,4d,4f'), (5, '5s,5p,5d,5f,5g'), (6, '6s,6p,6d,6f,6g')]
r = {}
for nm, ref, j2, root, act0 in [('s2', ['2s(2,i)2p(6,5)3s(2,*)'], 0, '1', '3s,2p'), ('sp1', ['2s(2,i)2p(6,5)3s(1,*)3p(1,*)'], 2, '1,2', '3s,3p'),
                                 ('sp0', ['2s(2,i)2p(6,5)3s(1,*)3p(1,*)'], 0, '1', '3s,3p'), ('sp2', ['2s(2,i)2p(6,5)3s(1,*)3p(1,*)'], 4, '1', '3s,3p')]:
    dt, out, lv = g2.layered('mgcv_' + nm, 12, 24, 1, ref, j2, root, L, act0)
    r[nm] = [e for j, e in levels(lv)]; print(nm, out, '%.1f s' % dt, r[nm], flush=True)
g = r['s2'][0]
print('3P0 %.1f 3P1 %.1f 3P2 %.1f 1P1 %.1f' % ((r['sp0'][0]-g)*HC, (r['sp1'][0]-g)*HC, (r['sp2'][0]-g)*HC, (r['sp1'][1]-g)*HC))
