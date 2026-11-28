// 0.93.0: hydrogen's Stark effect (atom/stark.h). Written before the code. The expected values are independent of the module:
//  - the linear shift (3/2) n (n1 - n2) F and the second-order term -(n^4/16)[17 n^2 - 3 (n1 - n2)^2 - 9 m^2 + 19] F^2 as printed in Anh-Tai et al.,
//    arXiv:2304.09480v3, eq. 22 and 29 (data/fields, private repo; extract data/fields/anh-tai_et_al_2024.extract.txt), typed here from the paper, not from the module;
//  - the ground state's series -1/2 - (9/4) F^2 - (3555/64) F^4 - (2512779/512) F^6 and the (3, 0, 1) series -1/50 + (45/2) F - (31875/2) F^2
//    + (54140625/4) F^3 - (715751953125/16) F^4 as printed in Jentschura, arXiv:physics/0010038v3, eq. 29 and 30;
//  - F_c = W^2 / 4 (Kishimoto et al., arXiv:physics/0204038, the introduction);
//  - orthonormality, a sum rule (a unitary change of basis inside each n keeps the line's total strength), and the series' own scaling (the remainder after
//    the F^2 term falls as F^3, as F^4 where n1 = n2, since E(F) of (n1, n2, m) is E(-F) of (n2, n1, m)).
#include "atom/stark.h"
#include "orbit/belts.h"

static void testStarkFormulas() {
    namespace st = atom::stark;
    // the printed coefficients, typed from the papers
    const auto lin = [](int n, int n1, int n2) { return 1.5 * n * (n1 - n2); };
    const auto sec = [](int n, int n1, int n2, int m) { const double k = n1 - n2; return -std::pow(double(n), 4) / 16 * (17.0 * n * n - 3 * k * k - 9.0 * m * m + 19); };
    bool okL = true, okS = true, okN = true;
    for (int n = 1; n <= 6; n++) {
        const std::vector<st::Par> v = st::manifold(n);
        okN = okN && int(v.size()) == n * n;
        for (const st::Par& s : v) {
            okN = okN && st::valid(s) && s.n1 + s.n2 + std::abs(s.m) + 1 == n;
            okL = okL && st::linearAu(s) == lin(n, s.n1, s.n2);
            okS = okS && std::fabs(st::secondAu(s) - sec(n, s.n1, s.n2, s.m)) <= 1e-12 * std::fabs(sec(n, s.n1, s.n2, s.m));
        }
    }
    checkTrue("stark: every n has n^2 parabolic states with n = n1 + n2 + |m| + 1 (n = 1..6)", okN);
    checkTrue("stark: the linear shift is (3/2) n (n1 - n2) (Anh-Tai et al. eq. 22)", okL);
    checkTrue("stark: the second-order term is -(n^4/16)[17n^2 - 3(n1-n2)^2 - 9m^2 + 19] (Anh-Tai et al. eq. 29)", okS);
    // the ground state's F^2 coefficient -9/4 (Jentschura eq. 29) is the general formula's at n = 1
    checkNum("stark: 1s second-order coefficient = -9/4 (Jentschura eq. 29)", st::secondAu(st::Par{1, 0, 0, 0}), -9.0 / 4, 1e-15);
    // (3, 0, 1): 45/2 and -31875/2 (Jentschura eq. 30) from the general formulas at n = 5
    checkNum("stark: (3,0,1) linear coefficient = 45/2 (Jentschura eq. 30)", st::linearAu(st::Par{5, 3, 0, 1}), 45.0 / 2, 1e-15);
    checkNum("stark: (3,0,1) second-order coefficient = -31875/2 (Jentschura eq. 30)", st::secondAu(st::Par{5, 3, 0, 1}), -31875.0 / 2, 1e-15);
    // the saddle-point field
    for (int n = 1; n <= 6; n++) {
        const double W = 1.0 / (2.0 * n * n);
        checkNum("stark: F_c = W^2/4 for n = " + std::to_string(n) + " (Kishimoto et al.)", st::saddleFieldAu(n), W * W / 4, 1e-15);
    }
    // units: the atomic unit of field from CODATA E_h and a0: E_h / (e a0) = 5.142 206 751 12(80)e11 V/m is CODATA 2022's own value (Mohr et al., arXiv:2409.03787, Table XXXV,
    // "atomic unit of electric field"; data/calc/codata2022_mohr2024_arxiv2409.03787.pdf); the module derives it from E_h and a0
    checkNum("stark: atomic unit of field = 5.14220675112e11 V/m (CODATA 2022)", st::fieldUnitVm(), 5.14220675112e11, 2e-11);
    checkNum("stark: hydrogen's reduced mass 1/(1 + 1/1836.152673426)", st::reducedMassH(), 1.0 / (1.0 + 1.0 / 1836.152673426), 1e-15);
    // energyAu: E0 + E1 F + E2 F^2, and the reduced-mass scaling E(F; mu) = mu e(F / mu^2)
    const st::Par p{3, 2, 0, 0};
    const double F = 1e-5, mu = 0.99945;
    checkNum("stark: energyAu = -1/(2n^2) + E1 F + E2 F^2", st::energyAu(p, F), -1.0 / 18 + lin(3, 2, 0) * F + sec(3, 2, 0, 0) * F * F, 1e-14);
    checkNum("stark: energyAu with a reduced mass = mu e(F / mu^2)", st::energyAu(p, F, mu), mu * (-1.0 / 18 + lin(3, 2, 0) * F / (mu * mu) + sec(3, 2, 0, 0) * F * F / (mu * mu * mu * mu)), 1e-14);
    checkNum("stark: F_c with a reduced mass = mu^2 / (16 n^4)", st::saddleFieldAu(2, mu), mu * mu / 256, 1e-15);
}

