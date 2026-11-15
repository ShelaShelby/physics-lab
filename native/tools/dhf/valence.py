"""Frozen-core (V^(N-1)) Dirac-Hartree-Fock valence orbitals of the one-valence-electron atoms (0.46).

The closed-shell core of the ion (Li+ = [He], Na+ = [Ne], K+ = [Ar], Rb+ = [Kr], Cs+ = [Xe]) is solved self-consistently (radhf.Atom.scf);
every valence orbital (ns, np1/2, np3/2, nd3/2, nd5/2, ...) is then an eigenfunction of that core's Fock operator: the valence electron moves in the
field of the fixed core (direct and exchange), the "frozen-core DHF" starting point of e.g. M. S. Safronova, W. R. Johnson and A. Derevianko,
Phys. Rev. A 60, 4476 (1999) (data/alkali). The core is NOT relaxed for each excited orbital: the model the app draws (core kept fixed).
Same Hamiltonian, nucleus and basis family as run_atom.py, with a wider basis (smaller smallest exponent) for the diffuse excited orbitals.

    python3 native/tools/dhf/valence.py Z [rel|nr]     -> $DHF_WORK/valence_Z_rel.pkl (or _nr), prints energies (cm-1) and <r> (bohr)
"""
import sys, os, math, pickle, numpy as np, scipy.linalg
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from radhf import *
import run_atom as R

HARTREE_CM = 219474.63136314          # CODATA 2022 hartree in cm-1 (R_inf * 2 = 2 x 109737.31568157 cm-1)
CORES = {3: 'He', 11: 'Ne', 19: 'Ar', 37: 'Kr', 55: 'Xe'}
VALENCE = {   # the orbitals of the lowest NIST levels each atom's jumps use (n, l)
    3: [(2, 0), (2, 1), (3, 0), (3, 1), (3, 2), (4, 0), (4, 1)],
    11: [(3, 0), (3, 1), (4, 0), (3, 2), (4, 1), (5, 0), (4, 2)],
    19: [(4, 0), (4, 1), (5, 0), (3, 2), (5, 1), (4, 2), (6, 0)],
    37: [(5, 0), (5, 1), (4, 2), (6, 0), (6, 1), (5, 2), (7, 0)],
    55: [(6, 0), (6, 1), (5, 2), (7, 0), (7, 1), (6, 2), (8, 0)],
}

def fock(a, subs, l, j2):
    """the closed-shell core's Fock operator in the (l, j) block: h + sum_b g_b (J_b - exchange)"""
    U = {b: a.radial(b) for b in subs}
    V = a.Vn.copy(); K = []
    for b in subs:
        gb = b[2] + 1
        V = V + gb * a.ypot(U[b][0] ** 2 + U[b][1] ** 2, 0)
        for k in range(abs(l - b[1]), l + b[1] + 2):
            gm = gamma(l, j2, b[1], b[2], k)
            if gm: K.append((gb * gm, U[b], k))
    H, S = a.onebody(l, j2, V)
    return H - a.exchange(l, j2, K), S

def run(Z, rel=True, amin=0.0002):
    A = R.default_A(Z)
    cfg = R.expand('[' + CORES[Z] + ']')
    a = Atom(Z, A=A, rel=rel, amin=amin)
    subs, Q, W = al_weights(cfg)
    a.scf(subs, Q, W)
    assert a.converged, Z
    out = dict(Z=Z, A=A, rel=rel, core=CORES[Z], E_core=a.E, orb={}, amin=float(a.alpha[0]), ratio=float(a.alpha[1] / a.alpha[0]), nb=a.nb, c=a.c)
    core_rexp = {s: a.rexp(s) for s in subs}
    out['core_rexp'] = core_rexp
    for (n, l) in VALENCE[Z]:
        for j2 in ([2 * l - 1, 2 * l + 1] if l else [1]) if rel else [2 * l + 1]:
            H, S = fock(a, subs, l, j2)
            e, cv = scipy.linalg.eigh(H, S)
            off = a.nb if rel else 0
            v = cv[:, off + n - l - 1]
            v = v / math.sqrt(a.norm2((n, l, j2), v))
            P, Qs = a.radial((n, l, j2), v)
            if P[np.argmax(np.abs(P))] < 0: v = -v; P = -P; Qs = -Qs   # sign: the largest lobe positive
            eps = float(e[off + n - l - 1])
            rex = float(((P * P + Qs * Qs) * a.r * a.w).sum())
            tail = float(((P * P + Qs * Qs) * a.w)[a.r > 0.6 * a.r[-1]].sum())
            B = a.basis(l, j2); graw = a.r ** (l + 1) * np.exp(-a.alpha[:, None] * a.r * a.r); ng = np.sqrt((graw * graw * a.w).sum(1))
            dP = v[:a.nb] / ng; dQ = v[a.nb:] / (ng * 2 * a.c) if rel else np.zeros(a.nb)
            out['orb'][(n, l, j2)] = dict(eps=eps, rexp=rex, tail=tail, dP=dP, dQ=dQ, P=P, Q=Qs)
    out['r'] = a.r; out['w'] = a.w
    return out

if __name__ == '__main__':
    Z = int(sys.argv[1]); rel = (sys.argv[2] if len(sys.argv) > 2 else 'rel') == 'rel'
    os.makedirs(R.WORK, exist_ok=True)
    r = run(Z, rel)
    pickle.dump(r, open(os.path.join(R.WORK, f"valence_{Z}_{'rel' if rel else 'nr'}.pkl"), 'wb'))
    for k, o in r['orb'].items():
        print(f"Z {Z} {'rel' if rel else 'nr'} {k[0]}{'spdf'[k[1]]}{k[2]}/2  removal {-o['eps'] * HARTREE_CM:10.2f} cm-1  <r> {o['rexp']:.5f}  tail {o['tail']:.1e}", flush=True)
