#!/usr/bin/env python3
"""GRASP2018 driver for the one-outer-electron atoms (physics-lab native 0.47.2). Computes the levels of one series (one l and one J: e.g. Na 3s, 4s, 5s in J = 1/2)
in one run and writes what the app needs.

    python3 native/tools/grasp/run_level.py Na [s1 p1 p3 d3 d5] [--grasp $HOME/grasp] [--work $HOME/gwork]

Method (PLAN_JUMPS.md section 2; the recipe that put Na 3p within +0.2 % of NIST in 0.47.1, data/jumps/trial_runs.tsv):
  1. Dirac-Fock (DF): the configurations core + n l (every n of the series), orbitals from Thomas-Fermi (or screened hydrogenic, or staged: core first), the lowest roots of the
     J block optimized together (extended optimal level, EOL), so the excited orbitals are orthogonal to the lower ones of the series.
  2. Core-valence MCDHF in layers: single and double excitations from those configurations with at most one hole in the outer p shell of the core (1s for Li), into a layer of new
     correlation orbitals (one more of each l = s..g) per step; each layer varies only its new orbitals (not spectroscopic: no node condition), EOL over the same roots.
  3. RCI with the Breit interaction and QED (vacuum polarization, self-energy) on the last layer.
  4. rmultipole (tools/grasp/multipoles): the even multipoles of every level's density; multipoles.py makes natural orbitals, occupations and rho_k(r).
GRASP programs exit with status 0 even when rmcdhf fails: every rmcdhf output is checked for "Convergence not obtained" and a failure raises (with up to 3 start strategies).
"""
import argparse, json, os, re, shutil, subprocess, sys, time
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
LET = 'spdfg'
# Z, mass number of the nucleus (GRASP's Fermi nucleus with Angeli's default radius), closed core given to rcsfgenerate (0 none, 1 He, 2 Ne, 3 Ar, 4 Kr, 5 Xe),
# the rest of the core: inactive shells and the outer p shell (or Li 1s) with at most one hole
ATOMS = {
    'Li': (3, 7, 0, '1s(2,1)'),
    'Na': (11, 23, 1, '2s(2,i)2p(6,5)'),
    'K': (19, 39, 2, '3s(2,i)3p(6,5)'),
    'Rb': (37, 85, 3, '3d(10,i)4s(2,i)4p(6,5)'),
    'Cs': (55, 133, 4, '4d(10,i)5s(2,i)5p(6,5)'),
    'Fr': (87, 223, 5, '4f(14,i)5d(10,i)6s(2,i)6p(6,5)'),
}
CLOSED = {0: [], 1: ['1s'], 2: ['1s', '2s', '2p'], 3: ['1s', '2s', '2p', '3s', '3p'], 4: ['1s', '2s', '2p', '3s', '3p', '3d', '4s', '4p'],
          5: ['1s', '2s', '2p', '3s', '3p', '3d', '4s', '4p', '4d', '5s', '5p']}
LAYERS = 3
# levels computed with a series only to give its wanted levels a good basis, not used by the app: francium's 9s (with only 7s and 8s, 8s came out 31 % off;
# with 9s in the run 8s is +0.9 %, while 9s itself stays 11-20 % off and is left out of the app)
EXTRA = {('Fr', 's1'): [9]}
HC = 219474.6313632   # cm-1 per hartree (CODATA 2018)
MPI_PROGS = {'rangular', 'rmcdhf_mem', 'rci', 'rtransition', 'rbiotransform'}


def series_of(levels):
    """{'s1': [3, 4, 5], 'p1': [3, 4], ...} from [(n, l, j2)]"""
    out = {}
    for n, l, j2 in levels: out.setdefault(f'{LET[l]}{j2}', []).append(n)
    return {k: sorted(v) for k, v in out.items()}


