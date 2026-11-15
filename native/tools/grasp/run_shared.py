#!/usr/bin/env python3
"""GRASP2018 driver, native 0.47.4: the two-s-electron atoms with ONE orbital set per atom (PLAN_JUMPS.md part 2; replaces run_two.py's one-set-per-block recipe).

    GRASP_NP=4 python3 native/tools/grasp/run_shared.py Ca [--work $HOME/gw4] [--stage df|orb|ci|all] [--blocks e0 o1 ...] [--layers 2]

Why (0.47.3, REPORT.md): with its own orbitals per block (parity, J) a term's J levels came out spread (Zn 3P0..2: -3.4..-5.3 %), a level had to be measured from a ground
computed with the same number of layers, and blocks with many reference configurations (Ca odd J = 1: 4s4p, 3d4p, 4s6p) hit the CSF cap after one layer.
Method:
  1. Dirac-Fock orbitals (run_two.orbitals: ns2 first, then each valence orbital in the lowest configuration that holds it).
  2. Correlation orbitals, once per atom: the "main" valence orbitals are ns, (n+1)s, the lowest p and the lowest d of the level list; the main configurations are the listed
     configurations made of them only (Ca: 4s2, 4s5s, 4s4p, 3d4s, 3d4p, 3d2, 4p2). One MCDHF run over several blocks at once (extended optimal level over every state of
     the main configurations in each block; a main configuration enters the block of its J that holds most of its listed levels; the ground configuration enters e0).
     Expansion: single and double excitations from the main configurations with at most one hole in the outer core shell (core-valence + valence-valence) into the core,
     the main orbitals and LAYERS layers of correlation orbitals (layer 1 s..g, layer 2 s..f, layer 3 s..d; only the new layer varied). The other spectroscopic orbitals
     (Ca 6s, 5p, 6p, 4d...) are left out here: they would make the lists grow by a factor of about 5 (counted 0.47.4).
  3. Per block, RCI with Breit and QED on ONE orbital file for the whole atom: all Dirac-Fock orbitals + the correlation orbitals of step 2, renamed above the spectroscopic
     ones and made orthogonal to them (Gram-Schmidt per kappa). Expansion: valence-valence SD from every reference configuration of the block (core closed) plus core-valence
     SD (one outer-core hole) from its main configurations (or its lowest configuration when it has none). Every level, the ground included, uses the same orbitals, so the
     excitation energies need no balancing and the J levels of a term share their radial functions.
  4. jj2lsj names and rmultipole, as run_two.finish; the block folders look like run_two's (info.json, ci.c/.w/.cm/.lsj.lbl/.mp), so multipoles_two.py reads them.
Every GRASP output is checked as in run_two (non-convergence, NaN, a run-away layer, "Execution complete").
"""
import argparse, json, os, re, shutil, sys, time
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE); sys.path.insert(0, os.path.dirname(HERE))
import gfiles
import run_two as R
from run_level import Run, CLOSED, LET

LAYERS = 3
LMAX = (4, 3, 2)   # highest l of the correlation orbitals added in layer 1, 2, 3
CAP_CI = 60000     # step 3: core-valence references are added (lowest first) while the block's list stays under this
WEIGHTS = os.environ.get('GRASP_WEIGHTS', 'config')   # step 2's level weights: 'config' (see stage_orb) or 'equal'
CAP_ORB = 30000    # step 2: a layer whose lists together hold more CSFs is not made (the layers before it are kept, said in info.json)


def core_top(X):
    Z, A, closed, pre, n0 = R.ATOMS[X]
    mx = {}
    for s in CLOSED[closed] + re.findall(r'(\d+[spdfg])\(', pre):
        l = LET.index(s[-1]); mx[l] = max(mx.get(l, 0), int(s[:-1]))
    return mx


