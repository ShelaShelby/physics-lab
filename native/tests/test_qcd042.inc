// 0.42: the nucleus, the nucleon and the quarks against published values. Written BEFORE the code. Every reference number below is typed from the saved source
// (native/data/qcd/, see its README.txt), never taken from the app.

// ---- the nucleon: CODATA 2022, PDG 2024 ----
static void testNucleonConstants() {
    // CODATA 2022 (data/qcd/nucleon/codata2022_rp.html): r_p = 0.840 75(64) fm; PDG 2024 average (pdg2024_list_p.pdf): 0.8409 +- 0.0004 fm
    checkNum("proton rms charge radius = CODATA 2022 0.84075 fm", atom::kProtonRmsFm, 0.84075, 0);
    checkTrue("... and inside the combined uncertainty of the PDG 2024 average 0.8409(4)", std::fabs(atom::kProtonRmsFm - 0.8409) <= 0.0004 + 0.00064);
    checkNum("the drawn nucleon radius is that value (was 0.84, rounded)", atom::kNucleonR, 0.84075, 0);
    // PDG 2024 neutron listing (pdg2024_list_n.pdf): <r_n^2> = -0.1155 +- 0.0017 fm^2
    checkNum("neutron mean-square charge radius = PDG 2024 -0.1155 fm^2", atom::kNeutronMsFm2, -0.1155, 0);
    // Darwin-Foldy term 3/(4 M^2) (hbar = c = 1) with the CODATA 2022 proton mass 938.272 089 43 MeV and hbar c = 197.326 980 4 MeV fm (CODATA 2022);
    // Ong, Berengut and Flambaum, PRC 82, 014320 (2010), eq. (8), print it as 0.033 fm^2.
    const double df = 0.75 * std::pow(197.3269804 / 938.27208943, 2);
    checkNum("Darwin-Foldy term 3/4 (hbar / m_p c)^2", atom::kDarwinFoldyFm2, df, 1e-12);
    checkNum("... = 0.033 fm^2 as Ong et al. 2010 print it (two digits)", atom::kDarwinFoldyFm2, 0.033, 0, 0.0005);
    // magnetic moments, CODATA 2022 (codata2022_mupsmun.html, codata2022_munsmun.html), in nuclear magnetons
    checkNum("proton magnetic moment 2.792 847 344 63 mu_N (CODATA 2022)", atom::kMuP, 2.79284734463, 0);
    checkNum("neutron magnetic moment -1.913 042 76 mu_N (CODATA 2022)", atom::kMuN, -1.91304276, 0);
    // PDG 2024 quark model review, section 15.8: measured mu_n / mu_p = -0.6849793 +- 0.0000003 (the simple quark model predicts -2/3)
    checkNum("mu_n / mu_p from the two CODATA values = PDG's measured -0.6849793", atom::kMuN / atom::kMuP, -0.6849793, 4e-7);
}