static void testStarkWithinN() {
    namespace st = atom::stark;
    // the hydrogen radial functions are normalised and give <r> = (3 n^2 - l(l+1)) / 2 (any quantum-mechanics text; here only as a check of the quadrature):
    // the normalisation is checked by an independent trapezoid sum over a fine grid
    bool okNorm = true;
    for (int n = 1; n <= 5; n++)
        for (int l = 0; l < n; l++) {
            double s = 0; const double h = 1e-3 * n;
            for (int i = 1; i < int(80.0 * n / h); i++) { const double r = i * h, R = st::hydrogenR(n, l, r); s += R * R * r * r * h; }
            okNorm = okNorm && std::fabs(s - 1) < 1e-6;
        }
    checkTrue("stark: R_nl normalised (trapezoid sum, n <= 5)", okNorm);
    // within-n diagonalisation: eigenvalues (3/2) n k (eq. 22), eigenvectors orthonormal, n = 2, 3, 4 (and 5)
    for (int n = 2; n <= 5; n++) {
        bool okE = true, okO = true, okK = true;
        double worst = 0;
        for (int m = 0; m < n; m++) {
            const st::Block b = st::withinN(n, m);
            const int dim = n - m;
            okK = okK && int(b.z.size()) == dim && int(b.k.size()) == dim && int(b.vec.size()) == dim;
            if (!okK) continue;
            for (int i = 0; i < dim; i++) {
                const int kWant = -(dim - 1) + 2 * i;                     // k = n1 - n2 runs over -(n-|m|-1) .. n-|m|-1 in steps of 2
                okK = okK && b.k[std::size_t(i)] == kWant;
                const double want = 1.5 * n * kWant;
                worst = std::max(worst, std::fabs(b.z[std::size_t(i)] - want));
                okE = okE && std::fabs(b.z[std::size_t(i)] - want) <= 1e-11 * (1 + std::fabs(want));
                for (int j = 0; j < dim; j++) {
                    double d = 0;
                    for (int l = 0; l < dim; l++) d += b.vec[std::size_t(i)][std::size_t(l)] * b.vec[std::size_t(j)][std::size_t(l)];
                    okO = okO && std::fabs(d - (i == j ? 1 : 0)) <= 1e-12;
                }
            }
        }
        checkTrue("stark: n = " + std::to_string(n) + " within-n eigenvalues of z are (3/2) n (n1 - n2) (Anh-Tai et al. eq. 22; worst " + std::to_string(worst) + " a0)", okE);
        checkTrue("stark: n = " + std::to_string(n) + " each eigenvalue is labelled with its k = n1 - n2", okK);
        checkTrue("stark: n = " + std::to_string(n) + " within-n eigenvectors orthonormal", okO);
    }
}

