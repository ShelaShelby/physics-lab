#!/usr/bin/env python3
"""Pack the Dirac-Hartree-Fock atoms (tools/dhf/run_atom.py output) into data/dhf/dhf.bin for the app, and write data/dhf/summary.tsv
(and data/dhf/visscher_check.tsv and koga_point_check.tsv when the vis and point runs are there).

    DHF_WORK=<folder of prod_Z.pkl / vis_Z.pkl> python3 native/tools/dhf/make_dhf_data.py

dhf.bin, little endian (read by src/atom/hf.cpp):
  "PLDHF001", u32 records; each record: u16 Z, u16 A, i16 2J (-1 if none), u8 relativistic, u8 parts, f64 energy (hartree),
  three strings (u8 length + bytes: ground shells, level, source); then per part: u8 n, u8 l, u8 2j, u8 nb, f64 occ, f64 eps (hartree),
  f64 amin, f64 ratio, f64 dP[nb], f64 dQ[nb]   with   P = sum dP_i r^(l+1) exp(-alpha_i r^2),
  Q = sum dQ_i ((l+1+kappa)/r - 2 alpha_i r) r^(l+1) exp(-alpha_i r^2),  alpha_i = amin ratio^i  (atomic units).
Two records per element He..Og: the relativistic one, then the nonrelativistic limit (one part per (n,l), dQ = 0).
"""
import os, sys, struct, pickle, math
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from radhf import kappa_of, LET
import run_atom as R

OUT = os.path.join(R.REPO, 'data', 'dhf')

def sstr(s):
    b = s.encode('utf-8'); return struct.pack('<B', len(b)) + b

def part_bytes(n, l, j2, occ, eps, basis, s):
    dP, dQ = basis['coef'][s]
    nb = basis['nb']
    return (struct.pack('<BBBBdddd', n, l, j2, nb, occ, eps, basis['amin'], basis['ratio'])
            + struct.pack(f'<{nb}d', *dP) + struct.pack(f'<{nb}d', *dQ))

def main():
    recs = []; rows = []
    for Z in range(2, 119):
        f = os.path.join(R.WORK, f'prod_{Z}.pkl')
        partial = '--partial' in sys.argv                       # (a trial build from the runs done so far: never for the app's data)
        if not os.path.exists(f):
            if partial: continue
            sys.exit(f'missing {f}')
        r = pickle.load(open(f, 'rb'))
        if 'basis' not in r:
            if partial: continue
            sys.exit(f'{f}: no coefficients (run it again with the current run_atom.py)')
        J2 = int(round(2 * r['J'])) if r.get('J') is not None else -1
        parts = [part_bytes(s[0], s[1], s[2], r['Q'][s], r['eps'][s], r['basis'], s) for s in sorted(r['subs'])]
        recs.append(struct.pack('<HHhBBd', Z, r['A'], J2, 1, len(parts), r['E']) + sstr(r['shells']) + sstr(r['level']) + sstr(r['src']) + b''.join(parts))
        nr = r['nr']; nparts = []
        for s in sorted(nr['Q']):
            n, l, j2 = s
            if j2 != 2 * l + 1: continue                      # (the nonrelativistic j = l - 1/2 radial function is the same)
            occ = sum(q for t, q in nr['Q'].items() if t[:2] == (n, l))
            nparts.append(part_bytes(n, l, j2, occ, 0.0, nr['basis'], s))
        recs.append(struct.pack('<HHhBBd', Z, r['A'], -1, 0, len(nparts), nr['E']) + sstr(r['shells']) + sstr(r['level']) + sstr(r['src']) + b''.join(nparts))
        lev = r.get('levels', [0.0])
        first = next((x for x in lev[1:] if x > 1e-9), float('nan'))
        rows.append([Z, r['shells'], r['level'], r['src'], r['Jref'], r['J'], 'yes' if r['Jref'] is not None and abs(r['J'] - r['Jref']) < 1e-3 else 'NO',
                     f"{r['E']:.8f}", f"{nr['E']:.8f}", r['conv'], nr['conv'], r.get('cycles', 0), r.get('ndet', 1),
                     f"{first * 219474.6313632:.1f}", ' '.join(f"{s[0]}{LET[s[1]]}{s[2]}/2:{r['Q'][s]:.4f}" for s in sorted(r['subs']) if 0 < r['Q'][s] < s[2] + 1 - 1e-9)])
    blob = b'PLDHF001' + struct.pack('<I', len(recs)) + b''.join(recs)
    open(os.path.join(OUT, 'dhf.bin'), 'wb').write(blob)
    with open(os.path.join(OUT, 'summary.tsv'), 'w') as fo:
        fo.write('# made by tools/dhf/make_dhf_data.py from the runs of tools/dhf/run_atom.py (prod mode). J_ref: NIST ASD 5.12 (Z <= 108) or Smits et al. 2023 (Z >= 109);\n'
                 '# J_CI: the ground level of the configuration interaction in the open shells; E: total energies (hartree), relativistic and nonrelativistic limit;\n'
                 '# first_excited_cm1: the next level of that CI (Dirac-Coulomb, no Breit, no correlation: a check, not a prediction); open: open subshells and their electrons\n')
        fo.write('\t'.join(['Z', 'shells', 'level', 'source', 'J_ref', 'J_CI', 'J_match', 'E_rel', 'E_nonrel', 'converged', 'nonrel_converged', 'ci_cycles', 'determinants', 'first_excited_cm1', 'open']) + '\n')
        for row in rows: fo.write('\t'.join(str(x) for x in row) + '\n')
    vis = []
    for Z in range(1, 110):
        f = os.path.join(R.WORK, f'vis_{Z}.pkl')
        if os.path.exists(f):
            v = pickle.load(open(f, 'rb'))
            for s, ref in sorted(v['ref'].items()):
                vis.append([Z, v['cfg'], f'{s[0]}{LET[s[1]]}{s[2]}/2', f'{ref:.7e}', f"{v['rexp'][s]:.7e}", f"{v['rexp'][s] / ref - 1:+.2e}", v['conv']])
    if vis:
        with open(os.path.join(OUT, 'visscher_check.tsv'), 'w') as fo:
            fo.write('# <r> (bohr) of every orbital: Visscher & Dyall 1997 Table IV (Gaussian nucleus, their configuration and c) against tools/dhf run in the same configuration (vis mode)\n')
            fo.write('Z\tconfiguration\tsubshell\tvisscher\tthis\trel_diff\tconverged\n')
            for row in vis: fo.write('\t'.join(str(x) for x in row) + '\n')
    pts = []
    for Z in (80, 86, 88, 102):
        f = os.path.join(R.WORK, f'point_{Z}.pkl')
        if os.path.exists(f):
            v = pickle.load(open(f, 'rb'))
            pts += [[Z, s[0], s[1], f"{x:.9e}", v['conv']] for s, x in sorted(v['rexp'].items())]
    if pts:
        with open(os.path.join(OUT, 'koga_point_check.tsv'), 'w') as fo:
            fo.write('# <r> (bohr) of every orbital of the nonrelativistic limit with a POINT nucleus (tools/dhf/run_atom.py point Z; the model of Koga et al.), for the tests\n')
            fo.write('Z\tn\tl\tr_mean\tconverged\n')
            for row in pts: fo.write('\t'.join(str(x) for x in row) + '\n')
    print(f'{len(recs)} records, {len(blob)} bytes; J mismatches: {[r[0] for r in rows if r[6] != "yes"]}; vis rows {len(vis)}')

if __name__ == '__main__':
    main()
