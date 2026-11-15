import g, re, sys
HC = 219474.63136314
def E(lv):  # total energy (a.u.) of the first level in the rlevels table
    m = re.search(r'^\s+1\s+1\s+\S+\s+[+-]\s+(-\d+\.\d+)', lv, re.M); return float(m.group(1))
cases = [('3s', '3s(1,i)', '3s,2p', 1), ('3p1', '3p(1,i)', '2s,3p', 1), ('3p3', '3p(1,i)', '2s,3p', 3),
         ('4s', '4s(1,i)', '4s,2p', 1), ('3d3', '3d(1,i)', '2s,2p,3d', 3), ('3d5', '3d(1,i)', '2s,2p,3d', 5), ('4p1', '4p(1,i)', '2s,4p', 1)]
res = {}
for nm, c, act, j2 in cases:
    dt, n, lv = g.level('na_' + nm, 11, 23, 2, c, act, j2)
    res[nm] = E(lv); print(nm, n, '%.2f s' % dt, res[nm], '%.2f cm-1' % ((res[nm] - res['3s']) * HC), flush=True)
