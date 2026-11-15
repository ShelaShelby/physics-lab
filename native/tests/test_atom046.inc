// 0.46: the atom review (A1-A11), jumps up to n = 6 with the strongest route (B1, B2), jumps of the one-valence-electron atoms (B3). Written BEFORE the code.
// Reference numbers are typed from the saved sources (native/data/asd, data/asd/alkali, data/alkali, data/qcd), or are textbook formulas; never taken from the app.

static bool has46(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }
static bool anyHas46(const std::vector<std::string>& v, const std::string& part) { for (const std::string& s : v) if (has46(s, part)) return true; return false; }

static void testReview046A() {
    using namespace atom;
    // A2, A3: every electron by default; the one-electron choice renamed, old saved value still loads
    checkStr("A2: Electrons defaults to All", AtomParams().elec, "All");
    checkTrue("A3: 'Hydrogen-like' is a valid Electrons choice", AtomLab::validChoice("elec", "Hydrogen-like"));
    checkTrue("A3: the old 'One' is no longer a choice", !AtomLab::validChoice("elec", "One"));
    checkStr("A3: a saved 'One' loads as 'Hydrogen-like'", AtomLab::migrateChoice("elec", "One"), "Hydrogen-like");
    {   AtomLab L; L.p = AtomParams(); L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", 26.0);
        checkTrue("A2: iron starts as the neutral atom (every electron)", L.manyOn());
        L.setParam("Z", 1.0);
        checkTrue("A2: hydrogen (no many-electron atom) stays one electron", !L.manyOn());
        L.setParam("elec", std::string("One"));
        checkStr("A3: setParam('elec', 'One') (old scripts) lands on Hydrogen-like", L.p.elec, "Hydrogen-like"); }
    // A4: heavy one-electron ion: the headline energy is the Dirac value (point nucleus), the non-relativistic one marked as such
    {   AtomLab L; L.p = AtomParams(); L.restart(); L.setParam("elec", std::string("Hydrogen-like")); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", 92.0);
        const std::string h = L.info1();
        const double d = diracEV(92, L.p.N, 1, 1);   // 1s1/2 (dirac.h): mc^2 (1 - sqrt(1 - (Z alpha)^2)) reduced, about -132 keV
        checkTrue("A4: the 1s Dirac energy of U91+ is about -132 keV (" + std::to_string(d) + " eV)", d < -131.5e3 && d > -133.0e3);
        checkTrue("A4: the U91+ headline gives the Dirac energy first (" + h + ")", has46(h, "E −" + fmtEV(-d)) && has46(h, "Dirac"));
        checkTrue("A4 (0.49.3): the Dirac cloud's headline (the default at U) gives the Dirac level with the Lamb shift and its state", has46(h, "Dirac + Lamb shift") && has46(h, "1s₁/₂"));
        L.setParam("hrel", std::string("Non-relativistic"));
        checkTrue("A4: with the non-relativistic cloud the non-relativistic number is labelled", has46(L.info1(), "non-relativistic") && has46(L.info1(), "Dirac"));
        L.setParam("hrel", std::string("Auto"));
        L.setParam("Z", 8.0);
        checkTrue("A4: light ions keep the plain headline (no Dirac word)", !has46(L.info1(), "Dirac")); }
    // A5: a lone neutron has no positive charge cloud
    checkTrue("A5: no charge-density dots for a lone neutron", nucleusChargeDots(0, 1, 500, 0, 0, 1, 3u).empty());
    checkTrue("A5: the proton keeps its charge cloud", nucleusChargeDots(1, 0, 500, 0, 0, 1, 3u).size() == 1500);
    // A6: the texts say what the lab now draws
    checkTrue("A6: the radius explanation no longer says 'uniform ball' is drawn", !has46(chargeRadiusExplain(), "draws a nucleus as a uniform ball"));
    checkTrue("A6: ... and no longer says quadrupole moments are not used", !has46(chargeRadiusExplain(), "not used yet"));
    checkTrue("A6: ... and names the Fermi (Woods-Saxon) density", has46(chargeRadiusExplain(), "Fermi"));
    checkTrue("A6: the size lines of iron-56 do not say 'Drawn: a uniform ball'", !anyHas46(nuclearSizeLines(26, 30), "Drawn: a uniform ball"));
    // A7: the shape estimate is said to be the lab's own unsourced rule
    checkTrue("A7 (0.46.3): the unpublished estimate is gone: FRDM is labelled a calculation, a nucleus without any published value says so", has46(shapeSourceLabel(ShapeSource::FRDM, 50), "FRDM") && has46(shapeSourceLabel(ShapeSource::None, 5), "no published value"));
    checkTrue("A7: measured labels unchanged", has46(shapeSourceLabel(ShapeSource::MeasuredMoment, 50), "measured quadrupole moment"));
    // A7: an unmeasured light nucleus is sized by the usual rule (r = 1.2 A^(1/3) fm, the card's words), as the Density look already was: the protons' point rms of a snapshot is
    // sqrt(pointProtonMs) of the rule ball, about 1.74 fm for carbon-9 (no measured radius in the IAEA table), not the old 1.93 fm (1.68 + 0.085 (A - 4) fm)
    {   checkTrue("A7: carbon-9 has no measured radius (the test's premise)", !sizedByMeasurement(6, 3));
        double s2 = 0; int np = 0;
        for (int k = 0; k < 40; k++) { const std::vector<Nucleon> v = buildNucleus(6, 3); double cx = 0, cy = 0, cz = 0; for (const Nucleon& q : v) { cx += q.x; cy += q.y; cz += q.z; }
            cx /= v.size(); cy /= v.size(); cz /= v.size();
            for (const Nucleon& q : v) if (q.isP) { const double x = (q.x - cx) / kFmLab, y = (q.y - cy) / kFmLab, z = (q.z - cz) / kFmLab; s2 += x * x + y * y + z * z; np++; } }
        const double got = std::sqrt(s2 / np), want = std::sqrt(pointProtonMs(6, 3));
        checkNum("A7: carbon-9's drawn proton rms follows the rule ball (" + std::to_string(got) + " fm)", got, want, 0.08); }
    // A8: point nucleons are sampled with the point-nucleon diffuseness (Chamon et al. 2002: 0.50 fm), the charge density keeps the charge one (0.54 fm)
    checkNum("A8: point-nucleon diffuseness 0.50 fm (Chamon et al., PRC 66, 014610 (2002))", kWsPointDiffuseFm, 0.50, 0);
    checkNum("A8: charge diffuseness unchanged, 0.54 fm", kWsDiffuseFm, 0.54, 0);
    checkTrue("A8: folding a 0.50 fm point edge with the nucleon size gives about the charge edge: sqrt(0.50^2 + <r^2>_N / pi^2) within 0.03 of 0.54",
              std::fabs(std::sqrt(0.25 + nucleonSizeMs(26, 30) / (kPi * kPi)) - 0.54) < 0.03);
    // A9: one rule for the proton and the deuteron: CODATA 2022 (data/qcd/nucleon/codata2022_rd.html: 2.127 78(27) fm)
    checkTrue("A9: the deuteron's radius row exists", chargeRadius(1, 1) != nullptr);
    if (chargeRadius(1, 1)) { checkNum("A9: deuteron rms charge radius = CODATA 2022 2.12778 fm", chargeRadius(1, 1)->rms, 2.12778, 0, 1e-9);
                              checkNum("A9: ... its uncertainty 0.00027 fm", chargeRadius(1, 1)->unc, 0.00027, 0, 1e-12); }
    checkTrue("A9: the deuteron's size line names CODATA 2022", anyHas46(nuclearSizeLines(1, 1), "CODATA 2022"));
    // A10, A11: the words where the picture is seen
    {   AtomLab L; L.p = AtomParams(); L.restart(); L.setParam("elec", std::string("Hydrogen-like")); L.setParam("view", std::string("Orbit")); L.setParam("ostyle", std::string("Modern"));
        L.setParam("Z", 1.0);
        checkTrue("A11: the Modern Orbit readout says the circles are Bohr's model", anyHas46(L.info(), "Bohr")); }
    checkTrue("A10: the nucleon-ball words say the drawn balls overlap (a drawing choice)", has46(nucleonBallNote(), "overlap") && has46(nucleonBallNote(), "1.0 fm"));
}

