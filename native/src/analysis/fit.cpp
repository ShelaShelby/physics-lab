// Curve fitting (0.51): see fit.h.
#include "analysis/fit.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numeric>

namespace analysis {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// Least squares min |A x - b| by Householder QR. A: m rows of n (row by row). The columns are scaled to unit length first (the answer does not change; the
// rank test and the triangular solve are then about the shape of the problem, not its units). cov (optional): (A^T A)^-1. False: the columns are (nearly) dependent.
bool qrSolve(std::vector<double> A, std::vector<double> b, int m, int n, std::vector<double>& x, std::vector<double>* cov) {
    if (m < n || n < 1) return false;
    std::vector<double> cs(static_cast<std::size_t>(n), 0.0);
    for (int j = 0; j < n; j++) {
        double s = 0;
        for (int i = 0; i < m; i++) s += A[std::size_t(i) * n + j] * A[std::size_t(i) * n + j];
        s = std::sqrt(s);
        if (!(s > 0) || !std::isfinite(s)) return false;     // a column of zeros: that parameter does nothing
        cs[std::size_t(j)] = s;
        for (int i = 0; i < m; i++) A[std::size_t(i) * n + j] /= s;
    }
    std::vector<double> diag(static_cast<std::size_t>(n));
    for (int k = 0; k < n; k++) {
        double norm = 0;
        for (int i = k; i < m; i++) norm += A[std::size_t(i) * n + k] * A[std::size_t(i) * n + k];
        norm = std::sqrt(norm);
        if (norm == 0) return false;
        const double akk = A[std::size_t(k) * n + k];
        const double alpha = akk > 0 ? -norm : norm;          // the sign that avoids cancellation
        // v = a_k - alpha e_k, stored in place of column k (rows k..m-1)
        A[std::size_t(k) * n + k] = akk - alpha;
        double vv = 0;
        for (int i = k; i < m; i++) vv += A[std::size_t(i) * n + k] * A[std::size_t(i) * n + k];
        if (vv > 0) {
            for (int j = k + 1; j < n; j++) {
                double d = 0;
                for (int i = k; i < m; i++) d += A[std::size_t(i) * n + k] * A[std::size_t(i) * n + j];
                const double f = 2 * d / vv;
                for (int i = k; i < m; i++) A[std::size_t(i) * n + j] -= f * A[std::size_t(i) * n + k];
            }
            double d = 0;
            for (int i = k; i < m; i++) d += A[std::size_t(i) * n + k] * b[std::size_t(i)];
            const double f = 2 * d / vv;
            for (int i = k; i < m; i++) b[std::size_t(i)] -= f * A[std::size_t(i) * n + k];
        }
        diag[std::size_t(k)] = alpha;
    }
    double big = 0;
    for (double d : diag) big = std::max(big, std::fabs(d));
    for (double d : diag) if (!(std::fabs(d) > 1e-13 * big)) return false;   // rank deficient: the data cannot tell these parameters apart
    // R: diag on the diagonal, A above it
    const auto R = [&](int i, int j) { return i == j ? diag[std::size_t(i)] : A[std::size_t(i) * n + j]; };
    x.assign(std::size_t(n), 0.0);
    for (int i = n - 1; i >= 0; i--) {
        double s = b[std::size_t(i)];
        for (int j = i + 1; j < n; j++) s -= R(i, j) * x[std::size_t(j)];
        x[std::size_t(i)] = s / R(i, i);
    }
    if (cov) {
        // R^-1 (upper triangular), then (A^T A)^-1 = R^-1 R^-T, then undo the column scaling
        std::vector<double> Ri(std::size_t(n) * n, 0.0);
        for (int j = 0; j < n; j++) {
            Ri[std::size_t(j) * n + j] = 1 / R(j, j);
            for (int i = j - 1; i >= 0; i--) {
                double s = 0;
                for (int k = i + 1; k <= j; k++) s += R(i, k) * Ri[std::size_t(k) * n + j];
                Ri[std::size_t(i) * n + j] = -s / R(i, i);
            }
        }
        cov->assign(std::size_t(n) * n, 0.0);
        for (int i = 0; i < n; i++)
            for (int j = 0; j < n; j++) {
                double s = 0;
                for (int k = std::max(i, j); k < n; k++) s += Ri[std::size_t(i) * n + k] * Ri[std::size_t(j) * n + k];
                (*cov)[std::size_t(i) * n + j] = s / (cs[std::size_t(i)] * cs[std::size_t(j)]);
            }
    }
    for (int j = 0; j < n; j++) x[std::size_t(j)] /= cs[std::size_t(j)];
    return true;
}

bool allPositive(const std::vector<double>& s, std::size_t n) {
    if (s.size() != n) return false;
    for (double v : s) if (!(v > 0) || !std::isfinite(v)) return false;
    return true;
}
bool anyPositive(const std::vector<double>& s, std::size_t n) {
    if (s.size() != n) return false;
    for (double v : s) if (v > 0 && std::isfinite(v)) return true;
    return false;
}

// The statistics every fit ends with. ssr: the (weighted) sum of squares at the answer; cov0: (J^T W J)^-1.
void finish(FitResult& r, double ssr, const std::vector<double>& cov0, int np) {
    r.ssr = ssr;
    r.dof = r.n - np;
    r.cov = cov0;
    r.err.assign(std::size_t(np), kNaN);
    if (r.dof > 0) {
        r.s = std::sqrt(ssr / r.dof);
        r.redChi2 = ssr / r.dof;
    } else {
        r.s = kNaN;
        r.redChi2 = kNaN;
    }
    r.chi2 = ssr;
    if (!r.weighted) {
        if (r.dof > 0) for (double& c : r.cov) c *= ssr / r.dof;
        else { for (double& c : r.cov) c = kNaN; r.notes.push_back("As many rows as parameters: the curve goes through every point and the data say nothing about the errors."); }
    } else {
        r.q = r.dof > 0 ? chi2Tail(ssr, r.dof) : -1;
    }
    for (int i = 0; i < np; i++) r.err[std::size_t(i)] = std::sqrt(std::max(0.0, r.cov[std::size_t(i) * np + i]));
    if (!r.weighted && r.dof <= 0) for (double& e : r.err) e = kNaN;
    r.ok = true;
}

}  // namespace

