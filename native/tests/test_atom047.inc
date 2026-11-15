// 0.47: "put the outer electron in" (tap a shell, the outer electron goes there; no photon) and the outer electron's real m = 0 shape in the whole-atom views. Written BEFORE the code.
// References: the atom's own shells with data (alkaliOrbitals + alkaliAtom), the NIST lines (data/asd/alkali), and the textbook angular densities:
// <cos^2 theta> over |Y00|^2 (uniform) = 1/3, over |Y10|^2 ~ cos^2 = 3/5, over |Y20|^2 ~ (3 cos^2 - 1)^2 = 11/21; on a plane containing z, over cos^2 phi: 3/4.
static void testValencePlace() {
    using namespace atom;
    for (const int Z : {3, 11, 19, 37, 55}) {
        AtomLab L; L.p = AtomParams(); L.reducedMotion = true; L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", double(Z));
        const std::string el = name(Z);
        checkTrue("0.47: " + el + " has the place row (every electron, valence on)", L.valenceOn());
        std::vector<AlkOrbital> want;
        for (const AlkOrbital& o : alkaliOrbitals(Z)) if (alkaliAtom(Z, o.n, o.l, true)) want.push_back(o);
        const std::vector<AlkOrbital> got = L.valenceChoices();
        bool same = got.size() == want.size() && !want.empty();
        for (std::size_t i = 0; same && i < got.size(); i++) same = got[i].n == want[i].n && got[i].l == want[i].l;
        checkTrue("0.47: " + el + ": the row lists exactly the shells with data (" + std::to_string(want.size()) + ")", same);
        bool all = true;
        std::string bad;
        for (const AlkOrbital& o : want) {
            const bool ok = L.placeValence(o.n, o.l) && L.valenceNow().n == o.n && L.valenceNow().l == o.l && L.manyAtom() == alkaliAtom(Z, o.n, o.l, true) && !L.s.photon && !L.s.trans;
            if (!ok) { all = false; bad += " " + std::to_string(o.n) + "spdf"[o.l]; }
        }
        checkTrue("0.47: " + el + ": placing the electron in each shell puts it there, with no photon" + (bad.empty() ? "" : " (failed:" + bad + ")"), all);
        L.placeValence(want[0].n, want[0].l);
        // 0.47.2: the ground atom is the GRASP2018 ground level (was the app's DHF ground state, hf::find)
        checkTrue("0.47: " + el + ": back in the ground shell gives the ground atom", L.manyAtom() == alkaliAtom(Z, want[0].n, 0, true) && L.valenceNow().n == want[0].n && L.valenceNow().l == 0);
        checkTrue("0.47: " + el + ": a shell without data is refused", !L.placeValence(9, 3) && L.manyAtom() == alkaliAtom(Z, want[0].n, 0, true));
    }
    {   AtomLab L; L.p = AtomParams(); L.reducedMotion = true; L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", 3.0);
        std::string up; for (const AlkOrbital& o : L.valenceTargets(1)) up += std::to_string(o.n) + "spdf"[o.l] + " ";
        checkTrue("0.47: lithium 2s absorbs only to 2p 3p 4p (NIST lines; got: " + up + ")", up == "2p 3p 4p ");
        checkTrue("0.47: lithium 2s emits nothing (the ground level)", L.valenceTargets(-1).empty());
        // with motion on, a placement is a cross-fade that lands in the new shell
        AtomLab M; M.p = AtomParams(); M.reducedMotion = false; M.restart(); M.setParam("view", std::string("Cloud 3D")); M.setParam("Z", 3.0);
        for (int i = 0; i < 60; i++) M.step(1.0 / 60);
        const bool started = M.placeValence(3, 1) && M.s.trans.has_value() && !M.s.photon;
        for (int i = 0; i < 120; i++) M.step(1.0 / 60);
        checkTrue("0.47: with motion on, placing lithium's electron in 3p cross-fades (no photon) and lands in 3p", started && !M.s.trans && M.valenceNow().n == 3 && M.valenceNow().l == 1);
        checkTrue("0.47: the readout says the outer electron is in 3p", [&] { for (const std::string& s : M.info()) if (s.find("outer electron: 3p") != std::string::npos) return true; return false; }()); }
    // the outer electron's shape: the mean of cos^2 theta over its own dots (picked by their shell colour) against the textbook values.
    // 0.47.2: the m = 0 orbital is the NON-RELATIVISTIC model's picture (no J there); the relativistic model draws the M_J = J state (test_atom0472.inc)
    const auto meanCos2 = [](int Z, int n, int l, const std::string& view, bool inPlane, long& count) {
        AtomLab L; L.p = AtomParams(); L.reducedMotion = true; L.restart(); L.setParam("view", view); L.setParam("Z", double(Z)); L.setParam("mrel", std::string("Non-relativistic"));
        L.placeValence(n, l);
        for (int i = 0; i < 1200; i++) L.step(1.0 / 60);
        const float ph = float(AtomLab::shellPhase(n));
        double s = 0; count = 0;
        const long cap = std::min<long>(L.s.k, long(L.s.sg.size()));
        for (long i = 0; i < cap; i++) {
            if (L.s.ph[i] != ph) continue;
            const double x = L.s.pts[3 * i], y = L.s.pts[3 * i + 1], z = L.s.pts[3 * i + 2];
            const double r2 = inPlane ? x * x + z * z : x * x + y * y + z * z;
            if (!(r2 > 0)) continue;
            s += z * z / r2; count++;
        }
        return count ? s / double(count) : std::nan("");
    };
    {   long N = 0; double m = meanCos2(3, 2, 0, "Cloud 3D", false, N);
        checkNum("0.47: Li 2s outer-electron dots: <cos^2 theta> = 1/3 (uniform; " + std::to_string(N) + " dots)", m, 1.0 / 3, 0, 4 * std::sqrt(4.0 / 45 / std::max<long>(N, 1)));
        m = meanCos2(3, 2, 1, "Cloud 3D", false, N);
        checkNum("0.47: Li 2p outer-electron dots: <cos^2 theta> = 3/5 (|Y10|^2; " + std::to_string(N) + " dots)", m, 3.0 / 5, 0, 4 * std::sqrt(12.0 / 175 / std::max<long>(N, 1)));
        m = meanCos2(11, 3, 2, "Cloud 3D", false, N);
        checkNum("0.47: Na 3d outer-electron dots: <cos^2 theta> = 11/21 (|Y20|^2; " + std::to_string(N) + " dots)", m, 11.0 / 21, 0, 4 * std::sqrt(0.16 / std::max<long>(N, 1)));
        // the Slice view (vertical cut through the nucleus, the plane holds z): the in-plane angle of a pz electron goes as cos^2, so <cos^2> = 3/4 (uniform would be 1/2)
        m = meanCos2(3, 2, 1, "Slice", true, N);
        checkNum("0.47: Li 2p in the Slice view: in-plane <cos^2> = 3/4 (" + std::to_string(N) + " dots)", m, 3.0 / 4, 0, 4 * std::sqrt(1.0 / 16 / std::max<long>(N, 1)) + 1e-9);
        checkTrue("0.47: the readout says why the outer electron points along z", [&] { AtomLab L; L.p = AtomParams(); L.reducedMotion = true; L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", 3.0); L.setParam("mrel", std::string("Non-relativistic")); L.placeValence(2, 1);
            for (const std::string& s : L.info()) if (s.find("along z") != std::string::npos && s.find("round") != std::string::npos) return true; return false; }()); }
}
