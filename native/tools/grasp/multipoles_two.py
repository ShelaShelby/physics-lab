#!/usr/bin/env python3
"""From the finished GRASP2018 runs of tools/grasp/run_two.py to the app's clouds of the two-s-electron atoms (physics-lab native 0.47.3).

    python3 native/tools/grasp/multipoles_two.py Mg [--work $HOME/gwork2]      -> native/data/jumps/grasp/Mg/{levels.tsv, clouds.bin}

Each RCI root is named by its LS composition (jj2lsj, <block>/<final>.lsj.lbl): the weights of its components summed per (valence configuration, term). In each block (parity, J)
the roots are paired with the NIST levels of the block's reference configurations (all of them, not only the wanted ones) so that the summed weight is largest (an assignment);
a wanted level whose root has less than MINW of its weight in the NIST level's configuration and term is left out (named in levels.tsv as "not identified").
The cloud of each level: natural orbitals and the M_J = J multipoles exactly as tools/grasp/multipoles.py (level_cloud, group, resample); clouds.bin format "PLG2": as "PLG1"
(multipoles.py) with each level's NIST configuration and term (int length + bytes, twice) before its record; n, l of the record = the outer valence orbital.
"""
import argparse, json, os, re, shutil, struct, subprocess, sys
import numpy as np
from scipy.optimize import linear_sum_assignment

HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE); sys.path.insert(0, os.path.dirname(HERE))
import gfiles, multipoles as M, make_level_data as LD
from run_two import ATOMS, block_key, cname
HC = M.HC
MINW = 0.30
# a level whose calculated excitation energy is more than QMAX % from NIST's gets no cloud (listed with the figure): a cloud that far off is not shown (as francium's 9s, 0.47.2)
QMAX = 5.0
LET = 'spdfg'


