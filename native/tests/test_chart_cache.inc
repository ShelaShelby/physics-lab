// 0.13 stage 4 part B: the chart of nuclides' tiles / blocks kept on the GPU (included by test_atom.cpp after test_stage4_cloud.inc). WRITTEN BEFORE the code (chart_mesh.*, ChartCache).
// REFERENCE: the per-frame builders of 0.13 stage 3 (drawChartStaticFlat / Ground / Blocks in chart_draw.cpp, kept as they were), recorded by the stand-in renderer above, expanded into triangles in
// the order they were made. The new mesh (chart_mesh.h), projected on the CPU by chartMeshReference (the twin of the shader), must give the SAME triangles: the same count, the same order, the
// same colours, positions equal to within float rounding. The pictures the real shader makes are compared with the per-frame pictures in tests/gl_compare.cpp.
struct CcTri { float xy[6]; render::Color c; };

static void ccClear() { g_rec.clear(); g_tri.clear(); g_lines.clear(); g_discs.clear(); g_events.clear(); }

// the recorded calls as triangles, in order: a rect as Renderer2D::rect pushes it, a triangle as given, a line as Renderer2D::line pushes it (6 vertices: P0 + n, P1 + n, P1 - n, P0 + n, P1 - n, P0 - n)
static std::vector<CcTri> ccTriangles() {
    std::vector<CcTri> out;
    for (const auto& ev : g_events) {
        if (ev.first == 'r') {
            const RecRect& r = g_rec[std::size_t(ev.second)];
            out.push_back({{float(r.x), float(r.y), float(r.x + r.w), float(r.y), float(r.x + r.w), float(r.y + r.h)}, r.c});
            out.push_back({{float(r.x), float(r.y), float(r.x + r.w), float(r.y + r.h), float(r.x), float(r.y + r.h)}, r.c});
        } else if (ev.first == 't') {
            const RecTri& t = g_tri[std::size_t(ev.second)];
            out.push_back({{float(t.p[0]), float(t.p[1]), float(t.p[2]), float(t.p[3]), float(t.p[4]), float(t.p[5])}, t.c});
        } else {
            const RecLine& l = g_lines[std::size_t(ev.second)];
            const double dx = l.x1 - l.x0, dy = l.y1 - l.y0, len = std::hypot(dx, dy);
            if (len < 1e-9) continue;
            render::Color c = l.c; double w = l.w;
            if (w < 1) { c.a *= float(w); w = 1; }
            const double nx = -dy / len * w / 2, ny = dx / len * w / 2;
            out.push_back({{float(l.x0 + nx), float(l.y0 + ny), float(l.x1 + nx), float(l.y1 + ny), float(l.x1 - nx), float(l.y1 - ny)}, c});
            out.push_back({{float(l.x0 + nx), float(l.y0 + ny), float(l.x1 - nx), float(l.y1 - ny), float(l.x0 - nx), float(l.y0 - ny)}, c});
        }
    }
    return out;
}

// compares the recorded per-frame triangles with the mesh's (xy, rgba from chartMeshReference); returns the worst position difference, and whether count and colours agree
static bool ccSame(const std::vector<CcTri>& ref, const std::vector<float>& xy, const std::vector<float>& rgba, double tol, double& worst, std::string& why) {
    worst = 0;
    if (xy.size() != ref.size() * 6) { why = "triangle count " + std::to_string(xy.size() / 6) + " against " + std::to_string(ref.size()); return false; }
    for (std::size_t t = 0; t < ref.size(); t++) {
        for (int k = 0; k < 6; k++) worst = std::max(worst, double(std::fabs(xy[6 * t + std::size_t(k)] - ref[t].xy[k])));
        for (int v = 0; v < 3; v++) {
            const float* c = &rgba[12 * t + 4 * std::size_t(v)];
            if (c[0] != ref[t].c.r || c[1] != ref[t].c.g || c[2] != ref[t].c.b || c[3] != ref[t].c.a) { why = "colour of triangle " + std::to_string(t); return false; }
        }
    }
    if (worst > tol) { why = "position difference " + std::to_string(worst); return false; }
    return true;
}

