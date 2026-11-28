// 0.54.0: atoms in a magnetic field (atom/zeeman.h). Written before the code. Every expected value is independent of the module: textbook patterns in units of the
// Lorentz splitting mu_B B / hc, the closed-form two-level formula of a 2P doublet built here from L.S in the uncoupled basis, sum rules, CODATA 2022's own mu_B/hc
// (data/calc/codata2022_mohr2024_arxiv2409.03787.pdf, Table XXXIII, read with pdftotext -layout) and the Lande g values as printed in the saved NIST ASD files.
#include "atom/threej.h"
#include "atom/zeeman.h"

namespace zt {
namespace z = atom::zeeman;
// a lab line by its two J levels and the upper/lower term names
static const z::LineRef* findLine(const std::vector<z::LineRef>& v, const std::string& up, const std::string& lo, int ju, int jl) {
    for (const z::LineRef& r : v) if (r.line.up.name == up && r.line.lo.name == lo && r.line.upJ2 == ju && r.line.loJ2 == jl) return &r;
    return nullptr;
}
static int levelIndex(int Z, const std::string& conf, const std::string& term, int j2) {
    const std::vector<atom::JLevel>& lv = atom::jumpLevels(Z);
    for (std::size_t i = 0; i < lv.size(); i++) if (lv[i].conf == conf && lv[i].term == term && lv[i].j2 == j2) return static_cast<int>(i);
    return -1;
}
// the weak-field slopes of a line's components (Lorentz units) and their strengths (the mean of +B and -B: the strengths change in first order), by a symmetric difference at a field where mu_B B is 1e-5 of the smaller gap (the cubic term then 1e-10)
static std::vector<double> weakSlopes(const z::Line& L, double gs, std::vector<int>* q = nullptr, std::vector<double>* s = nullptr) {
    double mu = 0, gap = 0;
    z::regime(L, 1, mu, gap);
    const double B = std::isfinite(gap) ? 1e-5 * gap / z::lorentzCmPerT() : 1e-3;
    const std::vector<z::Component> a = z::components(L, B, gs), b = z::components(L, -B, gs);
    std::vector<double> out;
    for (const z::Component& c : a)
        for (const z::Component& d : b)
            if (c.M2u == d.M2u && c.M2l == d.M2l) { out.push_back((c.shiftCm - d.shiftCm) / (2 * B * z::lorentzCmPerT())); if (q) q->push_back(c.q); if (s) s->push_back((c.strength + d.strength) / 2); }
    return out;
}
static bool sameSet(std::vector<double> a, std::vector<double> b, double tol) {
    if (a.size() != b.size()) return false;
    std::sort(a.begin(), a.end()); std::sort(b.begin(), b.end());
    for (std::size_t i = 0; i < a.size(); i++) if (std::fabs(a[i] - b[i]) > tol) return false;
    return true;
}
}  // namespace zt

static void testZeemanLorentz() {
    namespace z = atom::zeeman;
    // CODATA 2022 Table XXXIII: mu_B / hc = 46.686 447 719(15) m^-1 T^-1 (relative uncertainty 3.1e-10)
    checkNum("Zeeman: Lorentz unit mu_B/hc from the lab's mu_B, h, c against CODATA's own 46.686 447 719 m^-1 T^-1", z::lorentzCmPerT() * 100, 46.686447719, 1e-10);
    // CODATA 2022: g_e = -2.002 319 304 360 92(36); the module's g_s is its size
    checkNum("Zeeman: g_s = |g_e| (CODATA 2022)", z::gsCodata(), 2.00231930436092, 1e-15);
}