def lsj(path, core_n):
    """[(E, {(valence tuple, 'term'): weight})] per root from a .lsj.lbl file"""
    out = []
    for line in open(path):
        f = line.split()
        if len(f) >= 5 and f[0].isdigit() and f[3].startswith('-'):
            out.append((float(f[3]), {}))
        elif len(f) == 3 and out and '_' in f[2]:
            conf, term = f[2].rsplit('_', 1)
            val = LD.valence('.'.join(re.sub(r'\((\d+)\)', r'\1', t) for t in conf.split('.')), core_n)
            key = (tuple(val) if val else None, term)
            out[-1][1][key] = out[-1][1].get(key, 0.0) + float(f[1])
    return out


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('atom'); ap.add_argument('--work', default=os.path.expanduser('~/gwork2'))
    a = ap.parse_args(); X = a.atom
    Z, A_, closed, pre, n0 = ATOMS[X]
    At = LD.atom(X, runs=False); lv = At['levels']
    for L in lv: L.setdefault('val', LD.valence(L['conf'], n0))
    want = [i for i, L in enumerate(lv) if L['cloud']]
    got, notes, root_of = {}, {}, {}
    for key in sorted({block_key(lv[i]) for i in want}):
        d = os.path.join(a.work, X, key)
        if not os.path.exists(os.path.join(d, 'info.json')):
            for i in want:
                if block_key(lv[i]) == key: notes[i] = 'block not computed'
            continue
        inf = json.load(open(os.path.join(d, 'info.json')))
        ow = gfiles.read_w(os.path.join(d, inf['final'] + '.w'))
        om, lvs = gfiles.read_mp(os.path.join(d, inf['final'] + '.mp'))
        names = lsj(os.path.join(d, inf['final'] + '.lsj.lbl'), n0)
        assert len(names) == len(lvs), (key, len(names), len(lvs))
        refs = {tuple(lv[i]['val']) for i in want if block_key(lv[i]) == key}
        J2 = int(key[1:]) * 2; odd = key[0] == 'o'
        cand = [i for i, L in enumerate(lv) if L['val'] and tuple(L['val']) in refs and L['J2'] == J2 and L['odd'] == odd]
        W = np.array([[names[r][1].get((tuple(lv[i]['val']), lv[i]['term'].rstrip('*')), 0.0) for i in cand] for r in range(len(lvs))])
        rr, cc = linear_sum_assignment(-W)
        for r, c in zip(rr, cc):
            i = cand[c]
            if i not in want: continue
            if W[r, c] < MINW: notes[i] = f'not identified (best root has {W[r, c]:.2f} of its weight in this term)'; continue
            got[i] = (lvs[r], W[r, c], ow, om, inf); root_of[i] = (key, int(r) + 1)
        for i in want:
            if block_key(lv[i]) == key and i not in got and i not in notes: notes[i] = 'no root left for it'
    if 0 not in got: raise SystemExit(f'{X}: the ground level has no cloud')
    # 0.47.4 (run_shared atoms): a level holding an orbital whose Dirac-Fock did not converge (the hydrogen-like start kept) gets no cloud, whatever its energy: Hg's 6s7d came
    # out +4.7 % only because its 7d piled up with 6d (+12 %) near 80,700 cm-1
    dfi = os.path.join(a.work, X, 'df', 'info.json')
    if 'recipe' in got[0][4] and os.path.exists(dfi):
        bad = {d[0] for d in json.load(open(dfi))['df'] if 'hydrogen-like' in str(d[1])}
        for i in list(got):
            hl = sorted({f'{n}{LET[l]}' for n, l in lv[i]['val']} & bad)
            if i and hl: notes[i] = f'the Dirac-Fock of its {", ".join(hl)} orbital did not converge (no cloud from a stand-in orbital)'; del got[i]
    # 0.47.4: natural orbitals under 0.01 electrons drawn together per l for the run_shared atoms (their levels hold about 50 natural orbitals; the lump keeps the density
    # exact and cut Ca's clouds.bin from 2.2 to 1.7 MB: decision D3's 10-20 MB, REPORT.md)
    if 'recipe' in got[0][4] and 'PL_LUMP' not in os.environ: M.LUMP = 1e-2
    # balance: a level of a block that kept k correlation layers is measured from the ground level computed with the same k layers (RCI of the ground block's saved
    # layer k, cached in <work>/X/e0/g<k>): with the ground's 3 layers against 1 layer, Mg 3s3p 1P1 came out +10 % instead of a few tenths (0.47.3)
    g_final = got[0][4]['final']
    def ground_E(fin):
        if fin in (g_final, 'L0'): return got[0][0]['E']
        d = os.path.join(a.work, X, 'e0', 'g' + fin)
        mpf = os.path.join(d, fin + '.mp')
        if not os.path.exists(mpf):
            os.makedirs(d, exist_ok=True)
            for ext in ('.c', '.w'): shutil.copy(os.path.join(a.work, X, 'e0', fin + ext), os.path.join(d, fin + ext))
            shutil.copy(os.path.join(a.work, X, 'e0', 'isodata'), os.path.join(d, 'isodata'))
            bins = os.path.expanduser('~/grasp/bin')
            subprocess.run([os.path.join(bins, 'rci')], input=f'y\n{fin}\ny\ny\n1.d-6\ny\nn\nn\ny\n4\n1\n', text=True, cwd=d, capture_output=True)
            subprocess.run([os.path.join(bins, 'rmultipole')], input=f'{fin}\ny\n', text=True, cwd=d, capture_output=True)
        return gfiles.read_mp(mpf)[1][0]['E']
    E0 = got[0][0]['E']
    out = os.path.join(M.REPO, 'data', 'jumps', 'grasp', X); os.makedirs(out, exist_ok=True)
    shared = 'recipe' in got[0][4]
    rows = ['# ' + X + (': GRASP2018 clouds of the levels the app draws, one orbital set for the whole atom (core-valence + valence-valence MCDHF on the main configurations, '
                       'then RCI per J and parity with Breit and QED); tools/grasp/run_shared.py + multipoles_two.py, native 0.47.4.' if shared else
                       ': GRASP2018 core-valence + valence-valence MCDHF + RCI (Breit, QED) clouds of the levels the app draws; tools/grasp/run_two.py + multipoles_two.py, native 0.47.3.'),
            '# computed = excitation energy from the ground level (cm-1; CODATA 2018 hartree); diff_% = (computed - NIST) / NIST; NIST = data/jumps/asd (tools/make_level_data.py).',
            '# LS_weight = the root\'s weight in the NIST level\'s configuration and term (jj2lsj); natural_orbitals, k_max = highest multipole of M_J = J; CSFs, seconds: the block run.',
            'level\tJ\tNIST_cm\tcomputed_cm\tdiff_%\tLS_weight\tnatural_orbitals\tk_max\tCSFs\tseconds']
    blob = bytearray(b'PLG2' + struct.pack('<iidd', Z, M.NG, M.R0, M.RMAX))
    for i in list(got):
        if i == 0: continue
        lvl, w, ow, om, inf = got[i]
        q = 100 * ((lvl['E'] - ground_E(inf['final'])) * HC - lv[i]['cm']) / lv[i]['cm']
        if abs(q) > QMAX: notes[i] = f'its calculation is {q:+.1f} % from NIST (over the {QMAX:.0f} % limit)'; del got[i]
    order = sorted(got, key=lambda i: lv[i]['cm'])
    # the CI root of each level with a cloud (rates_two.py takes the computed rates of these, D4)
    json.dump({str(i): [root_of[i][0], root_of[i][1], lv[i]['conf'], lv[i]['term'], lv[i]['J2']] for i in order}, open(os.path.join(a.work, X, 'roots.json'), 'w'))
    blob += struct.pack('<i', len(order))
    worst = {'int': 0, 'mean_r': 0}
    for i in order:
        L = lv[i]; lvl, w, ow, om, inf = got[i]
        g, nos, D, chk = M.level_cloud(ow, om, lvl, Z, None)
        comp = (lvl['E'] - ground_E(inf['final'])) * HC; ni = L['cm']
        outer = max(L['val'], key=lambda o: (o != (n0, 0), o[0] + 0.7 * o[1]))
        rows.append(f'{L["conf"]} {L["term"]}\t{L["J2"] // 2}\t{ni}\t{comp:.1f}\t' + (f'{100 * (comp - ni) / ni:+.3f}' if ni else '') +
                    f'\t{w:.3f}\t{len(nos)}\t{max(D)}\t{inf["stages"][-1][1]}\t{inf["seconds"]}')
        for s_ in (L['conf'], L['term']):
            bs = s_.encode(); blob += struct.pack('<i', len(bs)) + bs
        subs = M.group(nos)
        blob += struct.pack('<iiiiddii', outer[0], outer[1], L['J2'], 0, comp, ni, len(subs), len(D) - 1)
        for sn, sl, parts, lump in subs:
            blob += struct.pack('<iiii', sn, sl, len(parts), int(lump))
            for pj, po, P, Q in parts:
                rg, Ps = M.resample(g, P); _, Qs = M.resample(g, Q)
                blob += struct.pack('<id', pj, po) + Ps.astype('<f4').tobytes() + Qs.astype('<f4').tobytes()
                rr_ = np.exp(np.linspace(np.log(1e-6), np.log(M.RMAX), 20001))
                dd = M.app_eval(rg, Ps.astype('<f4'), rr_, sl) ** 2 + M.app_eval(rg, Qs.astype('<f4'), rr_, sl) ** 2
                nr = np.trapezoid(dd, rr_); mr = np.trapezoid(dd * rr_, rr_) / nr
                ex = gfiles.integrate(g, (P ** 2 + Q ** 2) * g)
                if abs(nr - 1) > worst['int']: worst['which'] = f'{L["conf"]} {L["term"]} J={L["J2"] // 2}: {sn}{LET[sl]} j={pj}/2 occ {po:.3g}' + (' (lump)' if lump else '')
                worst['int'] = max(worst['int'], abs(nr - 1)); worst['mean_r'] = max(worst['mean_r'], abs(mr - ex) / ex)
        for k in sorted(D):
            if k == 0: continue
            _, Dk = M.resample(g, D[k])
            blob += struct.pack('<i', k) + Dk.astype('<f4').tobytes()
    for i, why in sorted(notes.items(), key=lambda kv: lv[kv[0]]['cm']):
        rows.append(f'# no cloud: {lv[i]["conf"]} {lv[i]["term"]} J={lv[i]["J2"] // 2} ({lv[i]["cm"]} cm-1): {why}')
    open(os.path.join(out, 'clouds.bin'), 'wb').write(bytes(blob))
    rows.append(f'# stored grid: {M.NG} points, r = {M.R0} .. {M.RMAX} bohr (log); natural orbitals under {M.LUMP} electrons drawn together per l. '
                f'Interpolation check: norm within {worst["int"]:.1e} (worst: {worst.get("which", "-")}), <r> within {worst["mean_r"]:.1e} (relative). clouds.bin: {len(blob)} bytes.')
    open(os.path.join(out, 'levels.tsv'), 'w').write('\n'.join(rows) + '\n')
    print('\n'.join(rows[3:]))


if __name__ == '__main__':
    main()
