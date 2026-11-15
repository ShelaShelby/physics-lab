// 0.54.1, written before the code.
// (1) The nucleus Density look projected exactly (atom_core.h: nucleusChargeDensity, projectChargeDensity). Independent references:
//   - the charge is Z: a plain Cartesian sum of the density over a 3D grid (not the code's own body-frame sums), and the projected grid's sum times the cell area;
//   - the density is the points' model: <r^2> is the measured rms charge radius (IAEA table, radii.h) and a deformed one's <3z^2 - r^2>/<r^2> the shape's own ratio (quadrupole.h);
//   - a round nucleus: Pb-208 against a two-parameter Fermi written here (a = 0.54 fm, c solved here for the measured rms, rho0 for the charge Z), projected here by a direct
//     quadrature along the line of sight; O-16 against its HO charge density projected in closed form, sqrt(pi) A rho0 e^(-R^2/A^2) (1 + alpha (R^2/A^2 + 1/2));
//   - a deformed one (U-238): seen along its axis the picture is round; seen across, the picture's second moments along / across the axis are <z^2> / <x^2> = (1 + q) / (1 - q/2),
//     q the shape's ratio; at an oblique view the cells equal a direct quadrature of the density along the line, written here;
//   - the deuteron in its M = +-1 state: the closed-form L = 2 projection against a direct quadrature along the line at an oblique view.
static double ncdLine(const NucleusChargeDensity& d, const double p[3], const double e[3], double L) {   // the integral of d.at along p + l e, |l| < L (Simpson, 4000 steps)
    const int K = 4000;
    const double h = 2 * L / K;
    double s = 0;
    for (int k = 0; k <= K; k++) {
        const double l = -L + k * h, w = (k == 0 || k == K) ? 1 : (k % 2 ? 4 : 2);
        s += w * d.at(p[0] + l * e[0], p[1] + l * e[1], p[2] + l * e[2]);
    }
    return s * h / 3;
}
static void testDensityProjection0541() {
    struct Nuc { int Z, N; };
    // the charge and the moments, by a Cartesian sum (the axis tilted, so no symmetry of the grid helps)
    for (const Nuc& u : {Nuc{82, 126}, Nuc{92, 146}, Nuc{8, 8}, Nuc{10, 10}, Nuc{1, 1}, Nuc{1, 0}}) {
        const double ax = 0.3, ay = -0.5, az = 0.81;
        const NucleusChargeDensity d = nucleusChargeDensity(u.Z, u.N, ax, ay, az, u.Z == 1 && u.N == 1 ? 1 : 0);
        const double R = d.rEff, h = R / 110;
        const double nl = std::sqrt(ax * ax + ay * ay + az * az), n[3] = {ax / nl, ay / nl, az / nl};
        double q = 0, r2 = 0, zz = 0;
        for (int i = -110; i < 110; i++) for (int j = -110; j < 110; j++) for (int k = -110; k < 110; k++) {
            const double x = (i + 0.5) * h, y = (j + 0.5) * h, z = (k + 0.5) * h, v = d.at(x, y, z) * h * h * h, za = x * n[0] + y * n[1] + z * n[2];
            q += v; r2 += v * (x * x + y * y + z * z); zz += v * za * za;
        }
        const RadiusRow* row = chargeRadius(u.Z, u.N);
        const double rms = std::sqrt(r2 / q);
        std::printf("  density %d-%d: charge %.6f (Z %d), rms %.5f fm (measured %.5f)\n", u.Z, u.N, q, u.Z, rms, row ? row->rms : NAN);
        checkTrue("0.54.1 density " + std::to_string(u.Z) + "-" + std::to_string(u.N) + ": the volume integral (Cartesian sum) is Z within 1e-3", std::fabs(q / u.Z - 1) < 1e-3);
        if (u.Z + u.N > 1 && row) checkTrue("0.54.1 density " + std::to_string(u.Z) + "-" + std::to_string(u.N) + ": rms = the measured charge radius within 1e-3", std::fabs(rms / row->rms - 1) < 1e-3);
        if (d.beta != 0) {
            const double want = shapeMomentRatio(d.beta), got = (3 * zz - r2) / r2;
            checkTrue("0.54.1 density " + std::to_string(u.Z) + "-" + std::to_string(u.N) + ": <3z^2 - r^2>/<r^2> = the shape's ratio within 1e-3 (" + std::to_string(got) + " / " + std::to_string(want) + ")", std::fabs(got - want) < 1e-3);
        }
    }
    const double X[3] = {1, 0, 0}, Y[3] = {0, 1, 0}, Zv[3] = {0, 0, 1};
    // the projected grid's sum is Z (round, deformed across and oblique, the deuteron's dumbbell)
    {
        const NucleusChargeDensity pb = nucleusChargeDensity(82, 126, 0, 0, 1), u = nucleusChargeDensity(92, 146, 1, 0, 0), dd = nucleusChargeDensity(1, 1, 1, 0, 0, 1);
        const double c = std::cos(0.7), s = std::sin(0.7), o1[3] = {c, s, 0}, o2[3] = {-s * 0.6, c * 0.6, 0.8}, o3[3] = {s * 0.8, -c * 0.8, 0.6};
        struct V { const NucleusChargeDensity* d; const double *e1, *e2, *e3; const char* what; };
        for (const V& v : {V{&pb, X, Y, Zv, "Pb-208"}, V{&u, X, Y, Zv, "U-238 across"}, V{&u, o1, o2, o3, "U-238 oblique"}, V{&dd, o1, o2, o3, "deuteron M=1 oblique"}}) {
            std::vector<float> g;
            const int G = 401;
            const double cell = 2 * v.d->rEff / G;
            projectChargeDensity(*v.d, v.e1, v.e2, v.e3, G, cell, g, 400);
            double sum = 0; for (float f : g) sum += f;
            sum *= cell * cell;
            checkTrue(std::string("0.54.1 projection ") + v.what + ": the grid's sum x cell area = Z within 2e-3 (" + std::to_string(sum) + ")", std::fabs(sum / v.d->Z - 1) < 2e-3);
        }
    }
    // Pb-208 against the Fermi written here
    {
        const NucleusChargeDensity d = nucleusChargeDensity(82, 126, 0, 0, 1);
        checkTrue("0.54.1 Pb-208 is round in the lab (beta 0)", d.beta == 0 && d.radial);
        const double a = 0.54, rms = chargeRadius(82, 126)->rms;
        const auto ms = [&](double c) { double n = 0, m = 0; for (int i = 0; i < 40000; i++) { const double r = (i + 0.5) * 0.0005, f = r * r / (1 + std::exp((r - c) / a)); n += f; m += f * r * r; } return m / n; };
        double lo = 5, hi = 8;
        for (int i = 0; i < 60; i++) { const double mid = 0.5 * (lo + hi); (ms(mid) < rms * rms ? lo : hi) = mid; }
        const double c = 0.5 * (lo + hi);
        double n = 0; for (int i = 0; i < 40000; i++) { const double r = (i + 0.5) * 0.0005; n += 4 * kPi * r * r / (1 + std::exp((r - c) / a)) * 0.0005; }
        const double rho0 = 82 / n;
        const int G = 101;
        const double cell = 0.25;
        std::vector<float> g;
        projectChargeDensity(d, X, Y, Zv, G, cell, g);
        double worst = 0, peak = 0;
        for (int col = 50; col < G; col += 4) {
            const double R = (col - 50) * cell;
            double s = 0; const int K = 20000; const double L = 20, hl = L / K;
            for (int k = 0; k < K; k++) { const double l = (k + 0.5) * hl, r = std::sqrt(R * R + l * l); s += 2 * rho0 / (1 + std::exp((r - c) / a)) * hl; }
            peak = std::max(peak, s);
            worst = std::max(worst, std::fabs(g[50 * G + col] - s));
        }
        std::printf("  Pb-208: c %.4f fm; the projection against the Fermi written here: worst %.2e of the middle %.4f e/fm^2\n", c, worst, peak);
        checkTrue("0.54.1 Pb-208: the projection = the Fermi written here (c from the measured rms, a = 0.54 fm), projected by quadrature here, within 1e-3 of the middle", worst < 1e-3 * peak);
    }
    // O-16 against its HO density projected in closed form
    {
        const NucleusChargeDensity d = nucleusChargeDensity(8, 8, 0, 0, 1);
        double A = 0, al = 0;
        nucleusHoCharge(8, 8, A, al);
        checkTrue("0.54.1 O-16 is round in the lab (beta 0) and an HO density", d.beta == 0 && d.radial && d.kind == 2);
        const double rmsHo = A * std::sqrt((6 + 15 * al) / (4 + 6 * al)), sc = chargeRadius(8, 8)->rms / rmsHo, As = A * sc;   // <r^2> of (1 + al r^2/A^2) e^(-r^2/A^2) = A^2 (6 + 15 al) / (4 + 6 al)
        const double rho0 = 8 / (kPi * std::sqrt(kPi) * As * As * As * (1 + 1.5 * al));   // the integral of rho0 (1 + al r^2/As^2) e^(-r^2/As^2) is pi^(3/2) As^3 rho0 (1 + 3 al / 2)
        const int G = 81;
        const double cell = 0.2;
        std::vector<float> g;
        projectChargeDensity(d, X, Y, Zv, G, cell, g);
        double worst = 0, peak = 0;
        for (int col = 40; col < G; col += 3) {
            const double R = (col - 40) * cell, x = R * R / (As * As), want = std::sqrt(kPi) * As * rho0 * std::exp(-x) * (1 + al * (x + 0.5));
            peak = std::max(peak, want); worst = std::max(worst, std::fabs(g[40 * G + col] - want));
        }
        std::printf("  O-16: the projection against the closed form: worst %.2e of %.4f e/fm^2\n", worst, peak);
        checkTrue("0.54.1 O-16: the projection = the HO closed form sqrt(pi) A rho0 e^(-R^2/A^2) (1 + alpha (R^2/A^2 + 1/2)) within 1e-3 of the middle", worst < 1e-3 * peak);
    }
    // U-238: along the axis round; across it the moment ratio; oblique: the cells against a direct quadrature; the drawn quarter grid (qn 80) against a fine one
    {
        const NucleusChargeDensity d = nucleusChargeDensity(92, 146, 0, 0, 1);
        checkTrue("0.54.1 U-238 is deformed in the lab and projected ray by ray", d.beta > 0.2 && !d.radial);
        const int G = 161;
        const double cell = 2.2 * d.ext997 / G;
        std::vector<float> along, across;
        projectChargeDensity(d, X, Y, Zv, G, cell, along, 200);     // the line of sight along the axis z
        projectChargeDensity(d, X, Zv, Y, G, cell, across, 200);    // across: the picture's y is the axis
        const auto moments = [&](const std::vector<float>& g, double& mxx, double& myy) {
            double s = 0; mxx = myy = 0;
            for (int row = 0; row < G; row++) for (int col = 0; col < G; col++) { const double x = (col - G / 2) * cell, y = (G / 2 - row) * cell, v = g[row * G + col]; s += v; mxx += v * x * x; myy += v * y * y; }
            mxx /= s; myy /= s;
        };
        double ax1, ay1, ax2, ay2;
        moments(along, ax1, ay1); moments(across, ax2, ay2);
        double peak = 0, ring = 0;
        for (float v : along) peak = std::max(peak, double(v));
        for (int k = 0; k < 8; k++) {   // round: the value at 6 fm from the middle in 8 directions (between cells: the nearest cell's centre is used, its radius kept)
            const double ph = k * kPi / 8;
            const int col = G / 2 + int(std::lround(6 / cell * std::cos(ph))), row = G / 2 - int(std::lround(6 / cell * std::sin(ph)));
            const double x = (col - G / 2) * cell, y = (G / 2 - row) * cell, rr = std::sqrt(x * x + y * y);
            const int c0 = G / 2 + int(std::lround(rr / cell));   // the same radius along x, by interpolation of the row through the middle
            const double f = rr / cell - (c0 - G / 2), v0 = along[(G / 2) * G + c0], v1 = along[(G / 2) * G + c0 + (f >= 0 ? 1 : -1)];
            ring = std::max(ring, std::fabs(along[row * G + col] - (v0 + std::fabs(f) * (v1 - v0))));
        }
        const double q = shapeMomentRatio(d.beta), want = (1 + q) / (1 - q / 2);
        std::printf("  U-238 (beta %.3f): along the axis <x^2> %.4f <y^2> %.4f, round to %.1e of the middle; across <axis^2>/<x^2> %.5f (want %.5f)\n", d.beta, ax1, ay1, ring / peak, ay2 / ax2, want);
        checkTrue("0.54.1 U-238 seen along its axis: round (<x^2> = <y^2> within 1e-4, the values at one radius within 3e-3 of the middle)", std::fabs(ax1 / ay1 - 1) < 1e-4 && ring < 3e-3 * peak);
        checkTrue("0.54.1 U-238 seen across its axis: <along^2>/<across^2> = (1 + q)/(1 - q/2) of the shape within 2e-3", std::fabs(ay2 / ax2 / want - 1) < 2e-3 && std::fabs(ax2 / ax1 - 1) < 2e-3);
        // oblique
        const double c = std::cos(0.7), s = std::sin(0.7), o1[3] = {c, s, 0}, o2[3] = {-s * 0.6, c * 0.6, 0.8}, o3[3] = {s * 0.8, -c * 0.8, 0.6};
        std::vector<float> ob, coarse;
        const int Gs = 41;
        const double cs = 2.2 * d.ext997 / Gs;
        projectChargeDensity(d, o1, o2, o3, Gs, cs, ob, 300);
        projectChargeDensity(d, o1, o2, o3, Gs, cs, coarse);
        double worst = 0, wc = 0, pk = 0;
        for (int row = 0; row < Gs; row += 2) for (int col = 0; col < Gs; col += 2) {
            const double x = -0.5 * Gs * cs + (col + 0.5) * cs, y = 0.5 * Gs * cs - (row + 0.5) * cs, p[3] = {x * o1[0] + y * o2[0], x * o1[1] + y * o2[1], x * o1[2] + y * o2[2]};
            const double v = ncdLine(d, p, o3, d.rEff);
            pk = std::max(pk, v); worst = std::max(worst, std::fabs(ob[row * Gs + col] - v)); wc = std::max(wc, std::fabs(coarse[row * Gs + col] - v));
        }
        std::printf("  U-238 oblique: against the quadrature here: fine grid %.1e, the drawn grid (qn 64) %.1e of the middle\n", worst / pk, wc / pk);
        checkTrue("0.54.1 U-238 oblique: the cells = a direct quadrature along the line within 2e-3 of the middle (fine quarter grid)", worst < 2e-3 * pk);
        checkTrue("0.54.1 U-238 oblique: the drawn quarter grid (64 rays a side) within 5e-3 of the middle (under one step of an 8-bit picture: 1/255)", wc < 5e-3 * pk);
    }
    // the deuteron's dumbbell, oblique: the closed-form L = 2 projection against a direct quadrature
    {
        const NucleusChargeDensity d = nucleusChargeDensity(1, 1, 0.2, 0.3, 0.9, 1);
        const double c = std::cos(0.4), s = std::sin(0.4), o1[3] = {c, s, 0}, o2[3] = {-s * 0.6, c * 0.6, 0.8}, o3[3] = {s * 0.8, -c * 0.8, 0.6};
        const int Gs = 31;
        const double cs = 2.2 * d.ext997 / Gs;
        std::vector<float> g;
        projectChargeDensity(d, o1, o2, o3, Gs, cs, g);
        double worst = 0, pk = 0;
        for (int row = 1; row < Gs; row += 3) for (int col = 1; col < Gs; col += 3) {
            const double x = -0.5 * Gs * cs + (col + 0.5) * cs, y = 0.5 * Gs * cs - (row + 0.5) * cs, p[3] = {x * o1[0] + y * o2[0], x * o1[1] + y * o2[1], x * o1[2] + y * o2[2]};
            const double v = ncdLine(d, p, o3, d.rEff);
            pk = std::max(pk, v); worst = std::max(worst, std::fabs(g[row * Gs + col] - v));
        }
        std::printf("  deuteron M=1 oblique: the closed-form projection against the quadrature here: %.1e of the middle\n", worst / pk);
        checkTrue("0.54.1 deuteron M = +-1, oblique: the L <= 2 projection = a direct quadrature along the line within 3e-3 of the middle", worst < 3e-3 * pk);
    }
    // the lone neutron: nothing
    checkTrue("0.54.1 a lone neutron: no charge density", nucleusChargeDensity(0, 1, 0, 0, 1).kind < 0);
}

