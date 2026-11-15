// 0.43: the Accurate quark clouds against published values and exact relations. Written BEFORE the drawing code. Reference numbers are typed from the
// saved sources (native/data/qcd/, README.txt there), never taken from the app or from the tool's output.
#include "atom/quark_density.h"
#include <random>

// The slope of a form factor gives the cloud's spread exactly: <b^2> = -4 dF1/dQ^2 at Q^2 = 0 (2D transform; Miller, PRL 99, 112001 (2007)).
// F1 = (GE + tau GM) / (1 + tau) gives dF1/dQ^2 (0) = GE'(0) + (GM(0) - GE(0)) / (4 M^2).
//   BBBA05 (data/qcd/nucleon/bbba05_bradford_2006_hep-ex0602017.pdf, Table 1): GEp = (1 - 0.0578 tau) / (1 + 11.1 tau + ...) -> GEp'(0) = (-0.0578 - 11.1) / (4 Mp^2)
//   Atac et al. 2021 (data/qcd/nucleon/atac_2021_2103.10840.pdf, eq. 5): GEn = (1 + Q^2/A)^-2 B tau / (1 + C tau), B = 1.655 -> GEn'(0) = 1.655 / (4 Mn^2)
//   CODATA 2022 masses and moments (data/qcd/nucleon/codata2022_*.html).
static void testQuarkCloudSpread() {
    const double hc = 0.1973269804, mp = 0.93827208943, mn = 0.93956542194, mup = 2.79284734463, mun = -1.91304276;
    const double f1p = ((-0.0578 - 11.1) + (mup - 1)) / (4 * mp * mp) * hc * hc;   // fm^2
    const double f1n = (1.655 + mun) / (4 * mn * mn) * hc * hc;
    const double bu = -4 * (2 * f1p + f1n) / 2, bd = -4 * (f1p + 2 * f1n);           // per quark: rho_u = 2 rho_p + rho_n (2 quarks), rho_d = rho_p + 2 rho_n (1 quark)
    // the tables stop at 4 fm; the tail beyond carries 0.15 to 0.2 % of <b^2> (b^2 weighs the far edge): the tolerance is 0.5 %, relative
    // (first written with 3 fm tables and a 1.5 % tolerance from the u cloud's tail; the d cloud's tail beyond 3 fm is 1.5 %, so the table was extended to 4 fm, not the tolerance widened)
    checkNum("u cloud: <b^2> from the form factors' slopes (BBBA05, Atac 2021)", atom::quarkDensityMeanB2(atom::QdKind::U), bu, 0.005);
    checkNum("d cloud: <b^2> from the form factors' slopes", atom::quarkDensityMeanB2(atom::QdKind::D), bd, 0.005);
    checkTrue("the d cloud is wider than the u cloud (Miller 2007: the d density 'becomes larger at distances away from the center')", atom::quarkDensityMeanB2(atom::QdKind::D) > atom::quarkDensityMeanB2(atom::QdKind::U));
    // gluons: A_g tripole, m_A = 1.641 GeV (lattice; Duran et al., Nature 615, 813 (2023), Table 1; data/qcd/quarks/duran_2023_2207.05212.pdf): <b^2> = 4 x 3 / m_A^2
    const double mA = 1.641;
    checkNum("gluon cloud: <b^2> = 12 / m_A^2 (tripole, m_A = 1.641 GeV)", atom::quarkDensityMeanB2(atom::QdKind::G), 12 / (mA * mA) * hc * hc, 1e-3);
    for (double b : {0.1, 0.3, 0.6, 1.2}) {   // closed form of the 2D transform of a tripole: m^2 (m b)^2 K2(m b) / (16 pi)
        const double m = mA / hc, want = m * m * (m * b) * (m * b) * std::cyl_bessel_k(2.0, m * b) / (16 * kPi);
        checkNum("gluon cloud at b = " + fmt::fixed(b, 1) + " fm: the closed form m^2 (mb)^2 K2(mb) / 16 pi", atom::quarkDensity(atom::QdKind::G, b), want, 1e-5);
    }
}

// Each cloud holds one quark: integral of 2 pi b rho db = 1 (the charges add up: proton +1, neutron 0). Inside 4 fm: at least 99.9 %.
static void testQuarkCloudNorm() {
    for (atom::QdKind k : {atom::QdKind::U, atom::QdKind::D, atom::QdKind::G}) {
        double s = 0;
        for (int i = 0; i + 1 < atom::kQdN; i++) { const double b0 = i * atom::kQdStep, b1 = b0 + atom::kQdStep; s += 0.5 * atom::kQdStep * 2 * kPi * (b0 * atom::quarkDensity(k, b0) + b1 * atom::quarkDensity(k, b1)); }
        checkNum("cloud " + std::to_string(int(k)) + " holds one quark (or all the gluons) inside 4 fm, to 0.1 %", s, 1.0, 1e-3);
    }
    // the drawing picks radii from the cumulative table: its inverse must give back each step
    bool ok = true;
    for (atom::QdKind k : {atom::QdKind::U, atom::QdKind::D, atom::QdKind::G})
        for (int i = 1; i < atom::kQdN; i += 7) ok = ok && std::fabs(atom::quarkDensityRadius(k, atom::qdCum(k)[i]) - i * atom::kQdStep) < 1e-9;
    checkTrue("the radius picked for a fraction is the table's own b (inverse of the cumulative table)", ok);
    // and dots picked that way have the cloud's own spread (Monte Carlo, 200,000 dots, fixed seed)
    std::mt19937 rng(43);
    std::uniform_real_distribution<double> U(0, 1);
    double s2 = 0;
    for (int i = 0; i < 200000; i++) { const double b = atom::quarkDensityRadius(atom::QdKind::U, U(rng)); s2 += b * b; }
    checkNum("200,000 dots drawn for the u cloud have its <b^2> (Monte Carlo, 1 %)", s2 / 200000, atom::quarkDensityMeanB2(atom::QdKind::U), 0.01);
}

