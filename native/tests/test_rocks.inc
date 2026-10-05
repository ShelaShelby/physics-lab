// 0.13 stage 4 part A: where the belt rocks are, checked against Kepler's equation written out as the textbook has it (included by test_orbit.cpp). WRITTEN BEFORE the code.
//
// The textbook reference (bisection, JPL's formulas) is in tests/rock_reference.h. SOURCE of the formulas (already in the About page, "Space and orbits"): NASA/JPL, "Keplerian Elements for Approximate Positions of the Major Planets"
// (https://ssd.jpl.nasa.gov/planets/approx_pos.html, page read 5 Oct 2026): Kepler's equation M = E - e sin E; the position in the orbit plane x' = a (cos E - e), y' = a sqrt(1 - e^2) sin E,
// x' towards perihelion; M brought into -180..+180 degrees before E is solved. The mean motion n = sqrt(GM / a^3) is Kepler's third law. Nothing here calls the lab's own solver: E comes from
// BISECTION (E - e sin E - M rises with E for every e < 1, so there is exactly one root and halving cannot miss it, however close e is to 1).
// FINDING (REPORT.md): the lab's belt code solved E with exactly 3 Newton steps from E0 = M + e sin M. That is exact for every rock the app makes (the largest e is 0.498) but not for e >= 0.8
// (e = 0.9: 5.8e-3 rad off, 0.99: 0.45 rad, 0.999: it runs away). The checks for e = 0.9 and 0.99 below failed before the new solver (orbit/belts.h: solveKepler).

