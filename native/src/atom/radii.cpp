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
    static const RadiusRow deuteron = [] { RadiusRow r = *iaeaRadius(1, 1); r.rms = kDeuteronRmsFm; r.unc = kDeuteronRmsUncFm; return r; }();
    if (Z == 1 && N == 1) return &deuteron;
    // 0.46.5: the proton too (the Density look drew a lone proton at the IAEA table's 0.8783 fm, the older electron-scattering value, while every other picture uses CODATA 2022's)
    static const RadiusRow proton = [] { RadiusRow r = *iaeaRadius(1, 0); r.rms = kProtonRmsFm; r.unc = kProtonRmsUncFm; return r; }();
    if (Z == 1 && N == 0) return &proton;
    return iaeaRadius(Z, N);
}
const RadiusRow* iaeaRadius(int Z, int N) {
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
double nucleonSizeMs(int Z, int N) { return kProtonRmsFm * kProtonRmsFm + (Z > 0 ? double(N) / Z : 0.0) * kNeutronMsFm2 + kDarwinFoldyFm2; }
double pointProtonMsFromCharge(int Z, int N, double rms) { return std::max(0.1, rms * rms - nucleonSizeMs(Z, N)); }
double neutronSkinFm(int Z, int N) { const int A = Z + N; return A > 0 ? std::max(-0.3, std::min(0.6, 0.90 * (N - Z) / double(A) - 0.03)) : 0; }
double wsHalfDensityRadiusForPointMs(double ms) {
    const double a = kWsPointDiffuseFm;
    const double target = std::max(0.1, ms);
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
    const RadiusRow* r = A == 1 ? iaeaRadius(Z, N) : chargeRadius(Z, N);   // (0.46.5: chargeRadius gives CODATA for the proton; the card names the table's own value)
    if (A == 1) {
        v.push_back("One proton. It is drawn at its charge radius, 0.84075 ± 0.00064 fm (CODATA 2022; PDG 2024: 0.8409 ± 0.0004). Some electron-scattering results still give about 0.88 fm (the proton radius puzzle, now mostly resolved).");
        if (r) v.push_back("The IAEA table lists " + f4(r->rms) + " ± " + f4(r->unc) + " fm for hydrogen; that is not the CODATA 2022 proton radius, so it is not used.");
        return v;
    }
    if (sizedByMeasurement(Z, N)) {
        const double rule = nuclearR(A), rr = ruleRms(A), d = r->rms - rr, R = nuclearRadius(Z, N);
        if (Z == 1 && N == 1) v.push_back("Measured: rms charge radius " + f4(r->rms) + " ± " + fmt::fixed(r->unc, 5) + " fm (CODATA 2022, from hydrogen and deuterium spectroscopy: the same source as the proton's radius). "
                                          "The IAEA table of electron-scattering radii gives " + f4(iaeaRadius(1, 1)->rms) + " ± " + f4(iaeaRadius(1, 1)->unc) + " fm, 1.6 standard deviations larger.");
        else v.push_back("Measured: rms charge radius " + f4(r->rms) + " ± " + f4(r->unc) + " fm (IAEA table of nuclear charge radii).");
        v.push_back("The usual rule (r = 1.2 A^(1/3) fm: a uniform ball of radius " + f4(rule) + " fm) is an rms charge radius of " + f4(rr) + " fm. Measured minus rule: " + sgn4(d) + " fm (" + (d >= 0 ? "+" : "-") + fmt::fixed(100 * std::fabs(d) / rr, 1) + " %).");
        v.push_back("Drawn (0.46): a soft-edged nucleus with exactly this rms radius: the Density look as a Fermi (Woods-Saxon) charge density, edge 0.54 fm, the Nucleons look as one snapshot of "
                    "nucleon centres sampled from it (edge 0.50 fm, each nucleon drawn with its own size). Light nuclei (0.46.5): the deuteron from its wave function (CD-Bonn), C-12 and O-16 from their measured harmonic-oscillator charge densities (de Vries 1987), the others Gaussians (no measured profile saved). The equivalent uniform ball, R = rms × √(5/3) = " + f4(R) +
                    " fm, only sets the picture's scale and the inside-the-nucleus estimate; the rule alone would give " + f4(rule) + " fm.");
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
           "How it is drawn (0.46): as a density with a soft edge, not a ball. The Density look draws the charge density as a Fermi (Woods-Saxon) distribution, 1 / (1 + e^((r − R) / a)) with a = 0.54 fm "
           "(the fits to electron scattering); light nuclei (A <= 16) differently: the deuteron from its wave function (CD-Bonn), C-12 and O-16 from their measured harmonic-oscillator charge densities (de Vries 1987), the others as a Gaussian; each scaled so its rms radius is the measured one; the Nucleons look samples nucleon centres from the same shape with the "
           "point-nucleon edge a = 0.50 fm (Chamon et al. 2002) and draws each nucleon with its own size on top. The equivalent uniform ball (R = rms × √(5/3)) only sets the picture's scale. "
           "A radius is a size, not a shape: the shape comes from a measured quadrupole moment where one can be used, else from a measured B(E2), else from the FRDM(2012) model, else it is drawn round (no published value); "
           "the card says which. ""The picture is static: real nuclei also vibrate and rotate, which the lab does not model. Where the table has no value, the usual rule r = 1.2 A^(1/3) fm is kept and the card says so.";
}

}  // namespace atom