static void testStarkSturmian() {
    namespace st = atom::stark;
    // n = 2, 3, 4: the full diagonalisation (many n and l) against the linear formula. The difference must be the second-order term: within 2 % of
    // E2 F^2 at F = 0.02 F_c (the third-order term is at most a few per cent of it there: the test below checks it falls as F^3), and the remainder after
    // E2 F^2 must fall as F^3 (k != 0) or F^4 (k = 0): the ratio R(F) / R(F/2) is 8 or 16 to within 7 %.
    for (int n = 2; n <= 4; n++) {
        bool okLin = true, okSec = true, okScale = true;
        double worstSec = 0;
        for (int m = 0; m < n; m++) {
            const double F = 0.02 * st::saddleFieldAu(n);
            const std::vector<double> e1 = st::sturmianManifold(n, m, F, n + 4, n + 12), e2 = st::sturmianManifold(n, m, F / 2, n + 4, n + 12);
            const int dim = n - m;
            if (int(e1.size()) != dim || int(e2.size()) != dim) { okLin = okSec = okScale = false; continue; }
            for (int i = 0; i < dim; i++) {
                const int k = -(dim - 1) + 2 * i, n1 = (n - m - 1 + k) / 2, n2 = (n - m - 1 - k) / 2;
                const st::Par s{n, n1, n2, m};
                const double e0 = -1.0 / (2.0 * n * n), E1 = 1.5 * n * k, E2 = -std::pow(double(n), 4) / 16 * (17.0 * n * n - 3.0 * k * k - 9.0 * m * m + 19);
                const double d1 = e1[std::size_t(i)] - e0 - E1 * F;                          // the part beyond the linear shift
                okLin = okLin && std::fabs(d1) <= 1.05 * std::fabs(E2) * F * F;             // the linear formula holds to the size of the second-order term
                okSec = okSec && std::fabs(d1 / (E2 * F * F) - 1) <= 0.02;
                worstSec = std::max(worstSec, std::fabs(d1 / (E2 * F * F) - 1));
                const double R1 = d1 - E2 * F * F, R2 = e2[std::size_t(i)] - e0 - E1 * F / 2 - E2 * F * F / 4;
                const double want = k == 0 ? 16 : 8, ratio = R1 / R2;
                okScale = okScale && std::fabs(ratio / want - 1) <= 0.07;
                (void)s;
            }
        }
        checkTrue("stark: n = " + std::to_string(n) + " full diagonalisation minus the linear shift is within the second-order term (F = 0.02 F_c)", okLin);
        checkTrue("stark: n = " + std::to_string(n) + " full diagonalisation agrees with E0 + E1 F + E2 F^2 to 2 % of E2 F^2 (worst " + std::to_string(worstSec) + ")", okSec);
        checkTrue("stark: n = " + std::to_string(n) + " the remainder after E2 F^2 falls as F^3 (F^4 for n1 = n2)", okScale);
    }
    // the ground state (Jentschura eq. 29): the second-order formula holds within the F^4 term, and the series to F^6 leaves the F^8 term: the remainder
    // r6 = E - (-1/2 - (9/4) F^2 - (3555/64) F^4 - (2512779/512) F^6) is -(13012777803/16384) F^8 to 10 %. The 10 % is the next term's share: the printed
    // coefficients grow by 24.7, 88 and 162 from one even order to the next, so c10 F^2 / c8 is a few per cent at F <= 0.015. Below F = 0.01 the F^8 term
    // (3e-13 hartree at F = 0.005) is near the diagonalisation's own rounding (about 1e-13: its answers move that much with the basis size).
    for (double F : {0.01, 0.015}) {
        const std::vector<double> e = st::sturmianManifold(1, 0, F, 10, 24);
        const double E = e.empty() ? 0 : e[0];
        const double c4 = 3555.0 / 64, c6 = 2512779.0 / 512, c8 = 13012777803.0 / 16384;
        const double r2 = E - (-0.5 - 2.25 * F * F), r6 = r2 + c4 * std::pow(F, 4) + c6 * std::pow(F, 6);
        checkTrue("stark: 1s at F = " + std::to_string(F) + " a.u.: E + 1/2 + (9/4) F^2 is within the F^4 term (Jentschura eq. 29)", !e.empty() && std::fabs(r2) <= 1.05 * c4 * std::pow(F, 4));
        checkTrue("stark: 1s at F = " + std::to_string(F) + " a.u.: the series to F^6 leaves the F^8 term to 10 % (Jentschura eq. 29)",
                  !e.empty() && std::fabs(r6 / (-c8 * std::pow(F, 8)) - 1) <= 0.1);
    }
    // the state (3, 0, 1) (Jentschura eq. 30): to third order within the F^4 term, at F = 1e-5 (a tenth of F_c of n = 5)
    {
        const double F = 1e-5;
        const std::vector<double> e = st::sturmianManifold(5, 1, F, 10, 20);
        const double E = e.size() == 4 ? e[3] : 0;             // k = n1 - n2 = 3: the highest of the m = 1 block
        const double s3 = -1.0 / 50 + 22.5 * F - 31875.0 / 2 * F * F + 54140625.0 / 4 * F * F * F, c4 = 715751953125.0 / 16;
        checkTrue("stark: (3,0,1) at F = 1e-5 a.u.: the series to F^3 holds within the F^4 term (Jentschura eq. 30)", e.size() == 4 && std::fabs(E - s3) <= 1.1 * c4 * std::pow(F, 4));
    }
}

