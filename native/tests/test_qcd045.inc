// 0.45: the outside review of 0.44's quarks (8 Nov 2026): each finding that is physics or a number gets a check here, written BEFORE the fix.
// Reference numbers are typed from the saved papers (native/data/qcd/), never taken from the app.

static void testReviewWidths() {
    using namespace atom;
    // finding 5: above the largest measured x the sea and gluon widths shrink as (1 - x)^2 (Diehl and Kroll section 7.3: <b^2> ~ (1 - x)^2 as x -> 1);
    // the measured points stay exactly where the papers put them (COMPASS 0.58 fm at x = 0.028; FSW 0.28 fm^2 at x = 0.1)
    checkNum("sea at the COMPASS point still 2 x 4.3 GeV^-2 (hbar c)^2", pkMeanB2(Pk::Ubar, 0.028), 2 * 4.3 * kHc2, 1e-12);
    checkNum("gluons at x = 0.1 still 0.28 fm^2 (FSW 2005)", pkMeanB2(Pk::G, 0.1), 0.28, 1e-12);
    checkNum("gluons at x = 0.5: 0.28 (0.5 / 0.9)^2 fm^2", pkMeanB2(Pk::G, 0.5), 0.28 * (0.5 / 0.9) * (0.5 / 0.9), 1e-12);
    checkNum("sea at x = 0.3: shrunk by (0.7 / 0.972)^2", pkMeanB2(Pk::Dbar, 0.3), 2 * 4.3 * kHc2 * (0.7 / 0.972) * (0.7 / 0.972), 1e-12);
    checkTrue("no jump at the last measured points (continuous within 1e-6)", std::fabs(pkMeanB2(Pk::G, 0.1 + 1e-9) - 0.28) < 1e-6 && std::fabs(pkMeanB2(Pk::Usea, 0.028 + 1e-9) - 2 * 4.3 * kHc2) < 1e-6);
    checkTrue("fast gluons and sea partons sit at the centre as x -> 1", pkMeanB2(Pk::G, 0.999) < 1e-4 && pkMeanB2(Pk::Dsea, 0.999) < 1e-4);
    // finding 1: the size of the clouds the dots draw is momentum weighted; printed here, and the texts must give these numbers (testReviewWords)
    std::printf("dots' rms distance from the centre (momentum weighted): u_v %.3f, d_v %.3f, gluons %.3f, sea %.3f fm\n", std::sqrt(pkDotsMeanB2(Pk::Uv)), std::sqrt(pkDotsMeanB2(Pk::Dv)),
                std::sqrt(pkDotsMeanB2(Pk::G)), std::sqrt(pkDotsMeanB2(Pk::Dbar)));
    checkTrue("the drawn up cloud is narrower than the number-counted one (Diehl-Kroll table 15: 0.627 fm): fast quarks carry the momentum", std::sqrt(pkDotsMeanB2(Pk::Uv)) < 0.5);
}

static void testReviewGlow() {
    using namespace atom;
    // finding 9: the glow under the dots is the dots' own density (a sum of Gaussians); its momentum-weighted <b^2> is the dots' exactly
    for (int nk : {2, 3, 7}) {
        const PkGlow g = pkGlow(nk);
        double sw = 0, s = 0, mom = 0, b2 = 0;
        for (int i = 0; i < g.n; i++) { sw += g.weight[i]; s += g.weight[i] * g.w[i]; }
        for (int k = 0; k < nk; k++) { const double m = pkShare(pkDrawOrder()[k]); mom += m; b2 += m * pkDotsMeanB2(pkDrawOrder()[k]); }
        checkNum("glow for " + std::to_string(nk) + " kinds: the Gaussians' weights add up to 1", sw, 1.0, 1e-9);
        checkNum("glow for " + std::to_string(nk) + " kinds: its <b^2> is the dots' momentum-weighted <b^2>", s, b2 / mom, 1e-9);
        checkTrue("glow for " + std::to_string(nk) + " kinds: 6 Gaussians, widths in order", g.n == 6 && std::is_sorted(g.w, g.w + g.n));
    }
}

static void testReviewSea() {
    // finding 7: the Simple style's sea uses the data's own ratio: d-bar / u-bar by momentum, ABM11 at 2 GeV, about 1.26 (0.44: 1.5, from SeaQuest's range at 0.13 < x < 0.45 only)
    const double r = atom::pkSeaDownShare() / (1 - atom::pkSeaDownShare());
    checkNum("Simple sea: d-bar / u-bar = the momentum ratio of the parton densities", r, atom::pkShare(atom::Pk::Dbar) / atom::pkShare(atom::Pk::Ubar), 1e-12);
    checkTrue("... which is about 1.26, below SeaQuest's 1.3 to 1.6 for 0.13 < x < 0.45 (where the excess is largest)", r > 1.2 && r < 1.3);
    // finding 2: the flux tube radius the Simple style draws is Bissey et al.'s 0.38 +- 0.03 fm
    checkNum("flux tube radius 0.38 fm (Bissey et al. 2007)", atom::kFluxTubeRadiusFm, 0.38, 0);
}

static void testReviewWords() {
    const std::string acc = labWords("Accurate", "+ Sea", true), sim = labWords("Simple", "+ Gluons", true), val = labWords("Accurate", "Valence", true);
    // finding 1: the drawn clouds' sizes, from the tables (two decimals)
    const std::string su = fmt::fixed(std::sqrt(atom::pkDotsMeanB2(atom::Pk::Uv)), 2), sd = fmt::fixed(std::sqrt(atom::pkDotsMeanB2(atom::Pk::Dv)), 2);
    checkTrue("the text gives the drawn (momentum-weighted) sizes " + su + " and " + sd + " fm", has(acc, su + " fm") && has(acc, sd + " fm"));
    checkTrue("... and says the number-counted 0.63 / 0.64 fm count every quark the same", has(acc, "0.63") && has(acc, "every quark"));
    // finding 2: Bissey's distances are from the centre (radius), and the Simple quarks sit where the real field is a bubble
    checkTrue("Simple: 'from the centre', not 'apart', and the bubble at the drawn distance", has(sim, "from the centre") && !has(sim, "0.5 fm apart") && has(sim, "bubble"));
    // finding 3: Yang et al. 2018's own text at the physical point: 32 % and 36 % (with 23 % and 9 %: 100 %)
    checkTrue("no 33 % / 37 % mass split left in the Quarks texts", !has(acc, "33 %") && !has(acc, "37 %"));
    // finding 4: gluon spin below x = 0.05 has not been measured
    checkTrue("spin: the rest is not all orbital motion: gluon spin at small x is unmeasured", has(acc, "x < 0.05") && !has(acc, "the rest is orbital motion"));
    // finding 5: where widths are extrapolated, the text says so
    checkTrue("+ Sea: says the sea's spread is measured only at small x and extrapolated beyond", has(acc, "extrapolated"));
    (void)val;
}

static void testQcd045() {
    testReviewWidths();
    testReviewGlow();
    testReviewSea();
    testReviewWords();
}
