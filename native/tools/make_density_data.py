#!/usr/bin/env python3
"""0.21 stage 1: the Molecule Lab "Realistic" style's data (NOT run by the app; see data/molecules/density/README.txt).

For each of the 15 viewer molecules: a Kohn-Sham DFT calculation (PySCF, B3LYP, aug-cc-pVTZ, DFT grid level 3) at EXACTLY the app's geometry
(data/molecules/xyz/<id>.xyz, written by the molecule_xyz tool from molecule::get(i)), then:
  - the electron density on a cubic grid (point values, e/bohr^3; uint16, log-scaled),
  - the same density on fine cubes round every non-hydrogen nucleus (for the integrals only: a cusp needs a fine grid),
  - the electrostatic potential (nuclei + electrons) on a coarser grid (hartree per elementary charge = atomic units; int16 with a scale),
  - the dipole moment from the exact density matrix (PySCF dip_moment, origin-independent for a neutral molecule).
Writes data/molecules/density/<id>.pld (format below) and <id>.txt (the data sheet). Every grid is COMPUTED, not measured.

Usage: python3 make_density_data.py <xyz dir> <out dir> [id ...]      (pip install pyscf numpy)

File format (little endian), "PLDENS1" -- read by src/molecule/density.cpp:
  char[8]  magic "PLDENS1\\0"
  int32    nAtoms; then per atom: int32 Z, float64 x, y, z (bohr)
  float64  dipole x, y, z (atomic units, e bohr, from the density matrix); float64 total energy (hartree); int32 electrons; int32 spin (2S)
  char[16] basis; char[8] method ("RKS"/"UKS")
  float64  lnMin, lnStep: density code k >= 1 means exp(lnMin + (k - 1) lnStep); code 0 means below exp(lnMin) (stored as 0)
  grid G:  float64 origin x, y, z (bohr); float64 h (bohr); int32 nx, ny, nz; uint16 code[nx*ny*nz], index (iz*ny + iy)*nx + ix: point values at origin + h*(ix, iy, iz)
  float64  r1, r0 (bohr): the partition of unity used to integrate: w(d) = 1 for d <= r1, 0 for d >= r0, the C-infinity step between (src/molecule/density.cpp smoothStep)
  int32    nFine; per fine cube: int32 atom index, float64 hf, int32 m; uint16 code[(2m+1)^3] at atom + hf*(i, j, k), i, j, k = -m..m, same index order
  grid P:  float64 origin x, y, z, h; int32 nx, ny, nz; float64 scale (au per unit); int16 value[nx*ny*nz] (value * scale = potential, au); -32768 = not computed
Integral of f over all space with the stored data: sum_G f rho (1 - sum_A w(|r - R_A|)) h^3 + sum_A sum_fine f rho w(|r - R_A|) hf^3 (the fine cubes cover w > 0).
"""
import struct, sys, os, time
import numpy as np
from pyscf import gto, dft, df, __version__ as PYSCF_VERSION
from pyscf.dft import numint

A0_ANGSTROM = 0.529177210544          # CODATA 2022 Bohr radius in angstrom (NIST CODATA 2022, physics.nist.gov/cuu/Constants)
AU_DEBYE = 2.5417464715             # 1 e bohr in debye: CODATA 2022 e * a0 * c * 1e21 (1 D = 1e-21 / c C m) = 2.54174647147
BASIS = "aug-cc-pvtz"
HC, MARGIN = 0.25, 5.0                # coarse grid step and margin round the nuclei (bohr); the grid points are whole multiples of the step, so the grid is as symmetric as the molecule
R1, R0 = 0.1, 0.6                     # partition of unity round each non-H nucleus (bohr)
HF = {6: 0.05, 7: 0.05, 8: 0.05, 17: 0.03}   # fine step (bohr) by Z: chlorine's sharper cusp needs a finer grid (tested: 0.05 gave 0.9 % too many electrons for HCl, 0.03 gave 0.15 %)
HP = 0.4                              # potential grid step (bohr)
LNMIN, LNMAX = np.log(1e-7), np.log(1e4)
LNSTEP = (LNMAX - LNMIN) / 65534.0
PSCALE = 1e-4                         # potential: au per int16 unit (+-3.2767 au; beyond that clamped, only next to a nucleus)
SPIN = {"O2": 2}                      # 2S: O2 is a triplet (UKS); every other molecule closed shell (RKS)
ZOF = {"H": 1, "C": 6, "N": 7, "O": 8, "Cl": 17}


