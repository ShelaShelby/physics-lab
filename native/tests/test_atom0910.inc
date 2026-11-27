// 0.91.0, written before the code.
// Part B: the Slice's "Outer electron's dots" choice (True share, the default / Outer electron alone). Independent references:
//  - which electron a dot is: the subshell the sampler drew it from (the outer electron's subshell of an alkali, a level atom's outer subshells);
//  - its radius on the plane: a spherical density D(r) gives the plane (at |off|) the radius density D(r) / r from |off| on (hf.h, Slice notes), so the distribution
//    function of one subshell is F(r) = 1 - T(r) / T(|off|), T(r) = planeTail(subshell, r), and of several subshells the occupation-weighted mixture
//    sum q_i (T_i(off) - T_i(r)) / sum q_i T_i(off). Drawn round (the free atom: every direction alike) so the shape does not weigh the radii.
//    Statistic: the Kolmogorov-Smirnov distance of N dots below 1.63 / sqrt(N) (99 %; a fixed seed), as the HF cloud tests (test_hf.inc);
//  - with the choice off, the dots are the old build's: a hash of every dot's float bits after 400 steps, from 0.90.0 (ccea312) built on the build machine
//    (Na with its outer electron in 3d, then Mg in its level 1, in one fresh program; the cut 0.1, Vertical).
static std::uint64_t dotsHash0910(const AtomLab& L) {
    std::uint64_t h = 1469598103934665603ull;
    const long n = std::min<long>(L.s.k, L.cloudCap());
    for (long i = 0; i < 3 * n; i++) { std::uint32_t b; std::memcpy(&b, &L.s.pts[std::size_t(i)], 4); h = (h ^ b) * 1099511628211ull; }
    return h ^ std::uint64_t(L.s.k);
}
static double ks0910(std::vector<double> x, const std::function<double(double)>& F) {
    std::sort(x.begin(), x.end());
    const double N = double(x.size());
    double d = 0;
    for (std::size_t i = 0; i < x.size(); i++) { const double f = F(x[i]); d = std::max({d, std::fabs(f - double(i) / N), std::fabs(f - double(i + 1) / N)}); }
    return d;
}
static void testSliceAlone0910() {
    using namespace atom;
    // (1) choice off: the dots are unchanged (compared exactly with the old build; the shared random stream as a fresh program starts it, Na then Mg as there)
    seedRandom(0x9e3779b9u);
    for (const int Z : {11, 12}) {
        AtomLab L; L.nowMs = [] { return 0.0; }; L.p = AtomParams(); L.reducedMotion = true; L.restart();
        L.setParam("view", std::string("Slice")); L.setParam("Z", double(Z)); L.setParam("elec", std::string("All")); L.setParam("cut", 0.1); L.setParam("cutAxis", std::string("Vertical"));
        const bool ok = Z == 11 ? L.placeValence(3, 2) : L.placeLevel(1);
        for (int i = 0; i < 400; i++) L.step(1.0 / 240);
        const std::uint64_t want = Z == 11 ? 11471494278846976663ull : 3861988307822462633ull;
        const std::uint64_t got = dotsHash0910(L);
        std::printf("  0.91.0 Slice %s, True share: dots hash %llu (0.90.0: %llu)\n", Z == 11 ? "Na 3d" : "Mg level 1", (unsigned long long)got, (unsigned long long)want);
        checkTrue(std::string("0.91.0 Slice ") + (Z == 11 ? "Na 3d" : "Mg level 1") + ": the choice applies, True share is the default, and its dots are 0.90.0's exactly",
                  ok && L.sliceAloneChoice() && L.p.sdots == "True share" && !L.sliceAlone() && got == want);
    }
    // (2) choice on: every dot is the outer electron's; its radii follow its plane distribution
    struct Case { int Z; const char* name; int n, l, level; };
    for (const Case c : {Case{11, "Na 3d", 3, 2, -1}, Case{55, "Cs 6p", 6, 1, -1}, Case{12, "Mg level 1", 0, 0, 1}}) {
        AtomLab L; L.nowMs = [] { return 0.0; }; L.p = AtomParams(); L.reducedMotion = true; L.restart();
        L.setParam("view", std::string("Slice")); L.setParam("Z", double(c.Z)); L.setParam("elec", std::string("All")); L.setParam("cut", 0.1); L.setParam("cutAxis", std::string("Vertical"));
        const bool placed = c.level >= 0 ? L.placeLevel(c.level) : L.placeValence(c.n, c.l);
        L.setParam("sdots", std::string("Outer electron alone"));
        checkTrue(std::string("0.91.0 Slice ") + c.name + ": Outer electron alone is taken", placed && L.p.sdots == "Outer electron alone" && L.sliceAlone());
        bool said = false;
        for (const std::string& t : L.info()) said = said || t.find("only the outer electron's dots, all of them: its cloud alone, not the atom's") != std::string::npos;
        checkTrue(std::string("0.91.0 Slice ") + c.name + ": the readout says it shows one electron's cloud alone", said);
        const int N = 20000;
        std::vector<double> rad;
        const long notOuter = L.sliceOuterSample(N, 9101u, rad);
        checkTrue(std::string("0.91.0 Slice ") + c.name + ", lined up: every dot is the outer electron's (" + std::to_string(rad.size()) + " dots, " + std::to_string(notOuter) + " not)",
                  notOuter == 0 && int(rad.size()) == N);
        L.setRound(true);
        const long notOuter2 = L.sliceOuterSample(N, 9102u, rad);
        const hf::Atom* a = L.manyAtom();
        const double off = std::fabs(L.sliceOffNowForTests());
        std::vector<int> outer;                                              // the outer electron's subshells: the alkali's one singly filled subshell, or a level atom's set
        double cum = 0;
        if (c.level < 0) { for (std::size_t i = a->sub.size(); i-- > 0;) if (a->sub[i].occ == 1) { outer.push_back(int(i)); break; } }
        else {                                                               // (the same set as the lab's: outside the ground level's closed shells, above 0.02 electron)
            const GraspLevel* g0 = levelCloud(c.Z, 0);
            int ns = 0;
            for (const hf::Subshell& sh : g0->atom.sub) if (sh.l == 0 && !sh.lumped && sh.focc > 1.5) ns = std::max(ns, sh.n);
            for (int i = 0; i < int(a->sub.size()); i++) {
                const hf::Subshell& sh = a->sub[std::size_t(i)];
                if (sh.lumped || sh.focc < 0.02) continue;
                const bool core = !(sh.l == 0 && sh.n == ns) && std::any_of(g0->atom.sub.begin(), g0->atom.sub.end(), [&](const hf::Subshell& q) { return q.n == sh.n && q.l == sh.l && !q.lumped && q.focc > 1.5; });
                if (!core) outer.push_back(i);
            }
        }
        for (const int i : outer) cum += hf::electronsIn(a->sub[std::size_t(i)]) * hf::planeTail(a->sub[std::size_t(i)], off);
        const auto F = [&](double r) {
            double s = 0;
            for (const int i : outer) { const hf::Subshell& sh = a->sub[std::size_t(i)]; s += hf::electronsIn(sh) * (hf::planeTail(sh, off) - hf::planeTail(sh, std::max(r, off))); }
            return s / cum;
        };
        const double d = ks0910(rad, F), lim = 1.63 / std::sqrt(double(rad.size()));
        std::printf("  0.91.0 Slice %s, Outer electron alone, round: %zu dots, %zu outer subshells, KS distance %.5f (99 %% bound %.5f)\n", c.name, rad.size(), outer.size(), d, lim);
        checkTrue(std::string("0.91.0 Slice ") + c.name + ", round: every dot is the outer electron's and its radii follow its density on the plane (Kolmogorov-Smirnov " +
                  std::to_string(d) + " < " + std::to_string(lim) + ")", notOuter2 == 0 && int(rad.size()) == N && !outer.empty() && d < lim);
        // the full dot count: the Slice's ring fills with the outer electron's dots only
        L.setRound(false);
        for (int i = 0; i < 400; i++) L.step(1.0 / 240);
        checkTrue(std::string("0.91.0 Slice ") + c.name + ": the cloud keeps its full dot count (" + std::to_string(std::min<long>(L.s.k, L.cloudCap())) + " of " + std::to_string(L.cloudCap()) + ")",
                  L.s.k >= L.cloudCap());
        L.setParam("sdots", std::string("Nonsense"));
        checkTrue("0.91.0 an unknown choice is not taken", L.p.sdots == "Outer electron alone");
        L.setParam("view", std::string("Cloud 3D"));
        checkTrue(std::string("0.91.0 ") + c.name + ": outside the Slice the choice does not apply", !L.sliceAloneChoice() && !L.sliceAlone());
    }
    {   AtomLab L; L.p = AtomParams(); L.reducedMotion = true; L.restart(); L.setParam("view", std::string("Slice")); L.setParam("Z", 26.0); L.setParam("elec", std::string("All"));
        checkTrue("0.91.0 iron (no jumps), Slice: the choice does not apply", !L.sliceAloneChoice()); }
}