static ChartColours ccPalette(bool light) {
    using render::Color;
    return light ? ChartColours{Color{0.1f, 0.1f, 0.12f, 1}, Color{0.4f, 0.4f, 0.45f, 1}, Color{0.2f, 0.4f, 0.9f, 1}, Color{1, 1, 1, 1}, Color{0.8f, 0.8f, 0.82f, 1}}
                 : ChartColours{Color{0.92f, 0.92f, 0.95f, 1}, Color{0.62f, 0.62f, 0.68f, 1}, Color{0.42f, 0.61f, 1, 1}, Color{0.12f, 0.12f, 0.14f, 1}, Color{0.3f, 0.3f, 0.34f, 1}};
}

struct CcView { double w, h; ChartHeightMode mode; double tilt, zoom, panX, panY; };

static void testChartCache() {
    render::Renderer2D R;                                                  // (the stand-in above: nothing is drawn, calls are recorded)
    // ---- 1) the flat chart: the mesh against drawChartStaticFlat, three picture sizes (the smallest has squares under 3 points: no gap), both palettes ----
    for (const std::pair<double, double>& sz : {std::pair<double, double>{1040, 713}, {600, 420}, {300, 200}}) {
        for (bool light : {false, true}) {
            const ChartColours C = ccPalette(light);
            const ChartLayout L = chartLayout(sz.first, sz.second);
            ccClear();
            drawChartStaticFlat(R, C, L);
            const std::vector<CcTri> ref = ccTriangles();
            const ChartMesh M = chartFlatMesh(L, C);
            std::vector<float> xy, rgba;
            chartMeshReference(M.v, M.partB, M.partBCount, ChartProjection(), xy, rgba);
            double worst; std::string why;
            const bool ok = ccSame(ref, xy, rgba, 1e-3, worst, why);
            checkTrue("flat chart " + std::to_string(int(sz.first)) + "x" + std::to_string(int(sz.second)) + (light ? " light" : " dark") + ": the retained mesh is the per-frame builder's triangles, same count (" + std::to_string(ref.size()) +
                      "), order and colours, positions within 1e-3 points (worst " + std::to_string(worst) + ")" + (ok ? "" : ": " + why), ok);
            checkTrue("flat chart: " + std::to_string(M.v.size()) + " vertices = 3 per triangle, and the whole static part is in one run", M.v.size() == 3 * ref.size() && M.partBCount == int(M.v.size()) && M.partB == 0);
        }
    }

    // ---- 2) the landscape, both heights, flat tilt 0 zoomed, tilted, zoomed and panned, two sizes, both palettes: ground and blocks ----
    const CcView views[] = {{1040, 713, ChartHeightMode::HalfLife, 1, 1, 0, 0}, {1040, 713, ChartHeightMode::Binding, 1, 1, 0, 0}, {1040, 713, ChartHeightMode::HalfLife, 0.5, 1, 0, 0},
                            {1040, 713, ChartHeightMode::HalfLife, 0.02, 1, 0, 0}, {1040, 713, ChartHeightMode::HalfLife, 0, 3, 40, -25}, {1040, 713, ChartHeightMode::HalfLife, 1, 4, -120, 90},
                            {1040, 713, ChartHeightMode::Binding, 0.7, 16, 600, -300}, {640, 460, ChartHeightMode::HalfLife, 1, 1, 0, 0}, {640, 460, ChartHeightMode::Binding, 0.3, 2, 30, 30}};
    double worstAll = 0;
    long trianglesChecked = 0;
    for (const CcView& v : views) {
        for (bool light : {false, true}) {
            const ChartColours C = ccPalette(light);
            ChartScene S; ChartViewState st; st.tilt = v.tilt; st.zoom = v.zoom; st.panX = v.panX; st.panY = v.panY; st.height = v.mode;
            S.build(v.w, v.h, st);
            const ChartLayout& L = S.layout();
            const ChartMesh M = chartLandscapeMesh(L, C, v.mode);
            // a tolerance from where the numbers come from: the per-frame code rounds every corner to float at scale 1 (up to about 1,000 points: 6e-5) and then scales it by K; the mesh rounds once at the end
            const double tol = 1.5e-4 * std::max(1.0, S.proj().K) + 1e-4;
            std::vector<float> xy, rgba;
            double worst; std::string why;
            ccClear();
            drawChartStaticGround(R, C, L, S.proj());
            chartMeshReference(M.v, M.partA, M.partACount, S.proj(), xy, rgba);
            const std::vector<CcTri> refA = ccTriangles();
            bool ok = ccSame(refA, xy, rgba, tol, worst, why);
            worstAll = std::max(worstAll, worst); trianglesChecked += long(refA.size());
            const std::string tag = std::string("landscape ") + (v.mode == ChartHeightMode::Binding ? "binding" : "half-life") + " tilt " + std::to_string(v.tilt).substr(0, 4) + " zoom " + std::to_string(int(v.zoom)) + (light ? " light" : " dark") + ", " +
                                    std::to_string(int(v.w)) + "x" + std::to_string(int(v.h));
            checkTrue(tag + ": the ground (" + std::to_string(refA.size()) + " triangles) is the per-frame builder's (worst " + std::to_string(worst) + ", allowed " + std::to_string(tol) + ")" + (ok ? "" : ": " + why), ok);
            ccClear();
            drawChartStaticBlocks(R, C, S);
            chartMeshReference(M.v, M.partB, M.partBCount, S.proj(), xy, rgba);
            const std::vector<CcTri> refB = ccTriangles();
            ok = ccSame(refB, xy, rgba, tol, worst, why);
            worstAll = std::max(worstAll, worst); trianglesChecked += long(refB.size());
            checkTrue(tag + ": the blocks (" + std::to_string(refB.size()) + " triangles, far to near, with the side faces the tilt shows) are the per-frame builder's (worst " + std::to_string(worst) + ", allowed " + std::to_string(tol) + ")" + (ok ? "" : ": " + why), ok);
        }
    }
    std::printf("chart cache: the retained meshes against the per-frame builders: %ld triangles, worst position difference %.3g points\n", trianglesChecked, worstAll);

    // ---- 3) the rebuild triggers: size, theme colours, height meaning rebuild; tilt, zoom, pan, the chosen and the hovered nuclide, the legend and the other view do NOT ----
    {
        const RecText T;
        ChartCache cache;
        const ChartColours dark = ccPalette(false), light = ccPalette(true);
        const auto flatFrame = [&](double w, double h, const ChartColours& C, int selZ, int selN, int hovZ, int hovN) { g_meshDraws.clear(); drawChart(R, T, C, w, h, selZ, selN, hovZ, hovN, nullptr, &cache); };
        const auto landFrame = [&](double w, double h, const ChartColours& C, const ChartViewState& st, int selZ, int selN, int hovZ, int hovN) {
            ChartScene S; S.build(w, h, st);
            g_meshDraws.clear(); drawChartScene(R, T, C, S, w, h, selZ, selN, hovZ, hovN, nullptr, &cache);
            return S.proj();
        };
        g_meshCreates = g_meshDeletes = 0;
        flatFrame(1040, 713, dark, 92, 146, 0, 0);
        checkTrue("cache, flat: the first frame builds the mesh once and draws it in one call (the whole static part)", cache.flat.builds == 1 && g_meshCreates == 1 && g_meshDraws.size() == 1 && g_meshDraws[0].count == cache.flat.mesh.partBCount && g_meshDraws[0].p.identity);
        flatFrame(1040, 713, dark, 92, 146, 0, 0);
        flatFrame(1040, 713, dark, 82, 126, 50, 80);
        flatFrame(1040, 713, dark, 1, 0, 0, 0);
        checkTrue("cache, flat: nothing is rebuilt when the chosen nuclide, the hovered one or nothing changes (3 more frames, still 1 build)", cache.flat.builds == 1 && g_meshCreates == 1 && g_meshDeletes == 0);
        flatFrame(900, 600, dark, 92, 146, 0, 0);
        checkTrue("cache, flat: a new picture size rebuilds (2 builds; the old buffer is freed first)", cache.flat.builds == 2 && g_meshCreates == 2 && g_meshDeletes == 1);
        flatFrame(900, 600, light, 92, 146, 0, 0);
        checkTrue("cache, flat: a new theme (other colours) rebuilds (3 builds)", cache.flat.builds == 3 && g_meshCreates == 3 && g_meshDeletes == 2);
        flatFrame(900, 600, light, 92, 146, 0, 0);
        checkTrue("cache, flat: the same theme again does not", cache.flat.builds == 3);

        ChartViewState st; st.tilt = 1;
        landFrame(1040, 713, dark, st, 92, 146, 0, 0);
        const long landBuilds0 = cache.land.builds;
        checkTrue("cache, landscape: the first frame builds the mesh and draws it in two calls (the ground, then the blocks, with the chosen footprint drawn between them each frame)", landBuilds0 == 1 && g_meshDraws.size() == 2 &&
                  g_meshDraws[0].first == cache.land.mesh.partA && g_meshDraws[1].first == cache.land.mesh.partB && g_meshDraws[1].first == g_meshDraws[0].first + g_meshDraws[0].count);
        const long created = g_meshCreates;
        for (double tilt : {0.9, 0.5, 0.01, 1.0}) { st.tilt = tilt; const ChartProjection P = landFrame(1040, 713, dark, st, 92, 146, 0, 0);
            const render::MeshProjection mp = chartMeshProjection(P);
            checkTrue("cache, landscape: tilt " + std::to_string(tilt).substr(0, 4) + " goes in as the projection uniforms of the draw (no rebuild)", g_meshDraws.size() == 2 && g_meshDraws[1].p.cosP == mp.cosP && g_meshDraws[1].p.sinY == mp.sinY &&
                      g_meshDraws[1].p.K == mp.K && g_meshDraws[1].p.fx == mp.fx && !g_meshDraws[1].p.identity); }
        for (double zoom : {2.0, 7.0}) { st.zoom = zoom; st.panX = 33 * zoom; st.panY = -20 * zoom; landFrame(1040, 713, dark, st, 92, 146, 0, 0); }
        landFrame(1040, 713, dark, st, 40, 50, 20, 20);
        checkTrue("cache, landscape: tilt, zoom, pan, the chosen nuclide and the hovered one rebuild nothing (7 more frames, still 1 build, no new buffer)", cache.land.builds == landBuilds0 && g_meshCreates == created);
        st = ChartViewState(); st.tilt = 1;
        flatFrame(1040, 713, dark, 92, 146, 0, 0);
        landFrame(1040, 713, dark, st, 92, 146, 0, 0);
        checkTrue("cache: going between the flat chart and the landscape rebuilds neither (each has its own slot) except the flat chart's size, which differs from its last (1 build)", cache.land.builds == landBuilds0);
        const long b1 = cache.land.builds; st.height = ChartHeightMode::Binding;
        landFrame(1040, 713, dark, st, 92, 146, 0, 0);
        checkTrue("cache, landscape: the height meaning (half-life to binding energy) rebuilds (the blocks' heights are in the mesh)", cache.land.builds == b1 + 1);
        landFrame(1040, 713, light, st, 92, 146, 0, 0);
        checkTrue("cache, landscape: a new theme rebuilds", cache.land.builds == b1 + 2);
        landFrame(700, 500, light, st, 92, 146, 0, 0);
        checkTrue("cache, landscape: a new picture size rebuilds", cache.land.builds == b1 + 3);
        landFrame(700, 500, light, st, 92, 146, 0, 0);
        checkTrue("cache, landscape: and the same again does not", cache.land.builds == b1 + 3);
        const int deletesBefore = g_meshDeletes;
        // (the stand-in has no GL: release() only has to free the slots it has made)
        cache.release(R);
        checkTrue("cache: release frees both buffers and forgets them (the next draw builds again)", g_meshDeletes == deletesBefore + 2 && cache.flat.id < 0 && cache.land.id < 0);

        // what a cached frame no longer makes: the per-frame calls of the static part (3,557 tiles / blocks) are not made; only the chosen nuclide, legend, notes, hover
        ChartScene S; ChartViewState s1; s1.tilt = 1; S.build(1040, 713, s1);
        ChartCache c2;
        ccClear(); drawChartScene(R, T, dark, S, 1040, 713, 92, 146, 0, 0, nullptr, nullptr);
        const std::size_t perFrame = g_events.size();
        ccClear(); drawChartScene(R, T, dark, S, 1040, 713, 92, 146, 0, 0, nullptr, &c2);
        const std::size_t cached = g_events.size();
        std::printf("chart cache: a landscape frame makes %zu shape calls per frame without the cache and %zu with it\n", perFrame, cached);
        checkTrue("cache, landscape: a cached frame makes under 5 % of the shape calls the per-frame one makes (" + std::to_string(cached) + " against " + std::to_string(perFrame) + "): the blocks are not built again", cached * 20 < perFrame);
        ChartCache c3;
        ccClear(); drawChart(R, T, dark, 1040, 713, 92, 146, 0, 0, nullptr, nullptr);
        const std::size_t flatPer = g_events.size();
        ccClear(); drawChart(R, T, dark, 1040, 713, 92, 146, 0, 0, nullptr, &c3);
        std::printf("chart cache: a flat frame makes %zu shape calls per frame without the cache and %zu with it\n", flatPer, g_events.size());
        checkTrue("cache, flat: a cached frame makes under 5 % of the shape calls the per-frame one makes (" + std::to_string(g_events.size()) + " against " + std::to_string(flatPer) + ")", g_events.size() * 20 < flatPer);
    }

    // ---- 4) draw order and the hit test: what the mesh draws topmost under a pointer is what the hit test (ChartScene::pick) answers ----
    {
        const ChartColours C = ccPalette(false);
        const CcView picks[] = {{1040, 713, ChartHeightMode::HalfLife, 1, 1, 0, 0}, {1040, 713, ChartHeightMode::Binding, 1, 1, 0, 0}, {1040, 713, ChartHeightMode::HalfLife, 0.5, 1, 0, 0}, {1040, 713, ChartHeightMode::HalfLife, 0.03, 1, 0, 0},
                                {1040, 713, ChartHeightMode::HalfLife, 1, 3, -150, 60}};
        for (const CcView& v : picks) {
            ChartScene S; ChartViewState st; st.tilt = v.tilt; st.zoom = v.zoom; st.panX = v.panX; st.panY = v.panY; st.height = v.mode;
            S.build(v.w, v.h, st);
            const ChartLayout& L = S.layout();
            const ChartMesh M = chartLandscapeMesh(L, C, v.mode);
            std::vector<float> xy, rgba; std::vector<int> first;
            chartMeshReference(M.v, M.partB, M.partBCount, S.proj(), xy, rgba, &first);
            // which block each vertex belongs to: the mesh is block after block in chartPaintOrder, so count each block's vertices (6 a quad, 24 the four outlines; a side face counts even when the tilt drops it)
            std::vector<int> start;                                                  // the first vertex (in M.v, relative to partB) of each block, and the block's tile
            int at = 0;
            std::vector<ChartTile> order;
            for (int idx : chartPaintOrder()) {
                const ChartTile& t = chartTiles()[std::size_t(idx)];
                const HeightKind k = chartHeightOf(t.Z, t.N, v.mode).kind;
                start.push_back(at); order.push_back(t);
                at += (k == HeightKind::Measured || k == HeightKind::Stable ? 18 : k == HeightKind::StableNoted ? 24 : k == HeightKind::Approx || k == HeightKind::Limit ? 12 + 6 + 24 : k == HeightKind::Extrapolated ? 6 + 24 : 24);
            }
            checkTrue("hit test: the blocks' vertex counts add up to the mesh (" + std::to_string(at) + ")", at == M.partBCount);
            const auto blockOfVertex = [&](int vtx) { return int(std::upper_bound(start.begin(), start.end(), vtx - M.partB) - start.begin()) - 1; };
            // the filled triangles of each block (from the mesh), and for a hollow block (no half-life known: only an outline is drawn, but it can still be hovered and chosen) the four corners of its outline
            std::vector<std::vector<int>> trisOf(order.size());
            for (std::size_t t = 0; t < first.size(); t++) if (M.v[std::size_t(first[t])].corner < 0) trisOf[std::size_t(blockOfVertex(first[t]))].push_back(int(t));
            std::vector<char> hollow(order.size(), 0);
            for (std::size_t b = 0; b < order.size(); b++) hollow[b] = chartHeightOf(order[b].Z, order[b].N, v.mode).kind == HeightKind::Unknown;
            const auto inTri = [&](const float* q, double px, double py) {
                const double d1 = (px - q[2]) * (q[1] - q[3]) - (q[0] - q[2]) * (py - q[3]), d2 = (px - q[4]) * (q[3] - q[5]) - (q[2] - q[4]) * (py - q[5]), d3 = (px - q[0]) * (q[5] - q[1]) - (q[4] - q[0]) * (py - q[1]);
                return !((d1 < 0 || d2 < 0 || d3 < 0) && (d1 > 0 || d2 > 0 || d3 > 0));
            };
            long points = 0, agree = 0, hits = 0;
            for (double py = 5; py < v.h; py += 13.7) for (double px = 5; px < v.w; px += 15.3) {
                int bestBlock = -1;
                for (std::size_t b = order.size(); b-- > 0 && bestBlock < 0;) {        // last drawn first: the topmost
                    for (int t : trisOf[b]) if (inTri(&xy[6 * std::size_t(t)], px, py)) { bestBlock = int(b); break; }
                    if (bestBlock < 0 && hollow[b]) {                                   // the inside of an outline
                        float q[8]; double sx, sy, d;
                        for (int i = 0; i < 4; i++) { S.proj().map(M.v[std::size_t(M.partB + start[b] + 6 * i)].p0[0], M.v[std::size_t(M.partB + start[b] + 6 * i)].p0[1], M.v[std::size_t(M.partB + start[b] + 6 * i)].p0[2], sx, sy, d); q[2 * i] = float(sx); q[2 * i + 1] = float(sy); }
                        const float t1[6] = {q[0], q[1], q[2], q[3], q[4], q[5]}, t2[6] = {q[0], q[1], q[4], q[5], q[6], q[7]};
                        if (inTri(t1, px, py) || inTri(t2, px, py)) bestBlock = int(b);
                    }
                }
                int Z = 0, N = 0;
                const bool picked = S.pick(px, py, Z, N);
                const bool meshHit = bestBlock >= 0;
                const bool same = picked == meshHit && (!picked || (order[std::size_t(bestBlock)].Z == Z && order[std::size_t(bestBlock)].N == N));
                points++; agree += same; hits += picked;
            }
            std::printf("chart cache: hit test, %s tilt %.2f zoom %d: %ld pointer positions, %ld on a block, %ld differ from the mesh\n", v.mode == ChartHeightMode::Binding ? "binding" : "half-life", v.tilt, int(v.zoom), points, hits, points - agree);
            checkTrue(std::string("hit test, ") + (v.mode == ChartHeightMode::Binding ? "binding" : "half-life") + " tilt " + std::to_string(v.tilt).substr(0, 4) + " zoom " + std::to_string(int(v.zoom)) + ": the block the mesh draws topmost under the pointer is the one ChartScene::pick answers, at " +
                      std::to_string(points) + " pointer positions (" + std::to_string(hits) + " on a block), " + std::to_string(points - agree) + " differ", points - agree <= points / 500);
        }
    }
}
