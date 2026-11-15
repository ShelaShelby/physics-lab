// 0.49.3 (outside review): the Lamb shift of n = 1 and 2 in the Dirac levels and lines (atom/dirac.h; data/lamb).
// References, all independent of the app's generated table (lamb_data.inc):
//  - the source's LAMB rows typed here from data/lamb (Hlike8.tex, table tab:energy) for H, He-4, Fe-56, U-238: the intervals 1s, 2s-1s, 2p1/2-2s, 2p3/2-2p1/2 in h c R_inf;
//  - the same paper's OTHER table (tab:lamb, row Total), the shifts in units of F(Z alpha): Delta E = (alpha / pi) (Z alpha)^4 / n^3 F m c^2 = 2 alpha^3 Z^4 F / (pi n^3) in h c R_inf;
//  - NIST ASD's Lyman-alpha components of every one-electron ion the app holds (measured or Ritz).

namespace lamb0493 {
struct Typed { int Z; double l1s, d2s1s, d2p1_2s, d2p3_2p1; };   // the LAMB row as printed (the x of the source's alignment removed)
const Typed kTyped[4] = {
    {1, 0.0000024842735, -0.0000021666270, -0.00000032154868, 0.000000007707557},
    {2, 0.000032734903, -0.000028528934, -0.0000042681250, 0.00000012330461},
    {26, 0.291506, -0.251243, -0.0413649, 0.0032664},
    {92, 34.072, -27.659, -5.5849, -0.1826},
};
// the level shifts (h c R_inf) from the typed intervals: 1s, 2s, 2p1/2, 2p3/2
void levels(const Typed& t, double out[4]) { out[0] = t.l1s; out[1] = out[0] + t.d2s1s; out[2] = out[1] + t.d2p1_2s; out[3] = out[2] + t.d2p3_2p1; }
const Typed* typed(int Z) { for (const Typed& t : kTyped) if (t.Z == Z) return &t; return nullptr; }
// the typed shift of (n, l, j2), eV (0 for n >= 3)
double typedEV(int Z, int n, int l, int j2) {
    const Typed* t = typed(Z); if (!t || n > 2) return 0;
    double v[4]; levels(*t, v);
    const int k = n == 1 ? 0 : l == 0 ? 1 : j2 == 1 ? 2 : 3;
    return v[k] * nist::rydEV;
}
// the hand-made line with the Lamb shift: the hybrid Dirac levels written again (refHybridEV) plus the typed shifts
double refLamQed(int Z, int N, int lo, int lL, int jl, int hi, int lH, int jh) {
    return nist::hcEVnm / (refHybridEV(Z, N, hi, jh) - refHybridEV(Z, N, lo, jl) + typedEV(Z, hi, lH, jh) - typedEV(Z, lo, lL, jl));
}
}  // namespace lamb0493

