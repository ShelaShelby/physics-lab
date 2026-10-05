// 0.13 stage 4 part A: the Orbit Lab's rocks on the GPU against the CPU (included by gl_compare.cpp, llvmpipe with OpenGL 4.3+).
// REFERENCE: orbit::beltPoint + orbit::beltScreen (double), and for the physics the textbook of tests/rock_reference.h (Kepler's equation by bisection, JPL's formulas).
// The harness was written before the GPU class was first run (the physics tests of tests/test_rocks.inc before any code).
//
// FLOAT RULE (the owner's, stage 2): a 32-bit GPU place may differ from the 64-bit CPU place only by float rounding, and the difference is bounded and stated:
//   screen error (points) <= kRockPx = 0.01 (one hundredth of a pixel, at the benchmark's view) for every rock within 3,000 points of the picture;
//   farther out the error relative to the coordinate: <= 3e-4 of it. Why that large: a rock close to the perspective limit has a huge scale f = D / (D - depth) and the float subtraction D - depth
//   cancels (the depth is up to 3e16 m, D about 1e13 m: 6e-8 x 3e16 = 2e9 m of 5e11 m = 4e-3 at the worst case). Such a rock is thousands of points off the picture; the bound keeps it off the picture.
// The orbit tests below state their own bound from the float spacing and 1 - e: Kepler's equation in float, E up to pi, with the slope 1 - e cos E as small as 1 - e, gives an error in E of
// about 2^-24 * pi / (1 - e); the position adds a few more roundings: bound = (2e-7 + 4e-7 / (1 - e)) of the semi-major axis, a hair above that.
static const double kRockPx = 0.01;

static orbit::BeltCamera rockCamera(const orbit::SolarSystem& sys, double zoom, bool d3, double ox, double oy, double cdFraction, double W = 1040, double H = 713) {
    orbit::BeltCamera c;
    const orbit::Vec3 bc = sys.barycentre();
    const double yawA = d3 ? 0.6 + units::kPi : 0, pitchA = d3 ? -1.0 : 0;       // scene 1's camera: yaw 0.6, pitch 1.0, as OrbitView builds it
    c.bx = bc.x; c.by = bc.y; c.bz = bc.z;
    c.ca = std::cos(yawA); c.sa = std::sin(yawA); c.cb = std::cos(pitchA); c.sb = std::sin(pitchA);
    c.px = std::min(W, H) / 34 * std::pow(10.0, zoom) / units::kSimLength;
    c.W = W; c.H = H; c.D = std::max(90.0, 3 * 34 / std::pow(10.0, zoom)) * units::kSimLength;
    c.cd = cdFraction * c.D; c.ox = ox; c.oy = oy; c.d3 = d3;
    return c;
}

struct RockDiff { double nearPx = 0, farRel = 0; long near = 0, far = 0, rocks = 0; };
// Draws the layer the GPU way through the real renderer (the compute shader runs in R.end()), then reads every rock's place back and compares with the CPU reference.
static RockDiff compareRocks(render::Renderer2D& R, orbit::BeltLayer& layer, const orbit::SolarSystem& sys, const orbit::BeltCamera& cam) {
    RockDiff d;
    draw1(R, 1040, 713, Color{0, 0, 0, 1}, [&] { layer.draw(R, sys, cam, -1, render::hexColor("#ff2bd6"), true); });
    render::IRockCompute* rc = R.rockCompute();
    for (const orbit::BeltGroup& g : sys.belts) {
        const int slot = rc->slotFor(layer.gpuSet(g));
        std::vector<float> xy;
        if (!rc->readPositions(slot, xy) || int(xy.size()) != 2 * g.n) { check("rocks: the GPU positions of " + g.key + " can be read back", false); continue; }
        for (int k = 0; k < g.n; k++) {
            double X, Y;
            orbit::beltScreen(cam, orbit::beltPoint(g, k, sys.t), X, Y);
            const double dx = double(xy[std::size_t(2 * k)]) - X, dy = double(xy[std::size_t(2 * k + 1)]) - Y, err = std::hypot(dx, dy), mag = std::max(std::fabs(X), std::fabs(Y));
            if (mag < 3000) { d.near++; d.nearPx = std::max(d.nearPx, err); }
            else { d.far++; d.farRel = std::max(d.farRel, err / mag); }
            d.rocks++;
        }
    }
    return d;
}

static Picture g_cpuRocks1;                                                     // scene 1's rocks, the CPU way, on the 4.3 context: for the fallback test

