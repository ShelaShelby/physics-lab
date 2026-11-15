#!/usr/bin/env python3
"""0.24: the Molecule Lab's Density style, vibrating (NOT run by the app; see data/molecules/density/README.txt).

For a molecule and each of its vibrations (src/molecule/vib_data.inc), the electron density at BOTH ENDS of the swing the picture draws when "3 times bigger" is
chosen: the atoms at R0 +- A d, d the mode's zero-point turning point (src/molecule/vibration.cpp turningPoint) and A the picture's factor (vib::drawnScale:
3, less when that would move an atom more than 0.3 angstrom). The same calculation as the still density (tools/make_density_data.py: PySCF, B3LYP, aug-cc-pVTZ,
DFT grid level 3; its own functions are used), on THE SAME GRID as the still density (its origin, step and size), with the fine cubes round the moved nuclei and the
potential on the same potential grid. The app blends the three (- , still, +) quadratically: at each moment the density for the atoms where they are
(the electrons follow the nuclei at once: the Born-Oppenheimer picture). CALCULATED, not measured.

File <id>_vib.pld, little endian, "PLDVIB1":
  char[8] magic "PLDVIB1\\0"; int32 nModes; int32 nAtoms
  per mode: int32 mode index (vib::modes order); float64 A (the picture's factor); then for the + end and then the - end:
    nAtoms x float64 x, y, z (bohr: the moved nuclei); float64 dipole x, y, z (au, from the density matrix); float64 energy (hartree)
    uint16 code[nx*ny*nz] on the still density's grid G (the same log scale, LNMIN / LNSTEP); int32 nFine; per fine cube: int32 atom, float64 hf, int32 m, uint16 code[(2m+1)^3]
    (round the MOVED nucleus); int16 potential[nPx*nPy*nPz] on the still potential grid (PSCALE au per unit)
Usage: python3 make_vib_density.py <xyz dir> <vib_data.inc> <out dir> <id> [mode ...]
"""
import os, re, struct, sys, time
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import make_density_data as D
from pyscf import gto, dft

HBAR, C, U = 1.054571817e-34, 299792458.0, 1.66053906892e-27      # CODATA 2022
MASS = {"H": 1.00782503223, "C": 12.0, "N": 14.00307400443, "O": 15.99491461957, "Cl": 34.968852682}   # NIST, as src/molecule/molecule.cpp
EXAGGERATE, MAX_DRAWN_A = 3.0, 0.3                                 # vibration.h kExaggerate, kMaxDrawnA

def modes_of(inc, mid, n):
    out = []
    for l in open(inc):
        m = re.match(r'\{"(\w+)", ([-\d.]+), ([-\d.e]+), \{(.*)\}\},', l)
        if m and m.group(1) == mid: out.append((float(m.group(2)), np.array([float(v) for v in m.group(4).split(",")])[:3 * n]))
    out.sort(key=lambda t: t[0])                                    # (vibration.cpp sorts by the calculated wavenumber, stable)
    return out

def scf(syms, R, spin, basis):
    mol = gto.M(atom=[(s, tuple(p)) for s, p in zip(syms, R)], unit="B", basis=basis, spin=spin, verbose=0)
    mf = dft.UKS(mol) if spin else dft.RKS(mol)
    mf.xc = "b3lyp"; mf.grids.level = 3; mf.conv_tol = 1e-10; mf.max_cycle = 200
    mf.kernel()
    if not mf.converged: raise SystemExit("SCF did not converge")
    dm = mf.make_rdm1()
    if spin: dm = dm[0] + dm[1]
    return mol, mf, dm

