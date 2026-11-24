// 0.78.0: the photon's direction, the electric-dipole pattern (atom/photon_pattern.h; Jackson, Classical Electrodynamics, 3rd ed., sect. 9.2).
// References: the textbook closed forms (Delta m = 0: 3/(8 pi) sin^2 theta; Delta m = +-1: 3/(16 pi) (1 + cos^2 theta)), their integrals over the sphere, and a
// chi-squared goodness of fit of drawn directions (the p-value from the regularised incomplete gamma function Q(k/2, chi^2/2), Numerical Recipes 3rd ed. sect. 6.2).
// (Before 0.78.0 every photon left at an angle drawn evenly in the picture's plane: no pattern at all.)
#include <random>
#include "atom/photon_pattern.h"

static double gammaQ078(double a, double x) {   // the regularised upper incomplete gamma function Q(a, x): series below a + 1, continued fraction above
    const double gln = std::lgamma(a);
    if (x < a + 1) {
        double ap = a, sum = 1 / a, del = sum;
        for (int n = 0; n < 1000; n++) { ap += 1; del *= x / ap; sum += del; if (std::fabs(del) < std::fabs(sum) * 1e-15) break; }
        return 1 - sum * std::exp(-x + a * std::log(x) - gln);
    }
    double b = x + 1 - a, c = 1 / 1e-300, d = 1 / b, h = d;
    for (int i = 1; i < 1000; i++) {
        const double an = -i * (i - a);
        b += 2; d = an * d + b; if (std::fabs(d) < 1e-300) d = 1e-300; c = b + an / c; if (std::fabs(c) < 1e-300) c = 1e-300;
        d = 1 / d; const double del = d * c; h *= del; if (std::fabs(del - 1) < 1e-15) break;
    }
    return std::exp(-x + a * std::log(x) - gln) * h;
}