class Run:
    def __init__(self, grasp, work, log):
        self.bin = os.path.join(grasp, 'bin'); self.work = work; self.log = log
        self.np = int(os.environ.get('GRASP_NP', '1'))   # set to 1 for the small Dirac-Fock lists (the MPI rmcdhf aborts on a one-CSF list)
        os.makedirs(work, exist_ok=True)

    def __call__(self, prog, inp, args=()):
        # GRASP_NP=N (native 0.47.4): the programs that have an MPI version run as N processes (mpirun; scratch files in <work>/mpitmp, MPI_TMP)
        np_ = self.np
        if np_ > 1 and prog in MPI_PROGS:
            tmp = os.path.join(self.work, 'mpitmp'); os.makedirs(tmp, exist_ok=True)
            cmd = ['mpirun', '-np', str(np_), '--allow-run-as-root', '--oversubscribe', os.path.join(self.bin, prog + '_mpi'), *args]
            env = dict(os.environ, MPI_TMP=tmp)
        else:
            cmd = [os.path.join(self.bin, prog), *args]; env = None
        p = subprocess.run(cmd, input=inp, text=True, cwd=self.work, capture_output=True, env=env)
        with open(os.path.join(self.work, 'driver.log'), 'a') as f: f.write(f'==== {prog}\n{inp}\n---- stdout\n{p.stdout}\n---- stderr\n{p.stderr[-4000:]}\n')
        if p.returncode: raise RuntimeError(f'{prog} exit {p.returncode} in {self.work}: {p.stderr[-500:]}')
        return p.stdout


def shells(pre, closed):
    """max n per l of the core: closed shells plus the shells written in pre"""
    mx = {}
    for s in CLOSED[closed] + re.findall(r'(\d+[spdfg])\(', pre):
        l = LET.index(s[-1]); mx[l] = max(mx.get(l, 0), int(s[:-1]))
    return mx


def active(mx):
    return ','.join(f'{mx[l]}{LET[l]}' for l in sorted(mx))


def gen(run, closed, refs, act, j2, nexc):
    s = '*\n%d\n' % closed + ''.join(r + '\n' for r in refs) + f'\n{act}\n{j2},{j2}\n{nexc}\nn\n'
    run('rcsfgenerate', s)
    shutil.copy(os.path.join(run.work, 'rcsf.out'), os.path.join(run.work, 'rcsf.inp'))
    return sum(1 for line in open(os.path.join(run.work, 'rcsf.inp')) if line.lstrip().startswith(('1', '2', '3', '4', '5', '6', '7', '8', '9')) and '(' in line)


def mcdhf(run, roots, vary, spec, cycles=200, tag=''):
    inp = f'y\n{roots}\n' + ('1\n' if ',' in roots else '') + f'{vary}\n{spec}\n{cycles}\n'
    out = run('rmcdhf_mem', inp)
    open(os.path.join(run.work, f'rmcdhf{tag}.out'), 'w').write(out)
    if 'Convergence not obtained' in out or 'onvergence not' in out: raise RuntimeError(f'rmcdhf did not converge ({tag}) in {run.work}')
    # it can also run away without saying so (Cs 5d5/2, first layer: -4e8 hartree, then NaN): the energies must be finite and, in a correlation layer, the last within 1 hartree of the first
    e = [float(x.replace('D', 'E')) if 'NaN' not in x else float('nan') for x in re.findall(r'Level +\d+ +Energy = +(\S+)', out)]
    if not e or any(x != x for x in e) or (tag.startswith('_L') and abs(e[-1] - e[0]) > 1.0): raise RuntimeError(f'rmcdhf ran away ({tag}) in {run.work}: {e[:1]} -> {e[-1:]}')
    return out


def gfiles_mod():
    import gfiles
    return gfiles


def write_start(work, n, l, j2):
    import numpy as np
    import gfiles
    kap = -(l + 1) if j2 == 2 * l + 1 else l
    r = np.concatenate([[0.0], np.exp(np.linspace(np.log(1e-6), np.log(600.0), 4000))])
    P, Q, e = gfiles.hydrogenic(n, kap, 1.0, r)
    gfiles.write_w(os.path.join(work, 'start.w'), [(n, kap, e, r, P, Q)])


