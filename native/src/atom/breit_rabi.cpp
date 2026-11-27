// 0.93.0: the ground state's hyperfine levels in a magnetic field (breit_rabi.h).
#include "breit_rabi.h"

#include "atom_core.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace atom::breitrabi {

double muBHzPerT() { return kMuBJT / kPlanckJs; }   // CODATA 2022 mu_B (atom_core.h) over the exact h: 1.399 624 491 7e10 Hz/T (CODATA's own mu_B / h)

double gJHydrogen() {
    // CODATA 2022 (Mohr et al., arXiv:2409.03787, data/calc/codata2022_mohr2024_arxiv2409.03787.pdf) section on the bound-electron g-factor: the Dirac value
    // g_D = -(2/3) [1 + 2 sqrt(1 - (Z alpha)^2)] (eq. 114, Breit 1928) and the one-photon radiative term -2 C(Z alpha) (alpha / pi) with
    // C(Z alpha) = (1/2) [1 + (Z alpha)^2 / 6 + ...] (eq. 116, 117). The free electron's whole anomaly |g_e| - 2 (CODATA 2022) is given that binding factor.
    // Left out: the higher terms of eq. 117 (order (Z alpha)^4), the two-photon term's binding factor, the recoil and the nucleus's size.
    const double za2 = kAlpha * kAlpha;
    const double gD = 2.0 / 3.0 * (1 + 2 * std::sqrt(1 - za2));
    return gD + (-kGe - 2) * (1 + za2 / 6);
}

const std::vector<GroundState>& groundStates() {
    // Steck, "Sodium D Line Data", "Rubidium 85 D Line Data", "Rubidium 87 D Line Data", "Cesium D Line Data", revision 2.3.4 (steck.us/alkalidata; data/fields/),
    // Table 6 (g_J of the ground state, g_I) and the ground state's magnetic dipole constant A (the hyperfine table). Hydrogen: the 1s splitting
    // 1420.405 751 766 7(10) MHz from Martin and Wiese, "Atomic Spectroscopy" (NIST; data/fields/martin_wiese_atomic_spectroscopy.pdf, section 4),
    // g_I = -2 mu_p / mu_B with mu_p / mu_B = 1.521 032 202 30e-3 (CODATA 2022), g_J from gJHydrogen().
    static const std::vector<GroundState> v = [] {
        std::vector<GroundState> r;
        r.push_back({1, 0, "hydrogen-1 1s 2S1/2", 1, 1420.4057517667e6, gJHydrogen(), -2 * 1.52103220230e-3,
                     "the splitting from Martin and Wiese (NIST, Atomic Spectroscopy); g_I from CODATA 2022 (the proton's moment); g_J from CODATA 2022's bound-electron formula"});
        r.push_back({11, 12, "sodium-23 3s 2S1/2", 3, 885.81306440e6, 2.00229600, -0.00080461080, "Steck, Sodium D Line Data (rev. 2.3.4): A, g_J and g_I"});
        r.push_back({37, 48, "rubidium-85 5s 2S1/2", 5, 1011.9108130e6, 2.002331070, -0.00029364000, "Steck, Rubidium 85 D Line Data (rev. 2.3.4): A, g_J and g_I"});
        r.push_back({37, 50, "rubidium-87 5s 2S1/2", 3, 3417.341305452145e6, 2.002331070, -0.0009951414, "Steck, Rubidium 87 D Line Data (rev. 2.3.4): A, g_J and g_I"});
        r.push_back({55, 78, "caesium-133 6s 2S1/2", 7, 2298.1579425e6, 2.002540261, -0.00039885395,
                     "Steck, Cesium D Line Data (rev. 2.3.4): A (exact: it defines the second), g_J and g_I"});
        return r;
    }();
    return v;
}

const GroundState* find(int Z, int N) {
    for (const GroundState& g : groundStates()) if (g.Z == Z && g.N == N) return &g;
    return nullptr;
}

double splittingHz(const GroundState& g) { return g.aHz * (g.twoI + 1) / 2.0; }
double xOf(const GroundState& g, double B) { return (g.gJ - g.gI) * muBHzPerT() * B / splittingHz(g); }
double bOfX(const GroundState& g, double x) { return x * splittingHz(g) / ((g.gJ - g.gI) * muBHzPerT()); }

double formulaHz(const GroundState& g, int twoF, int twoM, double B) {
    const double I = g.twoI / 2.0, dE = splittingHz(g), m = twoM / 2.0, muB = muBHzPerT() * B;
    if (std::abs(twoM) == g.twoI + 1) return dE * I / (2 * I + 1) + (twoM > 0 ? 0.5 : -0.5) * (g.gJ + 2 * I * g.gI) * muB;   // Steck eq. 28
    const double x = (g.gJ - g.gI) * muB / dE;                                                                                  // Steck eq. 27
    const double root = std::sqrt(std::max(0.0, 1 + 4 * m * x / (2 * I + 1) + x * x));
    return -dE / (2 * (2 * I + 1)) + g.gI * muB * m + (twoF == g.twoI + 1 ? 0.5 : -0.5) * dE * root;                              // Steck eq. 26
}

std::vector<Sub> formula(const GroundState& g, double B) {
    std::vector<Sub> v;
    for (int twoF : {g.twoI + 1, g.twoI - 1})
        for (int twoM = -twoF; twoM <= twoF; twoM += 2) v.push_back({twoF, twoM, formulaHz(g, twoF, twoM, B)});
    return v;
}