static void testRockKepler() {
    // 1) the solver itself, against bisection, M over a full turn (and far beyond it), e up to 0.999
    {
        double worst = 0, worstE = 0;
        for (double e : {0.0, 0.2, 0.35, 0.5, 0.8, 0.9, 0.99, 0.999}) {
            double w = 0;
            for (int i = -2000; i <= 6000; i++) {
                const double M = 2 * units::kPi * i / 4000.0 + 0.00037;
                const double E = solveKepler(e, M), Eb = rkBisect(e, M), twoPi = 2 * units::kPi;
                w = std::fmax(w, std::fabs(E - Eb));
                const double mred = M - twoPi * std::floor(M / twoPi + 0.5);               // M brought into -pi..pi: solveKepler gives E in the same range
                if (std::fabs((E - e * std::sin(E)) - mred) > 2e-14 / (1 - e) + 1e-14) check(false, "solveKepler e=" + std::to_string(e) + " M=" + std::to_string(M) + ": E satisfies Kepler's equation");
            }
            worstE = std::fmax(worstE, w * (1 - e));
            near(w, 0, 1, 4e-15 / (1 - e) + 2e-15, "solveKepler e=" + std::to_string(e) + " agrees with bisection (worst |E difference| " + std::to_string(w) + ")");
            worst = std::fmax(worst, w);
        }
        std::printf("rocks: solveKepler against bisection, worst |E difference| over e in {0..0.999}: %.3g rad (times (1-e): %.3g)\n", worst, worstE);
    }

    // 2) beltPoint against the textbook, for orbits that are NOT the app's rocks: e = 0, 0.2, 0.9, 0.99 and an Oort-cloud-like long orbit, at several times up to 10 million years
    {
        struct Orb { const char* name; double a, e, M0, incl, om, Om; };
        const Orb orbs[] = {{"circle e=0", 3.0, 0.0, 1.0, 0.1, 0.4, 1.1}, {"e=0.2", 2.7, 0.2, 2.2, 0.3, 1.9, 0.5}, {"e=0.9", 40.0, 0.9, 0.3, 0.5, 4.0, 2.0},
                            {"e=0.99", 60.0, 0.99, 0.05, 0.9, 0.7, 5.0}, {"Oort-like a=50,000 AU e=0.97", 50000.0, 0.97, 0.2, 1.2, 2.5, 3.5}};
        const double years[] = {0, 0.3, 1, 37.5, 1000, 10000, 1e6, 1e7};
        double worst = 0;
        for (const Orb& o : orbs) {
            const BeltGroup g = rkGroup(o.a, o.e, o.M0, o.incl, o.om, o.Om);
            for (double y : years) {
                const double t = y * units::kYear;
                const Vec3 p = beltPoint(g, 0, t), q = rkTextbook(g, 0, t);
                const double scale = double(g.a[0]) * units::kAU, err = length(p - q) / scale;
                worst = std::fmax(worst, err);
                // the same M (the product n t rounds once, in both) then 1e-12 of a is far above the arithmetic and far below any wrong answer (the old e = 0.99 error was 0.45 rad = 45 % of a)
                near(err, 0, 1, 1e-12 / (1 - o.e) + 1e-12, std::string("beltPoint ") + o.name + " after " + std::to_string(y) + " years agrees with the textbook (error " + std::to_string(err) + " of a)");
            }
        }
        std::printf("rocks: beltPoint against the textbook (bisection), 5 orbits x 8 times up to 1e7 years: worst error %.3g of the semi-major axis\n", worst);
    }

    // 3) the Trojans' libration term and 10,000 years of the real belts: the app's own rocks, every moving rock, against the textbook
    {
        const auto belts = makeBelts(ref::kEpochMs);
        double worst = 0; long rocks = 0; double maxE = 0;
        for (const BeltGroup& g : belts) {
            if (g.isStatic) continue;
            for (int k = 0; k < g.n; k++) {
                maxE = std::fmax(maxE, g.e[k]);
                for (double y : {0.0, 100.0, 10000.0}) {
                    const double t = y * units::kYear;
                    const Vec3 p = beltPoint(g, k, t), q = rkTextbook(g, k, t);
                    worst = std::fmax(worst, length(p - q) / (double(g.a[k]) * units::kAU));
                }
                rocks++;
            }
        }
        near(worst, 0, 1, 2e-11, "all " + std::to_string(rocks) + " moving rocks of the app, at 0, 100 and 10,000 years, agree with the textbook (worst " + std::to_string(worst) + " of a)");
        check(maxE < 0.8, "every moving rock of the app has e < 0.8 (the largest is " + std::to_string(maxE) + "): the old 3-step solver was exact for all of them");
        std::printf("rocks: all %ld moving rocks of the app, 3 times, against the textbook: worst %.3g of a, largest e %.3f\n", rocks, worst, maxE);
    }

    // 4) the app's rocks did not move: the new beltPoint against the OLD one (3 Newton steps from M + e sin M), written out here as it was
    {
        const auto belts = makeBelts(ref::kEpochMs);
        double worst = 0;
        for (const BeltGroup& g : belts) {
            if (g.isStatic) continue;
            for (int k = 0; k < g.n; k += 3) {
                for (double y : {0.0, 12.5, 10000.0}) {
                    const double t = y * units::kYear, aN = double(g.aN[k]) * units::kAU;
                    double M = g.M0[k] + std::sqrt(units::kGMSun / (aN * aN * aN)) * t;
                    if (g.libA[k] != 0) M += g.libA[k] * std::sin(t / units::kYear / 150 * 6.2832 + g.libPh[k]);
                    const double e = g.e[k];
                    double E = M + e * std::sin(M);
                    for (int i = 0; i < 3; i++) E += (M - (E - e * std::sin(E))) / (1 - e * std::cos(E));
                    const double a = double(g.a[k]) * units::kAU, xp = a * (std::cos(E) - e), yp = a * std::sqrt(1 - e * e) * std::sin(E);
                    const Vec3 old{xp * g.Px[k] + yp * g.Qx[k], xp * g.Py[k] + yp * g.Qy[k], xp * g.Pz[k] + yp * g.Qz[k]};
                    worst = std::fmax(worst, length(beltPoint(g, k, t) - old) / a);
                }
            }
        }
        near(worst, 0, 1, 1e-9, "the new beltPoint equals the old 3-step one for the app's rocks (worst " + std::to_string(worst) + " of a)");
    }

    // 5) the camera: beltScreen against OrbitView's own formulas (the web's base() and proj(), written out here as the picture has them)
    {
        BeltCamera c; c.bx = 3e9; c.by = -2e9; c.bz = 1e9; c.W = 1040; c.H = 713; c.px = 2.5e-10; c.D = 4e13; c.cd = 1.5e12; c.ox = 37; c.oy = -81; c.d3 = true;
        const double yaw = 0.9 + units::kPi, pitch = -1.1;
        c.ca = std::cos(yaw); c.sa = std::sin(yaw); c.cb = std::cos(pitch); c.sb = std::sin(pitch);
        const Vec3 pts[] = {{1e11, 2e11, 3e10}, {-4e11, 1e12, -2e11}, {7e12, -3e12, 5e11}, {3e9, -2e9, 1e9}};
        for (bool d3 : {true, false}) {
            c.d3 = d3;
            for (const Vec3& p : pts) {
                double x = p.x - c.bx, y = p.y - c.by, z = p.z - c.bz;
                const double x1 = x * c.ca - y * c.sa, y1 = x * c.sa + y * c.ca, y2 = y1 * c.cb - z * c.sb, dep = y1 * c.sb + z * c.cb;
                const double f = d3 ? c.D / std::max(c.D - (dep - c.cd), 0.05 * c.D) : 1;
                double X, Y; beltScreen(c, p, X, Y);
                near(X, c.W / 2 + x1 * c.px * f + c.ox, 1000, 1e-13, "beltScreen x, " + std::string(d3 ? "3D" : "2D"));
                near(Y, c.H / 2 - y2 * c.px * f + c.oy, 1000, 1e-13, "beltScreen y, " + std::string(d3 ? "3D" : "2D"));
            }
        }
        BeltCamera top; top.W = 800; top.H = 600; top.px = 1e-9; top.ox = 10; top.oy = 20;       // top-down, nothing turned: x to the right, y up the picture
        double X, Y; beltScreen(top, Vec3{1e11, 2e11, 5e11}, X, Y);
        near(X, 400 + 100 + 10, 1, 1e-12, "beltScreen top-down: x moves right by px x metres"); near(Y, 300 - 200 + 20, 1, 1e-12, "beltScreen top-down: y moves UP the picture (smaller Y), z does nothing");
    }

    // 6) every set of rocks has a number of its own (the GPU keeps the rocks it was sent under it): new belts, new numbers; a copy keeps its number
    {
        const auto a = makeBelts(ref::kEpochMs), b = makeBelts(ref::kEpochMs);
        std::set<std::uint64_t> seen;
        for (const auto& g : a) seen.insert(g.serial);
        for (const auto& g : b) seen.insert(g.serial);
        check(seen.size() == 8 && !seen.count(0), "every group of every makeBelts call has its own serial (8 of 8 different, none 0)");
        const auto copy = a;
        check(copy[0].serial == a[0].serial && copy[3].serial == a[3].serial, "a copy of the belts keeps the serials (the same rocks)");
        BeltGroup g = a[0];
        check(std::fabs(beltMeanMotion(g, 0) - std::sqrt(units::kGMSun / std::pow(double(g.aN[0]) * units::kAU, 3))) < 1e-18, "beltMeanMotion is Kepler's third law with the rock's own speed distance");
    }
}

