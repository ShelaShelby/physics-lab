#pragma once
// 0.93.0: hydrogen in a static electric field (the Stark effect). No drawing here, so it can be tested (tests/test_stark.inc, ctest atom_physics).
//
// The model (the app says what it leaves out: limitsText):
//  - Non-relativistic hydrogen (Z = 1), no fine structure, no spin, in a uniform field F along +z: H = H0 + F z (atomic units, the electron's charge -e, so the
//    field term is +F z). The reduced mass mu = m_e m_p / (m_e + m_p) is kept: lengths in a0 m_e / mu, energies in mu / m_e hartree, the field in (mu / m_e)^2
//    of the atomic unit E_h / (e a0) (CODATA 2022, atom_core.h).
//  - Inside one n the field term is diagonalised exactly in the |n l m> basis (m is kept, so one block per m): its eigenvalues are F times the eigenvalues of z
//    there. They are the parabolic states (n1, n2, m), n = n1 + n2 + |m| + 1, with the linear shift E1 = (3/2) n (n1 - n2) F
//    (Anh-Tai et al., arXiv:2304.09480v3 (2024), eq. 20-22; data/fields/anh-tai_et_al_2024_arxiv2304.09480v3.pdf).
//  - The levels the app draws add the second-order term E2 = -(n^4 / 16) [17 n^2 - 3 (n1 - n2)^2 - 9 m^2 + 19] F^2 (the same paper, eq. 29-30). It comes from
//    the mixing between different n. The tests check both terms against a full diagonalisation in a Coulomb-Sturmian basis (many n and l at once:
//    sturmianLevels), and the ground state's series -1/2 - (9/4) F^2 - (3555/64) F^4 - ... and the (3, 0, 1) series against Jentschura,
//    arXiv:physics/0010038v3 (2001), eq. 29 and 30 (data/fields/jentschura_2001_arxiv_physics0010038v3.pdf).
//  - The field is kept below the classical saddle-point field of the highest n shown, F_c = W^2 / 4 with W the binding energy (atomic units; Kishimoto et al.,
//    arXiv:physics/0204038 (2002), the introduction; data/fields/kishimoto_et_al_2002_arxiv_physics0204038.pdf): F_c = 1 / (16 n^4). Above it the electron
//    can leave over the saddle of the potential, and the levels become resonances with a width. The lab draws no width and no ionisation.
//  - A parabolic state's cloud: psi = A u_{n1,m}(xi / n) u_{n2,m}(eta / n) e^{i m phi}, u_{k,m}(x) = x^{m/2} e^{-x/2} L^m_k(x), xi = r + z, eta = r - z,
//    dV = (xi + eta) / 4 d xi d eta d phi (Anh-Tai et al., eq. 3, 4, 7, 11). A is worked out here from the polynomial integrals (normaliseA), not typed in.
#include <complex>
#include <cstdint>
#include <string>
#include <vector>

namespace orbit { struct Mulberry32; }

