"""Configuration-internal CI in the open shells (Dirac-Coulomb, determinants of spinors).

Active spinors: every (n, kappa, m) of the open nonrelativistic shells; the closed subshells are a
frozen core.  Determinants keep the number of electrons in each open nonrelativistic shell and a
fixed total M (M = 0 or 1/2, so every J level appears).  The lowest root is the ground level; its
J comes from <J^2> = |J+ Psi|^2 + M^2 + M.  Its subshell occupations and the weights of its
relativistic configurations (subshell occupation patterns) are averages over the level, the same
for every M_J (they are scalars), so the density built from them is the M_J-averaged, spherical one.

Two-electron integrals (Grant's convention):
  (ij|kl) = sum_K R^K(a_i a_j; a_k a_l) sum_q (-1)^q <i|C^K_q|j> <k|C^K_-q|l>
  <a m|C^K_q|b m'> = (-1)^(j_a - m) (j_a K j_b; -m q m') <a||C^K||b>
  <a||C^K||b> = (-1)^(j_a + 1/2) sqrt((2j_a+1)(2j_b+1)) (j_a K j_b; 1/2 0 -1/2), if l_a + K + l_b even
"""
import itertools, math, numpy as np, scipy.linalg
from radhf import threej2, gamma, LET

def red(la, ja, lb, jb, K):            # j's doubled
    if (la + K + lb) % 2: return 0.0
    return (-1) ** ((ja + 1) // 2) * math.sqrt((ja + 1) * (jb + 1)) * threej2(ja, 2 * K, jb, 1, 0, -1)

def cme(sa, ma, sb, mb, K, q):          # <a ma|C^K_q|b mb>, m's doubled, q integer
    la, ja = sa[1], sa[2]; lb, jb = sb[1], sb[2]
    r = red(la, ja, lb, jb, K)
    if r == 0: return 0.0
    return (-1) ** ((ja - ma) // 2) * threej2(ja, 2 * K, jb, -ma, 2 * q, mb) * r

class CI:
    def __init__(self, atom, cfg):
        """atom: radhf2.Atom after scf (its .vec has every subshell); cfg: {(n,l): q}."""
        self.a = atom; self.cfg = cfg
        self.open = [A for A in sorted(cfg) if 0 < cfg[A] < 4 * A[1] + 2]
        self.act_sub = [s for s in atom.vec if s[:2] in self.open]
        self.act_sub.sort()
        self.core_sub = [s for s in atom.vec if s[:2] in cfg and s[:2] not in self.open]
        self.spin = [(s, m) for s in self.act_sub for m in range(-s[2], s[2] + 1, 2)]
        self.ns = len(self.spin)
        self._ints()

    def _ints(self):
        a = self.a
        U = {s: a.radial(s) for s in a.vec}
        # one-body: h + core direct - core exchange, for each active subshell (diagonal)
        self.I = {}
        Ycore = sum((s[2] + 1) * a.ypot(U[s][0] ** 2 + U[s][1] ** 2, 0) for s in self.core_sub)
        for s in self.act_sub:
            V = a.Vn + Ycore; K = []
            for c in self.core_sub:
                for k in range(abs(s[1] - c[1]), s[1] + c[1] + 2):
                    gm = gamma(s[1], s[2], c[1], c[2], k)
                    if gm: K.append(((c[2] + 1) * gm, U[c], k))
            H, S = a.onebody(s[1], s[2], V); H = H - a.exchange(s[1], s[2], K)
            self.I[s] = a.vec[s] @ H @ a.vec[s]
        # radial Slater integrals among active subshells
        rho = {(x, y): U[x][0] * U[y][0] + U[x][1] * U[y][1] for x in self.act_sub for y in self.act_sub}
        self.R = {}
        Kmax = 2 * max(s[1] for s in self.act_sub) + 1
        for (x, y), rxy in rho.items():
            for K in range(0, Kmax + 1):
                V = a.ypot(rxy, K)
                for (z, w), rzw in rho.items():
                    self.R[x, y, z, w, K] = (rzw * V * a.w).sum()
        # spinor two-electron integrals (ij|kl)
        ns = self.ns; sp = self.spin
        g = np.zeros((ns, ns, ns, ns))
        for i, j, k, l in itertools.product(range(ns), repeat=4):
            (si, mi), (sj, mj), (sk, mk), (sl, ml) = sp[i], sp[j], sp[k], sp[l]
            if mi - mj != ml - mk: continue
            q2 = mj - mi            # <i|C_q|j> needs q = m_i - m_j  -> 3j(-mi, q, mj): q = mi - mj
            q = (mi - mj) // 2
            v = 0.0
            for K in range(0, Kmax + 1):
                c1 = cme(si, mi, sj, mj, K, q)
                if c1 == 0: continue
                c2 = cme(sk, mk, sl, ml, K, -q)
                if c2 == 0: continue
                v += self.R[si, sj, sk, sl, K] * (-1) ** abs(q) * c1 * c2
            g[i, j, k, l] = v
        self.g = g

    def dets(self, M2):
        """bit-tuples of occupied active spinors with the open-shell counts of cfg and 2M = M2."""
        groups = []
        for A in self.open:
            idx = [i for i, (s, m) in enumerate(self.spin) if s[:2] == A]
            groups.append([c for c in itertools.combinations(idx, int(round(self.cfg[A])))])
        out = []
        for combo in itertools.product(*groups):
            occ = tuple(sorted(i for c in combo for i in c))
            if sum(self.spin[i][1] for i in occ) == M2: out.append(occ)
        return out

    def hamiltonian(self, D):
        n = len(D); H = np.zeros((n, n)); index = {d: k for k, d in enumerate(D)}
        h1 = np.array([self.I[s] for s, m in self.spin]); g = self.g
        for a_, d in enumerate(D):
            occ = d; oset = set(occ)
            # diagonal
            e = sum(h1[i] for i in occ)
            for x in range(len(occ)):
                for y in range(x + 1, len(occ)):
                    i, j = occ[x], occ[y]
                    e += g[i, i, j, j] - g[i, j, j, i]
            H[a_, a_] = e
            emp = [p for p in range(self.ns) if p not in oset]
            # singles and doubles: generate, keep those in D
            for x, i in enumerate(occ):
                for p in emp:
                    nd = tuple(sorted(oset - {i} | {p}))
                    b = index.get(nd)
                    if b is None or b <= a_: continue
                    # one-body is diagonal in subshell (I has no off-diagonal), so only 2e part
                    val = sum(g[p, i, j, j] - g[p, j, j, i] for j in occ if j != i)
                    H[a_, b] = H[b, a_] = val * self._sign(d, [i], [p])
            for (x, i), (y, j) in itertools.combinations(list(enumerate(occ)), 2):
                for p, q in itertools.combinations(emp, 2):
                    nd = tuple(sorted(oset - {i, j} | {p, q}))
                    b = index.get(nd)
                    if b is None or b <= a_: continue
                    val = g[p, i, q, j] - g[p, j, q, i]
                    H[a_, b] = H[b, a_] = val * self._sign(d, [i, j], [p, q])
        return H

    @staticmethod
    def _sign(d, holes, parts):
        """sign of a+_{p..} a_{i..} acting on det d (ordered annihilate i then j, create q then p)."""
        occ = list(d); s = 1
        for h in holes[::-1] if False else holes:
            pass
        # apply a_i (first hole), a_j, then a+_q, a+_p for doubles: |..> -> a+_p a+_q a_j a_i |d>
        def ann(occ, i):
            k = occ.index(i); return (-1) ** k, occ[:k] + occ[k + 1:]
        def cre(occ, p):
            k = sum(1 for x in occ if x < p); return (-1) ** k, occ[:k] + [p] + occ[k:]
        if len(holes) == 1:
            s1, o = ann(occ, holes[0]); s2, o = cre(o, parts[0]); return s1 * s2
        i, j = holes; p, q = parts
        s1, o = ann(occ, i); s2, o = ann(o, j); s3, o = cre(o, q); s4, o = cre(o, p)
        return s1 * s2 * s3 * s4

    def solve(self):
        nel = int(round(sum(self.cfg[A] for A in self.open)))
        M2 = nel % 2
        D = self.dets(M2)
        H = self.hamiltonian(D)
        e, v = scipy.linalg.eigh(H)
        c = v[:, 0]
        # J from <J^2> = |J+ psi|^2 + M^2 + M
        jp = {}
        for k, d in enumerate(D):
            if abs(c[k]) < 1e-14: continue
            for x, i in enumerate(d):
                s, m = self.spin[i]
                if m == s[2]: continue
                pidx = self.spin.index((s, m + 2))
                if pidx in d: continue
                coef = math.sqrt((s[2] / 2) * (s[2] / 2 + 1) - (m / 2) * (m / 2 + 1))
                sgn, o = 1, list(d)
                kk = o.index(i); sgn *= (-1) ** kk; o = o[:kk] + o[kk + 1:]
                kk = sum(1 for y in o if y < pidx); sgn *= (-1) ** kk; o = o[:kk] + [pidx] + o[kk:]
                t = tuple(o); jp[t] = jp.get(t, 0) + sgn * coef * c[k]
        M = M2 / 2
        J2 = sum(x * x for x in jp.values()) + M * M + M
        J = (-1 + math.sqrt(1 + 4 * J2)) / 2
        # subshell occupations and relativistic-configuration weights
        occ = {s: 0.0 for s in self.act_sub}; confs = {}
        for k, d in enumerate(D):
            w = c[k] ** 2
            pat = tuple(sum(1 for i in d if self.spin[i][0] == s) for s in self.act_sub)
            confs[pat] = confs.get(pat, 0) + w
            for s, nn in zip(self.act_sub, pat): occ[s] += w * nn
        self.e = e; self.J = J; self.occ = occ; self.confs = confs; self.ndet = len(D)
        return J, occ, confs, e

    def levels(self, tol=1e-6):
        """the lowest few eigenvalues relative to the ground one (hartree)."""
        return self.e[:8] - self.e[0]
