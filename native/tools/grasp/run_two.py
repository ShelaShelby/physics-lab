#!/usr/bin/env python3
"""GRASP2018 driver for the atoms with two s electrons outside closed shells (physics-lab native 0.47.3; PLAN_JUMPS.md part 2): He, Be, Mg, Ca, Sr, Ba, Ra, Zn, Cd, Hg, Yb.
The levels come from tools/make_level_data.py (the D2 set of each atom, the ones with "cloud"). One run per symmetry block (parity and J), all its levels together.

    python3 native/tools/grasp/run_two.py Mg [blocks e0 o2 ...] [--grasp $HOME/grasp] [--work $HOME/gwork2] [--layers 3]

Method (generalises run_level.py, the alkali recipe, to two valence electrons; lessons of 0.47.1/0.47.2 in README.txt):
  1. Dirac-Fock orbitals, one valence orbital at a time: the ground configuration ns2 varies everything (the core relaxes for it); then each further valence orbital n'l'
     in the lowest configuration of the level list that holds it (ns n'l', or another one whose other orbital is known), averaged over all its levels (every J, equal weights),
     only n'l' varied, the rest fixed; made orthogonal to the lower orbitals of its kappa (Gram-Schmidt). A step that lands more than 35 % from NIST's (the configuration's
     lowest level above the ground) is a wrong root: other starts are tried; when all fail the hydrogen-like orbital of charge 1 is kept (flagged), as for Na 4d in 0.47.2.
  2. Per block (parity, J): the reference configurations are those of the block's levels; core-valence + valence-valence MCDHF: single and double excitations with at most
     one hole in the outer core shell (He: none), three layers of correlation orbitals (s..g, only the new ones varied; the first layer also any skipped n below the
     spectroscopic ones), extended optimal level over every state of the reference configurations (they are the lowest states of the block).
  3. RCI with the Breit interaction and QED; rmultipole (as run_level.py).
  4. Each RCI root is named by jj2lsj (its leading LS component: configuration and term) and matched to the NIST level of the same configuration, term and J.
GRASP programs exit with status 0 even when rmcdhf fails: every output is checked (Convergence not obtained, NaN, a layer that moves more than 1 hartree).
"""
import argparse, json, os, re, shutil, subprocess, sys, time
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE); sys.path.insert(0, os.path.dirname(HERE))
import gfiles
from run_level import Run, CLOSED, LET, HC, mcdhf, write_start

# Z, mass number (GRASP's Fermi nucleus, Angeli's default radius), closed core for rcsfgenerate (0 none, 1 He, 2 Ne, 3 Ar, 4 Kr, 5 Xe), the rest of the core
# (inactive shells, and the outer core shell with at most one hole: the outer p shell; Be 1s; Zn, Cd, Hg the (n-1)d shell; Yb the 4f shell), the valence s shell's n
ATOMS = {
    'He': (2, 4, 0, '', 1),
    'Be': (4, 9, 0, '1s(2,1)', 2),
    'Mg': (12, 24, 1, '2s(2,i)2p(6,5)', 3),
    'Ca': (20, 40, 2, '3s(2,i)3p(6,5)', 4),
    'Sr': (38, 88, 3, '3d(10,i)4s(2,i)4p(6,5)', 5),
    'Ba': (56, 138, 4, '4d(10,i)5s(2,i)5p(6,5)', 6),
    'Ra': (88, 226, 5, '4f(14,i)5d(10,i)6s(2,i)6p(6,5)', 7),
    'Zn': (30, 64, 2, '3s(2,i)3p(6,i)3d(10,9)', 4),
    'Cd': (48, 114, 3, '3d(10,i)4s(2,i)4p(6,i)4d(10,9)', 5),
    'Hg': (80, 202, 4, '4d(10,i)4f(14,i)5s(2,i)5p(6,i)5d(10,9)', 6),
    'Yb': (70, 174, 4, '4d(10,i)5s(2,i)5p(6,i)4f(14,13)', 6),
}
LAYERS = 3
TOL_DF = 0.35
# a correlation layer with more CSFs than this is not made (the run time grows faster than its square: Mg's J = 1 even block, 0.47.3: layer 2 with 16,000 CSFs took over an hour; 10,000 keeps a block under about half an hour);
# the levels keep the layers before it ("layer k failed, k-1 kept" in info.json)
MAXCSF = 10000


def inactive(pre):
    return re.sub(r',\d+\)', ',i)', pre)


def vconf(val, mark):
    """[(3,0),(3,1)] -> '3s(1,*)3p(1,*)'; mark 'i' or '*'"""
    out = ''
    for o in sorted(set(val)):
        out += f'{o[0]}{LET[o[1]]}({val.count(o)},{mark})'
    return out