def main():
    xyzdir, inc, outdir, mid = sys.argv[1:5]
    only = [int(v) for v in sys.argv[5:]]
    atoms = D.read_xyz(os.path.join(xyzdir, mid + ".xyz"))           # bohr, as the still density
    syms = [s for s, _ in atoms]; R0 = np.array([p for _, p in atoms]); n = len(syms)
    spin = D.SPIN.get(mid, 0); basis = os.environ.get("PL_BASIS_" + mid, D.BASIS)
    lo, hi = R0.min(0) - D.MARGIN, R0.max(0) + D.MARGIN              # the still density's grids, exactly
    loG, nG, g = D.axes(lo, hi, D.HC)
    loP, nP, gp = D.axes(lo, hi, D.HP)
    mass = np.array([MASS[s] for s in syms])
    modes = modes_of(inc, mid, n)
    ck = os.path.join(outdir, "ckpt_vibdens_" + mid); os.makedirs(ck, exist_ok=True)
    rows = []
    for k, (nu, L) in enumerate(modes):
        if only and k not in only: continue
        w = 2 * np.pi * C * nu * 100
        dA = (L.reshape(n, 3) * np.sqrt(HBAR / w) / np.sqrt(mass * U)[:, None]) * 1e10 / D.A0_ANGSTROM   # the turning point, bohr
        big = np.linalg.norm(dA, axis=1).max() * D.A0_ANGSTROM
        A = max(1.0, min(EXAGGERATE, MAX_DRAWN_A / big)) if big > 0 else EXAGGERATE
        ends = []
        for sgn in (1, -1):
            f = os.path.join(ck, f"{k}_{'p' if sgn > 0 else 'm'}.npz")
            if not os.path.exists(f):                                # (each end saved at once: the cloud machine restarts while idle)
                t0 = time.time()
                R = R0 + sgn * A * dA
                mol, mf, dm = scf(syms, R, spin, basis)
                code = D.quant(D.density_at(mol, dm, g))
                Z = mol.atom_charges(); fine = []
                for a in range(n):
                    if Z[a] == 1: continue
                    hf = D.HF[int(Z[a])]; m = int(np.rint(D.R0 / hf)); a1 = hf * np.arange(-m, m + 1)
                    cube = np.stack(np.meshgrid(a1, a1, a1, indexing="ij"), -1).transpose(2, 1, 0, 3).reshape(-1, 3)
                    fine.append(np.concatenate([[a, hf, m], D.quant(D.density_at(mol, dm, cube + R[a])).astype(float)]))
                pot = np.clip(np.rint(D.esp_at(mol, dm, gp) / D.PSCALE), -32767, 32767).astype(np.int16)
                dip = np.asarray(mf.dip_moment(unit="AU", verbose=0))
                np.savez(f + ".tmp.npz", R=R, dip=dip, e=mf.e_tot, code=code, pot=pot, fine=np.array(fine, dtype=object), allow_pickle=True)
                os.replace(f + ".tmp.npz", f)
                print(f"{mid} mode {k} ({nu:.0f} cm-1) {'+' if sgn > 0 else '-'} end: {time.time() - t0:.0f} s, dipole {np.round(dip * D.AU_DEBYE, 4)} D", flush=True)
            ends.append(np.load(f, allow_pickle=True))
        rows.append((k, A, ends))
    with open(os.path.join(outdir, mid + "_vib.pld"), "wb") as fo:
        fo.write(b"PLDVIB1\0"); fo.write(struct.pack("<2i", len(rows), n))
        for k, A, ends in rows:
            fo.write(struct.pack("<id", k, A))
            for z in ends:
                for p in z["R"]: fo.write(struct.pack("<3d", *p))
                fo.write(struct.pack("<4d", *z["dip"], float(z["e"])))
                fo.write(z["code"].astype("<u2").tobytes())
                fine = z["fine"]
                fo.write(struct.pack("<i", len(fine)))
                for fc in fine:
                    fo.write(struct.pack("<idi", int(fc[0]), float(fc[1]), int(fc[2]))); fo.write(np.asarray(fc[3:], dtype=float).astype("<u2").tobytes())
                fo.write(z["pot"].astype("<i2").tobytes())
    print("wrote", os.path.join(outdir, mid + "_vib.pld"), len(rows), "modes")

if __name__ == "__main__":
    main()