// ---- the chi^2 tail: the regularised upper incomplete gamma function, by its series (x < a + 1) or its continued fraction (Lentz) ----
double gammaQ(double a, double x) {
    if (!(a > 0) || !(x >= 0)) return kNaN;
    if (x == 0) return 1;
    const double lg = std::lgamma(a);
    if (x < a + 1) {
        double ap = a, sum = 1 / a, del = sum;
        for (int n = 0; n < 1000; n++) {
            ap += 1;
            del *= x / ap;
            sum += del;
            if (std::fabs(del) < std::fabs(sum) * 1e-16) break;
        }
        return std::max(0.0, 1 - sum * std::exp(-x + a * std::log(x) - lg));
    }
    const double tiny = 1e-300;
    double b = x + 1 - a, c = 1 / tiny, d = 1 / b, h = d;
    for (int i = 1; i < 1000; i++) {
        const double an = -i * (i - a);
        b += 2;
        d = an * d + b;
        if (std::fabs(d) < tiny) d = tiny;
        c = b + an / c;
        if (std::fabs(c) < tiny) c = tiny;
        d = 1 / d;
        const double del = d * c;
        h *= del;
        if (std::fabs(del - 1) < 1e-16) break;
    }
    return std::exp(-x + a * std::log(x) - lg) * h;
}
double chi2Tail(double chi2, int dof) { return dof > 0 ? gammaQ(0.5 * dof, 0.5 * chi2) : kNaN; }

