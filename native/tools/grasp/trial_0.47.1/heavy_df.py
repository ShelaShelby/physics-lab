import g, re, sys, time
HC = 219474.63136314
def levels(lv): return [(m.group(1), float(m.group(2))) for m in re.finditer(r'^\s+\d+\s+\d+\s+(\S+)\s+[+-]\s+(-\d+\.\d+)', lv, re.M)]
which = sys.argv[1]
C = {'fe': [('g', 26, 56, 3, '3d(6,i)4s(2,i)', '4s,3p,3d', 8, '1'), ('o5', 26, 56, 3, '3d(6,i)4s(1,i)4p(1,i)', '4s,4p,3d', 10, '1')],
     'hg': [('g', 80, 202, 5, '4f(14,i)5d(10,i)6s(2,i)', '6s,5p,5d,4f', 0, '1'), ('o1', 80, 202, 5, '4f(14,i)5d(10,i)6s(1,i)6p(1,i)', '6s,6p,5d,4f', 2, '1,2')],
     'eu': [('g', 63, 153, 5, '4f(7,i)6s(2,i)', '6s,5p,5d,4f', 7, '1'), ('o9', 63, 153, 5, '4f(7,i)6s(1,i)6p(1,i)', '6s,6p,5d,4f', 9, '1')]}[which]
E0 = None
for nm, Z, A, core, c, act, j2, root in C:
    t = time.time()
    import subprocess, os, shutil
    # TF start, EOL on the given roots, then RCI (Breit + QED)
    w = f'{which}_{nm}'; os.makedirs(w, exist_ok=True)
    g.run('rnucleus', f'{Z}\n{A}\nn\n{A}\n0\n0\n0\n', w)
    g.run('rcsfgenerate', f'*\n{core}\n{c}\n\n{act}\n{j2},{j2}\n0\nn\n', w); shutil.copy(w + '/rcsf.out', w + '/rcsf.inp')
    n = open(w + '/rcsf.inp').read().count('(')
    g.run('rangular', 'y\n', w)
    if E0 is None: g.run('rwfnestimate', 'y\n2\n*\n', w)
    else: g.run('rwfnestimate', f'y\n1\n../{which}_g/lv.w\n*\n2\n*\n', w)
    _, so = g.run('rmcdhf_mem', f'y\n{root}\n' + ('1\n' if ',' in root else '') + '*\n*\n200\n', w); subprocess.run([g.BIN + '/rsave', 'lv'], cwd=w, capture_output=True); open(w + '/rmcdhf.out', 'w').write(so)
    g.run('rci', f'y\nlv\ny\ny\n1.d-6\ny\nn\nn\ny\n4\n{root}\n', w)
    lv = subprocess.run([g.BIN + '/rlevels', 'lv.cm'], cwd=w, capture_output=True, text=True).stdout
    es = [e for j, e in levels(lv)]
    if E0 is None: E0 = es[0]
    print(which, nm, 'CSFs~', n, '%.1f s' % (time.time() - t), ['%.1f' % ((e - E0) * HC) for e in es], flush=True)
