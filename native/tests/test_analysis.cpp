// The Data panel's logic (0.51, src/analysis/*): the fitting engine against the NIST Statistical Reference Datasets for nonlinear regression (data/strd, read as
// received: the certified parameters, standard deviations and residual sum of squares come from the files themselves), the ready-made models against
// textbook closed forms, the chi^2 tail against its textbook values, the tables against independent physics (Kepler's third law with the IAU GM of the Sun,
// the Rydberg constant of CODATA 2022, CCCBDB's scale factor), and the export and import of text.
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "analysis/fit.h"
#include "analysis/lab_tables.h"
#include "analysis/table.h"
#include "app/calc.h"
#include "atom/atom_core.h"
#include "atom/zeeman.h"
#include "molecule/morse.h"
#include "molecule/vibration.h"
#include "orbit/kepler.h"
#include "orbit/solar_data.h"
#include "orbit/solar_system.h"
#include "orbit/units.h"

static int g_checks = 0, g_failed = 0;
static void check(const std::string& name, bool ok) { g_checks++; if (!ok) { g_failed++; std::printf("FAIL %s\n", name.c_str()); } }
static bool rel(double a, double b, double tol) { return std::fabs(a - b) <= tol * std::fabs(b); }

using namespace analysis;

// ---- a NIST StRD file: the certified values, the starts and the data ----
struct Strd {
    std::vector<double> start1, start2, cert, certSd;
    double rss = NAN, rsd = NAN;
    int dof = 0;
    std::vector<double> x, y;
};
static bool readStrd(const std::string& name, Strd& s) {
    std::ifstream f(std::string(PL_DATA_DIR) + "/strd/" + name + ".dat");
    if (!f) return false;
    std::string line;
    bool data = false;
    while (std::getline(f, line)) {
        std::istringstream is(line);
        std::string a, eq;
        if (data) {
            double yv, xv;
            if (is >> yv >> xv) { s.y.push_back(yv); s.x.push_back(xv); }
            continue;
        }
        if (line.rfind("Data:", 0) == 0 && line.find(" y ") != std::string::npos) { data = true; continue; }
        if (is >> a >> eq && a.size() >= 2 && a[0] == 'b' && std::isdigit(static_cast<unsigned char>(a[1])) && eq == "=") {
            double v1, v2, c, sd;
            if (is >> v1 >> v2 >> c >> sd) { s.start1.push_back(v1); s.start2.push_back(v2); s.cert.push_back(c); s.certSd.push_back(sd); }
        }
        const auto after = [&](const char* key, double& out) { const std::size_t k = line.find(key); if (k != std::string::npos) out = std::atof(line.c_str() + k + std::string(key).size()); };
        after("Residual Sum of Squares:", s.rss);
        after("Residual Standard Deviation:", s.rsd);
        double d = NAN;
        after("Degrees of Freedom:", d);
        if (std::isfinite(d)) s.dof = static_cast<int>(d);
    }
    return !s.x.empty() && !s.cert.empty();
}

static const std::map<std::string, CurveFn>& strdModels() {
    static const double pi = 3.141592653589793238462643383279;
    static const std::map<std::string, CurveFn> m = {
        {"Misra1a", [](const double* b, double x) { return b[0] * (1 - std::exp(-b[1] * x)); }},
        {"Misra1b", [](const double* b, double x) { return b[0] * (1 - std::pow(1 + b[1] * x / 2, -2)); }},
        {"Misra1c", [](const double* b, double x) { return b[0] * (1 - std::pow(1 + 2 * b[1] * x, -0.5)); }},
        {"Misra1d", [](const double* b, double x) { return b[0] * b[1] * x / (1 + b[1] * x); }},
        {"Chwirut1", [](const double* b, double x) { return std::exp(-b[0] * x) / (b[1] + b[2] * x); }},
        {"Chwirut2", [](const double* b, double x) { return std::exp(-b[0] * x) / (b[1] + b[2] * x); }},
        {"DanWood", [](const double* b, double x) { return b[0] * std::pow(x, b[1]); }},
        {"Lanczos1", [](const double* b, double x) { return b[0] * std::exp(-b[1] * x) + b[2] * std::exp(-b[3] * x) + b[4] * std::exp(-b[5] * x); }},
        {"Lanczos2", [](const double* b, double x) { return b[0] * std::exp(-b[1] * x) + b[2] * std::exp(-b[3] * x) + b[4] * std::exp(-b[5] * x); }},
        {"Lanczos3", [](const double* b, double x) { return b[0] * std::exp(-b[1] * x) + b[2] * std::exp(-b[3] * x) + b[4] * std::exp(-b[5] * x); }},
        {"Gauss1", [](const double* b, double x) { return b[0] * std::exp(-b[1] * x) + b[2] * std::exp(-(x - b[3]) * (x - b[3]) / (b[4] * b[4])) + b[5] * std::exp(-(x - b[6]) * (x - b[6]) / (b[7] * b[7])); }},
        {"Gauss2", [](const double* b, double x) { return b[0] * std::exp(-b[1] * x) + b[2] * std::exp(-(x - b[3]) * (x - b[3]) / (b[4] * b[4])) + b[5] * std::exp(-(x - b[6]) * (x - b[6]) / (b[7] * b[7])); }},
        {"Gauss3", [](const double* b, double x) { return b[0] * std::exp(-b[1] * x) + b[2] * std::exp(-(x - b[3]) * (x - b[3]) / (b[4] * b[4])) + b[5] * std::exp(-(x - b[6]) * (x - b[6]) / (b[7] * b[7])); }},
        {"Kirby2", [](const double* b, double x) { return (b[0] + b[1] * x + b[2] * x * x) / (1 + b[3] * x + b[4] * x * x); }},
        {"Hahn1", [](const double* b, double x) { return (b[0] + b[1] * x + b[2] * x * x + b[3] * x * x * x) / (1 + b[4] * x + b[5] * x * x + b[6] * x * x * x); }},
        {"Thurber", [](const double* b, double x) { return (b[0] + b[1] * x + b[2] * x * x + b[3] * x * x * x) / (1 + b[4] * x + b[5] * x * x + b[6] * x * x * x); }},
        {"MGH09", [](const double* b, double x) { return b[0] * (x * x + x * b[1]) / (x * x + x * b[2] + b[3]); }},
        {"MGH10", [](const double* b, double x) { return b[0] * std::exp(b[1] / (x + b[2])); }},
        {"MGH17", [](const double* b, double x) { return b[0] + b[1] * std::exp(-x * b[3]) + b[2] * std::exp(-x * b[4]); }},
        {"BoxBOD", [](const double* b, double x) { return b[0] * (1 - std::exp(-b[1] * x)); }},
        {"Eckerle4", [](const double* b, double x) { return (b[0] / b[1]) * std::exp(-0.5 * ((x - b[2]) / b[1]) * ((x - b[2]) / b[1])); }},
        {"Rat42", [](const double* b, double x) { return b[0] / (1 + std::exp(b[1] - b[2] * x)); }},
        {"Rat43", [](const double* b, double x) { return b[0] / std::pow(1 + std::exp(b[1] - b[2] * x), 1 / b[3]); }},
        {"Bennett5", [](const double* b, double x) { return b[0] * std::pow(b[1] + x, -1 / b[2]); }},
        {"Roszman1", [](const double* b, double x) { return b[0] - b[1] * x - std::atan(b[2] / (x - b[3])) / pi; }},
        {"ENSO", [](const double* b, double x) { return b[0] + b[1] * std::cos(2 * pi * x / 12) + b[2] * std::sin(2 * pi * x / 12) + b[4] * std::cos(2 * pi * x / b[3]) + b[5] * std::sin(2 * pi * x / b[3]) +
                                                         b[7] * std::cos(2 * pi * x / b[6]) + b[8] * std::sin(2 * pi * x / b[6]); }},
    };
    return m;
}

// The digits a fit gets right: -log10 of the relative error (NIST's "log relative error", LRE), capped at the certified 11
static double lre(double got, double cert) { if (got == cert) return 11; const double e = std::fabs(got - cert) / std::fabs(cert); return std::min(11.0, -std::log10(e)); }