// ---- B1: levels up to n = 6; the n = 5, 6 hydrogen-like orbitals are the exact functions ----
static void testJumpsN6() {
    using namespace atom;
    checkNum("B1: the animated levels go up to n = 6", AtomLab::kLineMaxN, 6, 0);
    for (int n = 5; n <= 6; n++)
        for (int l = 0; l <= 3; l++) {
            const std::string sh = std::to_string(n) + "spdf"[l];
            int idx = -1;
            for (int k = 0; k < int(orbitals().size()); k++) if (orbitals()[std::size_t(k)].shell == sh) { idx = k; break; }
            checkTrue("B1: orbital " + sh + " exists", idx >= 0);
            if (idx < 0) continue;
            // radial integrals on a grid, Z = 1, along a direction where the angular part is not zero (the angular part is normalised on the sphere: integrate psi^2 over all space
            // by Monte Carlo would be slow, so check the radial factor: psi(r u) / psi(r0 u) is R(r)/R(r0) for a fixed direction u)
            const double ux = 0.48, uy = 0.36, uz = 0.8;   // a generic unit direction (0.48^2 + 0.36^2 + 0.8^2 = 1)
            double norm = 0, mean = 0, nodes = 0, prev = 0;
            const double h = 0.01;
            for (double r = h; r < 400; r += h) {
                const double v = psi(idx, 1, r * ux, r * uy, r * uz);
                norm += v * v * r * r * h; mean += v * v * r * r * r * h;
                if (prev != 0 && v != 0 && (v > 0) != (prev > 0)) nodes++;
                if (v != 0) prev = v;
            }
            // <r> = (3 n^2 - l (l + 1)) / (2 Z) a0 (Bethe and Salpeter, eq. 3.24)
            checkNum("B1: " + sh + " <r> = (3n^2 - l(l+1))/2 a0", mean / norm, (3.0 * n * n - l * (l + 1)) / 2, 1e-4);
            checkNum("B1: " + sh + " has n - l - 1 radial nodes", nodes, n - l - 1, 0);
            checkNum("B1: " + sh + " <r> from the app's formula", meanRadius(n, l, 1), (3.0 * n * n - l * (l + 1)) / 2, 1e-12);
        }
    // photon of 6 -> 2 (H-delta) from the lab: the Balmer formula with the reduced mass; NIST 410.2892 nm (observed, vacuum)
    {   AtomLab L; L.p = AtomParams(); L.restart(); L.setParam("elec", std::string("Hydrogen-like")); L.setParam("view", std::string("Orbit")); L.setParam("Z", 1.0);
        L.setParam("n", 6.0);
        checkNum("B1: the n slider reaches 6", L.p.n, 6, 0);
        checkTrue("B1: H-delta can be emitted (6 -> 2)", L.emitLine(6, 2));
        checkTrue("B1: its photon is H-delta, within 0.01 % of NIST 410.2892 nm", L.s.photon && std::fabs(L.s.photon->lam / 410.2892 - 1) < 1e-4 && has46(L.s.photon->name, "Balmer-δ"));
        L = AtomLab(); L.p = AtomParams(); L.restart(); L.setParam("elec", std::string("Hydrogen-like")); L.setParam("view", std::string("Orbit")); L.setParam("Z", 1.0); L.setParam("n", 6.0);
        checkTrue("B1: 6 -> 5 (Pfund-alpha) can be emitted", L.emitLine(6, 5) && L.s.photon && has46(L.s.photon->name, "Pfund-α")); }
}

