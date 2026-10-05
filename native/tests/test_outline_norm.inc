// 0.13 stage 3, part B: the EXACT normalisation of the hydrogen-like states (included by test_atom.cpp after test_gpu_compute.inc). Written BEFORE the fix, and failing until it:
// stage 2 found that orbitalNorm (a midpoint sum, 400 points in cos theta) left the amplitude too big by 3.1e-6 (2p) and 1.6e-5 (3d z2).
// REFERENCES, none of them the lab's own sums:
//  (1) hand-derived exact values of the integral of psi^2 over all space at Z = 1, with the lab's unnormalised psi (R~ = L_(n-l-1)^(2l+1)(2r/n) e^(-r/n), angular polynomial P):
//        1s:   psi = e^-r                    int e^(-2r) r^2 dr * 4 pi            = (1/4) * 4 pi                 = pi
//        2p z: psi = z e^(-r/2)              int r^4 e^(-r) dr * 4 pi/3           = 24 * 4 pi/3                  = 32 pi
//        3d z2: psi = (3 z^2 - r^2) e^(-r/3) int r^6 e^(-2r/3) dr * 16 pi/5       = 720 (3/2)^7 * 16 pi/5        = 39366 pi
//        4f z3: psi = z (5 z^2 - 3 r^2) e^(-r/4)   int r^8 e^(-r/2) dr * 16 pi/7  = 8! 2^9 * 16 pi/7            = 47185920 pi
//      (radial integrals: int r^k e^(-a r) dr = k!/a^(k+1); angular: int (3 cos^2 - 1)^2 dOmega = 16 pi/5, int cos^2 (5 cos^2 - 3)^2 dOmega = 16 pi/7, Griffiths ch. 4 / the sphere moments).
//  (2) for EVERY orbital the app offers (all n, l, real and complex m, and several Z): the integral of |normFactor psi|^2 by composite Gauss-Legendre in r, Gauss-Legendre in cos(theta) and a
//      uniform phi grid (exact for polynomials), written in this file, equals 1 to 1e-10.
// The closed form the code now uses: radial int = (n/2)^(2l+3) 2n (n+l)!/(n-l-1)! (DLMF 18.3 and 18.9.5, the Laguerre orthogonality and recurrence; Abramowitz and Stegun 22.2.12, 22.7.12),
// angular int = the sphere integral of the polynomial squared (Griffiths ch. 4: the cubic harmonics are combinations of the Y_l^m).

static void testExactNormalisation() {
    const double pi = 3.14159265358979323846;
    struct Hand { const char* key; double value; };
    const Hand hand[] = {{"1s", pi}, {"2p|z", 32 * pi}, {"3d|z2", 39366 * pi}, {"4f|z3", 47185920 * pi}};
    for (const Hand& h : hand)
        checkNum(std::string("exact normalisation: orbitalNorm of ") + h.key + " is the hand-derived integral (1e-14)", orbitalNorm(orbIndex(h.key)), h.value, 1e-14);
    // the quadrature of every state
    std::vector<double> gx, gw;
    gcpGaussLegendre(24, gx, gw);
    std::vector<double> rx, rw;
    gcpGaussLegendre(16, rx, rw);
    double worst = 0; int states = 0; std::string worstName;
    for (std::size_t oi = 0; oi < orbitals().size(); oi++) {
        const Orbital& o = orbitals()[oi];
        std::vector<int> ids = {int(oi)};
        if (o.key.substr(o.key.find('|') + 1) == orbitals()[std::size_t(orbIndex(o.shell))].ori)       // the first orientation of a shell: also its complex states
            for (int m = -o.l; m <= o.l; m++) if (m) ids.push_back(complexState(int(oi), m));
        for (const int id : ids)
            for (const int Z : {1, 2, 5}) {
                const double nf = normFactor(id, Z);
                double sum = 0;
                const double panel = 2.5 / Z, top = 130.0 / Z;
                for (double a = 0; a < top; a += panel)
                    for (int ir = 0; ir < 16; ir++) {
                        const double r = a + 0.5 * panel * (rx[std::size_t(ir)] + 1), wr = 0.5 * panel * rw[std::size_t(ir)];
                        for (int j = 0; j < 24; j++) {
                            const double ct = gx[std::size_t(j)], st = std::sqrt(1 - ct * ct);
                            for (int k = 0; k < 16; k++) {
                                const double ph = 2 * pi * (k + 0.5) / 16, x = r * st * std::cos(ph), y = r * st * std::sin(ph), z = r * ct;
                                const double v = isComplexState(id) ? std::norm(psiComplex(id, Z, x, y, z)) : psi(id, Z, x, y, z) * psi(id, Z, x, y, z);
                                sum += nf * nf * v * r * r * wr * gw[std::size_t(j)] * (2 * pi / 16);
                            }
                        }
                    }
                if (std::fabs(sum - 1) > worst) { worst = std::fabs(sum - 1); worstName = stateText(id) + " Z=" + std::to_string(Z); }
                states++;
            }
    }
    std::printf("exact normalisation: %d state and Z combinations integrated by quadrature, worst |integral - 1| = %.2e (%s)\n", states, worst, worstName.c_str());
    checkTrue("exact normalisation: every orbital the app offers (all n, l, real and complex m, Z = 1, 2, 5) integrates to 1 within 1e-10 by an independent quadrature (" + std::to_string(states) + " cases)", worst < 1e-10 && states > 100);
}