static void testStrd() {
    // Every dataset from both of NIST's starting points. Pass: every parameter, its standard deviation and the residual sum of squares to at least 6 digits
    // (LRE >= 6; NIST's own guidance for a good nonlinear solver is 4 to 6). The weakest result of each is printed. Two exceptions, said here and in REPORT.md:
    //   - Lanczos1: NIST made its data with no noise, to 13 digits; the residuals (RSS 1.4e-25) are then rounding of 64-bit numbers, so only the parameters can be
    //     certified to 64-bit arithmetic: they must come to 9 digits, the standard deviations and the RSS (which are rounding) to 2.
    //   - BoxBOD and MGH10 from start 1 are far from the answer (NIST rates both "higher difficulty"); there Levenberg-Marquardt may stop. What must hold: it then
    //     says so (no fit), and never shows a wrong answer as a fit.
    int runs = 0, passed = 0;
    for (const auto& [name, f] : strdModels()) {
        Strd s;
        if (!readStrd(name, s)) { check("StRD " + name + ": file read", false); continue; }
        // (Rat43's file prints 9 degrees of freedom for 15 rows and 4 parameters; its certified residual standard deviation is sqrt(RSS / 11): the 9 is a misprint)
        check("StRD " + name + ": the certified residual standard deviation is sqrt(RSS / (rows - parameters))", rel(s.rsd * s.rsd * (s.x.size() - s.cert.size()), s.rss, 1e-9));
        for (int st = 1; st <= 2; st++) {
            const FitResult r = fitCurve(f, st == 1 ? s.start1 : s.start2, s.x, s.y);
            runs++;
            double worstP = 11, worstSd = 11;
            if (r.ok) {
                for (std::size_t i = 0; i < s.cert.size(); i++) { worstP = std::min(worstP, lre(r.p[i], s.cert[i])); worstSd = std::min(worstSd, lre(r.err[i], s.certSd[i])); }
                worstSd = std::min({worstSd, lre(r.ssr, s.rss), lre(r.s, s.rsd)});
            } else worstP = worstSd = 0;
            const double worst = std::min(worstP, worstSd);
            std::printf("  StRD %-9s start %d: %s, %d steps, LRE parameters %.1f, errors and RSS %.1f\n", name.c_str(), st, r.ok ? "ok" : r.error.c_str(), r.iterations, worstP, worstSd);
            const std::string what = "StRD " + name + " from start " + std::to_string(st);
            if (name == "Lanczos1") check(what + ": parameters to 9 digits, errors and RSS (64-bit rounding) to 2", r.ok && worstP >= 9 && worstSd >= 2);
            else if (st == 1 && (name == "BoxBOD" || name == "MGH10")) check(what + ": a fit, if one is given, is the certified one (6 digits)", !r.ok || worst >= 6);
            else check(what + ": parameters, their standard deviations and the RSS to 6 digits", worst >= 6);
            if (worst >= 6) passed++;
        }
    }
    std::printf("  StRD: %d of %d runs to 6 digits\n", passed, runs);

    // The ready-made models on the same certified problems (their own start values, no NIST start): DanWood is y = A x^n, Misra1a and BoxBOD are A (1 - e^(-x/tau))
    // with tau = 1/b2 (sigma_tau = sigma_b2 / b2^2), Eckerle4 is a Gaussian of height b1/b2, centre b3, width b2.
    {
        Strd s; readStrd("DanWood", s);
        const FitResult r = fitModel(Model::Power, s.x, s.y);
        check("Power law on DanWood: A, n and their errors as certified (6 digits)", r.ok && lre(r.p[0], s.cert[0]) >= 6 && lre(r.p[1], s.cert[1]) >= 6 && lre(r.err[0], s.certSd[0]) >= 6 && lre(r.err[1], s.certSd[1]) >= 6);
    }
    for (const char* nm : {"Misra1a", "BoxBOD"}) {
        Strd s; readStrd(nm, s);
        const FitResult r = fitModel(Model::Rise, s.x, s.y);
        const double tau = 1 / s.cert[1], sTau = s.certSd[1] / (s.cert[1] * s.cert[1]);
        check(std::string("Rise to a limit on ") + nm + ": A, tau = 1/b2 and their errors as certified (6 digits)",
              r.ok && lre(r.p[0], s.cert[0]) >= 6 && lre(r.p[1], tau) >= 6 && lre(r.err[0], s.certSd[0]) >= 6 && lre(r.err[1], sTau) >= 5.5);
        if (r.ok) std::printf("  Rise on %s: A %.10g (%.10g), tau %.10g (%.10g)\n", nm, r.p[0], s.cert[0], r.p[1], tau);
    }
    {
        Strd s; readStrd("Eckerle4", s);
        const FitResult r = fitModel(Model::Gaussian, s.x, s.y);
        check("Gaussian on Eckerle4: height b1/b2, centre b3, width b2 as certified (6 digits)",
              r.ok && lre(r.p[0], s.cert[0] / s.cert[1]) >= 6 && lre(r.p[1], s.cert[2]) >= 6 && lre(r.p[2], s.cert[1]) >= 6 && lre(r.err[1], s.certSd[2]) >= 6 && lre(r.ssr, s.rss) >= 6);
    }
}

// ---- straight lines against the closed forms (Bevington and Robinson, Data Reduction and Error Analysis, 3rd ed., eqs. 6.12 and 6.21-6.22) ----
static void testLines() {
    const std::vector<double> x = {1, 2, 3, 4, 5, 6, 7}, y = {2.1, 3.9, 6.2, 7.8, 10.1, 12.2, 13.8}, sy = {0.1, 0.2, 0.1, 0.3, 0.2, 0.1, 0.2};
    // weighted: Delta = S Sxx - Sx^2, a = (Sxx Sy - Sx Sxy)/Delta, b = (S Sxy - Sx Sy)/Delta, sigma_a^2 = Sxx/Delta, sigma_b^2 = S/Delta
    double S = 0, Sx = 0, Sy = 0, Sxx = 0, Sxy = 0;
    for (std::size_t i = 0; i < x.size(); i++) { const double w = 1 / (sy[i] * sy[i]); S += w; Sx += w * x[i]; Sy += w * y[i]; Sxx += w * x[i] * x[i]; Sxy += w * x[i] * y[i]; }
    const double D = S * Sxx - Sx * Sx, a = (Sxx * Sy - Sx * Sxy) / D, b = (S * Sxy - Sx * Sy) / D;
    double chi2 = 0;
    for (std::size_t i = 0; i < x.size(); i++) chi2 += std::pow((y[i] - a - b * x[i]) / sy[i], 2);
    const FitResult r = fitModel(Model::Line, x, y, sy);
    check("weighted line: a, b, sigma_a, sigma_b, chi^2 as the closed form (1e-12)", r.ok && r.weighted && rel(r.p[0], a, 1e-12) && rel(r.p[1], b, 1e-12) && rel(r.err[0], std::sqrt(Sxx / D), 1e-12) &&
          rel(r.err[1], std::sqrt(S / D), 1e-12) && rel(r.chi2, chi2, 1e-12) && r.dof == 5 && rel(r.q, chi2Tail(chi2, 5), 1e-12));
    // the general engine reaches the same line
    const FitResult g = fitCurve([](const double* p, double xx) { return p[0] + p[1] * xx; }, {0, 1}, x, y, sy);
    check("weighted line by Levenberg-Marquardt = the exact solve (1e-9)", g.ok && rel(g.p[0], a, 1e-9) && rel(g.p[1], b, 1e-9) && rel(g.err[1], r.err[1], 1e-6));
    // unweighted: the errors from the scatter, s^2 = SSR/(n - 2): sigma_b^2 = s^2 / sum (x - xbar)^2
    {
        double mx = 0, my = 0;
        for (std::size_t i = 0; i < x.size(); i++) { mx += x[i]; my += y[i]; }
        mx /= x.size(); my /= x.size();
        double sxx = 0, sxy = 0;
        for (std::size_t i = 0; i < x.size(); i++) { sxx += (x[i] - mx) * (x[i] - mx); sxy += (x[i] - mx) * (y[i] - my); }
        const double bb = sxy / sxx, aa = my - bb * mx;
        double ssr = 0;
        for (std::size_t i = 0; i < x.size(); i++) ssr += std::pow(y[i] - aa - bb * x[i], 2);
        const double s2 = ssr / (x.size() - 2);
        const FitResult u = fitModel(Model::Line, x, y);
        check("unweighted line: slope, intercept and errors from the scatter as the closed form", u.ok && !u.weighted && rel(u.p[1], bb, 1e-12) && rel(u.p[0], aa, 1e-12) &&
              rel(u.err[1], std::sqrt(s2 / sxx), 1e-12) && rel(u.err[0], std::sqrt(s2 * (1.0 / x.size() + mx * mx / sxx)), 1e-12) && u.q < 0);
        // through the origin: b = sum xy / sum x^2
        double sxy0 = 0, sxx0 = 0;
        for (std::size_t i = 0; i < x.size(); i++) { sxy0 += x[i] * y[i]; sxx0 += x[i] * x[i]; }
        const FitResult pr = fitModel(Model::Proportional, x, y);
        check("through the origin: b = sum(xy) / sum(x^2)", pr.ok && rel(pr.p[0], sxy0 / sxx0, 1e-12) && pr.dof == 6);
    }
    // errors only in x on a line: the maximum-likelihood answer is the regression of x on y, inverted (Orear's effective variance with the slope inside the minimisation)
    {
        const std::vector<double> sx(x.size(), 0.05);
        const FitResult ex = fitModel(Model::Line, x, y, {}, sx);
        double mx = 0, my = 0;
        for (std::size_t i = 0; i < x.size(); i++) { mx += x[i]; my += y[i]; }
        mx /= x.size(); my /= x.size();
        double syy = 0, sxy = 0;
        for (std::size_t i = 0; i < x.size(); i++) { syy += (y[i] - my) * (y[i] - my); sxy += (x[i] - mx) * (y[i] - my); }
        const double bInv = syy / sxy;            // x = c + d y  ->  y = -c/d + x/d, d = sxy / syy
        check("errors only in x: the slope is 1 / (the slope of x on y) (1e-7)", ex.ok && ex.xErrors && rel(ex.p[1], bInv, 1e-7) && rel(ex.p[0], my - bInv * mx, 1e-6));
    }
    // a polynomial reproduces an exact cubic
    {
        std::vector<double> xs, ys;
        for (int i = -5; i <= 6; i++) { xs.push_back(i * 0.7); ys.push_back(1.5 - 2 * xs.back() + 0.25 * xs.back() * xs.back() - 0.125 * std::pow(xs.back(), 3)); }
        const FitResult c = fitModel(Model::Cubic, xs, ys);
        check("cubic: an exact cubic comes back (1e-11)", c.ok && rel(c.p[0], 1.5, 1e-11) && rel(c.p[1], -2, 1e-11) && rel(c.p[2], 0.25, 1e-11) && rel(c.p[3], -0.125, 1e-11));
    }
    // refused: too few rows; all x the same
    check("a line through one point is refused", !fitModel(Model::Line, {1}, {2}).ok);
    check("a line with every x the same is refused", !fitModel(Model::Line, {1, 1, 1}, {2, 3, 4}).ok);
    check("a power law with x <= 0 is refused", !fitModel(Model::Power, {-1, 1, 2}, {1, 1, 4}).ok);
}

