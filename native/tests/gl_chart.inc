// 0.13 stage 4 part B: the chart of nuclides drawn from the retained GPU mesh against drawn the per-frame way (included by gl_compare.cpp, llvmpipe). The mesh is made once and the real vertex
// shader (kChartVS) projects it; the reference is the per-frame builders of 0.13 stage 3 with ChartScene's own projection. The numbers of triangles and positions are compared in
// tests/test_chart_cache.inc; here the PICTURES must agree. FLOAT RULE: the shader projects in 32-bit, the per-frame code in 64-bit with a float rounding of its own, so a hairline or an edge
// may fall on the other side of a pixel centre: the pictures may differ in the pixels along edges, never in what is drawn.
struct ChartDummyText : ChartText {
    double width(const std::string& t, double size, bool) const override { return 0.55 * size * double(t.size()); }
    void draw(const std::string&, double, double, double, Color, double, bool) const override {}
};

static ChartColours chartPalette(bool light) {
    return light ? ChartColours{Color{0.1f, 0.1f, 0.12f, 1}, Color{0.4f, 0.4f, 0.45f, 1}, Color{0.2f, 0.4f, 0.9f, 1}, Color{1, 1, 1, 1}, Color{0.8f, 0.8f, 0.82f, 1}}
                 : ChartColours{Color{0.92f, 0.92f, 0.95f, 1}, Color{0.62f, 0.62f, 0.68f, 1}, Color{0.42f, 0.61f, 1, 1}, Color{0.12f, 0.12f, 0.14f, 1}, Color{0.3f, 0.3f, 0.34f, 1}};
}

static Picture g_chartCpu19;                                                    // scene 19 (flat chart) drawn the per-frame way on the 4.3 context: for the 3.3 fallback test

static void chartPictures(render::Renderer2D& R, const std::string& dir) {
    const ChartDummyText T;
    struct Sc { int which; const char* name; double tilt, zoom, panX, panY; };
    const Sc list[] = {{16, "scene 19 chart flat", 0, 1, 0, 0}, {17, "scene 20 landscape", 1, 1, 0, 0}, {18, "scene 21 landscape binding", 1, 1, 0, 0}, {19, "scene 22 landscape with the chain", 1, 1, 0, 0},
                       {17, "landscape tilt 0.5", 0.5, 1, 0, 0}, {17, "landscape tilt 0.02", 0.02, 1, 0, 0}, {17, "landscape zoom 3 panned", 1, 3, -200, 80}, {16, "flat chart zoomed 2 (the landscape's own drawing)", 0, 2, 100, -60}};
    ChartCache cache;
    double worstFrac = 0;
    for (const Sc& sc : list) {
        for (bool light : {false, true}) {
            AtomLab L;
            benchSetup(L, sc.which, 1040, 713);
            L.s.chartView.tilt = sc.tilt; L.s.chartView.zoom = sc.zoom; L.s.chartView.panX = sc.panX; L.s.chartView.panY = sc.panY;
            const ChartColours C = chartPalette(light);
            const bool flat = L.s.chartView.flat();
            const ChainOverlay chainOv{L.s.showChain ? &L.decayGraph() : nullptr, &L.s.walk};
            const Color bg = C.card;
            const auto frame = [&](ChartCache* c) {
                return draw1(R, 1040, 713, bg, [&] {
                    if (flat) drawChart(R, T, C, 1040, 713, L.p.Z, L.p.N, 82, 126, L.s.showChain ? &chainOv : nullptr, c);          // (the hovered square 82 / 126 is drawn too)
                    else drawChartScene(R, T, C, L.chartScene(), 1040, 713, L.p.Z, L.p.N, 82, 126, L.s.showChain ? &chainOv : nullptr, c);
                });
            };
            const Picture a = frame(nullptr);
            const Picture b = frame(&cache);
            const long uploads1 = R.meshUploads();
            const Picture b2 = frame(&cache);                                    // the second cached frame: nothing is sent to the GPU any more
            const std::string name = std::string(sc.name) + (light ? " light" : " dark");
            const Diff d = compare(a, b);
            long lit = 0; for (std::size_t i = 0; i < a.px.size(); i += 4) lit += a.px[i] != a.px[0] || a.px[i + 1] != a.px[1] || a.px[i + 2] != a.px[2];
            const double frac = double(d.differing) / double(d.total);
            worstFrac = std::max(worstFrac, frac);
            std::printf("%-58s %ld of %ld pixels differ (%.4f %%), largest channel difference %d, %ld pixels not the page\n", name.c_str(), d.differing, d.total, 100 * frac, d.maxLevel, lit);
            check(name + ": the picture shows the chart (more than 50,000 pixels differ from the page)", lit > 50000);
            check(name + ": cached and per-frame pictures differ in under 0.1 % of the pixels (only edge pixels)", frac < 0.001);
            check(name + ": the second cached frame sent no vertex to the GPU (the mesh stays there)", R.meshUploads() == uploads1);
            check(name + ": the second cached frame is the first cached frame, pixel for pixel", compare(b, b2).differing == 0);
            save(dir, std::string("chart_") + std::to_string(&sc - list) + (light ? "_light" : "_dark"), a, b);
            if (sc.which == 16 && sc.zoom == 1 && !light) g_chartCpu19 = a;
        }
    }
    std::printf("chart cache: worst fraction of differing pixels %.4f %%\n", 100 * worstFrac);
    cache.release(R);
    check("the cache's release frees its GPU buffers", R.meshesAlive() == 0);
}

