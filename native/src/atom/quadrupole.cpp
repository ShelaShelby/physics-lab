// See quadrupole.h: measured electric quadrupole moments (IAEA) and the shape the lab draws from them.
#include "atom/quadrupole.h"

#include <algorithm>
#include <cmath>

#include "atom/atom_core.h"
#include "atom/radii.h"
#include "orbit/format.h"

namespace atom {

#include "atom/quadrupole_data.inc"

static const int kQuadCount = int(sizeof(kQuadRows) / sizeof(kQuadRows[0]));

const QuadRow* quadRows(int& count) { count = kQuadCount; return kQuadRows; }
const QuadRow* quadRow(int Z, int N) {
    const int A = Z + N;
    const QuadRow* e = kQuadRows + kQuadCount;
    const QuadRow* it = std::lower_bound(kQuadRows, e, std::pair<int, int>{Z, A}, [](const QuadRow& r, const std::pair<int, int>& k) { return r.Z < k.first || (r.Z == k.first && r.A < k.second); });
    return it != e && it->Z == Z && it->A == A ? it : nullptr;
}
void quadFileCounts(int& fileRows, int& isomerRows) { fileRows = kQuadFileRows; isomerRows = kQuadIsomerRows; }

double intrinsicQ0(double Qs, int spin2) {
    if (spin2 < 2) return std::nan("");                                  // spin 0 and 1/2: I (2 I - 1) = 0, the spectroscopic moment is zero for any Q0
    const double I = spin2 / 2.0;
    return Qs * (I + 1) * (2 * I + 3) / (I * (2 * I - 1));              // K = I
}

// <(3 cos^2 theta - 1)^j> over the sphere, exactly (1/2 integral of (3u^2 - 1)^j du): 1, 0, 4/5, 16/35, 48/35, 128/77, 3392/1001
static const double kMom[7] = {1.0, 0.0, 4.0 / 5, 16.0 / 35, 48.0 / 35, 128.0 / 77, 3392.0 / 1001};
static const double kY20 = 0.31539156525252005;                          // sqrt(5 / (16 pi)): Y20 = kY20 (3 cos^2 theta - 1)
static double binom(int n, int k) { double r = 1; for (int i = 1; i <= k; i++) r = r * (n - k + i) / i; return r; }
// <(1 + beta Y20)^k> = sum_j C(k, j) (beta c)^j m_j      and      <(1 + beta Y20)^5 (3 cos^2 theta - 1)> = sum_j C(5, j) (beta c)^j m_(j + 1)
static double avgPow(int k, double beta) { double s = 0, bc = 1; for (int j = 0; j <= k; j++) { s += binom(k, j) * bc * kMom[j]; bc *= beta * kY20; } return s; }
static double momentNum5(double beta) { double s = 0, bc = 1; for (int j = 0; j <= 5; j++) { s += binom(5, j) * bc * kMom[j + 1]; bc *= beta * kY20; } return s; }

double shapeVolumeFactor(double beta) { return avgPow(3, beta); }
double shapeRadiusFactor(double beta, double u) { return (1 + beta * kY20 * (3 * u * u - 1)) * std::pow(shapeVolumeFactor(beta), -1.0 / 3); }
double shapeMomentRatio(double beta) { return momentNum5(beta) / avgPow(5, beta); }
double volumeEquivalentRadius(double Rpt, double beta) { return Rpt * std::pow(shapeVolumeFactor(beta), 5.0 / 6) / std::sqrt(avgPow(5, beta)); }
double betaFromRatio(double ratio, bool& ok) {
    double lo = -kShapeBetaLimit, hi = kShapeBetaLimit;
    ok = ratio >= shapeMomentRatio(lo) && ratio <= shapeMomentRatio(hi);
    if (!ok) return 0;
    for (int i = 0; i < 60; i++) { const double mid = 0.5 * (lo + hi); (shapeMomentRatio(mid) < ratio ? lo : hi) = mid; }   // the ratio grows with beta2 over the whole range
    return 0.5 * (lo + hi);
}
double pointProtonMs(int Z, int N) {
    const double rms = sizedByMeasurement(Z, N) ? chargeRadius(Z, N)->rms : std::sqrt(0.6) * nuclearR(Z + N);
    return std::max(0.1, rms * rms - kNucleonR * kNucleonR);
}

Shape nucShape(int Z, int N) {
    Shape s;
    s.row = quadRow(Z, N);
    const QuadRow* r = s.row;
    if (!r) s.moment = MomentUse::NoRow;
    else if (r->kind == 'X') s.moment = MomentUse::RowNotUsable;
    else if (r->kind != 'S') s.moment = MomentUse::SignNotGiven;          // unsigned, or a tentative sign
    else if (!r->spinFirm || r->spin2 < 2) s.moment = MomentUse::SpinNotFirm;
    else {
        const double ratio = intrinsicQ0(r->Qb, r->spin2) * kBarnFm2 / (Z * pointProtonMs(Z, N));
        bool ok = false;
        const double b = betaFromRatio(ratio, ok);
        s.beta1 = ratio / (4 * kY20);                                     // the first-order beta2 this ratio would need, for the card
        if (ok) { s.beta = b; s.src = ShapeSource::MeasuredMoment; s.moment = MomentUse::Used; return s; }
        s.moment = MomentUse::TooLarge;
    }
    s.beta = nucBeta(Z, N);                                               // what the lab had: B(E2) where it has it, else its estimate
    s.src = nucBE2Known(Z, N) ? ShapeSource::BE2 : ShapeSource::Estimate;
    return s;
}

static std::string f4(double x) { return fmt::fixed(std::fabs(x), 4); }
static std::string sg4(double x) { return (x < 0 ? "-" : "+") + f4(x); }
static std::string qb(double x) { return (x < 0 ? "-" : "+") + fmt::fixed(std::fabs(x), std::fabs(x) < 0.01 ? 6 : 4) + " b"; }
static std::string unb(double x) { return fmt::fixed(x, x >= 1e-4 ? 4 : x >= 1e-6 ? 6 : 8) + " b"; }
static std::string spinText(int spin2) { return spin2 % 2 ? std::to_string(spin2) + "/2" : std::to_string(spin2 / 2); }
static std::string formWord(double b) {
    if (b > 0.005) return "prolate (stretched like a rugby ball)";
    if (b < -0.005) return "oblate (flattened like a pancake)";
    return "round";
}

std::vector<std::string> nuclearShapeLines(int Z, int N) {
    std::vector<std::string> v;
    const Shape s = nucShape(Z, N);
    const QuadRow* r = s.row;
    const int A = Z + N;
    switch (s.src) {
        case ShapeSource::MeasuredMoment: v.push_back("Shape source: measured quadrupole moment (IAEA table of recommended electric quadrupole moments)."); break;
        case ShapeSource::BE2: v.push_back("Shape source: B(E2) (the lab's table of measured B(E2; 0+ → 2+) values: β₂ from the transition strength, the sign from the table)."); break;
        case ShapeSource::Estimate:
            if (nucMeasured(Z, N) && !nucBE2Known(Z, N)) v.push_back("Shape source: estimate. This is a doubly magic nucleus, drawn round: that is the shell model's expectation, not a measurement.");
            else if (A < 12) v.push_back("Shape source: estimate. Nuclei lighter than A = 12 are drawn round: the lab makes no estimate for them and this is not a measurement.");
            else v.push_back("Shape source: estimate, from the number of valence protons and neutrons (Casten's P-factor): a rule of thumb, not a measurement.");
            if (nucUsesUnsourcedMagic(Z, N)) v.push_back("This estimate counts Z = 114 or N = 184 as a magic number. Those two numbers are the web version's, with no source (they are predictions for superheavy nuclei, never observed), and they set this shape.");
            break;
    }
    if (r && r->kind != 'X') {
        const std::string q = r->kind == 'U' ? unb(r->Qb) : qb(r->Qb);
        switch (s.moment) {
            case MomentUse::Used:
                v.push_back("Measured: spectroscopic quadrupole moment Q = " + q + " ± " + unb(r->uncB) + " in the ground state, spin " + spinText(r->spin2) + " (the table says \"" + r->raw + "\"; method " + r->method + (*r->nsr ? ", NSR " : "") + r->nsr + "; 1 b = 100 fm²).");
                break;
            case MomentUse::TooLarge:
                v.push_back("Measured: spectroscopic quadrupole moment Q = " + q + " ± " + unb(r->uncB) + " in the ground state, spin " + spinText(r->spin2) + " (the table says \"" + r->raw + "\"; method " + r->method + "; 1 b = 100 fm²).");
                break;
            case MomentUse::SignNotGiven:
                v.push_back(r->kind == 'U' ? "The table has |Q| = " + q + " for this isotope but gives no sign, so prolate or oblate cannot be told from it: not used for the shape."
                                           : "The table has Q = " + q + " for this isotope but its sign is only tentative (the file writes \"" + r->raw + "\"): not used for the shape.");
                break;
            case MomentUse::SpinNotFirm:
                v.push_back("The table has Q = " + q + " for this isotope but its spin is only tentative (the file writes \"" + r->spin + "\"), and the conversion needs the spin: not used for the shape.");
                break;
            default: break;
        }
    } else if (r) {
        v.push_back(std::string("The table lists this isotope but its value (\"") + r->raw + "\") is not a single clean number: not used for the shape.");
    }
    if (s.moment == MomentUse::Used || s.moment == MomentUse::TooLarge) {
        const double Q0 = intrinsicQ0(r->Qb, r->spin2);
        v.push_back("Model assumption, not a measurement: for spin I = " + spinText(r->spin2) + " the strong-coupling rule (K = I) turns it into the intrinsic moment Q₀ = Q × (I+1)(2I+3) / (I(2I−1)) = " + qb(Q0) + ".");
    }
    if (s.moment == MomentUse::Used) {
        v.push_back(std::string("Shape: Q₀ ") + (s.beta >= 0 ? "> 0" : "< 0") + ": " + formWord(s.beta) + ", β₂ = " + sg4(s.beta) + " (a uniformly charged body whose volume is kept as it is deformed).");
    } else {
        if (s.moment == MomentUse::TooLarge)
            v.push_back("The strong-coupling rule would need |β₂| = " + fmt::fixed(std::fabs(s.beta1), 2) + ", above the limit " + fmt::fixed(kShapeBetaLimit, 1) + " this lab trusts it to: the moment is real, but it comes from a few nucleons and not from a deformed core. Not used; the shape stays the lab's B(E2) value or estimate.");
        if (!r) {
            if (A == 1) v.push_back("One proton, spin 1/2: a spin-1/2 nucleus has no spectroscopic quadrupole moment whatever its shape, so no moment can be measured; round is not implied.");
            else if (Z % 2 == 0 && N % 2 == 0) v.push_back("No measured moment: even Z and even N means spin 0, and a spin-0 nucleus has no spectroscopic quadrupole moment whatever its shape, so this table cannot say whether it is round or deformed. Round is not implied.");
            else v.push_back("This isotope has no row in the table. If its spin is 0 or 1/2 its moment is exactly zero whatever the shape and cannot be measured this way; the lab has no spin table, so it cannot say which applies.");
        }
        if (A > 1) v.push_back("Shape drawn: " + formWord(s.beta) + (std::fabs(s.beta) > 0.005 ? ", β₂ = " + sg4(s.beta) : "") + ".");
    }
    return v;
}

std::string shapeExplain() {
    return "Spectroscopic and intrinsic. The table gives the spectroscopic quadrupole moment Q: what a laboratory measures with the nucleus in its spin state I. It is exactly zero for spin 0 and 1/2 whatever the shape, so a missing moment never means round. "
           "The shape number β₂ describes the intrinsic moment Q₀, in the nucleus' own frame, and the two are different numbers. The conversion Q = Q₀ I(2I−1) / ((I+1)(2I+3)) assumes strong coupling (K = I): a rotating, permanently deformed nucleus. "
           "That is a model assumption, not a measurement, and it is poor for light nuclei and near closed shells, where a few nucleons carry the moment and there is no deformed core; the lab does not use it where it would need |β₂| above 0.6. "
           "Positive Q₀ means prolate (stretched like a football), negative oblate (squashed). The picture is a uniformly charged body r = R(1 + β₂Y₂₀) whose volume is kept as it is deformed and whose size still gives the measured charge radius; the cloud of nucleons has a diffuse edge, so its own moment differs slightly. "
           "It is static: real nuclei also vibrate and rotate, which the lab does not model.";
}

}  // namespace atom