// (2) The outer electron's dots at their true share, as a choice (AtomParams::vdots). References: the electron count (one outer electron of Z: its share 1/Z of the whole atom's
// density, which is the sum of its electrons' densities, each holding one electron) and the Easier to see rule (a quarter of the dots plus its true share of the rest, 0.46.1);
// the counting noise is binomial: 4 sigma = 4 sqrt(p (1 - p) / n).
static void testTrueShare0541() {
    const int n = 40000;
    for (const int Z : {11, 55}) {
        AtomLab L; L.p = AtomParams(); L.reducedMotion = true; L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", double(Z));
        const std::string el = Z == 11 ? "sodium" : "caesium";
        checkTrue("0.54.1 " + el + ": whole atom with jumps, the choice applies; the default is Easier to see", L.valenceOn() && L.outerDotsChoice() && L.p.vdots == "Easier to see");
        const double x = 1.0 / Z, easy = AtomLab::kValenceDotShare + (1 - AtomLab::kValenceDotShare) * x;
        const double fe = L.outerDotFraction(n, 7u);
        checkTrue("0.54.1 " + el + " Easier to see: the outer electron's dots are 1/4 + 3/4 (1/Z) of them within 4 sigma (" + std::to_string(fe) + " / " + std::to_string(easy) + ")",
                  std::fabs(fe - easy) < 4 * std::sqrt(easy * (1 - easy) / n));
        bool denser = false; for (const std::string& t : L.info()) denser = denser || t.find("× denser than its true share") != std::string::npos;
        checkTrue("0.54.1 " + el + " Easier to see: the readout says the dots are drawn denser (as before)", denser);
        L.setParam("vdots", std::string("True share"));
        const double ft = L.outerDotFraction(n, 7u);
        std::printf("  %s: outer electron's share of the dots: Easier to see %.5f (rule %.5f), True share %.5f (1/Z %.5f)\n", el.c_str(), fe, easy, ft, x);
        checkTrue("0.54.1 " + el + " True share: the outer electron's dots are 1/Z of them within 4 sigma (" + std::to_string(ft) + " / " + std::to_string(x) + ")", std::fabs(ft - x) < 4 * std::sqrt(x * (1 - x) / n));
        bool d2 = false, t2 = false;
        for (const std::string& t : L.info()) { d2 = d2 || t.find("denser than its true share") != std::string::npos; t2 = t2 || t.find("its dots at their true share, 1 in " + std::to_string(Z) + ", so its cloud and its jumps are faint") != std::string::npos; }
        checkTrue("0.54.1 " + el + " True share: the readout drops \"drawn denser\" and says 1 in " + std::to_string(Z) + ", faint", !d2 && t2);
        L.setParam("vdots", std::string("Nonsense"));
        checkTrue("0.54.1 an unknown choice is not taken", L.p.vdots == "True share");
        L.setParam("view", std::string("Slice"));
        checkTrue("0.54.1 Slice: the choice does not apply (the Slice view keeps the true shares)", !L.outerDotsChoice());
    }
    {   AtomLab L; L.p = AtomParams(); L.reducedMotion = true; L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", 26.0);
        checkTrue("0.54.1 iron (no jumps): the choice does not apply", !L.outerDotsChoice()); }
    {   AtomLab L; L.p = AtomParams(); L.reducedMotion = true; L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", 12.0); L.setParam("vdots", std::string("True share"));
        bool d = false, t = false;
        // two outer electrons (3s^2); the correlated (GRASP2018) natural 3s orbital holds a little under 2 (the rest is spread over the other outer orbitals that hold less than 0.02 each)
        const std::string key = "the outer electrons' dots at their true share, ";
        double got = NAN;
        for (const std::string& s : L.info()) {
            d = d || s.find("× denser than their true share") != std::string::npos;
            const std::size_t at = s.find(key);
            if (at != std::string::npos && s.find(" in 12, so their cloud and its jumps are faint", at) != std::string::npos) { t = true; got = std::strtod(s.c_str() + at + key.size(), nullptr); }
        }
        std::printf("  magnesium True share: the readout's outer electrons %.3g in 12\n", got);
        checkTrue("0.54.1 magnesium (a level atom) True share: the choice applies, the readout says about 2 in 12 (1.95 to 2), faint, and not denser", L.levelOn() && L.outerDotsChoice() && t && !d && got >= 1.95 && got <= 2); }
}