// ---- the nucleus: point-proton radius (Ong et al. 2010 eq. 8; Filin et al. 2020 eq. 2 is the same split for the deuteron) ----
static void testPointProton() {
    // <r^2>_ch = <r^2>_pp + <R_p^2> + (N/Z) <R_n^2> + 3/(4M^2) (spin-orbit term left out). Pb-208: IAEA radius 5.5012 fm (data/radii/charge_radii.csv, row 82,Pb,126)
    const double pb = 5.5012 * 5.5012 - 0.84075 * 0.84075 - (126.0 / 82.0) * (-0.1155) - 0.75 * std::pow(197.3269804 / 938.27208943, 2);
    checkNum("Pb-208: point-proton mean square radius = rms_ch^2 - r_p^2 - (N/Z) <r_n^2> - Darwin-Foldy", pointProtonMs(82, 126), pb, 1e-9);
    const double ca = 3.4771 * 3.4771 - 0.84075 * 0.84075 - (28.0 / 20.0) * (-0.1155) - 0.75 * std::pow(197.3269804 / 938.27208943, 2);
    checkNum("Ca-48 (IAEA 3.4771 fm): the same split", pointProtonMs(20, 28), ca, 1e-9);
    // the deuteron (0.46: CODATA 2022 2.12778 fm, review A9; was IAEA 2.1421 fm, row 1,H,1): its proton sits at d/2 from the centre, so (d/2)^2 = r_d^2 - r_p^2 - r_n^2 - 3/(4M^2) (Filin et al., PRL 124, 082501 (2020), eq. 2)
    const double h = std::sqrt(2.12778 * 2.12778 - 0.84075 * 0.84075 + 0.1155 - 0.75 * std::pow(197.3269804 / 938.27208943, 2));
    seedRandom(5);                                   // 0.46.5: the distance is drawn from the wave function; its mean square is scaled to this value
    double m = 0; const int n = 4000;
    for (int i = 0; i < n; i++) { const auto v = buildNucleus(1, 1); const double d = std::hypot(std::hypot(v[0].x - v[1].x, v[0].y - v[1].y), v[0].z - v[1].z) / kFmLab; m += d * d / 4; }
    checkNum("deuteron: mean (half separation)^2 from the measured charge radius with both nucleons' sizes and Darwin-Foldy", m / n, h * h, 0.05);
}

// ---- the neutron skin: Trzcinska et al., PRL 87, 082501 (2001), as quoted by Centelles et al., PRL 102, 122502 (2009): S = (0.90 +- 0.15) I + (-0.03 +- 0.02) fm,
// I = (N - Z) / A. Pb-208: 0.160 fm (PREX-II measured 0.283 +- 0.071, Adhikari et al. 2021); Ca-48: 0.120 fm (CREX 0.121 +- 0.026 +- 0.024, Adhikari et al. 2022).
static void testNeutronSkin(double tol) {
    checkNum("skin rule, Pb-208: 0.90 x 44/208 - 0.03", neutronSkinFm(82, 126), 0.90 * 44.0 / 208.0 - 0.03, 1e-12);
    checkNum("skin rule, Ca-48: 0.90 x 8/48 - 0.03", neutronSkinFm(20, 28), 0.90 * 8.0 / 48.0 - 0.03, 1e-12);
    for (const auto& zn : {std::pair<int, int>{82, 126}, {20, 28}}) {
        double sp = 0, sn = 0; long np = 0, nn = 0;
        seedRandom(4242);
        for (int b = 0; b < (zn.first > 50 ? 5 : 20); b++) {
            const auto v = buildNucleus(zn.first, zn.second);
            double cx = 0, cy = 0, cz = 0;
            for (const Nucleon& q : v) { cx += q.x; cy += q.y; cz += q.z; }
            cx /= v.size(); cy /= v.size(); cz /= v.size();
            for (const Nucleon& q : v) {
                const double x = (q.x - cx) / kFmLab, y = (q.y - cy) / kFmLab, z = (q.z - cz) / kFmLab, r2 = x * x + y * y + z * z;
                if (q.isP) { sp += r2; np++; } else { sn += r2; nn++; }
            }
        }
        const double skin = std::sqrt(sn / nn) - std::sqrt(sp / np), want = 0.90 * (zn.second - zn.first) / double(zn.first + zn.second) - 0.03;
        std::printf("neutron skin of the drawn %s: %.4f fm (rule %.4f)\n", at(zn.first, zn.second).c_str(), skin, want);
        checkNum("the drawn snapshots' neutron rms minus proton rms = the antiprotonic-atom rule: " + at(zn.first, zn.second), skin, want, 0, tol);
    }
}

// ---- the gluon field's shape: Bissey et al., PRD 76, 114512 (2007) (data/qcd/quarks/bissey_2007_hep-lat0606016.pdf): quarks about 0.3 fm or less from the centre:
// a round bubble, strongest in the middle; about 0.4 fm: a filled triangle; more than 0.5 fm: Y-shaped flux tubes ----
// (0.43: the lattice-shaped gluon field of 0.42 is gone with the old Accurate drawing: testGluonShape removed, see git.)

