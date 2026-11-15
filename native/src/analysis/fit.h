// Curve fitting for the Data panel (0.51). No graphics: ui/data_panel.cpp draws it, tests/test_analysis.cpp checks it against the NIST Statistical Reference Datasets
// (data/strd, certified values) and textbook formulas.
//
// What it does: least squares, the way a physics lab course and the usual programs (gnuplot, scipy's curve_fit, Origin) do it.
//   - Linear models (straight line, through the origin, polynomials): one exact solve by Householder QR (no normal equations, so no squared condition number).
//   - The other models: Levenberg-Marquardt (Marquardt 1963, with More's 1978 scaling of the step), the Jacobian by central differences, each step solved by QR
//     of the augmented system [J; sqrt(lambda) D] (again no normal equations).
//   - Weights: with an uncertainty sigma_y on every row the fit is weighted (chi^2 = sum ((y - f)/sigma)^2), the parameters' errors come from the covariance
//     (J^T W J)^-1 as it is (the sigmas are taken as true), and chi^2 / dof and the chance of a chi^2 at least this big, Q(chi^2 | dof), are given.
//     Without them every row counts the same, and the covariance is scaled by the residual variance s^2 = SSR / dof: the errors then come from the scatter
//     (as NIST's certified standard deviations, scipy's curve_fit with absolute_sigma=False and gnuplot do).
//   - Uncertainties in x too: the effective variance sigma_eff^2 = sigma_y^2 + (f'(x) sigma_x)^2 (J. Orear, Am. J. Phys. 50, 912 (1982)), with f' taken at the
//     current parameters INSIDE the minimisation (not frozen between steps), which for a straight line is the maximum-likelihood fit with errors in both variables.
//     Still a first-order approximation for curves that bend much within sigma_x: the panel says so.
#pragma once

#include <functional>
#include <string>
#include <vector>

namespace analysis {

// ---- the general engine ----
using CurveFn = std::function<double(const double* p, double x)>;

struct FitResult {
    bool ok = false;
    std::string error;                   // why there is no fit, in plain words
    std::vector<std::string> notes;      // worth knowing (a weighted fit's meaning, rows left out ...)
    std::vector<double> p, err;          // the parameters and their standard errors (1 sigma)
    std::vector<double> cov;             // the covariance matrix, n x n, row by row (scaled by s^2 when not weighted)
    int n = 0, dof = 0;                  // rows used, degrees of freedom (n - parameters)
    double ssr = 0;                      // the sum of the squared (weighted) residuals: chi^2 when weighted
    double chi2 = 0, redChi2 = 0;        // chi^2 and chi^2 / dof (weighted fits; for an unweighted one these are SSR and s^2)
    double s = 0;                        // the residual standard deviation sqrt(SSR / dof) (an unweighted fit: in y's unit)
    double q = -1;                       // weighted: Q(chi^2 | dof), the chance of a chi^2 at least this big if the model and the sigmas are right; -1: not weighted
    bool weighted = false, xErrors = false;
    int iterations = 0;
    std::vector<double> resid;           // y - f(x) for every row used, in the order given
};

// Least squares of f(p, x) to (x, y). sigmaY, sigmaX: empty, or one value per row (> 0). start: the first guess (Levenberg-Marquardt starts there).
FitResult fitCurve(const CurveFn& f, std::vector<double> start, const std::vector<double>& x, const std::vector<double>& y,
                   const std::vector<double>& sigmaY = {}, const std::vector<double>& sigmaX = {}, int maxIter = 2000);
// A model linear in its parameters: f = sum_k p_k basis_k(x). Solved exactly (QR) when there are no x errors; with x errors it goes to fitCurve (the weights then depend on p).
using BasisFn = std::function<void(double x, double* out)>;
FitResult fitLinear(const BasisFn& basis, int np, const std::vector<double>& x, const std::vector<double>& y,
                    const std::vector<double>& sigmaY = {}, const std::vector<double>& sigmaX = {});

// Q(chi^2 | dof): the upper tail of the chi^2 distribution, the regularised incomplete gamma function Q(dof/2, chi^2/2)
double chi2Tail(double chi2, int dof);
double gammaQ(double a, double x);

// ---- the ready-made models the panel offers ----
enum class Model { Line, Proportional, Quadratic, Cubic, Power, ExpDecay, ExpDecayOffset, Rise, Gaussian, GaussianOffset, Lorentzian, Count };
// A parameter: its name and its unit as powers of y's and x's units (the columns' own units: a slope of yr against au is in yr/au).
struct ParamInfo { const char* name; const char* ascii; int yPow, xPow; const char* meaning; };   // ascii: the name for the calculator ("tau")
struct ModelInfo {
    Model model;
    const char* id;          // a short name kept in the settings ("line", "power" ...)
    const char* name;        // "Straight line"
    const char* formula;     // "y = a + b·x"
    std::vector<ParamInfo> params;
    bool linear;             // linear in its parameters (one exact solve)
    const char* note;        // the model's convention, if any ("x₀ = 1 of x's unit, so A has y's unit")
};
const ModelInfo& modelInfo(Model m);
Model modelById(const std::string& id, Model dflt = Model::Line);
double modelValue(Model m, const double* p, double x);
// First guesses from the data (each model by a robust linear step: logs, or the integral method for an exponential with a constant). False with a reason when the
// data cannot have this shape (a power law needs x > 0 and y of one sign ...).
bool startValues(Model m, const std::vector<double>& x, const std::vector<double>& y, std::vector<double>& p, std::string& why);
// The fit of a model: start values, then fitLinear or fitCurve.
// held (0.51.1): empty, or one value per parameter: a number holds that parameter at it (it is not fitted: Kepler's n held at 3/2), NaN leaves it free. A held
// parameter's error and its covariances are 0, and the degrees of freedom count only the free ones.
FitResult fitModel(Model m, const std::vector<double>& x, const std::vector<double>& y, const std::vector<double>& sigmaY = {}, const std::vector<double>& sigmaX = {},
                   const std::vector<double>& held = {});
// A peak's width enters only as its square (Gaussian σ) or its absolute value (Lorentzian Γ), so Levenberg-Marquardt may end on a negative width: this makes it
// positive and flips the signs of its covariances with the other parameters to match (0.51.1; fitModel does it). Other models: nothing.
void positiveWidth(Model m, FitResult& r);
// The correlation matrix of the parameters, n x n row by row: cov_ij / (err_i err_j); NaN where an error is 0 or unknown (a held parameter).
std::vector<double> correlation(const FitResult& r);
// A weighted fit with its errors scaled by sqrt(chi^2 / dof) (the Birge ratio; what curve_fit's absolute_sigma=False does): the uncertainties are then taken
// as right only relative to each other. The fit itself does not change. Unweighted fits and dof <= 0: returned as they are.
FitResult scaledErrors(const FitResult& r);
// The 1-sigma uncertainty of the fitted curve at x, sqrt(g^T C g) with g = df/dp (the confidence band)
double modelBand(Model m, const std::vector<double>& p, const std::vector<double>& cov, double x);

}  // namespace analysis