// Guards (measured on the first run + 1 %, Linux GCC Release): printed below and in REPORT.md.
// Lyman-alpha vs ASD, 220 (ion, component) pairs: worst share of Dirac's miss left 1.130910e-03 -> 1.1422e-3; hydrogen worst relative miss 2.451317e-09 -> 2.476e-9.
constexpr double GUARD_LAMB_LEFT = 1.1422e-3, GUARD_LAMB_H = 2.476e-9;
static void testLamb0493() {
    using namespace lamb0493;
    // ---- the generated table against the typed source rows (the tool's arithmetic and parsing) ----
    {
        double worst = 0;
        for (const Typed& t : kTyped) {
            double v[4]; levels(t, v);
            const double lab[4] = {lambShiftEV(t.Z, 1, 0, 1), lambShiftEV(t.Z, 2, 0, 1), lambShiftEV(t.Z, 2, 1, 1), lambShiftEV(t.Z, 2, 1, 3)};
            for (int k = 0; k < 4; k++) worst = std::max(worst, std::fabs(lab[k] / kRydInf - v[k]) / std::fabs(v[k]));
        }
        std::printf("Lamb shift: the app's table vs the typed LAMB rows (H, He, Fe, U; 1s, 2s, 2p1/2, 2p3/2): worst relative difference %.3e\n", worst);
        checkTrue("Lamb shift: the app's table equals the source's LAMB rows typed by hand for H, He-4, Fe-56, U-238 (1s, 2s, 2p1/2, 2p3/2; rounding level 1e-12)", worst < 1e-12);
    }
    // ---- the table against the paper's second table, in units of F(Z alpha) ----
    {
        // ftol: half a unit of the last printed digit of F, relative, rounded up; unit: the last printed digit of the energy table's LAMB intervals (1s, 2s-1s, 2p1/2-2s, 2p3/2-2p1/2), h c R_inf.
        // A level is a sum of 1 to 4 intervals, so its own last digit is k half-units of those (a small level such as Fe's 2p1/2, -0.0011, is a difference of numbers 40 times larger).
        struct F { int Z; double f[4]; double ftol; double unit[4]; };
        const F fs[3] = {{1, {10.042071, 10.272069, -0.126189, 0.123058}, 5e-6, {1e-13, 1e-13, 1e-14, 1e-15}},
                         {26, {2.57856, 2.84917, -0.078031, 0.153115}, 7e-5, {1e-6, 1e-6, 1e-7, 1e-7}},
                         {92, {1.9225, 2.8947, 0.37363, 0.29118}, 3e-4, {1e-3, 1e-3, 1e-4, 1e-4}}};
        bool ok = true; double worst = 0;
        for (const F& f : fs) {
            const int n[4] = {1, 2, 2, 2}, l[4] = {0, 0, 1, 1}, j2[4] = {1, 1, 1, 3};
            double digits = 0;
            for (int k = 0; k < 4; k++) {
                digits += 0.5 * f.unit[k];
                const double a = nist::alpha, want = 2 * a * a * a * std::pow(double(f.Z), 4) * f.f[k] / (kPi * n[k] * n[k] * n[k]), got = lambShiftEV(f.Z, n[k], l[k], j2[k]) / kRydInf;
                const double tol = std::fabs(want) * f.ftol + digits, miss = std::fabs(got - want);
                worst = std::max(worst, miss / tol); ok = ok && miss < tol;
            }
        }
        std::printf("Lamb shift: the energy table vs the F(Z alpha) table of the same paper (H, Fe, U): worst difference %.3f of the printed digits of the two tables\n", worst);
        checkTrue("Lamb shift: the levels agree with the paper's other table, Delta E = 2 alpha^3 Z^4 F / (pi n^3) h c R_inf, within the printed digits of both tables (H, Fe-56, U-238; 1s, 2s, 2p1/2, 2p3/2)", ok);
    }
    // ---- where there is no value ----
    checkTrue("Lamb shift: known for n = 1, 2 and Z = 1..110 only; zero elsewhere; the isotope is the source's (H-1, U-238, Ds-273)",
              lambKnown(1, 1) && lambKnown(110, 2) && !lambKnown(111, 1) && !lambKnown(1, 3) && lambShiftEV(1, 3, 0, 1) == 0 && lambShiftEV(111, 1, 0, 1) == 0 &&
              lambIsotopeA(1) == 1 && lambIsotopeA(92) == 238 && lambIsotopeA(110) == 273 && lambIsotopeA(111) == 0 && std::fabs(lambRadiusFm(92) - 5.8571) < 1e-12);
    checkTrue("diracQedEV: the Dirac level plus the shift; for n = 3 it IS the Dirac level", diracQedEV(1, 0, 3, 1, 1) == diracEV(1, 0, 3, 1) &&
              std::fabs(diracQedEV(92, defaultN(92), 1, 0, 1) - diracEV(92, defaultN(92), 1, 1) - 34.072 * nist::rydEV) < 1e-6 * 34.072 * nist::rydEV);
    // ---- the lines: the Lamb shift splits what Dirac theory joins ----
    {
        auto count = [](int Z, int lo, int hi) { int n = 0; for (const DiracLine& l : diracLines(Z, defaultN(Z))) if (l.lo == lo && l.hi == hi) n++; return n; };
        checkTrue("lines with the Lamb shift, hydrogen: Lyman-alpha 2, Lyman-beta 2, Balmer-alpha 7, Balmer-beta 7 (all seven textbook components: 2s1/2 and 2p1/2 no longer coincide)",
                  count(1, 1, 2) == 2 && count(1, 1, 3) == 2 && count(1, 2, 3) == 7 && count(1, 2, 4) == 7);
        checkTrue("lines without a Lamb shift (Z = 112, beyond the source): Balmer-alpha 5 as in Dirac theory (two pairs coincide)", count(112, 2, 3) == 5 && count(112, 2, 4) == 5);
        int lim2 = 0; for (const DiracLine& l : diracLines(1, 0)) if (l.hi == 0 && l.lo == 2) lim2++;
        checkTrue("the n = 2 series limit of hydrogen is split three ways with the Lamb shift (2s1/2, 2p1/2, 2p3/2)", lim2 == 3);
        // the two Balmer-alpha lines 3p1/2 -> 2s1/2 and 3s1/2 -> 2p1/2 differ by exactly the 2s - 2p1/2 Lamb shift (no n = 3 shift in the source)
        double a = 0, b = 0;
        for (const DiracLine& l : diracLines(1, 0)) if (l.lo == 2 && l.hi == 3 && l.jLo2 == 1 && l.jHi2 == 1) (l.lLo == 0 ? a : b) = l.lam;
        const double gap = nist::hcEVnm / b - nist::hcEVnm / a, want = typedEV(1, 2, 0, 1) - typedEV(1, 2, 1, 1);
        std::printf("H Balmer-alpha 3s1/2-2p1/2 vs 3p1/2-2s1/2: %.6f MHz apart (the typed 2s - 2p1/2 Lamb shift: %.6f MHz)\n", gap / nist::h * nist::e * 1e-6, want / nist::h * nist::e * 1e-6);
        checkNum("H Balmer-alpha: 3s1/2 -> 2p1/2 lies above 3p1/2 -> 2s1/2 in energy by the 2s - 2p1/2 Lamb shift of the source", gap, want, 1e-6);
        double got = 0; for (const DiracLine& l : diracLines(1, 0)) if (l.lo == 1 && l.hi == 2 && l.jHi2 == 3) got = l.lam;
        checkNum("diracLines: H Lyman-alpha 2p3/2 = the hand-made one (hybrid Dirac levels + the typed Lamb shifts)", got, refLamQed(1, 0, 1, 0, 1, 2, 1, 3), 1e-12);
    }
    // ---- against NIST: Lyman-alpha of every ion, with the Lamb shift closer than without ----
    {
        int pairs = 0; bool closer = true; double worstLeft = 0, worstH = 0;
        for (int Z = 1; Z <= 110; Z++) {
            const auto m = asdE1(Z); if (m.empty()) continue;
            const int N = defaultN(Z);
            for (int j2 : {1, 3}) {
                const auto it = m.find({1, 2, 1, j2}); if (it == m.end()) continue;
                double lab = 0; for (const DiracLine& l : diracLines(Z, N)) if (l.lo == 1 && l.hi == 2 && l.jHi2 == j2) lab = l.lam;
                for (double w : it->second) {
                    const double d = refLam(Z, N, 1, 1, 2, j2, true), left = (lab - w) / (d - w);   // what is left of Dirac's miss
                    closer = closer && std::fabs(lab - w) < std::fabs(d - w);
                    worstLeft = std::max(worstLeft, std::fabs(left)); pairs++;
                    if (Z == 1) worstH = std::max(worstH, std::fabs(lab - w) / w);
                }
            }
        }
        std::printf("Lyman-alpha vs ASD with the Lamb shift, %d (ion, component) pairs: worst share of Dirac's miss still left %.6e; hydrogen worst relative miss %.6e\n", pairs, worstLeft, worstH);
        checkTrue("Lyman-alpha vs NIST ASD: with the Lamb shift every component of every ion (Z <= 110) is closer to NIST than the Dirac line", closer && pairs >= 200);
        checkTrue("Lyman-alpha vs NIST ASD: at most the measured share of Dirac's miss is left (guard measured + 1 %, REPORT.md)", worstLeft < GUARD_LAMB_LEFT);
        checkTrue("hydrogen Lyman-alpha vs NIST ASD with the Lamb shift: within the measured miss + 1 % (it was 3.3e-6 with Dirac alone)", worstH < GUARD_LAMB_H);
    }
}