// ---- the general engine ----
FitResult fitCurve(const CurveFn& f, std::vector<double> p, const std::vector<double>& x, const std::vector<double>& y, const std::vector<double>& sigmaY,
                   const std::vector<double>& sigmaX, int maxIter) {
    FitResult r;
    const std::size_t m = x.size();
    const int np = static_cast<int>(p.size());
    r.n = static_cast<int>(m);
    if (y.size() != m) { r.error = "x and y have different numbers of rows."; return r; }
    if (static_cast<int>(m) < np) { r.error = "Too few rows: this model has " + std::to_string(np) + " parameters and needs at least as many points."; return r; }
    const bool wy = allPositive(sigmaY, m), wx = anyPositive(sigmaX, m);
    r.weighted = wy || wx;
    r.xErrors = wx;
    if (wx && !wy && !sigmaY.empty()) { r.error = "Some rows have no y uncertainty while x has one: give every row a y uncertainty, or none."; return r; }
    double xlo = 1e300, xhi = -1e300;
    for (double v : x) { xlo = std::min(xlo, v); xhi = std::max(xhi, v); }
    const double xspan = std::max(xhi - xlo, 1e-300);
    // the residuals, divided by the (effective) sigma
    const auto resid = [&](const std::vector<double>& q, std::vector<double>& out) {
        out.resize(m);
        for (std::size_t i = 0; i < m; i++) {
            const double fi = f(q.data(), x[i]);
            double w = 1;
            if (r.weighted) {
                double v = wy ? sigmaY[i] * sigmaY[i] : 0;
                if (wx && sigmaX[i] > 0) {
                    const double h = 1e-6 * (std::fabs(x[i]) + 1e-3 * xspan);
                    const double d = (f(q.data(), x[i] + h) - f(q.data(), x[i] - h)) / (2 * h);
                    v += d * d * sigmaX[i] * sigmaX[i];
                }
                if (!(v > 0)) return false;
                w = std::sqrt(v);
            }
            out[i] = (y[i] - fi) / w;
            if (!std::isfinite(out[i])) return false;
        }
        return true;
    };
    const auto sumSq = [](const std::vector<double>& v) { double s = 0; for (double e : v) s += e * e; return s; };
    std::vector<double> typ(static_cast<std::size_t>(np));
    for (int j = 0; j < np; j++) typ[std::size_t(j)] = p[std::size_t(j)] != 0 ? std::fabs(p[std::size_t(j)]) : 1.0;
    const auto jacobian = [&](const std::vector<double>& q, std::vector<double>& J) {   // J = d f/d p (weighted), m x np: minus the derivative of the residual
        J.assign(m * std::size_t(np), 0.0);
        std::vector<double> qp = q, rp, rm;
        for (int j = 0; j < np; j++) {
            const double h = 6e-6 * std::max(std::fabs(q[std::size_t(j)]), 1e-6 * typ[std::size_t(j)]);
            qp[std::size_t(j)] = q[std::size_t(j)] + h;
            const bool a = resid(qp, rp);
            qp[std::size_t(j)] = q[std::size_t(j)] - h;
            const bool b = resid(qp, rm);
            qp[std::size_t(j)] = q[std::size_t(j)];
            if (!a || !b) return false;
            for (std::size_t i = 0; i < m; i++) J[i * np + j] = -(rp[i] - rm[i]) / (2 * h);
        }
        return true;
    };

    std::vector<double> res;
    if (!resid(p, res)) { r.error = "The model cannot be worked out at the starting values (a division by zero or an overflow)."; return r; }
    double S = sumSq(res);
    std::vector<double> D(std::size_t(np), 0.0), J, A, b, delta, pn, rn;
    double lambda = 1e-3;
    bool converged = false;
    int it = 0;
    for (; it < maxIter && !converged; it++) {
        if (S == 0) { converged = true; break; }
        if (!jacobian(p, J)) { r.error = "The model cannot be worked out near these parameters (a division by zero or an overflow)."; return r; }
        for (int j = 0; j < np; j++) {
            double s = 0;
            for (std::size_t i = 0; i < m; i++) s += J[i * np + j] * J[i * np + j];
            D[std::size_t(j)] = std::max(D[std::size_t(j)], std::sqrt(s));
            if (D[std::size_t(j)] == 0) D[std::size_t(j)] = 1;
        }
        for (;;) {
            const std::size_t rows = m + std::size_t(np);
            A.assign(rows * np, 0.0);
            b.assign(rows, 0.0);
            for (std::size_t i = 0; i < m; i++) { for (int j = 0; j < np; j++) A[i * np + j] = J[i * np + j]; b[i] = res[i]; }
            const double sl = std::sqrt(lambda);
            for (int j = 0; j < np; j++) A[(m + std::size_t(j)) * np + j] = sl * D[std::size_t(j)];
            bool stepOk = qrSolve(A, b, static_cast<int>(rows), np, delta, nullptr);
            double Sn = 0;
            if (stepOk) {
                pn = p;
                for (int j = 0; j < np; j++) pn[std::size_t(j)] += delta[std::size_t(j)];
                stepOk = resid(pn, rn);
                if (stepOk) Sn = sumSq(rn);
            }
            if (stepOk && Sn <= S) {
                double rel = 0;
                for (int j = 0; j < np; j++) rel = std::max(rel, std::fabs(delta[std::size_t(j)]) / (std::fabs(pn[std::size_t(j)]) + 1e-300));
                const double drop = S > 0 ? (S - Sn) / S : 0;
                p = pn; res = rn; S = Sn;
                lambda = std::max(lambda * 0.2, 1e-30);
                if (drop < 1e-15 || rel < 1e-13) converged = true;
                break;
            }
            lambda *= 8;
            if (lambda > 1e25) { converged = true; break; }   // no step lowers the sum: this is the minimum to the computer's precision
        }
    }
    r.iterations = it;
    if (!converged) { r.error = "The fit did not settle in " + std::to_string(maxIter) + " steps: try another model, or fewer parameters."; r.p = p; return r; }
    // the covariance from the Jacobian at the answer
    if (!jacobian(p, J)) { r.error = "The model cannot be worked out at the answer."; return r; }
    std::vector<double> dummy, cov0;
    if (!qrSolve(J, res, static_cast<int>(m), np, dummy, &cov0)) {
        r.error = "The data do not fix every parameter (two of them can trade off against each other exactly): use a model with fewer parameters, or more points.";
        r.p = p;
        return r;
    }
    r.p = p;
    r.resid.resize(m);
    for (std::size_t i = 0; i < m; i++) r.resid[i] = y[i] - f(p.data(), x[i]);
    finish(r, S, cov0, np);
    if (wx) r.notes.push_back("x uncertainties are carried by the effective variance σ² = σy² + (f′(x) σx)² (Orear 1982): exact for a straight line, a first-order approximation for a curve.");
    return r;
}

