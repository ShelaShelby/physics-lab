// Molecule Lab (0.53): rotation of the linear molecules. See rotation.h for the physics, its sources and its limits.
#include "molecule/rotation.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>

#include "atom/atom_core.h"
#include "molecule/vibration.h"

namespace molecule::rot {

namespace {
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
const double kHcOverK = atom::kPlanckJs * atom::kC * 100 / atom::kBoltzmann;   // hc/k, cm K (CODATA 2022, exact): a term of F cm^-1 at T K weighs exp(-kHcOverK F / T)

// Huber & Herzberg (1979), "Constants of Diatomic Molecules", via the NIST Chemistry WebBook (SRD 69), the X state's row, pages saved in
// data/molecules/vib/webbook_diatomic/ (read 6 Nov 2026; the DCl page 14 Nov 2026). A subscript digit in the table is its extra, uncertain digit, kept here.
//   H2  (C1333740)  Be 60.853_0, alpha_e 3.062_2, De 0.0471 (footnote 120: alpha_v has higher terms, Foltz, Rank et al. 1966; footnote 121: D_v changes with v)
//   N2  (C7727379)  Be 1.99824_1, alpha_e 0.017318 (footnote 110: from B0 and B1 of Bendtsen 1974), D [5.76E-6] (in brackets: D0)
//   O2  (C7782447)  [1.4376766] (in brackets: B0, footnote 70, Johns & Lepard 1975), alpha_e 0.0159_3, D [4.839E-6] (D0)
//   HCl (C7647010)  Be 10.59341_6, alpha_e 0.30718_1, De 5.3194E-4. Footnote 29 (to alpha_e): "+0.001772_4(v+1/2)^2 - 0.0001201(v+1/2)^3"; footnote 30 (to De):
//                   "-7.51_0E-6(v+1/2) + 4.0_0E-7(v+1/2)^2; higher order terms in Rank, Rao, et al., 1965" (not saved). The table's columns are Be, alpha_e, gamma_e, De,
//                   beta_e (B_v = Be - alpha_e (v+1/2) + gamma_e (v+1/2)^2 + ..., D_v = De + beta_e (v+1/2) + ...), and for the X state gamma_e and beta_e are empty with
//                   these footnotes on alpha_e and De instead, so they are read as the further terms of the same two series, with the signs as printed:
//                   B_v = Be - alpha_e x + 0.0017724 x^2 - 0.0001201 x^3, D_v = De - 7.510E-6 x + 4.00E-7 x^2, x = v + 1/2. (The page itself does not write the
//                   series out; the reading is checked by Rank et al. 1962's lines and by the DCl page's own measured gamma_e, 0.0004589 > 0, in tests/test_rotation.cpp.)
const Measured kMeasured[] = {
    {"H2", 60.8530, 3.0622, 0.0471, false, "Be, alpha_e, De (Huber & Herzberg 1979)"},
    {"N2", 1.998241, 0.017318, 5.76e-6, false, "Be, alpha_e, D0 (Huber & Herzberg 1979)"},
    {"O2", 1.4376766, 0.01593, 4.839e-6, true, "B0, alpha_e, D0 (Huber & Herzberg 1979; Johns & Lepard 1975)"},
    {"HCl", 10.593416, 0.307181, 5.3194e-4, false, "Be, alpha_e, De of H35Cl and the higher terms of B_v and D_v (Huber & Herzberg 1979, footnotes 29, 30; Rank, Rao et al. 1965)", 0.0017724, -0.0001201, -7.510e-6, 4.00e-7},
};
// NIST "Atomic Weights and Isotopic Compositions" (from AME2020), saved as data/molecules/nist_compositions_H.html and _Cl.html (18 Oct 2026): relative atomic
// mass and isotopic composition. 1H 1.00782503223(9), 0.999885(70); 2H 2.01410177812(12), 0.000115(70); 35Cl 34.968852682(37), 0.7576(10); 37Cl 36.965902602(55), 0.2424(10)
struct IsoMass { int z, a; double mass, ab; };
const IsoMass kIso[] = {{1, 1, 1.00782503223, 0.999885}, {1, 2, 2.01410177812, 0.000115}, {17, 35, 34.968852682, 0.7576}, {17, 37, 36.965902602, 0.2424}};
// The nuclear spins (2I) of the exchanged nuclei: NUBASE2020 (data/nubase/nubase_4.mas20.txt): 1H 1/2+, 14N 1+, 16O 0+ (12C 0+)
constexpr int kTwoI_H = 1, kTwoI_N14 = 2, kTwoI_O16 = 0;

// H35Cl's measured 1-0 lines (observed, vacuum, cm^-1): D. H. Rank, D. P. Eastman, B. S. Rao and T. A. Wiggins, "Rotational and vibrational constants of the HCl35 and
// DCl35 molecules", J. Opt. Soc. Am. 52, 1 (1962), Table III; the paper saved as data/molecules/vib/hcl_lines/rank_eastman_rao_wiggins_1962_josa52_1.pdf, the table
// read from it into rank1962_table3.txt (how: that file's head). tests/test_rotation.cpp checks this list against the file row for row.
const MeasuredLine kRank1962[] = {{'P', 1, 2865.0967}, {'P', 2, 2843.6234}, {'P', 3, 2821.5713}, {'P', 4, 2798.9401}, {'P', 5, 2775.7602}, {'P', 6, 2752.0363}, {'P', 7, 2727.7774}, {'P', 8, 2703.0068}, {'P', 9, 2677.7320}, {'P', 10, 2651.9664}, {'P', 11, 2625.7272}, {'P', 12, 2599.0208}, {'P', 13, 2571.8703}, {'P', 14, 2544.2817}, {'P', 15, 2516.2724}, {'R', 0, 2906.2521}, {'R', 1, 2925.8950}, {'R', 2, 2944.9154}, {'R', 3, 2963.2865}, {'R', 4, 2981.0013}, {'R', 5, 2998.0438}, {'R', 6, 3014.4114}, {'R', 7, 3030.0862}, {'R', 8, 3045.0569}, {'R', 9, 3059.3179}, {'R', 10, 3072.8490}, {'R', 11, 3085.6539}, {'R', 12, 3097.7034}, {'R', 13, 3109.0026}, {'R', 14, 3119.5362}, {'R', 15, 3129.2972}};

double reduced(double a, double b) { return a * b / (a + b); }
}  // namespace

const char* isoName(int iso) {
    static const char* const n[] = {"H³⁵Cl", "H³⁷Cl", "D³⁵Cl", "D³⁷Cl", "Natural mix"};
    return n[std::clamp(iso, 0, kIsoCount - 1)];
}
const char* isoShort(int iso) {
    static const char* const n[] = {"H³⁵Cl", "H³⁷Cl", "D³⁵Cl", "D³⁷Cl", "Mix"};
    return n[std::clamp(iso, 0, kIsoCount - 1)];
}

std::vector<MeasuredLine> measuredHClLines() { return std::vector<MeasuredLine>(std::begin(kRank1962), std::end(kRank1962)); }
double measuredHClLine(char branch, int J) { for (const MeasuredLine& l : kRank1962) if (l.branch == branch && l.J == J) return l.nu; return kNaN; }

const Measured* measured(const std::string& id) {
    for (const Measured& m : kMeasured) if (id == m.id) return &m;
    return nullptr;
}

double massU(int z, int a) { for (const IsoMass& i : kIso) if (i.z == z && i.a == a) return i.mass; return kNaN; }
double abundance(int z, int a) { for (const IsoMass& i : kIso) if (i.z == z && i.a == a) return i.ab; return kNaN; }

void spinWeights(int twoI, bool sigmaMinus, int& gEven, int& gOdd) {
    // (2I+1)^2 spin states: (I+1)(2I+1) symmetric, I(2I+1) antisymmetric under the exchange (in units of 1/2: (twoI+2)(twoI+1)/2 and twoI(twoI+1)/2)
    const int sym = (twoI + 2) * (twoI + 1) / 2, anti = twoI * (twoI + 1) / 2;
    const bool fermion = twoI % 2 == 1;
    // the rotational (with the electronic) part: symmetric for even J in Sigma_g+, for odd N in Sigma_g-. The whole must be symmetric (bosons) or antisymmetric (fermions).
    const bool evenSym = !sigmaMinus;
    const int evenSpin = (evenSym != fermion) ? sym : anti;   // the spin part that makes the whole right for an even level
    const int oddSpin = (evenSym != fermion) ? anti : sym;
    gEven = evenSpin; gOdd = oddSpin;
}

bool hasRotation(const Molecule& m) { return m.linear; }

Rotor rotor(const std::string& id, int iso) {
    Rotor r;
    r.id = id;
    const int mi = findId(id);
    if (mi < 0) return r;
    const Molecule& mol = get(mi);
    r.label = prettyFormula(mol.formula);
    if (const Measured* m = measured(id)) {
        double s = 1;   // mu(H35Cl) / mu(this isotope)
        if (id == "HCl") {
            if (iso == kNatural) iso = kH35Cl;
            r.iso = iso;
            const double mh = massU(1, iso == kD35Cl || iso == kD37Cl ? 2 : 1), mcl = massU(17, iso == kH37Cl || iso == kD37Cl ? 37 : 35);
            r.muU = reduced(mh, mcl);
            s = reduced(massU(1, 1), massU(17, 35)) / r.muU;
            r.scaled = iso != kH35Cl;
            r.abundance = iso == kH37Cl ? abundance(17, 37) : iso == kH35Cl ? abundance(17, 35) : 1;
            r.label = isoName(iso);
            r.dipole = true;
            if (const vib::Anharmonic* a = vib::anharmonic("HCl")) {   // the band origin from the measured ladder (scaled: Y_k0 ~ mu^-k/2)
                vib::Anharmonic b = *a;
                b.we *= std::sqrt(s); b.wexe *= s; b.weye *= std::pow(s, 1.5); b.weze *= s * s;
                r.nu0 = vib::termG(b, 1) - vib::termG(b, 0);
            }
        } else {
            const int e0 = mol.atoms[0].el;
            r.muU = reduced(element(e0).massU, element(e0).massU);
        }
        // Dunham's Y_kl ~ mu^-(k/2 + l): Be (Y_01) ~ s, alpha_e (Y_11) ~ s^3/2, gamma_e (Y_21) ~ s^2, the x^3 term (Y_31) ~ s^5/2; De (Y_02) ~ s^2, beta_e (Y_12) ~ s^5/2, Y_22 ~ s^3
        const double Be = m->Be * s, al = m->alphaE * std::pow(s, 1.5), g2 = m->bv2 * s * s, g3 = m->bv3 * std::pow(s, 2.5);
        const double De = m->De * s * s, d1 = m->dv1 * std::pow(s, 2.5), d2 = m->dv2 * s * s * s;
        for (int v = 0; v < 2; v++) {
            const double x = v + 0.5;
            r.B[v] = m->b0 ? Be - al * v : Be - al * x + g2 * x * x + g3 * x * x * x;
            r.D[v] = De + d1 * x + d2 * x * x;
        }
        r.source = m->note;
    } else {
        r.B[0] = mol.rotB; r.B[1] = 0; r.D[0] = r.D[1] = 0;
        r.source = "B0 (NIST CCCBDB), rigid";
    }
    // the weights and the symmetry number
    if (id == "H2") spinWeights(kTwoI_H, false, r.gEven, r.gOdd);
    else if (id == "N2") spinWeights(kTwoI_N14, false, r.gEven, r.gOdd);
    else if (id == "O2") { spinWeights(kTwoI_O16, true, r.gEven, r.gOdd); r.nLevels = true; }
    else if (id == "CO2") spinWeights(kTwoI_O16, false, r.gEven, r.gOdd);
    else if (id == "C2H2") spinWeights(kTwoI_H, false, r.gEven, r.gOdd);
    if (id == "HCN") r.dipole = true;
    r.sigma = (id == "HCl" || id == "HCN") ? 1 : 2;
    return r;
}

double term(const Rotor& r, int v, int J) {
    const double x = static_cast<double>(J) * (J + 1);
    const int i = v == 1 ? 1 : 0;
    return r.B[i] * x - r.D[i] * x * x;
}
double termG(const Rotor& r, int v) {
    // (only the difference G(1) - G(0) is used: the band origin)
    return v == 0 ? 0 : r.nu0;
}
int weight(const Rotor& r, int J) { return J % 2 ? r.gOdd : r.gEven; }
int lowestJ(const Rotor& r) { return r.gEven == 0 ? 1 : 0; }

int risingTop(const Rotor& r, int v) {
    if (r.D[v == 1 ? 1 : 0] <= 0) return 100000;
    int J = 0;
    while (term(r, v, J + 1) > term(r, v, J)) J++;
    return J;
}

// the sum with the energies counted from the lowest allowed level (no underflow when cold); Q_textbook = Q_rel exp(-hc F(j0) / kT)
Partition partitionRel(const Rotor& r, double T) {
    Partition p;
    const int j0 = lowestJ(r);
    const int top = risingTop(r);
    const double f0 = term(r, 0, j0);
    for (int J = j0; J <= top; J++) {
        const double t = weight(r, J) * (2 * J + 1) * std::exp(-kHcOverK * (term(r, 0, J) - f0) / T);
        p.Q += t;
        p.top = J;
        if (J > j0 + 1 && t > 0 && t < 1e-12 * p.Q) return p;
        if (J == top) { p.lastRel = t / p.Q; p.cut = p.lastRel >= 1e-6; }
    }
    return p;
}
Partition partition(const Rotor& r, double T) {
    if (T <= 0) { Partition p; p.top = lowestJ(r); return p; }
    Partition p = partitionRel(r, T);
    p.Q *= std::exp(-kHcOverK * term(r, 0, lowestJ(r)) / T);
    return p;
}

double share(const Rotor& r, int J, double T) {
    if (J < 0 || weight(r, J) == 0) return 0;
    const int j0 = lowestJ(r);
    if (T <= 0) return J == j0 ? 1 : 0;
    const Partition p = partitionRel(r, T);
    if (J > p.top) return 0;
    return weight(r, J) * (2 * J + 1) * std::exp(-kHcOverK * (term(r, 0, J) - term(r, 0, j0)) / T) / p.Q;
}

int mostPopulated(const Rotor& r, double T) {
    const int j0 = lowestJ(r);
    if (T <= 0) return j0;
    const int top = partition(r, T).top;
    int best = j0; double bs = -1;
    for (int J = j0; J <= top; J++) { const double s = share(r, J, T); if (s > bs) { bs = s; best = J; } }
    return best;
}

std::string Line::name() const {
    if (branch == 'R') return "R(" + std::to_string(J) + ")";
    if (branch == 'P') return "P(" + std::to_string(J) + ")";
    return "J " + std::to_string(jLo) + " → " + std::to_string(jHi);
}

bool hasSpectrum(const Rotor& r, Spectrum s) {
    if (s == Spectrum::PureRotation) return r.dipole;
    return r.id == "HCl";
}

extern const double kC2 = kHcOverK;
double absorbanceFactor(double nuCm, double T) { return T > 0 ? nuCm * (1 - std::exp(-kC2 * nuCm / T)) : nuCm; }

std::vector<Line> lines(const std::string& id, int iso, Spectrum s, double T, double minRel, Measure meas) {
    std::vector<Line> out;
    std::vector<int> isos{iso};
    if (id == "HCl" && iso == kNatural) isos = {kH35Cl, kH37Cl};
    for (const int is : isos) {
        const Rotor r = rotor(id, is);
        if (!hasSpectrum(r, s)) return {};
        const double ab = (id == "HCl" && iso == kNatural) ? r.abundance : 1;
        const int top = std::max(1, partition(r, T > 0 ? T : 1).top);
        for (int J = lowestJ(r); J <= top; J++) {
            const double n = share(r, J, T > 0 ? T : 0) * ab;
            if (s == Spectrum::PureRotation) {
                Line l; l.branch = 'J'; l.J = J; l.m = J + 1; l.jLo = J; l.jHi = J + 1; l.iso = r.iso;
                l.nu = term(r, 0, J + 1) - term(r, 0, J);
                l.strength = n * (J + 1.0) / (2 * J + 1);
                out.push_back(l);
            } else {
                Line R; R.branch = 'R'; R.J = J; R.m = J + 1; R.vLo = 0; R.vHi = 1; R.jLo = J; R.jHi = J + 1; R.iso = r.iso;
                R.nu = r.nu0 + term(r, 1, J + 1) - term(r, 0, J);
                R.strength = n * (J + 1.0) / (2 * J + 1);
                out.push_back(R);
                if (J >= 1) {
                    Line P; P.branch = 'P'; P.J = J; P.m = -J; P.vLo = 0; P.vHi = 1; P.jLo = J; P.jHi = J - 1; P.iso = r.iso;
                    P.nu = r.nu0 + term(r, 1, J - 1) - term(r, 0, J);
                    P.strength = n * static_cast<double>(J) / (2 * J + 1);
                    out.push_back(P);
                }
            }
        }
    }
    if (meas == Measure::Absorbance) for (Line& l : out) l.strength *= absorbanceFactor(l.nu, T);   // 0.96.0: HITRAN's line intensity (rotation.h, Measure)
    double top = 0;
    for (const Line& l : out) top = std::max(top, l.strength);
    if (top <= 0) return {};
    std::vector<Line> kept;
    for (Line& l : out) { l.strength /= top; if (l.strength >= minRel) kept.push_back(l); }
    std::sort(kept.begin(), kept.end(), [](const Line& a, const Line& b) { return a.nu < b.nu; });
    return kept;
}

double inertiaKgM2(const Rotor& r) { return r.B[0] > 0 ? atom::kPlanckJs / (8 * atom::kPi * atom::kPi * atom::kC * 100 * r.B[0]) : 0; }
double omegaRadS(const Rotor& r, int J) {
    const double I = inertiaKgM2(r);
    return I > 0 ? std::sqrt(static_cast<double>(J) * (J + 1)) * atom::kPlanckJs / (2 * atom::kPi) / I : 0;
}
double screenTurnS(const Rotor& r, int J) { const double w = omegaRadS(r, J); return w > 0 ? 2 * atom::kPi / w * kSlowDown : 0; }

}  // namespace molecule::rot