// ---- the words: what the Quarks layer says ----
static std::string labWords(const char* qstyle, const char* detail, bool proton) {
    AtomLab lab; lab.nowMs = [] { return 0.0; }; lab.p = oneParams(); lab.restart();
    lab.setParam("qstyle", std::string(qstyle)); lab.setParam("detail", std::string(detail));
    lab.setParam("Z", proton ? 1.0 : 1.0); lab.setParam("N", proton ? 0.0 : 1.0);   // H-1 (one proton) or H-2 (nucleon 1 is the neutron after the flip below)
    lab.ensureNucleus();
    lab.p.zoom = 6; lab.s.layer = "Quarks"; lab.s.focus = 0;
    if (!lab.s.nuc.empty()) lab.s.nuc[0].isP = proton;
    lab.s.more = true;
    std::string all = lab.hint() + "\n";
    for (const std::string& l : lab.info()) all += l + "\n";
    return all;
}
static void testQuarkWords() {
    // 0.43: Cinematic is gone and Classic is Simple (the owner, 8 Nov 2026); the Accurate quarks are clouds (test_qcd043.inc). The checks of the removed Cinematic words and of the
    // removed lattice-shaped field went with them; the neutron's words are checked in Accurate.
    const std::string acc = labWords("Accurate", "+ Sea", true), neu = labWords("Accurate", "+ Gluons", false), sim = labWords("Simple", "+ Sea", true);
    checkTrue("no claim that each quark has a colour that keeps changing (a colour singlet has no quark of definite colour: PDG quark model review)", !has(acc, "each keeps changing colour") && !has(sim, "each keeps changing colour"));
    checkTrue("Accurate says a nucleon is a colour singlet", has(acc, "colour singlet"));
    checkTrue("Accurate draws no colour charge at all (no quark has one: the owner, 8 Nov 2026: a realistic model has no cartoon) and says it colours by flavour", has(acc, "No colour charge is drawn") && has(acc, "by flavour") && !has(acc, "the swapping you see"));
    checkTrue("quark size: ZEUS 2016 limit 0.43e-16 cm = 4.3e-19 m is named", has(acc, "4.3×10⁻¹⁹ m") && has(sim, "4.3×10⁻¹⁹ m"));
    checkTrue("no 'below about 10^-20 m' as if it were the only limit", !has(acc, "below about 10⁻²⁰ m"));
    checkTrue("Simple: the lattice shapes are named, with the Y only beyond 0.5 fm (Bissey et al. 2007)", has(sim, "filled triangle") && has(sim, "0.5 fm"));
    checkTrue("the proton line: CODATA 2022 radius 0.8408 fm and magnetic moment +2.793", has(acc, "0.8408 fm") && has(acc, "+2.793"));
    checkTrue("the neutron line: <r^2> = -0.1155 fm^2 and the chiral-EFT -0.106 disagreement, magnetic moment -1.913", has(neu, "−0.1155 fm²") && has(neu, "−0.106") && has(neu, "−1.913"));
    checkTrue("the neutron's charge picture is frame dependent: positive core / negative edge (Kelly) and negative centre (Miller) both named", has(neu, "positive core") && has(neu, "negative centre"));
    checkTrue("+ Sea: anti-down over anti-up with SeaQuest's measured range", has(acc, "1.3") && has(acc, "1.6") && has(acc, "SeaQuest"));
    checkTrue("+ Sea: strange pairs left out and why (still debated)", has(acc, "strange") && has(sim, "strange"));
    checkTrue("spin: quark spins carry about a third (COMPASS 0.26-0.36)", has(acc, "0.26") && has(acc, "0.36"));
    checkTrue("Fermi motion: up to about 0.27 c, not 'move at about a quarter of light speed' for all", !has(acc, "move at about a quarter of light speed") && has(labWords("Accurate", "Valence", true) + labWords("Simple", "Valence", true), "0.27"));
}

static void testQcd042() {
    testNucleonConstants();
    testPointProton();
    testNeutronSkin(1e-9);   // exact by construction (atom_core.cpp scales the neutrons to the skin): a rounding-level guard
    testQuarkWords();
}