// Part A: every level a cloud. Independent references:
//  - NIST ASD energies (data/jumps/asd/<X>I_levels.tsv.gz, the numbers below copied from those files) and the lab's rule for its computed clouds: a level whose
//    calculated excitation energy is more than 5 % from NIST's gets no cloud (tools/grasp/multipoles_two.py QMAX; the same rule as every other computed level);
//  - orthogonality: two natural orbitals of one level with the same kappa (l and j) are orthogonal. GRASP's orbital file is orthonormal per kappa within 1e-6
//    (run_shared.ci_orbitals refuses more), and natural orbitals are an orthogonal rotation of it. The stored orbitals (300 points in ln r, single precision,
//    read with 4-point Lagrange) differ from GRASP's by the resampling (a cubic spline, multipoles.resample) and the reading: each at most about
//    (5/384 + 9/384) |D4| per point (the spline's and the 4-point rule's error constants, D4 the fourth difference of the stored samples in ln r), and
//    single precision (2^-24 each, so 2^-23 for an overlap by Cauchy-Schwarz). The bound on |S_ab| = |integral (Pa Pb + Qa Qb) dr|:
//        1e-6 + |e_a| + |e_b| + |e_a| |e_b| + 2^-23 + q,   |e|^2 = sum over the stored points of err_i^2 r_i h (the error's norm), q the quadrature's own change from N to N/2.
static double partErr0910(const atom::hf::Part& p) {
    const std::size_t n = p.tabP.size();
    double s = 0;
    for (std::size_t i = 0; i < n; i++) {
        double d = 0;
        for (std::size_t k = (i >= 2 ? i - 2 : 0); k + 4 < n && k <= i; k++)
            for (const std::vector<float>* f : {&p.tabP, &p.tabQ}) {
                const std::vector<float>& v = *f;
                d = std::max(d, std::fabs(double(v[k]) - 4.0 * v[k + 1] + 6.0 * v[k + 2] - 4.0 * v[k + 3] + v[k + 4]));
            }
        const double e = (5.0 / 384 + 9.0 / 384) * d, r = p.tabR0 * std::exp(p.tabH * double(i));
        s += e * e * r * p.tabH;
    }
    return std::sqrt(s);
}
static void testOrthogonal0910() {
    using namespace atom;
    for (const int Z : {20, 30, 38, 48, 56, 80, 88}) {
        long pairs = 0, bad = 0;
        double worst = 0, worstLim = 0;
        std::string where;
        for (const GraspLevel& g : graspAll(Z)) {
            std::vector<std::pair<const hf::Part*, int>> parts;
            for (const hf::Subshell& sh : g.atom.sub) { if (sh.lumped) continue; for (const hf::Part& p : sh.parts) parts.push_back({&p, sh.l}); }
            std::vector<double> err;
            for (const auto& pp : parts) err.push_back(partErr0910(*pp.first));
            const auto ov = [&](const hf::Part& a, const hf::Part& b, int l, int N) {
                const double t0 = std::log(1e-6), t1 = std::log(400.0), h = (t1 - t0) / N;
                double s = 0;
                for (int k = 0; k <= N; k++) {
                    const double r = std::exp(t0 + h * k), w = (k == 0 || k == N) ? 1 : (k % 2 ? 4 : 2);
                    s += w * r * (hf::tabValue(a.tabP, a.tabR0, a.tabH, l, r) * hf::tabValue(b.tabP, b.tabR0, b.tabH, l, r) + hf::tabValue(a.tabQ, a.tabR0, a.tabH, l, r) * hf::tabValue(b.tabQ, b.tabR0, b.tabH, l, r));
                }
                return s * h / 3;
            };
            for (std::size_t i = 0; i < parts.size(); i++)
                for (std::size_t j = 0; j < i; j++) {
                    if (parts[i].second != parts[j].second || parts[i].first->j2 != parts[j].first->j2) continue;
                    pairs++;
                    const double S = ov(*parts[i].first, *parts[j].first, parts[i].second, 8000), S2 = ov(*parts[i].first, *parts[j].first, parts[i].second, 4000);
                    const double lim = 1e-6 + err[i] + err[j] + err[i] * err[j] + std::ldexp(1.0, -23) + std::fabs(S - S2);
                    if (!(std::fabs(S) <= lim)) { bad++; if (where.size() < 200) where += " " + g.conf + " " + g.term + " J2=" + std::to_string(g.j2); }
                    if (std::fabs(S) / lim > worst / std::max(worstLim, 1e-300) || worstLim == 0) { worst = std::fabs(S); worstLim = lim; }
                }
        }
        std::printf("  0.91.0 %s: %ld pairs of natural orbitals with one kappa, %ld over their bound (the largest share of its bound: %.3g of %.3g)%s\n", name(Z), pairs, bad, worst, worstLim, where.c_str());
        checkTrue("0.91.0 " + std::string(name(Z)) + ": every level's natural orbitals of one kappa are orthogonal within the bound derived from the stored grid (" + std::to_string(pairs) + " pairs)", pairs > 0 && bad == 0);
    }
}