static void gpuRocks(render::Renderer2D& R, const std::string& /*dir*/) {
    check("rocks: the 4.3 context has a rock compute object (double precision shader compiled)", R.rockCompute() != nullptr && R.rockCompute()->ok());
    if (!R.rockCompute()) return;
    const std::int64_t epoch = 1767268800000LL;                                  // 2026-01-01 12:00 UTC
    // ---- every rock of scenes 1 and 6, at several times, three cameras ----
    for (const int scale : {1, 8}) {
        orbit::SolarSystem sys;
        sys.loadAt(epoch);
        if (scale != 1) sys.belts = orbit::makeBelts(sys.epochMs, scale);
        orbit::BeltLayer layer;
        const std::string tag = std::string("scene ") + (scale == 1 ? "1" : "6") + " (" + std::to_string(scale * 6000) + " rocks)";
        double worstNear = 0, worstFar = 0; long nearAll = 0, farAll = 0;
        const auto run = [&](const std::string& when) {
            const struct { const char* name; orbit::BeltCamera cam; } cams[] = {
                {"the benchmark's view (3D, zoom x1)", rockCamera(sys, 0.0, true, 0, 0, 0)},
                {"top-down 2D, zoom x30, moved", rockCamera(sys, 1.5, false, 140, -90, 0)},
                {"3D, zoom x10, following something (cd, moved)", rockCamera(sys, 1.0, true, -260, 75, 0.02)}};
            for (const auto& c : cams) {
                const RockDiff d = compareRocks(R, layer, sys, c.cam);
                worstNear = std::max(worstNear, d.nearPx); worstFar = std::max(worstFar, d.farRel); nearAll += d.near; farAll += d.far;
                check(tag + ", " + when + ", " + c.name + ": every rock within " + std::to_string(kRockPx) + " points of the CPU place (worst " + std::to_string(d.nearPx) + ", " + std::to_string(d.near) + " rocks near the picture)", d.nearPx <= kRockPx);
                check(tag + ", " + when + ", " + c.name + ": rocks far off the picture within 3e-4 of their coordinate (worst " + std::to_string(d.farRel) + ", " + std::to_string(d.far) + " rocks)", d.farRel <= 3e-4);
            }
        };
        for (const double years : {0.0, 0.25, 1.0, 100.0, 10000.0, 1e5, 1e6, 1e7}) { sys.t = years * units::kYear; run(std::to_string(long(years)) + " years"); }
        // the fastest Time speed after a long run: the real clock of the lab, 48,000 steps of 1/240 s = 200 real seconds at 1 s = about 50 years
        sys.loadAt(epoch);
        if (scale != 1) sys.belts = orbit::makeBelts(sys.epochMs, scale);
        for (int i = 0; i < 48000; i++) sys.step(units::kPhysicsStep, units::kTspeedMax);
        std::printf("rocks: %s after 200 real seconds at the fastest speed: simulated time %.1f years\n", tag.c_str(), sys.t / units::kYear);
        check(tag + ": 200 real seconds at the fastest speed are about 10,000 simulated years", sys.t / units::kYear > 9000 && sys.t / units::kYear < 11000);
        run("after the run (" + std::to_string(long(sys.t / units::kYear)) + " years)");
        std::printf("rocks: %s, GPU against CPU: worst %.3g points over %ld rocks within 3,000 points of the picture; worst %.3g of the coordinate over %ld farther out\n", tag.c_str(), worstNear, nearAll, worstFar, farAll);
        // regression guard (measured worst + 1 %, Linux llvmpipe): 0.0024 (scene 1), 0.00252 (scene 6) points; a change that makes the GPU's places worse fails here before it reaches the stated bound
        check(tag + ": regression guard: the worst place error stays at the measured " + (scale == 1 ? "0.0024" : "0.00252") + " points + 1 % (" + std::to_string(worstNear) + ")", worstNear <= (scale == 1 ? 0.002425 : 0.002546));
    }

    // ---- the physics, independent of the camera: orbits that are not the app's rocks (Kepler's equation by bisection, tests/rock_reference.h) ----
    {
        struct Orb { const char* name; double a, e, M0, incl, om, Om; };
        const Orb orbs[] = {{"circle e=0", 3.0, 0.0, 1.0, 0.1, 0.4, 1.1}, {"e=0.2", 2.7, 0.2, 2.2, 0.3, 1.9, 0.5}, {"e=0.5", 40.0, 0.5, 3.0, 0.5, 4.0, 2.0}, {"e=0.9", 40.0, 0.9, 0.3, 0.5, 4.0, 2.0},
                            {"e=0.99", 60.0, 0.99, 0.05, 0.9, 0.7, 5.0}, {"Oort-like a=50,000 AU e=0.97", 50000.0, 0.97, 0.2, 1.2, 2.5, 3.5}};
        orbit::SolarSystem sys;                                                   // (only for the barycentre: the camera below puts the Sun in the middle)
        double worstAll = 0;
        std::uint64_t serial = 9000;
        for (const Orb& o : orbs) {
            orbit::BeltGroup g = rk::rkGroup(o.a, o.e, o.M0, o.incl, o.om, o.Om);
            g.serial = ++serial;
            // 40 rocks on the same ellipse a quarter-turn apart in time: the same orbit with different M0, so all parts of the orbit (near perihelion too) are checked at each time
            const int n = 40;
            for (auto* v : {&g.aN, &g.a, &g.e, &g.M0, &g.libA, &g.libPh, &g.Px, &g.Py, &g.Pz, &g.Qx, &g.Qy, &g.Qz}) v->resize(n, (*v)[0]);
            for (int k = 0; k < n; k++) g.M0[std::size_t(k)] = float(2 * units::kPi * k / n + 0.0123);
            g.n = n;
            orbit::SolarSystem one; one.belts.clear(); one.belts.push_back(g);
            orbit::BeltLayer layer;
            // a camera with the Sun in the middle and 300 points to the semi-major axis, looking straight down: screen = 300 x position / a, so an error in points / 300 is an error in a
            orbit::BeltCamera cam; cam.W = cam.H = 1000; cam.px = 300.0 / (double(g.a[0]) * units::kAU); cam.D = 1; cam.d3 = false;
            const double bound = 2e-7 + 4e-7 / (1 - o.e);
            double worst = 0;
            for (const double years : {0.0, 0.3, 1.0, 37.5, 1000.0, 10000.0, 1e6, 1e7}) {
                one.t = years * units::kYear;
                draw1(R, 1000, 1000, Color{0, 0, 0, 1}, [&] { layer.draw(R, one, cam, -1, Color{1, 1, 1, 1}, true); });
                std::vector<float> xy;
                const int slot = R.rockCompute()->slotFor(layer.gpuSet(g));
                if (!R.rockCompute()->readPositions(slot, xy) || int(xy.size()) != 2 * n) { check(std::string("rocks on the GPU, ") + o.name + ": the positions can be read back", false); worst = 1e9; break; }
                for (int k = 0; k < n; k++) {
                    const orbit::Vec3 q = rk::rkTextbook(g, k, one.t);
                    const double X = 500 + q.x * cam.px, Y = 500 - q.y * cam.px;            // (straight down: ca = 1, sa = 0, cb = 1, sb = 0 is the default camera)
                    worst = std::max(worst, std::hypot(double(xy[std::size_t(2 * k)]) - X, double(xy[std::size_t(2 * k + 1)]) - Y) / 300.0);
                }
            }
            worstAll = std::max(worstAll, worst);
            check(std::string("rocks on the GPU, ") + o.name + ", 40 rocks x 8 times up to 10 million years, against the textbook: worst " + std::to_string(worst) + " of a, bound " + std::to_string(bound), worst <= bound);
        }
        std::printf("rocks: GPU against the textbook (bisection), 6 orbits x 40 rocks x 8 times: worst %.3g of the semi-major axis\n", worstAll);
    }
}