// ---- the 90 % level, independent of the lab's grid (part A, written before the GPU code) ----
// 1s: the probability inside radius R is P(R) = 1 - e^(-2R) (1 + 2R + 2R^2) (Griffiths ch. 4: the radial density 4 r^2 e^(-2r)); the radius enclosing 90 % solves P(R) = 0.9 (Newton, to 1e-14: the
// residual is checked, "1e-9" of the brief), and the level of the 90 % region is the density there, rho(R) = e^(-2R) / pi (a0 = 1).
// 2p z and 3d z2 (axially symmetric): rho(r, u = cos theta) = r^2 u^2 e^-r / (32 pi) and r^4 (3u^2 - 1)^2 e^(-2r/3) / (39366 pi), the textbook formulas with the exact norms of testExactNormalisation; the level is found
// by sorting the cells of a fine (r, u) grid (3000 x 400, midpoint) densest first until they hold 0.9 (the lab's own definition of the region, the standard textbook one), accurate to ~1e-5 relative.
// The lab's level comes from a 64^3 voxel grid, so it is compared "to the grid's resolution": see the tolerances (measured, stated).
struct LevelRef { double c = 0; };
static LevelRef gcpLevelAxial(int which) {                  // which: 1 = 2p z, 2 = 3d z2; the level c of the 90 % region, in the units of the normalised density (a0^-3)
    const double pi = 3.14159265358979323846;
    const int nr = 3000, nu = 400;
    const double top = which == 1 ? 40.0 : 70.0, dr = top / nr, du = 1.0 / nu;     // u in [0, 1], the density is symmetric in u: the mass of a cell counts twice
    struct Cell { double rho, mass; };
    std::vector<Cell> cells; cells.reserve(std::size_t(nr) * nu);
    for (int i = 0; i < nr; i++) {
        const double r = (i + 0.5) * dr;
        for (int j = 0; j < nu; j++) {
            const double u = (j + 0.5) * du;
            const double rho = which == 1 ? r * r * u * u * std::exp(-r) / (32 * pi) : std::pow(r, 4) * (3 * u * u - 1) * (3 * u * u - 1) * std::exp(-2 * r / 3) / (39366 * pi);
            cells.push_back({rho, rho * 2 * pi * r * r * dr * du * 2});
        }
    }
    std::sort(cells.begin(), cells.end(), [](const Cell& a, const Cell& b) { return a.rho > b.rho; });
    double acc = 0, total = 0; for (const Cell& c : cells) total += c.mass;
    LevelRef out;
    for (const Cell& c : cells) { acc += c.mass; if (acc >= 0.9 * total) { out.c = c.rho; break; } }
    return out;
}