// Part A, the energies: each level the job lists (NIST ASD, data/jumps/asd/<X>I_levels.tsv.gz, the numbers copied from those files). A level marked made has a
// cloud whose computed excitation energy is within 5 % of NIST's (the lab's rule for every computed cloud, multipoles_two.py QMAX); 0.90.0 had none of them.
// A level marked not made keeps "no cloud computed" (REPORT.md and ROADMAP.md say why: 3 tries of the route, tools/grasp/run_two.py and run_shared.py).
static void testLevelClouds0910() {
    using namespace atom;
    struct Lv { int Z; const char* conf; const char* term; int J; double cm; bool made; };
    const Lv want[] = {
        // Zn 5s
        {30, "3d10.4s.5s", "3S", 1, 53672.2398, true}, {30, "3d10.4s.5s", "1S", 0, 55789.216, true},
        // Ca, Sr, Ba: the lowest d levels; Ca 4p2 1D
        {20, "3p6.3d.4s", "3D", 1, 20335.360, false}, {20, "3p6.3d.4s", "3D", 2, 20349.260, false}, {20, "3p6.3d.4s", "3D", 3, 20371.000, false},
        {20, "3p6.3d.4s", "1D", 2, 21849.634, true}, {20, "3p6.4p2", "1D", 2, 40719.847, false},
        {38, "5s.4d", "3D", 1, 18159.040, true}, {38, "5s.4d", "3D", 2, 18218.784, true}, {38, "5s.4d", "3D", 3, 18319.261, true}, {38, "5s.4d", "1D", 2, 20149.685, true},
        {56, "6s.5d", "3D", 1, 9033.966, false}, {56, "6s.5d", "3D", 2, 9215.501, false}, {56, "6s.5d", "3D", 3, 9596.533, false}, {56, "6s.5d", "1D", 2, 11395.350, false},
        // Ra 7s8s, 7s9s, 7s5f, 7s8p
        {88, "7s.8s", "3S", 1, 26754.02, false}, {88, "7s.9s", "3S", 1, 34475.74, false}, {88, "7s.5f", "3F*", 2, 35255.51, false}, {88, "7s.8p", "3P*", 0, 31085.80, false},
        // Cd 6d, 7d
        {48, "4d10.5s.6d", "1D", 2, 65134.783, false}, {48, "4d10.5s.6d", "3D", 1, 65353.372, false}, {48, "4d10.5s.6d", "3D", 2, 65358.881, false},
        {48, "4d10.5s.6d", "3D", 3, 65367.227, false}, {48, "4d10.5s.7d", "1D", 2, 67838.401, false},
        // Hg 7s, 7p, 6d
        {80, "5d10.6s.7s", "3S", 1, 62350.325, false}, {80, "5d10.6s.7s", "1S", 0, 63928.120, false},
        {80, "5d10.6s.7p", "3P*", 0, 69516.576, false}, {80, "5d10.6s.7p", "3P*", 1, 69661.803, false}, {80, "5d10.6s.7p", "3P*", 2, 71207.296, false},
        {80, "5d10.6s.7p", "1P*", 1, 71294.86, false}, {80, "5d10.6s.6d", "1D", 2, 71333.053, false}, {80, "5d10.6s.6d", "3D", 1, 71336.005, false},
        {80, "5d10.6s.6d", "3D", 2, 71396.073, false}, {80, "5d10.6s.6d", "3D", 3, 71431.180, false},
    };
    int made = 0;
    for (const Lv& w : want) {
        const std::string nm = std::string("0.91.0 ") + name(w.Z) + " " + w.conf + " " + w.term + " J=" + std::to_string(w.J);
        const int i = level0474(w.Z, w.conf, w.term, 2 * w.J);
        checkTrue(nm + " is NIST's " + fmt::fixed(w.cm, 3) + " cm-1", i >= 0 && std::fabs(jumpLevels(w.Z)[std::size_t(i)].cm - w.cm) < 1e-6);
        if (i < 0) continue;
        const GraspLevel* g = levelCloud(w.Z, i);
        if (w.made) {
            made++;
            const double q = g ? g->qualityPercent() : NAN;
            std::printf("  %s: computed %.1f cm-1, %+.3f %% from NIST\n", nm.c_str(), g ? g->computedCm : NAN, q);
            checkTrue(nm + ": has a cloud, its computed energy within 5 % of NIST's", g && std::fabs(q) <= 5.0 && std::fabs(g->nistCm - w.cm) < 1e-6);
        } else checkTrue(nm + ": not made (no cloud computed)", g == nullptr);
    }
    std::printf("  0.91.0: %d of the %zu listed levels have a cloud\n", made, sizeof(want) / sizeof(want[0]));
}

