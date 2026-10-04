// See radii.h: measured nuclear charge radii (IAEA) and what the lab does with them.
#include "atom/radii.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "atom/atom_core.h"
#include "orbit/format.h"

namespace atom {

#include "atom/radii_data.inc"

static const int kRadiusCount = int(sizeof(kRadiusRows) / sizeof(kRadiusRows[0]));

const RadiusRow* radiusRows(int& count) { count = kRadiusCount; return kRadiusRows; }
const RadiusRow* chargeRadius(int Z, int N) {
    const RadiusRow* e = kRadiusRows + kRadiusCount;
    const RadiusRow* it = std::lower_bound(kRadiusRows, e, std::pair<int, int>{Z, N}, [](const RadiusRow& r, const std::pair<int, int>& k) { return r.Z < k.first || (r.Z == k.first && r.N < k.second); });
    return it != e && it->Z == Z && it->N == N ? it : nullptr;
}
bool sizedByMeasurement(int Z, int N) {
    if (Z + N <= 1) return false;                     // a single proton keeps the lab's own proton radius (CODATA 2022), not the table's older hydrogen value
    const RadiusRow* r = chargeRadius(Z, N);
    return r && !std::isnan(r->rms);                   // the preliminary column is not used
}
double ruleRms(int A) { return std::sqrt(0.6) * nuclearR(A); }
double nuclearRadius(int Z, int N) { return sizedByMeasurement(Z, N) ? kSharpOverRms * chargeRadius(Z, N)->rms : nuclearR(Z + N); }

// mean square radius of the Woods-Saxon density 1 / (1 + exp((r - R) / a)): the integral of r^4 f over the integral of r^2 f (Simpson, 1,000 intervals out to R + 40 a)
static double wsMeanSquare(double R, double a) {
    const int n = 1000;
    const double h = (R + 40 * a) / n;
    double num = 0, den = 0;
    for (int i = 0; i <= n; i++) {
        const double r = i * h, f = 1 / (1 + std::exp((r - R) / a)), w = (i == 0 || i == n) ? 1 : (i % 2 ? 4 : 2), r2f = r * r * f;
        num += w * r2f * r * r; den += w * r2f;
    }
    return num / den;
}
double wsHalfDensityRadiusForChargeRms(double rms) {
    const double a = 0.54;                                                             // the lab's diffuseness (fm)
    const double target = std::max(0.1, rms * rms - kNucleonR * kNucleonR);
    double lo = 0.05, hi = 25;
    for (int i = 0; i < 40; i++) { const double mid = 0.5 * (lo + hi); (wsMeanSquare(mid, a) < target ? lo : hi) = mid; }
    return 0.5 * (lo + hi);
}

static std::string f4(double x) { return fmt::fixed(x, 4); }
static std::string sgn4(double x) { return (x >= 0 ? "+" : "-") + fmt::fixed(std::fabs(x), 4); }
static std::string isoName(int Z, int A) { return std::string(symbol(Z)) + "-" + std::to_string(A); }

std::vector<std::string> nuclearSizeLines(int Z, int N) {
    std::vector<std::string> v;
    const int A = Z + N;
    const RadiusRow* r = chargeRadius(Z, N);
    if (A == 1) {
        v.push_back("One proton. It is drawn at the lab's proton radius, 0.84 fm (CODATA 2022: 0.84075 ± 0.00064 fm).");
        if (r) v.push_back("The IAEA table lists " + f4(r->rms) + " ± " + f4(r->unc) + " fm for hydrogen; that is not the CODATA 2022 proton radius, so it is not used.");
        return v;
    }
    if (sizedByMeasurement(Z, N)) {
        const double rule = nuclearR(A), rr = ruleRms(A), d = r->rms - rr, R = nuclearRadius(Z, N);
        v.push_back("Measured: rms charge radius " + f4(r->rms) + " ± " + f4(r->unc) + " fm (IAEA table of nuclear charge radii).");
        v.push_back("The usual rule (r = 1.2 A^(1/3) fm: a uniform ball of radius " + f4(rule) + " fm) is an rms charge radius of " + f4(rr) + " fm. Measured minus rule: " + sgn4(d) + " fm (" + (d >= 0 ? "+" : "-") + fmt::fixed(100 * std::fabs(d) / rr, 1) + " %).");
        v.push_back("Drawn: a uniform ball of radius " + f4(R) + " fm (the measured rms × sqrt(5/3)); the rule alone would draw " + f4(rule) + " fm.");
    } else {
        v.push_back("Size: no measured radius for this isotope; size from the usual rule (r = 1.2 A^(1/3) fm = " + f4(nuclearR(A)) + " fm).");
        if (r && !std::isnan(r->prelim)) v.push_back("The IAEA file has only a preliminary value for " + isoName(Z, A) + " (" + f4(r->prelim) + " ± " + f4(r->prelimUnc) + " fm); it is marked preliminary there and is not used.");
    }
    // two things the measurements themselves show, stated as measured (no theory is offered for them here)
    if (Z == 2 && sizedByMeasurement(2, 1) && sizedByMeasurement(2, 2) && chargeRadius(2, 2)->rms < chargeRadius(2, 1)->rms) {
        const Binding b3 = massBinding(2, 1), b4 = massBinding(2, 2);
        v.push_back("Measured: helium-4 (" + f4(chargeRadius(2, 2)->rms) + " fm) is smaller than helium-3 (" + f4(chargeRadius(2, 1)->rms) + " fm) although it has one more neutron. Helium-4 is far more tightly bound: " +
                    fmt::fixed(b4.B / 4, 2) + " MeV per nucleon against " + fmt::fixed(b3.B / 3, 2) + " for helium-3 (AME2020). Tightly bound nuclei are compact: that is what the numbers show; no explanation is claimed here.");
    }
    if (Z == 3) {
        std::vector<int> have, gaps;
        int minA = 0;
        for (int a = 3; a <= 14; a++) if (sizedByMeasurement(3, a - 3)) { have.push_back(a); if (!minA || chargeRadius(3, a - 3)->rms < chargeRadius(3, minA - 3)->rms) minA = a; }
        for (int a = have.front(); a <= have.back(); a++) if (!sizedByMeasurement(3, a - 3)) gaps.push_back(a);
        std::string s = "Measured lithium charge radii (fm):";
        for (int a : have) s += std::string(a == have.front() ? " " : ", ") + isoName(3, a) + " " + f4(chargeRadius(3, a - 3)->rms);
        s += ".";
        for (int a : gaps) s += " " + isoName(3, a) + " has no measured radius.";
        if (minA != have.front() && minA != have.back()) s += " The radius falls to " + isoName(3, minA) + " and rises again by " + isoName(3, have.back()) + ".";
        v.push_back(s);
        if (sizedByMeasurement(3, 8) && sizedByMeasurement(3, 4)) {
            const RadiusRow* a = chargeRadius(3, 8); const RadiusRow* b = chargeRadius(3, 4);
            v.push_back("Li-11 minus Li-7: " + sgn4(a->rms - b->rms) + " fm, with an uncertainty of ±" + f4(std::hypot(a->unc, b->unc)) + " fm: by charge radius the two are not clearly different. Li-11 is a halo nucleus, and a halo is a matter radius, not a charge radius (see \"What is a charge radius?\").");
        }
    }
    return v;
}

std::string chargeRadiusExplain() {
    return "What is a charge radius? It is the rms radius of the nucleus' electric charge: the square root of the average of r² over the charge, which is the protons'. Neutrons carry no electric charge, so the number says where the protons are, "
           "not directly where the neutrons are: in a neutron-rich nucleus the matter (protons and neutrons together) can reach further out than this number suggests. That is also why halo nuclei (a few light isotopes with one or two loosely bound neutrons "
           "far outside the rest) cannot be drawn from this table: their large size is a matter radius, and this lab does not have measured matter radii yet. "
           "How it is drawn: the lab draws a nucleus as a uniform ball. A uniform ball of radius R has rms radius R × sqrt(3/5), so the ball is drawn with R = measured rms × sqrt(5/3) (uniform ball is an assumption, not a measurement: real nuclei have soft edges). "
           "A radius is a size, not a shape: it does not say whether a nucleus is stretched or flattened; that part of the picture is still the lab's earlier estimate, and measured quadrupole moments are not used yet. "
           "The picture is static: real nuclei also vibrate and rotate, which the lab does not model. Where the table has no value, the usual rule r = 1.2 A^(1/3) fm is kept and the card says so.";
}

}  // namespace atom