def read_xyz(path):
    lines = open(path).read().splitlines()
    n = int(lines[0])
    return [(l.split()[0], tuple(float(v) / A0_ANGSTROM for v in l.split()[1:4])) for l in lines[2:2 + n]]


def quant(rho):
    k = np.zeros(rho.shape, dtype=np.uint16)
    ok = rho >= np.exp(LNMIN)
    k[ok] = np.clip(np.rint((np.log(rho[ok]) - LNMIN) / LNSTEP) + 1, 1, 65535).astype(np.uint16)
    return k


def density_at(mol, dm, pts):
    out = np.empty(len(pts))
    for i in range(0, len(pts), 20000):
        ao = numint.eval_ao(mol, pts[i:i + 20000])
        out[i:i + 20000] = numint.eval_rho(mol, ao, dm)
    return out


def esp_at(mol, dm, pts):
    """V(r) = sum_A Z_A / |r - R_A| - sum_mn D_mn (m n | 1/|r - .|), au. Electron part from the 3-centre integrals with point charges."""
    R, Z = mol.atom_coords(), mol.atom_charges()
    v = np.zeros(len(pts))
    for a in range(len(Z)):
        v += Z[a] / np.maximum(np.linalg.norm(pts - R[a], axis=1), 1e-6)
    chunk = max(50, int(3e8 / (8 * mol.nao ** 2)))
    for i in range(0, len(pts), chunk):
        fm = gto.fakemol_for_charges(pts[i:i + chunk])
        ints = df.incore.aux_e2(mol, fm, intor="int3c2e")
        v[i:i + chunk] -= np.einsum("ijp,ij->p", ints, dm)
    return v


def axes(lo, hi, h):
    lo, hi = np.floor(lo / h) * h, np.ceil(hi / h) * h                     # whole multiples of h
    n = np.rint((hi - lo) / h).astype(int) + 1
    return lo, n, np.stack(np.meshgrid(*[lo[i] + h * np.arange(n[i]) for i in range(3)], indexing="ij"), -1).transpose(2, 1, 0, 3).reshape(-1, 3)   # x fastest


