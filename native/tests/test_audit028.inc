// 0.28: the small items of the 0.26 audit, each checked against an independent reference.
#include "atom/decay_chain.h"
static void testAudit028() {
    // ---- 1. melting points = the ITS-90 defining fixed points (BIPM; table as given by Wikipedia's 'International Temperature Scale of 1990', saved in
    //      data/its90): triple points of argon and mercury, melting point of gallium, freezing points of In, Sn, Zn, Al, Ag, Au, Cu. A melting point at
    //      1 atm differs from a triple point by under 0.01 K here (argon 83.81 vs 83.8058, mercury 234.321 vs 234.3156), hence the 0.01 K guard.
    //      BEFORE (the unsourced table): gallium 303 K, indium 430, tin 505, zinc 693, aluminium 933, silver 1235, gold 1337, copper 1358: rounded to 1 K.
    {
        struct Fp { int Z; double K; const char* what; };
        static const Fp fp[] = {{18, 83.8058, "Ar triple"}, {80, 234.3156, "Hg triple"}, {31, 302.9146, "Ga melting"}, {49, 429.7485, "In freezing"}, {50, 505.078, "Sn freezing"},
                                {30, 692.677, "Zn freezing"}, {13, 933.473, "Al freezing"}, {47, 1234.93, "Ag freezing"}, {79, 1337.33, "Au freezing"}, {29, 1357.77, "Cu freezing"}};
        int bad = 0;
        for (const Fp& f : fp) if (!(std::fabs(tempData(f.Z).melt - f.K) <= 0.01)) { bad++; std::printf("  ITS-90 %s: lab %.4f K, ITS-90 %.4f K\n", f.what, tempData(f.Z).melt, f.K); }
        checkNum("temperatures: melting points of the 10 elements that define ITS-90 within 0.01 K of its fixed points", bad, 0, 0);
        // the predicted melting points say so (Wikipedia, data/elements/wiki: At, Fr, Fm, Md, No, Lr); BEFORE: Md, No, Lr were shown as plain values
        int unsaid = 0;
        for (int Z : {85, 87, 100, 101, 102, 103}) if (factsLinesHtml(Z).find("predicted") == std::string::npos) unsaid++;
        checkNum("temperatures: the 6 predicted melting points are called predicted on the card", unsaid, 0, 0);
    }
    // ---- 2. B(E2; 0+ -> 2+) = ENSDF's B(E2)down in Weisskopf units (typed from data/be2/iaea_*_gammas.csv, column b_e2) x 5 x B_W(E2),
    //      B_W(E2) = (1/4 pi) (3/5)^2 (1.2 A^(1/3))^4 e^2 fm^4; the sign = minus the sign of the measured 2+ quadrupole moment (typed from
    //      data/quadrupole/elec_mom_recomm.csv). BEFORE: argon-36 was 0.01 e^2 b^2 (ENSDF: 0.0290) and drawn prolate (its Q(2+) = +0.11(6) b: oblate).
    {
        struct Be { int Z, A; double wu, q; };
        static const Be be[] = {{10, 20, 20.3, -0.23}, {12, 24, 21.07, -0.29}, {14, 28, 13.2, +0.16}, {16, 32, 9.9, -0.16}, {18, 36, 8.2, +0.11}, {62, 154, 176, -1.87},
                                {64, 158, 198, -2.01}, {68, 166, 217, -1.9}, {70, 172, 212, -2.22}, {74, 182, 136.1, -2.1}, {92, 238, 281, 0}};
        int bad = 0;
        for (const Be& b : be) {
            const double bw = 1 / (4 * kPi) * 0.36 * std::pow(1.2 * std::cbrt(double(b.A)), 4), want = 5 * b.wu * bw / 1e4, got = nucBE2Up(b.Z, b.A - b.Z);
            const double beta = nucBeta(b.Z, b.A - b.Z);
            if (!(std::fabs(got - want) <= 0.001 * want) || (b.q != 0 && beta * b.q >= 0)) { bad++; std::printf("  B(E2) Z=%d A=%d: lab %g, ENSDF %g e2b2; beta %g, Q(2+) %g\n", b.Z, b.A, got, want, beta, b.q); }
        }
        checkNum("B(E2): the 11 measured values equal ENSDF's within 0.1 % and each sign is opposite to the measured Q(2+) (U-238: none measured)", bad, 0, 0);
    }
    // ---- 3. NUBASE2020 (data/nubase/nubase_4.mas20.txt): the six '~' half-lives are shown as approximate values (the 0.26 audit listed them as shown
    //      "not measured"; checked in 0.28: the chart, chain and card already say "about", so this is a guard); Zn-54 decays by 2p in 87(7) % (BEFORE: 100 %).
    {
        struct Ap { int Z, N; double sec; };
        static const Ap ap[] = {{63, 99, 10}, {65, 79, 1}, {81, 106, 51}, {87, 119, 16}, {102, 160, 0.005}, {103, 159, 4 * 3600.0}};
        int bad = 0;
        for (const Ap& a : ap) {
            const ChainTiming t = chainTiming(a.Z, a.N);
            if (t.kind != ChainTime::Approximate || std::fabs(t.halfLife - a.sec) > 1e-9 * a.sec || chainHalfLifeWords(a.Z, a.N).find("about") == std::string::npos) {
                bad++; std::printf("  NUBASE ~ Z=%d N=%d: kind %d, %g s, '%s'\n", a.Z, a.N, int(t.kind), t.halfLife, chainHalfLifeWords(a.Z, a.N).c_str());
            }
        }
        checkNum("NUBASE: the six '~' half-lives are timed and shown as 'about' NUBASE's value", bad, 0, 0);
        const auto d = decInfo(30, 24);
        checkTrue("NUBASE: Zn-54 decays by two-proton emission in 87 % (NUBASE2020: 2p=87 7)", d && d->br.size() == 1 && d->br[0].first == "2p" && std::fabs(d->br[0].second - 0.87) < 1e-12);
    }
}