static void testPhotonPattern078() {
    using cd = std::complex<double>;
    const double PI = 3.14159265358979323846, r2 = 1 / std::sqrt(2.0);
    const cd uz[3] = {0, 0, 1}, up[3] = {r2, cd(0, r2), 0}, um[3] = {r2, cd(0, -r2), 0}, ux[3] = {1, 0, 0};
    // 1. the closed forms and their integrals over the sphere (midpoint rule in cos theta and phi, 1000 x 2000 cells)
    {
        double worst = 0;
        for (double th : {0.0, 0.3, 1.0, PI / 2, 2.5, PI}) for (double ph : {0.0, 0.7, 3.0}) {
            const double n[3] = {std::sin(th) * std::cos(ph), std::sin(th) * std::sin(ph), std::cos(th)};
            worst = std::max(worst, std::fabs(dipolePattern(uz, n) - 3 / (8 * PI) * std::sin(th) * std::sin(th)));
            worst = std::max(worst, std::fabs(dipolePattern(up, n) - 3 / (16 * PI) * (1 + std::cos(th) * std::cos(th))));
            worst = std::max(worst, std::fabs(dipolePattern(um, n) - 3 / (16 * PI) * (1 + std::cos(th) * std::cos(th))));
        }
        checkNum("photon pattern: Delta m = 0 is 3/(8 pi) sin^2 theta and Delta m = +-1 is 3/(16 pi) (1 + cos^2 theta) (largest difference)", worst, 0, 0, 1e-15);
        const cd* us[4] = {uz, up, um, ux};
        const char* nm[4] = {"Delta m = 0", "Delta m = +1", "Delta m = -1", "a real p x orbital's"};
        for (int k = 0; k < 4; k++) {
            const int NU = 1000, NP = 2000;
            double I = 0;
            for (int i = 0; i < NU; i++) for (int j = 0; j < NP; j++) {
                const double z = -1 + (i + 0.5) * 2.0 / NU, s = std::sqrt(1 - z * z), ph = (j + 0.5) * 2 * PI / NP;
                const double n[3] = {s * std::cos(ph), s * std::sin(ph), z};
                I += dipolePattern(us[k], n) * (2.0 / NU) * (2 * PI / NP);
            }
            std::printf("photon pattern: %s pattern over the sphere: %.9f\n", nm[k], I);
            checkNum(std::string("photon pattern: the ") + nm[k] + " pattern integrates to 1 over the sphere", I, 1, 0, 1e-6);
        }
    }
    // 2. the lab's own transitions: 2p z -> 1s is Delta m = 0 (u along z); 2p (m = +1) -> 1s is Delta m = -1 (circular, about z); 2p x -> 1s along x; 2s -> 1s none
    {
        cd u[3];
        const int s1 = orbIndex("1s"), pz = orbIndex("2p|z"), px = orbIndex("2p|x"), s2 = orbIndex("2s"), p1 = complexState(pz, 1);
        const bool a = dipoleUnit(pz, s1, u);
        checkNum("photon pattern: 2p z -> 1s: the dipole is along z", a ? std::abs(u[2]) : 0, 1, 0, 1e-9);
        const bool b = dipoleUnit(p1, s1, u);
        checkNum("photon pattern: 2p (m = +1) -> 1s: the dipole is circular in the x-y plane (|u_x| = |u_y| = 1/sqrt 2, u_z = 0, a quarter turn apart)",
                 b ? std::fabs(std::abs(u[0]) - r2) + std::fabs(std::abs(u[1]) - r2) + std::abs(u[2]) + std::fabs(std::fabs(std::arg(u[1] / u[0])) - PI / 2) : 1, 0, 0, 1e-6);
        const bool c = dipoleUnit(px, s1, u);
        checkNum("photon pattern: 2p x -> 1s: the dipole is along x", c ? std::abs(u[0]) : 0, 1, 0, 1e-9);
        checkStr("photon pattern: 2s -> 1s has no dipole line", dipoleUnit(s2, s1, u) ? "a line" : "none", "none");
    }
    // 3. drawn directions fit the pattern: 400,000 each, 20 bins in cos theta about the axis, chi-squared with 19 degrees of freedom, p-value above 0.001
    std::mt19937_64 gen(78);
    const auto rnd = [&] { return double(gen() >> 11) * (1.0 / 9007199254740992.0); };
    const auto fit = [&](const cd* u, int axis, const std::function<double(double)>& cdf, const std::string& name) {
        const int N = 400000, B = 20;
        std::vector<double> cnt(B, 0);
        for (int i = 0; i < N; i++) {
            double n[3];
            drawDipoleDirection(u, rnd, n);
            const int b = std::min(B - 1, int((n[axis] + 1) / 2 * B));
            cnt[std::size_t(b)] += 1;
        }
        double chi2 = 0;
        for (int b = 0; b < B; b++) {
            const double e = N * (cdf(-1 + 2.0 * (b + 1) / B) - cdf(-1 + 2.0 * b / B));
            chi2 += (cnt[std::size_t(b)] - e) * (cnt[std::size_t(b)] - e) / e;
        }
        const double p = gammaQ078((B - 1) / 2.0, chi2 / 2);
        std::printf("photon pattern: %s: 400,000 drawn directions, chi-squared %.2f with 19 degrees of freedom, p-value %.3f\n", name.c_str(), chi2, p);
        checkNum("photon pattern: " + name + " drawn directions fit the pattern (p-value above 0.001)", p > 0.001 ? 1 : 0, 1, 0, 0);
    };
    fit(uz, 2, [](double x) { return 0.5 + 0.75 * (x - x * x * x / 3); }, "Delta m = 0 (about z)");
    fit(up, 2, [](double x) { return 0.5 + 0.375 * (x + x * x * x / 3); }, "Delta m = +1 (about z)");
    fit(ux, 0, [](double x) { return 0.5 + 0.75 * (x - x * x * x / 3); }, "2p x -> 1s (about x)");
    // 4. no Delta m = 0 photon along z: within 3 degrees of +-z the exact share is 2 x 3/4 [(1 - c) - (1 - c^3) / 3], c = cos 3 degrees
    {
        const double c = std::cos(3 * PI / 180), share = 1.5 * ((1 - c) - (1 - c * c * c) / 3);
        const long N = 2000000;
        long in = 0;
        for (long i = 0; i < N; i++) { double n[3]; drawDipoleDirection(uz, rnd, n); in += std::fabs(n[2]) > c; }
        const double mu = share * double(N);
        double pAtLeast = 1, term = std::exp(-mu);                      // P(X >= in) for a Poisson count of mean mu
        for (long k = 0; k < in; k++) { pAtLeast -= term; term *= mu / double(k + 1); }
        std::printf("photon pattern: Delta m = 0, within 3 degrees of the z axis: %ld of %ld drawn (the exact share %.3e: %.2f expected), P(at least that many) %.3f\n", in, N, share, mu, pAtLeast);
        checkNum("photon pattern: Delta m = 0 photons almost never leave near z (the count within 3 degrees agrees with the exact share: P above 0.001)", pAtLeast > 0.001 && share < 1e-5 ? 1 : 0, 1, 0, 0);
    }
}