FitResult fitLinear(const BasisFn& basis, int np, const std::vector<double>& x, const std::vector<double>& y, const std::vector<double>& sigmaY, const std::vector<double>& sigmaX) {
    const std::size_t m = x.size();
    FitResult r;
    r.n = static_cast<int>(m);
    if (y.size() != m) { r.error = "x and y have different numbers of rows."; return r; }
    if (static_cast<int>(m) < np) { r.error = "Too few rows: this model has " + std::to_string(np) + " parameters and needs at least as many points."; return r; }
    const bool wy = allPositive(sigmaY, m);
    std::vector<double> A(m * std::size_t(np)), b(m), row(static_cast<std::size_t>(np)), p, cov0;
    for (std::size_t i = 0; i < m; i++) {
        basis(x[i], row.data());
        const double w = wy ? sigmaY[i] : 1;
        for (int j = 0; j < np; j++) A[i * np + j] = row[std::size_t(j)] / w;
        b[i] = y[i] / w;
    }
    if (!qrSolve(A, b, static_cast<int>(m), np, p, &cov0)) { r.error = "The data do not fix every parameter (the x values are too few or all the same)."; return r; }
    if (anyPositive(sigmaX, m)) {   // the weights then depend on the slope: the general engine, from this answer
        const CurveFn f = [basis, np](const double* q, double xx) {
            double rowv[8];
            basis(xx, rowv);
            double s = 0;
            for (int j = 0; j < np; j++) s += q[j] * rowv[j];
            return s;
        };
        return fitCurve(f, p, x, y, sigmaY, sigmaX);
    }
    r.weighted = wy;
    r.p = p;
    r.resid.resize(m);
    double S = 0;
    for (std::size_t i = 0; i < m; i++) {
        basis(x[i], row.data());
        double fi = 0;
        for (int j = 0; j < np; j++) fi += p[std::size_t(j)] * row[std::size_t(j)];
        r.resid[i] = y[i] - fi;
        const double w = wy ? sigmaY[i] : 1;
        S += (r.resid[i] / w) * (r.resid[i] / w);
    }
    r.iterations = 1;
    finish(r, S, cov0, np);
    return r;
}