static void testZeemanWeakPatterns() {
    namespace z = atom::zeeman;
    using zt::findLine; using zt::weakSlopes; using zt::sameSet;
    // (1) singlet-singlet: the normal triplet -1, 0, +1 exactly (zinc 4s4p 1P1 -> 4s2 1S0, cadmium 5s5p 1P1 -> 5s2 1S0: NIST lists no measured g for these levels)
    for (const int Z : {30, 48}) {
        const std::vector<z::LineRef> v = z::labLines(Z, atom::defaultN(Z), 2);
        const z::LineRef* r = nullptr;
        for (const z::LineRef& x : v) if (x.line.up.S2 == 0 && x.line.lo.S2 == 0 && x.line.up.L == 1 && x.line.lo.L == 0 && x.line.lo.J2.size() == 1 && x.line.lo.cm[0] == 0) r = &x;
        checkTrue(std::string("Zeeman: ") + atom::name(Z) + " has its 1P1 -> 1S0 ground line", r != nullptr);
        if (!r) continue;
        std::vector<int> q; std::vector<double> s;
        const std::vector<double> sl = weakSlopes(r->line, z::gsCodata(), &q, &s);
        // 0.96.0: with the orbital g-factor g_L = 1 - m_e / m_nuc (testZeemanReducedMass) the triplet is -g_L, 0, +g_L (until 0.95.0 exactly -1, 0, +1)
        const double gLs = r->line.up.gL;
        checkTrue(std::string("Zeeman: ") + atom::name(Z) + " singlet line: the normal triplet -g_L, 0, +g_L (Lorentz units)", sameSet(sl, {-gLs, 0, gLs}, 1e-12) && gLs == z::orbitalG(Z, atom::defaultN(Z)));
        // each component carries a third of the line (3j-squared of J = 1 -> 0); across the field sigma : pi : sigma = 1 : 2 : 1
        bool third = s.size() == 3;
        for (const double x : s) third = third && std::fabs(x - 1.0 / 3) < 1e-12;
        checkTrue("Zeeman: normal triplet strengths 1/3 each", third);
        const std::vector<z::Component> c = z::components(r->line, 1.0, z::gsCodata());
        double pi = 0, sg = 0;
        for (const z::Component& k : c) (k.q == 0 ? pi : sg) += z::seenStrength(k, false);
        checkNum("Zeeman: normal triplet across the field: pi / (each sigma) = 2", pi / (sg / 2), 2, 1e-12);
        double along = 0, alongPi = 0;
        for (const z::Component& k : c) { along += z::seenStrength(k, true); if (k.q == 0) alongPi += z::seenStrength(k, true); }
        checkTrue("Zeeman: along the field the pi component is not seen", alongPi < 1e-30 && along > 0);
        // B = 1 T exactly one Lorentz unit for every B (no fine-structure partner: linear at every field)
        const std::vector<z::Component> c50 = z::components(r->line, 50.0, z::gsCodata());
        bool lin = c50.size() == 3;
        for (const z::Component& k : c50) lin = lin && std::fabs(k.shiftCm - k.q * gLs * 50.0 * z::lorentzCmPerT()) < 1e-12;
        checkTrue("Zeeman: the singlet triplet stays linear at 50 T", lin);
    }
    // (2) sodium D lines with g_s = 2 exactly: D1 at +-2/3, +-4/3 and D2 at +-1/3, +-1, +-5/3 (Bethe & Salpeter; any textbook's anomalous Zeeman pattern) when g_L = 1.
    // 0.96.0: with g_L = 1 - m_e / m_nuc the same pattern is built from g(2P1/2) = 4 g_L / 3 - g_s / 3, g(2P3/2) = 2 g_L / 3 + g_s / 3, g(2S1/2) = g_s (Steck Eq. (21))
    const std::vector<z::LineRef> na = z::labLines(11, atom::defaultN(11), 1);
    const z::LineRef* d1 = zt::findLine(na, "3p ²P°", "3s ²S", 1, 1);
    const z::LineRef* d2 = zt::findLine(na, "3p ²P°", "3s ²S", 3, 1);
    checkTrue("Zeeman: sodium D1 and D2 are lab lines", d1 && d2);
    if (d1 && d2) {
        std::vector<int> q1, q2; std::vector<double> s1, s2;
        const double gLn = d1->line.up.gL;
        const auto pattern = [](double gL, bool d2line) {   // g_s = 2
            const double gu = d2line ? 2 * gL / 3 + 2.0 / 3 : 4 * gL / 3 - 2.0 / 3;
            std::vector<double> t;
            const int Ju = d2line ? 3 : 1;
            for (int Mu = -Ju; Mu <= Ju; Mu += 2) for (int Ml = -1; Ml <= 1; Ml += 2) if (std::abs(Mu - Ml) <= 2) t.push_back(gu * Mu / 2.0 - 2.0 * Ml / 2.0);
            return t;
        };
        checkTrue("Zeeman: Na D1/D2 pattern with g_L = 1 is the textbook +-2/3, +-4/3 and +-1/3, +-1, +-5/3", sameSet(pattern(1.0, false), {-4. / 3, -2. / 3, 2. / 3, 4. / 3}, 1e-12) &&
                  sameSet(pattern(1.0, true), {-5. / 3, -1, -1. / 3, 1. / 3, 1, 5. / 3}, 1e-12));
        checkTrue("Zeeman: Na D1, g_s = 2: the 4 components with g_L of 23Na", sameSet(weakSlopes(d1->line, 2.0, &q1, &s1), pattern(gLn, false), 1e-9));
        checkTrue("Zeeman: Na D2, g_s = 2: the 6 components with g_L of 23Na", sameSet(weakSlopes(d2->line, 2.0, &q2, &s2), pattern(gLn, true), 1e-9));
        // the strengths: D1 pi : sigma = 1 : 2 (each pi 1/6, each sigma 1/3 of the line); D2 sigma(3/2 -> 1/2) : pi : sigma(1/2 -> -1/2) = 3 : 2 : 1 (twelfths: 3, 2, 1)
        std::vector<double> want1, want2;
        for (std::size_t i = 0; i < q1.size(); i++) want1.push_back(q1[i] == 0 ? 1.0 / 6 : 1.0 / 3);
        checkTrue("Zeeman: Na D1 strengths pi 1/6, sigma 1/3", sameSet(s1, want1, 1e-9));
        checkTrue("Zeeman: Na D2 strengths 3/12, 2/12, 1/12 (each twice)", sameSet(s2, {3. / 12, 3. / 12, 2. / 12, 2. / 12, 1. / 12, 1. / 12}, 1e-9));
        // with CODATA's g_s the shifts are g_u M_u - g_l M_l with Lande's formula: g(2P1/2) = g_L - (g_s - g_L)/3, g(2P3/2) = g_L + (g_s - g_L)/3, g(2S1/2) = g_s (0.96.0: g_L; until 0.95.0 g_L = 1)
        const double gs = z::gsCodata(), gP1 = gLn - (gs - gLn) / 3, gP3 = gLn + (gs - gLn) / 3, gS = gs;
        std::vector<double> e1, e2;
        for (int Mu = -1; Mu <= 1; Mu += 2) for (int Ml = -1; Ml <= 1; Ml += 2) e1.push_back(gP1 * Mu / 2.0 - gS * Ml / 2.0);
        for (int Mu = -3; Mu <= 3; Mu += 2) for (int Ml = -1; Ml <= 1; Ml += 2) if (std::abs(Mu - Ml) <= 2) e2.push_back(gP3 * Mu / 2.0 - gS * Ml / 2.0);
        checkTrue("Zeeman: Na D1 with CODATA g_s: g_u M_u - g_l M_l (Lande)", sameSet(weakSlopes(d1->line, gs), e1, 1e-9));
        checkTrue("Zeeman: Na D2 with CODATA g_s: g_u M_u - g_l M_l (Lande)", sameSet(weakSlopes(d2->line, gs), e2, 1e-9));
        // the weak-field pattern is the 3j-squared one of the line's J pair (Wigner-Eckart): (3/2 1 1/2; -M_u q M_l)^2 (2 J_u + 1) ... normalised to the line
        double tot = 0;
        std::vector<double> w3;
        for (int Mu = -3; Mu <= 3; Mu += 2) for (int Ml = -1; Ml <= 1; Ml += 2) { const double t = atom::threeJ(3, 2, 1, -Mu, Mu - Ml, Ml); if (t != 0) { w3.push_back(t * t); tot += t * t; } }
        for (double& x : w3) x /= tot;
        checkTrue("Zeeman: Na D2 weak-field strengths = the 3j-squared pattern", sameSet(s2, w3, 1e-9));
    }
}

