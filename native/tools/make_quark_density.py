#!/usr/bin/env python3
"""0.43: the quark and gluon clouds of the Accurate quark style, computed from measured form factors.

What it computes (method: native/data/qcd/densities/README.txt):
  rho(b) = integral_0^inf Q dQ / (2 pi) J0(Q b) F(Q^2)          (2D Fourier transform; Miller, PRL 99, 112001 (2007), eq. 11)
  - F1 (Dirac form factor) of the proton and neutron: F1 = (GE + tau GM) / (1 + tau), tau = Q^2 / (4 M^2)
      GEp, GMp, GMn: Bradford, Bodek, Budd and Arrington (BBBA05), Nucl. Phys. B Proc. Suppl. 159, 127 (2006), arXiv:hep-ex/0602017, Table 1
      GEn: Atac et al., Nature Commun. 12, 1759 (2021), arXiv:2103.10840, eq. 5 (A = 0.505 GeV^2, B = 1.655, C = 0.909)
    Quark flavours (isospin symmetry, strange quarks neglected; Miller 2007): rho_u = 2 rho_p + rho_n (2 u quarks), rho_d = rho_p + 2 rho_n (1 d quark),
    written here PER QUARK (each normalised to 1). In the neutron the roles swap (its d quarks have the proton's u profile).
    These are NET densities (quarks minus antiquarks of that flavour), in the plane transverse to a very fast proton's motion.
  - Gluons: the gluon gravitational form factor A_g(t) = A_g(0) / (1 - t / m_A^2)^3 (tripole), m_A = 1.641 GeV (lattice QCD,
    Pefkou, Hackett and Shanahan, PRD 105, 054509 (2022), as listed in Duran et al., Nature 615, 813 (2023), Table 1). Its 2D transform,
    normalised to 1, in closed form: rho_g(b) = m^2 (m b)^2 K2(m b) / (16 pi).
  Cross-check (printed): the proton with the independent fit of Venkat et al., PRC 83, 015203 (2011), arXiv:1010.3629, Table II.

Writes native/src/atom/quark_density_data.h (tables used by the app) and native/data/qcd/densities/densities.csv.
Needs numpy and scipy. Run from the repository root:  python3 native/tools/make_quark_density.py
"""
import os
import numpy as np
from scipy.special import j0, kv
from scipy.integrate import quad

HC = 0.1973269804                      # GeV fm (CODATA 2022)
MP, MN = 0.93827208943, 0.93956542194  # GeV (CODATA 2022)
MUP, MUN = 2.79284734463, -1.91304276  # nuclear magnetons (CODATA 2022)
M_A = 1.641                            # GeV, lattice tripole mass of A_g (Duran et al. 2023, Table 1, "Lattice")
DB, NB = 0.01, 401                     # grid: b = 0 .. 4 fm (99.96 % of each cloud lies inside 4 fm)


def rat(a, b, t):                      # BBBA05 eq. 6: sum a_k tau^k / (1 + sum b_k tau^k)
    return sum(ak * t ** k for k, ak in enumerate(a)) / (1 + sum(bk * t ** (k + 1) for k, bk in enumerate(b)))


def GEp(Q2): return rat([1, -0.0578], [11.1, 13.6, 33.0], Q2 / (4 * MP * MP))
def GMp(Q2): return MUP * rat([1, 0.150], [11.1, 19.6, 7.54], Q2 / (4 * MP * MP))
def GMn(Q2): return MUN * rat([1, 1.81], [14.1, 20.7, 68.7], Q2 / (4 * MN * MN))
def GEn(Q2): t = Q2 / (4 * MN * MN); return (1 + Q2 / 0.505) ** -2 * 1.655 * t / (1 + 0.909 * t)
def F1p(Q2): t = Q2 / (4 * MP * MP); return (GEp(Q2) + t * GMp(Q2)) / (1 + t)
def F1n(Q2): t = Q2 / (4 * MN * MN); return (GEn(Q2) + t * GMn(Q2)) / (1 + t)


VP = {2: 9.70703681, 4: 3.7357e-4, 6: -1.43573, 8: 6.0e-8, 10: 1.19052066, 12: 9.9527277, 14: 2.5455841e-1, 16: 12.7977739}
VQ = {2: 14.5187212, 4: 40.88333, 6: 2.90966, 8: 99.999998, 10: -1.11542229, 12: 4.579e-5, 14: 3.866171e-2, 16: 10.3580447}
def venkat(c, t): return (1 + c[6] * t + c[10] * t ** 2 + c[14] * t ** 3) / (1 + c[2] * t + c[4] * t ** 2 + c[8] * t ** 3 + c[12] * t ** 4 + c[16] * t ** 5)
def F1pVenkat(Q2): t = Q2 / (4 * MP * MP); return (venkat(VQ, t) + t * MUP * venkat(VP, t)) / (1 + t)