// ---- the ready-made models ----
const ModelInfo& modelInfo(Model m) {
    static const std::vector<ModelInfo> all = {
        {Model::Line, "line", "Straight line", "y = a + b·x", {{"a", "a", 1, 0, "y where x = 0"}, {"b", "b", 1, -1, "slope"}}, true, ""},
        {Model::Proportional, "prop", "Through the origin", "y = b·x", {{"b", "b", 1, -1, "slope"}}, true, ""},
        {Model::Quadratic, "quad", "Quadratic", "y = a + b·x + c·x²", {{"a", "a", 1, 0, ""}, {"b", "b", 1, -1, ""}, {"c", "c", 1, -2, ""}}, true, ""},
        {Model::Cubic, "cubic", "Cubic", "y = a + b·x + c·x² + d·x³", {{"a", "a", 1, 0, ""}, {"b", "b", 1, -1, ""}, {"c", "c", 1, -2, ""}, {"d", "d", 1, -3, ""}}, true, ""},
        {Model::Power, "power", "Power law", "y = A·xⁿ", {{"A", "Apow", 1, 0, "y at x = 1"}, {"n", "n", 0, 0, "the power"}}, false,
         "x is taken in its column's unit (x / 1 unit), so A has y's unit: A is y where x = 1 of that unit."},
        {Model::ExpDecay, "exp", "Exponential", "y = A·e^(−x/τ)", {{"A", "Aexp", 1, 0, "y at x = 0"}, {"τ", "tau", 0, 1, "e-folding length (negative: growth)"}}, false, ""},
        {Model::ExpDecayOffset, "expc", "Exponential + constant", "y = A·e^(−x/τ) + C", {{"A", "Aexp", 1, 0, ""}, {"τ", "tau", 0, 1, ""}, {"C", "C0", 1, 0, "the level it goes to"}}, false, ""},
        {Model::Rise, "rise", "Rise to a limit", "y = A·(1 − e^(−x/τ))", {{"A", "Alim", 1, 0, "the limit"}, {"τ", "tau", 0, 1, ""}}, false, ""},
        {Model::Gaussian, "gauss", "Gaussian peak", "y = A·e^(−(x−μ)²/2σ²)", {{"A", "Apk", 1, 0, "height"}, {"μ", "mu", 0, 1, "centre"}, {"σ", "sig", 0, 1, "width (FWHM = 2.3548 σ)"}}, false, ""},
        {Model::GaussianOffset, "gaussc", "Gaussian peak + constant", "y = A·e^(−(x−μ)²/2σ²) + C", {{"A", "Apk", 1, 0, "height"}, {"μ", "mu", 0, 1, "centre"}, {"σ", "sig", 0, 1, "width (FWHM = 2.3548 σ)"}, {"C", "C0", 1, 0, "background"}}, false, ""},
        {Model::Lorentzian, "lorentz", "Lorentzian peak + constant", "y = A·(Γ/2)² / ((x−x₀)² + (Γ/2)²) + C", {{"A", "Apk", 1, 0, "height"}, {"x₀", "x0", 0, 1, "centre"}, {"Γ", "Gam", 0, 1, "full width at half maximum"}, {"C", "C0", 1, 0, "background"}}, false, ""},
    };
    return all[static_cast<std::size_t>(m)];
}
Model modelById(const std::string& id, Model dflt) {
    for (int i = 0; i < static_cast<int>(Model::Count); i++) if (id == modelInfo(static_cast<Model>(i)).id) return static_cast<Model>(i);
    return dflt;
}

double modelValue(Model m, const double* p, double x) {
    switch (m) {
    case Model::Line: return p[0] + p[1] * x;
    case Model::Proportional: return p[0] * x;
    case Model::Quadratic: return p[0] + x * (p[1] + x * p[2]);
    case Model::Cubic: return p[0] + x * (p[1] + x * (p[2] + x * p[3]));
    case Model::Power: return p[0] * std::pow(x, p[1]);
    case Model::ExpDecay: return p[0] * std::exp(-x / p[1]);
    case Model::ExpDecayOffset: return p[0] * std::exp(-x / p[1]) + p[2];
    case Model::Rise: return p[0] * -std::expm1(-x / p[1]);
    case Model::Gaussian: { const double u = (x - p[1]) / p[2]; return p[0] * std::exp(-0.5 * u * u); }
    case Model::GaussianOffset: { const double u = (x - p[1]) / p[2]; return p[0] * std::exp(-0.5 * u * u) + p[3]; }
    case Model::Lorentzian: { const double g = 0.5 * p[2], d = x - p[1]; return p[0] * g * g / (d * d + g * g) + p[3]; }
    case Model::Count: break;
    }
    return kNaN;
}