def merge_series(work, first):
    """series.w = the orbitals so far plus this stage's new valence orbital, made orthogonal to the lower orbitals of its kappa"""
    import gfiles
    out = gfiles.read_w(os.path.join(work, 'rwfn.out'))
    if first:
        gfiles.write_w(os.path.join(work, 'series.w'), out); return
    old = gfiles.read_w(os.path.join(work, 'series.w'))
    have = {(o[0], o[1]) for o in old}
    for o in out:
        if (o[0], o[1]) in have: continue
        n, kap, e, r, P, Q, p0 = o
        for m in old:
            if m[1] != kap: continue
            # 0.51.3: a longer older orbital was cut to this one's length (see run_shared.ci_orbitals); this one is extended onto its grid first
            if len(m[4]) > len(P): r, P, Q = m[3], gfiles.pad(P, len(m[4])), gfiles.pad(Q, len(m[4]))
            ov = gfiles.overlap(m, (n, kap, e, r, P, Q))
            P = P - ov * gfiles.pad(m[4], len(P)); Q = Q - ov * gfiles.pad(m[5], len(Q))
        nrm = np.sqrt(gfiles.overlap((n, kap, e, r, P, Q), (n, kap, e, r, P, Q)))
        old.append((n, kap, e, r, P / nrm, Q / nrm, p0 / nrm))
    gfiles.write_w(os.path.join(work, 'series.w'), old)