// the closed form of a 2P doublet (L = 1, S = 1/2) for every M, built here from H = E_c + zeta L.S + mu_B B (g_L L_z + g_s S_z) in the uncoupled basis |M_L, M_S> (0.96.0: g_L, until 0.95.0 1)
// (Bethe & Salpeter; the matrix of L.S from L_z S_z + (L+ S- + L- S+)/2): E_c = (2 E(3/2) + E(1/2)) / 3, zeta = 2 (E(3/2) - E(1/2)) / 3.
// M = +-3/2: one state, E_c + zeta/2 + b (+-1 +- g_s/2). M = +-1/2: the 2x2 block of |M -+ 1/2... > solved by the quadratic formula.
static void testZeemanDoublet() {
    namespace z = atom::zeeman;
    for (const int Z : {3, 11}) {
        const z::Term t = z::termAlkali(Z, Z == 3 ? 2 : 3, 1);
        checkTrue(std::string("Zeeman: ") + atom::name(Z) + " p term has a matrix", t.mode == z::Mode::Matrix && t.J2.size() == 2);
        if (t.mode != z::Mode::Matrix) continue;
        const double E1 = t.cm[0], E3 = t.cm[1], Ec = (2 * E3 + E1) / 3, zeta = 2 * (E3 - E1) / 3, gs = z::gsCodata(), gL = t.gL;
        double worst = 0;
        for (const double B : {0.0, 0.01, 0.3, 1.0, 3.0, 10.0, 100.0}) {
            const double b = B * z::lorentzCmPerT();
            std::map<int, std::vector<double>> want;
            want[3] = {Ec + zeta / 2 + b * (gL + gs / 2)};
            want[-3] = {Ec + zeta / 2 - b * (gL + gs / 2)};
            for (const int M2 : {1, -1}) {
                // states: a = |M_L = (M2 - 1)/2 ... with M_S = +1/2|, c = |M_L = (M2 + 1)/2, M_S = -1/2|
                const double MLa = (M2 - 1) / 2.0, MLc = (M2 + 1) / 2.0;
                const double Haa = Ec + zeta * MLa * 0.5 + b * (gL * MLa + gs * 0.5), Hcc = Ec - zeta * MLc * 0.5 + b * (gL * MLc - gs * 0.5);
                // <c| L+ S- |a> /2: L+ |M_L> = sqrt(2 - M_L (M_L + 1)) |M_L + 1> for L = 1; S- |+1/2> = |-1/2>
                const double Hac = zeta * 0.5 * std::sqrt(2 - MLa * (MLa + 1));
                const double m = (Haa + Hcc) / 2, d = std::sqrt((Haa - Hcc) * (Haa - Hcc) / 4 + Hac * Hac);
                want[M2] = {m - d, m + d};
            }
            std::map<int, std::vector<double>> got;
            for (const z::Sub& s : z::sublevels(t, B, gs)) got[s.M2].push_back(s.cm);
            for (auto& [M2, v] : got) {
                std::sort(v.begin(), v.end());
                if (v.size() != want[M2].size()) { worst = 1e9; continue; }
                for (std::size_t i = 0; i < v.size(); i++) worst = std::max(worst, std::fabs(v[i] - want[M2][i]));
            }
        }
        checkNum(std::string("Zeeman: ") + atom::name(Z) + " 2P doublet: the matrix against the analytic two-level formula, every M, 0 to 100 T (cm^-1)", worst, 0, 0, 1e-9);
    }
}