namespace {

// a plain straight-line fit (no weights), for start values: y = a + b x
bool line2(const std::vector<double>& x, const std::vector<double>& y, double& a, double& b) {
    const std::size_t n = x.size();
    if (n < 2) return false;
    double mx = 0, my = 0;
    for (std::size_t i = 0; i < n; i++) { mx += x[i]; my += y[i]; }
    mx /= n; my /= n;
    double sxx = 0, sxy = 0;
    for (std::size_t i = 0; i < n; i++) { sxx += (x[i] - mx) * (x[i] - mx); sxy += (x[i] - mx) * (y[i] - my); }
    if (!(sxx > 0)) return false;
    b = sxy / sxx;
    a = my - b * mx;
    return std::isfinite(a) && std::isfinite(b);
}
std::vector<std::size_t> byX(const std::vector<double>& x) {
    std::vector<std::size_t> o(x.size());
    std::iota(o.begin(), o.end(), 0);
    std::stable_sort(o.begin(), o.end(), [&](std::size_t a, std::size_t b) { return x[a] < x[b]; });
    return o;
}
// The integral method for y = C + B e^(-x/tau) (J. Jacquelin, "Régressions et équations intégrales" (2009)): dy/dx = -(y - C)/tau, so y = const + (C/tau) x - (1/tau) S(x),
// S the running integral of y; one linear fit of y on (1, x, S) gives tau, then one on (1, e^(-x/tau)) gives C and B. No guess needed.
bool expIntegral(const std::vector<double>& x, const std::vector<double>& y, double& tau, double& C, double& B) {
    const std::vector<std::size_t> o = byX(x);
    const std::size_t n = o.size();
    if (n < 3) return false;
    std::vector<double> S(n, 0.0);
    for (std::size_t k = 1; k < n; k++) S[k] = S[k - 1] + 0.5 * (y[o[k]] + y[o[k - 1]]) * (x[o[k]] - x[o[k - 1]]);
    std::vector<double> A(n * 3), b(n), q;
    for (std::size_t k = 0; k < n; k++) { A[k * 3] = 1; A[k * 3 + 1] = x[o[k]] - x[o[0]]; A[k * 3 + 2] = S[k]; b[k] = y[o[k]]; }
    if (!qrSolve(A, b, static_cast<int>(n), 3, q, nullptr) || q[2] == 0) return false;
    tau = -1 / q[2];
    if (!std::isfinite(tau)) return false;
    std::vector<double> A2(n * 2), q2;
    for (std::size_t k = 0; k < n; k++) { A2[k * 2] = 1; A2[k * 2 + 1] = std::exp(-(x[o[k]] - x[o[0]]) / tau); b[k] = y[o[k]]; if (!std::isfinite(A2[k * 2 + 1])) return false; }
    if (!qrSolve(A2, b, static_cast<int>(n), 2, q2, nullptr)) return false;
    C = q2[0];
    B = q2[1] * std::exp(x[o[0]] / tau);      // back to e^(-x/tau) from e^(-(x - x0)/tau)
    return std::isfinite(C) && std::isfinite(B);
}
// a peak's place, height and full width at half its height (above `base`)
void peakGuess(const std::vector<double>& x, const std::vector<double>& y, double base, double& height, double& centre, double& fwhm) {
    const std::vector<std::size_t> o = byX(x);
    std::size_t k = 0;
    for (std::size_t i = 1; i < o.size(); i++) if (std::fabs(y[o[i]] - base) > std::fabs(y[o[k]] - base)) k = i;
    height = y[o[k]] - base;
    centre = x[o[k]];
    const double half = std::fabs(height) / 2;
    double left = kNaN, right = kNaN;
    for (std::size_t i = k; i-- > 0;) if (std::fabs(y[o[i]] - base) < half) { const double a = std::fabs(y[o[i]] - base), b = std::fabs(y[o[i + 1]] - base); left = x[o[i]] + (half - a) / (b - a) * (x[o[i + 1]] - x[o[i]]); break; }
    for (std::size_t i = k + 1; i < o.size(); i++) if (std::fabs(y[o[i]] - base) < half) { const double a = std::fabs(y[o[i - 1]] - base), b = std::fabs(y[o[i]] - base); right = x[o[i - 1]] + (a - half) / (a - b) * (x[o[i]] - x[o[i - 1]]); break; }
    const double span = x[o.back()] - x[o.front()];
    if (std::isfinite(left) && std::isfinite(right)) fwhm = right - left;
    else if (std::isfinite(left)) fwhm = 2 * (centre - left);
    else if (std::isfinite(right)) fwhm = 2 * (right - centre);
    else fwhm = span / 4;
    if (!(fwhm > 0)) fwhm = span > 0 ? span / 4 : 1;
}

}  // namespace