std::vector<Sub> diagonalise(const GroundState& g, double B) {
    // H (Hz) = A [m_I m_J + (I+ J- + I- J+) / 2] + mu_B B (g_J m_J + g_I m_I) in the basis |m_J m_I>. H keeps m_F = m_I + m_J, so it is block diagonal: for
    // each m_F the states |+1/2, m_F - 1/2> and |-1/2, m_F + 1/2> (those that exist). Each 2 x 2 block is diagonalised exactly (its eigenvalues by the
    // symmetric 2 x 2 formula, no Breit-Rabi algebra); the test also diagonalises the whole matrix (fullMatrix) by Jacobi rotations.
    const double I = g.twoI / 2.0, A = g.aHz, muB = muBHzPerT() * B;
    const auto diag = [&](double mJ, double mI) { return A * mI * mJ + muB * (g.gJ * mJ + g.gI * mI); };
    std::vector<Sub> up, lo;
    for (int twoM = -(g.twoI + 1); twoM <= g.twoI + 1; twoM += 2) {
        const double m = twoM / 2.0, mIa = m - 0.5, mIb = m + 0.5;   // a: m_J = +1/2; b: m_J = -1/2
        const bool hasA = std::fabs(mIa) <= I + 1e-9, hasB = std::fabs(mIb) <= I + 1e-9;
        if (hasA && hasB) {
            const double h11 = diag(0.5, mIa), h22 = diag(-0.5, mIb), h12 = A / 2 * std::sqrt(I * (I + 1) - mIa * (mIa + 1));
            const double mean = (h11 + h22) / 2, half = std::hypot((h11 - h22) / 2, h12);
            up.push_back({g.twoI + 1, twoM, mean + half});
            lo.push_back({g.twoI - 1, twoM, mean - half});
        } else {
            up.push_back({g.twoI + 1, twoM, hasA ? diag(0.5, mIa) : diag(-0.5, mIb)});
        }
    }
    up.insert(up.end(), lo.begin(), lo.end());
    return up;
}

std::vector<double> fullMatrix(const GroundState& g, double B) {
    // the whole matrix of H in |m_J m_I> (index: m_J = +1/2 first, then m_I from -I up), every element from the operators, then cyclic Jacobi rotations
    const int nI = g.twoI + 1, n = 2 * nI;
    const double I = g.twoI / 2.0, A = g.aHz, muB = muBHzPerT() * B;
    std::vector<double> h(std::size_t(n * n), 0.0);
    const auto at = [&](int r, int c) -> double& { return h[std::size_t(r * n + c)]; };
    for (int a = 0; a < 2; a++)
        for (int i = 0; i < nI; i++) {
            const double mJ = a == 0 ? 0.5 : -0.5, mI = -I + i;
            at(a * nI + i, a * nI + i) = A * mI * mJ + muB * (g.gJ * mJ + g.gI * mI);
        }
    for (int i = 0; i + 1 < nI; i++) {                       // I+ J-: |+1/2, m_I> -> |-1/2, m_I + 1>, coefficient sqrt(I(I+1) - m_I(m_I+1)) times J-'s 1
        const double mI = -I + i, v = A / 2 * std::sqrt(I * (I + 1) - mI * (mI + 1));
        at(nI + i + 1, i) = v; at(i, nI + i + 1) = v;
    }
    for (int sweep = 0; sweep < 100; sweep++) {
        double off = 0, scale = 0;
        for (int r = 0; r < n; r++) for (int c = 0; c < n; c++) (r == c ? scale : off) += at(r, c) * at(r, c);
        if (!(off > 1e-30 * scale)) break;
        for (int p = 0; p < n; p++)
            for (int q = p + 1; q < n; q++) {
                if (at(p, q) == 0) continue;
                const double th = (at(q, q) - at(p, p)) / (2 * at(p, q));
                const double t = (th >= 0 ? 1 : -1) / (std::fabs(th) + std::sqrt(th * th + 1)), cs = 1 / std::sqrt(t * t + 1), sn = t * cs;
                for (int k = 0; k < n; k++) {                // columns p and q
                    const double kp = at(k, p), kq = at(k, q);
                    at(k, p) = cs * kp - sn * kq; at(k, q) = sn * kp + cs * kq;
                }
                for (int k = 0; k < n; k++) {                // rows p and q
                    const double pk = at(p, k), qk = at(q, k);
                    at(p, k) = cs * pk - sn * qk; at(q, k) = sn * pk + cs * qk;
                }
            }
    }
    std::vector<double> e;
    for (int i = 0; i < n; i++) e.push_back(at(i, i));
    std::sort(e.begin(), e.end());
    return e;
}

double gF(const GroundState& g, int twoF) {
    if (twoF <= 0) return 0;                                  // F = 0: one state, no first-order shift
    const double I = g.twoI / 2.0, J = 0.5, F = twoF / 2.0, ff = F * (F + 1);
    return g.gJ * (ff - I * (I + 1) + J * (J + 1)) / (2 * ff) + g.gI * (ff + I * (I + 1) - J * (J + 1)) / (2 * ff);   // Steck eq. 23
}

double clockShiftHzPerT2(const GroundState& g) {
    const double d = (g.gJ - g.gI) * muBHzPerT();
    return d * d / (2 * splittingHz(g));                                                                                // Steck eq. 29, as a frequency
}

std::string limitsText() {
    return "The ground level alone (J = 1/2), in a uniform field. Its hyperfine constant and g-factors are measured values; the levels are exact for that "
           "Hamiltonian (the Breit-Rabi formula, checked against a full diagonalisation). Left out: the mixing with excited levels (far away), the "
           "diamagnetic shift (second order in the field, not in this model) and the nucleus's quadrupole (zero for J = 1/2).";
}

}  // namespace atom::breitrabi