// ---- B2: a line with several routes draws the strongest (largest Aki of NIST ASD, summed over the lower level's j) ----
static void testStrongestRoute() {
    using namespace atom;
    // typed from data/asd/asd_lines.tsv (H I, fine-structure components, AAA): H-alpha 2p1/2-3d3/2 5.3877e7 + 2p3/2-3d3/2 1.0775e7 = 6.4652e7; 3p-2s 2.2448e7; 3s-2p 2.1046e6 + 4.2097e6
    const LineRoute a = strongestRoute(3, 2);
    checkTrue("B2: H-alpha's strongest route is 3d -> 2p", a.lHi == 2 && a.lLo == 1);
    checkNum("B2: its Aki is 6.465e7 s^-1 (NIST, summed over the lower j)", a.aki, 5.3877e7 + 1.0775e7, 1e-3);
    checkTrue("B2: H-alpha lists three routes", a.routes == 3);
    for (int hi = 4; hi <= 6; hi++) { const LineRoute r = strongestRoute(hi, 2); checkTrue("B2: Balmer n = " + std::to_string(hi) + ": nd -> 2p is the strongest", r.lHi == 2 && r.lLo == 1); }
    { const LineRoute r = strongestRoute(4, 1); checkTrue("B2: Lyman-gamma has one route, 4p -> 1s", r.lHi == 1 && r.lLo == 0 && r.routes == 1); }
    {   AtomLab L; L.p = AtomParams(); L.restart(); L.setParam("elec", std::string("Hydrogen-like")); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", 1.0);
        checkTrue("B2: a click on H-alpha in a cloud view starts a jump", L.emitLine(3, 2));
        checkTrue("B2: ... from 3d", L.s.trans && orbitals()[std::size_t(L.s.trans->from)].shell == "3d");
        checkTrue("B2: ... to 2p", L.s.trans && orbitals()[std::size_t(L.s.trans->to)].shell == "2p");
        checkTrue("B2: the app says which route is drawn and why", has46(L.s.last, "3d → 2p") && has46(L.s.last, "strongest")); }
}

