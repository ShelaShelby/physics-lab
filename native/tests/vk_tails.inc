// 0.78.0: the comets' dust grains on the GPU (kGrainsCS) against the CPU (orbit::tail::CometGrains::position, double, then orbit::beltScreen), through the
// real layer (orbit/tail_layer.h: the ring sent whole, then only the new releases) and the real renderer (included by vk_compare.cpp).
// FLOAT RULE as the rocks' (vk_rocks.inc): the GPU's float orbit may differ from the CPU's double one only by float rounding, bounded and stated. The camera
// puts the comet in the middle with its dust tail's reach (the farthest grain) at 300 points, so kGrainPx points are kGrainPx / 300 of the tail's length.
static const double kGrainPx = 0.01;   // (3.3e-5 of the tail's length; measured 0.00107 on Linux llvmpipe, 0.78.0: the rocks' kRockPx)

struct GrainDiff { double worst = 0; long grains = 0, missing = 0; };

static GrainDiff compareGrains(render::Renderer2D& R, orbit::TailLayer& layer, const orbit::SolarSystem& sys, const orbit::Body& b, orbit::BeltCamera cam) {
    GrainDiff d;
    layer.prepare(R, sys, true);
    const orbit::tail::CometGrains* G = layer.grains(b.comet);
    orbit::Vec3 far;
    if (!G || !G->farthest(far)) { check(std::string("grains: ") + b.comet->name + " has grains", false); return d; }
    const double reach = orbit::length(far * units::kAU - b.pos);
    cam.px = 300 / reach;
    cam.ox = cam.oy = 0; cam.finish();
    double X, Y;
    orbit::beltScreen(cam, b.pos, X, Y);                                         // the comet to the middle
    cam.ox -= X - cam.W / 2; cam.oy -= Y - cam.H / 2; cam.finish();
    draw1(R, int(cam.W), int(cam.H), Color{0, 0, 0, 1}, [&] { layer.drawDust(R, b, cam, true, render::hexColor("#ffe6a0"), 1.4); });
    check(std::string("grains: ") + b.comet->name + ": drawn by the GPU", layer.lastPath() == orbit::TailLayer::Path::Gpu);
    std::vector<float> gpu, cpu;
    const int slot = layer.gpuSlot(b);
    if (slot < 0 || !R.rockCompute()->readPositions(slot, gpu) || int(gpu.size()) != 2 * orbit::tail::kGrains) { check("grains: the GPU places can be read back", false); return d; }
    layer.cpuScreen(b, cam, cpu);
    for (int g = 0; g < orbit::tail::kGrains; g++) {
        const float cx = cpu[std::size_t(2 * g)], cy = cpu[std::size_t(2 * g + 1)], gx = gpu[std::size_t(2 * g)], gy = gpu[std::size_t(2 * g + 1)];
        const bool cOn = cx > -1e8f, gOn = gx > -1e8f;
        if (cOn != gOn) { d.missing++; continue; }
        if (!cOn) continue;
        d.grains++;
        d.worst = std::max(d.worst, std::hypot(double(gx) - double(cx), double(gy) - double(cy)));
    }
    return d;
}

static void gpuGrains(render::Renderer2D& R) {
    render::IRockCompute* rc = R.rockCompute();
    check("grains: the rock compute object has the grains' program (kGrainsCS, its start-up check passed)", rc && rc->grainsOk());
    if (!rc || !rc->grainsOk()) return;
    struct Case { const char* comet; double days; bool d3; };
    // near perihelion (the fastest change), 30 days before and after; Hale-Bopp (e 0.995), NEOWISE (q 0.29 AU, e 0.9992), Encke (e 0.85), Halley (retrograde)
    const Case cases[] = {{"Hale\xE2\x80\x93" "Bopp", 0, false}, {"Hale\xE2\x80\x93" "Bopp", 0, true}, {"Hale\xE2\x80\x93" "Bopp", -30, true}, {"NEOWISE", 0, false}, {"NEOWISE", 25, true},
                          {"Encke", 0, false}, {"Encke", -20, true}, {"Halley", 0, true}, {"Halley", 30, false}};
    double worstAll = 0;
    long all = 0, missingAll = 0;
    for (const Case& c : cases) {
        const orbit::CometData* cd = nullptr;
        for (std::size_t i = 0; i < orbit::kCometCount; i++) if (std::string(orbit::kComets[i].name) == c.comet) cd = &orbit::kComets[i];
        if (!cd) { check(std::string("grains: ") + c.comet + " is in the comet list", false); continue; }
        orbit::TailLayer layer;                                                  // (one layer a case: the first draw sends the whole ring; the steps after send only new releases)
        double worst = 0;
        long n = 0, missing = 0;
        for (int step = 0; step < 4; step++) {
            orbit::SolarSystem sys;
            const double jdUT = cd->perihelionJD + c.days + 0.3 * step;          // (UT for loadAt; the lab turns it into TDB itself)
            sys.loadAt(static_cast<std::int64_t>(std::llround((jdUT - 2440587.5) * 86400000.0)));
            const orbit::Body* b = nullptr;
            for (const orbit::Body& x : sys.bodies) if (x.comet == cd) b = &x;
            if (!b) { check("grains: the comet is a body", false); break; }
            const GrainDiff d = compareGrains(R, layer, sys, *b, rockCamera(sys, 0, c.d3, 0, 0, 0.02, 1040, 713));
            worst = std::max(worst, d.worst); n += d.grains; missing += d.missing;
        }
        std::printf("grains: %s, %+.0f days from perihelion, %s: %ld grain places over 4 times 0.3 day apart, worst %.3g points (tail 300 points), %ld present on one side only\n",
                    c.comet, c.days, c.d3 ? "3D" : "2D", n, worst, missing);
        check(std::string("grains: ") + c.comet + ": every grain within " + std::to_string(kGrainPx) + " points of the CPU's double place (worst " + std::to_string(worst) + ")", n > 0 && worst <= kGrainPx);
        check(std::string("grains: ") + c.comet + ": the same grains present on the GPU and the CPU (" + std::to_string(missing) + " differ)", missing == 0);
        worstAll = std::max(worstAll, worst); all += n; missingAll += missing;
    }
    std::printf("grains: GPU against CPU over %ld grain places: worst %.3g points at 300 points to the dust tail's reach (%.2g of the tail), limit %.2f\n", all, worstAll, worstAll / 300, kGrainPx);
}