static void testStarkCloud() {
    namespace st = atom::stark;
    bool okN = true, okZ = true;
    for (int n = 1; n <= 5; n++)
        for (const st::Par& s : st::manifold(n)) {
            const st::ParSampler ps = st::parSampler(s);
            okN = okN && std::fabs(st::normalisation(ps) - 1) <= 1e-12;
            okZ = okZ && std::fabs(st::meanZ(ps) - 1.5 * n * (s.n1 - s.n2)) <= 1e-11 * (1 + n * n);
        }
    checkTrue("stark: every parabolic state (n <= 5) normalised by its polynomial integrals", okN);
    checkTrue("stark: <z> from the polynomial integrals = (3/2) n (n1 - n2) (Anh-Tai et al. eq. 20, the energy over F)", okZ);
    // 0.93.0: <r> from the same integrals against the sum over the |n l m> parts of the state (the within-n eigenvectors): r is a scalar, so it does not mix l,
    // and <n l m| r |n l m> = [3 n^2 - l (l + 1)] / 2 (Bethe and Salpeter, Quantum Mechanics of One- and Two-Electron Atoms (1957), eq. 3.26)
    bool okR = true; int nR = 0;
    for (int n = 1; n <= 5; n++)
        for (const st::Par& s : st::manifold(n)) {
            const st::Block b = st::withinN(n, s.m);
            for (std::size_t i = 0; i < b.k.size(); i++) {
                if (b.k[i] != s.n1 - s.n2) continue;
                double want = 0;
                for (std::size_t j = 0; j < b.vec[i].size(); j++) { const int l = std::abs(s.m) + int(j); want += b.vec[i][j] * b.vec[i][j] * (3.0 * n * n - l * (l + 1)) / 2; }
                okR = okR && std::fabs(st::meanR(st::parSampler(s)) - want) <= 1e-10 * n * n;
                nR++;
            }
        }
    checkTrue("stark: <r> from the polynomial integrals = the sum over the state's |n l m> parts of [3 n^2 - l (l + 1)] / 2 (every state of n <= 5: " + std::to_string(nR) + ")", okR && nR == 55);
    // the wave function's normalisation by an independent sum over a grid in (xi, eta) (dV = (xi + eta) / 4 d xi d eta d phi; |psi| does not depend on phi)
    for (const st::Par& s : {st::Par{2, 1, 0, 0}, st::Par{3, 0, 1, 1}, st::Par{4, 1, 1, -1}, st::Par{3, 0, 0, 2}}) {
        const st::ParSampler ps = st::parSampler(s);
        const double h = 0.02 * s.n, top = 60.0 * s.n;
        double sum = 0;
        for (double xi = h / 2; xi < top; xi += h)
            for (double eta = h / 2; eta < top; eta += h) {
                const double z = (xi - eta) / 2, rho = std::sqrt(xi * eta);
                sum += std::norm(st::psiPar(ps, rho, 0, z)) * (xi + eta) / 4 * h * h * 2 * 3.14159265358979323846;
            }
        checkNum("stark: " + st::name(s) + " int |psi|^2 dV = 1 (a sum over a grid in xi, eta)", sum, 1.0, 2e-4);
    }
    // the sampler: 200,000 points of (2, 0, 0) and (3, 0, 1)... their mean z within 4 standard errors of (3/2) n (n1 - n2), and their mean of
    // xi = r + z within 4 standard errors of the exact <xi> from the same density (the sum over the grid above)
    for (const st::Par& s : {st::Par{2, 1, 0, 0}, st::Par{3, 0, 2, 0}, st::Par{4, 2, 0, 1}, st::Par{5, 1, 1, 2}}) {
        const st::ParSampler ps = st::parSampler(s);
        orbit::Mulberry32 r(0x2468ACE1u);
        const int N = 200000;
        double sz = 0, szz = 0, sr = 0;
        for (int i = 0; i < N; i++) {
            double x, y, z;
            st::samplePar(ps, r, x, y, z);
            sz += z; szz += z * z; sr += std::sqrt(x * x + y * y + z * z);
        }
        const double mz = sz / N, sd = std::sqrt(std::max(0.0, szz / N - mz * mz) / N), want = 1.5 * s.n * (s.n1 - s.n2);
        checkTrue("stark: " + st::name(s) + " sampled <z> = " + std::to_string(mz) + " within 4 standard errors of (3/2) n (n1 - n2) = " + std::to_string(want),
                  std::fabs(mz - want) <= 4 * sd);
        // <r> = (3 n^2 - l(l+1))/2 is not defined for a parabolic state (a mixture of l): <r> = <(xi + eta) / 2> is checked against the grid instead
        const double h = 0.02 * s.n, top = 60.0 * s.n;
        double rr = 0;
        for (double xi = h / 2; xi < top; xi += h)
            for (double eta = h / 2; eta < top; eta += h) rr += (xi + eta) / 2 * std::norm(st::psiPar(ps, std::sqrt(xi * eta), 0, (xi - eta) / 2)) * (xi + eta) / 4 * h * h * 2 * 3.14159265358979323846;
        checkNum("stark: " + st::name(s) + " sampled <r> against the grid's", sr / N, rr, 0.01);
    }
    // the tail's inverse: e^{-x} Q(x) = T Q(0) at the answer (relative 1e-12), for a few T
    {
        const st::ParSampler ps = st::parSampler(st::Par{4, 2, 0, 1});
        bool ok = true;
        for (double T : {1.0, 0.9, 0.5, 1e-3, 1e-9}) {
            const double x = st::invertTail(ps.qA1, T);
            double Q = 0, Q0 = ps.qA1.empty() ? 0 : ps.qA1[0];
            for (std::size_t i = ps.qA1.size(); i-- > 0;) Q = Q * x + ps.qA1[i];
            ok = ok && std::fabs(std::exp(-x) * Q / Q0 - T) <= 1e-12 * (1 + T) + 1e-15;
        }
        checkTrue("stark: the tail's inverse solves e^{-x} Q(x) / Q(0) = T", ok);
    }
}