def plan(X):
    """the levels, the main orbitals, the main configurations and the blocks of step 2"""
    Z, A, closed, pre, n0 = R.ATOMS[X]
    At, lv = R.levels_of(X)
    allv = {o for i, L in lv for o in L['val']}
    main = {(n0, 0)}
    if (n0 + 1, 0) in allv: main.add((n0 + 1, 0))
    for l in (1, 2):
        c = [o for o in allv if o[1] == l]
        if c: main.add(min(c))
    confs = {tuple(L['val']) for i, L in lv}
    mconf = sorted(c for c in confs if set(c) <= main)
    gconf = tuple(lv[0][1]['val']) if lv[0][0] == 0 else ((n0, 0), (n0, 0))
    orb_blocks = {'e0': [gconf]}
    for c in mconf:
        if c == gconf: continue
        per = {}
        for i, L in lv:
            if tuple(L['val']) == c: per.setdefault(R.block_key(L), []).append(i)
        key = max(per, key=lambda k: (len(per[k]), -int(k[1:]), k[0] == 'e'))
        orb_blocks.setdefault(key, [])
        if c not in orb_blocks[key]: orb_blocks[key].append(c)
    return lv, main, mconf, orb_blocks


def read_csf(path):
    txt = open(path).read()
    head, body = txt.split('CSF(s):\n')
    blocks, cur = [], []
    lines = body.splitlines()
    i = 0
    while i < len(lines):
        if lines[i].strip() == '*': blocks.append(cur); cur = []; i += 1; continue
        if not lines[i].strip(): i += 1; continue
        cur.append('\n'.join(lines[i:i + 3])); i += 3
    blocks.append(cur)
    return head + 'CSF(s):\n', [b for b in blocks if b]


def jp(csf):
    return csf.splitlines()[2].split()[-1]


def write_csf(path, head, blocks):
    with open(path, 'w') as f:
        f.write(head + '\n *\n'.join('\n'.join(b) for b in blocks) + '\n')


def one_block(run, closed, refs, act, j2, nexc):
    """rcsfgenerate for one J; the list of the references' parity (rcsfgenerate keeps it; checked)"""
    R.gen(run, closed, refs, act, j2, j2, nexc)
    head, bl = read_csf(os.path.join(run.work, 'rcsf.inp'))
    if len(bl) != 1: raise RuntimeError(f'rcsfgenerate gave {len(bl)} blocks for J2 = {j2}: {[jp(b[0]) for b in bl]}')
    return head, bl[0]


def wider(h1, h2):
    """one header for two CSF lists: a list that never uses a subshell leaves it out of its header (Ca o0: no 5g+ in J = 0 valence-valence); each CSF line names its own
    subshells, so the list with fewer stays valid under the wider header when the order agrees (checked)"""
    def peel(h): return h.split('Peel subshells:')[1].split('CSF(s):')[0].split()
    a, b = peel(h1), peel(h2)
    if h1.split('Peel')[0] != h2.split('Peel')[0]: raise RuntimeError('CSF list core subshells differ')
    big, small = (a, h1), (b, h2)
    if len(b) > len(a): big, small = small, big
    it = iter(big[0])
    if not all(x in it for x in small[0]): raise RuntimeError('CSF list subshell orders differ')
    return big[1]


def active(mx):
    return ','.join(f'{mx[l]}{LET[l]}' for l in sorted(mx))


