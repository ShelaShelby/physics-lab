#!/usr/bin/env python3
"""0.23 stage 1: the Molecule Lab's vibrations (NOT run by the app; see data/molecules/vib/README.txt).

For each of the 15 viewer molecules, at EXACTLY the app's geometry (data/molecules/xyz/<id>.xyz, the measured CCCBDB shape the other styles show):
  - a Kohn-Sham DFT calculation (PySCF, B3LYP, BASIS below, DFT grid level 3; O2: UKS triplet, the rest RKS),
  - the analytic Hessian (second derivatives of the energy, hartree / bohr^2),
  - the harmonic normal modes: the Hessian divided by sqrt(m_i m_j) with the app's isotope masses (NIST, the most abundant isotope), the 3 translations and
    3 rotations (2 for a linear molecule) projected out, then diagonalised. Each mode: its harmonic wavenumber and its mass-weighted unit vector,
  - the dipole derivatives by central differences of the SCF dipole (step 0.005 bohr on every Cartesian coordinate), and from them each mode's
    harmonic (double-harmonic) infrared intensity in km/mol.
Everything here is CALCULATED, not measured. The measured fundamentals (NIST CCCBDB / NIST WebBook) are a separate table (src/molecule/vibration.cpp) and stay empty
until a source can be read.

Constants: CODATA 2022 (E_h = 4.3597447222060e-18 J, a0 = 5.29177210544e-11 m, u = 1.66053906892e-27 kg, c = 299792458 m/s, e = 1.602176634e-19 C,
N_A = 6.02214076e23 /mol, epsilon0 = 8.8541878188e-12 F/m).
Intensity: A = N_A / (12 epsilon0 c^2) |d mu / d Q|^2 (SI, m/mol), Q the mass-weighted normal coordinate (the standard integrated absorption coefficient of a
harmonic band, e.g. Wilson, Decius and Cross, Molecular Vibrations, 1955; written from knowledge, the book was not opened in this session).

Usage: python3 make_vib_data.py <xyz dir> <out .inc> <out sheet dir> [id ...]      (pip install pyscf numpy)
"""
import sys, os, time
import numpy as np
from pyscf import gto, dft, __version__ as PYSCF_VERSION

BASIS = os.environ.get("VIB_BASIS", "aug-cc-pvdz")
STEP = 0.005                                   # bohr, the dipole difference step
EH, A0, U, C, E, NA, EPS0 = 4.3597447222060e-18, 5.29177210544e-11, 1.66053906892e-27, 299792458.0, 1.602176634e-19, 6.02214076e23, 8.8541878188e-12
MASS = {"H": 1.00782503223, "C": 12.0, "N": 14.00307400443, "O": 15.99491461957, "Cl": 34.968852682}   # NIST, as src/molecule/molecule.cpp
IDS = ["H2", "N2", "O2", "HCl", "CO2", "H2O", "O3", "NH3", "CH4", "HCN", "C2H2", "H2CO", "C2H4", "CH3OH", "C6H6"]

def read_xyz(path):
    L = open(path).read().splitlines()
    n = int(L[0])
    at = [l.split() for l in L[2:2 + n]]
    return [a[0] for a in at], np.array([[float(v) for v in a[1:4]] for a in at])

def scf(mid, syms, xyz_bohr, verbose=0):
    m = gto.M(atom=[(s, tuple(p)) for s, p in zip(syms, xyz_bohr)], basis=BASIS, unit="Bohr", spin=2 if mid == "O2" else 0, verbose=verbose)
    mf = dft.UKS(m) if mid == "O2" else dft.RKS(m)
    mf.xc = "b3lyp"; mf.grids.level = 3; mf.conv_tol = 1e-11
    mf.kernel()
    if not mf.converged: raise RuntimeError(mid + ": SCF did not converge")
    return m, mf

def ext_basis(x, mass, linear):
    """the translations and rotations, mass-weighted, orthonormal (Eckart)"""
    n = len(mass); sm = np.sqrt(mass)
    com = (mass[:, None] * x).sum(0) / mass.sum(); r = x - com
    v = []
    for k in range(3):
        t = np.zeros((n, 3)); t[:, k] = sm; v.append(t.ravel())
    for k in range(3):
        e = np.zeros(3); e[k] = 1
        v.append((np.cross(e, r) * sm[:, None]).ravel())
    V = np.array(v).T
    q, s, _ = np.linalg.svd(V, full_matrices=False)
    keep = s > 1e-6 * s.max()
    return q[:, keep]