// weak-limit slopes = Lande, strong-limit slopes = g_L M_L + g_s M_S, the trace of each M block linear in B, no crossing of equal M (0.96.0: g_L = 1 - m_e / m_nuc of the term; until 0.95.0 1)
static void testZeemanLimits() {
    namespace z = atom::zeeman;
    std::vector<z::Term> terms = {z::termAlkali(3, 2, 1), z::termAlkali(11, 3, 1), z::termAlkali(11, 3, 2), z::termOneElectron(1, 0, 2, 1), z::termOneElectron(1, 0, 3, 2)};
    for (const auto& [Z, conf, term] : std::vector<std::tuple<int, std::string, std::string>>{{12, "3s.3p", "3P*"}, {20, "3p6.4s.4p", "3P*"}, {2, "1s.2p", "3P*"}, {30, "3d10.4s.4p", "3P*"}, {80, "5d10.6s.6p", "3P*"}}) {
        const int i = zt::levelIndex(Z, conf, term, 2);
        checkTrue("Zeeman: level " + std::string(atom::name(Z)) + " " + conf + " " + term + " J=1 is in the lab", i >= 0);
        if (i >= 0) terms.push_back(z::termLevel(Z, i));
    }
    const double gs = z::gsCodata();
    for (const z::Term& t : terms) {
        checkTrue("Zeeman: " + t.name + " has a matrix", t.mode == z::Mode::Matrix);
        if (t.mode != z::Mode::Matrix) continue;
        const double gap = z::gapCm(t), L = z::lorentzCmPerT();
        // weak: d E / d B at 0 (symmetric difference at mu_B B = 1e-5 gap: the cubic term is then 1e-10) = g_J M, g_J from Lande's formula written here
        const double Bw = 1e-5 * gap / L;
        const std::vector<z::Sub> p = z::sublevels(t, Bw, gs), m = z::sublevels(t, -Bw, gs);
        double worstW = 0;
        for (std::size_t k = 0; k < p.size(); k++) {
            const double J = p[k].J2 / 2.0, S = t.S2 / 2.0, gJ = t.gL + (gs - t.gL) * (J * (J + 1) + S * (S + 1) - t.L * (t.L + 1.0)) / (2 * J * (J + 1));
            worstW = std::max(worstW, std::fabs((p[k].rel - m[k].rel) / (2 * Bw * L) - gJ * p[k].M2 / 2.0));
        }
        checkNum("Zeeman: " + t.name + " weak-field slopes = Lande g_J M", worstW, 0, 0, 1e-9);
        // strong: slopes at mu_B B = 1e7 gap; per M block, sorted, against the sorted g_L M_L + g_s M_S
        const double B1 = 1e7 * gap / L, B2 = 2 * B1;
        const std::vector<z::Sub> a = z::sublevels(t, B1, gs), b = z::sublevels(t, B2, gs);
        std::map<int, std::vector<double>> got, want;
        for (std::size_t k = 0; k < a.size(); k++) got[a[k].M2].push_back((b[k].rel - a[k].rel) / ((B2 - B1) * L));
        for (int ML2 = -2 * t.L; ML2 <= 2 * t.L; ML2 += 2) for (int MS2 = -t.S2; MS2 <= t.S2; MS2 += 2) want[ML2 + MS2].push_back(t.gL * ML2 / 2.0 + gs * MS2 / 2.0);
        bool ok = got.size() == want.size();
        for (auto& [M2, v] : got) ok = ok && zt::sameSet(v, want[M2], 1e-6);
        checkTrue("Zeeman: " + t.name + " strong-field slopes = g_L M_L + g_s M_S", ok && t.gL < 1 && t.gL > 0.999);
        // trace of each M block = sum of E_J + b sum (g_L M_L + g_s M_S) over the block: linear in B (checked at 5 fields against that line)
        double worstT = 0;
        for (const double B : {0.0, 0.5 * gap / L, gap / L, 7 * gap / L, 300 * gap / L}) {
            std::map<int, double> tr, lin;
            double mean = 0;
            for (const double e : t.cm) mean += e / static_cast<double>(t.cm.size());
            for (const z::Sub& s : z::sublevels(t, B, gs)) tr[s.M2] += s.rel;
            for (std::size_t k = 0; k < t.J2.size(); k++) for (int M2 = -t.J2[k]; M2 <= t.J2[k]; M2 += 2) lin[M2] += t.cm[k] - mean;
            for (auto& [M2, w] : want) for (const double x : w) lin[M2] += B * L * x;
            for (auto& [M2, v] : tr) worstT = std::max(worstT, std::fabs(v - lin[M2]) / std::max(gap, std::fabs(lin[M2])));
        }
        checkNum("Zeeman: " + t.name + " the trace of each M block is linear in B", worstT, 0, 0, 1e-12);
        // no crossing: on a scan through the region between, the gap between neighbouring states of equal M never closes
        double minGap = INFINITY;
        for (int i = 0; i <= 200; i++) {
            const double B = std::pow(10.0, -3 + 6 * i / 200.0) * gap / L;
            std::map<int, std::vector<double>> e;
            for (const z::Sub& s : z::sublevels(t, B, gs)) e[s.M2].push_back(s.cm);
            for (auto& [M2, v] : e) { std::sort(v.begin(), v.end()); for (std::size_t k = 1; k < v.size(); k++) minGap = std::min(minGap, (v[k] - v[k - 1]) / gap); }
        }
        checkTrue("Zeeman: " + t.name + " levels of equal M never cross (smallest gap " + std::to_string(minGap) + " of the fine-structure gap)", minGap > 1e-3);
    }
}