// ---- the other ready-made models, on exact data from known parameters ----
static void testModels() {
    struct Case { Model m; std::vector<double> p; double x0, x1; };
    const std::vector<Case> cases = {
        {Model::Power, {2.5, -1.7}, 0.5, 9},
        {Model::ExpDecay, {12, 3.3}, 0, 10},
        {Model::ExpDecayOffset, {-7, 1.9, 4.2}, 0, 9},
        {Model::Rise, {3.3, 0.8}, 0.05, 4},
        {Model::Gaussian, {5, 1.2, 0.4}, -1, 3},
        {Model::GaussianOffset, {-2, 10.5, 0.7, 3}, 7, 14},
        {Model::Lorentzian, {8, 656.28, 0.2, 0.5}, 655, 657.5},
    };
    for (const Case& c : cases) {
        std::vector<double> x, y;
        for (int i = 0; i < 41; i++) { x.push_back(c.x0 + (c.x1 - c.x0) * i / 40.0); y.push_back(modelValue(c.m, c.p.data(), x.back())); }
        // a small, fixed wiggle so the scatter is not zero (alternating +-1e-6 of the range of y)
        double lo = 1e300, hi = -1e300;
        for (double v : y) { lo = std::min(lo, v); hi = std::max(hi, v); }
        for (std::size_t i = 0; i < y.size(); i++) y[i] += (i % 2 ? 1 : -1) * 1e-6 * (hi - lo);
        const FitResult r = fitModel(c.m, x, y);
        bool good = r.ok;
        for (std::size_t k = 0; good && k < c.p.size(); k++) good = std::fabs(r.p[k] - c.p[k]) <= 1e-4 * std::max(1.0, std::fabs(c.p[k]));
        check(std::string("model ") + modelInfo(c.m).name + ": its own start values reach the parameters that made the data", good);
        // every parameter's unit powers are named, and the ascii names are plain
        for (const ParamInfo& pi : modelInfo(c.m).params) for (const char* q = pi.ascii; *q; q++) if (!std::isalnum(static_cast<unsigned char>(*q)) && *q != '_') check(std::string("ascii name ") + pi.ascii, false);
    }
    // the band: for a straight line sigma_f(x)^2 = sigma_a^2 + 2 x cov_ab + x^2 sigma_b^2
    const FitResult r = fitModel(Model::Line, {1, 2, 3, 4, 5}, {1.1, 1.9, 3.2, 3.9, 5.1});
    const double x = 2.7, v = r.cov[0] + 2 * x * r.cov[1] + x * x * r.cov[3];
    check("confidence band of a line: sqrt(var a + 2x cov + x^2 var b)", r.ok && rel(modelBand(Model::Line, r.p, r.cov, x), std::sqrt(v), 1e-6));
}

// ---- the chi^2 tail against printed tables (the 5 % and 1 % points: e.g. Bevington and Robinson Table C.4; any statistics table gives the same) ----
static void testChi2() {
    check("Q(3.841 | 1) = 0.05", std::fabs(chi2Tail(3.841459, 1) - 0.05) < 1e-6);
    check("Q(5.991 | 2) = 0.05 (= e^(-chi2/2) exactly for 2 dof)", std::fabs(chi2Tail(5.991465, 2) - 0.05) < 1e-6 && rel(chi2Tail(3.0, 2), std::exp(-1.5), 1e-13));
    check("Q(11.070 | 5) = 0.05", std::fabs(chi2Tail(11.070498, 5) - 0.05) < 1e-6);
    check("Q(23.209 | 10) = 0.01", std::fabs(chi2Tail(23.209251, 10) - 0.01) < 1e-6);
    check("Q(124.342 | 100) = 0.05", std::fabs(chi2Tail(124.342113, 100) - 0.05) < 1e-6);
    check("Q(0 | 4) = 1 and the tail falls with chi^2", chi2Tail(0, 4) == 1 && chi2Tail(10, 4) < chi2Tail(5, 4));
}


// ---- units, transforms, text in and out ----
static void testText() {
    check("unitPower(au, 3/2) = au^(3/2)", unitPower("au", 3, 2) == "au^(3/2)");
    check("unitPower(cm^-1, 2) = cm^-2", unitPower("cm^-1", 2) == "cm^-2");
    check("unitPower(km/s, 2) = (km/s)^2", unitPower("km/s", 2) == "(km/s)^2");
    check("unitPower(\"\", 2) is still a plain number", unitPower("", 2).empty());
    check("paramUnit: a slope of yr against au is yr/au", paramUnit("yr", "au", 1, -1) == "yr/au");
    check("paramUnit: a slope of a plain number against au is au^-1", paramUnit("", "au", 1, -1) == "au^-1");
    check("paramUnit: tau in x's unit", paramUnit("V", "s", 0, 1) == "s");
    // every such unit means what it says in the calculator
    const auto dimOf = [](const std::string& u) { return calc::evaluate("1 (" + u + ")"); };
    {
        const calc::Result a = dimOf(paramUnit("km/s", "s", 1, -1)), b = dimOf("m/s^2");
        check("paramUnit(km/s per s) reads as an acceleration in the calculator", a.ok && b.ok && a.q.dim == b.q.dim && rel(a.q.v, 1000, 1e-12));
        const calc::Result c = dimOf(paramUnit("cm^-1", "au", 1, -2)), d = dimOf("1/m^3");
        check("paramUnit(cm^-1 per au^2) reads as 1/m^3", c.ok && d.ok && c.q.dim == d.q.dim);
        const calc::Result e = dimOf(transformedUnit("au", Transform::Sqrt)), f = dimOf("m^(1/2)");
        check("the square root of au reads as m^(1/2)", e.ok && f.ok && e.q.dim == f.q.dim);
    }
    // transforms: values and the uncertainty carried by the derivative
    {
        double o, os;
        check("1/x of 4 ± 0.2 is 0.25 ± 0.0125", applyTransform(Transform::Inverse, 4, 0.2, o, os) && o == 0.25 && rel(os, 0.0125, 1e-15));
        check("ln of e ± 0.1 is 1 ± 0.1/e", applyTransform(Transform::Ln, std::exp(1.0), 0.1, o, os) && rel(o, 1, 1e-15) && rel(os, 0.1 / std::exp(1.0), 1e-15));
        check("ln of a negative number is refused", !applyTransform(Transform::Ln, -1, 0, o, os));
        check("1/x² of 2 ± 0.1 is 0.25 ± 0.025", applyTransform(Transform::InverseSquare, 2, 0.1, o, os) && o == 0.25 && rel(os, 0.025, 1e-15));
        check("axis label of ln a in au", axisLabel("a", "au", Transform::Ln) == "ln(a / au)");
    }
    // the user's own data
    {
        Table t; std::string why; std::vector<std::string> notes;
        check("text: a header with units and an uncertainty column", parseText("# a pendulum\nL (m), T (s), σT (s)\n0.2, 0.90, 0.02\n0.4, 1.27, 0.02\n0.8, 1.79, 0.03\n", t, why, notes) &&
              t.cols.size() == 2 && t.cols[0].unit == "m" && t.cols[1].unit == "s" && t.cols[1].sigma.size() == 3 && t.cols[1].sigma[2] == 0.03 && t.rows() == 3);
        check("text: semicolons with decimal commas", parseText("1,5;2,25\n2,5;6,25\n", t, why, notes) && t.cols.size() == 2 && t.cols[1].v[0] == 2.25);
        check("text: tabs, no header", parseText("1\t2\n3\t4\n", t, why, notes) && t.cols.size() == 2 && t.cols[0].name == "column 1" && t.cols[1].v[1] == 4);
        check("text: an uncertainty in mm of a column in m is converted", parseText("x [m]\terr [mm]\n1\t5\n2\t5\n", t, why, notes) && t.cols.size() == 1 && rel(t.cols[0].sigma[0], 0.005, 1e-12));
        check("text: an unknown unit is kept as a label, with a note", parseText("x (furlongs), y\n1, 2\n2, 3\n", t, why, notes) && t.cols[0].unit.empty() && !notes.empty());
        check("text: a text column and groups", parseText("set, x, y\nA, 1, 2\nA, 2, 3\nB, 1, 5\nB, 2, 7\n", t, why, notes) && t.cols[0].isText() && t.group == 0 && groupValues(t).size() == 2);
        check("text: nothing to read is refused", !parseText("# only a comment\n\n", t, why, notes) && !why.empty());
        check("text: the typographic minus and ×10^", parseText("x, y\n−2, 6.02×10^23\n1, 1\n", t, why, notes) && t.cols[0].v[0] == -2 && t.cols[1].v[0] == 6.02e23);
    }
}


