// 0.31: hyperfine structure (atom/hfs.h). References: the measured ground-state splittings of hydrogen and deuterium (M. I. Eides, H. Grotch, V. A. Shelyuto,
// Phys. Rep. 342 (2001), eqs. (406) and (410), saved in data/moments/), the IAEA table as saved (data/moments/magn_mom_recomm.csv), and the textbook level rule
// (the interval rule E_F - E_{F-1} = A F; the F levels' weighted mean (2F + 1) E_F is 0).
// 0.33: the relativistic (Breit) factor for s levels. References: Eides et al. eq. (274) and its expansion eq. (275); V. M. Shabaev, M. Tomaselli, T. Kuehl,
// A. N. Artemyev, V. A. Yerokhin, "Ground state hyperfine splitting of high Z hydrogenlike ions", Phys. Rev. A 56, 252 (1997), arXiv physics/9706031 (saved in
// data/moments/): its eq. (2) A(aZ) = 1 / (gamma (2 gamma - 1)), Table 3 (A for 15 ions, 4 decimals), Table 4 (holmium-165: point-nucleus values and the measurement).
static void testHfs() {
    using namespace atom::hfs;
    const double kHmeasMHz = 1420.4057517667, kDmeasMHz = 327.3843525219;
    const double h = splittingEV(1, 0, 1, 0, 1) * kEVtoMHz, d = splittingEV(1, 1, 1, 0, 1) * kEVtoMHz;
    std::printf("hyperfine: hydrogen 1s %.4f MHz (measured %.4f, %+.2e), deuterium 1s %.4f MHz (measured %.4f, %+.2e)\n", h, kHmeasMHz, h / kHmeasMHz - 1, d, kDmeasMHz, d / kDmeasMHz - 1);
    // 0.33: the lab now has the Breit factor (+1.5 (Z alpha)^2 = +8.0e-5 for hydrogen; 0.31 left it out and its comment said +1.3e-4, which was wrong). What is left
    // out, from Eides et al. Table XIX: the other nonrecoil (QED binding) corrections, the total nonrecoil factor 1.001 136 089 6 over the lab's (1 + a_e) x Breit;
    // the proton's size (Zemach) -42.4e-6; the recoil +5.22e-6 and +0.4585e-6. Put in, they must bring the lab onto the measured line within 1e-5 (the table's own
    // total is 4.5e-6 below the measurement: the proton's polarizability is not in it).
    {
        const double rest = 1.0011360896 / ((1 + kAe) * relativisticFactorS(1, 1)) * (1 - 42.4e-6 + 5.22e-6 + 0.4585e-6);
        std::printf("hyperfine: hydrogen with Eides' left-out terms %.4f MHz (%+.2e)\n", h * rest, h * rest / kHmeasMHz - 1);
        checkNum("hyperfine: hydrogen with the left-out terms of Eides et al. Table XIX is the measured line within 1e-5", h * rest, kHmeasMHz, 1e-5);
        checkNum("hyperfine: hydrogen's 21 cm line within 1.5e-4 of the measured 1,420.405 751 MHz (the left-out terms above sum to -1.4e-4)", h, kHmeasMHz, 1.5e-4);
    }
    // deuterium: the same, plus the deuteron's structure (a loosely bound proton and neutron), known to move it by about 1.4e-4 (Eides et al. section XL.B)
    checkNum("hyperfine: deuterium's 92 cm line within 3e-4 of the measured 327.384 353 MHz", d, kDmeasMHz, 3e-4);
    checkNum("hyperfine: hydrogen's line is 21.1 cm long", 299792458.0 / (h * 1e6) * 100, 21.106, 2e-4);
    // the table, as saved: hydrogen's proton +2.792847351, spin 1/2; deuteron +0.857438231, spin 1; helium-3 -2.12762531, spin 1/2
    const Nucleus* p = find(1, 1); const Nucleus* dn = find(1, 2); const Nucleus* he3 = find(2, 3);
    checkTrue("hyperfine: the proton, deuteron and helium-3 are in the table", p && dn && he3);
    if (p && dn && he3) {
        checkTrue("hyperfine: proton +2.792847351 nm, I 1/2", p->mu == 2.792847351 && p->twoI == 1 && p->sign == 1);
        checkTrue("hyperfine: deuteron +0.857438231 nm, I 1", dn->mu == 0.857438231 && dn->twoI == 2);
        checkTrue("hyperfine: helium-3 -2.12762531 nm, I 1/2", he3->mu == -2.12762531 && he3->twoI == 1);
    }
    checkTrue("hyperfine: an even-even nucleus (carbon-12) has no hyperfine structure", levels(6, 6, 1, 0, 1).empty() && splittingEV(6, 6, 2, 1, 3) == 0);
    checkTrue("hyperfine: 914 ground states from the table (9 ambiguous ones left out)", tableSize() == 914);
    // helium-3's negative moment turns the order over: in 3He+ the F = 1 level is BELOW F = 0
    {
        const auto v = levels(2, 1, 1, 0, 1);
        checkTrue("hyperfine: 3He+ 1s: F = 1 lies below F = 0 (the moment is negative)", v.size() == 2 && v[0].twoF == 2 && v[1].twoF == 0);
        const auto w = levels(1, 0, 1, 0, 1);
        checkTrue("hyperfine: hydrogen 1s: F = 0 lies below F = 1", w.size() == 2 && w[0].twoF == 0 && w[1].twoF == 2);
    }
    // the interval rule and the centre of gravity, for every nucleus in the table, on 2p(3/2) and 3d(5/2)
    int badRule = 0, badMean = 0, n = 0;
    for (int Z = 1; Z <= 118; Z++) for (int N = 0; N <= 180; N++) {
        if (!find(Z, Z + N)) continue;
        for (const auto& [nn, l, twoJ] : {std::tuple{2, 1, 3}, std::tuple{3, 2, 5}}) {
            const double a = aConstEV(Z, N, nn, l, twoJ);
            const auto v = levels(Z, N, nn, l, twoJ);
            if (a == 0 || v.empty()) continue;
            n++;
            auto byF = v; std::sort(byF.begin(), byF.end(), [](const atom::hfs::Level& x, const atom::hfs::Level& y) { return x.twoF < y.twoF; });
            double mean = 0, wsum = 0, scale = 0;
            for (std::size_t k = 0; k < byF.size(); k++) {
                mean += (byF[k].twoF + 1) * byF[k].eEV; wsum += byF[k].twoF + 1; scale = std::max(scale, std::fabs(byF[k].eEV));
                if (k && std::fabs((byF[k].eEV - byF[k - 1].eEV) - a * byF[k].twoF / 2.0) > 1e-12 * std::fabs(a) * byF[k].twoF) badRule++;
            }
            if (std::fabs(mean / wsum) > 1e-12 * scale) badMean++;
        }
    }
    checkTrue("hyperfine: the interval rule E_F - E_(F-1) = A F holds (" + std::to_string(n) + " level sets, " + std::to_string(badRule) + " wrong)", n > 1000 && badRule == 0);
    checkTrue("hyperfine: the F levels' (2F + 1)-weighted mean is the unsplit level (" + std::to_string(badMean) + " wrong)", badMean == 0);
    // Z^3 / n^3, times the Breit factor of each level (0.33): 2s / 1s = (1/8) F(2) / F(1)
    checkNum("hyperfine: the 2s splitting is 1/8 of the 1s one times the ratio of their Breit factors", splittingEV(1, 0, 2, 0, 1) / splittingEV(1, 0, 1, 0, 1), 0.125 * relativisticFactorS(1, 2) / relativisticFactorS(1, 1), 1e-12);
    // 0.33: the Breit factor. (a) Its expansion, Eides et al. eq. (275): 1 + (11 n^2 + 9 n - 11) / (6 n^2) (Z alpha)^2; at Z = 1 the next term is (Z alpha)^2 smaller
    for (int nn = 1; nn <= 3; nn++) {
        const double x = std::pow(atom::kAlpha, 2), c = (11.0 * nn * nn + 9 * nn - 11) / (6.0 * nn * nn);
        checkNum("hyperfine: Breit factor of " + std::to_string(nn) + "s at Z = 1 is 1 + (11n^2 + 9n - 11)/(6n^2) (Z alpha)^2 (Eides eq. 275)", (relativisticFactorS(1, nn) - 1) / x, c, 1e-3);
    }
    // (b) 1s: Shabaev et al. Table 3, A(aZ) to 4 decimals, In (Z 49) to Bi (Z 83)
    {
        const std::pair<int, double> kA[] = {{49, 1.2340}, {51, 1.2582}, {53, 1.2843}, {55, 1.3125}, {57, 1.3430}, {59, 1.3761}, {63, 1.4509}, {65, 1.4933},
                                             {67, 1.5395}, {71, 1.6453}, {73, 1.7061}, {75, 1.7731}, {81, 2.0217}, {82, 2.0718}, {83, 2.1250}};
        double worst = 0;
        for (const auto& [Z, a] : kA) worst = std::max(worst, std::fabs(relativisticFactorS(Z, 1) - a));
        std::printf("hyperfine: Breit factor vs Shabaev et al. Table 3 (15 ions): worst difference %.1e\n", worst);
        checkTrue("hyperfine: the 1s Breit factor is Shabaev et al.'s A(aZ) for all 15 ions of their Table 3 (within the 4th decimal)", worst <= 0.00006);
    }
    // (c) holmium-165 (Z 67), the hydrogen-like ion 165Ho66+. Shabaev et al. Table 4, for mu = 4.132 nuclear magnetons: non-relativistic 1.4945 eV, relativistic
    // point nucleus 2.3007 eV; their formula has no reduced mass and no a_e (theirs is in the QED term), so the lab's is divided by (1 + a_e) (m_r/m_e)^3 here.
    if (const Nucleus* ho = find(67, 165)) {
        const double mr = 1 / (1 + 1 / atom::nuclearMassMe(67, 98)), scale = 4.132 / ho->mu / ((1 + kAe) * mr * mr * mr);
        const double rel = splittingEV(67, 98, 1, 0, 1) * scale;
        std::printf("hyperfine: 165Ho66+ (mu %.4f): lab %.4f eV; Shabaev point nucleus %.4f eV (with mu 4.132: lab %.4f)\n", ho->mu, splittingEV(67, 98, 1, 0, 1), 2.3007, rel);
        checkNum("hyperfine: 165Ho66+ 1s, point nucleus, mu 4.132: Shabaev et al. Table 4's 2.3007 eV", rel, 2.3007, 1e-4);
        checkNum("hyperfine: 165Ho66+ 1s without the Breit factor: Shabaev et al.'s non-relativistic 1.4945 eV", rel / relativisticFactorS(67, 1), 1.4945, 1e-4);
        // the measurement (Crespo Lopez-Urrutia et al. 1996, as Table 4 gives it): 2.1645(6) eV, 572.79 nm. The lab is above it by what it leaves out: the
        // nucleus's charge size -0.1050 eV, Bohr-Weisskopf -0.0195, QED beyond a_e (Table 4: vacuum polarization +0.0094, self energy -0.0197, minus the a_e part)
        const double lab = splittingEV(67, 98, 1, 0, 1), over = lab / 2.1645 - 1;
        std::printf("hyperfine: 165Ho66+ lab %.4f eV vs measured 2.1645 eV: %+.3f (non-relativistic before 0.33: %+.3f)\n", lab, over, lab / relativisticFactorS(67, 1) / 2.1645 - 1);
        checkTrue("hyperfine: 165Ho66+ lab line above the measured one by less than Shabaev's left-out terms (+6.2 %) plus 1 %", over > 0 && over < (2.3007 - 2.1659) / 2.1659 + 0.01);
        // 0.46.3 (outside review): the ground-level text quotes this gap, as "the measured line is N % below this lab's", N = 1 - measured / lab, to 1 decimal
        char pct[16]; std::snprintf(pct, sizeof pct, "%.1f %%", 100 * (1 - 2.1645 / lab));
        const std::string tx = groundText(67, 98);
        checkTrue(std::string("hyperfine: the text's holmium gap is the lab's own (") + pct + ")", tx.find(std::string("measured holmium-165 line is ") + pct + " below this lab") != std::string::npos);
        // and the size numbers are Shabaev et al.'s Table 3: delta 0.0456 (Ho) and 0.1111 (Bi), epsilon 0.0089 and 0.0118
        checkTrue("hyperfine: the text's size effects are Shabaev et al.'s Table 3 (4.6 / 11.1 % charge, 0.9 / 1.2 % magnetism)", tx.find("4.6 % for holmium-165") != std::string::npos && tx.find("11.1 % for bismuth-209") != std::string::npos && tx.find("0.9 % and 1.2 %") != std::string::npos);
    } else checkTrue("hyperfine: holmium-165 is in the table", false);
    // the Spectrum tab's words
    const std::string th = groundText(1, 0), tc = groundText(6, 6), tu = groundText(1, 6);
    std::printf("hyperfine text, hydrogen: %s\n", th.c_str());
    checkTrue("hyperfine text: hydrogen, F = 0 and F = 1, 1,420 MHz, 21.1 cm", th.find("F = 0 and F = 1") != std::string::npos && th.find("1420.6 MHz") != std::string::npos && th.find("21.1 cm") != std::string::npos);
    checkTrue("hyperfine text: carbon-12 has spin 0", tc.find("spin 0") != std::string::npos);
    checkTrue("hyperfine text: a nucleus the table does not have (hydrogen-7) says so", tu.find("no measured value") != std::string::npos);
}
