"""Scratch driver for GRASP2018 single-level DHF (EOL) + RCI(Breit+QED) runs; prints total energies and wall time."""
import subprocess, os, sys, time, re, shutil
BIN = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'grasp', 'bin')
def run(prog, inp, cwd):
    t = time.time()
    p = subprocess.run([os.path.join(BIN, prog)], input=inp, text=True, cwd=cwd, capture_output=True)
    open(os.path.join(cwd, prog + '.stdout'), 'a').write(p.stdout + p.stderr)
    if p.returncode: print(p.stdout[-2000:], p.stderr[-2000:]); raise SystemExit(prog + ' failed')
    return time.time() - t, p.stdout
def level(work, Z, A, core, conf, active, j2, nexc=0, vary='*', spec='*', root=1, extra=None, rci=True, name='lv'):
    os.makedirs(work, exist_ok=True); t0 = time.time()
    run('rnucleus', f'{Z}\n{A}\nn\n{A}\n0\n0\n0\n', work)
    lists = conf if isinstance(conf, list) else [conf]
    s = '*\n%d\n' % core
    for k, c in enumerate(lists):
        s += f'{c}\n\n{active}\n{j2},{j2}\n{nexc}\n' + ('y\n' if k < len(lists) - 1 else 'n\n')
    run('rcsfgenerate', s, work); shutil.copy(os.path.join(work, 'rcsf.out'), os.path.join(work, 'rcsf.inp'))
    ncsf = open(os.path.join(work, 'rcsf.inp')).read().count('\n')
    run('rangular', 'y\n', work)
    if extra: run('rwfnestimate', f'y\n1\n{extra}\n*\n3\n*\n', work)
    else: run('rwfnestimate', 'y\n3\n*\n', work)
    tm, out = run('rmcdhf_mem', f'y\n{root}\n{vary}\n{spec}\n200\n', work)
    run('rsave', name + '\n', work) if False else subprocess.run([os.path.join(BIN, 'rsave'), name], cwd=work, capture_output=True)
    E = None
    if rci:
        tc, out = run('rci', f'y\n{name}\ny\ny\n1.d-6\ny\nn\nn\ny\n4\n{root}\n', work)
    lv = subprocess.run([os.path.join(BIN, 'rlevels'), name + ('.cm' if rci else '.m')], cwd=work, capture_output=True, text=True).stdout
    return time.time() - t0, ncsf, lv
if __name__ == '__main__':
    pass