// ---- 0.51.1: held parameters, the width's sign, correlations, scaled errors, York's line, x³, uncertainty headers ----
static void testFitExtras() {
    // York et al., Am. J. Phys. 72, 367 (2004), section III: the iteration for a straight line with errors in x and y (no x-y correlation), written out here from the
    // paper's equations as the independent reference: W_i = w_X w_Y / (w_X + b² w_Y), beta_i = W_i (U_i / w_Y + b V_i / w_X), b = Σ W beta V / Σ W beta U, and the
    // errors from the "least-squares adjusted" x: σ_b² = 1 / Σ W u², σ_a² = 1 / Σ W + x̄² σ_b². Pearson's data with York's weights (the paper's test case).
    {
        const std::vector<double> X = {0, 0.9, 1.8, 2.6, 3.3, 4.4, 5.2, 6.1, 6.5, 7.4}, wX = {1000, 1000, 500, 800, 200, 80, 60, 20, 1.8, 1},
                                  Y = {5.9, 5.4, 4.4, 4.6, 3.5, 3.7, 2.8, 2.8, 2.4, 1.5}, wY = {1, 1.8, 4, 8, 20, 20, 70, 70, 100, 500};
        const std::size_t n = X.size();
        double b = -0.5, a = 0, Xb = 0, Yb = 0;
        std::vector<double> W(n), beta(n);
        for (int it = 0; it < 200; it++) {
            double sw = 0, swx = 0, swy = 0;
            for (std::size_t i = 0; i < n; i++) { W[i] = wX[i] * wY[i] / (wX[i] + b * b * wY[i]); sw += W[i]; swx += W[i] * X[i]; swy += W[i] * Y[i]; }
            Xb = swx / sw; Yb = swy / sw;
            double num = 0, den = 0;
            for (std::size_t i = 0; i < n; i++) { const double U = X[i] - Xb, V = Y[i] - Yb; beta[i] = W[i] * (U / wY[i] + b * V / wX[i]); num += W[i] * beta[i] * V; den += W[i] * beta[i] * U; }
            b = num / den;
        }
        a = Yb - b * Xb;
        double sw = 0, swx = 0;
        for (std::size_t i = 0; i < n; i++) { sw += W[i]; swx += W[i] * (Xb + beta[i]); }
        const double xb = swx / sw;
        double swu = 0;
        for (std::size_t i = 0; i < n; i++) { const double u = Xb + beta[i] - xb; swu += W[i] * u * u; }
        const double sb = std::sqrt(1 / swu), sa = std::sqrt(1 / sw + xb * xb * sb * sb);
        std::vector<double> sX, sY;
        for (std::size_t i = 0; i < n; i++) { sX.push_back(1 / std::sqrt(wX[i])); sY.push_back(1 / std::sqrt(wY[i])); }
        const FitResult r = fitModel(Model::Line, X, Y, sY, sX);
        std::printf("  York 2004 on Pearson's data: a = %.6f ± %.6f, b = %.6f ± %.6f (York's iteration: %.6f ± %.6f, %.6f ± %.6f)\n", r.p[0], r.err[0], r.p[1], r.err[1], a, sa, b, sb);
        check("errors in x and y: Pearson's data give York's a, b, σa and σb (1e-5)", r.ok && rel(r.p[0], a, 1e-5) && rel(r.p[1], b, 1e-5) && rel(r.err[0], sa, 1e-5) && rel(r.err[1], sb, 1e-5));
    }
    // a held parameter: with n held, y = A xⁿ is linear in A: A = Σ y xⁿ / Σ x²ⁿ, σ_A = s / √Σ x²ⁿ with s² = SSR / (rows − 1) (one free parameter)
    {
        const std::vector<double> x = {0.39, 0.72, 1.0, 1.52, 5.2, 9.5};
        std::vector<double> y;
        for (std::size_t i = 0; i < x.size(); i++) y.push_back(1.0003 * std::pow(x[i], 1.5) * (1 + (i % 2 ? 2e-4 : -3e-4)));
        double sxy = 0, sxx = 0;
        for (std::size_t i = 0; i < x.size(); i++) { sxy += y[i] * std::pow(x[i], 1.5); sxx += std::pow(x[i], 3.0); }
        const double A = sxy / sxx;
        double ssr = 0;
        for (std::size_t i = 0; i < x.size(); i++) ssr += std::pow(y[i] - A * std::pow(x[i], 1.5), 2);
        const double sA = std::sqrt(ssr / (x.size() - 1)) / std::sqrt(sxx);
        const FitResult r = fitModel(Model::Power, x, y, {}, {}, {NAN, 1.5});
        check("power law with n held at 3/2: A = Σ y x^1.5 / Σ x³ and its error from the scatter (1e-8)", r.ok && r.dof == 5 && r.p[1] == 1.5 && r.err[1] == 0 && rel(r.p[0], A, 1e-8) && rel(r.err[0], sA, 1e-6));
        check("a held parameter has no covariance and a note says it is held", r.ok && r.cov.size() == 4 && r.cov[1] == 0 && r.cov[2] == 0 && r.cov[3] == 0 && !r.notes.empty());
        const FitResult l = fitModel(Model::Line, {1, 2, 3, 4}, {3.1, 4.9, 7.2, 8.8}, {}, {}, {NAN, 2.0});
        check("a line with its slope held at 2: a = mean(y − 2x)", l.ok && rel(l.p[0], (3.1 - 2 + 4.9 - 4 + 7.2 - 6 + 8.8 - 8) / 4, 1e-9) && l.dof == 3);
        check("every parameter held is refused", !fitModel(Model::Line, {1, 2, 3}, {1, 2, 3}, {}, {}, {1.0, 1.0}).ok);
        check("x and y of different lengths are refused (no read past the end)", !fitModel(Model::Rise, {1, 2, 3, 4}, {1, 2}).ok);
    }
    // a negative width: the same curve, and after positiveWidth the same parameters, covariance and band as a fit that ended on the positive width
    {
        std::vector<double> x, y;
        for (int i = 0; i < 41; i++) { x.push_back(500 + 0.5 * i); y.push_back(3 * std::exp(-0.5 * std::pow((x.back() - 510.3) / 2.2, 2)) + 0.01 * std::sin(7.0 * i)); }
        const CurveFn f = [](const double* q, double xx) { return modelValue(Model::Gaussian, q, xx); };
        FitResult neg = fitCurve(f, {2.5, 510, -2.0}, x, y);
        const bool wasNeg = neg.ok && neg.p[2] < 0;
        positiveWidth(Model::Gaussian, neg);
        const FitResult pos = fitModel(Model::Gaussian, x, y);
        bool same = wasNeg && pos.ok && neg.p[2] > 0;
        for (std::size_t k = 0; same && k < 9; k++) same = std::fabs(neg.cov[k] - pos.cov[k]) <= 1e-6 * std::sqrt(std::fabs(pos.cov[k / 3 * 4] * pos.cov[k % 3 * 4]));
        for (double xx : {505.0, 510.0, 513.0}) same = same && rel(modelBand(Model::Gaussian, neg.p, neg.cov, xx), modelBand(Model::Gaussian, pos.p, pos.cov, xx), 1e-4);
        check("a Gaussian that ends on a negative σ: made positive with its covariances' signs, the same band as the positive fit", same);
    }
    // correlations: for a weighted line ρ_ab = −S_x / √(S S_xx) (Bevington eq. 6.21-6.22 for the covariance)
    {
        const std::vector<double> x = {1, 2, 3, 4, 5, 6}, y = {2.1, 3.9, 6.2, 7.8, 10.1, 12.2}, s = {0.1, 0.2, 0.1, 0.3, 0.2, 0.1};
        double S = 0, Sx = 0, Sxx = 0;
        for (std::size_t i = 0; i < x.size(); i++) { const double w = 1 / (s[i] * s[i]); S += w; Sx += w * x[i]; Sxx += w * x[i] * x[i]; }
        const FitResult r = fitModel(Model::Line, x, y, s);
        const std::vector<double> c = correlation(r);
        check("correlation matrix of a weighted line: 1 on the diagonal, ρ_ab = −S_x/√(S S_xx), symmetric", r.ok && c.size() == 4 && c[0] == 1 && c[3] == 1 && rel(c[1], -Sx / std::sqrt(S * Sxx), 1e-10) && c[1] == c[2]);
        // scaled errors: with every σ the same, the weighted fit's errors times √(χ²/dof) are the unweighted fit's (both are s² (AᵀA)⁻¹)
        const std::vector<double> same(x.size(), 0.37);
        const FitResult w = scaledErrors(fitModel(Model::Line, x, y, same)), u = fitModel(Model::Line, x, y);
        check("errors scaled by √(χ²/dof), all σ equal: the unweighted fit's errors (1e-10)", w.ok && rel(w.err[0], u.err[0], 1e-10) && rel(w.err[1], u.err[1], 1e-10) && rel(w.p[1], u.p[1], 1e-12));
        check("scaling leaves an unweighted fit as it is", scaledErrors(u).err == u.err);
    }
    // x³ and its uncertainty: 2 ± 0.1 cubed is 8 ± 1.2; the unit km³
    {
        double o, os;
        check("x³ of 2 ± 0.1 is 8 ± 1.2", applyTransform(Transform::Cube, 2, 0.1, o, os) && o == 8 && rel(os, 1.2, 1e-14));
        check("x³ of km is km^3", transformedUnit("km", Transform::Cube) == "km^3" && axisLabel("a", "km", Transform::Cube) == "a³ (km^3)");
        check("cube has an id and comes back from it", transformById(transformId(Transform::Cube)) == Transform::Cube);
    }
    // uncertainty headers: dV, ΔT and δ x after their column; a "dx" after another column is a column of its own
    {
        Table t; std::string why; std::vector<std::string> notes;
        check("text: dV after V is V's uncertainty", parseText("t (s), V (V), dV (V)\n0, 5, 0.1\n1, 3, 0.2\n", t, why, notes) && t.cols.size() == 2 && t.cols[1].hasSigma() && t.cols[1].sigma[1] == 0.2);
        check("text: ΔT (mK) after T (K) is converted", parseText("x, T (K), ΔT (mK)\n1, 300, 5\n2, 310, 5\n", t, why, notes) && t.cols.size() == 2 && rel(t.cols[1].sigma[0], 0.005, 1e-12));
        check("text: \"δ x\" after x is x's uncertainty", parseText("x, δ x, y\n1, 0.1, 2\n2, 0.1, 4\n", t, why, notes) && t.cols.size() == 2 && t.cols[0].hasSigma());
        check("text: dx after y is a column of its own", parseText("y, dx\n1, 2\n3, 4\n", t, why, notes) && t.cols.size() == 2 && !t.cols[0].hasSigma());
    }
}