static void testOutlineLevelIndependent() {
    const double pi = 3.14159265358979323846;
    // 1s closed form
    double R = 2.5;
    for (int it = 0; it < 60; it++) { const double e = std::exp(-2 * R), P = 1 - e * (1 + 2 * R + 2 * R * R), dP = 4 * R * R * e; R -= (P - 0.9) / dP; }
    const double residual = std::fabs(1 - std::exp(-2 * R) * (1 + 2 * R + 2 * R * R) - 0.9);
    checkTrue("outline level: the 1s radius enclosing 90 % solves 1 - e^(-2R)(1 + 2R + 2R^2) = 0.9: R = " + std::to_string(R).substr(0, 9) + " a0, residual " + std::to_string(residual) + " (< 1e-9)", residual < 1e-9 && R > 2 && R < 3);
    const double c1 = std::exp(-2 * R) / pi;
    const Outline& o1 = outlineOf(orbIndex("1s"));
    const double lab1 = o1.c / o1.norm, h1 = 2 * o1.half / kOutlineGrid;
    std::printf("outline level 1s: exact %.6e, lab (64^3 grid) %.6e (%.2f %% off), mass inside the lab's region %.5f, lab reach %.4f a0 against the exact R %.4f (grid step %.4f)\n", c1, lab1, 100 * (lab1 / c1 - 1), o1.mass, o1.rmax, R, h1);
    checkTrue("outline level: the lab's 1s level is the exact level to the 64^3 grid's resolution (within 3 % of the density; measured 1.91 %)", std::fabs(lab1 / c1 - 1) < 0.03);
    checkTrue("outline level: the lab's 1s region holds 0.9 to the grid's resolution (within 0.01) and reaches R to a grid step", std::fabs(o1.mass - 0.9) < 0.01 && std::fabs(o1.rmax - R) < 1.5 * h1);
    // 2p z and 3d z2
    for (const int which : {1, 2}) {
        const LevelRef ref = gcpLevelAxial(which);
        const Outline& o = outlineOf(orbIndex(which == 1 ? "2p|z" : "3d|z2"));
        const double lab = o.c / o.norm;
        std::printf("outline level %s: textbook quadrature %.6e, lab (64^3 grid) %.6e (%.2f %% off), mass inside %.5f\n", which == 1 ? "2p z" : "3d z2", ref.c, lab, 100 * (lab / ref.c - 1), o.mass);
        checkTrue(std::string("outline level: the lab's ") + (which == 1 ? "2p z" : "3d z2") + " level is the textbook one to the grid's resolution (within 3 %; measured above), and its region holds 0.9 to 0.01", std::fabs(lab / ref.c - 1) < 0.03 && std::fabs(o.mass - 0.9) < 0.01);
    }
}

