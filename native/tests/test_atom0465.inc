// 0.46.5: realistic light nuclei. Written before the code. Reference numbers are typed from the saved sources, never taken from the app:
// Machleidt, PRC 63, 024001 (2001), arXiv:nucl-th/0006014 (data/qcd/nucleus/machleidt_cdbonn_2001_nucl-th0006014.pdf): Table XIX (u, w on a grid), Table XV (r_d 1.966 fm,
// Q_d 0.270 fm^2, P_D 4.85 %), eqs. D14 (norm), D16 (Q_d), D17 (r_d), D18 (P_D); Shukla, arXiv:nucl-th/0302030 Table I (C-12, O-16); CODATA 2022 r_d 2.12778 fm, r_p 0.84075 fm.
// Formulas: the spin-1 density rho_M ~ u^2 + w^2 + k P2(cos) (sqrt(2) u w - w^2 / 2), k = +1 (M = +-1), -2 (M = 0), worked out from S12 (and Forest et al., PRC 54, 646 (1996), eq. 3.3-3.6);
// moments of r^n exp(-r^2/a^2) from the Gamma function; the 90-10 % thickness of exp(-r/b) (b ln 9), of a Fermi edge (4 a ln 3) and of a Gaussian.

static void testDeuteron0465() {
    using namespace atom;
    // (a) the parametrisation reproduces Table XIX (the paper: the fit is good to 2.2e-4 in u and 1.1e-4 in w, eqs. D26-D27)
    const double tab[][3] = {{0.5, 0.194545, 0.0235574}, {1.0, 0.431072, 0.119165}, {1.7, 0.520158, 0.152287}, {3.0, 0.431275, 0.0937453}, {6.0, 0.220245, 0.0256761}, {10.0, 0.0873354, 0.00636565}};
    for (const auto& t : tab) {
        checkNum("CD-Bonn deuteron u(" + fmt::fixed(t[0], 1) + " fm) against Table XIX", deuteronU(t[0]), t[1], 0, 1e-4);
        checkNum("CD-Bonn deuteron w(" + fmt::fixed(t[0], 1) + " fm) against Table XIX", deuteronW(t[0]), t[2], 0, 1e-4);
    }
    // (b) Table XV through eqs. D14, D16-D18
    double nrm = 0, r2 = 0, q = 0, pd = 0;
    for (double r = 0.0005; r < 60; r += 0.001) { const double u = deuteronU(r), w = deuteronW(r); nrm += (u * u + w * w) * 0.001; r2 += r * r * (u * u + w * w) * 0.001; q += r * r * w * (std::sqrt(8.0) * u - w) * 0.001; pd += w * w * 0.001; }
    checkNum("CD-Bonn deuteron: normalised (eq. D14)", nrm, 1.0, 0, 1e-4);
    checkNum("CD-Bonn deuteron: matter radius 1.966 fm (Table XV, eq. D17)", 0.5 * std::sqrt(r2), 1.966, 0, 1e-3);
    checkNum("CD-Bonn deuteron: quadrupole moment 0.270 fm^2 (Table XV, eq. D16)", q / 20, 0.270, 0, 1e-3);
    checkNum("CD-Bonn deuteron: D-state probability 4.85 % (Table XV, eq. D18)", pd, 0.0485, 0, 5e-4);
    // (c) the far share from Table XIX alone (trapezoids from 3 to 14 fm, then the S-wave tail A_S^2 e^(-2 gamma r) / (2 gamma), gamma = 0.2315380 fm^-1, A_S = 0.8846 fm^-1/2)
    {   const double g[][3] = {{3.0, 0.431275, 0.0937453}, {3.2, 0.413778, 0.0855923}, {3.4, 0.396552, 0.0781235}, {3.6, 0.379727, 0.0713176}, {3.8, 0.363387, 0.0651366},
                               {4.0, 0.347583, 0.0595344}, {4.2, 0.332343, 0.0544623}, {4.4, 0.317678, 0.0498721}, {4.6, 0.303592, 0.0457178}, {4.8, 0.290078, 0.0419565},
                               {5.0, 0.277126, 0.0385487}, {5.2, 0.264721, 0.0354587}, {5.4, 0.252849, 0.0326540}, {5.6, 0.241491, 0.0301056}, {5.8, 0.230629, 0.0277874},
                               {6.0, 0.220245, 0.0256761}, {6.5, 0.196252, 0.0211717}, {7.0, 0.174846, 0.0175676}, {7.5, 0.155759, 0.0146616}, {8.0, 0.138747, 0.0123010},
                               {8.5, 0.123589, 0.0103699}, {9.0, 0.110084, 0.00877993}, {9.5, 0.0980525, 0.00746281}, {10.0, 0.0873354, 0.00636565}, {10.5, 0.0777891, 0.00544705},
                               {11.0, 0.0692859, 0.00467438}, {11.5, 0.0617120, 0.00402170}, {12.0, 0.0549660, 0.00346826}, {12.5, 0.0489573, 0.00299734}, {13.0, 0.0436055, 0.00259535},
                               {13.5, 0.0388386, 0.00225120}, {14.0, 0.0345929, 0.00195582}};
        double far = 0;
        for (std::size_t i = 1; i < std::size(g); i++) far += 0.5 * (g[i][0] - g[i - 1][0]) * (g[i][1] * g[i][1] + g[i][2] * g[i][2] + g[i - 1][1] * g[i - 1][1] + g[i - 1][2] * g[i - 1][2]);
        far += 0.8846 * 0.8846 * std::exp(-2 * 0.2315380 * 14.0) / (2 * 0.2315380);
        std::printf("deuteron: neutron and proton more than 3 fm apart: %.4f (Table XIX), %.4f (drawn)\n", far, deuteronFarShare(3.0));
        checkNum("deuteron: the chance the pair is more than 3 fm apart, against Table XIX", deuteronFarShare(3.0), far, 0, 3e-3);
    }
    // (d) the drawn separations: <s^2> / 4 = r_d^2 and the quadrupole (1/4) <3 z^2 - s^2> = Q_d (M = +-1), -2 Q_d (M = 0), 0 (unpolarised): eq. D16 with the spin-1 algebra
    std::mt19937 rng(465);
    std::uniform_real_distribution<double> U(0.0, 1.0);
    const std::function<double()> uni = [&] { return U(rng); };
    const double want[3] = {0.0, 0.270, -0.540};
    const char* nm[3] = {"unpolarised", "M = +-1", "M = 0"};
    for (int sp = 0; sp < 3; sp++) {
        double s2 = 0, qq = 0; const int n = 400000;
        for (int i = 0; i < n; i++) { double x, y, z; deuteronSeparation(sp, uni, x, y, z); const double d2 = x * x + y * y + z * z; s2 += d2; qq += 3 * z * z - d2; }
        std::printf("deuteron %s: rms half separation %.4f fm, quadrupole %.4f fm^2\n", nm[sp], std::sqrt(s2 / n / 4), qq / n / 4);
        checkNum(std::string("deuteron ") + nm[sp] + ": the drawn pairs' half separation, rms = r_d 1.966 fm", std::sqrt(s2 / n / 4), 1.966, 0.01);
        checkNum(std::string("deuteron ") + nm[sp] + ": the drawn pairs' quadrupole (1/4)<3z^2 - s^2>", qq / n / 4, want[sp], 0, 0.03);
    }
    // (e) the snapshot: one pair drawn from the wave function, scaled so that on average the proton's point distance matches the measured charge radius (CODATA r_d 2.12778 fm);
    // it is no longer always the same distance, nor always in one plane
    {   const double want2 = pointProtonMsFromCharge(1, 1, 2.12778);
        double s = 0, smin = 1e9, smax = 0, qq[3] = {0, 0, 0}; const int n = 3000;
        seedRandom(77);
        for (int sp = 0; sp < 3; sp++)
            for (int i = 0; i < n; i++) {
                double ax[3];
                const auto v = buildNucleus(1, 1, ax, sp);
                const double dx = (v[0].x - v[1].x) / kFmLab, dy = (v[0].y - v[1].y) / kFmLab, dz = (v[0].z - v[1].z) / kFmLab, d2 = dx * dx + dy * dy + dz * dz, along = dx * ax[0] + dy * ax[1] + dz * ax[2];
                if (sp == 0) { s += d2 / 4; smin = std::min(smin, std::sqrt(d2)); smax = std::max(smax, std::sqrt(d2)); }
                qq[sp] += (3 * along * along - d2) / 4;
                checkTrue("deuteron snapshot: a proton and a neutron", v.size() == 2 && v[0].isP != v[1].isP);
                if (v.size() != 2 || v[0].isP == v[1].isP) break;
            }
        std::printf("deuteron snapshots: mean half separation^2 %.3f fm^2 (want %.3f), pair %.2f to %.2f fm apart, quadrupoles %.3f %.3f %.3f\n", s / n, want2, smin, smax, qq[0] / n, qq[1] / n, qq[2] / n);
        checkNum("deuteron snapshots: mean (half separation)^2 = the measured point-proton mean square radius", s / n, want2, 0.05);
        checkTrue("deuteron snapshots: the distance varies (the wave function), from under 1.5 fm to over 5 fm", smin < 1.5 && smax > 5);
        checkTrue("deuteron snapshots: M = +-1 stretched along the spin axis, M = 0 flattened round it, unpolarised neither", qq[1] / n > 0.12 && qq[2] / n < -0.3 && std::fabs(qq[0] / n) < 0.15);
    }
    // (f) the Density look: the charge (proton at s/2, its own exponential charge) scaled to the measured radius; its quadrupole is the wave function's, times that scale squared
    {   const double f2 = 2.12778 * 2.12778 / (1.966 * 1.966 + 0.84075 * 0.84075);
        for (int sp = 0; sp < 3; sp++) {
            const std::vector<double> d = nucleusChargeDots(1, 1, 300000, 0, 0, 1, 5u, sp);
            double ms = 0, qq = 0; const std::size_t n = d.size() / 3;
            for (std::size_t i = 0; i < n; i++) { const double x = d[3 * i], y = d[3 * i + 1], z = d[3 * i + 2], r2 = x * x + y * y + z * z; ms += r2; qq += 3 * z * z - r2; }
            checkNum(std::string("deuteron charge dots ") + nm[sp] + ": rms = CODATA 2.12778 fm", std::sqrt(ms / n), 2.12778, 1e-9);
            checkNum(std::string("deuteron charge dots ") + nm[sp] + ": quadrupole = CD-Bonn's x the scale^2", qq / n, f2 * want[sp], 0, 0.04);
        }
        checkTrue("deuteron charge dots: the same seed gives the same points", nucleusChargeDots(1, 1, 500, 0, 0, 1, 9u, 2) == nucleusChargeDots(1, 1, 500, 0, 0, 1, 9u, 2));
    }
}