def stage_orb(X, grasp, work, layers):
    """step 2: <work>/X/orb/L<k>.w"""
    Z, A, closed, pre, n0 = R.ATOMS[X]
    lv, main, mconf, ob = plan(X)
    run = Run(grasp, os.path.join(work, X, 'orb'), None)
    if os.path.isdir(run.work): shutil.rmtree(run.work)
    os.makedirs(run.work)
    shutil.copy(os.path.join(work, X, 'df', 'isodata'), run.work)
    ct = core_top(X)
    keep = lambda o: o[0] <= ct.get(gfiles.ell(o[1]), 0) or (o[0], gfiles.ell(o[1])) in main
    gfiles.write_w(os.path.join(run.work, 'L0.w'), [o for o in gfiles.read_w(os.path.join(work, X, 'df', 'series.w')) if keep(o)])
    mx = dict(ct)
    for n, l in main: mx[l] = max(mx.get(l, 0), n)
    keys = sorted(ob, key=lambda k: (int(k[1:]), k[0]))
    info = {'atom': X, 'main': sorted(f'{n}{LET[l]}' for n, l in main), 'blocks': {k: [R.cname(list(c)) for c in ob[k]] for k in keys}, 'stages': []}
    print(X, 'orb blocks', info['blocks'], flush=True)
    prev = 'L0'
    for k in range(1, layers + 1):
        t = time.time()
        new = []
        for l in range(LMAX[k - 1] + 1):
            mx[l] = max(mx.get(l, l), l) + 1
            new.append(f'{mx[l]}{LET[l]}' + ('*' if l else ''))
        head, lists, n0s = None, [], []
        for key in keys:
            j2 = 2 * int(key[1:])
            h, b = one_block(run, closed, [pre + R.vconf(list(c), '*') for c in ob[key]], active(mx), j2, 2)
            head = h if head is None else wider(head, h)
            if jp(b[0])[-1] != ('-' if key[0] == 'o' else '+'): raise RuntimeError(f'{X} {key}: parity {jp(b[0])}')
            lists.append(b)
            _, r0 = one_block(run, closed, [R.inactive(pre) + R.vconf(list(c), 'i') for c in ob[key]], active(mx), j2, 0)
            n0s.append(len(r0))
        # weights (WEIGHTS = 'config'): each main configuration counts once, spread over the states of its block (with equal weights per state the 3d configurations,
        # which have many states, outweighed the single ground state: Ca 3d4s -7 %, 0.47.4)
        wts = None
        if WEIGHTS == 'config':
            wts = [len(ob[key]) / n for key, n in zip(keys, n0s) for _ in range(n)]
        info['weights'] = WEIGHTS
        nc = sum(len(b) for b in lists)
        if nc > CAP_ORB:
            info['stages'].append((f'layer {k} not made: {nc} CSFs, more than {CAP_ORB}', nc, 0)); break
        write_csf(os.path.join(run.work, 'rcsf.inp'), head, lists)
        run('rangular', 'y\n')
        ok, errs = False, []
        for est in ('3', '2'):
            try:
                run('rwfnestimate', f'y\n1\n{prev}.w\n*\n{est}\n*\nn\n')
                out, e = R.mcdhf_blocks(run, n0s, ','.join(new), '', 200, f'_L{k}', wts)
                ok = True; break
            except RuntimeError as x: errs.append(str(x))
        if not ok:
            info['stages'].append((f'layer {k} failed, {k - 1} kept: {errs[-1][:200]}', nc, round(time.time() - t, 1))); break
        run('rsave', '', [f'L{k}']); prev = f'L{k}'
        info['stages'].append((f'layer {k} ({",".join(new)})', nc, round(time.time() - t, 1)))
        print(X, 'orb', info['stages'][-1], flush=True)
        for f in os.listdir(run.work):
            if f.startswith('mcp.'): os.remove(os.path.join(run.work, f))
        shutil.rmtree(os.path.join(run.work, 'mpitmp'), ignore_errors=True)
    info['final'] = prev
    json.dump(info, open(os.path.join(run.work, 'info.json'), 'w'))
    return info