// ---- 0.51.1: the recording's osculating orbit, against an exact Kepler state; the perihelion's relativistic turning ----
static void testRecording() {
    const double GM = units::kGMSun, pi = 3.141592653589793;
    const double a = 1.52, e = 0.093, I = 1.85, node = 49.56, peri = 286.5;    // degrees (any inclined orbit; these are near Mars's)
    std::vector<RecSample> s;
    bool each = true;
    for (int k = 0; k < 40; k++) {
        const orbit::State st = orbit::keplerState(a * units::kAU, e, I, node, peri, 9.0 * k);
        const RecSample x = recSample(k, st.pos, st.vel);
        s.push_back(x);
        each = each && rel(x.energy, -GM / (2 * a * units::kAU) / 1e6, 1e-11) && rel(x.h, std::sqrt(GM * a * units::kAU * (1 - e * e)) / 1e6, 1e-11) && rel(x.aAu, a, 1e-11) &&
               std::fabs(x.e - e) < 1e-12 && std::fabs(x.varpiDeg - std::fmod(node + peri, 360.0)) < 1e-8;
    }
    check("recording: E = −GM☉/2a, h = √(GM☉ a (1 − e²)), a, e and ϖ = Ω + ω of a Kepler state, all along the orbit", each);
    // ϖ near 360° unwraps: an orbit whose ϖ steps from 359.9° on keeps counting
    std::vector<RecSample> w;
    for (int k = 0; k < 5; k++) {
        const orbit::State st = orbit::keplerState(units::kAU, 0.1, 0.0, 0.0, 359.9 + 0.05 * k, 30.0 * k);
        w.push_back(recSample(k, st.pos, st.vel));
    }
    const Table t = orbitRecording("test", w, false, false);
    bool up = true;
    for (std::size_t k = 1; k < t.rows(); k++) up = up && std::fabs(t.cols[7].v[k] - t.cols[7].v[k - 1] - 0.05) < 1e-8;
    check("recording: ϖ is unwrapped past 360° (359.9, 359.95, 360.0, 360.05 ...)", up && t.cols[7].v.back() > 360);
    // every unit of the recording reads in the calculator, every preset's columns are numbers
    bool units_ = true;
    for (const Column& c : t.cols) units_ = units_ && unitKnown(c.unit);
    for (const Preset& p : t.presets) units_ = units_ && p.x < static_cast<int>(t.cols.size()) && p.y < static_cast<int>(t.cols.size());
    check("recording: units the calculator reads, presets on its columns", units_);
    // vis-viva on the new samples: slope 2GM☉ in km²/s² au, intercept −GM☉/a
    {
        std::vector<double> x, y;
        for (const RecSample& r : s) { x.push_back(1 / r.rAu); y.push_back(r.vKmS * r.vKmS); }
        const FitResult r = fitModel(Model::Line, x, y);
        check("vis-viva: slope 2GM☉ in km²/s² au, intercept −GM☉/a", r.ok && rel(r.p[1], 2 * GM / 1e6 / units::kAU, 1e-10) && rel(r.p[0], -GM / (a * units::kAU) / 1e6, 1e-9));
    }
    // general relativity: Mercury's 42.98″ per century (the published value the N-body test also uses: Clemence 1947 and every textbook since), from JPL's a and e of Mercury
    const double gr = grPrecessionArcsecPerCentury(orbit::kPlanets[0].el[0], orbit::kPlanets[0].el[2]);
    std::printf("  GR perihelion turning of Mercury: %.3f arcsec per century\n", gr);
    check("GR: Mercury's perihelion turns 42.98″ per century (0.02″)", std::fabs(gr - 42.98) < 0.02);
    // the recording on the app's own Real simulation: Mercury's ϖ over 40 years, relativity off and on (the same start date). The planets turn it about 532″ per century
    // (Clemence 1947; tests/test_nbody.cpp checks the model to 5 %); relativity adds 42.98″. The difference of the two runs cancels the planets' periodic wobble.
    double slope[2] = {0, 0};
    for (int g = 0; g < 2; g++) {
        orbit::SolarSystem sys;
        sys.loadAt(1767225600000LL);                 // 1 Jan 2026
        sys.setRealPlanets(true);
        sys.setRelativity(g == 1);
        std::vector<RecSample> rec;
        const int steps = 4000;
        const double per = 40 * units::kYear / steps;
        for (int k = 0; k <= steps; k++) {
            for (const orbit::Body& b : sys.bodies) if (b.name == "Mercury") { rec.push_back(recSample(k * per / units::kDay, b.pos, b.vel)); break; }
            sys.step(per / units::kSimTime, 0);
        }
        const Table rt = orbitRecording("Mercury", rec, true, g == 1);
        const Preset& p = rt.presets[3];
        const FitResult r = fitModel(p.model, rt.cols[std::size_t(p.x)].v, rt.cols[std::size_t(p.y)].v);
        slope[g] = r.ok ? r.p[1] * 36525 * 3600 : 0;     // deg/d -> arcsec per Julian century
    }
    std::printf("  Mercury's perihelion, recorded in Real over 40 years: %.2f″/century without relativity, %.2f″ with (difference %.2f″)\n", slope[0], slope[1], slope[1] - slope[0]);
    check("recording: Mercury's ϖ turns about 532″ per century from the planets (3 %)", std::fabs(slope[0] / 532 - 1) < 0.03);
    check("recording: relativity on adds 42.98″ per century (0.5″)", std::fabs(slope[1] - slope[0] - 42.98) < 0.5);
    (void)pi;
}