// the strengths: sums at every B, the light the same in every direction, pi = sigma+ = sigma- for the whole line
static void testZeemanStrengths() {
    namespace z = atom::zeeman;
    const double gs = z::gsCodata();
    struct M { std::string what; std::vector<z::Line> lines; };
    std::vector<M> mult;
    const std::vector<z::LineRef> li = z::labLines(3, atom::defaultN(3), 1), na = z::labLines(11, atom::defaultN(11), 1);
    const auto pick = [](const std::vector<z::LineRef>& v, const std::string& up, const std::string& lo) {
        std::vector<z::Line> out;
        for (const z::LineRef& r : v) if (r.line.up.name == up && r.line.lo.name == lo) out.push_back(r.line);
        return out;
    };
    mult.push_back({"Li 2p -> 2s", pick(li, "2p ²P°", "2s ²S")});
    mult.push_back({"Na 3d -> 3p", pick(na, "3d ²D", "3p ²P°")});
    {
        const std::vector<z::LineRef> mg = z::labLines(12, atom::defaultN(12), 2);
        mult.push_back({"Mg 3s4s 3S -> 3s3p 3P", pick(mg, "3s4s ³S", "3s3p ³P°")});
    }
    for (const M& m : mult) {
        checkTrue("Zeeman: multiplet " + m.what + " has its lines", m.lines.size() >= 2);
        // every J pair of the two terms, also one with no line at zero field (it gains strength in the field)
        std::vector<z::Line> pairs;
        if (!m.lines.empty())
            for (const int ju : m.lines[0].up.J2) for (const int jl : m.lines[0].lo.J2) { z::Line x = m.lines[0]; x.upJ2 = ju; x.loJ2 = jl; pairs.push_back(x); }
        double raw0 = 0;
        for (const z::Line& L : pairs) raw0 += z::fieldFreeRaw(L);
        double worstSum = 0, worstIso = 0, worstPol = 0;
        for (const double B : {0.0, 0.1, 1.0, 3.0, 10.0, 100.0}) {
            double raw = 0, tq[3] = {0, 0, 0};
            std::vector<z::Component> all;
            for (const z::Line& L : pairs)
                for (const z::Component& c : z::rawComponents(L, B, gs)) { raw += c.raw; tq[c.q + 1] += c.raw; all.push_back(c); }
            worstSum = std::max(worstSum, std::fabs(raw / raw0 - 1));
            worstPol = std::max({worstPol, std::fabs(tq[0] / tq[1] - 1), std::fabs(tq[2] / tq[1] - 1)});
            double lo = INFINITY, hi = 0;
            for (int k = 0; k <= 18; k++) {
                const double th = atom::kPi * k / 18;
                double seen = 0;
                for (const z::Component& c : all) seen += c.raw * z::polWeight(c.q, th);
                lo = std::min(lo, seen); hi = std::max(hi, seen);
            }
            worstIso = std::max(worstIso, hi / lo - 1);
        }
        checkNum("Zeeman: " + m.what + ": the components of the multiplet (every J pair) sum to its field-free strength at every B", worstSum, 0, 0, 1e-12);
        checkNum("Zeeman: " + m.what + ": pi total = sigma+ total = sigma- total at every B", worstPol, 0, 0, 1e-12);
        checkNum("Zeeman: " + m.what + ": summed over polarisations the light is the same in every direction", worstIso, 0, 0, 1e-12);
        // each line alone: its components sum to its own field-free strength in a weak field (between the regimes strength moves between the lines of a multiplet)
        for (const z::Line& L : m.lines) {
            double s = 0;
            for (const z::Component& c : z::components(L, 1e-6, gs)) s += c.strength;
            checkNum("Zeeman: " + L.name + ": its components sum to 1 in a weak field", s, 1, 1e-9);
        }
    }
    // a singlet line (no mixing at all): its own components sum to its field-free strength at every B
    const std::vector<z::LineRef> cd = z::labLines(48, atom::defaultN(48), 2);
    for (const z::LineRef& r : cd)
        if (r.line.up.S2 == 0 && r.line.lo.S2 == 0 && r.line.up.mode != z::Mode::None) {
            double worst = 0;
            for (const double B : {0.0, 1.0, 100.0}) { double s = 0; for (const z::Component& c : z::components(r.line, B, gs)) s += c.strength; worst = std::max(worst, std::fabs(s - 1)); }
            checkNum("Zeeman: " + r.line.name + ": singlet line sums to 1 at every B", worst, 0, 0, 1e-12);
            break;
        }
    // field 0: every component sits on the line
    const std::vector<z::LineRef> na2 = z::labLines(11, atom::defaultN(11), 1);
    double w0 = 0;
    for (const z::LineRef& r : na2) for (const z::Component& c : z::components(r.line, 0, gs)) w0 = std::max(w0, std::fabs(c.shiftCm));
    checkNum("Zeeman: at B = 0 no component is shifted", w0, 0, 0, 1e-9);
}