def cname(val):
    return '.'.join(f'{n}{LET[l]}' + ('2' if val.count((n, l)) == 2 else '') for n, l in sorted(set(val)))


def gen(run, closed, refs, act, j2lo, j2hi, nexc):
    s = '*\n%d\n' % closed + ''.join(r + '\n' for r in refs) + f'\n{act}\n{j2lo},{j2hi}\n{nexc}\nn\n'
    run('rcsfgenerate', s)
    shutil.copy(os.path.join(run.work, 'rcsf.out'), os.path.join(run.work, 'rcsf.inp'))
    blocks, n = [], 0
    for line in open(os.path.join(run.work, 'rcsf.inp')).read().split('CSF(s):')[1].splitlines():
        if line.strip() == '*': blocks.append(n); n = 0
        elif '(' in line: n += 1
    blocks.append(n)
    return blocks


def mcdhf_blocks(run, blocks, vary, spec, cycles, tag, wts=None, damp=None, thresh=None):
    lists = '\n'.join(','.join(str(i + 1) for i in range(b)) for b in blocks)
    # level weights: 1 equal; 9 user (wts, one per level in block order: run_shared.py, 0.47.4)
    wsel = ('9\n' + ' '.join(f'{w:.6f}' for w in wts) + '\n') if wts else '1\n'
    inp = f'y\n{lists}\n' + (wsel if sum(blocks) > 1 else '') + f'{vary}\n{spec}\n{cycles}\n'
    if damp is not None:
        # 0.51.3 (Yb ground, 4f14 6s2: 6s oscillated from every start with GRASP's default damping): non-default run, every subshell held at the constant accelerating
        # parameter |damp| (GRASP: -1 < ODAMP < 0 keeps |ODAMP| constant); the prompt order is getscd.f90's (no debug, no grid or ACCY change, then "Modify other defaults")
        # 0.91.0: also a run of several levels (Zn 4s5s, its two levels): the weight answer follows the lists, as in the default run
        inp = f'n\nn\nn\nn\n{lists}\n' + (wsel if sum(blocks) > 1 else '') + f'{vary}\n{spec}\n{cycles}\ny\n' + (f'y\n{thresh}\n' if thresh else 'n\n') + f'n\nn\ny\nn\n{-abs(damp)}\nn\nn\nn\nn\n1\n'
    out = run('rmcdhf_mem', inp)
    open(os.path.join(run.work, f'rmcdhf{tag}.out'), 'w').write(out)
    if 'onvergence not' in out: raise RuntimeError(f'rmcdhf did not converge ({tag})')
    # it can also stop half-way with status 0 (He, 0.47.3: "maximum tabulation point exceeds ... may indicate a continuum state"): then no RMCDHF end line
    if 'tabulation point exceeds' in out or 'Execution complete' not in out:
        raise RuntimeError(f'rmcdhf stopped before the end ({tag})')
    e = [float(x.replace('D', 'E')) if 'NaN' not in x else float('nan') for x in re.findall(r'Level +\d+ +Energy = +(\S+)', out)]
    if not e or any(x != x for x in e): raise RuntimeError(f'rmcdhf: no energy or NaN ({tag})')
    if tag.startswith('_L'):
        k = len([b for b in blocks]) and sum(blocks)
        first, last = e[:k], e[-k:]
        if max(abs(a - b) for a, b in zip(first, last)) > 1.0: raise RuntimeError(f'rmcdhf ran away ({tag}): {first[:1]} -> {last[:1]}')
    return out, e


def levels_of(X):
    import make_level_data as m
    A = m.atom(X, runs=False)
    return A, [(i, L) for i, L in enumerate(A['levels']) if L['cloud']]


def merge(work, new_key, first, dry=False):
    """series.w += the new orbitals of rwfn.out (all of them when first), Gram-Schmidt against the lower orbitals of their kappa; returns the smallest norm after projection"""
    out = gfiles.read_w(os.path.join(work, 'rwfn.out'))
    if first:
        gfiles.write_w(os.path.join(work, 'series.w'), out); return 1.0
    old = gfiles.read_w(os.path.join(work, 'series.w'))
    have = {(o[0], o[1]) for o in old}
    worst = 1.0
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
        worst = min(worst, nrm)
        old.append((n, kap, e, r, P / nrm, Q / nrm, p0 / nrm))
    if dry: return worst
    gfiles.write_w(os.path.join(work, 'series.w'), old)
    return worst


