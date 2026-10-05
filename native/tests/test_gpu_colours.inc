// 0.13 stage 1: phase colours on the GPU (included by test_atom.cpp after test_clean.inc).
// REFERENCE: what 0.12 did, written out here as it was (the colour-by-colour scan over all dots, the band tests, the bin of a phase), and compared INSIDE ONE RUN with the new
// way (cloud_dots.h). No stored numbers, no hashes. The colours of the wheel themselves are checked against scikit-image in testPhaseWheel; the GLSL port is checked in
// tests/gl_compare.cpp (the llvmpipe pictures) and by glslang (tests/check_shaders.py); here only that its constants are the C++ function's.

// 48,000 dots of a fixed seed, as the benchmark's scene 8 builds them (orb: "4f" ..., ax: the orientation, e.g. "z3" real or "m+3" complex: all 24 bins get used).
static void gcFill(AtomLab& L, const char* orb, const char* ax) {
    seedRandom(20260101);
    L.p = AtomParams::appDefaults(); L.p.super = "Off"; L.p.outline = "Off"; L.viewW = 1040; L.viewH = 713;
    L.setParam("Z", 92.0); L.setParam("N", 146.0); L.restart(); L.s.nucSeed = 1; L.s.spin = false;
    L.setParam("phase", std::string("Wheel"));
    L.p.cstyle = "Classic"; L.p.cdens = "Ultra";
    L.setParam("view", std::string("Cloud 3D")); L.setParam("orb", std::string(orb));
    const std::string o = orb;
    L.setParam(o.back() == 'p' ? "pAx" : o.back() == 'd' ? "dAx" : "fAx", std::string(ax));
    L.setParam("zoom", 0.0); L.resetView();
    for (int i = 0; i < 40000 && L.s.k < L.cloudCap(); i++) L.step(1.0 / 240);
}

// the v0.12 colour of a dot, the line of atom_view.cpp as it was
static unsigned char gcOldGroup(bool wheel, const AtomLab& L, long i) { return wheel ? (unsigned char)phaseBin(L.s.ph[std::size_t(i)]) : (unsigned char)(L.s.sg[i] > 0 ? 0 : 1); }