static void testLightNuclei0465() {
    using namespace atom;
    double a = 0, al = 0;
    checkTrue("C-12 has the measured HO charge density: a 1.692 fm, alpha 1.082 (Shukla Table I)", nucleusHoCharge(6, 6, a, al) && a == 1.692 && al == 1.082);
    checkTrue("O-16 has the measured HO charge density: a 1.833 fm, alpha 1.544 (Shukla and Loizides Table I)", nucleusHoCharge(8, 8, a, al) && a == 1.833 && al == 1.544);
    checkTrue("no HO density for He-4, N-14, C-13 (no source saved)", !nucleusHoCharge(2, 2, a, al) && !nucleusHoCharge(7, 7, a, al) && !nucleusHoCharge(6, 7, a, al));
    // the charge dots follow the HO shape: <r^4> / <r^2>^2 = (G(7/2) + alpha G(9/2)) (G(3/2) + alpha G(5/2)) / (G(5/2) + alpha G(7/2))^2 (a Gaussian: 5/3); the scaling to the measured
    // radius does not change this ratio for a round nucleus
    for (const auto& zn : {std::pair<int, int>{8, 8}, {6, 6}}) {
        nucleusHoCharge(zn.first, zn.second, a, al);
        const double r22 = std::tgamma(2.5) + al * std::tgamma(3.5), r44 = std::tgamma(3.5) + al * std::tgamma(4.5), r00 = std::tgamma(1.5) + al * std::tgamma(2.5);
        const double want = r44 * r00 / (r22 * r22);
        const std::vector<double> d = nucleusChargeDots(zn.first, zn.second, 200000, 0, 0, 1, 3u);
        double m2 = 0, m4 = 0, xx = 0, zz = 0; const std::size_t n = d.size() / 3;
        for (std::size_t i = 0; i < n; i++) { const double r2 = d[3 * i] * d[3 * i] + d[3 * i + 1] * d[3 * i + 1] + d[3 * i + 2] * d[3 * i + 2]; m2 += r2; m4 += r2 * r2; xx += d[3 * i] * d[3 * i]; zz += d[3 * i + 2] * d[3 * i + 2]; }
        const double got = (m4 / n) / ((m2 / n) * (m2 / n));
        std::printf("%s charge dots: <r^4>/<r^2>^2 = %.4f (HO %.4f, Gaussian 1.6667), <z^2>/<x^2> = %.3f\n", at(zn.first, zn.second).c_str(), got, want, zz / xx);
        if (std::fabs(zz / xx - 1) < 0.02) checkNum("charge dots of " + at(zn.first, zn.second) + ": the measured HO shape (moment ratio)", got, want, 0.015);
        else checkTrue("charge dots of " + at(zn.first, zn.second) + " (deformed): nearer the HO moment ratio than a Gaussian's", std::fabs(got - want) < std::fabs(got - 5.0 / 3.0));
    }
    // the edge thickness of what is drawn, 90 % to 10 %
    checkNum("edge: the proton, exp(-r/b), b = r_p / sqrt(12): b ln 9", nucEdgeFm(1, 0), 0.84075 / std::sqrt(12.0) * std::log(9.0), 2e-3);
    checkNum("edge: Pb-208, a Fermi edge 0.54 fm: 4 a ln 3", nucEdgeFm(82, 126), 4 * 0.54 * std::log(3.0), 3e-3);
    {   const RadiusRow* r = chargeRadius(3, 4);
        const double sg = r->rms / std::sqrt(3.0);
        checkNum("edge: Li-7, a Gaussian of the measured rms: sigma (sqrt(2 ln 10) - sqrt(2 ln 10/9))", nucEdgeFm(3, 4), sg * (std::sqrt(2 * std::log(10.0)) - std::sqrt(2 * std::log(10.0 / 9.0))), 3e-3);
    }
    checkTrue("edge: the deuteron has none (NaN)", std::isnan(nucEdgeFm(1, 1)));
    std::printf("edges: C-12 %.3f fm, O-16 %.3f fm, Ca-40 %.3f fm, U-238 %.3f fm\n", nucEdgeFm(6, 6), nucEdgeFm(8, 8), nucEdgeFm(20, 20), nucEdgeFm(92, 146));
}
