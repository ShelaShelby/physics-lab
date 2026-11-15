"""Radial Dirac-Hartree-Fock (Dirac-Coulomb) for free atoms, for a weighted average of
relativistic configurations (GRASP's 'extended average level'; the plain average level of a
nonrelativistic configuration is a special case).

Subshell a = (n, l, 2j): one radial pair u_a = (P_a, Q_a), P = r R_large, Q = r R_small.
Basis: even-tempered Gaussians g_i = r^(l+1) exp(-alpha_i r^2) for P and the restricted
kinetic-balance partners f_i = g_i' + kappa g_i / r (times 1/2c) for Q.
Matrices (energy from +mc^2):  [[V_LL, T], [T, W/(4c^2) - T]] x = E [[S, 0], [0, T/(2c^2)]] x,
with T = <f|f>/2, W = <f|V|f>.

Energy for subshell occupations Q_a and pair weights W_ab (averages over the configurations):
  E = sum_a Q_a <a|h|a> + 1/2 sum_ab W_ab g_a g_b [F0(a,b) - sum_k gamma^k_ab G^k(a,b)],  g_a = 2j_a+1
  gamma^k_ab = (j_a j_b k; 1/2 -1/2 0)^2 for l_a + l_b + k even, 0 otherwise.
One configuration c with q_a electrons in a: w_ab = q_a q_b/(g_a g_b) (a != b), q_a(q_a-1)/(g_a(g_a-1)) (a = b).
Shell Fock operator: F_a = h + sum_b (W_ab / f_a) g_b [V0_bb - sum_k gamma^k_ab V^k_ab u_b], f_a = Q_a/g_a.
Closed subshells share one operator; an open subshell gets its own, coupled to the closed ones of
the same kappa by a Guest-Saunders operator so that <c|F_c - f_o F_o|o> = 0 at convergence.

Nucleus: Gaussian charge, Visscher & Dyall ADNDT 67, 207 (1997):
  xi = 1.5/R_rms^2, R_rms = (0.836 A^(1/3) + 0.570) fm;  V(r) = -Z erf(sqrt(xi) r)/r.
"""
import math, re, itertools, numpy as np, scipy.linalg
from functools import lru_cache
from scipy.integrate import cumulative_simpson
from scipy.special import erf

LET = 'spdfghi'
C_CODATA2022 = 137.035999177          # 1/alpha, CODATA 2022
FM_PER_BOHR = 52917.7249              # Visscher & Dyall's a0 in fm (for their nuclear radius formula only)

def parse_config(s):
    out = {}
    for tok in s.split():
        m = re.fullmatch(r'(\d+)([spdfg])(\d+(?:\.\d+)?)?', tok)
        n, l, q = int(m.group(1)), LET.index(m.group(2)), float(m.group(3) or 1)
        out[(n, l)] = out.get((n, l), 0) + q
    return out

