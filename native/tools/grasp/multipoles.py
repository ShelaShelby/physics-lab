#!/usr/bin/env python3
"""From a finished GRASP2018 run (tools/grasp/run_level.py) to the app's cloud of every level (physics-lab native 0.47.2).

    python3 native/tools/grasp/multipoles.py Na [--work $HOME/gwork]      -> native/data/jumps/grasp/Na/{levels.tsv, clouds.bin}

For each level (rmultipole's R_k(a,b), tools/grasp/multipoles/rmultipole.f90):
  - the M_J = J state's density, per unit r and solid angle: D(r, theta) = (1/4 pi) sum_k D_k(r) P_k(cos theta), k even, k <= 2J, integral of D_0 dr = number of electrons, with
      D_k(r) = -(2k + 1) [ (J k J; -J 0 J) / (J 0 J; -J 0 J) ] sum_ab R_k(a,b) (P_a P_b + Q_a Q_b)(r)
    (Wigner-Eckart; the sign and scale of GRASP's reduced matrix elements fixed by the k = 0 sum = electron count and checked against the analytic one-configuration
    p3/2 and d5/2 densities (M = J): D_2 / D_0 = -1 and -10/7, D_4 / D_0 = 3/7 of the outer electron; checks below);
  - natural orbitals: the k = 0 one-body density matrix gamma_ab = -R_0(a,b) diagonalized in each kappa block; occupations sum to the electron count; D_0 = sum occ (P^2 + Q^2);
  - natural orbitals holding less than LUMP electrons are drawn together per l (their densities summed with their occupations: exact for the density, the count is kept),
    labelled with the n after the outermost shell the level holds.
The excitation energies (RCI with Breit and QED; CODATA 2018 hartree = 219474.6313632 cm-1) are the cloud's quality figure next to NIST; the app shows only NIST energies.
"""
import argparse, functools, json, os, struct, sys
import numpy as np
from scipy.interpolate import CubicSpline

HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import gfiles, levels, run_level
REPO = os.path.dirname(os.path.dirname(HERE))
HC = 219474.6313632            # cm-1 per hartree, CODATA 2018 (NIST, physics.nist.gov/cuu: hartree-inverse meter relationship / 100)
LUMP = float(os.environ.get("PL_LUMP", "1e-3"))   # (0.47.4: run_shared atoms use 1e-2, see multipoles_two.py)
R0, RMAX, NG = 2e-4, 400.0, 300   # the stored grid: r_j = R0 exp(j h), j = 0 .. NG - 1 (h = ln(RMAX / R0) / (NG - 1) = 0.0487)
LET = 'spdfg'


@functools.lru_cache(None)
def we_ratio(J2, k):
    from sympy.physics.wigner import wigner_3j
    from sympy import Rational
    J = Rational(J2, 2)
    return float(wigner_3j(J, k, J, -J, 0, J) / wigner_3j(J, 0, J, -J, 0, J))


def full(o, g):
    return gfiles.pad(o[4], len(g)), gfiles.pad(o[5], len(g))


def level_cloud(orbs_w, orbs_mp, lv, nelec, lid=None):
    """natural orbitals [(n, kappa, occ, P, Q)] and {k: D_k} on GRASP's grid g, plus checks"""
    g = gfiles.grasp_grid(orbs_w[0][3])
    byk = {(o[0], o[1]): o for o in orbs_w}
    PQ = [full(byk[key], g) for key in orbs_mp]
    R = lv['R']
    D = {}
    for k, Rk in R.items():
        s = np.zeros(len(g))
        for (a, b), v in Rk.items(): s += v * (PQ[a][0] * PQ[b][0] + PQ[a][1] * PQ[b][1])
        D[k] = -(2 * k + 1) * we_ratio(lv['J2'], k) * s
    nos = []
    for kap in sorted({o[1] for o in orbs_mp}):
        idx = [i for i, o in enumerate(orbs_mp) if o[1] == kap]
        G = np.array([[-R[0].get((a, b), 0.0) for b in idx] for a in idx])
        assert np.allclose(G, G.T, atol=1e-10), 'density matrix not symmetric'
        w, U = np.linalg.eigh(G)
        # labels: a core natural orbital (more than 1.5 electrons) takes the n of the orbital that dominates it; the most occupied of the others in the outer electron's
        # kappa is the outer electron (the level's own n); the rest take the n of their dominant orbital, or the next free n (labels only: the shell colour and name)
        taken, used = set(), set()
        order = list(np.argsort(-w))
        vkap = None
        if lid: vkap = -(lid[1] + 1) if lid[2] == 2 * lid[1] + 1 else lid[1]
        val = next((c for c in order if w[c] < 1.5), None) if kap == vkap else None
        for c in order:
            P = sum(U[i, c] * PQ[a][0] for i, a in enumerate(idx)); Q = sum(U[i, c] * PQ[a][1] for i, a in enumerate(idx))
            dom = max((i for i in range(len(idx)) if i not in taken), key=lambda i: abs(U[i, c]))
            taken.add(dom)
            if P[np.argmax(np.abs(P))] < 0: P, Q = -P, -Q
            n = lid[0] if c == val else orbs_mp[idx[dom]][0]
            if c != val and val is not None and n == lid[0]: n = None
            while n is None or n in used: n = (n or max([lid[0] if lid else 0] + list(used))) + 1
            used.add(n)
            nos.append((n, kap, float(w[c]), P, Q))
    occ = sum(x[2] for x in nos)
    d0 = sum(x[2] * (x[3] ** 2 + x[4] ** 2) for x in nos)
    chk = {'occ_sum': occ, 'int_D0': gfiles.integrate(g, D[0]), 'D0_vs_NO': float(np.max(np.abs(d0 - D[0])) / np.max(D[0])),
           'min_occ': min(x[2] for x in nos)}
    assert abs(occ - nelec) < 1e-8, chk
    assert abs(chk['int_D0'] - nelec) < 1e-3 * nelec, chk   # (0.47.3: helium's 1snp Rydberg levels reach past 100 bohr: up to 5e-4 short on GRASP's grid; the app's test allows 1e-3)
    if abs(chk['int_D0'] - nelec) > 1e-5 * nelec: print('note: density integral', chk['int_D0'], 'for a level of J2', lv['J2'], file=sys.stderr)
    assert chk['D0_vs_NO'] < 1e-8, chk
    if lv['J2'] == 1: assert all(k == 0 for k in D), 'a J = 1/2 level has only k = 0'
    return g, nos, D, chk


