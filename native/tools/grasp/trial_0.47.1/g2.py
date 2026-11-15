"""Layered MCDHF (core-valence) for one J/parity block; each layer adds n orbitals of l<=lmax, varies only them."""
import g, os, re, shutil, subprocess, time
def layered(work, Z, A, core, ref, j2, root, layers, df_active, nexc=2):
    os.makedirs(work, exist_ok=True); T = time.time(); out = []
    g.run('rnucleus', f'{Z}\n{A}\nn\n{A}\n0\n0\n0\n', work)
    def gen(lst, act, ne):
        s = '*\n%d\n' % core
        for k, c in enumerate(lst):
            s += f'{c}\n\n{act}\n{j2},{j2}\n{ne}\n' + ('y\n' if k < len(lst) - 1 else 'n\n')
        g.run('rcsfgenerate', s, work); shutil.copy(work + '/rcsf.out', work + '/rcsf.inp')
        return open(work + '/rcsf.inp').read().count('(')
    # DF
    gen([r.replace('*', 'i').replace(',5)', ',i)').replace(',1)', ',i)') for r in ref], df_active, 0)
    g.run('rangular', 'y\n', work); g.run('rwfnestimate', 'y\n2\n*\n', work)
    g.run('rmcdhf_mem', f'y\n{root}\n' + ('1\n' if ',' in root else '') + '*\n*\n200\n', work); subprocess.run([g.BIN + '/rsave', 'df'], cwd=work, capture_output=True)
    prev = 'df'
    for n, act in layers:
        nc = gen(ref, act, nexc)
        g.run('rangular', 'y\n', work)
        g.run('rwfnestimate', f'y\n1\n{prev}.w\n*\n3\n*\n', work)
        t = time.time(); g.run('rmcdhf_mem', f'y\n{root}\n' + ('1\n' if ',' in root else '') + f'{n}*\n\n200\n', work)
        nm = f'l{n}'; subprocess.run([g.BIN + '/rsave', nm], cwd=work, capture_output=True); prev = nm
        out.append((n, nc, time.time() - t))
    t = time.time(); g.run('rci', f'y\n{prev}\ny\ny\n1.d-6\ny\nn\nn\ny\n4\n{root}\n', work)
    lv = subprocess.run([g.BIN + '/rlevels', prev + '.cm'], cwd=work, capture_output=True, text=True).stdout
    return time.time() - T, out, lv