def _f(x2): return math.factorial(x2 // 2)

@lru_cache(None)
def threej2(j1, j2, j3, m1, m2, m3):
    """Wigner 3j symbol, all arguments doubled (Racah's formula)."""
    if m1 + m2 + m3 != 0 or j3 > j1 + j2 or j3 < abs(j1 - j2): return 0.0
    if abs(m1) > j1 or abs(m2) > j2 or abs(m3) > j3 or (j1 + j2 + j3) % 2: return 0.0
    if (j1 + m1) % 2 or (j2 + m2) % 2 or (j3 + m3) % 2: return 0.0
    pre = math.sqrt(_f(j1 + j2 - j3) * _f(j1 - j2 + j3) * _f(-j1 + j2 + j3) / _f(j1 + j2 + j3 + 2)
                    * _f(j1 + m1) * _f(j1 - m1) * _f(j2 + m2) * _f(j2 - m2) * _f(j3 + m3) * _f(j3 - m3))
    s = 0.0
    for t in range(0, 2 * (j1 + j2 + j3) + 2, 2):
        a = [t, j1 + j2 - j3 - t, j1 - m1 - t, j2 + m2 - t, j3 - j2 + m1 + t, j3 - j1 - m2 + t]
        if min(a) < 0: continue
        d = 1.0
        for x in a: d *= _f(x)
        s += (-1) ** (t // 2) / d
    return (-1) ** ((j1 - j2 - m3) // 2) * pre * s

def gamma(la, j2a, lb, j2b, k):
    if (la + lb + k) % 2: return 0.0
    return threej2(j2a, j2b, 2 * k, 1, -1, 0) ** 2

def kappa_of(l, j2): return -(l + 1) if j2 == 2 * l + 1 else l

def nucleus_xi(A):
    return 1.5 / ((0.836 * A ** (1.0 / 3) + 0.570) / FM_PER_BOHR) ** 2

def subshells_of(n, l): return [(n, l, j2) for j2 in ([2 * l - 1, 2 * l + 1] if l else [1])]

def al_weights(cfg):
    """Average level of a nonrelativistic configuration {(n,l): q}: (Q_a, W_ab)."""
    subs = [s for A in sorted(cfg) for s in subshells_of(*A)]
    Q = {}; W = {}
    for a in subs:
        A = a[:2]; gA = 4 * a[1] + 2; Q[a] = cfg[A] * (a[2] + 1) / gA
    for a in subs:
        for b in subs:
            A, B = a[:2], b[:2]
            if A == B:
                q = cfg[A]; g = 4 * A[1] + 2; W[a, b] = q * (q - 1) / (g * (g - 1))
            else:
                W[a, b] = (cfg[A] / (4 * A[1] + 2)) * (cfg[B] / (4 * B[1] + 2))
    return subs, Q, W

def eal_weights(confs):
    """Weighted relativistic configurations [(weight, {(n,l,2j): q})]: (Q_a, W_ab)."""
    subs = sorted(set(a for _, c in confs for a, q in c.items()))
    Q = {a: 0.0 for a in subs}; W = {(a, b): 0.0 for a in subs for b in subs}
    for wc, c in confs:
        for a in subs:
            qa = c.get(a, 0); ga = a[2] + 1; Q[a] += wc * qa
            for b in subs:
                qb = c.get(b, 0); gb = b[2] + 1
                W[a, b] += wc * (qa * (qa - 1) / (ga * (ga - 1)) if a == b else qa * qb / (ga * gb))
    subs = [a for a in subs if Q[a] > 1e-12]
    return subs, Q, W

class Atom:
    def __init__(self, Z, A=None, rel=True, c=C_CODATA2022, ratio=1.9, amin=0.004, grid=(-14.0, 5.0, 1601)):
        self.Z = Z; self.rel = rel; self.c = c
        t0, t1, nt = grid
        self.t = np.linspace(t0, t1, nt); self.h = self.t[1] - self.t[0]
        self.r = np.exp(self.t); self.w = self.r * self.h
        r = self.r
        if A is None:
            self.Vn = -Z / r; amax = 2e3 * Z * Z
        else:
            self.xi = nucleus_xi(A); self.Vn = -Z * erf(np.sqrt(self.xi) * r) / r
            amax = 40 * self.xi if rel else min(40 * self.xi, 2e3 * Z * Z)   # (the NR limit has no r^(gamma-1) core: smaller exponents suffice)
        nb = int(math.ceil(math.log(amax / amin) / math.log(ratio))) + 1
        self.alpha = amin * ratio ** np.arange(nb); self.nb = nb
        self._basis = {}
        self.vec = {}

    def basis(self, l, j2):
        if (l, j2) in self._basis: return self._basis[(l, j2)]
        r = self.r; k = kappa_of(l, j2); a = self.alpha[:, None]
        g = r ** (l + 1) * np.exp(-a * r * r)
        f = ((l + 1) / r - 2 * a * r) * g + k * g / r
        ng = np.sqrt((g * g * self.w).sum(1)); g = g / ng[:, None]; f = f / ng[:, None]
        B = dict(g=g, f=f, fs=f / (2 * self.c), S=(g * self.w) @ g.T, T=0.5 * (f * self.w) @ f.T)
        B['chi'] = np.vstack([g, B['fs']]) if self.rel else g
        self._basis[(l, j2)] = B
        return B

    def cum(self, y):
        c = cumulative_simpson(y * self.r, dx=self.h, axis=-1)
        return np.concatenate([np.zeros(c.shape[:-1] + (1,)), c], axis=-1)

    def ypot(self, rho, k):
        """V^k[rho](r) = int rho(s) r<^k / r>^(k+1) ds."""
        r = self.r
        inner = self.cum(rho * r ** k); cout = self.cum(rho * r ** (-k - 1))
        return inner / r ** (k + 1) + (cout[..., -1:] - cout) * r ** k

    def radial(self, a, vec=None):
        vec = self.vec[a] if vec is None else vec
        B = self.basis(a[1], a[2]); nb = self.nb
        P = vec[:nb] @ B['g']
        Q = vec[nb:] @ B['fs'] if self.rel else np.zeros_like(P)
        return P, Q

    def norm2(self, a, v):
        B = self.basis(a[1], a[2]); nb = self.nb
        if not self.rel: return v @ B['S'] @ v
        return v[:nb] @ B['S'] @ v[:nb] + v[nb:] @ B['T'] @ v[nb:] / (2 * self.c ** 2)

    def onebody(self, l, j2, V):
        B = self.basis(l, j2); g, f = B['g'], B['f']; w = self.w; c = self.c; nb = self.nb
        VLL = (g * (V * w)) @ g.T
        if not self.rel: return VLL + B['T'], B['S']
        Wm = (f * (V * w)) @ f.T
        H = np.block([[VLL, B['T']], [B['T'], Wm / (4 * c * c) - B['T']]])
        S = np.block([[B['S'], np.zeros((nb, nb))], [np.zeros((nb, nb)), B['T'] / (2 * c * c)]])
        return H, S

    def exchange(self, l, j2, terms):
        """sum over (coef, u_b, k): coef * <chi_i u_b | r<^k/r>^k+1 | u_b chi_j>"""
        B = self.basis(l, j2); chi = B['chi']; nb = self.nb
        X = 0
        for coef, (Pb, Qb), k in terms:
            if self.rel:
                rho = np.vstack([B['g'] * Pb, B['fs'] * Qb])
            else:
                rho = B['g'] * Pb
            X = X + coef * (rho * self.w) @ self.ypot(rho, k).T
        return 0.5 * (X + np.transpose(X)) if not isinstance(X, int) else 0

    def guess(self, subs, Q):
        Z = self.Z; N = sum(Q.values())
        b = 0.8853 * Z ** (-1.0 / 3); x = self.r / b
        phi = 1.0 / (1 + 0.02747 * x ** 0.5 + 1.243 * x - 0.1486 * x ** 1.5 + 0.2302 * x ** 2 + 0.007298 * x ** 2.5 + 0.006944 * x ** 3)
        zeff = np.maximum(Z - N + 1, Z * phi)
        V = self.Vn + Z / self.r - zeff / self.r
        for (l, j2) in sorted(set(a[1:] for a in subs)):
            H, S = self.onebody(l, j2, V)
            e, cv = scipy.linalg.eigh(H, S)
            off = self.nb if self.rel else 0
            for a in subs:
                if a[1:] == (l, j2):
                    v = cv[:, off + a[0] - l - 1]; self.vec[a] = v / math.sqrt(self.norm2(a, v))

    def energy(self, subs, Q, W):
        U = {a: self.radial(a) for a in subs}
        E = 0.0
        for a in subs:
            H, S = self.onebody(a[1], a[2], self.Vn); E += Q[a] * (self.vec[a] @ H @ self.vec[a])
        rho = {a: U[a][0] ** 2 + U[a][1] ** 2 for a in subs}
        Y0 = {a: self.ypot(rho[a], 0) for a in subs}
        for a in subs:
            for b in subs:
                w = W[a, b]
                if w == 0: continue
                F0 = (rho[a] * Y0[b] * self.w).sum(); G = 0.0
                rab = U[a][0] * U[b][0] + U[a][1] * U[b][1]
                for k in range(abs(a[1] - b[1]), a[1] + b[1] + 2):
                    gm = gamma(a[1], a[2], b[1], b[2], k)
                    if gm: G += gm * (rab * self.ypot(rab, k) * self.w).sum()
                E += 0.5 * w * (a[2] + 1) * (b[2] + 1) * (F0 - G)
        return E

    def scf(self, subs, Q, W, maxit=400, tol=1e-7, mix=0.5, verbose=False, guess=True):
        nb = self.nb
        if guess or any(a not in self.vec for a in subs): self.guess(subs, Q)
        g = {a: a[2] + 1 for a in subs}; f = {a: Q[a] / g[a] for a in subs}
        closedset = set(a for a in subs if abs(Q[a] - g[a]) < 1e-12 and all(abs(W[a, b] - f[b]) < 1e-12 for b in subs))
        Fold = {}
        for it in range(maxit):
            U = {a: self.radial(a) for a in subs}
            Y0 = {a: self.ypot(U[a][0] ** 2 + U[a][1] ** 2, 0) for a in subs}
            newvec = {}
            for (l, j2) in sorted(set(a[1:] for a in subs)):
                blk = sorted([a for a in subs if a[1:] == (l, j2)])
                closed = [a for a in blk if a in closedset]; opn = [a for a in blk if a not in closedset]
                assert len(opn) <= 1 and (not opn or all(c[0] < opn[0][0] for c in closed)), blk
                ops = {}
                for a in (closed[:1] + opn):
                    V = self.Vn.copy(); K = []
                    for b in subs:
                        coef = W[a, b] / f[a] * g[b]
                        if coef == 0: continue
                        V = V + coef * Y0[b]
                        for k in range(abs(l - b[1]), l + b[1] + 2):
                            gm = gamma(l, j2, b[1], b[2], k)
                            if gm: K.append((coef * gm, U[b], k))
                    H, S = self.onebody(l, j2, V)
                    H = H - self.exchange(l, j2, K)
                    key = (a in closedset, l, j2)
                    if key in Fold: H = mix * H + (1 - mix) * Fold[key]
                    Fold[key] = H; ops[a in closedset] = (H, S)
                off = nb if self.rel else 0
                if not opn:
                    H, S = ops[True]; e, cv = scipy.linalg.eigh(H, S)
                    for r_, a in enumerate(closed): newvec[a] = cv[:, off + r_]
                    continue
                o = opn[0]; fo = f[o]; Ho, S = ops[False]; Hc = ops[True][0] if closed else Ho
                # Guest-Saunders operator written with S-metric projectors (no explicit complement):
                # blocks cc: F_c, oo: F_o, vv: F_c, co: F_c - f F_o, ov: F_o, cv: F_c
                Cc = np.array([self.vec[a] for a in closed]).T.reshape(S.shape[0], len(closed))
                ov = self.vec[o][:, None]
                Sc = S @ Cc; So = S @ ov
                Qm = np.eye(S.shape[0]) - Sc @ Cc.T - So @ ov.T        # = S P_virtual
                G = Hc - fo * Ho
                R = (Sc @ (Cc.T @ Hc @ Cc) @ Sc.T + So @ (ov.T @ Ho @ ov) @ So.T + Qm @ Hc @ Qm.T
                     + Sc @ (Cc.T @ G @ ov) @ So.T + Qm @ Ho @ ov @ So.T + Qm @ Hc @ Cc @ Sc.T)
                R = R + (Sc @ (Cc.T @ G @ ov) @ So.T + Qm @ Ho @ ov @ So.T + Qm @ Hc @ Cc @ Sc.T).T
                e, cv = scipy.linalg.eigh(0.5 * (R + R.T), S)
                Ub = np.eye(S.shape[0])
                cand = [k for k in range(len(e)) if (not self.rel) or e[k] > -self.c ** 2]
                for r_, a in enumerate(closed + [o]): newvec[a] = Ub @ cv[:, cand[r_]]
            dv = 0.0
            for a, v in newvec.items():
                v = v / math.sqrt(self.norm2(a, v))
                if a in self.vec and v @ self.vec[a] < 0: v = -v
                if a in self.vec:      # change of the radial functions on the grid (P and Q are O(1) or less)
                    P1, Q1 = self.radial(a, v); P0, Q0 = self.radial(a)
                    dv = max(dv, np.abs(P1 - P0).max(), np.abs(Q1 - Q0).max())
                else: dv = 1.0
                newvec[a] = v
            self.vec.update(newvec)
            if verbose: print(it, dv, flush=True)
            if dv < tol:
                self.converged = True; break
        else:
            self.converged = False
        self.E = self.energy(subs, Q, W); self.subs = subs; self.Q = Q
        return self.E

    def rexp(self, a, p=1):
        P, Q = self.radial(a); return ((P * P + Q * Q) * self.r ** p * self.w).sum()

    def eps(self, a, subs, Q, W):
        """orbital energy <a|F_a|a> (Koopmans-like, for checks)."""
        g = {b: b[2] + 1 for b in subs}; f = Q[a] / g[a]
        U = {b: self.radial(b) for b in subs}
        V = self.Vn.copy(); K = []
        for b in subs:
            coef = W[a, b] / f * g[b]
            if coef == 0: continue
            V = V + coef * self.ypot(U[b][0] ** 2 + U[b][1] ** 2, 0)
            for k in range(abs(a[1] - b[1]), a[1] + b[1] + 2):
                gm = gamma(a[1], a[2], b[1], b[2], k)
                if gm: K.append((coef * gm, U[b], k))
        H, S = self.onebody(a[1], a[2], V); H = H - self.exchange(a[1], a[2], K)
        return self.vec[a] @ H @ self.vec[a]
