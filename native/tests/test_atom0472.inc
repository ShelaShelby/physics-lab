// 0.47.2: the GRASP2018 clouds of Li, Na, K, Rb, Cs and Fr (alkali.h, tools/grasp). Written BEFORE the data and the code that reads it (each check failed with no clouds.bin).
// References, all independent of the app's own earlier output:
//  - NIST ASD levels (src/atom/alkali_data.inc, made from data/asd/alkali and data/jumps/asd by tools/make_alkali_data.py): each computed excitation energy within a per-atom
//    limit set from the runs of this job (data/jumps/grasp/<X>/levels.tsv; REPORT.md 0.47.2);
//  - conservation: the natural orbitals' occupations add up to Z; the stored orbitals are normalised, so the density integrates to Z;
//  - angular momentum: a J = 1/2 level has no k > 0 multipole (round in every M_J state, PLAN_JUMPS.md section 3); the M_J = J state of ONE electron j outside closed shells has
//    integral D_k dr = (2k+1) <C^k_0> = -1 (j = 3/2, k = 2), -10/7 and 3/7 (j = 5/2, k = 2, 4): the textbook spinor densities (3/8 pi) sin^2 theta and (15/32 pi) sin^4 theta;
//  - the 0.47.1 lines (counts, wavelengths and A values of Li..Cs) unchanged; francium's from NIST ASD Fr I.
static void testGrasp0472() {
    using namespace atom;
    const struct { int Z; const char* X; double limit; } atoms[] = {
        // per-atom limits on |computed - NIST| / NIST of the excitation energies (%), from this job's runs (the worst level of each atom, rounded up)
        {3, "Li", 1.0}, {11, "Na", 0.5}, {19, "K", 2.5}, {37, "Rb", 2.0}, {55, "Cs", 4.0}, {87, "Fr", 2.5}};
    for (const auto& A : atoms) {
        const std::string X = A.X;
        checkTrue("0.47.2: " + X + " has GRASP clouds and valence jumps", graspHas(A.Z) && alkaliJumps(A.Z));
        int missing = 0, worse = 0, occBad = 0, normBad = 0, jHalf = 0;
        double worst = 0;
        std::string worstName;
        for (const AlkLevel& v : alkaliLevels(A.Z)) {
            const GraspLevel* g = graspLevel(A.Z, v.n, v.l, v.j2);
            const std::string nm = std::to_string(v.n) + "spdf"[v.l] + std::to_string(v.j2) + "/2";
            if (!g) { missing++; continue; }
            checkNum("0.47.2: " + X + " " + nm + ": the stored NIST energy is NIST's", g->nistCm, v.cm, 0, 1e-6);
            if (v.cm > 0) {
                const double q = std::fabs(g->qualityPercent());
                if (q > worst) { worst = q; worstName = nm; }
                if (!(q <= A.limit)) worse++;
            }
            double occ = 0, integral = 0;
            for (const hf::Subshell& sh : g->atom.sub)
                for (const hf::Part& pt : sh.parts) {
                    occ += pt.occ;
                    double s = 0;                                  // trapezoid on a fine log grid from 1e-6 to 400 bohr
                    const int N = 20000; const double t0 = std::log(1e-6), t1 = std::log(400.0);
                    for (int i = 0; i <= N; i++) { const double r = std::exp(t0 + (t1 - t0) * i / N); s += (i == 0 || i == N ? 0.5 : 1.0) * partDensity(pt, sh.l, r) * r; }
                    integral += pt.occ * s * (t1 - t0) / N;
                }
            if (std::fabs(occ - A.Z) > 1e-6) occBad++;
            if (std::fabs(integral - A.Z) > 1e-3 * A.Z) normBad++;
            if (v.j2 == 1 && !g->ks.empty()) jHalf++;
            if (v.j2 == 1) { double b = 0; for (double c : {-1.0, -0.3, 0.0, 0.7, 1.0}) if (graspShape(*g, 3.0, c, b) != 1.0) jHalf++; }
        }
        checkTrue("0.47.2: " + X + ": every level the app uses has a cloud", missing == 0);
        checkTrue("0.47.2: " + X + ": every computed excitation energy within " + fmt::fixed(A.limit, 1) + " % of NIST (worst " + worstName + ": " + fmt::fixed(worst, 2) + " %)", worse == 0 && missing == 0);
        checkTrue("0.47.2: " + X + ": the occupations of every level add up to Z = " + std::to_string(A.Z), occBad == 0);
        checkTrue("0.47.2: " + X + ": every level's density integrates to Z (stored orbitals normalised, 1e-3)", normBad == 0);
        checkTrue("0.47.2: " + X + ": every J = 1/2 level is round (no k > 0 multipole)", jHalf == 0);
    }
    // one electron j outside closed shells, M_J = J: the k > 0 multipoles integrate to the spinor's (2k+1)<C^k_0>. Core polarization shields them a little (the core takes on
    // the opposite shape): this job's runs, Na 3p3/2 0.3 %, K 4p3/2 1.0 %, Cs 6p3/2 2.1 %, Cs 5d3/2 4.9 %, Fr 6d3/2 4.8 % (compact d orbitals of the heavy atoms most): within 6 %
    {   int bad = 0, n = 0;
        for (const auto& A : atoms)
            for (const AlkLevel& v : alkaliLevels(A.Z)) {
                const GraspLevel* g = graspLevel(A.Z, v.n, v.l, v.j2);
                if (!g || v.j2 == 1) continue;
                for (std::size_t i = 0; i < g->ks.size(); i++) {
                    const double want = v.j2 == 3 ? (g->ks[i] == 2 ? -1.0 : 0.0) : (g->ks[i] == 2 ? -10.0 / 7 : g->ks[i] == 4 ? 3.0 / 7 : 0.0);
                    double s = 0; const int N = 20000; const double t0 = std::log(g->r0), t1 = std::log(400.0);
                    for (int k = 0; k <= N; k++) { const double r = std::exp(t0 + (t1 - t0) * k / N); s += (k == 0 || k == N ? 0.5 : 1.0) * graspDk(*g, i, r) * r; }
                    s *= (t1 - t0) / N;
                    n++;
                    if (!(std::fabs(s - want) <= 0.06 * std::fabs(want))) bad++;
                }
            }
        checkTrue("0.47.2: every J > 1/2 level's multipoles integrate to the one-electron spinor values within 6 % (" + std::to_string(n) + " multipoles)", bad == 0 && n > 0); }
    // the one-configuration Na 3p 2P3/2 (Dirac-Fock, kept as a test level): D_2(r) = -D_3p(r) at every r (the shape is (3/8 pi) sin^2 theta exactly, large and small components alike),
    // and the whole atom's dots: <P_2(cos theta)> = integral D_2 / (5 x 11) = -1/55
    {   const GraspLevel* g = graspTestLevel(11);
        checkTrue("0.47.2: the one-configuration Na 3p 2P3/2 test level is there", g && g->n == 3 && g->l == 1 && g->j2 == 3 && g->ks.size() == 1 && g->ks[0] == 2);
        if (g && !g->ks.empty()) {
            const int vi = hf::subshellIndex(g->atom, 3, 1);
            double worst = 0, top = 0;
            for (int i = 0; i < 400 && vi >= 0; i++) {
                const double r = 0.01 * std::pow(10.0, 4.0 * i / 399);
                double dv = 0;
                for (const hf::Part& pt : g->atom.sub[std::size_t(vi)].parts) dv += pt.occ * partDensity(pt, 1, r);
                top = std::max(top, dv); worst = std::max(worst, std::fabs(graspDk(*g, 0, r) + dv));
            }
            checkTrue("0.47.2: Na 3p 2P3/2, one configuration: D_2(r) = -D_3p(r), the p3/2 M = 3/2 spinor density (3/8 pi) sin^2 theta (within " + fmt::fixed(worst / std::max(top, 1e-300), 7) + " of the top)",
                      vi >= 0 && worst <= 1e-4 * top);   // (float32 storage and the 4-point interpolation of D_2, P and Q: about 1e-5)
            orbit::Mulberry32 rng(12345);
            const long N = 400000;
            double s = 0;
            for (long i = 0; i < N; i++) { double x, y, z; int sh; graspSample(*g, rng, x, y, z, sh); const double r = std::sqrt(x * x + y * y + z * z), c = z / r; s += 0.5 * (3 * c * c - 1); }
            const double m = s / N, sd = std::sqrt(0.2 / N);       // <P_2^2> = 1/5 for directions near uniform
            checkNum("0.47.2: Na 3p 2P3/2, one configuration: the whole atom's dots give <P_2(cos theta)> = -1/55 (" + std::to_string(N) + " dots)", m, -1.0 / 55, 0, 4 * sd);
        }
    }
    // the lines and A values are those of 0.47.1 (Li..Cs: counts, sum of wavelengths, sum of A, from 0.47.1's alkali_data.inc) and NIST's for francium
    {   const struct { int Z; int n; double nm, a; } ref[] = {{3, 24, 38930.868000, 2.768009e+08}, {11, 24, 55093.002191, 3.224459e+08}, {19, 24, 45185.977544, 1.824531e+08},
                                                             {37, 18, 23496.601390, 7.753000e+07}, {55, 20, 33904.446446, 6.593070e+07}};
        for (const auto& R : ref) {
            double nm = 0, a = 0; int n = 0;
            for (const AlkLine& L : alkaliLines(R.Z)) { n++; nm += L.nm(); if (std::isfinite(L.aki)) a += L.aki; }
            checkTrue("0.47.2: Z = " + std::to_string(R.Z) + ": the lines and A values are 0.47.1's (" + std::to_string(n) + " lines)", n == R.n && std::fabs(nm - R.nm) < 1e-5 && std::fabs(a - R.a) < 1e-6 * R.a);
        }
        const std::vector<AlkLine> fr = alkaliLines(87);
        bool d2 = false;
        for (const AlkLine& L : fr) d2 |= L.nLo == 7 && L.lLo == 0 && L.nHi == 7 && L.lHi == 1 && L.j2Hi == 3 && std::fabs(L.nm() - 718.1845) < 1e-6 && std::fabs(L.aki - 4.78e7) < 1;
        checkTrue("0.47.2: francium: 7 NIST E1 lines among its 10 levels, 7s-7p3/2 at 718.1845 nm with A = 4.78e7 /s (NIST ASD Fr I)", fr.size() == 7 && d2);
        checkTrue("0.47.2: francium: 10 NIST levels (7s 8s 7p 8p 6d 7d; 9s left out, its cloud 11-20 % off), 7s 2S1/2 the ground",
                  alkaliLevels(87).size() == 10 && alkaliOrbitals(87).size() == 6 && alkaliOrbitals(87)[0].n == 7 && alkaliOrbitals(87)[0].l == 0); }
    // the lab: the level has a J; from 3p 2P3/2 only D2; a placed electron takes the term's highest J; the readout names the aligned state and the quality figure
    {   AtomLab L; L.p = AtomParams(); L.reducedMotion = true; L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", 11.0);
        checkTrue("0.47.2: sodium's ground atom is the GRASP 3s 2S1/2 cloud", L.manyAtom() == alkaliAtom(11, 3, 0, true) && graspOfAtom(L.manyAtom()) == graspLevel(11, 3, 0, 1));
        bool onlyD2 = true;
        for (int i = 0; i < 12; i++) { L.placeValence(3, 1); onlyD2 &= L.valenceJ2() == 3 && L.manyAtom() == &graspLevel(11, 3, 1, 3)->atom; L.emit(); onlyD2 &= L.s.photon && std::fabs(L.s.photon->lam - 589.1583264) < 1e-6; L.s.photon.reset(); }
        checkTrue("0.47.2: sodium placed in 3p is 2P3/2 and emits only D2 (589.158 nm), 12 of 12", onlyD2);
        checkTrue("0.47.2: after D2 the outer electron is in 3s 2S1/2", L.valenceNow().n == 3 && L.valenceNow().l == 0 && L.manyAtom() == &graspLevel(11, 3, 0, 1)->atom);
        L.placeValence(3, 1);
        checkTrue("0.47.2: the readout names 3p 2P3/2, the aligned state M_J = 3/2 and the round average", anyHas46(L.info(), "3p 2P3/2") && anyHas46(L.info(), "M_J = 3/2") && anyHas46(L.info(), "average is round"));
        checkTrue("0.47.2: the readout gives the cloud's quality figure against NIST", anyHas46(L.info(), "cloud quality") && anyHas46(L.info(), "% from NIST"));
        checkTrue("0.47.2: emitting the D1 component (clicked) starts from 3p 2P1/2 and lands in 3s", L.emitValenceLine(3, 1, 3, 0, 589.7558147) && L.s.photon && std::fabs(L.s.photon->lam - 589.7558147) < 1e-6 && L.valenceNow().n == 3 && L.valenceNow().l == 0);
        L.s.photon.reset();
        L.setValence(3, 1); L.s.valJ2 = 1;                     // the electron in 3p 2P1/2 (as after absorbing D1)
        const bool d1 = L.valenceJ2() == 1 && L.manyAtom() == &graspLevel(11, 3, 1, 1)->atom;
        checkTrue("0.47.2: sodium 3p 2P1/2 is drawn from its own cloud", d1);
        checkTrue("0.47.2: ... and the readout says a J = 1/2 level is round in every M_J state", anyHas46(L.info(), "J = 1/2 level is round"));
        // the outer electron's dots in 3p 2P1/2: every direction alike, <cos^2 theta> = 1/3
        for (int i = 0; i < 1200; i++) L.step(1.0 / 60);
        const float ph = float(AtomLab::shellPhase(3));
        double s = 0; long cnt = 0;
        const long cap = std::min<long>(L.s.k, long(L.s.sg.size()));
        for (long i = 0; i < cap; i++) {
            if (L.s.ph[i] != ph) continue;
            const double x = L.s.pts[3 * i], y = L.s.pts[3 * i + 1], z = L.s.pts[3 * i + 2], r2 = x * x + y * y + z * z;
            if (r2 > 0) { s += z * z / r2; cnt++; }
        }
        checkNum("0.47.2: Na 3p 2P1/2: the n = 3 dots are round, <cos^2 theta> = 1/3 (" + std::to_string(cnt) + " dots)", cnt ? s / double(cnt) : 0.0, 1.0 / 3, 0, 4 * std::sqrt(4.0 / 45 / std::max<long>(cnt, 1)));
        // 3p 2P3/2, M_J = 3/2: flattened towards the equator. The outer electron alone (sin^2 theta) gives <cos^2 theta> = 1/5; where the round core overlaps it the shape fades, so the
        // dots lie between 1/5 and 1/3 (this job: 0.23)
        AtomLab M; M.p = AtomParams(); M.reducedMotion = true; M.restart(); M.setParam("view", std::string("Cloud 3D")); M.setParam("Z", 11.0);
        M.setValence(3, 1); M.s.valJ2 = 3;
        for (int i = 0; i < 1200; i++) M.step(1.0 / 60);
        double s3 = 0; long c3 = 0;
        const long cap3 = std::min<long>(M.s.k, long(M.s.sg.size()));
        for (long i = 0; i < cap3; i++) {
            if (M.s.ph[i] != ph) continue;
            const double x = M.s.pts[3 * i], y = M.s.pts[3 * i + 1], z = M.s.pts[3 * i + 2], r2 = x * x + y * y + z * z;
            if (r2 > 0) { s3 += z * z / r2; c3++; }
        }
        const double m3 = c3 ? s3 / double(c3) : 0.0, sd3 = std::sqrt(4.0 / 45 / std::max<long>(c3, 1));
        checkTrue("0.47.2: Na 3p 2P3/2: the n = 3 dots are flattened, 1/5 <= <cos^2 theta> < 1/3 (" + fmt::fixed(m3, 4) + ", " + std::to_string(c3) + " dots)", m3 > 0.2 - 4 * sd3 && m3 < 1.0 / 3 - 10 * sd3); }
    {   AtomLab L; L.p = AtomParams(); L.reducedMotion = true; L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", 87.0);
        checkTrue("0.47.2: francium has valence jumps (GRASP clouds; NIST lines)", L.valenceOn() && L.valenceChoices().size() == 6);
        std::string up; for (const AlkOrbital& o : L.valenceTargets(1)) up += std::to_string(o.n) + "spdf"[o.l] + " ";
        checkTrue("0.47.2: francium 7s absorbs to 7p and 8p (its NIST lines from 7s; got: " + up + ")", up == "7p 8p ");
        L.setParam("mrel", std::string("Non-relativistic"));
        checkTrue("0.47.2: francium in the non-relativistic model: no valence jumps (its clouds are relativistic only)", !L.valenceOn()); }
}