def group(nos):
    """[(n, l, [(j2, occ, P, Q)])]: natural orbitals by (n, l), j parts; those under LUMP electrons drawn together per l"""
    keep, small = {}, {}
    for n, kap, occ, P, Q in nos:
        l = gfiles.ell(kap); j2 = gfiles.j2(kap)
        if occ >= LUMP: keep.setdefault((n, l), []).append((j2, occ, P, Q))
        elif occ > 0: small.setdefault(l, {}).setdefault(j2, []).append((occ, P, Q))   # (0.47.3: a natural orbital with an occupation of -1e-9 or so, rounding of CI, is left out)
    out = [(n, l, sorted(parts, key=lambda p: p[0])) for (n, l), parts in keep.items()]
    top = max(n for (n, ll) in keep)
    for l, byj in small.items():
        nmax = max(top + 1, l + 1)     # labelled beyond the outermost kept shell, so no lump takes the name of a shell the level holds
        while (nmax, l) in keep: nmax += 1
        parts = []
        for j2, lst in sorted(byj.items()):
            tot = sum(x[0] for x in lst)
            if tot <= 0: continue
            P = np.sqrt(sum(x[0] * x[1] ** 2 for x in lst) / tot); Q = np.sqrt(sum(x[0] * x[2] ** 2 for x in lst) / tot)
            parts.append((j2, tot, P, Q))
        if parts: out.append((nmax, l, parts, True))
    return sorted([o if len(o) == 4 else o + (False,) for o in out], key=lambda o: (o[0], o[1]))


def resample(g, f):
    """onto the stored grid (cubic spline in ln r; 0 beyond GRASP's last point)"""
    rg = R0 * np.exp(np.arange(NG) * np.log(RMAX / R0) / (NG - 1))
    m = g > 0
    cs = CubicSpline(np.log(g[m]), f[m])
    out = cs(np.log(rg)); out[rg > g[m][-1]] = 0.0
    return rg, out


