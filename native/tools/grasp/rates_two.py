#!/usr/bin/env python3
"""D4 (owner decision 9 Nov 2026; native 0.47.4): this app's own transition rates A for the lines between levels with clouds, from the same GRASP2018 calculation.

    GRASP_NP=4 python3 native/tools/grasp/rates_two.py Ca [--work $HOME/gw4]     -> native/data/jumps/grasp/Ca/rates.tsv

Needs the finished blocks of run_shared.py and <work>/X/roots.json (multipoles_two.py: the CI root of each level). Per pair of blocks joined by a line: rbiotransform
(biorthogonal orbitals; here both blocks share one orbital set, it then only reorders the mixing) and rtransition (E1) in <work>/X/tr_<a>_<b>. The line strength S of the two
roots, in the length (Babushkin) and velocity (Coulomb) forms, is turned into A with NIST's wavelength:
    A = S / (4.935525e-19 g_k lambda^3),   lambda in angstrom, g_k = 2 J_k + 1 of the upper level, S in atomic units
(NIST ASD lines help, "Conversion between transition probabilities and line strengths", E1; data/jumps/sources/asd_lines_help.html). The length form is the one the app shows;
how far the velocity form's A is from it (gauge_%) is a rough sign of the calculation's quality. Nothing here is NIST's except the wavelength and, for comparison, NIST's A.
"""
import argparse, json, os, re, shutil, sys
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE); sys.path.insert(0, os.path.dirname(HERE))
import make_level_data as LD
from run_level import Run

K_E1 = 4.935525e-19


def parse_ct(path):
    """{(f_up, lev_up, f_lo, lev_lo): {'C': (A, S), 'B': (A, S)}} from rtransition's .ct"""
    out, cur = {}, None
    num = lambda x: float(x.replace('D', 'E'))
    for line in open(path):
        m = re.match(r'\s*(f[12])\s+(\d+)\s+\S+\s+[+-]\s+(f[12])\s+(\d+)\s+\S+\s+[+-]\s+(\S+)\s+([CB])\s+(\S+)\s+(\S+)\s+(\S+)', line)
        if m:
            cur = (m.group(1), int(m.group(2)), m.group(3), int(m.group(4))); out[cur] = {m.group(6): (num(m.group(7)), num(m.group(9)))}; continue
        m = re.match(r'\s+([CB])\s+(\S+)\s+(\S+)\s+(\S+)\s*$', line)
        if m and cur: out[cur][m.group(1)] = (num(m.group(2)), num(m.group(4)))
    return out


def main():
    ap = argparse.ArgumentParser(); ap.add_argument('atom'); ap.add_argument('--work', default=os.path.expanduser('~/gw4'))
    ap.add_argument('--grasp', default=os.path.expanduser('~/grasp'))
    a = ap.parse_args(); X = a.atom; W = os.path.join(a.work, X)
    roots = {int(k): v for k, v in json.load(open(os.path.join(W, 'roots.json'))).items()}
    At = LD.atom(X); lv = At['levels']
    todo = {}
    for r in At['lines']:
        if r['lo'] in roots and r['hi'] in roots: todo.setdefault(tuple(sorted((roots[r['lo']][0], roots[r['hi']][0]))), []).append(r)
    rows = ['# ' + X + ': this app\'s E1 rates (D4) from the GRASP2018 levels of data/jumps/grasp/' + X + ' (tools/grasp/rates_two.py, native 0.47.4).',
            '# A = S / (4.935525e-19 g_k lambda^3) with NIST\'s wavelength (NIST ASD lines help); A_len: length (Babushkin) form, shown by the app; gauge_% = 100 (A_vel - A_len) / A_len.',
            '# NIST_A, grade: ASD\'s value where it has one (for comparison; the app shows NIST\'s where it exists).',
            'lower\tJ_lo\tupper\tJ_up\tlambda_nm\tS_len\tS_vel\tA_len\tgauge_%\tNIST_A\tgrade']
    for (ka, kb), lines in sorted(todo.items()):
        d = os.path.join(W, f'tr_{ka}_{kb}')
        run = Run(a.grasp, d, None)
        for k, nm in ((ka, 'a'), (kb, 'b')):
            for ext in ('c', 'w', 'cm'): shutil.copy(os.path.join(W, k, 'ci.' + ext), os.path.join(d, f'{nm}.{ext}'))
        shutil.copy(os.path.join(W, ka, 'isodata'), d)
        if not os.path.exists(os.path.join(d, 'a.b.ct')):
            out = run('rbiotransform', 'y\ny\na\nb\ny\n')
            out = run('rtransition', 'y\ny\na\nb\nE1\n')
            for f in os.listdir(d):
                if f.endswith('.TB') or f.startswith('mcp'): os.remove(os.path.join(d, f))
            shutil.rmtree(os.path.join(d, 'mpitmp'), ignore_errors=True)
        ct = parse_ct(os.path.join(d, 'a.b.ct'))
        fl = {ka: 'f1', kb: 'f2'}
        for r in lines:
            lo, hi = r['lo'], r['hi']
            kl, rl = roots[lo][:2]; kh, rh = roots[hi][:2]
            if kl == kh: continue
            e = ct.get((fl[kh], rh, fl[kl], rl)) or ct.get((fl[kl], rl, fl[kh], rh))   # (rtransition puts the computed higher level first)
            if not e or 'B' not in e or 'C' not in e: print('no rtransition entry for', lv[lo]['conf'], lv[hi]['conf']); continue
            lam = LD.num(r['obs']) if r['obs'] and LD.num(r['obs']) else LD.num(r['ritz'])
            gk = lv[hi]['J2'] + 1
            A_len = e['B'][1] / (K_E1 * gk * (lam * 10) ** 3); A_vel = e['C'][1] / (K_E1 * gk * (lam * 10) ** 3)
            rows.append(f"{lv[lo]['conf']} {lv[lo]['term']}\t{lv[lo]['J2'] // 2}\t{lv[hi]['conf']} {lv[hi]['term']}\t{lv[hi]['J2'] // 2}\t{lam}\t{e['B'][1]:.5e}\t{e['C'][1]:.5e}"
                        f"\t{A_len:.4e}\t{100 * (A_vel - A_len) / A_len:+.1f}\t{r['A']}\t{r['acc']}")
        print(X, ka, kb, len(lines), 'lines', flush=True)
    out = os.path.join(os.path.dirname(os.path.dirname(HERE)), 'data', 'jumps', 'grasp', X, 'rates.tsv')
    open(out, 'w').write('\n'.join(rows) + '\n')
    print('\n'.join(rows[4:]))


if __name__ == '__main__':
    main()