// ---- the labs' tables ----
static void testTables() {
    const double pi = 3.141592653589793;
    LabContext c;
    Table t;
    // every table: its units are the calculator's, its presets point at columns of numbers, every column has a source, and the CSV reads back to the same numbers
    std::vector<LabContext> ctxs;
    for (int lab = 0; lab < 3; lab++) { LabContext x; x.lab = lab; ctxs.push_back(x); }
    { LabContext x; x.lab = 3; check("0.55.0: the Black Hole Lab has no tables yet (none of another lab's)", labTables(x).empty()); }
    for (int z : {1, 2, 3, 11, 12, 20, 26, 55, 92}) { LabContext x; x.lab = 1; x.Z = z; x.N = atom::defaultN(z); ctxs.push_back(x); }
    for (const char* m : {"HCl", "CO", "N2", "H2O", "CH3OH", "CO2"}) { LabContext x; x.lab = 2; x.molecule = m; ctxs.push_back(x); }
    int tables = 0;
    for (const LabContext& x : ctxs)
        for (const TableRef& ref : labTables(x)) {
            tables++;
            if (!makeTable(ref.id, x, t)) { check("table " + ref.id + " builds", false); continue; }
            const std::string w = "table \"" + t.title + "\"";
            bool units = true, sized = true, sourced = !t.sources.empty();
            for (const Column& col : t.cols) {
                units = units && (col.isText() || unitKnown(col.unit));
                sized = sized && (col.isText() ? col.text.size() : col.v.size()) == t.rows() && (col.sigma.empty() || col.sigma.size() == col.v.size());
                sourced = sourced && (col.isText() || !col.source.empty() || col.name == "v" || col.name == "v + 1" || col.name == "degeneracy");
                if (!col.isText() && !unitKnown(col.unit)) std::printf("  unknown unit %s in %s\n", col.unit.c_str(), t.title.c_str());
            }
            check(w + ": every unit is one the calculator reads", units);
            check(w + ": every column has a row for every row", sized);
            check(w + ": sources named", sourced);
            for (const Preset& p : t.presets) {
                const bool cols = p.x >= 0 && p.y >= 0 && p.x < static_cast<int>(t.cols.size()) && p.y < static_cast<int>(t.cols.size()) && !t.cols[std::size_t(p.x)].isText() && !t.cols[std::size_t(p.y)].isText();
                check(w + ", preset \"" + p.label + "\": its columns are columns of numbers", cols);
                if (!cols) continue;
                for (const ParamInfo& pi2 : modelInfo(p.model).params) {
                    const std::string u = paramUnit(transformedUnit(t.cols[std::size_t(p.y)].unit, p.ty), transformedUnit(t.cols[std::size_t(p.x)].unit, p.tx), pi2.yPow, pi2.xPow);
                    check(w + ", preset \"" + p.label + "\": parameter " + pi2.ascii + "'s unit \"" + u + "\" reads in the calculator", unitKnown(u));
                }
            }
            // CSV out and back in
            Table back; std::string why; std::vector<std::string> notes;
            const bool read = parseText(toCsv(t, "test"), back, why, notes);
            bool same = read && back.cols.size() == t.cols.size();
            for (std::size_t k = 0; same && k < t.cols.size(); k++) {
                const Column& a = t.cols[k]; const Column& b = back.cols[k];
                same = a.isText() == b.isText() && a.unit == b.unit;
                if (!a.isText()) for (std::size_t r = 0; same && r < a.v.size(); r++) same = (std::isnan(a.v[r]) && std::isnan(b.v[r])) || rel(b.v[r], a.v[r], 1e-14) || a.v[r] == b.v[r];
                if (a.hasSigma()) for (std::size_t r = 0; same && r < a.sigma.size(); r++) same = (std::isnan(a.sigma[r]) && std::isnan(b.sigma[r])) || rel(b.sigma[r], a.sigma[r], 1e-14);
            }
            check(w + ": the CSV reads back to the same columns, units, numbers and uncertainties", same);
        }
    check("every lab offers tables (" + std::to_string(tables) + ")", tables >= 30);

    // Kepler's third law on the planets: n = 3/2, A = 2 pi sqrt(au^3 / GM_sun) (IAU 2009) in Julian years, within Jupiter's mass ratio (1e-3)
    c.lab = 0;
    makeTable("planets", c, t);
    {
        const FitResult r = fitModel(Model::Power, t.cols[1].v, t.cols[4].v);
        const double A = 2 * pi * std::sqrt(std::pow(units::kAU, 3) / units::kGMSun) / units::kYear;
        check("Kepler III, planets: n = 3/2 (1e-3)", r.ok && std::fabs(r.p[1] - 1.5) < 1e-3);
        check("Kepler III, planets: A = 2π √(au³/GM☉) (1e-3)", r.ok && rel(r.p[0], A, 1e-3));
        std::printf("  Kepler III, planets: n = %.6f ± %.1g, A = %.6f yr (2π√(au³/GM☉) = %.6f)\n", r.p[1], r.err[1], r.p[0], A);
    }
    // Jupiter's four big moons: n = 3/2, and GM = 4 pi^2 a^3 / T^2 from each moon agrees with the card's mass (NASA, 317.8 Earth masses, 4 digits) times G (CODATA 2022) to 0.2 %
    makeTable("moons", c, t);
    {
        std::vector<double> a, T;
        for (std::size_t r = 0; r < t.rows(); r++) if (t.cols[0].text[r] == "Jupiter") { a.push_back(t.cols[2].v[r]); T.push_back(t.cols[3].v[r]); }
        const FitResult r = fitModel(Model::Power, a, T);
        check("Kepler III, Jupiter's moons: four moons, n = 3/2 (2e-3)", a.size() == 4 && r.ok && std::fabs(r.p[1] - 1.5) < 2e-3);
        const double GMcard = 6.67430e-11 * 317.8 * units::kEarthMassKg;
        bool each = true;
        for (std::size_t i = 0; i < a.size(); i++) each = each && rel(4 * pi * pi * std::pow(a[i] * 1e3, 3) / std::pow(T[i] * 86400, 2), GMcard, 2e-3);
        check("Kepler III, Jupiter's moons: each moon's 4π²a³/T² is Jupiter's GM (0.2 %)", each);
        // 0.51.1: the presets as the panel runs them: n held at 3/2, then GM = 4π² (1 km)³ / A² (A in days); and T² against a³ through the origin, GM = 4π² / slope
        const Preset& p0 = t.presets[0];
        std::vector<double> hold(modelInfo(p0.model).params.size(), NAN);
        if (p0.hold >= 0) hold[std::size_t(p0.hold)] = p0.holdValue;
        const FitResult h = fitModel(p0.model, a, T, {}, {}, hold);
        const double GMh = h.ok ? 4 * pi * pi * 1e9 / std::pow(h.p[0] * 86400, 2) : 0;
        const Preset& p1 = t.presets[1];
        std::vector<double> x3, y2;
        for (std::size_t i = 0; i < a.size(); i++) { double o, os; applyTransform(p1.tx, a[i], NAN, o, os); x3.push_back(o); applyTransform(p1.ty, T[i], NAN, o, os); y2.push_back(o); }
        const FitResult q = fitModel(p1.model, x3, y2);
        const double GMq = q.ok ? 4 * pi * pi * 1e9 / (q.p[0] * 86400 * 86400) : 0;
        std::printf("  Jupiter's GM: n held at 3/2 %.6g, T² against a³ %.6g, card %.6g m³/s² (n free, the old recipe: %.6g)\n", GMh, GMq, GMcard, 4 * pi * pi * 1e9 / std::pow(r.p[0] * 86400, 2));
        check("Kepler III preset, n held at 3/2: GM = 4π²(1 km)³/A² is Jupiter's within 0.2 %", p0.hold == 1 && p0.holdValue == 1.5 && rel(GMh, GMcard, 2e-3));
        check("Kepler III preset, T² against a³: GM = 4π²/slope is Jupiter's within 0.2 %", p1.tx == Transform::Cube && p1.ty == Transform::Square && rel(GMq, GMcard, 2e-3));
    }
    // 0.51.1: the planets' mass against radius is a straight line on log-log axes (not a power law on linear ones, which Jupiter alone would decide)
    makeTable("planets", c, t);
    {
        const Preset& p = t.presets[1];
        std::vector<double> x, y;
        for (std::size_t i = 0; i < t.rows(); i++) { double o, os; applyTransform(p.tx, t.cols[std::size_t(p.x)].v[i], NAN, o, os); x.push_back(o); applyTransform(p.ty, t.cols[std::size_t(p.y)].v[i], NAN, o, os); y.push_back(o); }
        const FitResult r = fitModel(p.model, x, y);
        double sxy = 0, sxx = 0, mx = 0, my = 0;
        for (std::size_t i = 0; i < x.size(); i++) { mx += x[i] / x.size(); my += y[i] / y.size(); }
        for (std::size_t i = 0; i < x.size(); i++) { sxy += (x[i] - mx) * (y[i] - my); sxx += (x[i] - mx) * (x[i] - mx); }
        std::printf("  mass against radius, log-log: n = %.3f ± %.2g\n", r.p.size() > 1 ? r.p[1] : 0, r.err.size() > 1 ? r.err[1] : 0);
        check("mass against radius: log₁₀ on both axes and a straight line, slope = the least-squares slope of log M on log R", p.tx == Transform::Log10 && p.ty == Transform::Log10 && p.model == Model::Line && r.ok && rel(r.p[1], sxy / sxx, 1e-10));
    }
    // 0.51.1: the dwarf planets' and comets' periods are worked out from a with Gauss's constant k = 0.01720209895 (IAU 1976): 2π/k days at a = 1 au, exactly
    for (const char* id : {"dwarfs", "comets"}) {
        makeTable(id, c, t);
        const Preset& p = t.presets[0];
        std::vector<double> x, y;
        for (std::size_t i = 0; i < t.rows(); i++) if (std::isfinite(t.cols[std::size_t(p.x)].v[i]) && std::isfinite(t.cols[std::size_t(p.y)].v[i])) { x.push_back(t.cols[std::size_t(p.x)].v[i]); y.push_back(t.cols[std::size_t(p.y)].v[i]); }
        const FitResult r = fitModel(p.model, x, y);
        const double Ak = 2 * pi / 0.01720209895 / 365.25;
        check(std::string(id) + ": the periods are Kepler's with Gauss's k (n = 3/2, A = 2π/k days, 1e-9): the fit is circular, and the text says so",
              r.ok && std::fabs(r.p[1] - 1.5) < 1e-9 && rel(r.p[0], Ak, 1e-9) && p.expect.find("circular") != std::string::npos);
    }
    // 0.73.0: the Alpha Centauri tables (written before the tables): the A-B orbits' a^3 / P^2 equals each paper's mA + mB (Kepler's third law in au, Julian
    // years and solar masses, to the 1e-4 of GM_sun x (1 yr)^2 / (4 pi^2 au^3) - 1); Proxima b, d and c follow Kepler's law about 0.1221 Suns (the papers' own a)
    {
        check("Alpha Centauri tables build (orbits, stars, planets)", makeTable("acenorbit", c, t) && t.rows() == 3);
        for (std::size_t r = 0; r < t.rows(); r++) {
            const double a = t.cols[std::size_t(t.find("a"))].v[r] / (t.cols[std::size_t(t.find("parallax (mas)"))].v[r] / 1000), P = t.cols[std::size_t(t.find("period"))].v[r];
            const double M = t.cols[std::size_t(t.find("mass A"))].v[r] + t.cols[std::size_t(t.find("mass B"))].v[r];
            check("Alpha Centauri orbit " + t.cols[0].text[r] + ": a^3 / P^2 = mA + mB within 0.6 % (its own numbers)", rel(a * a * a / (P * P), M, 0.006));
        }
        check("Alpha Centauri stars build", makeTable("acenstars", c, t) && t.rows() == 3 && t.cols[0].text[2] == "Proxima Centauri");
        check("Alpha Centauri planets build, with status", makeTable("acenplanets", c, t) && t.rows() == 4 && t.cols[1].text[2] == "disputed" && t.cols[1].text[3].find("unconfirmed") == 0);
        const Preset& p = t.presets[0];
        std::vector<double> x, y;
        for (std::size_t i = 0; i < t.rows(); i++) if (std::isfinite(t.cols[std::size_t(p.x)].v[i]) && std::isfinite(t.cols[std::size_t(p.y)].v[i])) { x.push_back(t.cols[std::size_t(p.x)].v[i]); y.push_back(t.cols[std::size_t(p.y)].v[i]); }
        const FitResult r = fitModel(p.model, x, y);
        check("Proxima's planets: period against a is Kepler's law about 0.1221 Suns (n = 3/2 within 0.01, A within 1 % of 365.25 / sqrt(0.1221) days)", x.size() == 3 && r.ok && std::fabs(r.p[1] - 1.5) < 0.01 && rel(r.p[0], 365.25 / std::sqrt(0.1221), 0.01));
    }
    // the Rydberg constant from hydrogen's and He II's measured lines: R_M = R_inf / (1 + m_e / M) (CODATA 2022), within (Z alpha)^2 (the fine structure the Bohr levels leave out)
    for (int Z : {1, 2}) {
        c.lab = 1; c.Z = Z; c.N = Z == 1 ? 0 : 2;
        check("ion lines of Z = " + std::to_string(Z) + " build", makeTable("ionlines", c, t));
        const double M = Z == 1 ? 1836.152673426 : atom::nuclearMassMe(2, 2);
        const double want = Z * Z * kRinfCm / (1 + 1 / M), za2 = std::pow(Z * 7.2973525643e-3, 2);
        const FitResult u = fitModel(Model::Proportional, t.cols[3].v, t.cols[4].v);
        const FitResult w = fitModel(Model::Proportional, t.cols[3].v, t.cols[4].v, t.cols[4].sigma);
        std::printf("  Rydberg, Z = %d: %zu lines, slope %.4f (unweighted), %.4f (weighted, chi2/dof %.3g); Z² R_M = %.4f cm^-1\n", Z, t.rows(), u.p.empty() ? 0 : u.p[0], w.p.empty() ? 0 : w.p[0], w.redChi2, want);
        check("Rydberg, Z = " + std::to_string(Z) + ": the slope of 1/λ against 1/n₁² − 1/n₂² is Z² R_M within (Zα)²", u.ok && rel(u.p[0], want, za2));
        check("Rydberg, Z = " + std::to_string(Z) + ": weighted by NIST's uncertainties too", w.ok && w.weighted && rel(w.p[0], want, za2));
    }
    // quantum defects of sodium (the saved NIST levels reach n = 5 for s, 4 for p and d): s > p > d at the same n, d far below s and p (the d electron hardly enters the
    // 1s²2s²2p⁶ core), and each series' defect changes less and less from one n to the next (Rydberg-Ritz: δ = δ₀ + δ₂/n*²)
    c.Z = 11; c.N = 12;
    check("quantum defects of Na build", makeTable("defects", c, t));
    {
        std::map<int, std::map<int, double>> dl;   // l -> n -> δ (of the last J listed: the J split of δ, below 0.001 here, is far below these comparisons)
        for (std::size_t r = 0; r < t.rows(); r++) dl[static_cast<int>(t.cols[1].v[r])][static_cast<int>(t.cols[0].v[r])] = t.cols[6].v[r];
        bool order = true; int compared = 0;
        for (const auto& [n, d] : dl[2]) if (dl[0].count(n) && dl[1].count(n)) { compared++; order = order && dl[0][n] > dl[1][n] && dl[1][n] > d && d < dl[0][n] / 10; }
        check("Na: s > p > d at the same n, d below a tenth of s", order && compared >= 2);
        bool settle = true; int series = 0;
        for (int l = 0; l <= 2; l++) {
            std::vector<double> d;
            for (const auto& [n, v] : dl[l]) d.push_back(v);
            if (d.size() >= 3) series++;
            for (std::size_t i = 2; i < d.size(); i++) settle = settle && std::fabs(d[i] - d[i - 1]) < std::fabs(d[i - 1] - d[i - 2]);
        }
        check("Na: in each series the defect changes less and less with n (Rydberg-Ritz)", settle && series >= 1);
    }
    // B3LYP's scale factor from all the molecules: within CCCBDB's own 1-sigma of its factor
    c.lab = 2; c.molecule = "H2O";
    makeTable("modesall", c, t);
    {
        const FitResult r = fitModel(Model::Proportional, t.cols[3].v, t.cols[2].v);
        const double f = molecule::vib::scaleOf("H2O"), sf = molecule::vib::scaleSigmaOf("H2O");
        std::printf("  scale factor from %zu vibrations: %.4f ± %.4f (CCCBDB %.4f ± %.3f)\n", t.rows(), r.p.empty() ? 0 : r.p[0], r.err.empty() ? 0 : r.err[0], f, sf);
        check("the 15 molecules' measured fundamentals against B3LYP: slope within CCCBDB's ±1σ of its factor", r.ok && std::fabs(r.p[0] - f) <= sf);
    }
    // 0.53: HCl's bond length from its measured fundamental lines (Rank et al. 1962). B0 = (b - c) / 2, r0 = sqrt(h / (8 pi^2 c mu B0)). References: the lab's measured re
    // (NIST CCCBDB, 1.275 A), which r0 must exceed by a little (the zero-point average of 1/r^2), and Huber & Herzberg's B0 = Be - alpha_e / 2, which the cubic fit (with
    // the centrifugal -4D m^3) must give back within 1e-3 (the quadratic, without it, within 1 %)
    for (int iso : {0, 4}) {
        c.lab = 2; c.molecule = "HCl"; c.rotIso = iso; c.T = 298.15;
        check("HCl lines build (iso " + std::to_string(iso) + ")", makeTable("rotlines", c, t));
        double rq = 0, rc = 0, B0c = 0;
        for (int k = 0; k < 2; k++) {
            const Preset& p = t.presets[std::size_t(k)];
            std::vector<double> x, y;
            for (std::size_t i = 0; i < t.rows(); i++) {
                if (!p.group.empty() && t.cols[std::size_t(t.group)].text[i] != p.group) continue;
                if (std::isfinite(t.cols[std::size_t(p.y)].v[i])) { x.push_back(t.cols[std::size_t(p.x)].v[i]); y.push_back(t.cols[std::size_t(p.y)].v[i]); }
            }
            const FitResult r = fitModel(p.model, x, y);
            const double B0 = (r.p[1] - r.p[2]) / 2, mu = 1.00782503223 * 34.968852682 / (1.00782503223 + 34.968852682);
            const double rr = std::sqrt(6.62607015e-34 / (8 * 3.141592653589793 * 3.141592653589793 * 299792458.0 * 100 * B0) / (mu * 1.66053906892e-27)) * 1e10;
            check("HCl preset " + std::to_string(k) + " fits the 20 measured lines (" + std::to_string(x.size()) + ")", r.ok && x.size() == 20);
            if (k == 0) rq = rr; else { rc = rr; B0c = B0; }
        }
        std::printf("  HCl bond length from its lines: quadratic r0 = %.4f A, cubic r0 = %.4f A (B0 %.5f); CCCBDB re 1.275\n", rq, rc, B0c);
        const double hh = 10.593416 - 0.307181 / 2;   // Huber & Herzberg's Be - alpha_e / 2 (the saved WebBook page)
        check("HCl: the cubic fit's B0 is Huber & Herzberg's Be - alpha_e/2 within 1e-3", std::fabs(B0c / hh - 1) < 1e-3);
        check("HCl: r0 from the lines is longer than re (1.275 A, CCCBDB) by less than 1.5 %", rq > 1.275 && rc > 1.275 && rq < 1.275 * 1.015 && rc < 1.275 * 1.015);
    }
    // Birge-Sponer: a quadratic in v+1 of ΔG(v+½) gives back ωe + ωeye/4, −2ωexe and 3ωeye exactly (when ωeze = 0)
    int ladders = 0;   // (0.53.1: the measured rows only; the Morse model's levels follow them)
    for (int m = 0; m < molecule::count(); m++) {
        const std::string id = molecule::get(m).id;
        const molecule::vib::Anharmonic* a = molecule::vib::anharmonic(id);
        c.molecule = id;
        if (!a || a->weze != 0 || !makeTable("ladder", c, t)) continue;
        std::vector<double> x, y;
        for (std::size_t r = 0; r < t.rows(); r++) if (std::isfinite(t.cols[3].v[r]) && t.cols[4].text[r] == "measured") { x.push_back(t.cols[2].v[r]); y.push_back(t.cols[3].v[r]); }
        if (x.size() < 3) continue;
        ladders++;
        const FitResult r = fitModel(Model::Quadratic, x, y);
        check("Birge-Sponer for " + id + ": ωe + ωeye/4, −2ωexe and 3ωeye come back", r.ok && rel(r.p[0], a->we + a->weye / 4, 1e-10) && rel(r.p[1], -2 * a->wexe, 1e-9) &&
              std::fabs(r.p[2] - 3 * a->weye) < 1e-9 * a->we);
    }
    check("Birge-Sponer checked on at least one diatomic", ladders >= 1);
    // 0.53.1: the levels table goes on with the Morse model to the last bound level; HCl's Birge-Sponer fits Rank et al. 1962's measured spacings
    c.molecule = "HCl";
    const bool hclTable = makeTable("ladder", c, t);
    check("HCl: a Vibration levels table", hclTable && t.cols.size() == 6 && !t.presets.empty());
    if (hclTable && t.cols.size() == 6 && !t.presets.empty()) {
        const molecule::morse::Curve mc = molecule::morse::curve("HCl");
        check("HCl levels: v = 0 to the last bound Morse level, measured to v = 5", t.rows() == std::size_t(molecule::morse::vMax(mc) + 1) && t.cols[4].text[5] == "measured" && t.cols[4].text[6] != "measured");
        const Preset& p = t.presets[0];
        std::vector<double> x, y;
        for (std::size_t r = 0; r < t.rows(); r++) if (t.cols[4].text[r] == p.group && std::isfinite(t.cols[std::size_t(p.y)].v[r])) { x.push_back(t.cols[std::size_t(p.x)].v[r]); y.push_back(t.cols[std::size_t(p.y)].v[r]); }
        const FitResult r = fitModel(Model::Line, x, y);
        const molecule::morse::BirgeSponer b = molecule::morse::birgeSponer("HCl");
        check("HCl Birge-Sponer: the preset's line on the 5 measured spacings is morse::birgeSponer's", x.size() == 5 && r.ok && rel(r.p[0], b.intercept, 1e-9) && rel(r.p[1], b.slope, 1e-9));
    }
    // the rotational constant from the shape is h / (8 π² c I), as the calculator works it out
    makeTable("rotation", c, t);
    {
        const calc::Result r = calc::evaluate("hP / (8 π^2 c (" + numberText(t.cols[2].v[0], 17) + " u Å^2)) to cm^-1");
        check("B from the shape = h / (8π² c I) by the calculator (1e-9)", r.ok && rel(r.inUnit, t.cols[3].v[0], 1e-9));
    }
}