// ---- B3: jumps of the one-valence-electron atoms with every electron shown ----
static void testAlkaliJumps() {
    using namespace atom;
    checkTrue("B3: Li, Na, K, Rb, Cs have valence jumps; Fe and He do not", alkaliJumps(3) && alkaliJumps(11) && alkaliJumps(19) && alkaliJumps(37) && alkaliJumps(55) && !alkaliJumps(26) && !alkaliJumps(2));
    // levels typed from data/asd/alkali/NaI_levels.tsv
    const auto lev = [](int Z, int n, int l, int j2) { for (const AlkLevel& v : alkaliLevels(Z)) if (v.n == n && v.l == l && v.j2 == j2) return v.cm; return -1.0; };
    checkNum("B3: Na 3p 2P1/2 = 16956.17025 cm-1 (NIST)", lev(11, 3, 1, 1), 16956.17025, 0, 1e-9);
    checkNum("B3: Na 3p 2P3/2 = 16973.36619 cm-1 (NIST)", lev(11, 3, 1, 3), 16973.36619, 0, 1e-9);
    checkNum("B3: Na 4d 2D5/2 = 34548.729 cm-1 (NIST)", lev(11, 4, 2, 5), 34548.729, 0, 1e-9);
    checkNum("B3: K 4p 2P3/2 = 13042.896027 cm-1 (NIST, data/asd/alkali/KI_levels.tsv)", lev(19, 4, 1, 3), 13042.896027, 0, 1e-9);
    // lines typed from data/asd/alkali/NaI_lines.tsv: D2 589.1583264 nm (obs, vacuum), Aki 6.16e7 (AA); D1 589.7558147 nm, 6.14e7
    const auto line = [](int Z, int nLo, int lLo, int jLo, int nHi, int lHi, int jHi, AlkLine& out) {
        for (const AlkLine& L : alkaliLines(Z)) if (L.nLo == nLo && L.lLo == lLo && L.j2Lo == jLo && L.nHi == nHi && L.lHi == lHi && L.j2Hi == jHi) { out = L; return true; }
        return false; };
    AlkLine d2{}, d1{};
    checkTrue("B3: sodium D2 (3s - 3p3/2) is in the data", line(11, 3, 0, 1, 3, 1, 3, d2));
    checkTrue("B3: sodium D1 (3s - 3p1/2) is in the data", line(11, 3, 0, 1, 3, 1, 1, d1));
    checkNum("B3: D2 589.1583264 nm (NIST, vacuum, observed)", d2.nm(), 589.1583264, 0, 1e-9);
    checkNum("B3: D2 Aki 6.16e7 s-1 (NIST, AA)", d2.aki, 6.16e7, 0, 1);
    checkNum("B3: D1 589.7558147 nm (NIST, vacuum, observed)", d1.nm(), 589.7558147, 0, 1e-9);
    checkNum("B3: D1 Aki 6.14e7 s-1 (NIST, AA)", d1.aki, 6.14e7, 0, 1);
    // every kept line is electric dipole (l changes by 1) and joins two kept levels, and its wavelength matches the level difference (vacuum: 1e7 / nm = E_hi - E_lo, the Ritz principle)
    for (int Z : {3, 11, 19, 37, 55}) {
        int bad = 0, n = 0, badE = 0;
        for (const AlkLine& L : alkaliLines(Z)) {
            n++;
            if (std::abs(L.lHi - L.lLo) != 1) bad++;
            const double a = lev(Z, L.nLo, L.lLo, L.j2Lo), b = lev(Z, L.nHi, L.lHi, L.j2Hi);
            if (a < 0 || b < 0) { bad++; continue; }
            if (std::isfinite(L.ritzNm) && std::fabs(1e7 / L.ritzNm - std::fabs(b - a)) > 0.05 + 1e-5 * std::fabs(b - a)) badE++;
        }
        checkTrue("B3: Z = " + std::to_string(Z) + ": " + std::to_string(n) + " lines, all l -> l +- 1 between kept levels", n > 0 && bad == 0);
        checkTrue("B3: Z = " + std::to_string(Z) + ": each Ritz wavelength is the level difference (vacuum, within 0.05 cm-1)", badE == 0);
    }
    // the frozen-core method against the published DHF removal energies (Safronova, Johnson, Derevianko 1999, Table I: K 4s 32370, Rb 5s 30571, Cs 6s 27954 cm-1)
    const double cm = 219474.63136314;   // hartree in cm-1 (CODATA 2022)
    checkNum("B3: K 4s frozen-core DHF removal energy = 32370 cm-1 (Safronova 1999)", -valenceEnergyHartree(19, 4, 0, 1, true) * cm, 32370, 0, 1.0);
    checkNum("B3: Rb 5s = 30571 cm-1 (Safronova 1999)", -valenceEnergyHartree(37, 5, 0, 1, true) * cm, 30571, 0, 1.0);
    checkNum("B3: Cs 6s = 27954 cm-1 (Safronova 1999)", -valenceEnergyHartree(55, 6, 0, 1, true) * cm, 27954, 0, 1.0);
    // the excited orbitals: normalised, and the ground one agrees with the app's relaxed neutral atom (the core barely changes)
    for (int Z : {3, 11, 19, 37, 55})
        for (const AlkOrbital& o : alkaliOrbitals(Z)) {
            const hf::Subshell* v = valenceOrbital(Z, o.n, o.l, true);
            checkTrue("B3: Z = " + std::to_string(Z) + " " + std::to_string(o.n) + "spdf"[o.l] + ": frozen-core orbital exists", v != nullptr);
            if (v) checkNum("B3: Z = " + std::to_string(Z) + " " + std::to_string(o.n) + "spdf"[o.l] + ": normalised (all within 200 bohr)", hf::radialCdf(*v, 200), 1, 1e-5);
        }
    {   const hf::Atom* na = hf::find(11, 0, true);
        const int i = na ? hf::subshellIndex(*na, 3, 0) : -1;
        const hf::Subshell* v = valenceOrbital(11, 3, 0, true);
        checkTrue("B3: sodium's relaxed 3s and the frozen-core 3s both exist", i >= 0 && v);
        if (i >= 0 && v) checkNum("B3: sodium 3s <r>: frozen core within 0.5 % of the relaxed neutral atom", hf::meanRadius(*v), hf::meanRadius(na->sub[std::size_t(i)]), 5e-3); }
    {   const hf::Atom* a = alkaliAtom(11, 3, 1, true);
        checkTrue("B3: sodium with the outer electron in 3p: 11 electrons, 3p occupied, no 3s", a && a->electrons() == 11 && hf::subshellIndex(*a, 3, 1) >= 0 && hf::subshellIndex(*a, 3, 0) < 0);
        // 0.47.2 (deliberate change): the relativistic ground state is now GRASP2018's correlated 3s 2S1/2 cloud; the non-relativistic model keeps the atom as drawn before
        checkTrue("B3: non-relativistic: the ground state is the atom as drawn before", alkaliAtom(11, 3, 0, false) == hf::find(11, 0, false));
        checkTrue("B3 (0.47.2): relativistic: the ground state is GRASP2018's 3s 2S1/2", graspLevel(11, 3, 0, 1) && alkaliAtom(11, 3, 0, true) == &graspLevel(11, 3, 0, 1)->atom);
        // <r> of sodium's orbitals rises with the term energy: 3s < 3p < 3d < 4s ... as in any table; the excited 3p is about 1.4 times 3s (DHF: 5.99 / 4.21 bohr)
        const hf::Subshell* s3 = valenceOrbital(11, 3, 0, true); const hf::Subshell* p3 = valenceOrbital(11, 3, 1, true);
        if (s3 && p3) checkTrue("B3: sodium 3p is larger than 3s (<r> 5.99 against 4.21 bohr)", hf::meanRadius(*p3) > 1.3 * hf::meanRadius(*s3)); }
    // the lab: sodium, every electron; the outer electron put in 3p emits a D line; absorption from 3s reaches only levels joined by a NIST line
    {   AtomLab L; L.p = AtomParams(); L.reducedMotion = true; L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", 11.0);
        checkTrue("B3: sodium starts with every electron and valence jumps on", L.manyOn() && L.valenceOn());
        checkTrue("B3: absorb from 3s: every target is joined to 3s by a NIST line", [&] { for (const AlkOrbital& o : L.valenceTargets(1)) if (alkaliLinesBetween(11, 3, 0, o.n, o.l).empty()) return false; return !L.valenceTargets(1).empty(); }());
        checkTrue("B3: 3s cannot emit (the ground level)", L.valenceTargets(-1).empty());
        L.setValence(3, 1);
        checkTrue("B3: the atom drawn now has its outer electron in 3p", L.manyAtom() == alkaliAtom(11, 3, 1, true));
        L.emit();
        checkTrue("B3: 3p emits a D line (589.16 or 589.76 nm)", L.s.photon && (std::fabs(L.s.photon->lam - 589.1583264) < 1e-6 || std::fabs(L.s.photon->lam - 589.7558147) < 1e-6));
        checkTrue("B3: after the jump the outer electron is back in 3s", L.valenceNow().n == 3 && L.valenceNow().l == 0 && L.manyAtom() == alkaliAtom(11, 3, 0, true));
        checkTrue("B3: the last-photon line names the measured line (" + L.s.last + ")", has46(L.s.last, "589.") && has46(L.s.last, "NIST"));
        checkTrue("B3 (0.47.2): the readout says every electron comes from a correlated calculation (GRASP2018)", anyHas46(L.info(), "correlated calculation (GRASP2018)"));
        L.setParam("mrel", std::string("Non-relativistic"));
        checkTrue("B3: the non-relativistic model's readout says the core is kept fixed", anyHas46(L.info(), "core kept fixed")); }
    {   AtomLab L; L.p = AtomParams(); L.reducedMotion = true; L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", 26.0);
        checkTrue("B3: iron (many outer electrons) has no valence jumps", !L.valenceOn() && L.valenceTargets(1).empty()); }
}
