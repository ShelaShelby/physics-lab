// 0.46.1: the outside review of the Atom Lab. Written before running the code. Reference numbers are typed from the saved sources (native/data/asd/asd_lines.tsv: H I; data/asd/alkali/NaI_lines.tsv;
// data/twophoton: Jentschura 2004) or are textbook formulas (the dipole sum rule; (l^2 - m^2) / ((2l + 1)(2l - 1)) for the m = 0 -> 0 share). Never taken from the app.

static bool has461(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

static void testEmitOdds0461() {
    using namespace atom;
    // the angular sum rule: for one orbital A of l, the shares of every orientation B of one shell l' = l +- 1 add up to l_> / (2l + 1)
    {   double worst = 0; int sums = 0;
        const auto& all = orbitals();
        for (int a = 0; a < int(all.size()); a++)
            for (int n2 = 1; n2 <= 6; n2++)
                for (const int l2 : {all[std::size_t(a)].l - 1, all[std::size_t(a)].l + 1}) {
                    if (l2 < 0 || l2 >= n2) continue;
                    double sum = 0; bool any = false;
                    for (int b = 0; b < int(all.size()); b++) if (all[std::size_t(b)].n == n2 && all[std::size_t(b)].l == l2) { sum += dipoleAngularSq(a, b); any = true; }
                    if (!any) continue;
                    const int l = all[std::size_t(a)].l;
                    worst = std::max(worst, std::fabs(sum - double(std::max(l, l2)) / (2 * l + 1)));
                    sums++;
                }
        checkTrue("0.46.1: dipole sum rule, the orientation shares of a shell add up to l_>/(2l+1) for every orbital (" + std::to_string(sums) + " sums, worst off " + std::to_string(worst) + ")", sums > 100 && worst < 1e-4); }
    // 3d z2 -> 2p: m = 0 -> 0 share (l^2 - m^2) / ((2l + 1)(2l - 1)) = 4/15 of the sum 2/5, i.e. 2/3 to 2pz, 1/6 each to 2px and 2py
    {   const int d = orbIndex("3d|z2");
        const double sz = dipoleAngularSq(d, orbIndex("2p|z")), sx = dipoleAngularSq(d, orbIndex("2p|x")), sy = dipoleAngularSq(d, orbIndex("2p|y")), t = sx + sy + sz;
        checkNum("0.46.1: 3d z2 -> 2p z takes 2/3 of the 3d z2 -> 2p decays", sz / t, 2.0 / 3, 1e-4);
        checkNum("0.46.1: 3d z2 -> 2p x takes 1/6", sx / t, 1.0 / 6, 1e-4); }
    // the term rates from NIST ASD's H I components (asd_lines.tsv): 3p -> 1s 1.6725e8 (both j), 3p -> 2s 2.2448e7 (j 3/2), 2.2449e7 (j 1/2)
    checkNum("0.46.1: H 3p -> 1s rate 1.6725e8 /s (NIST)", hydrogenTermRate(3, 1, 1, 0), 1.6725e8, 1e-9);
    checkNum("0.46.1: H 3p -> 2s rate (4 x 2.2448e7 + 2 x 2.2449e7) / 6 /s (NIST)", hydrogenTermRate(3, 1, 2, 0), (4 * 2.2448e7 + 2 * 2.2449e7) / 6, 1e-9);
    checkNum("0.46.1: H 3d -> 2p rate (6 x 6.4651e7 + 4 x (1.0775e7 + 5.3877e7)) / 10 /s (NIST)", hydrogenTermRate(3, 2, 2, 1), (6 * 6.4651e7 + 4 * (1.0775e7 + 5.3877e7)) / 10, 1e-9);
    {   int missing = 0;   // every dipole route of the lab's levels (n <= 6) has a rate
        for (int n = 2; n <= 6; n++) for (int l = 0; l < n; l++) for (int m = 1; m < n; m++) for (const int l2 : {l - 1, l + 1}) if (l2 >= 0 && l2 < m && !std::isfinite(hydrogenTermRate(n, l, m, l2))) missing++;
        checkTrue("0.46.1: NIST ASD gives a rate for every dipole route between the lab's levels n <= 6 (" + std::to_string(missing) + " missing)", missing == 0); }
    // the lab: H 3p z decays to 1s with odds 1.6725e8 / (1.6725e8 + 2.24483e7) = 88.17 %
    const double want = 1.6725e8 / (1.6725e8 + (4 * 2.2448e7 + 2 * 2.2449e7) / 6);
    {   AtomLab L; L.nowMs = [] { return 0.0; }; L.p = oneParams(); L.reducedMotion = true; L.restart(); L.setParam("view", std::string("Cloud")); L.setParam("Z", 1.0); L.setParam("orb", std::string("3p")); L.setParam("pAx", std::string("z"));
        const std::vector<int> t = L.targets(-1);
        double r1 = 0, r2 = 0;
        for (const int k : t) (orbitals()[std::size_t(k)].n == 1 ? r1 : r2) += L.emitRate(k);
        checkTrue("0.46.1: H 3p z can decay to 1s and 2s only", t.size() == 2);
        checkNum("0.46.1: H 3p -> 1s odds 88.17 % (NIST rates), was 50 %", r1 / (r1 + r2), want, 1e-6);
        int to1s = 0; const int N = 4000;
        seedRandom(461);
        for (int i = 0; i < N; i++) { double sh = 0; if (orbitals()[std::size_t(L.pickEmit(t, &sh))].n == 1) to1s++; }
        checkNum("0.46.1: 4,000 draws land on 1s about 88 % of the time (binomial sigma 0.5 %)", double(to1s) / N, want, 0.02);
        L.emit();
        checkTrue("0.46.1: the last-photon line gives the odds of the way taken (" + L.s.last + ")", has461(L.s.last, "% of decays (NIST rates)")); }
    // He+ has the same odds (rates scale as Z^4)
    {   AtomLab L; L.nowMs = [] { return 0.0; }; L.p = oneParams(); L.restart(); L.setParam("view", std::string("Cloud")); L.setParam("Z", 2.0); L.setParam("orb", std::string("3p")); L.setParam("pAx", std::string("z"));
        double r1 = 0, r2 = 0; for (const int k : L.targets(-1)) (orbitals()[std::size_t(k)].n == 1 ? r1 : r2) += L.emitRate(k);
        checkNum("0.46.1: He+ 3p -> 1s has hydrogen's odds", r1 / (r1 + r2), want, 1e-6); }
    // the Orbit view: n = 3 -> 1 against 3 -> 2, the l-averaged rates (2l + 1) / n^2
    {   AtomLab L; L.nowMs = [] { return 0.0; }; L.p = oneParams(); L.restart(); L.setParam("view", std::string("Orbit")); L.setParam("Z", 1.0); L.setParam("n", 3.0);
        const double a31 = 3.0 / 9 * 1.6725e8;
        const double a32 = (1 * (2.1046e6 + 4.2097e6) /* 3s -> 2p: one upper j (1/2), A summed over the lower j */ + 3 * (4 * 2.2448e7 + 2 * 2.2449e7) / 6 + 5 * (6 * 6.4651e7 + 4 * (1.0775e7 + 5.3877e7)) / 10) / 9;
        checkNum("0.46.1: Orbit view, n = 3 -> 1 rate (3/9) A(3p -> 1s)", L.emitRate(1), a31, 1e-6);
        checkNum("0.46.1: Orbit view, n = 3 -> 2 rate, l-averaged", L.emitRate(2), a32, 1e-6); }
    // sodium 4p (every electron): 3s, 4s and 3d with NIST's Na I rates: 4p -> 3s (4 x 2.75e6 + 2 x 2.73e6) / 6, -> 4s (4 x 6.64e6 + 2 x 6.62e6) / 6, -> 3d (4 x (1.43e5 + 1.59e4) + 2 x 1.57e5) / 6
    {   AtomLab L; L.nowMs = [] { return 0.0; }; L.p = AtomParams(); L.reducedMotion = true; L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", 11.0);
        L.setValence(4, 1);
        const double w3s = (4 * 2.75e6 + 2 * 2.73e6) / 6, w4s = (4 * 6.64e6 + 2 * 6.62e6) / 6, w3d = (4 * (1.43e5 + 1.59e4) + 2 * 1.57e5) / 6;
        double g3s = 0, g4s = 0, g3d = 0;
        const auto orbs = alkaliOrbitals(11);
        for (const int k : L.targets(-1)) { const AlkOrbital o = orbs[std::size_t(k)]; if (o.n == 3 && o.l == 0) g3s = L.emitRate(k); if (o.n == 4 && o.l == 0) g4s = L.emitRate(k); if (o.n == 3 && o.l == 2) g3d = L.emitRate(k); }
        checkTrue("0.46.1: sodium 4p decays to 3s, 4s and 3d", L.targets(-1).size() == 3);
        checkNum("0.46.1: sodium 4p -> 3s rate (NIST Na I)", g3s, w3s, 1e-9);
        checkNum("0.46.1: sodium 4p -> 4s rate (NIST Na I)", g4s, w4s, 1e-9);
        checkNum("0.46.1: sodium 4p -> 3d rate (NIST Na I)", g3d, w3d, 1e-9);
        std::string all; for (const std::string& l : L.info()) all += l + "\n";
        checkTrue("0.46.1 (0.48: with the factor): the sodium readout says its outer electron's dots are drawn 3.5x denser (a quarter of the dots plus its 1-in-11 share of the rest, against 1/11)", has461(all, "its dots drawn 3.5× denser than its true share")); }
}

static void testTexts0461() {
    using namespace atom;
    // relativity: no text calls the lab non-relativistic where its model or readout is relativistic
    {   AtomLab L; L.nowMs = [] { return 0.0; }; L.p = AtomParams(); L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", 79.0);
        checkTrue("0.46.1: gold, every electron, Dirac-Hartree-Fock: the hint does not say the lab is non-relativistic", L.manyOn() && !has461(L.hint(), "non-relativistic physics") && has461(L.hint(), "Dirac-Hartree-Fock model includes this"));
        L.setParam("mrel", std::string("Non-relativistic"));
        checkTrue("0.46.1: gold, Model Non-relativistic: the hint says this model is non-relativistic", has461(L.hint(), "this model is non-relativistic")); }
    {   AtomLab L; L.nowMs = [] { return 0.0; }; L.p = oneParams(); L.restart(); L.setParam("view", std::string("Cloud")); L.setParam("Z", 92.0); L.setParam("dirac", std::string("On")); L.s.more = true;
        std::string all; for (const std::string& l : L.info()) all += l + "\n";
        checkTrue("0.46.1: U91+ with the relativity setting on: no line calls the readout's energy non-relativistic", !has461(all, "this readout's energy") && has461(all, "the energy over the picture is the Dirac value from cobalt"));   // 0.49.3: the words changed with the Cloud model setting
        checkTrue("0.46.1: U91+: the 1s speed line compares Dirac with non-relativistic, not 'in this lab'", has461(all, "Dirac 1s energy") && !has461(all, " in this lab\n")); }
    checkTrue("0.46.1: the Dirac setting's words no longer say it leaves 'the readouts' alone", !has461(diracExplain(), "the readouts") && !has461(diracOnNote(26), "the energy readouts keep"));
    // 2s: it decays by two photons; the message says so
    {   AtomLab L; L.nowMs = [] { return 0.0; }; L.p = oneParams(); L.restart(); L.setParam("view", std::string("Cloud")); L.setParam("Z", 1.0); L.setParam("orb", std::string("2s"));
        L.emit();
        checkTrue("0.46.1: 2s Emit: says it decays by two photons, with the sourced lifetime (" + L.s.msg + ")", has461(L.s.msg, "two photons") && has461(L.s.msg, fmtLife(0.1215)) && !has461(L.s.msg, "stays excited")); }
    // one-electron ions: heavy ones are made in accelerators and ion traps
    checkTrue("0.46.1: the spectrum note of U91+ does not say it glows in stars", !has461(spectrumNote(92, defaultN(92)), "glow in very hot plasmas and stars") && has461(spectrumNote(92, defaultN(92)), "accelerators"));
    checkTrue("0.46.1: the spectrum note of He+ keeps the hot plasmas", has461(spectrumNote(2, defaultN(2)), "hot plasmas"));
    // gas speeds: two-atom gases fly as pairs: v = sqrt(3 k T / m), m = 2 x 14.003074 u (N-14), k = 1.380649e-23 J/K, u = 1.66053906892e-27 kg (CODATA 2022)
    {   const std::string n = tempSpeedHtml(7, 293.15), he = tempSpeedHtml(2, 293.15);
        const double v = std::sqrt(3 * 1.380649e-23 * 293.15 / (2 * 14 * 1.66053906892e-27));   // (the lab uses Z + N mass numbers, as its atom speed always did)
        checkTrue("0.46.1: nitrogen gas: the speed of N2 molecules (" + n + ")", has461(n, "molecule speed") && has461(n, "N₂") && has461(n, std::to_string(int(std::lround(v)))));
        checkTrue("0.46.1: helium gas stays an atom speed", has461(he, "Typical atom speed")); }
    // 0.46.3 (outside review: the pairs stayed pairs up to 6,000 K). Reference: NIST-JANAF log Kf of H(g) (data/janaf/H-001.txt): 3000 K -0.801, 4000 K 0.203; X2 <-> 2X at 1 atm:
    // Kp = Kf^2, alpha = sqrt(Kp / (Kp + 4 x 1.01325)). At 3000 K: Kp = 10^-1.602 = 0.0250, alpha = 0.0783; at 4000 K: Kp = 10^0.406 = 2.547, alpha = 0.621.
    {   checkTrue("0.46.3: H2 at 3000 K, 1 atm: 7.8 % split (JANAF)", std::fabs(pairSplitFraction(1, 3000) - 0.0783) < 0.0005);
        checkTrue("0.46.3: H2 at 4000 K, 1 atm: 62 % split (JANAF)", std::fabs(pairSplitFraction(1, 4000) - 0.621) < 0.001);
        checkTrue("0.46.3: helium has no pairs", pairSplitFraction(2, 4000) < 0);
        checkTrue("0.46.3: N2 at 293 K does not split", pairSplitFraction(7, 293.15) < 1e-30);
        const std::string h3 = tempSpeedHtml(1, 3000), h5 = tempSpeedHtml(1, 5000), i2 = tempSpeedHtml(53, 1500);
        checkTrue("0.46.3: H2 at 3000 K is still mostly pairs and says 8 % have split (" + h3 + ")", has461(h3, "molecule speed") && has461(h3, "8 % of them have split"));
        const double vH = std::sqrt(3 * 1.380649e-23 * 5000 / (1.67262192595e-27 + 9.1093837139e-31));   // 0.46.4 (deliberate): the H atom's real mass, m_p + m_e (CODATA 2022), not 1 u
        char kms[16]; std::snprintf(kms, sizeof kms, "%.2f km/s", vH / 1000);
        checkTrue("0.46.3: H at 5000 K: mostly atoms, the atom speed (" + h5 + ")", has461(h5, "Typical atom speed") && has461(h5, kms) && has461(h5, "have split into single atoms"));
        // iodine splits early: JANAF log Kf of I(g) at 1500 K is read from data/janaf/I-001.txt by the generated table; the check is only that over half has split by 1500 K
        checkTrue("0.46.3: I2 at 1500 K: over half split, so atoms (" + i2 + ")", pairSplitFraction(53, 1500) > 0.5 && has461(i2, "Typical atom speed")); }
}