def ci_orbitals(X, work):
    """step 3's orbital file <work>/X/ci.w: the Dirac-Fock orbitals + the correlation orbitals renamed above them, Gram-Schmidt per kappa; returns the renaming"""
    oi = json.load(open(os.path.join(work, X, 'orb', 'info.json')))
    spec = gfiles.read_w(os.path.join(work, X, 'df', 'series.w'))
    corr_all = gfiles.read_w(os.path.join(work, X, 'orb', oi['final'] + '.w'))
    have = {(o[0], o[1]) for o in gfiles.read_w(os.path.join(work, X, 'orb', 'L0.w'))}
    corr = [o for o in corr_all if (o[0], o[1]) not in have]
    top = {}
    for o in spec: top[o[1]] = max(top.get(o[1], 0), o[0])
    out, ren, worst = list(spec), {}, 1.0
    for o in sorted(corr, key=lambda o: (o[1], o[0])):
        n, kap, e, r, P, Q, p0 = o
        n2 = max(top.get(kap, 0), n - 1) + 1
        top[kap] = n2
        for m in out:
            if m[1] != kap: continue
            # 0.51.3: an orbital longer than this one (Ra's hydrogen-like 8p reaches 493 bohr, the correlation p orbitals 116) was cut to this one's length here,
            # which left up to 1.4 % overlap in ci.w (GRASP takes the orbitals as orthonormal); now this one is extended onto the longer grid first
            if len(m[4]) > len(P): r, P, Q = m[3], gfiles.pad(P, len(m[4])), gfiles.pad(Q, len(m[4]))
            ov = gfiles.overlap(m, (n2, kap, e, r, P, Q))
            P = P - ov * gfiles.pad(m[4], len(P)); Q = Q - ov * gfiles.pad(m[5], len(Q))
        nrm = np.sqrt(gfiles.overlap((n2, kap, e, r, P, Q), (n2, kap, e, r, P, Q)))
        worst = min(worst, nrm)
        out.append((n2, kap, e, r, P / nrm, Q / nrm, p0 / nrm))
        ren[f'{n}{kap}'] = n2
    # 0.51.3: the file must be orthonormal per kappa (GRASP assumes it; Ra 0.47.4: 1.4 % overlap, and every block's lowest roots fell onto correlation orbitals)
    bad = max(abs(gfiles.overlap(a, b) - (1.0 if i == j else 0.0)) for i, a in enumerate(out) for j, b in enumerate(out[:i + 1]) if a[1] == b[1])
    if bad > 1e-6: raise RuntimeError(f'{X}: ci.w not orthonormal ({bad:.1e})')
    gfiles.write_w(os.path.join(work, X, 'ci.w'), out)
    return ren, worst, out


def ci_top(work, X):
    mx = {}
    for o in gfiles.read_w(os.path.join(work, X, 'ci.w')):
        l = gfiles.ell(o[1]); mx[l] = max(mx.get(l, 0), o[0])
    return mx


def run_probe(grasp, work, X):
    return Run(grasp, os.path.join(work, X, 'probe'), None)