namespace atom::stark {

// ---- units ----
double fieldUnitVm();                   // the atomic unit of field E_h / (e a0), V/m, from CODATA 2022 E_h (eV) and a0 (atom_core.h)
double reducedMassH();                  // mu / m_e of hydrogen, 1 / (1 + m_e / m_p) (CODATA 2022 m_p / m_e)

// ---- the parabolic states ----
struct Par { int n = 1, n1 = 0, n2 = 0, m = 0; };   // m may be negative; n = n1 + n2 + |m| + 1
bool valid(const Par& s);
std::vector<Par> manifold(int n);                  // every state of n (n^2 of them), by m (from -(n-1)) then n1 - n2 (lowest first)
std::string name(const Par& s);                    // "(n1, n2, m) = (1, 0, 0)"
double linearAu(const Par& s);                     // E1 / F = (3/2) n (n1 - n2)
double secondAu(const Par& s);                     // E2 / F^2 = -(n^4 / 16) [17 n^2 - 3 (n1 - n2)^2 - 9 m^2 + 19]
// the energy to second order, in hartree, of hydrogen (reduced mass mu, in m_e) in the field F (atomic units E_h / (e a0)). mu = 1: an infinitely heavy nucleus
double energyAu(const Par& s, double F, double mu = 1.0);
double saddleFieldAu(int n, double mu = 1.0);      // F_c = W^2 / 4 with W = mu / (2 n^2): mu^2 / (16 n^4)

// ---- the within-n diagonalisation ----
// The field term inside one n, block m: H1 = F z in the basis |n l m>, l = |m| .. n-1. The radial integrals <n l| r |n l'> are worked out by Gauss-Laguerre
// quadrature of the hydrogen radial functions (exact: a polynomial times e^{-2r/n}), the angular part <l m| cos theta |l-1 m> = sqrt((l^2 - m^2) / ((2l+1)(2l-1))).
struct Block {
    int n = 1, m = 0;
    std::vector<double> z;                         // the eigenvalues of z (a0), lowest first: E1 = F z. Each is (3/2) n k for one k = n1 - n2
    std::vector<int> k;                            // the k = n1 - n2 each eigenvalue is nearest to (rounded)
    std::vector<std::vector<double>> vec;          // vec[i][l - |m|]: the i-th eigenvector's amplitudes on |n l m>
};
Block withinN(int n, int m);
double radialR(int n, int l, int n2, int l2);      // <n l| r |n2 l2> = int R_nl R_n2l2 r^3 dr (a0), by Gauss-Laguerre quadrature
double hydrogenR(int n, int l, double r);          // the normalised radial function R_nl(r) (Z = 1, infinite mass, a0)

// ---- the full diagonalisation (many n and l at once) ----
// H0 + F z for one m in the Coulomb-Sturmian basis chi_{k,l}(r) = r^l e^{-r/nk} L^{2l+1}_k(2 r / nk), k = 0 .. K, l = |m| .. lmax (with nk = n the
// manifold n is in the basis exactly at F = 0). Generalised symmetric problem H c = E S c by Cholesky and Jacobi rotations. Returns all eigenvalues, lowest first.
// Used by the tests (and the Data table) as the independent check of the perturbation formulas; the app's levels use energyAu.
std::vector<double> sturmianLevels(int nk, int m, double F, int lmax, int K);
// the eigenvalues of sturmianLevels nearest to each state of n with this m, in the order of the states' k = n1 - n2 (lowest first)
std::vector<double> sturmianManifold(int n, int m, double F, int lmax, int K);

// ---- the parabolic state's cloud ----
// The density |psi|^2 dV in x = xi / n, y = eta / n is proportional to (x + y) f1(x) f2(y), f_j(x) = x^|m| e^{-x} L^|m|_{n_j}(x)^2. It is a mixture of two
// products: x f1(x) f2(y) (weight w1) and f1(x) y f2(y) (weight w2). A point takes 4 random numbers: the part (u0 < w1 / (w1 + w2)), x and y by inverting
// their tails, e^{-x} Q(x) / Q(0) = 1 - u (Q = p + p' + p'' + ..., p the density's polynomial), and phi = 2 pi u3.
struct ParSampler {
    Par s;
    double w1 = 0, w2 = 0;                         // the two parts' weights (their integrals)
    std::vector<double> pA1, pA2, pB1, pB2;        // part 1: x density pA1 = x f1 poly, y density pA2 = f2 poly; part 2: pB1 = f1 poly, pB2 = y f2 poly
    std::vector<double> qA1, qA2, qB1, qB2;        // their tail polynomials Q (Q(0) = the integral)
    double A = 0;                                  // the normalisation of psi (in the lab's lengths, a0 / mu)
    double share() const { return w1 / (w1 + w2); }
};
ParSampler parSampler(const Par& s);
// one point (always kept), in units of a0 / mu (the caller scales); four numbers from r
void samplePar(const ParSampler& ps, orbit::Mulberry32& r, double& x, double& y, double& z);
// the inverse of a tail: the x with e^{-x} Q(x) = T Q(0), 0 < T <= 1, by bisection on [0, xmax] (60 halvings; xmax from the tail's own size)
double invertTail(const std::vector<double>& q, double T);
std::complex<double> psiPar(const ParSampler& ps, double x, double y, double z);   // the normalised wave function (lengths a0 / mu)
double meanZ(const ParSampler& ps);                // <z> from the polynomial integrals (exact; a0 / mu)
double meanR(const ParSampler& ps);                // <r> from the same integrals (exact; a0 / mu)
double normalisation(const ParSampler& ps);        // int |psi|^2 dV from the same integrals (1 when A is right)

// ---- the lines ----
// The components of the line n_u -> n_l: every pair of parabolic states with dm = 0 (pi) or +-1 (sigma), strength |<u| r_q |l>|^2 (a0^2) from the within-n
// eigenvectors and the radial integrals (r_q = r C^1_q, Edmonds' spherical components). The sum over components is the field-free line's sum of
// |<n_u l m| r |n_l l' m'>|^2 over all its states (the eigenvectors are orthonormal).
struct Component { Par up, lo; int q = 0; double shiftAu = 0, strength = 0; };   // shift: of the transition energy from the field-free line, hartree
std::vector<Component> lineComponents(int nu, int nl, double F, double mu = 1.0);
double fieldFreeStrength(int nu, int nl);          // the field-free sum of |<n_u l m| r_q |n_l l' m'>|^2 over q and all states

std::string limitsText();                          // the limits in plain words
}  // namespace atom::stark