// Part D: hydrogen's recoil beyond the reduced mass (atom/dirac.h diracMassRy, diracRecoilEV). Independent reference: Yerokhin & Shabaev 2015's table, rows MASS
// and TOTAL of H (M/m = 1836.1526724) and He+ (7294.299529), with their alpha (1 / 137.035999074, CODATA 2010) (data/lamb/yerokhin_shabaev_2015_recoil.extract.txt,
// copied from the saved LaTeX source). Each bound: half a unit of the last printed digit + the bracketed uncertainty + double's rounding (1e-15 of the value).
// The app's levels (CODATA 2022 alpha and the app's nuclear mass) against TOTAL: the same, plus the change the two sets of constants make in DIRAC + MASS
// (both worked out with the formulas and each set), as testDiracRows086 does for DIRAC.
static void testRecoil0910() {
    using namespace atom;
    const double aP = 1 / 137.035999074;
    struct Row { int Z; double x; double m1s, m21, mfs; int d1s, d21, dfs; double u1s, u21, ufs; };
    // MASS: 1s, 2s - 1s, 2p3/2 - 2p1/2 (2p1/2 - 2s is 0 in both): the printed value, its decimals, its bracket in units of the last digit
    const Row rows[] = {{1, 1 / 1836.1526724, 0.0005443205755, -0.0004082386190, -0.000000001812648, 13, 13, 15, 2, 2, 0},
                        {2, 1 / 7294.299529, 0.000548298255, -0.000411216390, -0.00000000730118, 12, 12, 14, 0, 0, 0}};
    for (const Row& r : rows) {
        const double got[3] = {diracMassRy(r.Z, 1, 1, r.x, aP), diracMassRy(r.Z, 2, 1, r.x, aP) - diracMassRy(r.Z, 1, 1, r.x, aP), diracMassRy(r.Z, 2, 3, r.x, aP) - diracMassRy(r.Z, 2, 1, r.x, aP)};
        const double want[3] = {r.m1s, r.m21, r.mfs};
        const int dec[3] = {r.d1s, r.d21, r.dfs};
        const double unc[3] = {r.u1s, r.u21, r.ufs};
        const char* nm[3] = {"1s", "2s - 1s", "2p3/2 - 2p1/2"};
        for (int k = 0; k < 3; k++) {
            const double bound = (0.5 + unc[k]) * std::pow(10.0, -dec[k]) + 1e-15 * std::fabs(want[k]);
            std::printf("  0.91.0 YS15 MASS Z=%d %s: %.15e (printed %.15e, bound %.1e)\n", r.Z, nm[k], got[k], want[k], bound);
            checkTrue("0.91.0 YS15 MASS row, Z = " + std::to_string(r.Z) + ", " + nm[k] + ": Eq. (3) gives the printed value within its digits", std::fabs(got[k] - want[k]) <= bound);
        }
        checkTrue("0.91.0 YS15 MASS row, Z = " + std::to_string(r.Z) + ": 2p1/2 - 2s is 0 (Eq. (3) depends on n and j only)", diracMassRy(r.Z, 2, 1, r.x, aP) == diracMassRy(r.Z, 2, 1, r.x, aP));
    }
    // the app's levels against TOTAL (H): 2s - 1s, 2p1/2 - 2s, 2p3/2 - 2p1/2
    {
        const double xP = 1 / 1836.1526724, xA = 1 / nuclearMassMe(1, 0);
        const auto lvl = [](int n, int l, int j2) { return diracQedEV(1, 0, n, l, j2) / kRydInf; };
        const auto dm = [&](int n, int j2, double x, double a) { return diracInfiniteRy(1, n, j2, a) + diracMassRy(1, n, j2, x, a); };
        struct Iv { const char* name; int nA, lA, jA, nB, lB, jB; double total; int dec; double unc; };
        const Iv iv[] = {{"2s - 1s", 2, 0, 1, 1, 0, 1, 0.7495987475687, 13, 43}, {"2p1/2 - 2s", 2, 1, 1, 2, 0, 1, -0.00000032154868, 14, 54},
                         {"2p3/2 - 2p1/2 (the 2p gap)", 2, 1, 3, 2, 1, 1, 0.000003334215341, 15, 33}};
        for (const Iv& v : iv) {
            const double got = lvl(v.nA, v.lA, v.jA) - lvl(v.nB, v.lB, v.jB);
            const double consts = std::fabs((dm(v.nA, v.jA, xA, kAlpha) - dm(v.nB, v.jB, xA, kAlpha)) - (dm(v.nA, v.jA, xP, aP) - dm(v.nB, v.jB, xP, aP)));
            const double bound = (0.5 + v.unc) * std::pow(10.0, -v.dec) + consts + 1e-15 * std::fabs(v.total) + 1e-15;
            std::printf("  0.91.0 YS15 TOTAL H %s: the app %.15e Ry (printed %.15e; bound %.2e, of it the constants %.2e)\n", v.name, got, v.total, bound, consts);
            checkTrue(std::string("0.91.0 hydrogen ") + v.name + ": the app's level difference is YS15's TOTAL within its printed digits and the constants' change", std::fabs(got - v.total) <= bound);
        }
        checkTrue("0.91.0 the recoil beyond the reduced mass is 0 for n_r = 0 (1s, 2p3/2) and positive for 2s, 2p1/2", diracRecoilEV(1, 0, 1, 1) == 0 && diracRecoilEV(1, 0, 2, 3) == 0 && diracRecoilEV(1, 0, 2, 1) > 0);
    }
}