// ---- the logic around the GPU outline, with a stand-in for the GPU (so it runs on every machine, also without OpenGL) ----
namespace {
struct OcFake : render::IOutlineCompute {
    // what the stand-in "GPU" answers: the CPU reference's own grid and level for the orbital the test is about (the test sets them), after `delay` polls that say Pending
    std::vector<float> grid; float level = 0; std::vector<float> plane;
    int delay = 2, gridCalls = 0, planeCalls = 0, cancels = 0, nextJob = 1, pollsSeen = 0;
    bool fail = false, startFails = false;
    render::OutlineDensity lastDesc; int lastN = 0; float lastHalf = 0, lastScale = 0, lastTarget = 0; bool lastHorizontal = false; float lastOffset = 0; int lastPlaneN = 0;
    int jobGrid = 0, jobPlane = 0;
    bool ok() const override { return true; }
    int beginGrid(const render::OutlineDensity& d, int N, float half, float scale, float target) override {
        gridCalls++; lastDesc = d; lastN = N; lastHalf = half; lastScale = scale; lastTarget = target;
        if (startFails) return 0;
        pollsSeen = 0; return jobGrid = nextJob++;
    }
    int beginPlane(const render::OutlineDensity& d, int n, float half, bool horizontal, float offset) override {
        planeCalls++; lastDesc = d; lastPlaneN = n; lastHalf = half; lastHorizontal = horizontal; lastOffset = offset;
        if (startFails) return 0;
        pollsSeen = 0; return jobPlane = nextJob++;
    }
    render::JobState pollGrid(int job, std::vector<float>& g, float& lv) override {
        if (job != jobGrid) return render::JobState::Failed;
        if (fail) return render::JobState::Failed;
        if (pollsSeen++ < delay) return render::JobState::Pending;
        g = grid; lv = level; jobGrid = 0; return render::JobState::Done;
    }
    render::JobState pollPlane(int job, std::vector<float>& v) override {
        if (job != jobPlane) return render::JobState::Failed;
        if (fail) return render::JobState::Failed;
        if (pollsSeen++ < delay) return render::JobState::Pending;
        v = plane; jobPlane = 0; return render::JobState::Done;
    }
    void cancel(int job) override { cancels++; if (job == jobGrid) jobGrid = 0; if (job == jobPlane) jobPlane = 0; }
};

// the kernel's density, from the description alone (the C++ twin of kOutlineDensityCS, used to check the description)
double descDensity(const render::OutlineDensity& d, double x, double y, double z) {
    const double r = std::sqrt(x * x + y * y + z * z), q = 2 * r / d.n;
    double L = 0;
    for (int i = 3; i >= 0; i--) L = L * q + d.lag[i];
    const double R = L * std::exp(-r / d.n), A = angular(d.ang, x, y, z), B = angular(d.angIm, x, y, z);
    return R * R * d.weight * (A * A + double(d.cIm) * d.cIm * B * B);
}
}  // namespace

static bool sameOutline(const Outline& a, const Outline& b) {
    if (a.c != b.c || a.mass != b.mass || a.rmax != b.rmax || a.half != b.half || a.norm != b.norm || a.wire.size() != b.wire.size() || a.side.size() != b.side.size()) return false;
    for (std::size_t i = 0; i < a.wire.size(); i++) if (a.wire[i].x0 != b.wire[i].x0 || a.wire[i].z1 != b.wire[i].z1) return false;
    for (std::size_t i = 0; i < a.side.size(); i++) if (a.side[i].x0 != b.side[i].x0 || a.side[i].y1 != b.side[i].y1) return false;
    return true;
}