// 0.96.0 (outside review of 28 Nov 2026, part F): the orbital g-factor with the nucleus's motion, g_L = 1 - m_e / m_nuc, and the Lande g_J with it,
// g_J = g_L [J(J+1) - S(S+1) + L(L+1)] / (2J(J+1)) + g_S [J(J+1) + S(S+1) - L(L+1)] / (2J(J+1)) (D. A. Steck, "Sodium D Line Data" rev. 2.3.4, Eqs. (19) and (21), citing
// Bethe & Salpeter 1957; extract native/data/jumps/steck/steck_sodium_d_line_2.3.4.extract.txt). Against: the formula by hand, Steck's Table 6 g_L of 23Na, and the measured g_J of
// Na 3P1/2 and 3P3/2 (Arimondo, Inguscio & Violino 1977, in Steck's Table 6). NIST ASD lists no Lande g for any atom lighter than K (data/jumps/README.txt); K's are checked below.
static void testZeemanReducedMass() {
    namespace z = atom::zeeman;
    const double gs = z::gsCodata();
    // by hand: hydrogen 2p 2P3/2, g_L = 1 - 1 / 1836.152673426 (CODATA 2022 m_p / m_e) with the proton as the nucleus (N = 0)
    const double gLH = 1 - 1 / 1836.152673426;
    checkNum("Lande: hydrogen's g_L = 1 - m_e / m_p", z::orbitalG(1, 0), gLH, 0, 1e-12);
    checkNum("Lande: hydrogen 2P3/2 by hand, g_L 2/3 + g_S 1/3", z::landeLS(1, 1, 3, gs, gLH), gLH * 2 / 3 + gs / 3, 0, 1e-15);
    checkNum("Lande: hydrogen 2P1/2 by hand, g_L 4/3 - g_S 1/3", z::landeLS(1, 1, 1, gs, gLH), gLH * 4 / 3 - gs / 3, 0, 1e-15);
    checkNum("Lande: an S level (L = 0) has no orbital part: g = g_S", z::landeLS(0, 1, 1, gs, gLH), gs, 0, 1e-15);
    checkNum("Lande: g_L = 1 gives the old formula 1 + (g_S - 1)[...] exactly (3P1)", z::landeLS(1, 2, 2, gs, 1.0), 1 + (gs - 1) * (2.0 + 2.0 - 2.0) / 4.0, 0, 1e-15);
    // Steck's Table 6: g_L of 23Na 0.999 976 13 (8 decimals printed)
    const double gLNa = z::orbitalG(11, 12);
    std::printf("Lande: 23Na g_L from the lab's nuclear mass %.10f (Steck 0.99997613)\n", gLNa);
    checkNum("Lande: 23Na g_L = 1 - m_e / m_nuc from the lab's nuclear mass, against Steck's Table 6 0.999 976 13", gLNa, 0.99997613, 0, 5e-9);
    // the measured g_J of sodium 3P (Arimondo et al. 1977 via Steck Table 6): the formula with g_L is inside the measurement's 1-sigma
    const double p12 = z::landeLS(1, 1, 1, gs, gLNa), p32 = z::landeLS(1, 1, 3, gs, gLNa);
    std::printf("Lande: Na 3P1/2 %.6f (measured 0.66581(12)), 3P3/2 %.6f (measured 1.33420(20))\n", p12, p32);
    checkTrue("Lande: Na 3P1/2 and 3P3/2 with g_L within the measured g_J's 1-sigma", std::fabs(p12 - 0.66581) <= 0.00012 && std::fabs(p32 - 1.33420) <= 0.00020);
    // the lab's terms carry it: one-electron atoms by their isotope, the others by the usual isotope
    checkNum("Lande: the hydrogen 2p term carries g_L of the proton", z::termOneElectron(1, 0, 2, 1).gL, gLH, 0, 1e-15);
    checkNum("Lande: the sodium 3p term carries g_L of 23Na", z::termAlkali(11, 3, 1).gL, z::orbitalG(11, atom::defaultN(11)), 0, 1e-15);
    checkTrue("Lande: deuterium's g_L is nearer 1 than hydrogen's (heavier nucleus)", z::orbitalG(1, 1) > gLH && z::orbitalG(1, 1) < 1);
    // the matrix with g_L: hydrogen 2p in a weak field (1e-7 T, far below the 0.365 cm^-1 fine-structure gap: the mixing's share of the slope is about 1e-7): each level's slope dE/dM is its g with g_L
    const z::Term h2p = z::termOneElectron(1, 0, 2, 1);
    const double B = 1e-7, b = B * z::lorentzCmPerT();
    double worst = 0;
    for (const z::Sub& u : z::sublevels(h2p, B, gs)) {
        if (u.M2 == 0) continue;
        double e0 = 0;
        for (const z::Sub& v : z::sublevels(h2p, 0, gs)) if (v.J2 == u.J2 && v.M2 == u.M2) e0 = v.rel;
        const double g = (u.rel - e0) / (b * u.M2 / 2.0);
        worst = std::max(worst, std::fabs(g - z::landeLS(1, 1, u.J2, gs, gLH)));
    }
    checkNum("Lande: hydrogen 2p's matrix in 1e-7 T: every sublevel moves with g_J (g_L = 1 - m_e/m_p) to 1e-6", worst, 0, 0, 1e-6);
    checkTrue("Lande: ... and the difference g_L makes (3.6e-4 for 2P3/2) is far above that", std::fabs(z::landeLS(1, 1, 3, gs, gLH) - z::landeLS(1, 1, 3, gs, 1.0)) > 3e-4);
}