def rho(F, b):                         # fm^-2; Q in GeV, b in fm
    f = lambda Q: Q * j0(Q * b / HC) * F(Q * Q) / (2 * np.pi)
    edges = np.concatenate([np.linspace(0, 5, 401), np.linspace(5, 60, 221)[1:]])
    return sum(quad(f, a, c, limit=200)[0] for a, c in zip(edges[:-1], edges[1:])) / HC ** 2


def rhoGluon(b):                       # fm^-2, normalised to 1; at b = 0 the limit (m b)^2 K2(m b) -> 2 gives m^2 / (8 pi)
    m = M_A / HC
    return m * m / (8 * np.pi) if b == 0 else m * m * (m * b) ** 2 * kv(2, m * b) / (16 * np.pi)


def cumulative(r, bs):                 # fraction inside b (trapezoid on 2 pi b rho), renormalised to 1 at 4 fm
    w = 2 * np.pi * bs * r
    c = np.concatenate([[0], np.cumsum((w[1:] + w[:-1]) / 2 * DB)])
    return c, c[-1]


def main():
    bs = np.arange(NB) * DB
    P = np.array([rho(F1p, b) for b in bs])
    N = np.array([rho(F1n, b) for b in bs])
    U, D, G = (2 * P + N) / 2, P + 2 * N, np.array([rhoGluon(b) for b in bs])
    assert U.min() > 0 and D.min() > 0, "a net quark density is negative somewhere: it cannot be sampled as dots"
    out = {}
    for name, r in (("u", U), ("d", D), ("g", G)):
        c, tot = cumulative(r, bs)
        m2 = np.sum(((2 * np.pi * bs ** 3 * r)[1:] + (2 * np.pi * bs ** 3 * r)[:-1]) / 2 * DB)
        out[name] = (r, c / tot)
        print(f"{name}: inside 4 fm {tot:.5f}, <b^2> inside 4 fm {m2:.5f} fm^2, centre {r[0]:.4f} fm^-2")
    print(f"proton centre {P[0]:.4f} fm^-2 (Venkat 2011 fit: {rho(F1pVenkat, 0):.4f}); neutron centre {N[0]:.5f} fm^-2")
    print(f"central u / d per quark: {U[0] / D[0]:.3f} (Miller 2007: 'about 70 %' larger, with Kelly's 2004 fit)")

    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
    os.makedirs(os.path.join(root, "data/qcd/densities"), exist_ok=True)
    with open(os.path.join(root, "data/qcd/densities/densities.csv"), "w") as f:
        f.write("b_fm,rho_p_fm-2,rho_n_fm-2,u_per_quark_fm-2,d_per_quark_fm-2,gluon_fm-2\n")
        for i, b in enumerate(bs):
            f.write(f"{b:.2f},{P[i]:.6e},{N[i]:.6e},{U[i]:.6e},{D[i]:.6e},{G[i]:.6e}\n")

    def arr(v): return ",".join(f"{x:.12e}" for x in v)
    with open(os.path.join(root, "src/atom/quark_density_data.h"), "w") as f:
        f.write("// Generated by native/tools/make_quark_density.py (0.43); do not edit. Method and sources: native/data/qcd/densities/README.txt\n")
        f.write("// Transverse densities (fm^-2) of the proton's u and d quarks (PER QUARK, net of antiquarks) and of its gluons' momentum (A_g), on b = 0 .. 4 fm\n")
        f.write("// in steps of 0.01 fm, and the fraction of each cloud inside b (1 at 4 fm). In the neutron the u and d profiles swap.\n#pragma once\n")
        f.write(f"namespace atom {{\nconstexpr int kQdN = {NB};\nconstexpr double kQdStep = {DB};\n")
        for name, label in (("u", "U"), ("d", "D"), ("g", "G")):
            r, c = out[name]
            f.write(f"constexpr double kQdRho{label}[kQdN] = {{{arr(r)}}};\n")
            f.write(f"constexpr double kQdCum{label}[kQdN] = {{{arr(c)}}};\n")
        f.write("}  // namespace atom\n")


if __name__ == "__main__":
    main()