static void testOutlineGpuLogic() {
    const int orbs[] = {orbIndex("4f|z3"), orbIndex("2p|z"), orbIndex("3d|z2"), complexState(orbIndex("4f"), 3), orbIndex("3s")};
    // 1. the description handed to the GPU is the lab's density, for real and complex states, at 30 points each
    {
        bool desc = true; double worst = 0;
        for (const int orb : orbs) {
            const render::OutlineDensity d = outlineDensityOf(orb);
            for (int k = 0; k < 30; k++) {
                const double x = 0.7 * std::sin(1.3 * k + 0.2) * (1 + k % 5), y = 0.9 * std::cos(0.7 * k) * (1 + k % 3), z = 1.1 * std::sin(2.1 * k + 1) * (1 + k % 4);
                const double a = descDensity(d, x, y, z), b = density(orb, 1, x, y, z);
                worst = std::max(worst, std::fabs(a - b) / std::max(b, 1e-300));
                if (b > 1e-30) desc = desc && std::fabs(a - b) <= 1e-12 * b;
            }
        }
        checkTrue("outline GPU: the description of the density handed to the kernel (n, Laguerre coefficients, angular polynomials, weight) gives the lab's density for 4f, 2p, 3d, a complex 4f (m = +3) and 3s (1e-12; worst " + std::to_string(worst).substr(0, 8) + ")", desc);
    }
    for (const int orb : orbs) {
        const std::string tag = "outline GPU, " + stateText(orb) + ": ";
        const std::vector<float> g = outlineGridCpu(orb);
        const Orbital& o = orbitals()[stateOrbital(orb)];
        const double half = 1.15 * kExt[o.n], h = 2 * half / kOutlineGrid, dV = h * h * h, norm = orbitalNorm(orb);
        const float level = outlineLevelCpu(g, dV, norm);
        const Outline ref = outlineFromGrid(orb, g, level);                   // what the CPU makes from this grid and level
        clearOutlineCache();
        OcFake fake; fake.grid = g; fake.level = level;
        setOutlineCompute(&fake);
        const Outline* first = outlineReady(orb, true);
        checkTrue(tag + "the first call starts the job and returns nothing (it never waits); the job asks for the 64^3 grid over 1.15 times the orbital's reach, the scale dV / norm and 0.9", first == nullptr && fake.gridCalls == 1 && fake.lastN == kOutlineGrid &&
                  fake.lastHalf == float(half) && fake.lastScale == float(dV / norm) && fake.lastTarget == float(kOutlineProb));
        const Outline* p1 = outlineReady(orb, true); const Outline* p2 = outlineReady(orb, true);
        checkTrue(tag + "while the GPU is busy the answer stays nothing, and no second job is started", p1 == nullptr && p2 == nullptr && fake.gridCalls == 1);
        const Outline* done = outlineReady(orb, true);
        checkTrue(tag + "when the GPU is done the outline appears, built from the GPU's grid and level by the CPU's own code (identical to the CPU's from the same grid)", done != nullptr && sameOutline(*done, ref));
        checkTrue(tag + "and it is cached: the next call returns it at once without a new job", outlineReady(orb, true) == done && fake.gridCalls == 1);
        // the CPU path's own outline from its own grid is the same function: outlineOf(orb) equals outlineFromGrid of its grid and level
        clearOutlineCache();
        checkTrue(tag + "the CPU reference outlineOf is exactly outlineFromGrid(outlineGridCpu, outlineLevelCpu)", sameOutline(outlineOf(orb), ref));
        setOutlineCompute(nullptr);
    }
    // 2. a failing GPU, the switch Off, and no compute object: the CPU path, the same outline
    {
        const int orb = orbIndex("2p|z");
        clearOutlineCache();
        const Outline cpu = outlineOf(orb);
        clearOutlineCache();
        OcFake bad; bad.fail = true; setOutlineCompute(&bad);
        const Outline* a = outlineReady(orb, true);                              // starts
        const Outline* b = a ? a : outlineReady(orb, true);                      // the poll says Failed: the CPU makes it now
        checkTrue("outline GPU: a job that fails falls back to the CPU outline, the same one, without a blank frame beyond the one that started it", b != nullptr && sameOutline(*b, cpu));
        clearOutlineCache();
        OcFake off; setOutlineCompute(&off);
        const Outline* c = outlineReady(orb, false);
        checkTrue("outline GPU: switch Off: the CPU path at once, the compute object is not called", c != nullptr && off.gridCalls == 0 && sameOutline(*c, cpu));
        clearOutlineCache();
        OcFake nostart; nostart.startFails = true; setOutlineCompute(&nostart);
        const Outline* d = outlineReady(orb, true);
        checkTrue("outline GPU: a job that cannot start: the CPU path at once", d != nullptr && sameOutline(*d, cpu));
        clearOutlineCache();
        setOutlineCompute(nullptr);
        const Outline* e = outlineReady(orb, true);
        checkTrue("outline GPU: no compute object (OpenGL 3.3): the CPU path at once", e != nullptr && sameOutline(*e, cpu));
        // clearing the cache cancels a job in flight
        clearOutlineCache();
        OcFake fk; fk.delay = 100; setOutlineCompute(&fk);
        outlineReady(orb, true);
        const int calls = fk.gridCalls;
        clearOutlineCache();
        checkTrue("outline GPU: clearing the outline cache cancels the job in flight, and the next call starts a new one", fk.cancels == 1 && (outlineReady(orb, true) == nullptr) && fk.gridCalls == calls + 1);
        clearOutlineCache(); setOutlineCompute(nullptr);
    }
    // 3. the Slice: the contour from given values is the contour outlineSlice makes
    {
        const int orb = orbIndex("4f|z3");
        clearOutlineCache();
        const Outline& O = outlineOf(orb);
        bool same = true;
        for (const bool hz : {false, true})
            for (const double off : {-0.8, 0.0, 0.37, 2.5}) {
                const std::vector<OutSeg2> a = outlineSlice(orb, hz, off), b = outlineSliceFromValues(O, outlinePlaneCpu(orb, O.half, hz, off, 200));
                same = same && a.size() == b.size() && !a.empty() == !b.empty();
                for (std::size_t i = 0; same && i < a.size(); i++) same = a[i].x0 == b[i].x0 && a[i].y0 == b[i].y0 && a[i].x1 == b[i].x1 && a[i].y1 == b[i].y1;
            }
        checkTrue("outline GPU: the Slice contour built from the plane values is identical to outlineSlice's, for both planes and four cuts", same);
    }
    // 3b. the Slice contour kept from frame to frame: the CPU path, and the GPU job's life (start, pending, done, the cut moving, another orbital, the switch, a failure)
    {
        const int orb = orbIndex("4f|z3"), orb2 = orbIndex("3d|z2");
        clearOutlineCache();
        const Outline& O = outlineOf(orb);
        const Outline& O2 = outlineOf(orb2);
        const auto sameSegs = [](const std::vector<OutSeg2>& a, const std::vector<OutSeg2>& b) {
            if (a.size() != b.size()) return false;
            for (std::size_t i = 0; i < a.size(); i++) if (a[i].x0 != b[i].x0 || a[i].y0 != b[i].y0 || a[i].x1 != b[i].x1 || a[i].y1 != b[i].y1) return false;
            return true;
        };
        setOutlineCompute(nullptr);
        SliceContour cpu;
        cpu.update(O, orb, false, 0.3, true);
        checkTrue("Slice contour: with no compute object the contour is made at once, and is outlineSlice's", cpu.shows(orb, false) && !cpu.jobInFlight() && sameSegs(cpu.segments(), outlineSlice(orb, false, 0.3)));
        OcFake fake; fake.delay = 1; setOutlineCompute(&fake);
        fake.plane = outlinePlaneCpu(orb, O.half, false, 0.3, 200);
        SliceContour sc;
        sc.update(O, orb, false, 0.3, true);
        checkTrue("Slice contour: the first frame starts a job for this plane, with the outline's reach, 200 cells and the cut, and shows nothing yet", sc.jobInFlight() && !sc.shows(orb, false) && fake.planeCalls == 1 && fake.lastPlaneN == 200 &&
                  fake.lastHalf == float(O.half) && !fake.lastHorizontal && fake.lastOffset == float(0.3));
        sc.update(O, orb, false, 0.3, true);
        checkTrue("Slice contour: while the GPU is busy nothing is shown and no second job starts", sc.jobInFlight() && !sc.shows(orb, false) && fake.planeCalls == 1);
        sc.update(O, orb, false, 0.3, true);
        checkTrue("Slice contour: when the job is done the contour is the one built from its values, equal to outlineSlice's, and no new job starts for the same cut", sc.shows(orb, false) && !sc.jobInFlight() && fake.planeCalls == 1 && sameSegs(sc.segments(), outlineSlice(orb, false, 0.3)));
        fake.plane = outlinePlaneCpu(orb, O.half, false, 0.5, 200);
        sc.update(O, orb, false, 0.5, true);
        checkTrue("Slice contour: the cut moves: a new job starts and meanwhile the contour of the old cut stays on screen (a frame behind)", sc.jobInFlight() && sc.shows(orb, false) && fake.planeCalls == 2 && sameSegs(sc.segments(), outlineSlice(orb, false, 0.3)));
        fake.plane = outlinePlaneCpu(orb2, O2.half, false, 0.2, 200);
        sc.update(O2, orb2, false, 0.2, true);
        checkTrue("Slice contour: another orbital is asked for while a job is pending: the old orbital's contour is never shown for it, and the late result is dropped", !sc.shows(orb2, false) && fake.planeCalls == 2);
        sc.update(O2, orb2, false, 0.2, true);
        checkTrue("Slice contour: the job for the old orbital finished and was thrown away (the contour held is exactly what it was: a result for another orbital is never built with this orbital's level), and a job for the new one has started",
                  sc.jobInFlight() && !sc.shows(orb2, false) && fake.planeCalls == 3 && sc.shows(orb, false) && sameSegs(sc.segments(), outlineSlice(orb, false, 0.3)));
        sc.update(O2, orb2, false, 0.2, true); sc.update(O2, orb2, false, 0.2, true);
        checkTrue("Slice contour: and then the new orbital's contour is there, equal to outlineSlice's", sc.shows(orb2, false) && sameSegs(sc.segments(), outlineSlice(orb2, false, 0.2)));
        sc.update(O2, orb2, false, 0.7, true);
        const int cancels = fake.cancels;
        sc.update(O2, orb2, false, 0.7, false);
        checkTrue("Slice contour: switching the GPU outline off cancels the job and makes the contour on the CPU at once", fake.cancels == cancels + 1 && !sc.jobInFlight() && sameSegs(sc.segments(), outlineSlice(orb2, false, 0.7)));
        OcFake bad; bad.fail = true; setOutlineCompute(&bad);
        SliceContour sf;
        sf.update(O, orb, true, 0.4, true); sf.update(O, orb, true, 0.4, true);
        checkTrue("Slice contour: a job that fails is replaced by the CPU's contour", sf.shows(orb, true) && sameSegs(sf.segments(), outlineSlice(orb, true, 0.4)));
        OcFake ns; ns.startFails = true; setOutlineCompute(&ns);
        SliceContour sn;
        sn.update(O, orb, true, 0.4, true);
        checkTrue("Slice contour: a job that cannot start: the CPU's contour at once", sn.shows(orb, true) && !sn.jobInFlight());
        setOutlineCompute(nullptr);
        clearOutlineCache();
    }
    // 4. the radial graph's 90 % mark does not make the panel wait: pending while the GPU works, filled in when it is done
    {
        const int orb = orbIndex("3d|z2");
        clearOutlineCache();
        const Outline cpuO = outlineOf(orb);
        const std::vector<float> g = outlineGridCpu(orb);
        const Orbital& o = orbitals()[orb];
        const double h = 2 * 1.15 * kExt[o.n] / kOutlineGrid;
        clearOutlineCache();
        OcFake fake; fake.grid = g; fake.level = outlineLevelCpu(g, h * h * h, orbitalNorm(orb)); setOutlineCompute(&fake);
        RadialGraph rg = radialGraphNoWait(orb, 3, true);
        checkTrue("radial graph: with the GPU making the outline the graph comes at once, its 90 % mark is pending (0) and the rest of it is there", rg.reach90Pending && rg.reach90 == 0 && rg.peak > 0 && !rg.P.empty() && !rg.maxima.empty());
        bool done = false;
        for (int i = 0; i < 10 && !done; i++) done = completeReach90(rg, orb, 3, true);
        checkTrue("radial graph: when the outline is ready the mark is the outline's reach / Z, as the waiting version gives", done && !rg.reach90Pending && rg.reach90 == cpuO.rmax / 3 && rg.reach90 == radialGraph(orb, 3).reach90);
        setOutlineCompute(nullptr); clearOutlineCache();
        const RadialGraph r2 = radialGraphNoWait(orb, 3, true);
        checkTrue("radial graph: with no compute object it is the waiting version, not pending", !r2.reach90Pending && r2.reach90 == cpuO.rmax / 3);
        clearOutlineCache();
    }
}