def extra_nodes(work, o):
    """0.91.0: the new orbital's kappas in rwfn.out whose large component crosses zero more often than n - l - 1 (counted where |P| is above 0.05 of its largest value,
    GRASP's own node-count threshold): a root of a higher n. Too few crossings are not counted against it (an outer orbital's inner lobes can be below 0.05).
    Returns [(kappa, counted, wanted)]."""
    bad = []
    for n, kap, e, r, P, Q, p0 in gfiles.read_w(os.path.join(work, 'rwfn.out')):
        if (n, -kap - 1 if kap < 0 else kap) != (o[0], o[1]): continue
        s = np.sign(P[np.abs(P) > 0.05 * np.abs(P).max()])
        got, want = int((s[1:] != s[:-1]).sum()), o[0] - o[1] - 1
        if got > want: bad.append((kap, got, want))
    return bad


def orbitals(X, grasp, work, log):
    """step 1: <work>/<X>/df/series.w; returns info"""
    Z, A, closed, pre, n0 = ATOMS[X]
    At, lv = levels_of(X)
    run = Run(grasp, os.path.join(work, X, 'df'), None); run.np = 1
    if os.path.isdir(run.work): shutil.rmtree(run.work)
    os.makedirs(run.work)
    # helium: a point nucleus (GRASP's Fermi model with its default skin thickness 2.3 fm is undefined for He's rms radius 1.68 fm: every energy came out NaN); mass of 4He,
    # 4.0026032541 u (AME2020); the finite size of the nucleus changes helium's levels by far less than the correlation left out
    run('rnucleus', '2\n0\n4.0026032541\n0\n0\n0\n' if Z == 2 else f'{Z}\n{A}\nn\n{A}\n0\n0\n0\n')
    info = {'df': []}
    mx = {}
    for s in CLOSED[closed] + re.findall(r'(\d+[spdfg])\(', pre):
        l = LET.index(s[-1]); mx[l] = max(mx.get(l, 0), int(s[:-1]))
    known = {(n0, 0)}
    def act(extra):
        m = dict(mx)
        for n, l in extra: m[l] = max(m.get(l, 0), n)
        return ','.join(f'{m[l]}{LET[l]}' for l in sorted(m))
    # ground
    g = [(n0, 0), (n0, 0)]
    gen(run, closed, [inactive(pre) + vconf(g, '*')], act(g), 0, 0, 0)   # (He: a CSF list with no peel subshell gave NaN)
    run('rangular', 'y\n')
    errs = []
    for est in ('2', '3', '2d'):
        try:
            run('rwfnestimate', f'y\n{est[0]}\n*\n')
            # '2d': Thomas-Fermi start, constant damping 0.7 (0.51.3: converges Yb's ground; 0.4 did not)
            out, e = mcdhf_blocks(run, [1], '*', '*', 300, '_g', damp=0.7 if est == '2d' else None); break
        except RuntimeError as x: errs.append(str(x))
    else:
        # staged (0.47.4, Yb: from Thomas-Fermi the first iteration left 4s and 5s unbound and the solver stopped): the shells of n <= 3 first with the rest held at the
        # estimate, then each further shell alone, then everything; each stage starts from the one before
        orbs = re.findall(r'(\d+)([spdf])', act(g).replace(',', ' '))
        allsh = sorted({(int(n), l) for s_ in CLOSED[closed] + re.findall(r'(\d+[spdfg])\(', pre) + [f'{n0}s'] for n, l in [(s_[:-1], s_[-1])]})
        # (Yb: one shell at a time by n then l; 4f, inside 5s 5p, is solved before them)
        LL = 'spdf'
        groups = [[o for o in allsh if o[0] <= 3]] + [[o] for o in sorted((o for o in allsh if o[0] > 3), key=lambda o: (o[0], LL.index(o[1])))] + [None]
        est = 'staged'
        try:
            # (an f shell from the screened hydrogenic estimate: Thomas-Fermi's 4f went unbound in Yb)
            run('rwfnestimate', 'y\n3\n4f*\n2\n*\n' if any(l == 'f' for n, l in allsh) else 'y\n2\n*\n')
            for k, grp in enumerate(groups):
                if grp == []: continue
                vary = '*' if grp is None else ','.join(f'{n}{l}' + ('*' if l != 's' else '') for n, l in grp)
                try: out, e = mcdhf_blocks(run, [1], vary, '*', 300, f'_g{k}')
                except RuntimeError:
                    # a single shell that does not converge from its Thomas-Fermi estimate (Yb 6s) starts again from the screened hydrogenic one
                    if grp is None or len(grp) > 1: raise
                    others = ','.join(f'{n}{l}' + ('*' if l != 's' else '') for n, l in allsh if (n, l) != grp[0])
                    run('rwfnestimate', f'y\n1\nstage.w\n{others}\n3\n{vary}\n')
                    try: out, e = mcdhf_blocks(run, [1], vary, '*', 300, f'_g{k}b')
                    except RuntimeError:
                        # still not alone: the shell is varied with all the others at once (the last stage) from here
                        if k != len(groups) - 2: raise
                        shutil.copy(os.path.join(run.work, 'rwfn.inp'), os.path.join(run.work, 'rwfn.out'))
                shutil.copy(os.path.join(run.work, 'rwfn.out'), os.path.join(run.work, 'stage.w'))
                if grp is not None: run('rwfnestimate', 'y\n1\nstage.w\n*\n')
        except RuntimeError as x: raise RuntimeError(f'{X}: ground DF failed: {errs + [str(x)]}')
    eg = e[-1]; merge(run.work, None, True); info['df'].append(('ground', {'2': 'tf', '3': 'hydrogenic', '2d': 'tf, damped 0.7'}.get(est, est), eg))
    # the further valence orbitals, in the order of the lowest level that holds them
    first = {}
    for i, L in sorted(lv, key=lambda x: x[1]['cm']):
        for o in set(L['val']):
            if o not in first: first[o] = (L['cm'], i)
    for o in sorted(first, key=lambda o: first[o][0]):
        if o in known: continue
        # the lowest level holding o whose other electrons are known
        cand = [(L['cm'], L) for i, L in lv if o in L['val'] and all(p in known or p == o for p in L['val'])]
        if not cand: raise RuntimeError(f'{X}: no configuration to make {o} in')
        cm, L = min(cand, key=lambda c: c[0])
        val = L['val']; jhi = 2 * sum(l for n, l in val) + 2
        blocks = gen(run, closed, [inactive(pre) + vconf(val, '*')], act(set(val) | known), 0, jhi, 0)
        blocks = [b for b in blocks if b]
        run('rangular', 'y\n')
        kap = [-(o[1] + 1)] + ([o[1]] if o[1] else [])
        vary = f'{o[0]}{LET[o[1]]}' + ('*' if o[1] else '')
        ok = None
        # 0.47.4: when the orbital alone does not converge (Ca 4p: GRASP's solver stops in IMPROV from every start), the valence s orbital is varied with it; only the
        # new orbital is kept (the ground's ns stays the one of ns2)
        # 0.91.0: (an s orbital only) its highest J alone, then damped starts, then damped with a higher node-count threshold (Zn 5s, Cd 6d, Hg 7s ... oscillated from every undamped start; the constant accelerating parameter 0.7 converged Yb's ground in 0.51.3)
        for strat, with_s in (('h1', 0), ('tf', 0), ('hydrogenic', 0), ('h1', 1), ('tf', 1)) + ((('tfJ', 0), ('tfJd', 0)) if o[1] == 0 else ()) + (('tfd', 0), ('hydrogenicd', 0), ('tfdt', 0), ('hydrogenicdt', 0)):
            try:
                if strat == 'tfJ':
                    # 0.91.0: the highest J of the configuration alone (Zn 4s5s: 3S1 only; for an s orbital only: a p or d orbital's other kappa would not be in it). Its one CSF does not mix with the ground's ns2 (a 1S0 average pulls 5s towards 4s). The starts after it keep this block.
                    blocks = [b for b in gen(run, closed, [inactive(pre) + vconf(val, '*')], act(set(val) | known), jhi, jhi, 0) if b]
                    run('rangular', 'y\n')
                if strat == 'h1':
                    import numpy as _np
                    r = _np.concatenate([[0.0], _np.exp(_np.linspace(_np.log(1e-6), _np.log(800.0), 5000))])
                    orbs = []
                    for k in kap:
                        P, Q, e1 = gfiles.hydrogenic(o[0], k, 1.0, r); orbs.append((o[0], k, e1, r, P, Q))
                    gfiles.write_w(os.path.join(run.work, 'start.w'), orbs)
                    run('rwfnestimate', 'y\n1\nseries.w\n*\n1\nstart.w\n*\nn\n')   # (0.47.4: it then asks "Revise any of these estimates?"; without this answer it crashed and h1 was never tried)
                else:
                    run('rwfnestimate', f'y\n1\nseries.w\n*\n{"2" if strat.startswith("tf") else "3"}\n*\nn\n')   # (the "Revise?" answer: Hg, 0.47.4, crashed here)
                out, e = mcdhf_blocks(run, blocks, (f'{n0}s,' if with_s else '') + vary, '*', 300, f'_{o[0]}{LET[o[1]]}', damp=0.7 if strat in ('tfd', 'hydrogenicd', 'tfdt', 'hydrogenicdt', 'tfJd') else None,
                                      # 0.91.0 (Hg 6d, 7p: "nodes counted 5, correct 3"): the node-count threshold (GRASP's default 0.05 of the largest amplitude) raised to 0.2
                                      thresh=0.2 if strat.endswith('dt') else None)
                step = (min(e[-sum(blocks):]) - eg) * HC
                # with ns varied too the step is lower (Dirac-Fock underbinds when both orbitals relax: Ca 4s4p 8,900 cm-1 against NIST 15,158); GRASP's solver keeps the
                # node count, so the wider window still rejects a root of another n (0.91.0: not with the 'dt' threshold; extra_nodes below checks every start)
                tol = 0.6 if with_s else TOL_DF
                if not (abs(step - cm) <= tol * cm): raise RuntimeError(f'{o}: DF step {step:.0f} cm-1 against NIST {cm:.0f}: a wrong root')
                # an orbital that converged onto the shape of a lower one of its kappa (Ca 6p onto 5p, 0.47.4: 11 % left after projection) is a failed start
                if merge(run.work, o, False, dry=True) < 0.3: raise RuntimeError(f'{o}: collapsed onto a lower orbital ({strat})')
                # 0.91.0: a converged orbital with too many nodes is a root of a higher n. The energy check above misses it when the other kappa's levels are right
                # (Cd 5d5/2 with 3 nodes beside a right 5d3/2), and the raised threshold of the 'dt' starts let Hg 7p converge with 8 nodes and Cd 7d with 6.
                xn = extra_nodes(run.work, o)
                if xn: raise RuntimeError(f'{o}: a root of a higher n ({strat}): kappa, nodes counted, wanted {xn}')
                ok = strat + (f' with {n0}s varied' if with_s else ''); break
            except RuntimeError as x: errs.append(str(x)); print(X, 'DF try', o, strat, with_s, str(x)[:160].replace('\n', ' '), flush=True)
        if ok is None:
            # keep the hydrogen-like orbital of charge 1 (made orthogonal), flagged
            r = np.concatenate([[0.0], np.exp(np.linspace(np.log(1e-6), np.log(800.0), 5000))])
            gg = gfiles.grasp_grid(gfiles.read_w(os.path.join(run.work, 'series.w'))[0][3])
            orbs = []
            for k in kap:
                P, Q, e1 = gfiles.hydrogenic(o[0], k, 1.0, r); orbs.append(gfiles.onto((o[0], k, e1, r, P, Q), gg))
            gfiles.write_w(os.path.join(run.work, 'rwfn.out'), orbs)
            ok = 'hydrogen-like kept'; step = float('nan')
        nrm = merge(run.work, o, False)
        if nrm < 0.3: errs.append(f'{o}: collapsed onto a lower orbital (norm {nrm:.2f} after projection)')
        known.add(o); info['df'].append((f'{o[0]}{LET[o[1]]}', ok, step, cm, round(nrm, 3)))
        print(X, 'DF', o, ok, f'{step:.0f} vs {cm:.0f}', flush=True)
    info['errors'] = errs[-10:]
    json.dump(info, open(os.path.join(run.work, 'info.json'), 'w'))
    return info


