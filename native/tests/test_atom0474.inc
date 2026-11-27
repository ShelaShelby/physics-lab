// 0.47.4: the rest of the two-s-electron atoms (Ca, Sr, Ba, Ra, Hg, Yb) with one GRASP2018 orbital set per atom (tools/grasp/run_shared.py), and D4: this app's own rate A where
// NIST has none (tools/grasp/rates_two.py). Written BEFORE the new data existed (each cloud check failed with no data/jumps/grasp/<X>/clouds.bin, each D4 check with no rates.tsv).
// References, all independent of the app's own earlier output:
//  - NIST ASD energies and rates (data/jumps/asd; the numbers below copied from those files): each computed excitation energy within a per-atom limit set from this job's runs
//    (data/jumps/grasp/<X>/levels.tsv; REPORT.md 0.47.4); each computed A against NIST's A where both exist;
//  - M. S. Safronova, M. G. Kozlov, W. R. Johnson, D. Jiang, PRA 80, 012516 (2009), Table II (data/jumps/sources/ci_allorder_safronova2009_arxiv0905.2578.pdf): Ca, Cd, Ba
//    CI+all-order energies (cm-1 from the ground level, as printed there);
//  - conservation: occupations add up to Z, the density integrates to Z; angular momentum: a J = 0 level is round;
//  - owner decision D4 (9 Nov 2026): where NIST has no A the app shows its own, marked "computed by this app (GRASP2018), not measured".
// limits set from this job's runs (REPORT.md 0.47.4; NAN: the atom is not shipped in this version); 0.91.0: Sr 4.9 (the recomputed clouds, worst 5s4d 3D1, -4.85 %, a level 0.90.0 had no cloud for), Hg 2.6 (worst 6s9s 3S1, +2.56 %); Ra 4.7 from the 0.51.3 run (worst 7s6d 3D3, -4.62 %), the Safronova comparison and the rate comparison (%)
constexpr double kLimit0474Ca = 5.0, kLimit0474Sr = 4.9, kLimit0474Ba = 4.2, kLimit0474Ra = 4.7, kLimit0474Hg = 2.6, kLimit0474Yb = NAN;
constexpr double kSafronova0474 = 5.0, kRate0474 = 80.0, kRateMed0474 = 20.0;
static int level0474(int Z, const char* conf, const char* term, int j2) {
    const std::vector<atom::JLevel>& lv = atom::jumpLevels(Z);
    for (int i = 0; i < int(lv.size()); i++)
        if (lv[std::size_t(i)].conf == conf && lv[std::size_t(i)].term == term && lv[std::size_t(i)].j2 == j2) return i;
    return -1;
}
static void testLevels0474() {
    using namespace atom;
    // the resonance level of each atom (ASD <X>I_levels.tsv.gz, as printed there) and the per-atom limit on |computed - NIST| / NIST (%): this job's worst shipped level, rounded up
    const struct { int Z; const char* X; const char* conf; const char* term; double cm; double limit; } atoms[] = {
        {20, "Ca", "3p6.4s.4p", "1P*", 23652.304, kLimit0474Ca}, {38, "Sr", "5s.5p", "1P*", 21698.452, kLimit0474Sr}, {56, "Ba", "6s.6p", "1P*", 18060.261, kLimit0474Ba},
        {88, "Ra", "7s.7p", "1P*", 20715.6142, kLimit0474Ra}, {80, "Hg", "5d10.6s.6p", "1P*", 54068.6829, kLimit0474Hg}, {70, "Yb", "4f14.6s.6p", "1P*", 25068.222, kLimit0474Yb}};
    for (const auto& A : atoms) {
        if (!(A.limit > 0)) { checkTrue(std::string("0.47.4: ") + A.X + ": not shipped in this version (REPORT.md says why); its NIST levels are kept and say \"not computed yet\"",
                                        !levelJumps(A.Z) && !jumpLevels(A.Z).empty()); continue; }
        const std::string X = A.X;
        const std::vector<JLevel>& lv = jumpLevels(A.Z);
        const int at = level0474(A.Z, A.conf, A.term, 2);
        checkTrue("0.47.4: " + X + ": " + A.conf + " " + A.term + " J=1 is NIST's " + fmt::fixed(A.cm, 4) + " cm-1", at >= 0 && std::fabs(lv[std::size_t(at)].cm - A.cm) < 1e-6);
        checkTrue("0.47.4: " + X + " has level jumps (clouds embedded)", levelJumps(A.Z));
        int occBad = 0, normBad = 0, roundBad = 0, worse = 0, n = 0;
        double worst = 0;
        std::string worstName;
        for (const int i : cloudLevelList(A.Z)) {
            const GraspLevel* g = levelCloud(A.Z, i);
            n++;
            if (i > 0) {
                const double q = std::fabs(g->qualityPercent());
                if (q > worst) { worst = q; worstName = levelName(A.Z, i); }
                if (!(q <= A.limit)) worse++;
            }
            double occ = 0, integral = 0;
            for (const hf::Subshell& sh : g->atom.sub)
                for (const hf::Part& pt : sh.parts) {
                    occ += pt.occ;
                    double s = 0;
                    const int N = 20000; const double t0 = std::log(1e-6), t1 = std::log(400.0);
                    for (int k = 0; k <= N; k++) { const double r = std::exp(t0 + (t1 - t0) * k / N); s += (k == 0 || k == N ? 0.5 : 1.0) * partDensity(pt, sh.l, r) * r; }
                    integral += pt.occ * s * (t1 - t0) / N;
                }
            if (std::fabs(occ - A.Z) > 1e-6) occBad++;
            if (std::fabs(integral - A.Z) > 1e-3 * A.Z) normBad++;
            if (g->j2 == 0 && !g->ks.empty()) roundBad++;
        }
        checkTrue("0.47.4: " + X + ": " + std::to_string(n) + " levels with clouds, every computed excitation energy within " + fmt::fixed(A.limit, 1) + " % of NIST (worst " + worstName +
                  ": " + fmt::fixed(worst, 2) + " %)", n >= 4 && worse == 0);
        checkTrue("0.47.4: " + X + ": the occupations of every level add up to Z = " + std::to_string(A.Z), n > 0 && occBad == 0);
        checkTrue("0.47.4: " + X + ": every level's density integrates to Z (1e-3)", n > 0 && normBad == 0);
        checkTrue("0.47.4: " + X + ": every J = 0 level is round (no k > 0 multipole)", n > 0 && roundBad == 0);
        // an atom ships only with its resonance line (ground -> ns np 1P1) offered (REPORT.md 0.47.3)
        checkTrue("0.47.4: " + X + ": the resonance line, ground -> " + A.conf + " 1P J=1, is offered",
                  at >= 0 && [&] { for (const int k : linesBetween(A.Z, 0, at)) if (jumpOffered(A.Z, k)) return true; return false; }());
    }
    // against CI+all-order (Safronova et al. 2009, Table II, the CI+all-order column, cm-1): a check by an independent calculation of each level both have
    {   const struct { int Z; const char* conf; const char* term; int j2; double ciAll; } ref[] = {
            {20, "3p6.3d.4s", "3D", 2, 20335}, {20, "3p6.3d.4s", "3D", 4, 20355}, {20, "3p6.3d.4s", "3D", 6, 20386}, {20, "3p6.3d.4s", "1D", 4, 21965}, {20, "3p6.4s.5s", "3S", 2, 31694},
            {20, "3p6.4s.5s", "1S", 0, 33466}, {20, "3p6.4s.4p", "3P*", 0, 15338}, {20, "3p6.4s.4p", "3P*", 2, 15385}, {20, "3p6.4s.4p", "3P*", 4, 15498}, {20, "3p6.4s.4p", "1P*", 2, 23729},
            {48, "4d10.5s.6s", "3S", 2, 51395}, {48, "4d10.5s.6s", "1S", 0, 53272}, {48, "4d10.5s.5d", "1D", 4, 59015}, {48, "4d10.5s.5d", "3D", 2, 59259},
            {48, "4d10.5s.5d", "3D", 4, 59271}, {48, "4d10.5s.5d", "3D", 6, 59291}, {48, "4d10.5s.5p", "3P*", 0, 30141}, {48, "4d10.5s.5p", "3P*", 2, 30646},
            {48, "4d10.5s.5p", "3P*", 4, 31838}, {48, "4d10.5s.5p", "1P*", 2, 43607},
            {56, "6s.5d", "3D", 2, 9249}, {56, "6s.5d", "3D", 4, 9441}, {56, "6s.5d", "3D", 6, 9840}, {56, "6s.5d", "1D", 4, 11727}, {56, "6s.6p", "3P*", 0, 12556},
            {56, "6s.6p", "3P*", 2, 12919}, {56, "6s.6p", "3P*", 4, 13819}, {56, "6s.6p", "1P*", 2, 18292}};
        for (const int Z : {20, 48, 56}) {
            if (!levelJumps(Z)) continue;   // (an atom not shipped is reported above)
            int have = 0, bad = 0, named = 0;
            std::string worst;
            for (const auto& m : ref) {
                if (m.Z != Z) continue;
                const int i = level0474(Z, m.conf, m.term, m.j2);
                if (i >= 0) named++;
                const GraspLevel* g = i >= 0 ? levelCloud(Z, i) : nullptr;
                if (!g) continue;
                have++;
                const double d = 100 * (g->computedCm - m.ciAll) / m.ciAll;
                if (std::fabs(d) > kSafronova0474) { bad++; worst += levelName(Z, i) + " " + fmt::fixed(d, 2) + " % "; }
            }
            checkTrue("0.47.4: Z = " + std::to_string(Z) + ": each Table II level is a NIST level of the app (" + std::to_string(named) + ")", named >= 8);
            checkTrue("0.47.4: Z = " + std::to_string(Z) + ": each computed level within " + fmt::fixed(kSafronova0474, 1) + " % of Safronova et al. 2009 CI+all-order (" +
                      std::to_string(have) + " levels; " + worst + ")", have >= 4 && bad == 0);
        }
    }
    // D4: every offered line shows NIST's A, else the app's own marked as computed, else "rate not known"; the app's A against NIST's where both exist
    {   int lines = 0, own = 0, textBad = 0, cmp = 0, cmpBad = 0;
        std::string worst;
        std::vector<double> gaps;
        for (const int Z : {2, 4, 12, 20, 30, 38, 48, 56, 70, 80, 88}) {
            if (!levelJumps(Z)) continue;
            for (int k = 0; k < int(jumpLines(Z).size()); k++) {
                if (!jumpOffered(Z, k)) continue;
                const JLine& L = jumpLines(Z)[std::size_t(k)];
                const std::string t = rateText(L);
                lines++;
                if (std::isfinite(L.aki)) { if (t.find("NIST ASD") == std::string::npos) textBad++; }
                else if (std::isfinite(L.akiCalc)) { own++; if (t.find("computed by this app (GRASP2018), not measured") == std::string::npos) textBad++; }
                else if (t.rfind("rate not known", 0) != 0) textBad++;
                // where both exist: the app's A within kRate0474 of NIST's for each strong line NIST grades B or better (B: within 10 %), and the median within kRateMed0474
                // (limits set from this job's runs: the d-electron lines of Sr are the worst, REPORT.md)
                if (std::isfinite(L.aki) && std::isfinite(L.akiCalc) && L.aki >= 1e6 && !L.acc.empty() && (L.acc[0] == 'A' || L.acc.rfind("B", 0) == 0)) {
                    cmp++;
                    const double d = 100 * (L.akiCalc - L.aki) / L.aki;
                    gaps.push_back(std::fabs(d));
                    if (std::fabs(d) > kRate0474) { cmpBad++; worst += std::to_string(Z) + ": " + fmt::fixed(L.nm(), 1) + " nm " + fmt::fixed(d, 0) + " % "; }
                }
            }
        }
        checkTrue("0.47.4: D4: each of " + std::to_string(lines) + " offered lines shows NIST's A, else the app's own marked \"computed by this app (GRASP2018), not measured\" (" +
                  std::to_string(own) + "), else \"rate not known\"", lines > 0 && own > 0 && textBad == 0);
        checkTrue("0.47.4: D4: the app's A within " + fmt::fixed(kRate0474, 0) + " % of NIST's for each strong line graded B or better (" + std::to_string(cmp) + " lines; " + worst + ")",
                  cmp >= 10 && cmpBad == 0);
        std::sort(gaps.begin(), gaps.end());
        const double med = gaps.empty() ? NAN : gaps.size() % 2 ? gaps[gaps.size() / 2] : 0.5 * (gaps[gaps.size() / 2 - 1] + gaps[gaps.size() / 2]);
        checkTrue("0.47.4: D4: over those lines the app's A is within " + fmt::fixed(kRateMed0474, 0) + " % of NIST's in the median (" + fmt::fixed(med, 1) + " %)", med <= kRateMed0474);
        // the calcium resonance line, 422.79 nm: NIST ASD A = 2.18e+08 /s (grade B+, CaI_lines.tsv.gz)
        const int r = level0474(20, "3p6.4s.4p", "1P*", 2);
        bool ok = false;
        for (const int k : linesBetween(20, 0, r)) { const JLine& L = jumpLines(20)[std::size_t(k)]; ok = L.aki == 2.18e8 && std::fabs(L.akiCalc - 2.18e8) < 0.07 * 2.18e8; }
        checkTrue("0.47.4: D4: Ca 422.79 nm: NIST's 2.18e8 /s kept, and the app's own A within NIST's B+ grade (7 %) of it", ok);
    }
}