// 0.54.0: the Zeeman tables of the Atom Lab. Field 0: none offered (nothing in the lab changes). The e/m analysis on cadmium's 5s5p 1P1 -> 5s2 1S0 line: the fitted slope
// times 4 pi c is CODATA 2022's e/m_e = 1.758 820 008 38(55)e11 C/kg (Table XXXIII, data/calc), the slope CODATA's mu_B/hc = 46.686 447 719(15) m^-1 T^-1.
static void testZeemanTables() {
    LabContext c;
    c.lab = 1; c.Z = 48; c.N = atom::defaultN(48); c.zKind = 2;
    const std::vector<atom::zeeman::LineRef>& v = atom::zeeman::labLines(c.Z, c.N, 2);
    int k = -1;
    for (std::size_t i = 0; i < v.size(); i++) if (v[i].line.up.S2 == 0 && v[i].line.lo.S2 == 0 && v[i].line.up.L == 1 && v[i].line.lo.L == 0 && v[i].line.lo.cm[0] == 0) k = static_cast<int>(i);
    check("Zeeman tables: cadmium's 1P1 -> 1S0 line is offered", k >= 0);
    c.zLine = k;
    bool any = false;
    for (const TableRef& r : labTables(c)) any = any || r.id == "zeeman" || r.id == "zshift";
    check("Zeeman tables: none at field 0", !any);
    c.fieldT = 1;
    Table t;
    check("Zeeman tables: the components at 1 T, 3 of them", makeTable("zeeman", c, t) && t.rows() == 3);
    check("Zeeman tables: shift against field, 20 fields", makeTable("zshift", c, t) && t.rows() == 20 && !t.presets.empty());
    if (t.rows() == 20 && !t.presets.empty()) {
        const Preset& p = t.presets[0];
        std::vector<double> x, y;
        for (std::size_t r = 0; r < t.rows(); r++) { x.push_back(t.cols[std::size_t(p.x)].v[r]); y.push_back(t.cols[std::size_t(p.y)].v[r]); }
        const FitResult f = fitModel(p.model, x, y);
        check("Zeeman e/m: the slope is CODATA's mu_B/hc 46.686 447 719 m^-1 T^-1", f.ok && rel(f.p[1], 46.686447719, 1e-9));
        check("Zeeman e/m: 4 pi c x slope = CODATA's e/m_e 1.758 820 008 38e11 C/kg", f.ok && rel(4 * atom::kPi * atom::kC * f.p[1], 1.75882000838e11, 1e-9));
        check("Zeeman e/m: the line goes through 0", f.ok && std::fabs(f.p[0]) < 1e-9);
    }
}

int main() {
    testStrd();
    testLines();
    testModels();
    testChi2();
    testText();
    testFitExtras();
    testRecording();
    testTables();
    testZeemanTables();
    std::printf("analysis: %d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