bool startValues(Model m, const std::vector<double>& x, const std::vector<double>& y, std::vector<double>& p, std::string& why) {
    const std::size_t n = x.size();
    p.assign(modelInfo(m).params.size(), 0.0);
    switch (m) {
    case Model::Line: case Model::Proportional: case Model::Quadratic: case Model::Cubic: return true;   // linear: no start needed
    case Model::Power: {
        int pos = 0, neg = 0;
        for (std::size_t i = 0; i < n; i++) { if (y[i] > 0) pos++; else if (y[i] < 0) neg++; }
        const double sg = pos >= neg ? 1 : -1;
        std::vector<double> lx, ly;
        for (std::size_t i = 0; i < n; i++) if (x[i] > 0 && y[i] * sg > 0) { lx.push_back(std::log(x[i])); ly.push_back(std::log(y[i] * sg)); }
        double a, b;
        if (lx.size() < 2 || !line2(lx, ly, a, b)) { why = "A power law needs at least two points with x > 0 and y of one sign."; return false; }
        p = {sg * std::exp(a), b};
        return true;
    }
    case Model::ExpDecay: {
        int pos = 0, neg = 0;
        for (std::size_t i = 0; i < n; i++) { if (y[i] > 0) pos++; else if (y[i] < 0) neg++; }
        const double sg = pos >= neg ? 1 : -1;
        std::vector<double> lx, ly;
        for (std::size_t i = 0; i < n; i++) if (y[i] * sg > 0) { lx.push_back(x[i]); ly.push_back(std::log(y[i] * sg)); }
        double a, b;
        if (lx.size() < 2 || !line2(lx, ly, a, b)) { why = "An exponential needs at least two points with y of one sign."; return false; }
        if (b == 0) { why = "y does not change with x: there is no exponential to fit."; return false; }
        p = {sg * std::exp(a), -1 / b};
        return true;
    }
    case Model::ExpDecayOffset: case Model::Rise: {
        double tau = 0, C = 0, B = 0;
        if (!expIntegral(x, y, tau, C, B)) { why = "These points do not look like an exponential (the integral method found no time constant)."; return false; }
        if (m == Model::Rise) p = {C, tau};
        else p = {B, tau, C};
        return true;
    }
    case Model::Gaussian: case Model::GaussianOffset: case Model::Lorentzian: {
        if (n < 3) { why = "A peak needs at least three points."; return false; }
        double base = 0;
        if (m != Model::Gaussian) {                       // the background: the mean of the outer tenth on each side
            const std::vector<std::size_t> o = byX(x);
            const std::size_t k = std::max<std::size_t>(1, n / 10);
            double s = 0;
            for (std::size_t i = 0; i < k; i++) s += y[o[i]] + y[o[n - 1 - i]];
            base = s / (2.0 * k);
        }
        double h, c, w;
        peakGuess(x, y, base, h, c, w);
        if (m == Model::Lorentzian) p = {h, c, w, base};
        else if (m == Model::Gaussian) p = {h, c, w / 2.3548200450309493};
        else p = {h, c, w / 2.3548200450309493, base};
        return true;
    }
    case Model::Count: break;
    }
    why = "Unknown model.";
    return false;
}

FitResult fitModel(Model m, const std::vector<double>& x, const std::vector<double>& y, const std::vector<double>& sigmaY, const std::vector<double>& sigmaX,
                   const std::vector<double>& held) {
    const ModelInfo& mi = modelInfo(m);
    const int np = static_cast<int>(mi.params.size());
    if (y.size() != x.size()) { FitResult e; e.n = static_cast<int>(x.size()); e.error = "x and y have different numbers of rows."; return e; }
    std::vector<int> freeIdx;
    for (int j = 0; j < np; j++) if (!(held.size() == std::size_t(np) && std::isfinite(held[std::size_t(j)]))) freeIdx.push_back(j);
    if (freeIdx.empty()) { FitResult e; e.n = static_cast<int>(x.size()); e.error = "Every parameter is held: there is nothing to fit."; return e; }
    if (static_cast<int>(freeIdx.size()) < np) {
        // some parameters held: the free ones by Levenberg-Marquardt, from the all-free answer (or the start values) with the held ones put in
        if (m == Model::Power) for (double v : x) if (!(v > 0)) { FitResult e; e.n = static_cast<int>(x.size()); e.error = "A power law with a non-whole power needs every x > 0."; return e; }
        std::vector<double> full;
        if (mi.linear) {
            const FitResult free = fitModel(m, x, y, sigmaY, {});
            full = free.ok ? free.p : std::vector<double>(std::size_t(np), 0.0);
        } else {
            std::string why;
            if (!startValues(m, x, y, full, why)) { FitResult e; e.n = static_cast<int>(x.size()); e.error = why; return e; }
        }
        for (int j = 0; j < np; j++) if (std::find(freeIdx.begin(), freeIdx.end(), j) == freeIdx.end()) full[std::size_t(j)] = held[std::size_t(j)];
        std::vector<double> q;
        for (int j : freeIdx) q.push_back(full[std::size_t(j)]);
        const std::vector<double> base = full;
        const CurveFn f = [m, base, freeIdx](const double* qq, double xx) {
            double pp[8];
            for (std::size_t j = 0; j < base.size() && j < 8; j++) pp[j] = base[j];
            for (std::size_t k = 0; k < freeIdx.size(); k++) pp[freeIdx[k]] = qq[k];
            return modelValue(m, pp, xx);
        };
        FitResult r = fitCurve(f, q, x, y, sigmaY, sigmaX);
        if (!r.ok) return r;
        const std::size_t nf = freeIdx.size();
        std::vector<double> p = base, err(std::size_t(np), 0.0), cov(std::size_t(np) * np, 0.0);
        for (std::size_t a = 0; a < nf; a++) {
            p[std::size_t(freeIdx[a])] = r.p[a];
            err[std::size_t(freeIdx[a])] = r.err[a];
            for (std::size_t b = 0; b < nf; b++) cov[std::size_t(freeIdx[a]) * np + std::size_t(freeIdx[b])] = r.cov[a * nf + b];
        }
        r.p = p; r.err = err; r.cov = cov;
        for (int j = 0; j < np; j++)
            if (std::find(freeIdx.begin(), freeIdx.end(), j) == freeIdx.end()) {
                char b[160];
                std::snprintf(b, sizeof b, "%s is held at %.10g (not fitted): its error is 0 and the degrees of freedom count only the free parameters.", mi.params[std::size_t(j)].name, held[std::size_t(j)]);
                r.notes.push_back(b);
            }
        positiveWidth(m, r);
        return r;
    }
    if (mi.linear) {
        BasisFn basis;
        if (m == Model::Proportional) basis = [](double xx, double* o) { o[0] = xx; };
        else basis = [np](double xx, double* o) { double v = 1; for (int j = 0; j < np; j++) { o[j] = v; v *= xx; } };
        return fitLinear(basis, np, x, y, sigmaY, sigmaX);
    }
    std::vector<double> p;
    std::string why;
    FitResult r;
    if (!startValues(m, x, y, p, why)) { r.error = why; return r; }
    if (m == Model::Power) for (double v : x) if (!(v > 0)) { r.error = "A power law with a non-whole power needs every x > 0."; return r; }
    const CurveFn f = [m](const double* q, double xx) { return modelValue(m, q, xx); };
    r = fitCurve(f, p, x, y, sigmaY, sigmaX);
    positiveWidth(m, r);
    return r;
}