def main():
    xyzdir, out_inc, sheetdir = sys.argv[1], sys.argv[2], sys.argv[3]
    ids = sys.argv[4:] or IDS
    os.makedirs(sheetdir, exist_ok=True)
    rows = []
    for mid in ids:
        t0 = time.time()
        part = os.path.join(sheetdir, mid + ".inc.part")      # each molecule's lines, kept as soon as it is done (a stopped run starts again where it was)
        if os.path.exists(part):
            rows.append((mid, None, None, None, open(part).read())); print(mid + ": kept from an earlier run", flush=True); continue
        syms, xa = read_xyz(os.path.join(xyzdir, mid + ".xyz"))
        x = xa / (A0 * 1e10)                    # angstrom -> bohr
        n = len(syms)
        mass = np.array([MASS[s] for s in syms])
        m, mf = scf(mid, syms, x)
        e0 = mf.e_tot
        grad = mf.nuc_grad_method().kernel()
        fd = os.environ.get("VIB_FD") == "1"
        if fd:
            # 0.23: restart-proof: the Hessian and the dipole derivatives together, by central differences of the analytic gradient and of the dipole
            # (the same 6N displaced calculations), each displaced result saved at once in <sheet dir>/ckpt_<id>/ (a stopped run goes on where it was)
            ck = os.path.join(sheetdir, "ckpt_" + mid + "_" + BASIS); os.makedirs(ck, exist_ok=True)
            G = np.zeros((3 * n, 2, 3 * n)); D = np.zeros((3 * n, 2, 3))
            # Symmetry (VIB_SYM=1): only one atom of each set of equivalent atoms is displaced; the rows of the others follow exactly from the molecule's own
            # symmetry operations R (atom a -> perm[a]): H[perm(a) al, perm(c) ga] = sum R[al,be] R[ga,de] H[a be, c de], dmu[perm(a) al] = sum R[al,be] R dmu[a be]
            sym = os.environ.get("VIB_SYM") == "1"
            ops, rep = [], {}
            if sym:
                sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
                from make_vib_measured import symmetry_ops
                ops = list(symmetry_ops(syms, x).values()) + [(np.eye(3), list(range(n)))]
                reps = []
                for a in range(n):                               # rep[a] = (the displaced atom u, an operation taking u to a)
                    hit = next(((u, (R, pm)) for u in reps for R, pm in ops if pm[u] == a), None)
                    if hit is None: reps.append(a); hit = (a, (np.eye(3), list(range(n))))
                    rep[a] = hit
                print(f"  {mid}: {len(ops)} symmetry operations; displaced atoms {sorted(set(v[0] for v in rep.values()))}", flush=True)
            for i in range(3 * n):
                if sym and rep[i // 3][0] != i // 3: continue           # filled in from its equivalent atom below
                for si, sgn in enumerate((1, -1)):
                    f = os.path.join(ck, f"{i}_{si}.npz")
                    if not os.path.exists(f):
                        xx = x.copy().ravel(); xx[i] += sgn * STEP
                        _, mfd = scf(mid, syms, xx.reshape(n, 3))
                        np.savez(f + ".tmp.npz", g=mfd.nuc_grad_method().kernel().ravel(), d=np.array(mfd.dip_moment(unit="AU", verbose=0)))
                        os.replace(f + ".tmp.npz", f)
                        print(f"  {mid}: displacement {2 * i + si + 1} of {6 * n} saved", flush=True)
                    z = np.load(f); G[i, si] = z["g"]; D[i, si] = z["d"]
            if sym:
                for b in range(n):
                    u, (R, pm) = rep[b]
                    if u == b: continue
                    for si in range(2):
                        for al in range(3):
                            g = np.zeros(3 * n); dd = np.zeros(3)
                            for be in range(3):
                                if abs(R[al, be]) < 1e-12: continue
                                src = G[3 * u + be, si].reshape(n, 3) @ R.T          # the gradient field turned by R ...
                                moved = np.zeros((n, 3))
                                for c in range(n): moved[pm[c]] = src[c]            # ... and carried to the atoms R maps onto
                                g += R[al, be] * moved.ravel(); dd += R[al, be] * (R @ D[3 * u + be, si])
                            G[3 * b + al, si] = g; D[3 * b + al, si] = dd
            H = (G[:, 0, :] - G[:, 1, :]) / (2 * STEP)                       # H[i, j] = d g_j / d x_i, hartree / bohr^2
            H = (H + H.T) / 2
            if sym:                                                      # the average over the group: removes the differences' noise that breaks the symmetry
                uniq = []
                for R, pm in ops:
                    if not any(np.allclose(R, R2, atol=1e-8) and pm == pm2 for R2, pm2 in uniq): uniq.append((R, pm))
                Ps = []
                for R, pm in uniq:
                    Pm = np.zeros((3 * n, 3 * n))
                    for a in range(n): Pm[3 * pm[a]:3 * pm[a] + 3, 3 * a:3 * a + 3] = R
                    Ps.append((Pm, R))
                H = sum(Pm @ H @ Pm.T for Pm, _ in Ps) / len(Ps)
                Dd = (D[:, 0, :] - D[:, 1, :]) / (2 * STEP)              # dmu[i, :]: d mu / d x_i
                Dd = sum(Pm @ Dd @ R.T for Pm, R in Ps) / len(Ps)
                D[:, 0, :] = Dd * STEP; D[:, 1, :] = -Dd * STEP          # (so the code below gives Dd back)
                print(f"  {mid}: averaged over {len(Ps)} symmetry operations", flush=True)
        else:
            H = mf.Hessian().kernel().transpose(0, 2, 1, 3).reshape(3 * n, 3 * n)   # (atom i, atom j, xyz, xyz) -> (3i+a, 3j+b), hartree / bohr^2
            H = (H + H.T) / 2
        mw = np.repeat(mass, 3)
        Hm = H / np.sqrt(np.outer(mw, mw))      # hartree / (bohr^2 u)
        linear = n == 2 or np.linalg.matrix_rank(xa - xa.mean(0), tol=1e-6) <= 1
        Q = ext_basis(x, mass, linear)
        P = np.eye(3 * n) - Q @ Q.T
        lam, vec = np.linalg.eigh(P @ Hm @ P)
        nvib = 3 * n - (5 if linear else 6)
        # the vibrations: the eigenvectors outside the translation / rotation space
        overlap = np.linalg.norm(Q.T @ vec, axis=0)
        order = [k for k in np.argsort(overlap) if True][:nvib]
        order = sorted(order, key=lambda k: lam[k])
        conv = EH / (A0 ** 2 * U)               # hartree / (bohr^2 u) -> s^-2
        modes = []
        # dipole derivatives, e (au: e a0 per a0)
        dmu = np.zeros((3 * n, 3))
        for i in range(3 * n):
            if fd: dmu[i] = (D[i, 0] - D[i, 1]) / (2 * STEP); continue
            d = []
            for sgn in (1, -1):
                xx = x.copy().ravel(); xx[i] += sgn * STEP
                _, f = scf(mid, syms, xx.reshape(n, 3))
                d.append(f.dip_moment(unit="AU", verbose=0))
            dmu[i] = (np.array(d[0]) - np.array(d[1])) / (2 * STEP)
        for k in order:
            L = vec[:, k]
            if L[np.argmax(np.abs(L))] < 0: L = -L            # a fixed sign: the largest component positive (the picture does not care)
            nu = np.sign(lam[k]) * np.sqrt(abs(lam[k]) * conv) / (2 * np.pi * C) / 100   # cm^-1 (negative = imaginary)
            dmudq = (dmu * (L / np.sqrt(mw))[:, None]).sum(0)          # e / sqrt(u)
            dmudq_si = dmudq * E / np.sqrt(U)                            # C / sqrt(kg)
            A = NA / (12 * EPS0 * C ** 2) * float(dmudq_si @ dmudq_si) / 1000   # km/mol
            modes.append((nu, A, L.reshape(n, 3)))
        txt = "".join(f'{{"{mid}", {nu:.4f}, {A:.8f}, {{' + ", ".join(f"{v:.9f}" for v in L.ravel()) + "}},\n" for nu, A, L in modes)
        open(part, "w").write(txt)
        rows.append((mid, n, e0, float(np.abs(grad).max()), txt))
        with open(os.path.join(sheetdir, mid + ".txt"), "w") as fo:
            fo.write(f"{mid}: B3LYP/{BASIS}, PySCF {PYSCF_VERSION}, at the app's (measured, CCCBDB) geometry; CALCULATED, not measured\n")
            fo.write(f"total energy {e0:.10f} hartree; largest gradient component {np.abs(grad).max():.2e} hartree/bohr (the measured shape is not the B3LYP minimum)\n")
            fo.write("mode  wavenumber/cm-1  IR intensity/(km/mol)\n")
            for j, (nu, A, _) in enumerate(modes): fo.write(f"{j + 1:4d}  {nu:12.2f}  {A:14.4f}\n")
        print(f"{mid}: {nvib} modes, {time.time() - t0:.0f} s:", " ".join(f"{nu:.0f}({A:.1f})" for nu, A, _ in modes), flush=True)
    with open(out_inc, "w") as fo:
        fo.write(f"// Made by tools/make_vib_data.py (B3LYP/{BASIS}, PySCF {PYSCF_VERSION}, analytic Hessian, dipole derivatives by central differences of {STEP} bohr). CALCULATED, not measured.\n")
        fo.write("// One entry per mode: molecule id, harmonic wavenumber (cm^-1), harmonic IR intensity (km/mol), then the mass-weighted unit vector (x, y, z per atom, the app's frame).\n")
        for mid, n, e0, g, txt in rows:
            fo.write(txt)
    print("wrote", out_inc)

if __name__ == "__main__":
    main()