def stage_ci(X, key, grasp, work):
    Z, A, closed, pre, n0 = R.ATOMS[X]
    lv, main, mconf, ob = plan(X)
    mine = [(i, L) for i, L in lv if R.block_key(L) == key]
    confs = sorted({tuple(L['val']) for i, L in mine})
    # core-valence from EVERY reference configuration of the block (0.47.4: from the main ones only, Ca 4s5p and 4s6p came out +10 %: their 4s-core pair correlation,
    # 3p 4s -> two correlation orbitals, is a triple excitation from 4s4p); the lowest configurations first, the highest left out when the list would pass CAP_CI
    # every block of a parity starts from the same core-valence references, the main configurations of that parity (lowest first), then adds its own (lowest first): with
    # different sets per J a term's levels got different correlation (Ca 4s4p 3P1 below 3P0, 0.47.4 first run)
    lowest = lambda c: min([L['cm'] for i, L in lv if tuple(L['val']) == c] or [1e9])
    par = lambda c: sum(l for n, l in c) % 2
    order = sorted([c for c in mconf if par(c) == (key[0] == 'o')], key=lowest) + sorted([c for c in confs if c not in mconf], key=lowest)
    mains = []
    for c in order:
        _, b = one_block(run_probe(grasp, work, X), closed, [pre + R.vconf(list(x), '*') for x in mains + [c]], active(ci_top(work, X)), 2 * int(key[1:]), 2)
        if len(b) > CAP_CI and mains: break
        mains.append(c)
    run = Run(grasp, os.path.join(work, X, key), None)
    if os.path.isdir(run.work): shutil.rmtree(run.work)
    os.makedirs(run.work)
    shutil.copy(os.path.join(work, X, 'df', 'isodata'), run.work)
    shutil.copy(os.path.join(work, X, 'ci.w'), os.path.join(run.work, 'ci.w'))
    mx = {}
    for o in gfiles.read_w(os.path.join(run.work, 'ci.w')):
        l = gfiles.ell(o[1]); mx[l] = max(mx.get(l, 0), o[0])
    j2 = 2 * int(key[1:])
    T0 = time.time()
    h1, vv = one_block(run, closed, [R.inactive(pre) + R.vconf(list(c), '*') for c in confs], active(mx), j2, 2)
    h2, cv = one_block(run, closed, [pre + R.vconf(list(c), '*') for c in mains], active(mx), j2, 2)
    h1 = wider(h1, h2)
    seen = set(cv); lst = cv + [c for c in vv if c not in seen]
    _, r0 = one_block(run, closed, [R.inactive(pre) + R.vconf(list(c), 'i') for c in confs], active(mx), j2, 0)
    write_csf(os.path.join(run.work, 'ci.c'), h1, [lst])
    # roots: the reference states and as many again (other configurations of the spectroscopic orbitals, e.g. Ca 4s5p in o1, can lie between them)
    nroots = min(len(lst), 2 * len(r0) + 4)
    info = {'atom': X, 'block': key, 'confs': [R.cname(list(c)) for c in confs], 'cv_from': [R.cname(list(c)) for c in mains],
            'stages': [(f'RCI list: {len(cv)} core-valence + {len(lst) - len(cv)} more valence-valence', len(lst), 0)], 'ref_states': len(r0), 'roots': nroots,
            'recipe': 'shared orbitals (run_shared.py, 0.47.4)'}
    return R.finish(X, key, grasp, run, info, 'ci', nroots, len(lst), T0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('atom'); ap.add_argument('--grasp', default=os.path.expanduser('~/grasp')); ap.add_argument('--work', default=os.path.expanduser('~/gw4'))
    ap.add_argument('--stage', default='all', choices=['df', 'orb', 'ci', 'all']); ap.add_argument('--blocks', nargs='*', default=[])
    ap.add_argument('--layers', type=int, default=LAYERS); ap.add_argument('--resume', action='store_true')
    a = ap.parse_args(); X = a.atom
    if a.stage in ('df', 'all') and not (a.resume and os.path.exists(os.path.join(a.work, X, 'df', 'info.json'))):
        i = R.orbitals(X, a.grasp, a.work, None); print(X, 'df', i['df'], flush=True)
    if a.stage in ('orb', 'all') and not (a.resume and os.path.exists(os.path.join(a.work, X, 'orb', 'info.json'))):
        stage_orb(X, a.grasp, a.work, a.layers)
    if a.stage in ('ci', 'all'):
        ren, worst, _ = ci_orbitals(X, a.work)
        print(X, 'ci orbitals: renamed', ren, f'smallest norm after projection {worst:.3f}', flush=True)
        lv, main, mconf, ob = plan(X)
        keys = a.blocks or sorted({R.block_key(L) for i, L in lv}, key=lambda k: (int(k[1:]), k[0]))
        for key in keys:
            if a.resume and os.path.exists(os.path.join(a.work, X, key, 'info.json')): continue
            try:
                info = stage_ci(X, key, a.grasp, a.work)
                print(X, key, info['confs'], info['stages'], f"{info['seconds']} s", flush=True)
            except RuntimeError as e:
                print('FAILED', X, key, e, flush=True)


if __name__ == '__main__':
    main()