void positiveWidth(Model m, FitResult& r) {
    if (!r.ok || !(m == Model::Gaussian || m == Model::GaussianOffset || m == Model::Lorentzian) || r.p.size() < 3 || !(r.p[2] < 0)) return;
    const std::size_t np = r.p.size(), w = 2;
    r.p[w] = -r.p[w];                          // the model is even in the width: the same curve, and d f/d width changes sign, so its covariances with the others do too
    if (r.cov.size() == np * np)
        for (std::size_t j = 0; j < np; j++) if (j != w) { r.cov[w * np + j] = -r.cov[w * np + j]; r.cov[j * np + w] = -r.cov[j * np + w]; }
}

std::vector<double> correlation(const FitResult& r) {
    const std::size_t np = r.p.size();
    std::vector<double> c(np * np, kNaN);
    if (r.cov.size() != np * np || r.err.size() != np) return c;
    for (std::size_t i = 0; i < np; i++)
        for (std::size_t j = 0; j < np; j++)
            if (r.err[i] > 0 && r.err[j] > 0 && std::isfinite(r.err[i]) && std::isfinite(r.err[j])) c[i * np + j] = i == j ? 1.0 : r.cov[i * np + j] / (r.err[i] * r.err[j]);
    return c;
}

FitResult scaledErrors(const FitResult& r) {
    FitResult s = r;
    if (!r.ok || !r.weighted || r.dof <= 0 || !std::isfinite(r.redChi2)) return s;
    const double k = r.redChi2;
    for (double& c : s.cov) c *= k;
    for (double& e : s.err) e *= std::sqrt(k);
    s.notes.push_back("Errors scaled by √(χ²/dof) = " + std::to_string(std::sqrt(k)).substr(0, 6) + ": the uncertainties are taken as right only relative to each other (the fit itself is the same).");
    return s;
}

double modelBand(Model m, const std::vector<double>& p, const std::vector<double>& cov, double x) {
    const std::size_t np = p.size();
    if (cov.size() != np * np) return kNaN;
    std::vector<double> g(np), q = p;
    for (std::size_t j = 0; j < np; j++) {
        const double h = 1e-6 * (std::fabs(p[j]) + 1e-12);
        q[j] = p[j] + h;
        const double a = modelValue(m, q.data(), x);
        q[j] = p[j] - h;
        const double b = modelValue(m, q.data(), x);
        q[j] = p[j];
        g[j] = (a - b) / (2 * h);
    }
    double v = 0;
    for (std::size_t i = 0; i < np; i++) for (std::size_t j = 0; j < np; j++) v += g[i] * cov[i * np + j] * g[j];
    return std::sqrt(std::max(0.0, v));
}

}  // namespace analysis