static void testStarkLines() {
    namespace st = atom::stark;
    // the sum rule: a line's components (all q, all pairs) add up to the field-free total, at any field
    for (int nu = 2; nu <= 5; nu++)
        for (int nl = 1; nl < nu; nl++) {
            double s = 0;
            for (const st::Component& c : st::lineComponents(nu, nl, 0.3 * st::saddleFieldAu(nu))) s += c.strength;
            checkNum("stark: the line " + std::to_string(nu) + " -> " + std::to_string(nl) + " keeps its total strength in the field (sum rule)", s, st::fieldFreeStrength(nu, nl), 1e-11);
        }
    // Lyman alpha: the pi components come from (1,0,0) and (0,1,0) of n = 2 to 1s; their shifts are +-3 F (eq. 22) plus the second-order difference; the sigma
    // components from (0,0,+-1), shifted only by the second-order difference
    {
        const double F = 1e-4;
        const std::vector<st::Component> c = st::lineComponents(2, 1, F);
        int pi = 0, sg = 0;
        bool okShift = true;
        double sPi = 0, sSg = 0;
        for (const st::Component& x : c) {
            if (!(x.strength > 1e-12)) continue;
            const double want = (1.5 * 2 * (x.up.n1 - x.up.n2)) * F + (-16.0 / 16 * (17 * 4 - 3.0 * (x.up.n1 - x.up.n2) * (x.up.n1 - x.up.n2) - 9.0 * x.up.m * x.up.m + 19) + 9.0 / 4) * F * F;
            okShift = okShift && std::fabs(x.shiftAu - want) <= 1e-15;
            if (x.q == 0) { pi++; sPi += x.strength; } else { sg++; sSg += x.strength; }
        }
        checkTrue("stark: Lyman alpha has 2 pi and 2 sigma components", pi == 2 && sg == 2);
        checkTrue("stark: Lyman alpha's components are shifted by the difference of eq. 22 + eq. 29 of the two states (1s: -9/4 F^2)", okShift);
        checkNum("stark: Lyman alpha: pi and sigma carry the same total strength (an isotropic field-free line: 1/3 each of x, y, z)", sPi, sSg / 2, 1e-12);
    }
}