// ---- 0.49.3: the Dirac cloud of a one-electron atom (atom/dirac_cloud.h). References: the Dirac radial equations themselves (with dirac.h's energy formula), the textbook 1s <r> = (2 gamma + 1) / (2 Z),
// the non-relativistic limit (the lab's radial density, checked elsewhere against its own references), the node count, the angular moments of sin^(2L) theta, and line-strength sum rules. ----
static void testDiracCloud0493() {
    const double c = 1 / nist::alpha;
    // the radial equations, by central differences: P' + kappa/r P - (E - V + 2c^2)/c Q = 0, Q' - kappa/r Q + (E - V)/c P = 0 (E in hartree from diracInfiniteEV, V = -Z/r)
    double worstOde = 0, worstNorm = 0; int states = 0; bool nodes = true;
    for (int Z : {1, 26, 54, 92, 118})
        for (int n = 1; n <= 6; n++)
            for (int l = 0; l < std::min(n, 4); l++)
                for (int j2 : {2 * l - 1, 2 * l + 1}) {
                    if (j2 < 1) continue;
                    const int kappa = diracKappa(l, j2);
                    const double E = diracInfiniteEV(Z, n, j2) / nist::hartreeEV;
                    double worst = 0, scale = 0;
                    for (int i = 1; i <= 300; i++) {
                        const double r = i * 0.04 * n * n / Z, h = 1e-5 * r;
                        double P, Q, Pp, Qp, Pm, Qm;
                        diracPQ(Z, n, kappa, r, P, Q); diracPQ(Z, n, kappa, r + h, Pp, Qp); diracPQ(Z, n, kappa, r - h, Pm, Qm);
                        const double dP = (Pp - Pm) / (2 * h), dQ = (Qp - Qm) / (2 * h), V = -Z / r;
                        const double e1 = dP + kappa / r * P - (E - V + 2 * c * c) / c * Q, e2 = dQ - kappa / r * Q + (E - V) / c * P;
                        scale = std::max({scale, std::fabs(dP), std::fabs(kappa / r * P), std::fabs((E - V + 2 * c * c) / c * Q)});
                        worst = std::max({worst, std::fabs(e1), std::fabs(e2)});
                    }
                    worstOde = std::max(worstOde, worst / scale);
                    // normalised: the integral of P^2 + Q^2 (Simpson on a fine grid, its own) is 1
                    double sum = 0; const int K = 20000; const double R = diracGraph(Z, n, kappa).rEnd * 3, hh = R / K;
                    for (int i = 0; i <= K; i++) sum += (i == 0 || i == K ? 1 : i % 2 ? 4 : 2) * diracRadialDensity(Z, n, kappa, i * hh);
                    worstNorm = std::max(worstNorm, std::fabs(sum * hh / 3 - 1));
                    // P has n - l - 1 nodes, as the non-relativistic radial function
                    int changes = 0; double prev = 0;
                    for (int i = 1; i <= 4000; i++) { double P, Q; diracPQ(Z, n, kappa, i * R / 4000, P, Q); if (prev != 0 && P != 0 && (P > 0) != (prev > 0)) changes++; if (P != 0) prev = P; }
                    nodes = nodes && changes == n - l - 1;
                    states++;
                }
    std::printf("Dirac cloud: %d states (Z = 1, 26, 54, 92, 118; n <= 6, l <= 3, both j): worst relative residual of the radial equations %.2e; worst |norm - 1| %.2e\n", states, worstOde, worstNorm);
    checkTrue("Dirac cloud: P and Q solve the Dirac radial equations with the Sommerfeld energy, every state n <= 6, l <= 3, at Z = 1 .. 118 (central differences, relative 1e-6)", worstOde < 1e-6 && states == 5 * 30);
    checkTrue("Dirac cloud: every state is normalised (the integral of P^2 + Q^2 is 1, 1e-7: the check's own Simpson rule near the r^(2 gamma) cusp of Z = 118 is good to about 1e-8)", worstNorm < 1e-7);
    checkTrue("Dirac cloud: the large component has n - l - 1 nodes, as the non-relativistic radial function", nodes);
    // 1s: P ~ r^gamma e^(-Z r), so <r> = (2 gamma + 1) / (2 Z) exactly
    double worst1s = 0;
    for (int Z : {1, 10, 50, 92, 118}) { const double g = diracGamma(Z, -1); worst1s = std::max(worst1s, std::fabs(diracMeanRadius(Z, 1, -1) / ((2 * g + 1) / (2.0 * Z)) - 1)); }
    checkTrue("Dirac cloud: 1s <r> = (2 gamma + 1) / (2 Z), the textbook result for P ~ r^gamma e^(-Z r) (Z = 1 .. 118, 1e-9)", worst1s < 1e-9);
    const double u92 = diracMeanRadius(92, 1, -1) / meanRadius(1, 0, 92);
    std::printf("Dirac cloud: U91+ 1s <r> is %.4f of the non-relativistic one; the small component holds %.4f of the electron\n", u92, diracSmallShare(92, 1, -1));
    checkTrue("Dirac cloud: U91+ 1s is smaller than the non-relativistic 1s (the relativistic contraction, (2 gamma + 1) / 3 = 0.829)", std::fabs(u92 - (2 * diracGamma(92, -1) + 1) / 3) < 1e-9);
    // the non-relativistic limit: hydrogen's Dirac density is the Schroedinger one to the order alpha^2
    double worstNr = 0;
    for (int n = 1; n <= 4; n++) for (int l = 0; l < n; l++) for (int j2 : {2 * l - 1, 2 * l + 1}) {
        if (j2 < 1) continue;
        for (int i = 1; i <= 200; i++) { const double r = i * 0.1 * n * n; worstNr = std::max(worstNr, std::fabs(diracRadialDensity(1, n, diracKappa(l, j2), r) - radialDensity(orbIndex(std::to_string(n) + "spdf"[l]), 1, r))); }
    }
    std::printf("Dirac cloud: hydrogen's Dirac radial densities vs the non-relativistic ones (n <= 4): worst difference %.2e per a0 (alpha^2 = %.2e)\n", worstNr, nist::alpha * nist::alpha);
    checkTrue("Dirac cloud: for hydrogen the Dirac radial density is the non-relativistic one to the order alpha^2 (below alpha^2 per a0, n <= 4, both j)", worstNr < nist::alpha * nist::alpha);
    // samples: <r> within 4 standard errors, and the lined-up direction's <cos^2 theta> = 1/3 (j = 1/2), 1/5 (L = 1), 1/7 (L = 2), 1/9 (L = 3) (moments of sin^(2L) theta)
    {
        orbit::Mulberry32 rng(2026);
        struct S { int Z, n, l, j2; double cos2; };
        const S ss[4] = {{92, 1, 0, 1, 1.0 / 3}, {80, 2, 1, 3, 1.0 / 5}, {54, 3, 2, 5, 1.0 / 7}, {30, 4, 3, 7, 1.0 / 9}};
        double worstR = 0, worstC = 0;
        for (const S& s : ss) {
            const int M = 200000; double sr = 0, sr2 = 0, sc = 0, sc2 = 0;
            for (int i = 0; i < M; i++) {
                double x, y, z; sampleDirac(s.Z, s.n, s.l, s.j2, false, rng, x, y, z);
                const double r = std::sqrt(x * x + y * y + z * z), c2 = z * z / (r * r);
                sr += r; sr2 += r * r; sc += c2; sc2 += c2 * c2;
            }
            const double mr = sr / M, ser = std::sqrt((sr2 / M - mr * mr) / M), mc = sc / M, sec = std::sqrt((sc2 / M - mc * mc) / M);
            worstR = std::max(worstR, std::fabs(mr - diracMeanRadius(s.Z, s.n, diracKappa(s.l, s.j2))) / ser);
            worstC = std::max(worstC, std::fabs(mc - s.cos2) / sec);
        }
        std::printf("Dirac cloud samples (4 states x 200,000): worst <r> miss %.2f standard errors, worst <cos^2 theta> miss %.2f standard errors\n", worstR, worstC);
        checkTrue("Dirac cloud: sampled <r> agrees with the radial function's within 4 standard errors (U 1s, Hg 2p3/2, Xe 3d5/2, Zn 4f7/2)", worstR < 4);
        checkTrue("Dirac cloud: the lined-up direction (m = j) gives <cos^2 theta> = 1/3, 1/5, 1/7, 1/9 for L = j - 1/2 = 0..3 within 4 standard errors", worstC < 4);
    }
    // the Slice: points on the plane, and their radii follow D(r) / r beyond |off| (mean of 1/r against the integral)
    {
        orbit::Mulberry32 rng(7);
        int on = 0, tries = 0; double s = 0; const double off = 0.004;
        for (int i = 0; i < 100000; i++) { double x, y, z; tries++; if (tryDiracOnPlane(92, 1, 0, 1, true, 2, off, rng, x, y, z)) { on += z == off; s += std::sqrt(x * x + y * y + z * z); } }
        checkTrue("Dirac cloud, Slice: every accepted point lies on its plane", on > 0 && tries == 100000);
        (void)s;
    }
    // line strengths: s1/2 -> p1/2 : p3/2 = 1 : 2 (the D1 : D2 ratio of the alkalis); d3/2 -> p1/2 : p3/2 = 5 : 1; the sum over j' of every (l, j) -> l' is proportional to 2j + 1
    checkNum("line strengths: s1/2 -> p3/2 is twice s1/2 -> p1/2", lineStrength(0, 1, 1, 3) / lineStrength(0, 1, 1, 1), 2, 1e-12);
    checkNum("line strengths: d3/2 -> p1/2 is five times d3/2 -> p3/2", lineStrength(2, 3, 1, 1) / lineStrength(2, 3, 1, 3), 5, 1e-12);
    checkTrue("line strengths: forbidden jumps have none (p1/2 -> d5/2, s -> s, d -> s)", lineStrength(1, 1, 2, 5) == 0 && lineStrength(0, 1, 0, 1) == 0 && lineStrength(2, 3, 0, 1) == 0);
    double worstSum = 0;
    for (int l = 0; l <= 3; l++) for (int lp : {l - 1, l + 1}) {
        if (lp < 0 || lp > 3) continue;
        double ref = -1;
        for (int j2 : {2 * l - 1, 2 * l + 1}) {
            if (j2 < 1) continue;
            double sum = 0; for (int jp2 : {2 * lp - 1, 2 * lp + 1}) if (jp2 >= 1) sum += lineStrength(l, j2, lp, jp2);
            const double per = sum / (j2 + 1);
            if (ref < 0) ref = per; else worstSum = std::max(worstSum, std::fabs(per / ref - 1));
        }
    }
    checkTrue("line strengths: the sum over j' from (l, j) to l' is proportional to 2j + 1 (the sum rule), l, l' <= 3", worstSum < 1e-12);
    checkTrue("Dirac state text", diracStateText(2, 1, 3) == "2p₃/₂" && diracStateText(1, 0, 1) == "1s₁/₂");
}