static bool gcSameColour(const render::Color& a, const render::Color& b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

// the numbers in a piece of GLSL or C++ text, as a set (without 0, 1, 2 and 3: loop counts, array sizes and clamp ends); "6.0 / 29.0" gives 6 and 29
static std::set<double> gcNumbers(const std::string& text) {
    std::set<double> v;
    for (std::size_t i = 0; i < text.size();) {
        if (std::isdigit((unsigned char)text[i]) && (i == 0 || !(std::isalnum((unsigned char)text[i - 1]) || text[i - 1] == '_'))) {
            std::size_t j = i; while (j < text.size() && (std::isdigit((unsigned char)text[j]) || text[j] == '.')) j++;
            const double x = std::atof(text.substr(i, j - i).c_str());
            if (x != 0 && x != 1 && x != 2 && x != 3) v.insert(x);
            i = j;
        } else i++;
    }
    return v;
}

static void gcCheckCloud(const std::string& tag, AtomLab& L, bool wheel, bool complexState, double w, double h, double zoomBy) {
    using DBlob = render::Renderer2D::Blob;
    const long cnt = std::min<long>(L.s.k, L.cloudCap());
    checkTrue(tag + ": 48,000 dots", cnt == 48000);
    // the colour group of every dot: the new function against the 0.12 line, dot by dot
    std::vector<unsigned char> grp(static_cast<std::size_t>(cnt)), oldGrp(static_cast<std::size_t>(cnt));
    plainGroups(wheel, L.s.ph.data(), L.s.sg.data(), cnt, grp.data());
    for (long i = 0; i < cnt; i++) oldGrp[std::size_t(i)] = gcOldGroup(wheel, L, i);
    int used[kPhaseBins] = {};
    for (unsigned char g : grp) used[std::min<int>(g, kPhaseBins - 1)]++;
    int bins = 0; for (int u : used) bins += u > 0;
    checkTrue(tag + ": the colour group of every dot is the 0.12 one (" + std::to_string(bins) + " of " + (wheel ? "24" : "2") + " colours in use)", grp == oldGrp);
    checkTrue(tag + ": groups in range", std::all_of(grp.begin(), grp.end(), [&](unsigned char g) { return g < (wheel ? kPhaseBins : 2); }));
    if (wheel && complexState) checkTrue(tag + ": a complex state uses most of the 24 colours", bins >= 20);

    // the picture: the camera of the benchmark's scene (yaw, pitch of the lab, auto zoom)
    const double m = std::min(w, h), pxm = L.pxPerM(m, L.s.szCur) * std::pow(10, L.p.zoom) * zoomBy, ext = kExt[L.sizeN()] / L.p.Z * kA0;
    const CloudCamera cam{w / 2, h / 2, pxm, std::cos(L.s.yaw), std::sin(L.s.yaw), std::cos(L.s.pitch), std::sin(L.s.pitch), ext};
    std::vector<float> PX, PY, PD;
    projectCloud(L.s.pts.data(), cnt, cam, w, h, PX, PY, PD);
    long offScreen = 0; for (float v : PD) offScreen += std::isnan(v);

    // 0.12's three loops, as they were (the band tests written out, the scan over all dots for each band and colour)
    const int ng = wheel ? kPhaseBins : 2;
    std::vector<int> v12;
    long bandCount[3] = {};
    for (int bnd = 0; bnd < 3; bnd++)
        for (int g = 0; g < ng; g++)
            for (long i = 0; i < cnt; i++) {
                const float v = PD[std::size_t(i)];
                const bool in = bnd == 0 ? v > 0.35f : bnd == 1 ? (v <= 0.35f && v >= -0.35f) : v < -0.35f;
                if (oldGrp[std::size_t(i)] == g && in) { v12.push_back(int(i)); bandCount[bnd]++; }
            }
    // the new order functions: both give 0.12's list
    std::vector<unsigned char> band(static_cast<std::size_t>(cnt));
    for (long i = 0; i < cnt; i++) band[std::size_t(i)] = depthBandOf(PD[std::size_t(i)]);
    std::vector<int> byScan, byCount;
    drawOrderByScan(band.data(), grp.data(), cnt, kDepthBands, ng, byScan);
    drawOrderByCounting(band.data(), grp.data(), cnt, kDepthBands, ng, byCount);
    checkTrue(tag + ": the scan function gives the 0.12 draw order", byScan == v12);
    checkTrue(tag + ": the counting sort gives the 0.12 draw order (" + std::to_string(v12.size()) + " dots drawn, " + std::to_string(offScreen) + " off the picture)", byCount == v12);
    // every drawn dot exactly once; every off-screen dot never
    std::vector<int> times(static_cast<std::size_t>(cnt), 0);
    for (int i : byCount) times[std::size_t(i)]++;
    bool once = true;
    for (long i = 0; i < cnt; i++) once = once && times[std::size_t(i)] == (std::isnan(PD[std::size_t(i)]) ? 0 : 1);
    checkTrue(tag + ": every dot on the picture is in the GPU list exactly once, no dot off the picture is", once);
    checkTrue(tag + ": drawn + off-screen = all dots", long(byCount.size()) + offScreen == cnt);

    // the vertex data of both ways, blob by blob
    const render::Color plus = render::hexColor("#6c9cff"), minus = render::hexColor("#f08a3c");
    for (bool light : {false, true}) {
        const DotColours cpu = makeDotColours(false, wheel, light, plus, minus), gpu = makeDotColours(true, wheel, light, plus, minus);
        const double wgt = cloudDotWeight(cnt);
        std::vector<DBlob> bc, bg;
        buildClassic3D(PX.data(), PY.data(), PD.data(), grp.data(), cnt, cpu, wgt, bc);
        buildClassic3D(PX.data(), PY.data(), PD.data(), grp.data(), cnt, gpu, wgt, bg);
        checkTrue(tag + (light ? " (light page)" : " (dark page)") + ": both ways make one blob per drawn dot", bc.size() == v12.size() && bg.size() == v12.size());
        bool same = bc.size() == bg.size(), colourOk = true, bandOk = true, kindOk = true;
        long gpuBand[3] = {};
        for (std::size_t k = 0; same && k < bc.size(); k++) {
            const int i = v12[k];
            same = bc[k].x == bg[k].x && bc[k].y == bg[k].y && bc[k].r == bg[k].r && bc[k].c0.a == bg[k].c0.a && bc[k].x == PX[std::size_t(i)] && bc[k].y == PY[std::size_t(i)];
            kindOk = kindOk && bc[k].kind == 2 && bg[k].kind == render::kBlobPalSquare;
            const int group = int(bg[k].fx);
            colourOk = colourOk && group == oldGrp[std::size_t(i)] && gcSameColour(bc[k].c0.withAlpha(1), cpu.c[group].withAlpha(1));
            const int b = bg[k].r == float(kDepthBand[0].size / 2) ? 0 : bg[k].r == float(kDepthBand[1].size / 2) ? 1 : 2;
            bandOk = bandOk && b == int(band[std::size_t(i)]) && bg[k].c0.a == float(std::min(1.0, kDepthBand[b].alpha * wgt) * cpu.c[group].a);
            gpuBand[b]++;
        }
        checkTrue(tag + (light ? " (light page)" : " (dark page)") + ": the GPU blobs are the CPU blobs, in the same order, same place, size and light", same);
        checkTrue(tag + (light ? " (light page)" : " (dark page)") + ": each GPU blob carries the group the 0.12 scan put its dot in, and the CPU blob has that group's colour", colourOk);
        checkTrue(tag + (light ? " (light page)" : " (dark page)") + ": each dot is in its 0.12 depth band (size and light of the band); the blobs are kind 2 / palette kind", bandOk && kindOk);
        checkTrue(tag + (light ? " (light page)" : " (dark page)") + ": the dots per depth band equal 0.12's", gpuBand[0] == bandCount[0] && gpuBand[1] == bandCount[1] && gpuBand[2] == bandCount[2]);
        if (!light) std::printf("gpu colours %s: %zu dots drawn (far %ld, middle %ld, near %ld), %ld off the picture\n", tag.c_str(), v12.size(), bandCount[0], bandCount[1], bandCount[2], offScreen);
    }

    // dots that are not shown (a superposition thins, a jump's odds): group 255 is left out by both
    std::vector<unsigned char> thin = grp;
    for (long i = 0; i < cnt; i += 7) thin[std::size_t(i)] = kHiddenGroup;
    std::vector<int> a, b2, ref;
    for (int bnd = 0; bnd < 3; bnd++) for (int g = 0; g < ng; g++) for (long i = 0; i < cnt; i++) {
        const float v = PD[std::size_t(i)];
        if (thin[std::size_t(i)] == g && (bnd == 0 ? v > 0.35f : bnd == 1 ? (v <= 0.35f && v >= -0.35f) : v < -0.35f)) ref.push_back(int(i));
    }
    drawOrderByScan(band.data(), thin.data(), cnt, kDepthBands, ng, a); drawOrderByCounting(band.data(), thin.data(), cnt, kDepthBands, ng, b2);
    checkTrue(tag + ": with every 7th dot not shown, both orders still equal 0.12's", a == ref && b2 == ref);

    // the 2D side view: one band, group by group (the loop of 0.12 written out)
    std::vector<SideDot> sc, sk, s12;
    const double cx = w / 2, cy = h / 2;
    for (int g = 0; g < ng; g++)
        for (long i = 0; i < cnt; i++) {
            if (oldGrp[std::size_t(i)] != g) continue;
            const double X = cx + L.s.pts[std::size_t(3 * i)] * pxm - 1, Y = cy - L.s.pts[std::size_t(3 * i + 2)] * pxm - 1;
            if (!(X > -2 && X < w && Y > -2 && Y < h)) continue;
            s12.push_back({float(X + 1), float(Y + 1), (unsigned char)g});
        }
    sideDots(L.s.pts.data(), cnt, grp.data(), ng, cx, cy, pxm, w, h, false, sc);
    sideDots(L.s.pts.data(), cnt, grp.data(), ng, cx, cy, pxm, w, h, true, sk);
    auto sameSide = [](const std::vector<SideDot>& p, const std::vector<SideDot>& q) { if (p.size() != q.size()) return false; for (std::size_t k = 0; k < p.size(); k++) if (p[k].x != q[k].x || p[k].y != q[k].y || p[k].g != q[k].g) return false; return true; };
    checkTrue(tag + ": 2D side view: the scan function and the counting sort both give 0.12's dots in 0.12's order (" + std::to_string(s12.size()) + " dots)", sameSide(sc, s12) && sameSide(sk, s12));
}

// phases exactly on a bin edge, and at 0, pi, -pi, 2 pi: the same bin as 0.12's phaseBin (which is unchanged) and the plain-group function of the new path agree dot for dot
static void gcCheckEdges() {
    std::vector<float> ph;
    std::vector<signed char> sg;
    const double twoPi = 2 * kPi;
    ph.push_back(0.f); ph.push_back(float(kPi)); ph.push_back(float(-kPi)); ph.push_back(float(twoPi)); ph.push_back(float(-twoPi)); ph.push_back(float(3 * kPi)); ph.push_back(0.f);
    for (int b = 0; b < kPhaseBins; b++) {
        const double edge = twoPi * (b + 0.5) / kPhaseBins;                    // the edge between bin b and bin b + 1 (bin 0 is centred on phase 0)
        ph.push_back(float(edge)); ph.push_back(std::nextafter(float(edge), 10.f)); ph.push_back(std::nextafter(float(edge), -10.f));
        ph.push_back(float(phaseOfBin(b))); ph.push_back(float(phaseOfBin(b) - twoPi));        // a bin's middle, and the same a turn earlier
    }
    sg.assign(ph.size(), 1);
    std::vector<unsigned char> grp(ph.size());
    plainGroups(true, ph.data(), sg.data(), long(ph.size()), grp.data());
    bool same = true;
    for (std::size_t i = 0; i < ph.size(); i++) same = same && grp[i] == (unsigned char)phaseBin(ph[i]);   // 0.12's line
    checkTrue("bin edges: every phase on or next to a bin edge gets the 0.12 bin", same);
    // the values the bins must have, worked out by hand from the cut of the circle into 24 equal parts centred on 0, 15, 30 ... degrees
    checkNum("bin of phase 0", grp[0], 0, 0);
    checkNum("bin of phase pi", grp[1], 12, 0);
    checkNum("bin of phase -pi", grp[2], 12, 0);
    checkNum("bin of phase 2 pi", grp[3], 0, 0);
    checkNum("bin of phase -2 pi", grp[4], 0, 0);
    checkNum("bin of phase 3 pi", grp[5], 12, 0);
    bool mid = true;
    for (int b = 0; b < kPhaseBins; b++) mid = mid && grp[7 + 5 * b + 3] == b && grp[7 + 5 * b + 4] == b;
    checkTrue("bin of the middle of each bin (also a turn earlier) is that bin", mid);
    // an edge falls to one side or the other of two neighbours, never further; just below it is bin b, just above it is bin b + 1 (the edge itself: either, as in 0.12)
    bool edges = true;
    for (int b = 0; b < kPhaseBins; b++) {
        const int e = 7 + 5 * b;
        edges = edges && grp[e + 2] == b && grp[e + 1] == (b + 1) % kPhaseBins && (grp[e] == b || grp[e] == (b + 1) % kPhaseBins);
    }
    checkTrue("bin edges: just below an edge the bin before it, just above the bin after it, on it one of the two", edges);
}

static void gcCheckColours() {
    using DBlob = render::Renderer2D::Blob;
    const render::Color plus = render::hexColor("#6c9cff"), minus = render::hexColor("#f08a3c");
    for (bool light : {false, true}) {
        const DotColours w = makeDotColours(true, true, light, plus, minus), sgn = makeDotColours(true, false, light, plus, minus);
        bool wheel = w.n == kPhaseBins;
        for (int g = 0; g < kPhaseBins; g++) {
            const PhaseRgb c = phaseRgb(2 * kPi * g / kPhaseBins, light);           // the reference function, the phase of bin g written out
            wheel = wheel && w.c[g].r == c.r && w.c[g].g == c.g && w.c[g].b == c.b && w.c[g].a == 1;
        }
        checkTrue(std::string("colours: the 24 wheel colours are phaseRgb of each bin's middle (") + (light ? "light" : "dark") + " page)", wheel);
        checkTrue("colours: Sign has the two colours given, + first", sgn.n == 2 && gcSameColour(sgn.c[0], plus) && gcSameColour(sgn.c[1], minus));
        const render::Renderer2D::PhasePalette pw = w.palette(), ps = sgn.palette();
        checkTrue("colours: the palette sent to the shader says Wheel / Sign, the page, and the two Sign colours", pw.wheel && !ps.wheel && pw.light == light && ps.light == light && gcSameColour(ps.sign[0], plus) && gcSameColour(ps.sign[1], minus));
        // one dot, both ways
        const DotColours cpu = makeDotColours(false, true, light, plus, minus);
        for (int g : {0, 5, 12, 23}) {
            const DBlob a = squareDot(cpu, 10.25, 20.5, 1.2, g, 0.37), b = squareDot(w, 10.25, 20.5, 1.2, g, 0.37), s1 = softDot(cpu, 3, 4, 4.4, g, 2.0), s2 = softDot(w, 3, 4, 4.4, g, 2.0);
            checkTrue("colours: a square dot, CPU way and GPU way: same place, size, light; the GPU one carries the group", a.kind == 2 && b.kind == render::kBlobPalSquare && a.x == b.x && a.y == b.y && a.r == b.r && a.c0.a == b.c0.a && b.fx == float(g) &&
                      gcSameColour(a.c0.withAlpha(1), cpu.c[g].withAlpha(1)));
            checkTrue("colours: a soft dot likewise, light above 1 clamped to 1", s1.kind == 4 && s2.kind == render::kBlobPalSoft && s1.c0.a == 1 && s2.c0.a == 1 && s2.fx == float(g) && s1.r == s2.r);
        }
    }
    const DotColours dim = makeDotColours(true, false, false, render::Color{0.2f, 0.4f, 0.6f, 0.5f}, minus);
    checkTrue("colours: a colour's own alpha is kept in the light of the dot, as 0.12's withA did", squareDot(dim, 0, 0, 1, 0, 0.5).c0.a == 0.25f && squareDot(dim, 0, 0, 1, 1, 0.5).c0.a == 0.5f);
}

// the GLSL port: its constants are the C++ reference function's (the numbers of phaseRgb in phase_wheel.h: CIELAB 1976, D65, IEC 61966-2-1 sRGB)
static void gcCheckShaderConstants() {
    const std::string vs = render::kBlobVS;
    const std::size_t a = vs.find("float finv("), b = vs.find("return c;");
    checkTrue("shader: the blob vertex shader has the phase colour port", a != std::string::npos && b != std::string::npos && a < b);
    const std::set<double> glsl = gcNumbers(vs.substr(a, b - a));
    const std::set<double> want = {52, 72, 58, 16, 116, 500, 200, 6, 29, 4, 0.95047, 1.08883, 3.2406, 1.5372, 0.4986, 0.9689, 1.8758, 0.0415, 0.0557, 0.2040, 1.0570, 0.0031308, 12.92, 1.055, 2.4, 0.055};
    checkTrue("shader: the GLSL port uses exactly the constants of phaseRgb (typed here from CIE 1976 L*a*b*, D65, IEC 61966-2-1)", glsl == want);
    const std::size_t m = vs.find("float(g) / 24.0");
    checkTrue("shader: the bin's phase is 2 pi g / 24, and kPhaseBins is 24", m != std::string::npos && kPhaseBins == 24 && vs.find("2.0 * 3.14159265358979323846 * float(g) / 24.0") != std::string::npos);
    checkTrue("shader: palette kinds 5 and 6 are the ones the renderer names", render::kBlobPalSquare == 5 && render::kBlobPalSoft == 6 && vs.find("kind > 4.5") != std::string::npos && vs.find("kind > 5.5 ? 4.0 : 2.0") != std::string::npos);
}

static void testGpuColours() {
    gcCheckEdges();
    gcCheckColours();
    gcCheckShaderConstants();
    struct C { const char* orb; const char* ax; bool complex; };
    const C clouds[] = {{"4f", "z3", false}, {"4f", "m+3", true}, {"2p", "m+1", true}, {"3d", "m+2", true}, {"2p", "z", false}, {"3d", "z2", false}};
    for (const C& c : clouds) {
        AtomLab L;
        gcFill(L, c.orb, c.ax);
        const std::string tag = std::string(c.orb) + " " + c.ax;
        gcCheckCloud(tag + " Wheel", L, true, c.complex, 1040, 713, 1);
        gcCheckCloud(tag + " Wheel (zoomed in x2.5: many dots off the picture)", L, true, c.complex, 1040, 713, 2.5);
        gcCheckCloud(tag + " Sign", L, false, c.complex, 1040, 713, 1);
    }
}
