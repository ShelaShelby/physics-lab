#!/usr/bin/env python3
"""Pack the frozen-core valence orbitals (valence.py output, $DHF_WORK/valence_Z_rel.pkl and _nr.pkl) into src/atom/valence_data.inc for the app (0.46),
and write data/dhf/valence_summary.tsv (every orbital's removal energy and <r>, the check against Safronova et al. 1999 Table I).

    DHF_WORK=<folder> python3 native/tools/dhf/make_valence_data.py

Each orbital part: P = sum dP_i r^(l+1) exp(-alpha_i r^2), Q = sum dQ_i ((l+1+kappa)/r - 2 alpha_i r) r^(l+1) exp(-alpha_i r^2), alpha_i = amin ratio^i (atomic units),
the same form as data/dhf/dhf.bin (src/atom/hf.h Part). Nonrelativistic data: one part per (n, l) (2j = 2l + 1), dQ = 0.
"""
import os, sys, pickle
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
import run_atom as R
from valence import HARTREE_CM, CORES
SAFRONOVA = {19: ((4, 0, 1), 32370), 37: ((5, 0, 1), 30571), 55: ((6, 0, 1), 27954)}   # Table I, zeroth order (DHF), cm-1 (data/alkali/safronova_1999_physics9906044.pdf)

def main():
    out = ['// Made by native/tools/dhf/make_valence_data.py from tools/dhf/valence.py (frozen-core Dirac-Hartree-Fock valence orbitals, 0.46). Do not edit.',
           '// kValence: Z, relativistic, n, l, 2j, orbital energy (hartree), <r> (bohr), amin, ratio, nb, offset of dP in kValenceCoef (dQ follows, nb values).']
    coef = []; rows = []; summ = ['Z\tmodel\torbital\tremoval_cm-1\t<r>_bohr\tSafronova1999_DHF_cm-1']
    for Z in sorted(CORES):
        for rel in (True, False):
            r = pickle.load(open(os.path.join(R.WORK, f"valence_{Z}_{'rel' if rel else 'nr'}.pkl"), 'rb'))
            for (n, l, j2), o in sorted(r['orb'].items()):
                off = len(coef); coef += list(o['dP']) + list(o['dQ'])
                rows.append(f"    {{{Z}, {1 if rel else 0}, {n}, {l}, {j2}, {o['eps']!r}, {o['rexp']!r}, {r['amin']!r}, {r['ratio']!r}, {r['nb']}, {off}}},")
                ref = SAFRONOVA.get(Z)
                saf = str(ref[1]) if ref and rel and ref[0] == (n, l, j2) else ''
                summ.append(f"{Z}\t{'DHF' if rel else 'HF (nonrel.)'}\t{n}{'spdf'[l]}{j2}/2\t{-o['eps'] * HARTREE_CM:.2f}\t{o['rexp']:.6f}\t{saf}")
    out.append('static const double kValenceCoef[] = {')
    for i in range(0, len(coef), 6): out.append('    ' + ', '.join(repr(float(x)) for x in coef[i:i + 6]) + ',')
    out.append('};')
    out.append('struct ValenceRow { int Z, rel, n, l, j2; double eps, rexp, amin, ratio; int nb, off; };')
    out.append('static const ValenceRow kValence[] = {'); out += rows; out.append('};')
    open(os.path.join(R.REPO, 'src', 'atom', 'valence_data.inc'), 'w').write('\n'.join(out) + '\n')
    open(os.path.join(R.REPO, 'data', 'dhf', 'valence_summary.tsv'), 'w').write('\n'.join(summ) + '\n')
    print(len(rows), 'orbital parts,', len(coef), 'coefficients')

if __name__ == '__main__': main()