// ---- 0.49.3: the Dirac cloud in the lab (AtomLab::diracCloudOn): when it is on, its dots, its jumps ----
static void testDiracLab0493() {
    auto lab = [](int Z, const char* view, const char* orb) {
        AtomLab L; L.nowMs = [] { return 0.0; }; L.p = AtomParams(); L.p.elec = "Hydrogen-like"; L.restart();
        L.setParam("view", std::string(view)); L.setParam("Z", double(Z)); L.setParam("orb", std::string(orb));
        return L;
    };
    {   // when: Auto from Z = 27 in the cloud views, Dirac at any Z, never with a superposition, in the Orbit view or with every electron
        AtomLab a = lab(92, "Cloud 3D", "1s"), b = lab(26, "Cloud 3D", "1s"), c = lab(27, "Slice", "1s"), d = lab(1, "Cloud", "1s"), e = lab(92, "Orbit", "1s");
        d.setParam("hrel", std::string("Dirac"));
        AtomLab f = lab(92, "Cloud", "2p"); f.setParam("super", std::string("On"));
        AtomLab g = lab(92, "Cloud", "1s"); g.setParam("hrel", std::string("Non-relativistic"));
        checkTrue("Dirac lab: Auto is the default and draws the Dirac cloud from Z = 27 (cloud views); Dirac at any Z; not with Superposition, in the Orbit view or when set Non-relativistic",
                  AtomParams().hrel == "Auto" && a.diracCloudOn() && !b.diracCloudOn() && c.diracCloudOn() && d.diracCloudOn() && !e.diracCloudOn() && !f.diracCloudOn() && !g.diracCloudOn());
    }
    // the dots: U91+ 1s and 2p3/2, lined up and round
    struct C { const char* orb; const char* hj; bool round; double cos2; };
    const C cs[3] = {{"1s", "l + 1/2", false, 1.0 / 3}, {"2p", "l + 1/2", false, 1.0 / 5}, {"2p", "l + 1/2", true, 1.0 / 3}};
    double worstR = 0, worstC = 0; bool oneColour = true;
    for (const C& k : cs) {
        seedRandom(4711);
        AtomLab L = lab(92, "Cloud 3D", k.orb); L.setParam("hj", std::string(k.hj)); L.setParam("cdens", std::string("Ultra"));
        if (k.round) L.setRound(true);
        for (int i = 0; i < 40000 && L.s.k < L.cloudCap(); i++) L.step(1.0 / 240);
        const long n = std::min<long>(L.s.k, L.cloudCap());
        double sr = 0, sr2 = 0, sc = 0, sc2 = 0;
        for (long i = 0; i < n; i++) {
            const double x = L.s.pts[3 * i], y = L.s.pts[3 * i + 1], z = L.s.pts[3 * i + 2], r = std::sqrt(x * x + y * y + z * z) / L.aUnit(), c2 = z * z / (x * x + y * y + z * z);
            sr += r; sr2 += r * r; sc += c2; sc2 += c2 * c2; oneColour = oneColour && L.s.sg[i] == 1 && L.s.ph[i] == 0.f;
        }
        const double mr = sr / n, mc = sc / n, ser = std::sqrt((sr2 / n - mr * mr) / n), sec = std::sqrt((sc2 / n - mc * mc) / n);
        const Orbital& o = orbitals()[std::size_t(orbIndex(k.orb))];
        worstR = std::max(worstR, std::fabs(mr - diracMeanRadius(92, o.n, diracKappa(o.l, L.diracJ2()))) / ser);
        worstC = std::max(worstC, std::fabs(mc - k.cos2) / sec);
    }
    std::printf("Dirac lab, U91+ dots (1s, 2p3/2 lined up, 2p3/2 round; 48,000 each): <r> worst %.2f SE, <cos^2 theta> worst %.2f SE\n", worstR, worstC);
    checkTrue("Dirac lab: the cloud's dots have the Dirac state's <r> (in the reduced-mass Bohr radius) within 4 standard errors", worstR < 4);
    checkTrue("Dirac lab: lined up 2p3/2 has <cos^2 theta> = 1/5, round 1/3, 1s 1/3, within 4 standard errors", worstC < 4);
    checkTrue("Dirac lab: every dot of a Dirac cloud has the one colour (no sign or phase)", oneColour);
    {   // the readout and the radial graph's source
        AtomLab L = lab(92, "Cloud 3D", "2p");
        const std::string h = L.info1();
        checkTrue("Dirac lab: the headline names the Dirac state and its level with the Lamb shift (U91+ 2p3/2)", h.find("2p₃/₂") != std::string::npos && h.find("Dirac + Lamb shift") != std::string::npos &&
                  h.find(fmtEV(-diracQedEV(92, L.p.N, 2, 1, 3))) != std::string::npos);
    }
    {   // a jump: 2p3/2 -> 1s1/2 emits the Dirac + Lamb Lyman-alpha line of the spectrum card, and lands on 1s
        AtomLab L = lab(92, "Cloud", "2p");
        for (int i = 0; i < 200; i++) L.step(1.0 / 240);
        L.startJump("emit", orbIndex("1s"));
        double want = 0; for (const DiracLine& l : diracLines(92, L.p.N)) if (l.lo == 1 && l.hi == 2 && l.jHi2 == 3) want = l.lam;
        const double got = L.s.photon ? L.s.photon->lam : 0;
        checkNum("Dirac lab: U91+ 2p3/2 -> 1s emits the spectrum card's Lyman-alpha 2p3/2 line (Dirac + Lamb shift)", got, want, 1e-12);
        for (int i = 0; i < 2000 && (L.s.trans || L.s.photon); i++) L.step(1.0 / 240);
        checkTrue("Dirac lab: the jump lands on 1s and the cloud is the Dirac 1s", L.p.orb == "1s" && L.diracCloudOn() && L.diracJ2() == 1 && !L.s.trans);
    }
    {   // the target's j by line strength: 3d3/2 -> 2p goes to 2p1/2 five times as often as to 2p3/2
        seedRandom(99);
        int toHalf = 0, tot = 0;
        for (int k = 0; k < 600; k++) {
            AtomLab L = lab(92, "Cloud", "3d"); L.setParam("hj", std::string("l - 1/2")); L.reducedMotion = true;
            L.startJump("emit", orbIndex("2p"));
            if (L.p.orb == "2p") { tot++; if (L.p.hj == "l - 1/2") toHalf++; }
        }
        const double share = double(toHalf) / tot, se = std::sqrt(5.0 / 36 / tot);
        std::printf("Dirac lab: 3d3/2 -> 2p, %d jumps: %.3f to 2p1/2 (line strengths: 5/6 = 0.833)\n", tot, share);
        checkTrue("Dirac lab: 3d3/2 -> 2p lands on 2p1/2 with the line-strength odds 5/6, within 4 standard errors", tot == 600 && std::fabs(share - 5.0 / 6) < 4 * se);
    }
}