// 0.97.0 (outside review of 28 Nov 2026, second, part 4), written before the code: the slider's first non-zero step (shell_atom.cpp: top / 1000, top the
// saddle field of the highest n shown) against Dirac's fine-structure splitting. The splitting from the Dirac formula, E = m c^2 [1 + (Z alpha / (n - k +
// sqrt(k^2 - (Z alpha)^2)))^2]^(-1/2), k = j + 1/2, typed here with NIST's alpha (CODATA 2022), not from the module. For each n = 2 .. 5 shown as the highest,
// the largest linear shift (3/2) n (n - 1) F there (the lab's linearAu) is above the splitting of that n. The field where they are equal goes into the limits text.
static void testStarkFineStructure() {
    namespace st = atom::stark;
    const double a = nist::alpha;
    const auto dirac = [a](int n, double j) { const double k = j + 0.5; return (1 / std::sqrt(1 + std::pow(a / (n - k + std::sqrt(k * k - a * a)), 2)) - 1) / (a * a); };
    for (int n = 2; n <= 5; n++) {
        const double split = dirac(n, n - 0.5) - dirac(n, 0.5);
        const double F1 = st::saddleFieldAu(n, st::reducedMassH()) / 1000;
        st::Par top; top.n = n; top.n1 = n - 1; top.n2 = 0; top.m = 0;
        const double shift = st::linearAu(top) * F1;
        std::printf("  Stark n = %d: first step %.4g au, largest linear shift %.4g Eh, Dirac splitting %.4g Eh, equal at %.4g kV/cm\n", n, F1, shift, split, split / (1.5 * n * (n - 1)) * st::fieldUnitVm() / 1e5);
        checkTrue("Stark n = " + std::to_string(n) + ": the first step's largest linear shift is above the fine-structure splitting", shift > split);
        checkTrue("Stark n = " + std::to_string(n) + ": the module's Dirac splitting and crossing field (1e-12)", std::fabs(st::diracFineSplitAu(n) / split - 1) < 1e-12 &&
                  std::fabs(st::fineCrossFieldAu(n) * 1.5 * n * (n - 1) / split - 1) < 1e-12);
    }
    // with n = 5 the highest shown, the n = 2 levels at its first step are below their crossing (the limits text says from which field)
    checkTrue("Stark: the n = 5 slider's first step is below the n = 2 crossing", st::saddleFieldAu(5, st::reducedMassH()) / 1000 < st::fineCrossFieldAu(2));
    checkTrue("Stark: the limits text has the fine-structure sentence", st::limitsText().find("the levels shift with the square of the field") != std::string::npos);
}

static void testStark() {
    testStarkFormulas();
    testStarkWithinN();
    testStarkSturmian();
    testStarkCloud();
    testStarkLines();
    testStarkFineStructure();
}