// ---- the pictures: scenes 1 and 6, the GPU way against the CPU way, then the logic of the layer (uploads, the switch, the fallback) ----
static void gpuRocksPictures(render::Renderer2D& R, const std::string& dir) {
    if (!R.rockCompute()) return;
    const std::int64_t epoch = 1767268800000LL;
    const Color pink = render::hexColor("#ff2bd6");
    for (const int scale : {1, 8}) {
        orbit::SolarSystem sys;
        sys.loadAt(epoch);
        if (scale != 1) sys.belts = orbit::makeBelts(sys.epochMs, scale);
        orbit::BeltLayer cpuLayer, gpuLayer;
        for (const double years : {0.0, 10000.0}) {
            sys.t = years * units::kYear;
            for (const int selected : {-1, 1}) {
                const orbit::BeltCamera cam = rockCamera(sys, 0.0, true, 0, 0, 0);
                const Picture a = draw1(R, 1040, 713, render::hexColor("#05070d"), [&] { cpuLayer.draw(R, sys, cam, selected, pink, false); });
                const Picture b = draw1(R, 1040, 713, render::hexColor("#05070d"), [&] { gpuLayer.draw(R, sys, cam, selected, pink, true); });
                check("rocks picture: the switch Off draws the CPU way and On the GPU way", cpuLayer.lastPath() == orbit::BeltLayer::Path::Cpu && gpuLayer.lastPath() == orbit::BeltLayer::Path::Gpu);
                const std::string name = std::string("scene ") + (scale == 1 ? "1" : "6") + " rocks, " + std::to_string(long(years)) + " years" + (selected >= 0 ? ", the Trojans selected" : "");
                const Diff d = compare(a, b);
                long lit = 0; for (std::size_t i = 0; i < a.px.size(); i += 4) lit += a.px[i] != a.px[0] || a.px[i + 1] != a.px[1] || a.px[i + 2] != a.px[2];
                // only a rock whose place differs by more than its distance to a pixel-centre line can change a pixel (a place is off by at most 0.0025 points, the mean far less): the bound is one pixel per 100 rocks (measured: 0 to 3 of 48,000)
                const double allowed = double(scale) * 6000 / 100;
                std::printf("%-46s %ld of %ld pixels differ (%.4f %%), largest channel difference %d; %ld pixels lit\n", name.c_str(), d.differing, d.total, 100.0 * double(d.differing) / double(d.total), d.maxLevel, lit);
                check(name + ": the picture shows the rocks (more than 3,000 pixels lit)", lit > 3000);
                check(name + ": the GPU picture differs from the CPU picture in at most one pixel per 100 rocks (" + std::to_string(long(allowed)) + "): the edge pixels of squares that moved by a few thousandths of a point", double(d.differing) <= allowed);
                save(dir, std::string("rocks_") + (scale == 1 ? "s1" : "s6") + "_" + std::to_string(long(years)) + (selected >= 0 ? "_sel" : ""), a, b);
                if (scale == 1 && years == 0 && selected < 0) g_cpuRocks1 = a;
            }
        }
    }

    // ---- the logic ----
    {
        orbit::SolarSystem sys; sys.loadAt(epoch);
        orbit::BeltLayer layer;
        const orbit::BeltCamera cam = rockCamera(sys, 0.0, true, 0, 0, 0);
        render::IRockCompute* rc = R.rockCompute();
        const int up0 = rc->uploads();
        for (int f = 0; f < 50; f++) { sys.t = f * 3.0e5; draw1(R, 1040, 713, Color{0, 0, 0, 1}, [&] { layer.draw(R, sys, cam, -1, pink, true); }); }
        check("rocks logic: 50 GPU frames send the 4 belts once (not every frame)", rc->uploads() - up0 == 4);
        check("rocks logic: the GPU path does no CPU rock work (beltPoint is not called for any rock)", layer.cpuRocksPlaced() == 0);
        sys.loadAt(epoch + 86400000LL);                                          // a new date: new rocks, new serials
        draw1(R, 1040, 713, Color{0, 0, 0, 1}, [&] { layer.draw(R, sys, cam, -1, pink, true); });
        check("rocks logic: new belts (a new date) are sent again, once (4 more uploads)", rc->uploads() - up0 == 8);
        draw1(R, 1040, 713, Color{0, 0, 0, 1}, [&] { layer.draw(R, sys, cam, -1, pink, true); });
        check("rocks logic: and not again on the next frame", rc->uploads() - up0 == 8);
        // the switch Off: the CPU path, positions kept while the time does not move, made again when it moves or when the belts are replaced (a stale cache would show the old rocks)
        orbit::BeltLayer cpu;
        draw1(R, 1040, 713, Color{0, 0, 0, 1}, [&] { cpu.draw(R, sys, cam, -1, pink, false); });
        const long once = cpu.cpuRocksPlaced();
        check("rocks logic: the CPU path placed all 6,000 rocks on its first frame", once == 6000);
        draw1(R, 1040, 713, Color{0, 0, 0, 1}, [&] { cpu.draw(R, sys, cam, -1, pink, false); });
        check("rocks logic: the CPU path keeps the places while the time does not move (paused, or only the view moved)", cpu.cpuRocksPlaced() == once);
        sys.t += 1000;
        draw1(R, 1040, 713, Color{0, 0, 0, 1}, [&] { cpu.draw(R, sys, cam, -1, pink, false); });
        check("rocks logic: the CPU path places them again when the time moved", cpu.cpuRocksPlaced() == 2 * once);
        sys.belts = orbit::makeBelts(sys.epochMs, 1);                            // other rocks at the same time and date (as the benchmark's stress scene does)
        draw1(R, 1040, 713, Color{0, 0, 0, 1}, [&] { cpu.draw(R, sys, cam, -1, pink, false); });
        check("rocks logic: the CPU path places them again when the belts were replaced at the same time", cpu.cpuRocksPlaced() == 3 * once);
        // a compute object that is not ok, or that cannot take the rocks: the CPU path, the same picture
        struct Never : render::IRockCompute {
            int slots = 0, dispatches = 0;
            bool ok() const override { return true; }
            void keepOnly(const std::vector<std::uint64_t>&) override {}
            int slotFor(const render::RockSet&) override { slots++; return -1; }
            bool dispatch(int, const render::RockFrame&) override { dispatches++; return false; }
            unsigned positionsBuffer(int) const override { return 0; }
            bool readPositions(int, std::vector<float>&) override { return false; }
            int uploads() const override { return 0; }
        };
        // more sets than the GPU holds (16): the layer cannot send them all, so the CPU draws that frame; the next normal frame is on the GPU again
        {
            orbit::SolarSystem many = sys;
            many.belts.clear();
            for (int i = 0; i < 17; i++) { orbit::BeltGroup g = sys.belts[0]; g.serial = 70000 + std::uint64_t(i); many.belts.push_back(g); }
            orbit::BeltLayer ml;
            draw1(R, 1040, 713, Color{0, 0, 0, 1}, [&] { ml.draw(R, many, cam, -1, pink, true); });
            check("rocks logic: 17 sets of rocks do not fit in the 16 slots: that frame is drawn by the CPU", ml.lastPath() == orbit::BeltLayer::Path::Cpu);
            draw1(R, 1040, 713, Color{0, 0, 0, 1}, [&] { ml.draw(R, sys, cam, -1, pink, true); });
            check("rocks logic: the next frame with the normal belts is on the GPU again (the other sets were let go)", ml.lastPath() == orbit::BeltLayer::Path::Gpu);
        }
        render::Renderer2D Rf;
        Never* never = new Never;
        Rf.useRockCompute(never);
        orbit::BeltLayer fl;
        Rf.begin(1040, 713);
        fl.draw(Rf, sys, cam, -1, pink, true);
        check("rocks logic: a compute object that cannot take the rocks falls back to the CPU path (it was asked, then the CPU placed every rock)", never->slots >= 1 && fl.lastPath() == orbit::BeltLayer::Path::Cpu && fl.cpuRocksPlaced() == 6000);
        Rf.useRockCompute(nullptr);
        orbit::BeltLayer nl;
        Rf.begin(1040, 713);
        nl.draw(Rf, sys, cam, -1, pink, true);
        check("rocks logic: no compute object at all (a 3.3 PC): the CPU path", nl.lastPath() == orbit::BeltLayer::Path::Cpu && nl.cpuRocksPlaced() == 6000);
    }
}

// the 3.3 context (called from fallback33): no rock compute object, the CPU path, the picture the same as the 4.3 context's CPU picture
static void gpuRocksFallback33(render::Renderer2D& R33, const std::string& dir) {
    check("3.3 context: there is no rock compute object either", R33.rockCompute() == nullptr);
    orbit::SolarSystem sys; sys.loadAt(1767268800000LL);
    orbit::BeltLayer layer;
    const orbit::BeltCamera cam = rockCamera(sys, 0.0, true, 0, 0, 0);
    const Picture p = draw1(R33, 1040, 713, render::hexColor("#05070d"), [&] { layer.draw(R33, sys, cam, -1, render::hexColor("#ff2bd6"), true); });
    check("3.3 context: the rocks are drawn by the CPU path (with the switch On)", layer.lastPath() == orbit::BeltLayer::Path::Cpu && layer.cpuRocksPlaced() == 6000);
    if (g_cpuRocks1.px.empty()) check("scene 1 rocks picture from the 4.3 context was kept", false);
    else report(dir, "3.3 fallback scene 1 rocks against the 4.3 context's CPU picture", g_cpuRocks1, p, 0);
}