// The published shapes (Miller 2007, data/qcd/nucleon/miller_2007_0705.2409.pdf): the neutron's transverse charge density is negative at the centre,
// positive further out and negative again far out; the proton's u quarks are about 70 % denser at the centre than its d quark (per quark).
static void testQuarkCloudShapes() {
    const auto rp = [](double b) { return 2.0 / 3 * 2 * atom::quarkDensity(atom::QdKind::U, b) - 1.0 / 3 * atom::quarkDensity(atom::QdKind::D, b); };
    const auto rn = [](double b) { return 2.0 / 3 * atom::quarkDensity(atom::QdKind::D, b) - 1.0 / 3 * 2 * atom::quarkDensity(atom::QdKind::U, b); };   // the neutron: its u has the proton's d profile, its two d the u profile
    checkTrue("the proton's charge density is positive everywhere inside 4 fm", [&] { for (int i = 0; i < 400; i++) if (rp(i * 0.01) <= 0) return false; return true; }());
    checkTrue("the neutron's charge density is negative at the centre (Miller 2007)", rn(0) < 0);
    checkTrue("... positive at 0.5 fm", rn(0.5) > 0);
    checkTrue("... and negative again far out, at 1.5 fm (Miller: 'long range negatively')", rn(1.5) < 0);
    const double ratio = atom::quarkDensity(atom::QdKind::U, 0) / atom::quarkDensity(atom::QdKind::D, 0);
    // Miller used Kelly's 2004 fit and says 'about 70 %'; the app's fits differ, so only that size is checked: 1.5 to 1.9
    checkTrue("central u density per quark is about 70 % above the d quark's (Miller 2007): " + fmt::fixed(ratio, 3), ratio > 1.5 && ratio < 1.9);
}

// The choices: two quark styles (the owner, 8 Nov 2026: Accurate and a Simple one for learning), two nucleus styles (Density and Nucleons).
static void testStyleChoices() {
    using atom::AtomLab;   // (the Controls panel's lists, ui/atom_panel.cpp, are checked by the UI suite; here the values the lab accepts)
    checkTrue("quark styles: Accurate and Simple, no Cinematic or Classic", AtomLab::validChoice("qstyle", "Accurate") && AtomLab::validChoice("qstyle", "Simple") && !AtomLab::validChoice("qstyle", "Cinematic") && !AtomLab::validChoice("qstyle", "Classic"));
    checkTrue("nucleus styles: Density and Nucleons, no Fuzzy or Hard balls", AtomLab::validChoice("style", "Density") && AtomLab::validChoice("style", "Nucleons") && !AtomLab::validChoice("style", "Fuzzy") && !AtomLab::validChoice("style", "Hard balls"));
    AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
    lab.setParam("qstyle", std::string("Cinematic")); const std::string a = lab.p.qstyle;
    lab.setParam("qstyle", std::string("Classic")); const std::string b = lab.p.qstyle;
    lab.setParam("style", std::string("Fuzzy")); const std::string c = lab.p.style;
    lab.setParam("style", std::string("Hard balls")); const std::string d = lab.p.style;
    checkTrue("old saved choices still work: Cinematic -> Accurate, Classic -> Simple, Fuzzy and Hard balls -> Nucleons", a == "Accurate" && b == "Simple" && c == "Nucleons" && d == "Nucleons");
}

static void testQuarkCloudWords() {
    const std::string acc = labWords("Accurate", "+ Gluons", true), sea = labWords("Accurate", "+ Sea", false), sim = labWords("Simple", "+ Gluons", true);
    checkTrue("Accurate: the quarks are a probability cloud, not balls, from measured form factors", has(acc, "cloud") && has(acc, "form factor"));
    checkTrue("Accurate: the frame is named (seen along a very fast nucleon's motion)", has(acc, "very fast"));
    checkTrue("Accurate: the form factor fits and the lattice gluons are named (0.44: as the cross-checks)", has(acc, "Bradford") && has(acc, "Atac") && has(acc, "lattice"));
    checkTrue("Accurate: each dot is one place a quark may be found, not a quark", has(acc, "not a quark"));
    // (0.44: the sea is drawn now, from measured spreads (COMPASS, H1): the 0.43 check that its pairs cancel in net densities and that its place "has not been measured" went with the
    // net clouds; the second half was too strong: deeply virtual Compton scattering does measure the spread of the sea. test_qcd044.inc checks the new words.)
    (void)sea;
    checkTrue("Simple: the balls and tubes are called a model for learning", has(sim, "for learning"));
    checkTrue("no style named Cinematic is mentioned any more", !has(acc, "Cinematic") && !has(sim, "Cinematic"));
}

static void testQcd043() {
    testQuarkCloudSpread();
    testQuarkCloudNorm();
    testQuarkCloudShapes();
    testStyleChoices();
    testQuarkCloudWords();
}