// measured g: the table equals the saved NIST files (rows read here by eye from data/jumps/asd_g/<X>I_levels_g.tsv.gz, zcat); LS against measured within per-atom limits
static void testZeemanMeasuredG() {
    namespace z = atom::zeeman;
    checkNum("Zeeman: K 4s 2S1/2 measured g (ASD, L7185)", z::measuredGAlkali(19, 4, 0, 1), 2.002295, 0, 1e-12);
    checkNum("Zeeman: K 5d 2D5/2 measured g (ASD, L5770)", z::measuredGAlkali(19, 5, 2, 5), 1.2004, 0, 1e-12);
    checkStr("Zeeman: K 5d 2D5/2 reference", z::measuredGRef(19, 5, 2, 5, "", ""), "L5770");
    checkNum("Zeeman: Zn 4s4p 3P1 measured g", z::measuredGLevel(30, "3d10.4s.4p", "3P*", 2), 1.496, 0, 1e-12);
    checkNum("Zeeman: Hg 6s6p 3P1 measured g", z::measuredGLevel(80, "5d10.6s.6p", "3P*", 2), 1.486094, 0, 1e-12);
    checkNum("Zeeman: Hg 6s6p 1P1 measured g", z::measuredGLevel(80, "5d10.6s.6p", "1P*", 2), 1.01453, 0, 1e-12);
    checkNum("Zeeman: Ba 6s6p 1P1 measured g (ASD, L9224)", z::measuredGLevel(56, "6s.6p", "1P*", 2), 1.02, 0, 1e-12);
    checkTrue("Zeeman: Na 3p has no measured g in ASD", !std::isfinite(z::measuredGAlkali(11, 3, 1, 3)));
    // LS against measured over every level of the lab that has both (limits: the worst gap found, rounded up; REPORT.md names the largest)
    const auto worstOf = [&](int Z, std::string& who) {
        double w = 0;
        if (atom::alkaliJumps(Z)) {
            for (const atom::AlkLevel& a : atom::alkaliLevels(Z)) {
                const double g = z::measuredGAlkali(Z, a.n, a.l, a.j2);
                if (!std::isfinite(g)) continue;
                const double d = std::fabs(g - z::landeLS(a.l, 1, a.j2, z::gsCodata()));
                if (d > w) { w = d; who = std::to_string(a.n) + "spdf"[a.l] + " J2=" + std::to_string(a.j2); }
            }
        } else {
            const std::vector<atom::JLevel>& lv = atom::jumpLevels(Z);
            for (const atom::JLevel& a : lv) {
                int L = 0, S2 = 0;
                const double g = z::measuredGLevel(Z, a.conf, a.term, a.j2);
                if (!std::isfinite(g) || !z::parseLS(a.term, L, S2)) continue;
                const double d = std::fabs(g - z::landeLS(L, S2, a.j2, z::gsCodata()));
                if (d > w) { w = d; who = a.conf + " " + a.term + " J2=" + std::to_string(a.j2); }
            }
        }
        return w;
    };
    for (const auto& [Z, lim] : std::vector<std::pair<int, double>>{{19, 0.001}, {30, 0.006}, {56, 0.51}, {70, 0.37}, {80, 0.06}}) {
        std::string who;
        const double w = worstOf(Z, who);
        std::printf("  Zeeman: %s largest |g measured - g LS| = %.5f (%s)\n", atom::name(Z), w, who.c_str());
        checkTrue(std::string("Zeeman: ") + atom::name(Z) + " LS g against measured within " + std::to_string(lim), w > 0 && w <= lim);
    }
    // a level whose NIST term is not LS gets no splitting
    const std::vector<atom::JLevel>& hg = atom::jumpLevels(80);
    int jk = -1;
    for (std::size_t i = 0; i < hg.size() && jk < 0; i++) if (hg[i].term.find('[') != std::string::npos) jk = static_cast<int>(i);
    checkTrue("Zeeman: Hg has a level with a jK term in the lab", jk >= 0);
    if (jk >= 0) { const z::Term t = z::termLevel(80, jk); checkTrue("Zeeman: a jK level gets no splitting, with a reason", t.mode == z::Mode::None && !t.why.empty() && z::sublevels(t, 1, 2).empty()); }
    // a term with one J level splits with the measured g where NIST has one: Hg 6s6p 1P1
    const int p1 = zt::levelIndex(80, "5d10.6s.6p", "1P*", 2);
    if (p1 >= 0) {
        const z::Term t = z::termLevel(80, p1);
        checkTrue("Zeeman: Hg 1P1 splits linearly with its measured g", t.mode == z::Mode::Linear && t.gMeasured.size() == 1 && t.gMeasured[0] && std::fabs(t.gLin[0] - 1.01453) < 1e-12);
    }
}