def main():
    xyzdir, outdir = sys.argv[1], sys.argv[2]
    ids = sys.argv[3:] or sorted(f[:-4] for f in os.listdir(xyzdir) if f.endswith(".xyz"))
    for mid in ids:
        t0 = time.time()
        atoms = read_xyz(os.path.join(xyzdir, mid + ".xyz"))
        basis = os.environ.get("PL_BASIS_" + mid, BASIS)
        spin = SPIN.get(mid, 0)
        mol = gto.M(atom=[(s, p) for s, p in atoms], unit="B", basis=basis, spin=spin, verbose=0)
        mf = dft.UKS(mol) if spin else dft.RKS(mol)
        mf.xc = "b3lyp"; mf.grids.level = 3; mf.conv_tol = 1e-10; mf.max_cycle = 200
        mf.kernel()
        if not mf.converged: raise SystemExit(mid + ": SCF did not converge")
        dm = mf.make_rdm1()
        if spin: dm = dm[0] + dm[1]                                   # the TOTAL density (alpha + beta)
        dip_au = mf.dip_moment(unit="AU", verbose=0)
        if spin: dip_au = np.asarray(dip_au)
        t_scf = time.time() - t0
        R, Z = mol.atom_coords(), mol.atom_charges()
        lo, hi = R.min(0) - MARGIN, R.max(0) + MARGIN
        loG, n, g = axes(lo, hi, HC)
        code = quant(density_at(mol, dm, g))
        fine = []
        for a in range(len(Z)):
            if Z[a] == 1: continue
            hf = HF[int(Z[a])]
            m = int(np.rint(R0 / hf))
            a1 = hf * np.arange(-m, m + 1)
            cube = np.stack(np.meshgrid(a1, a1, a1, indexing="ij"), -1).transpose(2, 1, 0, 3).reshape(-1, 3)
            fine.append((a, hf, m, quant(density_at(mol, dm, cube + R[a]))))
        loP, nP, gp = axes(lo, hi, HP)
        pot = esp_at(mol, dm, gp)
        pq = np.clip(np.rint(pot / PSCALE), -32767, 32767).astype(np.int16)
        with open(os.path.join(outdir, mid + ".pld"), "wb") as f:
            f.write(b"PLDENS1\0")
            f.write(struct.pack("<i", len(Z)))
            for a in range(len(Z)): f.write(struct.pack("<i3d", int(Z[a]), *R[a]))
            f.write(struct.pack("<4d2i", *dip_au, float(mf.e_tot), int(mol.nelectron), spin))
            f.write(basis.encode().ljust(16, b"\0")[:16]); f.write((b"UKS" if spin else b"RKS").ljust(8, b"\0"))
            f.write(struct.pack("<2d", LNMIN, LNSTEP))
            f.write(struct.pack("<4d3i", *loG, HC, *n)); f.write(code.astype("<u2").tobytes())
            f.write(struct.pack("<2d", R1, R0))
            f.write(struct.pack("<i", len(fine)))
            for a, hf, m, c in fine: f.write(struct.pack("<idi", a, hf, m)); f.write(c.astype("<u2").tobytes())
            f.write(struct.pack("<4d3id", *loP, HP, *nP, PSCALE)); f.write(pq.astype("<i2").tobytes())
        dD = np.asarray(dip_au) * AU_DEBYE
        with open(os.path.join(outdir, mid + ".txt"), "w") as f:
            f.write(f"{mid}: COMPUTED electron density and electrostatic potential (not measured)\n")
            f.write(f"Software: PySCF {PYSCF_VERSION}; functional: PySCF 'b3lyp' (libxc HYB_GGA_XC_B3LYP, id 402); basis: {basis}; DFT grid level 3; {'UKS, triplet (2S = 2), total density alpha + beta' if spin else 'RKS (closed shell)'}\n")
            if mid == "O3": f.write("Ozone: closed-shell (RKS) approximation; ozone has multi-reference character, so its density is less certain than the others'.\n")
            f.write(f"Geometry: data/molecules/xyz/{mid}.xyz (the app's viewer coordinates, CCCBDB experimental), converted to bohr with CODATA 2022 a0 = {A0_ANGSTROM} angstrom\n")
            f.write(f"Total energy: {mf.e_tot:.8f} hartree; electrons: {mol.nelectron}; SCF + dipole time: {t_scf:.0f} s; total time {time.time() - t0:.0f} s\n")
            f.write(f"Dipole (from the density matrix): ({dD[0]:.4f}, {dD[1]:.4f}, {dD[2]:.4f}) D, |mu| = {np.linalg.norm(dD):.4f} D  (1 e bohr = {AU_DEBYE} D)\n")
            f.write(f"Density grid: {n[0]} x {n[1]} x {n[2]} points, step {HC} bohr, margin {MARGIN} bohr; fine cubes: {', '.join(f"{(2*m+1)**3} points step {hf} bohr" for _, hf, m, _ in fine) or 'none'}; partition r1 {R1}, r0 {R0} bohr\n")
            f.write(f"Potential grid: {nP[0]} x {nP[1]} x {nP[2]} points, step {HP} bohr, {PSCALE} au per unit\n")
        print(mid, "done", f"{time.time() - t0:.0f}s scf {t_scf:.0f}s", "dipole D", np.round(dD, 4), flush=True)


if __name__ == "__main__":
    main()
