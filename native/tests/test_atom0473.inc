// 0.47.3: level jumps of the two-s-electron atoms (levels.h, tools/grasp/run_two.py) and the jump status of all 118 atoms. Written BEFORE the clouds existed (each cloud check failed
// with no data/jumps/grasp/<X>/clouds.bin). References, all independent of the app's own earlier output:
//  - NIST ASD energies (data/jumps/asd, the numbers below copied from those files): spot checks of the level table, and each computed excitation energy within a per-atom limit
//    set from this job's runs (data/jumps/grasp/<X>/levels.tsv; REPORT.md 0.47.3);
//  - M. S. Safronova, M. G. Kozlov, W. R. Johnson, D. Jiang, PRA 80, 012516 (2009), Table II (data/jumps/sources/ci_allorder_safronova2009_arxiv0905.2578.pdf): Mg CI+all-order energies;
//  - conservation: occupations add up to Z, the density integrates to Z; a line's vacuum wavenumber equals the difference of its two levels (Ritz: 1e7 / nm = E_hi - E_lo);
//  - angular momentum: a J = 0 or J = 1/2 level has no k > 0 multipole; the round (M_J-averaged) view draws every direction alike, <cos^2 theta> = 1/3;
//  - owner decisions D1 (a jump only between measured levels), D4 (every A shown is NIST's, or "rate not known"), D5 (no-jump atoms: no jumps, the section 7 wording).
static void testLevels0473() {
    using namespace atom;
    const struct { int Z; const char* X; const char* conf; const char* term; int j2; double cm; double limit; } atoms[] = {
        // a NIST level of each atom (ASD <X>I_levels.tsv.gz, as printed there), and the per-atom limit on |computed - NIST| / NIST of the excitation energies (%): the worst level,
        // rounded up (levels.tsv; 0.47.4 recomputed Be, Mg, Zn, Cd with one orbital set per atom and tightened their limits)
        {2, "He", "1s.2p", "1P*", 2, 171134.894441, 0.2}, {4, "Be", "1s2.2s.2p", "1P*", 2, 42565.4502, 2.0}, {12, "Mg", "3s.3p", "1P*", 2, 35051.264, 0.6},
        {30, "Zn", "3d10.4s.4p", "1P*", 2, 46745.4032, 3.5}, {48, "Cd", "4d10.5s.5p", "1P*", 2, 43692.384, 2.1}};
    for (const auto& A : atoms) {
        const std::string X = A.X;
        const std::vector<JLevel>& lv = jumpLevels(A.Z);
        int at = -1;
        for (int i = 0; i < int(lv.size()); i++) if (lv[std::size_t(i)].conf == A.conf && lv[std::size_t(i)].term == A.term && lv[std::size_t(i)].j2 == A.j2) at = i;
        checkTrue("0.47.3: " + X + ": the level table starts with the ground level 1S J=0 at 0 cm-1", !lv.empty() && lv[0].cm == 0 && lv[0].j2 == 0 && lv[0].term == "1S");
        checkTrue("0.47.3: " + X + ": " + A.conf + " " + A.term + " is NIST's " + fmt::fixed(A.cm, 4) + " cm-1", at >= 0 && std::fabs(lv[std::size_t(at)].cm - A.cm) < 1e-6);
        checkTrue("0.47.3: " + X + " has level jumps (clouds embedded, relativistic model)", levelJumps(A.Z));
        int occBad = 0, normBad = 0, roundBad = 0, worse = 0, n = 0;
        double worst = 0;
        std::string worstName;
        for (const int i : cloudLevelList(A.Z)) {
            const GraspLevel* g = levelCloud(A.Z, i);
            n++;
            if (std::fabs(g->nistCm - lv[std::size_t(i)].cm) > 1e-6) worse++;
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
            if (g->j2 <= 1 && !g->ks.empty()) roundBad++;
        }
        checkTrue("0.47.3: " + X + ": " + std::to_string(n) + " levels with clouds, every computed excitation energy within " + fmt::fixed(A.limit, 1) + " % of NIST (worst " + worstName + ": " +
                  fmt::fixed(worst, 2) + " %)", n >= 2 && worse == 0);
        checkTrue("0.47.3: " + X + ": the occupations of every level add up to Z = " + std::to_string(A.Z), n > 0 && occBad == 0);
        checkTrue("0.47.3: " + X + ": every level's density integrates to Z (1e-3)", n > 0 && normBad == 0);
        checkTrue("0.47.3: " + X + ": every J = 0 level is round (no k > 0 multipole)", n > 0 && roundBad == 0);
        // D1 and the lines: both levels measured; Ritz: the vacuum wavenumber is the levels' difference (the observed wavelength within 0.01 nm x 1e7/nm^2 of it)
        int d1 = 0, ritz = 0, rate = 0, offered = 0;
        for (int k = 0; k < int(jumpLines(A.Z).size()); k++) {
            const JLine& L = jumpLines(A.Z)[std::size_t(k)];
            if (!jumpOffered(A.Z, k)) continue;
            offered++;
            if (!(lv[std::size_t(L.hi)].measured && (lv[std::size_t(L.lo)].measured || L.lo == 0))) d1++;
            const double sig = lv[std::size_t(L.hi)].cm - lv[std::size_t(L.lo)].cm, nm = L.nm();
            if (!(std::fabs(1e7 / nm - sig) <= 1e7 / (nm * nm) * 0.01 + 0.05)) ritz++;
            const std::string t = rateText(L);
            // (0.47.4, D4: where NIST has no A the app's own is shown, marked; the reference text changed with that decision, test_atom0474.inc checks the wording)
            if (std::isfinite(L.aki) ? t.find("NIST ASD") == std::string::npos
                                     : std::isfinite(L.akiCalc) ? t.find("computed by this app (GRASP2018), not measured") == std::string::npos : t.rfind("rate not known", 0) != 0) rate++;
        }
        checkTrue("0.47.3: " + X + ": every offered jump (" + std::to_string(offered) + ") joins two measured levels (D1)", offered > 0 && d1 == 0);
        checkTrue("0.47.3: " + X + ": every offered line's wavelength matches the difference of its two NIST levels (Ritz)", ritz == 0);
        checkTrue("0.47.3: " + X + ": every rate shown is NIST's with its grade, else the app's own marked as computed (0.47.4), else \"rate not known\" (D4)", rate == 0);
        checkTrue("0.47.3: " + X + " has no D5 sentence (NIST has measured jumps)", noJumpText(A.Z).empty() && jumpStatus(A.Z) == JumpStatus::Measured);
        // the resonance line (the ground level to ns np 1P J=1, at index "at") is offered: an atom ships only with it (REPORT.md 0.47.3)
        checkTrue("0.47.3: " + X + ": the resonance line, ground -> " + A.conf + " " + A.term + " J=1, is offered", at >= 0 && [&] { for (const int k : linesBetween(A.Z, 0, at)) if (jumpOffered(A.Z, k)) return true; return false; }());
    }
    // Mg against CI+all-order (Safronova et al. 2009, Table II; energies from the ground level, cm-1): our correlated calculation within 5 % of theirs for each level both have
    // (the limit set from this job's runs: their CI+all-order is within 0.1 % of NIST, so this is our own accuracy again, checked against an independent calculation)
    {   const struct { const char* conf; const char* term; int j2; double ciAll; } mg[] = {
            {"3s.4s", "3S", 2, 41175}, {"3s.4s", "1S", 0, 43502}, {"3s.3d", "1D", 4, 46384}, {"3s.3d", "3D", 2, 47936}, {"3s.3p", "3P*", 0, 21833},
            {"3s.3p", "3P*", 2, 21852}, {"3s.3p", "3P*", 4, 21897}, {"3s.3p", "1P*", 2, 35065}};
        int bad = 0, have = 0;
        std::string worst;
        for (const auto& m : mg)
            for (int i = 0; i < int(jumpLevels(12).size()); i++) {
                const JLevel& L = jumpLevels(12)[std::size_t(i)];
                if (L.conf != m.conf || L.term != m.term || L.j2 != m.j2) continue;
                const GraspLevel* g = levelCloud(12, i);
                if (!g) continue;
                have++;
                const double d = 100 * (g->computedCm - m.ciAll) / m.ciAll;
                if (std::fabs(d) > 1.0) { bad++; worst += levelName(12, i) + " " + fmt::fixed(d, 2) + " % "; }
            }
        checkTrue("0.47.3: Mg: each computed level within 1 % (0.47.4; was 5 %) of the CI+all-order value of Safronova et al. 2009 Table II (" + std::to_string(have) + " of 8 levels; " + worst + ")", have >= 6 && bad == 0); }
    // the two-s-electron atoms whose clouds come later (0.47.3: all six; 0.47.4 ships those it could compute, test_atom0474.inc): level tables, no jumps yet, said so
    for (const int Z : {20, 38, 56, 70, 80, 88}) {
        if (levelJumps(Z)) continue;
        AtomLab L; L.p = AtomParams(); L.reducedMotion = true; L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", double(Z));
        L.absorb();
        // (0.52.0: the reason says "were not computed": the element jumps are paused; it said "not computed yet")
        checkTrue("0.47.3: Z = " + std::to_string(Z) + ": NIST levels kept, no clouds, no jumps, the reason says so", !jumpLevels(Z).empty() && !levelJumps(Z) && !L.levelOn() && !L.s.photon && L.s.msg.find("were not computed") != std::string::npos);
    }
    // D5: every atom has a status; the ones without measured jumps say why and offer nothing
    {   int noStatus = 0, wrong = 0;
        for (int Z = 1; Z <= 118; Z++) {
            const JumpStatus st = jumpStatus(Z);
            if (st == JumpStatus::Measured) { if (!noJumpText(Z).empty()) wrong++; continue; }
            if (noJumpText(Z).empty()) noStatus++;
        }
        checkTrue("0.47.3: every atom without NIST jumps has its D5 sentence (Se, Zr, Nb, Pr, Pm, Re, Os, Pa-Es, Fm, No, Md, Lr, Rf-Og)", noStatus == 0 && wrong == 0);
        checkTrue("0.47.3: 83 atoms have measured jumps in NIST (elements.tsv), Og has none", [] { int k = 0; for (int Z = 1; Z <= 118; Z++) k += jumpStatus(Z) == JumpStatus::Measured; return k; }() == 83 &&
                  noJumpText(118) == "No jumps: nobody has measured light from this atom yet.");
        checkTrue("0.47.3: selenium: levels only", noJumpText(34) == "Measured levels exist, but NIST has not matched its lines to them yet; no jumps are shown.");
        checkTrue("0.47.3: uranium: measured, not in NIST", jumpStatus(92) == JumpStatus::NotInNist);
        for (const int Z : {34, 92, 102, 110, 118}) {
            AtomLab L; L.p = AtomParams(); L.reducedMotion = true; L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", double(Z));
            const bool none = !L.levelOn() && !L.valenceOn() && L.targets(1).empty() && L.targets(-1).empty();
            L.absorb();
            checkTrue("0.47.3: Z = " + std::to_string(Z) + ": no jump offered or made, and the reason is the D5 sentence", none && !L.s.photon && !L.s.trans && L.s.msg == noJumpText(Z));
        }
    }
    // the lab: magnesium
    {   AtomLab L; L.p = AtomParams(); L.reducedMotion = true; L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", 12.0);
        checkTrue("0.47.3: magnesium starts in its ground level 3s2 1S J=0, drawn from its GRASP cloud", L.levelOn() && L.levelNow() == 0 && L.manyAtom() == &levelCloud(12, 0)->atom);
        int p1 = -1, p3 = -1;
        for (int i = 0; i < int(jumpLevels(12).size()); i++) {
            const JLevel& q = jumpLevels(12)[std::size_t(i)];
            if (q.conf == "3s.3p" && q.term == "1P*" && q.j2 == 2) p1 = i;
            if (q.conf == "3s.3p" && q.term == "3P*" && q.j2 == 4) p3 = i;
        }
        std::vector<int> up = L.targets(1);
        checkTrue("0.47.3: from the ground, 3s3p 1P J=1 can be reached by absorbing (the 285.3 nm line)", std::find(up.begin(), up.end(), p1) != up.end());
        checkTrue("0.47.3: ... but not 3s3p 3P J=2 (no E1 line from J = 0 to J = 2)", std::find(up.begin(), up.end(), p3) == up.end());
        checkTrue("0.47.3: placing the atom in 3s3p 1P J=1 draws that level's cloud, no photon", L.placeLevel(p1) && L.levelNow() == p1 && L.manyAtom() == &levelCloud(12, p1)->atom && !L.s.photon);
        bool down = false;
        for (const int t : L.targets(-1)) down |= t == 0;
        L.emit();
        // NIST ASD Mg I: 3s2 1S0 - 3s3p 1P1 at 285.2965 nm (vacuum, observed), A = 4.91e8 /s (grade A)
        checkTrue("0.47.3: 3s3p 1P J=1 emits to the ground (285.30 nm) or to a lower level, and lands there", down && L.s.photon && L.levelNow() != p1);
        L.placeLevel(p3);
        checkTrue("0.47.3: the readout names the level, the lined-up state and the quality figure", anyHas46(L.info(), "3s3p ³P° J=2") && anyHas46(L.info(), "lined up") && anyHas46(L.info(), "cloud quality"));
        // D6: the round view: every direction alike (all dots), <cos^2 theta> = 1/3
        L.setRound(true);
        for (int i = 0; i < 900; i++) L.step(1.0 / 60);
        double s = 0; long cnt = 0;
        const long cap = std::min<long>(L.s.k, long(L.s.sg.size()));
        for (long i = 0; i < cap; i++) {
            const double x = L.s.pts[3 * i], y = L.s.pts[3 * i + 1], z = L.s.pts[3 * i + 2], r2 = x * x + y * y + z * z;
            if (r2 > 0) { s += z * z / r2; cnt++; }
        }
        checkNum("0.47.3: Mg 3s3p 3P J=2, round view: every direction alike, <cos^2 theta> = 1/3 (" + std::to_string(cnt) + " dots)", cnt ? s / double(cnt) : 0.0, 1.0 / 3, 0, 4 * std::sqrt(4.0 / 45 / std::max<long>(cnt, 1)));
        checkTrue("0.47.3: ... and the readout says it is drawn round, the free atom", anyHas46(L.info(), "drawn round"));
        L.setParam("mrel", std::string("Non-relativistic"));
        checkTrue("0.47.3: magnesium in the non-relativistic model: no level jumps (its clouds are relativistic)", !L.levelOn() && L.targets(1).empty()); }
    // iron: measured in NIST, no clouds yet: no jumps, and the reason says so
    {   AtomLab L; L.p = AtomParams(); L.reducedMotion = true; L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", 26.0);
        L.absorb();
        // (0.52.0: the sentence says "were not computed" and names the atoms that have jumps: the element jumps are paused, so it promises no more; it said "not computed yet")
        checkTrue("0.47.3: iron: no jumps, the reason says its clouds were not computed and names Ra among the atoms with jumps", !L.levelOn() && L.targets(1).empty() &&
                  L.s.msg.find("were not computed") != std::string::npos && L.s.msg.find("Ra have jumps") != std::string::npos && L.s.msg.find("yet") == std::string::npos); }
}
