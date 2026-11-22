// 0.73.0: the Alpha Centauri scene of the Orbit Lab (ViewSettings::alphaCen; the positions and facts: orbit/alphacen.h). Included at the end of orbit_view.cpp
// (it uses that file's picture colours, text painter, sky, scale bar and axes). Every line goes to the graphics card through the Orbit Lab's line path
// (orbit/orbit_lines.h addEllipseRuns: kind 2 runs, the same program and start-up check as the planets' orbits; the processor's drawEllipseCpu is the
// reference and the fallback); the dots, discs and bands are Renderer2D's (drawn by the card's 2D program, a few hundred triangles a frame).

void OrbitView::drawAlphaCen(render::Renderer2D& R, ImDrawList* dl, double originX, double originY, double uiScale, const SolarSystem& sys, const ViewSettings& set, double W, double H) {
    namespace ac = acen;
    TextPainter T{dl, ImGui::GetFont(), originX, originY, uiScale};
    const double px = std::min(W, H) / 34 * std::pow(10.0, set.zoom) / kSimLength;   // points per metre (the Solar System's rule)
    const double jd = sys.julianDayTDB();
    const ac::Stars st = ac::stars(jd);
    if (sel.kind != Selection::None && sel.kind != Selection::ACen) { sel = Selection{}; follow = false; }
    held_ = false;
    if (gliding() && (!(sel == glideSel_) || !follow || journey)) glide_ = glidePending_ = false;
    const auto thingPos = [&](int k) {
        switch (k) {
        case ac::StarA: case ac::CandidateA: return st.A;
        case ac::StarB: return st.B;
        case ac::Proxima: return st.P;
        case ac::PlanetB: case ac::PlanetD: case ac::PlanetC: return st.P + ac::planetOffset(k - ac::PlanetB, jd);
        default: return Vec3{};
        }
    };
    const double sunDist = kLightYearAU * kAU * 1000.0 / ac::kAbPlxMas * 3.26156;   // the Sun, 4.344 light years towards +z (1 pc = 3.26156 ly, as sky_data.inc)

    // ---- the camera: orbit_view.cpp's, about the A-B centre of mass ----
    const bool d3 = set.view3D;
    const double yawA = d3 ? yaw + kPi : 0, pitchA = d3 ? -pitch : 0;
    const double ca = std::cos(yawA), sa = std::sin(yawA), cb = std::cos(pitchA), sb = std::sin(pitchA);
    const double D = std::max(90.0, 3 * 34 / std::pow(10.0, set.zoom)) * kSimLength;
    double cd = cdep;
    const auto base = [&](double x, double y, double z) {
        const double x1 = x * ca - y * sa, y1 = x * sa + y * ca, y2 = y1 * cb - z * sb, dep = y1 * sb + z * cb;
        const double f = d3 ? D / std::max(D - (dep - cd), 0.05 * D) : 1;
        return Q{W / 2 + fx + x1 * px * f, H / 2 - y2 * px * f, dep, f};
    };
    const auto flat = [&](double x, double y, double z) {
        const double x1 = x * ca - y * sa, y1 = x * sa + y * ca, y2 = y1 * cb - z * sb, dep = y1 * sb + z * cb;
        return Q{W / 2 + fx + x1 * px + ox, H / 2 - y2 * px + oy, dep, 1};
    };
    if (follow && sel.kind == Selection::ACen && sel.moon != ac::Sun) {
        const Vec3 t = thingPos(sel.moon);
        cd = cdep = holdDepth(base(t.x, t.y, t.z).dep);  // (0.74.0: orbit_view.cpp's hold: the user's slide, a pick's glide)
        const Q q = base(t.x, t.y, t.z);
        holdAt(q.first, q.second, W, H, set);
    }
    const auto proj = [&](double x, double y, double z) { Q q = base(x, y, z); q.first += ox; q.second += oy; return q; };
    const auto projV = [&](const Vec3& v) { return proj(v.x, v.y, v.z); };
    LineCam lcam;
    lcam.bc = Vec3{}; lcam.ca = ca; lcam.sa = sa; lcam.cb = cb; lcam.sb = sb; lcam.px = px; lcam.D = D; lcam.cd = cd; lcam.W = W; lcam.H = H; lcam.fx = fx; lcam.ox = ox; lcam.oy = oy; lcam.d3 = d3;
    const bool cardLines = linesOnCard(R);
    LinesPoints linesPts;
    linesPts.card = cardLines ? R.linesCard() : nullptr;

    setPictureColours(set.sky != SkyMode::Off, set.theme);
    g_textScale = set.textScale;
    g_minTextU = set.minTextU; g_minTextPx = set.minTextPx;
    if (set.shell) cardHidden = true;
    g_skyHalo = set.sky != SkyMode::Off;
    R.rect(0, 0, W, H, kBg);
    R.setSpace(0.2126f * kBg.r + 0.7152f * kBg.g + 0.0722f * kBg.b < 0.25f ? kBg : Color{0, 0, 0, 1});
    hits_.clear();
    if (set.sky != SkyMode::Off) {                        // the night sky seen from Alpha Centauri (orbit/alphacen.h acenSky)
        const double zt = std::clamp(-set.zoom / 3.5, 0.0, 1.0), lv = set.sky == SkyMode::Full ? 0.45 + 0.55 * zt : 0.16 + 0.6 * zt;
        drawSky(R, T, W, H, ca, sa, cb, sb, set.cons, lv, ac::acenSky());
    }

    // ---- the habitable zones: a faint green band about each star (Kopparapu et al. 2014 with the star's Teff, sun_facts.h), in the A-B orbit's plane for A
    // and B (they move with their star) and in the planets' assumed plane for Proxima ----
    const ac::Ellipse oA = ac::orbitOfA(), oB = ac::orbitOfB(), oP = ac::proximaOrbit();
    const ac::Ellipse pb = ac::planetOrbit(0, st.P);
    const auto band = [&](const Vec3& c, const Vec3& P, const Vec3& Qv, double ri, double ro, Color col) {
        const double bandPx = (ro - ri) * px, fade = std::clamp(1.0 - (bandPx / std::max(W, 1.0) - 1.5) / 3.0, 0.0, 1.0);
        if (ro * px < 1.5 || bigR(W, H, ro * px) || fade <= 0) return;
        const Color k = col.withAlpha(static_cast<float>(col.a * fade));
        for (int i = 0; i < 96; i++) {
            const double t0 = i / 96.0 * 2 * kPi, t1 = (i + 1) / 96.0 * 2 * kPi;
            const Q a = projV(c + P * (ro * std::cos(t0)) + Qv * (ro * std::sin(t0))), b = projV(c + P * (ro * std::cos(t1)) + Qv * (ro * std::sin(t1)));
            const Q e = projV(c + P * (ri * std::cos(t1)) + Qv * (ri * std::sin(t1))), f = projV(c + P * (ri * std::cos(t0)) + Qv * (ri * std::sin(t0)));
            R.triangle(a.first, a.second, k, b.first, b.second, k, e.first, e.second, k);
            R.triangle(a.first, a.second, k, e.first, e.second, k, f.first, f.second, k);
        }
    };
    const Color hzCol = {marks::kHabitable[0] / 255.f, marks::kHabitable[1] / 255.f, marks::kHabitable[2] / 255.f, 0.13f};
    {
        double i0, o0;
        ac::habitableZoneAU(0, i0, o0); band(st.A, oB.P, oB.Q, i0 * kAU, o0 * kAU, hzCol);
        ac::habitableZoneAU(1, i0, o0); band(st.B, oB.P, oB.Q, i0 * kAU, o0 * kAU, hzCol);
        ac::habitableZoneAU(2, i0, o0); band(st.P, pb.P, pb.Q, i0 * kAU, o0 * kAU, hzCol);
        // the candidate around A (UNCONFIRMED): its possible orbit sizes, 1.58-2.23 au (Beichman et al. 2025 Table 4), a very faint band face-on as we see it
        band(st.A, Vec3{1, 0, 0}, Vec3{0, 1, 0}, ac::kCandInnerAU * kAU, ac::kCandOuterAU * kAU, Color{0.82f, 0.69f, 1.0f, 0.05f});
    }

    // ---- the orbits: A's and B's about their centre of mass, Proxima's about A-B, the planets' about Proxima; Proxima c and the candidate band's edges dashed ----
    const Color colA = rgba(255, 236, 200), colB = rgba(255, 196, 130), colP = rgba(255, 138, 112);
    const Color colPl[3] = {hexColor("#8fc7ff"), hexColor("#e8d890"), hexColor("#c9a6ff")};
    const Color colCand = Color{0.82f, 0.69f, 1.0f, 1};
    const auto isOn = [&](int k) { return sel.kind == Selection::ACen && sel.moon == k; };
    const auto hiRing = [&](render::Renderer2D& r2, double x, double y, double r) {   // the pink ring with a dark edge (orbit_view.cpp's)
        r2.ring(x, y, r, 4.0, Color{0, 0, 0, 0.55f});
        r2.ring(x, y, r, 2.0, kHL);
    };
    struct Ln { LineEllipse el; double w; Color c; };
    std::vector<Ln> lines;
    const auto toLine = [](const ac::Ellipse& o, bool dashed) { LineEllipse el; el.F = o.F; el.P = o.P; el.Q = o.Q; el.a = o.a; el.e = o.e; el.b = o.b; el.dashed = dashed; return el; };
    if (set.orbits) {
        lines.push_back({toLine(oA, false), isOn(ac::StarA) ? 1.6 : 1.1, colA.withAlpha(isOn(ac::StarA) ? 0.75f : 0.4f)});
        lines.push_back({toLine(oB, false), isOn(ac::StarB) ? 1.6 : 1.1, colB.withAlpha(isOn(ac::StarB) ? 0.75f : 0.4f)});
        lines.push_back({toLine(oP, false), isOn(ac::Proxima) ? 1.6 : 1.1, colP.withAlpha(isOn(ac::Proxima) ? 0.75f : 0.4f)});
        for (int k = 0; k < 3; k++) {
            const int t = ac::PlanetB + k;
            lines.push_back({toLine(ac::planetOrbit(k, st.P), k == 2), isOn(t) ? 1.6 : 1.1, colPl[k].withAlpha(isOn(t) ? 0.8f : k == 2 ? 0.45f : 0.42f)});
        }
    }
    for (double r : {ac::kCandInnerAU, ac::kCandOuterAU}) {   // the candidate band's edges: always drawn with it (the band is the candidate's mark)
        ac::Ellipse e; e.F = st.A; e.P = {1, 0, 0}; e.Q = {0, 1, 0}; e.a = e.b = r * kAU; e.e = 0;
        lines.push_back({toLine(e, true), 1.0, colCand.withAlpha(isOn(ac::CandidateA) ? 0.7f : 0.28f)});
    }
    {
        std::vector<Vec3> cpts;
        render::LinesDraw tc;
        tc.cam = cardLines ? linesCamera(lcam) : render::LinesCamera{};
        bool onCard = cardLines;
        for (std::size_t i = 0; i < lines.size() && onCard; i++)
            if (!addEllipseRuns(linesPts, tc, (std::uint64_t(4) << 56) | std::uint64_t(i), lines[i].el, lines[i].w, lines[i].c, cpts)) onCard = false;
        if (!(onCard && (tc.runs.empty() || R.lines(tc))))   // the processor's lines (the card's off, not ready or out of room)
            for (const Ln& l : lines) drawEllipseCpu(R, lcam, l.el, l.w, l.c, cpts);
        if (linesPts.card) linesPts.card->frameDone();
    }

    // ---- the bodies ----
    struct Lab { std::string t; double x, y; bool on; int prio; Color c; double size; double dotR = -1; double dotY = 0; double ax = -1e300, ay = 0; };   // (ax, ay): another place for it   // dotR >= 0: a label of a dot at (x, dotY): it may also go left or right of it
    std::vector<Lab> labels;
    const auto inPic = [&](const Q& q, double r) { return q.first > -r - 30 && q.first < W + r + 30 && q.second > -r - 30 && q.second < H + r + 30 && (!d3 || q.dep - cd < 0.9 * D); };
    const auto starCol = [](int s) { const std::array<int, 3> c = bvRGB(ac::starFacts(s).bv); return rgba(c[0], c[1], c[2]); };
    {   // the centre of mass: a crosshair in a ring
        const Q bq = proj(0, 0, 0);
        const bool on = isOn(ac::Bary);
        for (int pass = 0; pass < 2; pass++) {
            const Color c = pass ? (on ? kHL : Color{1, 1, 1, 0.8f}) : Color{0, 0, 0, 0.6f};
            const double w = pass ? 1.6 : 4;
            R.ring(bq.first, bq.second, 5, w, c);
            R.line(bq.first - 8, bq.second, bq.first + 8, bq.second, w, c);
            R.line(bq.first, bq.second - 8, bq.first, bq.second + 8, w, c);
        }
        hits_.push_back({{Selection::ACen, 0, ac::Bary}, bq.first, bq.second, 10});
        if (on) labels.push_back({"A-B centre of mass", bq.first, bq.second + 20, true, 0, kFg, 11});
    }
    const auto drawStar = [&](int k, int s, const Vec3& p, const char* name) {
        const Q q = projV(p);
        const double rTrue = ac::starFacts(s).radius * kSunRadius * px * q.f, r = std::max(rTrue, 3.2);
        if (!inPic(q, r)) return;
        const Color c = starCol(s);
        if (bigR(W, H, r)) { R.rect(0, 0, W, H, c); return; }   // zoomed into the star itself: the picture is its surface
        R.sunDisc(q.first, q.second, r, c);
        if (rTrue < 2.5) R.ring(q.first, q.second, r + 4, 1.2, c.withAlpha(0.7f));
        if (isOn(k)) hiRing(R, q.first, q.second, r + 7);
        hits_.push_back({{Selection::ACen, 0, k}, q.first, q.second, std::max(r, 9.0)});
        labels.push_back({name, q.first, q.second + r + 14, isOn(k), 1, kFg, 12, r, q.second});
    };
    drawStar(ac::StarA, 0, st.A, "Alpha Centauri A");
    drawStar(ac::StarB, 1, st.B, "Alpha Centauri B");
    drawStar(ac::Proxima, 2, st.P, "Proxima Centauri");
    for (int k = 0; k < 3; k++) {                         // the planets: a dot NOT to scale (their sizes are not known)
        const int t = ac::PlanetB + k;
        const Q q = projV(thingPos(t));
        const double orbitPx = ac::planet(k).aAU * kAU * px;
        if (!inPic(q, 6) || (orbitPx < 6 && !isOn(t))) continue;   // closer to Proxima than 6 points: hidden in its dot (pick it to see it)
        if (k < 2) R.disc(q.first, q.second, 3.4, colPl[k]);
        else { R.ring(q.first, q.second, 4.2, 1.3, colPl[k].withAlpha(0.9f)); }   // Proxima c: a hollow mark, it may not exist
        if (isOn(t)) hiRing(R, q.first, q.second, 10);
        hits_.push_back({{Selection::ACen, 0, t}, q.first, q.second, 9});
        labels.push_back({k == 0 ? "Proxima b · at least 1.055 Earths" : k == 1 ? "Proxima d · at least 0.26 Earths" : "Proxima c? · disputed", q.first, q.second - 10, isOn(t), 2, k == 2 ? colPl[2] : kFg, 11, 4.0, q.second});
    }
    {   // the candidate around A: its band's label
        const double r = ac::kCandOuterAU * kAU * px;
        const Q q = projV(st.A + Vec3{-std::sin(0.6), std::cos(0.6), 0} * (ac::kCandOuterAU * kAU));
        if (r > 40 && inPic(q, 0)) {
            labels.push_back({"candidate (unconfirmed, seen once)", q.first, q.second - 4, isOn(ac::CandidateA), 3, colCand, 10});
            hits_.push_back({{Selection::ACen, 0, ac::CandidateA}, q.first, q.second - 8, 14});
        }
    }
    {   // the planets' tilt and the habitable zone, said where they are drawn
        const double r = pb.a * px;
        if (r > 60) {
            const Q q = projV(st.P + pb.Q * (-1.35 * pb.a)), q2 = projV(st.P + pb.Q * (1.35 * pb.a));
            labels.push_back({"planets' tilt assumed (47°, the star's equator)", q.first, q.second, false, 4, kMuted, 10, -1, 0, q2.first, q2.second});
        }
        double i0, o0;
        ac::habitableZoneAU(0, i0, o0);
        if (o0 * kAU * px > 70 && o0 * kAU * px < 4 * W) { const Q q = projV(st.A + oB.P * (o0 * kAU)); labels.push_back({"A's habitable zone", q.first, q.second + 12, false, 5, hexColor("#3d9a5c", 0.9f), 10}); }
        ac::habitableZoneAU(2, i0, o0);
        if (o0 * kAU * px > 70 && o0 * kAU * px < 4 * W) { const Q q = projV(st.P + pb.P * (-o0 * kAU)); labels.push_back({"Proxima's habitable zone", q.first, q.second + 12, false, 5, hexColor("#3d9a5c", 0.9f), 10}); }
    }
    {   // the Sun, 4.344 light years towards +z: a far-away star (seen when the view turns away from it)
        const Q q = flat(0, 0, sunDist);
        if (q.dep - cd < 0.9 * D && q.first > -30 && q.first < W + 30 && q.second > -30 && q.second < H + 30) {
            R.disc(q.first, q.second, 2.8, sunColour()); R.ring(q.first, q.second, 6, 1, Color{1, 1, 1, 0.45f});
            if (isOn(ac::Sun)) hiRing(R, q.first, q.second, 11);
            hits_.push_back({{Selection::ACen, 0, ac::Sun}, q.first, q.second, 10});
            labels.push_back({"Sun · 4.34 light years", q.first, q.second - 11, isOn(ac::Sun), 1, kFg, 11});
        }
    }

    // ---- the labels: the picked one first, then by kind; one that would overlap a label already drawn, or sit under a bar or box, is left out ----
    std::stable_sort(labels.begin(), labels.end(), [](const Lab& a, const Lab& b) { return a.on != b.on ? a.on : a.prio < b.prio; });
    std::vector<LabelBox> placed;
    for (const Lab& l : labels) {
        // where it may go: as given (centred), else (a dot's label) left of the dot, right of it
        struct Try { double x, y, align; };
        std::vector<Try> tries{{l.x, l.y, 0.5}};
        if (l.ax > -1e299) tries.push_back({l.ax, l.ay, 0.5});
        if (l.dotR >= 0) { tries.push_back({l.x - l.dotR - 7, l.dotY + 4, 1.0}); tries.push_back({l.x + l.dotR + 7, l.dotY + 4, 0.0}); }
        bool done = false;
        for (std::size_t ti = 0; ti < tries.size() && !done; ti++) {
            const Try& tr = tries[ti];
            const LabelBox lb = labelBox(tr.x, tr.y, T.width(l.t, l.size), l.size, tr.align);
            bool clash = lb.x0 < 2 || lb.x1 > W - 2;              // not cut by the picture's edge
            for (const LabelBox& o : placed) if (lb.x1 > o.x0 - 3 && lb.x0 < o.x1 + 3 && lb.y1 > o.y0 - 1 && lb.y0 < o.y1 + 1) { clash = true; break; }
            if (set.shell) {
                if (!labelClear(lb, W, H, {set.insetTop, set.reserveBottom, set.insetRight})) clash = true;
                for (const auto& bx : set.blocked) if (lb.x1 > bx[0] && lb.x0 < bx[0] + bx[2] && lb.y1 > bx[1] && lb.y0 < bx[1] + bx[3]) clash = true;
            }
            if (clash && !(l.on && ti + 1 == tries.size())) continue;   // (the picked thing's label is drawn somewhere, at its last place if need be)
            placed.push_back(lb);
            T.text(l.t, tr.x, tr.y, l.size, l.on ? Color{1, 1, 1, 1} : l.c, tr.align, true);
            std::string rn = "aclabel." + l.t;                  // (for the automatic checks: no spaces in a rectangle's name)
            for (char& ch : rn) if (ch == ' ') ch = '_';
            ui::auditRect(rn, static_cast<float>(originX + lb.x0 * uiScale), static_cast<float>(originY + lb.y0 * uiScale), static_cast<float>((lb.x1 - lb.x0) * uiScale), static_cast<float>((lb.y1 - lb.y0) * uiScale));
            done = true;
        }
    }

    // ---- the honest limits, at the bottom left ----
    {
        const double y = H - set.reserveBottom - (set.shell ? 14 : 52);
        const std::string note = "Shown “now” (no light-travel time) · planets’ tilt and node assumed · planet dots not to scale, sizes unknown";
        const double w = T.width(note, 10);
        if (w < W - set.insetRight - 120) { T.text(note, 14, y, 10, kMuted, 0, true, kCard); ui::auditRect("acnote", static_cast<float>(originX + 14 * uiScale), static_cast<float>(originY + (y - 10) * uiScale), static_cast<float>(w * uiScale), static_cast<float>(12 * uiScale)); }
    }

    // ---- the card (the new UI's subject card is made from it), the scale bar, the axes, the date ----
    double gizY = H - 44 - set.reserveRight, barY = H - 24 - set.reserveBottom;
    Card card;
    CardOptions copt;
    copt.paused = set.paused; copt.tspeed = set.tspeed;
    if (set.shell) {
        cardOk = sel.kind != Selection::None && !journey && cardText(sys, sel, copt, card);
        if (cardOk) { lastCard = std::move(card); cardSel = sel; }
    } else if (sel.kind != Selection::None && !cardHidden && !journey && cardText(sys, sel, copt, card)) {
        const double k = std::max(0.74, std::min(1.0, W / 620)), LH = 13.5 * k, F = 10.5 * k, FT = 11 * k;
        double cw = T.width(card.title, FT);
        for (const auto& l : card.lines) cw = std::max(cw, T.width(l, F));
        cw = std::min(cw + 20 * k, W - 16);
        const double ch = 26 * k + LH * (card.lines.size() + 1) + 6 * k, x0 = W - cw - 8, y0 = H - ch - 8;
        gizY = y0 - 30; barY = y0 - 26;
        const ImVec2 a(static_cast<float>(originX + x0 * uiScale), static_cast<float>(originY + y0 * uiScale));
        const ImVec2 b(static_cast<float>(originX + (x0 + cw) * uiScale), static_cast<float>(originY + (y0 + ch) * uiScale));
        dl->AddRectFilled(a, b, ImGui::ColorConvertFloat4ToU32(ImVec4(kCard.r, kCard.g, kCard.b, 0.84f)), static_cast<float>(9 * k * uiScale));
        dl->AddRect(a, b, ImGui::ColorConvertFloat4ToU32(ImVec4(kLine.r, kLine.g, kLine.b, kLine.a)), static_cast<float>(9 * k * uiScale));
        T.text(card.title, x0 + 10 * k, y0 + 17 * k, FT, kFg, 0, false);
        for (std::size_t i = 0; i < card.lines.size(); i++) T.text(card.lines[i], x0 + 10 * k, y0 + 32 * k + LH * i, F, kMuted, 0, false);
        T.text("click anywhere to close this box", x0 + 10 * k, y0 + 32 * k + LH * card.lines.size() + 2, 9.5 * k, kMuted.withAlpha(0.7f), 0, false);
    }
    {
        const ScaleBar sbar = scaleBarFor(px, d3);
        scaleText = sbar.main; lightText = "light takes " + sbar.light; scaleLen = sbar.len;
        if (!set.shell) {
            const double x0 = 14, y = barY;
            for (int pass = 0; pass < 2; pass++) {
                const Color c = pass ? kFg : kCard;
                const double w = pass ? 1.6 : 5;
                R.line(x0, y - 4, x0, y + 4, w, c); R.line(x0, y, x0 + sbar.len, y, w, c); R.line(x0 + sbar.len, y - 4, x0 + sbar.len, y + 4, w, c);
            }
            T.text(sbar.main, x0, y - 8, 11, kFg, 0, true, kCard);
            T.text("light takes " + sbar.light, x0, y + 15, 10, kMuted, 0, true, kCard);
        }
    }
    drawAxesGizmo(R, T, W - 40 - set.insetRight, gizY, ca, sa, cb, sb, originX, originY, uiScale);
    if (!set.shell) T.text("Alpha Centauri on " + fmt::dateUTC(sys.currentUnixMs()), 12, 20, 13, kFg, 0, true);
}
