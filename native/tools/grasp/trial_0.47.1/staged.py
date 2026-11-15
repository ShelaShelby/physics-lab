import g, subprocess, shutil, os, re, sys, time
def staged(w, Z, A, core, conf, act, j2, root, steps, start='y\n2\n*\n', fromw=None):
    """rmcdhf in stages; each stage varies a list of orbitals, starting from the previous stage. Returns (ok, E, seconds)."""
    os.makedirs(w, exist_ok=True); t = time.time()
    g.run('rnucleus', f'{Z}\n{A}\nn\n{A}\n0\n0\n0\n', w)
    g.run('rcsfgenerate', f'*\n{core}\n{conf}\n\n{act}\n{j2},{j2}\n0\nn\n', w); shutil.copy(w+'/rcsf.out', w+'/rcsf.inp')
    g.run('rangular','y\n',w)
    g.run('rwfnestimate', f'y\n1\n{fromw}\n*\n2\n*\n' if fromw else start, w)
    ok = True; E = None
    for k, vary in enumerate(steps):
        if k: shutil.copy(w+'/rwfn.out', w+'/rwfn.inp')
        _, so = g.run('rmcdhf_mem', f'y\n{root}\n' + ('1\n' if ',' in root else '') + f'{vary}\n*\n300\n', w)
        open(w+f'/rmcdhf{k}.out','w').write(so)
        bad = 'Convergence not obtained' in so; E = re.findall(r'Energy = (\S+)', so)[-1:]
        print(w, k, vary, 'FAILED' if bad else 'ok', E, flush=True)
        if bad: ok = False
    subprocess.run([g.BIN+'/rsave','lv'],cwd=w,capture_output=True)
    return ok, E, time.time() - t
if __name__ == '__main__':
    core_orbs = '1s,2s,2p*,3s,3p*,3d*,4s,4p*,4d*,5s,5p*'
    print(staged('eu_s', 63, 153, 5, '4f(7,i)6s(2,i)', '6s,5p,5d,4f', 7, '1', [core_orbs, '4f*', '6s', '*']))