def app_eval(rg, F, r, l):
    """what the app does (src/atom/alkali.cpp, gridValue): 4-point Lagrange in ln r; below R0 F(R0) (r/R0)^(l+1); from RMAX on 0"""
    F = np.asarray(F, float)
    t = np.log(r / R0) / (np.log(RMAX / R0) / (NG - 1))
    j0 = np.clip(np.floor(t).astype(int) - 1, 0, NG - 4); x = t - j0
    y0, y1, y2, y3 = (F[j0 + i] for i in range(4))
    out = -y0 * (x - 1) * (x - 2) * (x - 3) / 6 + y1 * x * (x - 2) * (x - 3) / 2 - y2 * x * (x - 1) * (x - 3) / 2 + y3 * x * (x - 1) * (x - 2) / 6
    out = np.where(t < 0, F[0] * (r / R0) ** (l + 1), out)
    return np.where(t >= NG - 1, 0.0, out)


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('atom'); ap.add_argument('--work', default=os.path.expanduser('~/gwork'))
    ap.add_argument('--test-level', default='')   # e.g. "p3:df0": a one-configuration level kept for the tests (run_level.py one-config run)
    a = ap.parse_args(); X = a.atom
    Z = levels.Z_OF[X]; ser = run_level.series_of(levels.app_levels(X))
    nist = {(n, l, j): cm for n, l, j, cm in levels.nist_levels(X)}
    res = {}; info = {}
    for key, ns in ser.items():
        d = os.path.join(a.work, X, key)
        inf = json.load(open(os.path.join(d, 'info.json')))
        ow = gfiles.read_w(os.path.join(d, inf['final'] + '.w'))
        om, lvs = gfiles.read_mp(os.path.join(d, inf['final'] + '.mp'))
        for n, lv in zip(ns, lvs):
            lid = (n, LET.index(key[0]), int(key[1:]))
            res[lid] = (lv, level_cloud(ow, om, lv, Z, lid)); info[lid] = inf
    gnd = levels.nist_levels(X)[0][:3]
    E0 = res[gnd][0]['E']
    out = os.path.join(REPO, 'data', 'jumps', 'grasp', X); os.makedirs(out, exist_ok=True)
    rows = ['# ' + X + ': GRASP2018 core-valence MCDHF + RCI (Breit, QED) clouds of the levels the app uses; tools/grasp/run_level.py + multipoles.py, native 0.47.2.',
            '# computed = excitation energy from the ground level (cm-1; CODATA 2018 hartree); diff_% = (computed - NIST) / NIST; NIST = src/atom/alkali_data.inc (NIST ASD).',
            '# occ_outer = the occupation of the outer electron\'s natural orbital; k_max = highest multipole of the M_J = J state; CSFs, seconds: the series run (all its levels together).',
            'level\tJ\tNIST_cm\tcomputed_cm\tdiff_%\tocc_outer\tnatural_orbitals\tk_max\tCSFs\tseconds\tDF_start']
    blob = bytearray(b'PLG1' + struct.pack('<iidd', Z, NG, R0, RMAX))
    lvlist = [(lid, res[lid]) for lid in [tuple(x[:3]) for x in levels.nist_levels(X)] if lid in res]
    extra = []
    if a.test_level:
        key, name = a.test_level.split(':')
        d = os.path.join(a.work, X, key + '_' + name)
        ow = gfiles.read_w(os.path.join(d, name + '.w')); om, lvs = gfiles.read_mp(os.path.join(d, name + '.mp'))
        n0 = ser[key][0]; lid0 = (n0, LET.index(key[0]), int(key[1:])); extra.append((lid0, (lvs[0], level_cloud(ow, om, lvs[0], Z, lid0))))
    blob += struct.pack('<i', len(lvlist) + len(extra))
    worst = {'int': 0, 'mean_r': 0}
    for flag, (lid, (lv, (g, nos, D, chk))) in [(0, x) for x in lvlist] + [(1, x) for x in extra]:
        n, l, j2 = lid
        comp = (lv['E'] - E0) * HC; ni = nist[lid]
        subs = group(nos)
        val = [x for x in nos if x[0] == n and gfiles.ell(x[1]) == l and gfiles.j2(x[1]) == j2]
        if not flag:
            inf = info[lid]
            rows.append(f'{n}{LET[l]}\t{j2}/2\t{ni}\t{comp:.1f}\t' + (f'{100 * (comp - ni) / ni:+.3f}' if ni else '') + f'\t{val[0][2]:.5f}\t{len(nos)}\t{max(D)}\t'
                        f'{inf["stages"][-1][1]}\t{inf["seconds"]}\t{",".join(inf["df_start"])}')
        blob += struct.pack('<iiiiddii', n, l, j2, flag, comp, ni, len(subs), len(D) - 1)
        for sn, sl, parts, lump in subs:
            blob += struct.pack('<iiii', sn, sl, len(parts), int(lump))
            for pj, po, P, Q in parts:
                rg, Ps = resample(g, P); _, Qs = resample(g, Q)
                blob += struct.pack('<id', pj, po) + Ps.astype('<f4').tobytes() + Qs.astype('<f4').tobytes()
                # the app's interpolation keeps the norm and <r> (checked on a fine grid)
                rr = np.exp(np.linspace(np.log(1e-6), np.log(RMAX), 20001))
                dd = app_eval(rg, Ps.astype('<f4'), rr, sl) ** 2 + app_eval(rg, Qs.astype('<f4'), rr, sl) ** 2
                nr = np.trapezoid(dd, rr); mr = np.trapezoid(dd * rr, rr) / nr
                ex = gfiles.integrate(g, (P ** 2 + Q ** 2) * g)
                worst['int'] = max(worst['int'], abs(nr - 1)); worst['mean_r'] = max(worst['mean_r'], abs(mr - ex) / ex)
        for k in sorted(D):
            if k == 0: continue
            _, Dk = resample(g, D[k])
            blob += struct.pack('<i', k) + Dk.astype('<f4').tobytes()
    open(os.path.join(out, 'clouds.bin'), 'wb').write(bytes(blob))
    rows.append(f'# stored grid: {NG} points, r = {R0} .. {RMAX} bohr (log); natural orbitals under {LUMP} electrons drawn together per l. '
                f'Interpolation check: norm within {worst["int"]:.1e}, <r> within {worst["mean_r"]:.1e} (relative). clouds.bin: {len(blob)} bytes.')
    open(os.path.join(out, 'levels.tsv'), 'w').write('\n'.join(rows) + '\n')
    print('\n'.join(rows[3:]))


if __name__ == '__main__':
    main()
