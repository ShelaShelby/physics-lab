"""Readers for GRASP2018 files (physics-lab 0.47.2): the radial wave function file (.w, Fortran unformatted 'G92RWF') and rmultipole's .mp text."""
import numpy as np

def records(path):
    b = open(path, 'rb').read(); i = 0
    while i < len(b):
        n = int.from_bytes(b[i:i + 4], 'little'); yield b[i + 4:i + 4 + n]; i += n + 8

def read_w(path):
    """[(n, kappa, energy, r, P, Q, p0)], arrays on GRASP's own grid (atomic units); p0: P / r^gamma at the origin"""
    rec = list(records(path))
    assert rec[0][:6] == b'G92RWF', path
    out = []
    for k in range(1, len(rec), 3):
        n, kap = np.frombuffer(rec[k][:8], '<i4'); e = np.frombuffer(rec[k][8:16], '<f8')[0]; mf = int(np.frombuffer(rec[k][16:20], '<i4')[0])
        a = np.frombuffer(rec[k + 1], '<f8'); r = np.frombuffer(rec[k + 2], '<f8')
        assert len(a) == 1 + 2 * mf and len(r) == mf
        out.append((int(n), int(kap), float(e), r.copy(), a[1:1 + mf].copy(), a[1 + mf:].copy(), float(a[0])))
    return out

def read_mp(path):
    """orbitals [(n, kappa)] and levels [{'J2', 'parity', 'E', 'R': {k: {(a, b): value}}}] (orbital indices from 0)"""
    orbs, levels = [], []
    for line in open(path):
        f = line.split()
        if not f or f[0] == '#': continue
        if f[0] == 'orb': orbs.append((int(f[2]), int(f[3])))
        elif f[0] == 'level': levels.append({'J2': int(f[2]), 'parity': int(f[3]), 'E': float(f[4]), 'R': {}})
        elif f[0] == 'R': levels[-1]['R'].setdefault(int(f[1]), {})[(int(f[2]) - 1, int(f[3]) - 1)] = float(f[4])
    return orbs, levels

def ell(kappa): return kappa if kappa > 0 else -kappa - 1
def j2(kappa): return 2 * abs(kappa) - 1


def write_w(path, orbs):
    """writes a G92RWF file: orbs = [(n, kappa, energy, r, P, Q)] (GRASP interpolates onto its own grid when it reads one: rwfnestimate option 1)"""
    def rec(b): return len(b).to_bytes(4, 'little') + b + len(b).to_bytes(4, 'little')
    out = rec(b'G92RWF')
    for o in orbs:
        n, kap, e, r, P, Q = o[:6]
        l = ell(kap); pz = o[6] if len(o) > 6 else (P[1] / r[1] ** (l + 1) if r[0] == 0 else P[0] / r[0] ** (l + 1))
        out += rec(np.array([n, kap], '<i4').tobytes() + np.array([e], '<f8').tobytes() + np.array([len(r)], '<i4').tobytes())
        out += rec(np.concatenate([[pz], P, Q]).astype('<f8').tobytes())
        out += rec(np.asarray(r, '<f8').tobytes())
    open(path, 'wb').write(out)


def hydrogenic(n, kappa, zeff, r):
    """nonrelativistic hydrogen-like P_nl(r) for charge zeff (normalised), Q from the kinetic balance Q = (dP/dr + kappa P / r) / 2c; energy zeff^2 / 2n^2 (hartree, bound: positive as GRASP stores it)"""
    from scipy.special import genlaguerre, factorial
    l = ell(kappa); rho = 2 * zeff * r / n
    norm = np.sqrt((2 * zeff / n) ** 3 * factorial(n - l - 1) / (2 * n * factorial(n + l)))
    P = r * norm * np.exp(-rho / 2) * rho ** l * genlaguerre(n - l - 1, 2 * l + 1)(rho)
    c = 137.035999084
    dP = np.gradient(P, r)
    Q = np.where(r > 0, (dP + kappa * P / np.where(r > 0, r, 1)) / (2 * c), 0.0)
    return P, Q, zeff ** 2 / (2 * n * n)


def pad(a, n):
    return np.concatenate([a, np.zeros(n - len(a))]) if len(a) < n else a[:n]


def integrate(r, f):
    """integral of f dr on GRASP's grid r_i = RNT (exp((i - 1) h) - 1) (Simpson in i; r[0] = 0)"""
    h = np.log(r[2] / r[1] - 1); rnt = r[1] / np.expm1(h)
    g = f * (r + rnt) * h
    from scipy.integrate import simpson
    return simpson(g, dx=1.0)


def overlap(a, b):
    n = max(len(a[3]), len(b[3])); r = a[3] if len(a[3]) == n else b[3]
    return integrate(r, pad(a[4], n) * pad(b[4], n) + pad(a[5], n) * pad(b[5], n))


def grasp_grid(r, N=590):
    """GRASP's whole grid r_i = RNT (exp((i - 1) h) - 1), i = 1..N, from any orbital's (shorter) copy of it"""
    h = np.log(r[2] / r[1] - 1); rnt = r[1] / np.expm1(h)
    return rnt * np.expm1(h * np.arange(N))


def onto(o, g):
    """an orbital put on the grid g (linear interpolation in r; zero beyond its end), cut where it has become negligible"""
    n, kap, e, r, P, Q = o[:6]
    Pi = np.interp(g, r, P, right=0.0); Qi = np.interp(g, r, Q, right=0.0)
    big = np.nonzero(np.abs(Pi) > 1e-14 * np.abs(Pi).max())[0][-1] + 2
    return (n, kap, e, g[:big], Pi[:big], Qi[:big]) + tuple(o[6:])