// the 3.3 context (called from fallback33): the retained mesh needs nothing newer than OpenGL 3.3, so the cached drawing works there too, with the same picture
static void chartFallback33(render::Renderer2D& R33, const std::string& dir) {
    const ChartDummyText T;
    AtomLab L; benchSetup(L, 16, 1040, 713);
    const ChartColours C = chartPalette(false);
    ChartCache cache;
    const Picture p = draw1(R33, 1040, 713, C.card, [&] { drawChart(R33, T, C, 1040, 713, L.p.Z, L.p.N, 82, 126, nullptr, &cache); });
    if (g_chartCpu19.px.empty()) { check("scene 19 picture from the 4.3 context was kept", false); return; }
    const Diff d = compare(g_chartCpu19, p);
    std::printf("3.3 context: the cached chart against the per-frame chart of the 4.3 context: %ld of %ld pixels differ\n", d.differing, d.total);
    check("3.3 context: the cached flat chart is the per-frame chart (under 0.1 % of the pixels differ)", double(d.differing) / double(d.total) < 0.001);
    save(dir, "chart_fallback33", g_chartCpu19, p);
    cache.release(R33);
}

// the shader's own rules, on small meshes made for the purpose (in the chart's pictures a dropped side face is under 0.05 points tall and shows in about one pixel in 700,000)
static long litPixels(const Picture& p) { long n = 0; for (std::size_t i = 0; i < p.px.size(); i += 4) n += p.px[i] > 128; return n; }
static void chartShaderRules(render::Renderer2D& R) {
    using render::MeshVertex;
    const auto face = [](double a0, double a1, double b, double H, float height) {          // a vertical face at depth b, from z = 0 to H, two triangles, white
        const double q[4][3] = {{a0, b, 0}, {a1, b, 0}, {a1, b, H}, {a0, b, H}};
        std::vector<MeshVertex> v;
        for (int i : {0, 1, 2, 0, 2, 3}) { MeshVertex m{}; for (int k = 0; k < 3; k++) m.p0[k] = float(q[i][k]); m.rgba[0] = m.rgba[1] = m.rgba[2] = m.rgba[3] = 1; m.corner = -1; m.height = height; v.push_back(m); }
        return v;
    };
    render::MeshProjection tilted;                                                  // no turn about the vertical, tipped by an angle whose sine is 0.0124
    tilted.identity = false; tilted.flat = false; tilted.cosY = 1; tilted.sinY = 0; tilted.sinP = 0.0124; tilted.cosP = std::sqrt(1 - 0.0124 * 0.0124); tilted.cx = tilted.cy = 500; tilted.K = 1; tilted.fx = 500; tilted.fy = 500;
    render::MeshProjection flat = tilted; flat.flat = true; flat.sinP = 0; flat.cosP = 1;
    const auto lit = [&](const std::vector<MeshVertex>& v, const render::MeshProjection& P) {
        const int id = R.meshCreate(v);
        const Picture pic = draw1(R, 1000, 1000, Color{0, 0, 0, 1}, [&] { R.drawMesh(id, 0, int(v.size()), P); });
        R.meshDelete(id);
        return litPixels(pic);
    };
    // a 1,000-point wide side face, 4 points tall: it shows 4 x 0.0124 = 0.0496 of a point on the picture, under the 0.05 limit of ChartScene::build, so the shader drops it; 5 points tall it is 0.062 and is drawn
    const long dropped = lit(face(0, 1000, 500.52, 4, 4), tilted), shown = lit(face(0, 1000, 500.52, 5, 5), tilted), sameShape = lit(face(0, 1000, 500.52, 4, -1), tilted);
    std::printf("chart shader rules: side face 4 points tall: %ld pixels, 5 points tall: %ld, the same 4-point shape not marked as a side face: %ld\n", dropped, shown, sameShape);
    check("shader: a side face whose height on the picture is under 0.05 points is dropped (0 pixels)", dropped == 0);
    check("shader: one a little taller is drawn (about 1,000 pixels: a sliver crossing one row of pixel centres)", shown > 900);
    check("shader: the same sliver is drawn when it is not marked as a side face (the rule is the face's, not the geometry's)", sameShape > 900);
    check("shader: a side face is dropped when the chart is flat, whatever its height", lit(face(0, 1000, 500.52, 100, 100), flat) == 0);
    // a hairline: one point wide, centred on its line, as Renderer2D::line makes it
    const auto hair = [&](double x0, double y0, double x1, double y1) {
        std::vector<MeshVertex> v;
        const double p0[3] = {x0, y0, 0}, p1[3] = {x1, y1, 0};
        for (float c : {0.f, 1.f, 3.f, 0.f, 3.f, 2.f}) { MeshVertex m{}; for (int k = 0; k < 3; k++) { m.p0[k] = float(p0[k]); m.p1[k] = float(p1[k]); } m.rgba[0] = m.rgba[1] = m.rgba[2] = m.rgba[3] = 1; m.corner = c; m.height = -1; v.push_back(m); }
        return v;
    };
    render::MeshProjection ident;                                                   // the default: the picture's own points
    const long horiz = lit(hair(100, 300, 900, 300), ident), vertical = lit(hair(400, 100, 400, 900), ident), diag = lit(hair(100, 100, 900, 900), ident), none = lit(hair(300, 300, 300, 300), ident);
    std::printf("chart shader rules: hairlines: horizontal %ld pixels (800 long, 1 wide), vertical %ld, diagonal %ld (1,131 long), a line of no length %ld\n", horiz, vertical, diag, none);
    check("shader: a horizontal hairline 800 points long is 1 point wide (about 800 pixels, two rows of half-covered centres at most)", horiz >= 790 && horiz <= 1610);
    check("shader: a vertical hairline is the same", vertical >= 790 && vertical <= 1610);
    check("shader: a diagonal hairline 1 point wide has about its length in pixels (1,131 long: between 800 and 2,300)", diag >= 800 && diag <= 2300);
    check("shader: a line of no length draws nothing", none == 0);
}