def run_series(X, key, ns, grasp, work, layers=LAYERS):
    """Runs one series; returns a dict (CSF counts, seconds per stage, strategy). Raises when every strategy fails."""
    Z, A, closed, pre = ATOMS[X]
    l = LET.index(key[0]); j2 = int(key[1:])
    ns = list(ns) + EXTRA.get((X, key), [])
    roots = ','.join(str(i + 1) for i in range(len(ns)))
    run = Run(grasp, os.path.join(work, X, key), None)
    if os.path.isdir(run.work): shutil.rmtree(run.work)
    os.makedirs(run.work)
    T0 = time.time(); info = {'atom': X, 'series': key, 'n': ns, 'roots': roots, 'stages': []}
    run('rnucleus', f'{Z}\n{A}\nn\n{A}\n0\n0\n0\n')
    mx = shells(pre, closed)
    mx[l] = max(mx.get(l, 0), max(ns))
    refs_df = [pre.replace(',5)', ',i)').replace(',1)', ',i)') + f'{n}{LET[l]}(1,i)' for n in ns]
    refs = [pre + f'{n}{LET[l]}(1,*)' for n in ns]
    nist = {(a, b, c): cm for a, b, c, cm in levels_mod().nist_levels(X)}
    # 1. DF, one valence orbital at a time: the lowest level of the series first (every orbital varied: the core relaxes for it); then each higher n alone, in its own
    #    one-configuration run with the core fixed and only its orbital varied (spectroscopic: its node count is n - l - 1), made orthogonal to the lower ones of
    #    the series (Gram-Schmidt, series.w). Optimizing the higher roots of a multi-configuration list instead failed for Na 4d, 5s (0.47.2 trials).
    errs = []
    for i, n in enumerate(ns):
        ncsf = gen(run, closed, [refs_df[i]], active({**mx, l: n}), j2, 0)
        run('rangular', 'y\n')
        vary = '*' if i == 0 else f'{n}{LET[l]}' + ('*' if l else '')
        sib = [d for d in sorted(os.listdir(os.path.dirname(run.work))) if d != key and '_' not in d and os.path.exists(os.path.join(os.path.dirname(run.work), d, 'series.w'))]
        for strat in (('h1', 'tf', 'hydrogenic') if i else ('tf', 'hydrogenic', 'sibling', 'h1')):
            try:
                if strat == 'sibling':   # the core from another series' converged Dirac-Fock (Cs 5d5/2: Thomas-Fermi made the core's 4s fail), the rest Thomas-Fermi
                    if not sib: raise RuntimeError('no other series yet')
                    run('rwfnestimate', f'y\n1\n../{sib[0]}/series.w\n*\n2\n*\n')
                elif strat == 'h1':    # the new valence orbital from a hydrogen-like orbital of charge 1 (what the outer electron sees far out): GRASP's own starts fail for diffuse ones
                    write_start(run.work, n, l, j2)
                    run('rwfnestimate', f'y\n1\nstart.w\n*\n' + (f'1\nseries.w\n*\n' if i else '2\n*\n'))
                else:
                    est = {'tf': '2', 'hydrogenic': '3'}[strat]
                    run('rwfnestimate', f'y\n{est}\n*\n' if i == 0 else f'y\n1\nseries.w\n*\n{est}\n*\n')
                out = mcdhf(run, '1', vary, '*', 300, f'_df{i}')
                if i:   # a converged excited level must lie between the level below it and the ion's limit (Koopmans: lowest level + its valence orbital energy) ...
                    e = float(re.findall(r'Level  1    Energy = (\S+)', out)[-1].replace('D', 'E'))
                    if not (e_prev < e < e_ion): raise RuntimeError(f'DF for {n}{LET[l]} converged to a wrong solution: {e} outside ({e_prev}, {e_ion})')
                    # ... and its step from the lowest level of the series within 20 % of NIST's (only to reject a wrong root: GRASP counts a node only where the
                    # oscillation exceeds 1/20 of the largest, so for Fr 8s it took a 9s-like solution, 25 % off; Dirac-Fock alone is within 10 % for these atoms)
                    want = nist[(n, l, j2)] - nist[(ns[0], l, j2)] if (n, l, j2) in nist else 0
                    got = (e - e_first) * HC
                    if want > 0 and abs(got - want) > 0.2 * want: raise RuntimeError(f'DF for {n}{LET[l]}: step {got:.0f} cm-1 against NIST {want:.0f}: a wrong root')
                    e_prev = e
                else:
                    e_prev = e_first = float(re.findall(r'Level  1    Energy = (\S+)', out)[-1].replace('D', 'E'))
                    vo = [o for o in gfiles_mod().read_w(os.path.join(run.work, 'rwfn.out')) if o[0] == n and gfiles_mod().ell(o[1]) == l]
                    e_ion = e_prev + vo[0][2]
                info.setdefault('df_start', []).append(strat); break
            except RuntimeError as e:
                errs.append(str(e))
        else:
            if i == 0: raise RuntimeError(f'{X} {key}: DF failed for {n}{LET[l]} with every start: ' + ' | '.join(errs[-3:]))
            # a nearly hydrogen-like excited orbital (Na 4d: GRASP's solver diverges from every start, even from this one): keep the hydrogen-like orbital of charge 1,
            # made orthogonal to the lower ones; the correlation layers and the RCI work on top of it. Flagged in info.json and the summary.
            write_start(run.work, n, l, j2)
            st = gfiles_mod().read_w(os.path.join(run.work, 'start.w'))
            g = gfiles_mod().grasp_grid(gfiles_mod().read_w(os.path.join(run.work, 'series.w'))[0][3])
            gfiles_mod().write_w(os.path.join(run.work, 'rwfn.out'), [gfiles_mod().onto(st[0], g)])
            info.setdefault('df_start', []).append('hydrogen-like kept')
        merge_series(run.work, i == 0)
    info['stages'].append(('DF', len(ns), round(time.time() - T0, 1)))
    prev = 'series'
    # 2. layers of correlation orbitals
    lmax = 4
    for k in range(1, layers + 1):
        t = time.time()
        new = []
        for ll in range(lmax + 1):
            mx[ll] = max(mx.get(ll, ll), ll) + 1
            new.append(f'{mx[ll]}{LET[ll]}' + ('*' if ll else ''))
        ncsf = gen(run, closed, refs, active(mx), j2, 2)
        run('rangular', 'y\n')
        ok = False
        for strat in ('hydrogenic', 'tf'):
            try:
                run('rwfnestimate', f'y\n1\n{prev}.w\n*\n' + ('3' if strat == 'hydrogenic' else '2') + '\n*\n')
                mcdhf(run, roots, ','.join(new), '', 200, f'_L{k}')
                ok = True; break
            except RuntimeError as e:
                errs.append(str(e))
        if not ok: raise RuntimeError(f'{X} {key}: layer {k} failed: ' + ' | '.join(errs[-2:]))
        name = f'L{k}'; run('rsave', '', [name]); prev = name
        info['stages'].append((f'layer {k}', ncsf, round(time.time() - t, 1)))
    # 3. RCI with Breit and QED
    t = time.time()
    run('rci', f'y\n{prev}\ny\ny\n1.d-6\ny\nn\nn\ny\n4\n{roots}\n')
    info['stages'].append(('RCI', ncsf, round(time.time() - t, 1)))
    # 4. multipoles
    mp = os.path.join(grasp, 'bin', 'rmultipole')
    p = subprocess.run([mp], input=f'{prev}\ny\n', text=True, cwd=run.work, capture_output=True)
    if p.returncode or not os.path.exists(os.path.join(run.work, prev + '.mp')): raise RuntimeError(f'rmultipole failed: {p.stderr[-500:]}')
    info['final'] = prev; info['seconds'] = round(time.time() - T0, 1)
    json.dump(info, open(os.path.join(run.work, 'info.json'), 'w'))
    return info