def block_key(L): return ('o' if L['odd'] else 'e') + str(L['J2'] // 2)


def run_block(X, key, grasp, work, layers=LAYERS):
    """steps 2-4 for one block, e.g. 'o1' (odd, J = 1)"""
    Z, A, closed, pre, n0 = ATOMS[X]
    At, lv = levels_of(X)
    mine = [(i, L) for i, L in lv if block_key(L) == key]
    J = int(key[1:]); j2 = 2 * J
    confs = sorted({tuple(L['val']) for i, L in mine})
    run = Run(grasp, os.path.join(work, X, key), None)
    if os.path.isdir(run.work): shutil.rmtree(run.work)
    os.makedirs(run.work)
    shutil.copy(os.path.join(work, X, 'df', 'series.w'), os.path.join(run.work, 'series.w'))
    shutil.copy(os.path.join(work, X, 'df', 'isodata'), os.path.join(run.work, 'isodata'))
    T0 = time.time(); info = {'atom': X, 'block': key, 'confs': [cname(list(c)) for c in confs], 'stages': []}
    mx = {}
    for s in CLOSED[closed] + re.findall(r'(\d+[spdfg])\(', pre):
        l = LET.index(s[-1]); mx[l] = max(mx.get(l, 0), int(s[:-1]))
    for o in gfiles.read_w(os.path.join(run.work, 'series.w')):
        l = gfiles.ell(o[1]); mx[l] = max(mx.get(l, 0), o[0])
    spec_have = {(o[0], gfiles.ell(o[1])) for o in gfiles.read_w(os.path.join(run.work, 'series.w'))}
    # the reference space: every state of the reference configurations in this J
    n0s = gen(run, closed, [inactive(pre) + vconf(list(c), 'i') for c in confs], ','.join(f'{mx[l]}{LET[l]}' for l in sorted(mx)), j2, j2, 0)[0]
    info['ref_states'] = n0s
    if n0s == 0: raise RuntimeError(f'{X} {key}: no CSF')
    refs = [pre + vconf(list(c), '*') for c in confs]
    prev, errs = 'series', []
    for k in range(1, layers + 1):
        t = time.time()
        corecut = {}
        for s_ in CLOSED[closed] + re.findall(r'(\d+[spdfg])\(', pre):
            l_ = LET.index(s_[-1]); corecut[l_] = max(corecut.get(l_, 0), int(s_[:-1]))
        ok = False
        mx0 = dict(mx)
        # layer 1 tries correlation orbitals s..g, then s..d, then s only (helium, 0.47.3: its f orbital ran off to the continuum; the s orbital alone lets 1s relax);
        # the later layers keep the l range that worked
        for lmax in ((4, 2, 0) if k == 1 else (lmax_used,)):
            mx = dict(mx0)
            new = []
            for ll in range(lmax + 1):
                # an l whose orbitals in this block's configurations reach n = 7 or more (helium's 1s10p) gets no correlation orbital: a hydrogen-like start of n = 11 runs off to the
                # continuum (0.47.3), and its many spectroscopic orbitals already span that space
                if max((o[0] for c in confs for o in c if o[1] == ll), default=0) >= 7: continue   # (only where this block's own configurations hold such an orbital)
                top = max(mx.get(ll, ll), ll) + 1
                # layer 1 also makes any orbital of this l below the top that is neither core nor spectroscopic (a gap in the series)
                for n in (range(max(ll + 1, corecut.get(ll, 0) + 1), top + 1) if k == 1 else [top]):
                    if (n, ll) not in spec_have: new.append(f'{n}{LET[ll]}' + ('*' if ll else ''))
                mx[ll] = top
            if not new: continue
            nc = gen(run, closed, refs, ','.join(f'{mx[l]}{LET[l]}' for l in sorted(mx)), j2, j2, 2)[0]
            if k > 1 and nc > MAXCSF: errs.append(f'layer {k}: {nc} CSFs, more than {MAXCSF}'); break
            run('rangular', 'y\n')
            for est in ('3', '2'):
                try:
                    run('rwfnestimate', f'y\n1\n{prev}.w\n*\n{est}\n*\n')
                    out, e = mcdhf_blocks(run, [n0s], ','.join(new), '', 200, f'_L{k}')
                    ok = True; break
                except RuntimeError as x: errs.append(str(x))
            if ok: lmax_used = lmax; break
        if not ok: mx = mx0
        if not ok and k == 1:
            # no correlation layer converges (helium's Rydberg blocks, 0.47.3: the correlation orbitals ran off to the continuum): CI with single and double excitations into the
            # orbitals of another block of this atom whose layers did converge (the one with the most orbitals; "borrowed orbitals" in info.json), or, when none has, into the
            # spectroscopic orbitals only. Without correlation orbitals helium's 1s keeps the shape it has in 1s2 and the levels came out 5 % off; with them, 0.1 %.
            sib = []
            for d in sorted(os.listdir(os.path.dirname(run.work))):
                fi = os.path.join(os.path.dirname(run.work), d, 'info.json')
                if d in ('df', key) or not os.path.exists(fi): continue
                si = json.load(open(fi))
                if si.get('final', 'L0') != 'L0' and not si.get('borrowed'):
                    sib.append((len(gfiles.read_w(os.path.join(os.path.dirname(run.work), d, si['final'] + '.w'))), d, si['final']))
            src = os.path.join(run.work, 'series.w')
            if sib:
                _, d, fin = max(sib); src = os.path.join(os.path.dirname(run.work), d, fin + '.w'); info['borrowed'] = d
            orbs = gfiles.read_w(src)
            sp = {}
            for o in orbs: sp[gfiles.ell(o[1])] = max(sp.get(gfiles.ell(o[1]), 0), o[0])
            have_nl = {(o[0], gfiles.ell(o[1])) for o in orbs}
            # the active set must hold every orbital up to its top: the gaps are left out of the excitations by taking, per l, the top below the first gap
            for l_ in list(sp):
                n_ = max(l_ + 1, corecut.get(l_, 0) + 1)
                while (n_, l_) in have_nl: n_ += 1
                sp[l_] = n_ - 1
            sp = {l_: n_ for l_, n_ in sp.items() if n_ > l_}
            nc = gen(run, closed, refs, ','.join(f'{sp[l]}{LET[l]}' for l in sorted(sp)), j2, j2, 2)[0]
            run('rangular', 'y\n')
            shutil.copy(os.path.join(run.work, 'rcsf.inp'), os.path.join(run.work, 'L0.c'))
            shutil.copy(src, os.path.join(run.work, 'L0.w'))
            prev = 'L0'
            info['stages'].append(('no correlation layer converged: CI in ' + (f'the orbitals of block {info["borrowed"]}' if sib else 'the spectroscopic orbitals'), nc, round(time.time() - t, 1)))
            break
        if not ok:
            # a later layer that fails (a correlation orbital going to the continuum, He): the levels keep the layers before it (said in info.json and levels.tsv)
            info['stages'].append((f'layer {k} failed, {k - 1} kept', nc, round(time.time() - t, 1)))
            nc = info['stages'][-2][1]
            gen(run, closed, refs, ','.join(f'{n}{LET[l]}' for l, n in prev_mx.items()), j2, j2, 2)
            run('rangular', 'y\n')
            break
        run('rsave', '', [f'L{k}']); prev = f'L{k}'; prev_mx = {l: mx[l] for l in sorted(mx)}
        info['stages'].append((f'layer {k}' + ('' if lmax_used == 4 else f' (l up to {LET[lmax_used]})'), nc, round(time.time() - t, 1)))
        print(X, key, f'layer {k}', nc, 'CSFs', round(time.time() - t, 1), 's', flush=True)
    return finish(X, key, grasp, run, info, prev, n0s, nc, T0)


def multipoles(grasp, work, prev, nproc=1):
    """rmultipole -> <work>/<prev>.mp; with nproc > 1 as nproc processes over the CSFs (0.47.4: one process took 20 min on Ca's 43,000-CSF block), their partial sums added
    here and written as one process writes them (|R| > 1e-14, the same format)"""
    exe = os.path.join(grasp, 'bin', 'rmultipole')
    if nproc <= 1:
        mp = subprocess.run([exe], input=f'{prev}\ny\n', text=True, cwd=work, capture_output=True)
        if mp.returncode or not os.path.exists(os.path.join(work, prev + '.mp')): raise RuntimeError(f'rmultipole failed: {mp.stderr[-500:]}')
        return
    ps = [subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True, cwd=work) for r in range(nproc)]
    for r, p_ in enumerate(ps): p_.stdin.write(f'{prev}\ny\n{r} {nproc}\n'); p_.stdin.close()
    for p_ in ps:
        if p_.wait(): raise RuntimeError(f'rmultipole failed: {p_.stderr.read()[-500:]}')
    head, levels, sums = [], [], []
    for r in range(nproc):
        f = os.path.join(work, f'{prev}.mp.{r}')
        cur = -1
        for line in open(f):
            if line.startswith('level'):
                cur += 1
                if r == 0: levels.append(line); sums.append({})
            elif line.startswith('R'):
                k, a, b, v = line[1:4], line[4:9], line[9:14], float(line[14:])
                key = (int(k), int(a), int(b)); sums[cur][key] = sums[cur].get(key, 0.0) + v
            elif r == 0: head.append(line)
        os.remove(f)
    with open(os.path.join(work, prev + '.mp'), 'w') as f:
        f.write(''.join(head))
        for lvl, sm in zip(levels, sums):
            f.write(lvl)
            for (k, a, b) in sorted(sm):
                if abs(sm[(k, a, b)]) > 1e-14: f.write(f'R{k:3d}{a:5d}{b:5d}{sm[(k, a, b)]:26.16E}\n')