// the one-electron atom: the term energies are the lab's Dirac levels with the Lamb shift; hydrogen 2p's fine-structure gap; the hyperfine field of 1s
static void testZeemanOneElectron() {
    namespace z = atom::zeeman;
    const z::Term t = z::termOneElectron(1, 0, 2, 1);
    checkNum("Zeeman: hydrogen 2p gap = the lab's Dirac+Lamb 2p3/2 - 2p1/2", z::gapCm(t),
             (atom::diracQedEV(1, 0, 2, 1, 3) - atom::diracQedEV(1, 0, 2, 1, 1)) * 1e7 / atom::kHcEVnm, 1e-10);   // (two energies of 27,000 cm^-1 differenced: 1e-11 rounding)
    // the field where mu_B B equals hydrogen's 1s hyperfine splitting in the lab: Delta E / mu_B
    checkNum("Zeeman: hydrogen's hyperfine field = the lab's 1s splitting / mu_B", z::hyperfineFieldT(1, 0, 0), atom::hfs::splittingEV(1, 0, 1, 0, 1) / (atom::kMuBJT / atom::kEV), 1e-12);
    checkTrue("Zeeman: the lab's Lyman alpha 2p3/2 -> 1s is offered", zt::findLine(z::labLines(1, 0, 0), "2p ²P°", "1s ²S", 3, 1) != nullptr);
}