def one_config(X, key, grasp, work):
    """the lowest level of a series as ONE configuration (Dirac-Fock, no correlation): kept for the tests (its shape is known in closed form). <work>/<X>/<key>_df0/df0.{w,mp}"""
    Z, A, closed, pre = ATOMS[X]
    l = LET.index(key[0]); j2 = int(key[1:]); n = series_of(levels_mod().app_levels(X))[key][0]
    run = Run(grasp, os.path.join(work, X, key + '_df0'), None)
    if os.path.isdir(run.work): shutil.rmtree(run.work)
    os.makedirs(run.work)
    run('rnucleus', f'{Z}\n{A}\nn\n{A}\n0\n0\n0\n')
    mx = shells(pre, closed); mx[l] = max(mx.get(l, 0), n)
    gen(run, closed, [pre.replace(',5)', ',i)').replace(',1)', ',i)') + f'{n}{LET[l]}(1,i)'], active(mx), j2, 0)
    run('rangular', 'y\n'); run('rwfnestimate', 'y\n2\n*\n')
    mcdhf(run, '1', '*', '*', 300, '_df0'); run('rsave', '', ['df0'])
    p = subprocess.run([os.path.join(grasp, 'bin', 'rmultipole')], input='df0\nn\n', text=True, cwd=run.work, capture_output=True)
    if p.returncode: raise RuntimeError('rmultipole failed')


def levels_mod():
    import levels
    return levels


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('atom'); ap.add_argument('series', nargs='*')
    ap.add_argument('--grasp', default=os.path.expanduser('~/grasp')); ap.add_argument('--work', default=os.path.expanduser('~/gwork'))
    ap.add_argument('--layers', type=int, default=LAYERS)
    ap.add_argument('--one-config', action='store_true', help='only the one-configuration test level of each series given')
    a = ap.parse_args()
    if a.one_config:
        for key in a.series: one_config(a.atom, key, a.grasp, a.work)
        return
    sys.path.insert(0, HERE)
    from levels import app_levels
    ser = series_of(app_levels(a.atom))
    for key in (a.series or sorted(ser)):
        try:
            info = run_series(a.atom, key, ser[key], a.grasp, a.work, a.layers)
            print(a.atom, key, ser[key], info['df_start'], info['stages'], f"{info['seconds']} s", flush=True)
        except RuntimeError as e:
            print('FAILED', a.atom, key, e, flush=True)


if __name__ == '__main__':
    main()