def finish(X, key, grasp, run, info, prev, n0s, nc, T0):
    """steps 3-4 of a block: RCI on the orbitals and CSFs saved as <prev> (.c, .w), names of the roots, multipoles; info.json"""
    t = time.time()
    roots = ','.join(str(i + 1) for i in range(n0s))
    run('rci', f'y\n{prev}\ny\ny\n1.d-6\ny\nn\nn\ny\n4\n{roots}\n')
    if os.path.getsize(os.path.join(run.work, prev + '.cm')) < 200:
        # rci stopped without a result ("Brint1 Integral not found": GRASP's Breit integrals failed for helium's o0 block, 0.47.3): again without the Breit interaction
        # (QED kept), flagged in info.json
        run('rci', f'y\n{prev}\nn\ny\n1.d-6\ny\nn\nn\ny\n4\n{roots}\n')   # (it still asks for the frequencies and their scale)
        info['no_breit'] = True
        if os.path.getsize(os.path.join(run.work, prev + '.cm')) < 200: raise RuntimeError(f'{X} {key}: rci gave no result')
    info['stages'].append(('RCI' + (' (no Breit)' if info.get('no_breit') else ''), nc, round(time.time() - t, 1)))
    # names of the roots (jj2lsj: the leading LS components)
    p = subprocess.run([os.path.join(grasp, 'bin', 'jj2lsj')], input=f'{prev}\ny\nn\ny\n', text=True, cwd=run.work, capture_output=True)
    open(os.path.join(run.work, 'jj2lsj.log'), 'w').write(p.stdout + p.stderr)
    multipoles(grasp, run.work, prev, run.np)
    for f in os.listdir(run.work):   # GRASP's angular scratch files (mcp.*) of a finished block: up to a GB each; the disk filled in 0.47.3
        if f.startswith('mcp.') or f in ('rcsf.out', 'rwfn.inp', 'rwfn.out', 'clist.new'): os.remove(os.path.join(run.work, f))
    shutil.rmtree(os.path.join(run.work, 'mpitmp'), ignore_errors=True)   # (the MPI programs' scratch files, GRASP_NP > 1: 0.47.4)
    info['final'] = prev; info['seconds'] = round(time.time() - T0, 1)
    json.dump(info, open(os.path.join(run.work, 'info.json'), 'w'))
    return info


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('atom'); ap.add_argument('blocks', nargs='*')
    ap.add_argument('--grasp', default=os.path.expanduser('~/grasp')); ap.add_argument('--work', default=os.path.expanduser('~/gwork2'))
    ap.add_argument('--layers', type=int, default=LAYERS); ap.add_argument('--skip-df', action='store_true')
    ap.add_argument('--resume', action='store_true', help='keep the blocks already finished (info.json), and the orbitals of step 1')
    ap.add_argument('--finish', default='', help='KEY:LAYER, e.g. e1:L2: end that block with the layers saved so far (RCI, names, multipoles), keeping its stages')
    a = ap.parse_args()
    if a.finish:
        key, lay = a.finish.split(':')
        run = Run(a.grasp, os.path.join(a.work, a.atom, key), None)
        At, lv = levels_of(a.atom)
        confs = sorted({tuple(L['val']) for i, L in lv if block_key(L) == key})
        Z, A_, closed, pre, n0 = ATOMS[a.atom]
        n0s = sum(1 for _ in [0])
        # the reference states: counted again from the configurations (as run_block does), in a scratch copy of the CSF tools' output
        import tempfile
        tmp = Run(a.grasp, tempfile.mkdtemp(dir=run.work), None)
        mx = {}
        for o in gfiles.read_w(os.path.join(run.work, 'series.w')):
            l = gfiles.ell(o[1]); mx[l] = max(mx.get(l, 0), o[0])
        for s_ in CLOSED[closed] + re.findall(r'(\d+[spdfg])\(', pre):
            l = LET.index(s_[-1]); mx[l] = max(mx.get(l, 0), int(s_[:-1]))
        n0s = gen(tmp, closed, [inactive(pre) + vconf(list(c), 'i') for c in confs], ','.join(f'{mx[l]}{LET[l]}' for l in sorted(mx)), int(key[1:]) * 2, int(key[1:]) * 2, 0)[0]
        nc = sum(1 for line in open(os.path.join(run.work, lay + '.c')) if '(' in line) - 1
        info = {'atom': a.atom, 'block': key, 'confs': [cname(list(c)) for c in confs], 'stages': [(f'layers up to {lay} (the next one stopped: too slow)', nc, 0)], 'ref_states': n0s}
        info = finish(a.atom, key, a.grasp, run, info, lay, n0s, nc, time.time())
        print(a.atom, key, info['confs'], info['stages'], flush=True)
        return
    if not (a.skip_df or (a.resume and os.path.exists(os.path.join(a.work, a.atom, 'df', 'info.json')))): orbitals(a.atom, a.grasp, a.work, None)
    At, lv = levels_of(a.atom)
    keys = sorted({block_key(L) for i, L in lv}, key=lambda k: (int(k[1:]), k[0]))
    for key in (a.blocks or keys):
        if a.resume and os.path.exists(os.path.join(a.work, a.atom, key, 'info.json')): continue
        try:
            info = run_block(a.atom, key, a.grasp, a.work, a.layers)
            print(a.atom, key, info['confs'], info['stages'], f"{info['seconds']} s", flush=True)
        except RuntimeError as e:
            print('FAILED', a.atom, key, e, flush=True)


if __name__ == '__main__':
    main()
