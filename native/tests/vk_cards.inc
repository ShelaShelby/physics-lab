// 0.61.0 (Vulkan step 3 of 5): the card programs on Vulkan (src/render/vk_card.h), on Mesa's software Vulkan (lavapipe, no window), each against the
// processor's reference with the limits of the picture checks above.
// output (their feature is gone); every check against the processor stays as it was. The validation layers are on whenever they are installed: every
// message of severity error fails. (Until 0.68.1 tests/gl_vulkan.inc.)
#include "atom/atom_card.h"
#include "atom/stark.h"
#include "molecule/mol_cloud.h"
#include "molecule/molecule_card_paths.h"
#include "render/atom_card.h"
#include "render/molecule_card.h"
#include "render/vk_card.h"
#include "render/vk_blackhole.h"
#include "blackhole/sky_card.h"
#include "blackhole/spin_card.h"

static void vkGroups(render::IGroupCompute& vk) {
    // the superpositions of the benchmark's scenes 13 to 15, as gpuSuperScenes: against the processor (the same tolerances)
    struct Sc { int which; const char* name; };
    const Sc list[] = {{10, "scene 13 2p z + 2p x"}, {11, "scene 14 2p x + 2p m+1 (overlap)"}, {12, "scene 15 2p z + 3d z2 (beats)"}};
    for (const Sc& sc : list) {
        AtomLab L;
        benchSetup(L, sc.which, 1040, 713);
        GroupDiff total;
        for (const double beat : {0.0, 1.0, 2.0, 0.37, 3.1}) {
            L.s.beatT = beat;
            const double theta = L.beatTheta();
            addDiff(total, compareSuper(L, vk, theta, std::string("Vulkan ") + sc.name));
        }
        std::printf("Vulkan %-27s %ld dot-frames: %ld shown/hidden and %ld bin differences from the processor (worst %.3g of its tolerance)\n",
                    sc.name, total.dots, total.shownDiff, total.binDiff, total.worstRatio);
        check(std::string("Vulkan ") + sc.name + ": every dot's group equals the processor's, except dots within float rounding of a threshold", total.bad == 0 && total.dots > 0);
        check(std::string("Vulkan ") + sc.name + ": differing dots are rare (under 0.05 % of the dot-frames)", double(total.shownDiff + total.binDiff) < 5e-4 * double(total.dots));
    }
    // a cloud jump (scenes 11 and 12), Sign and Wheel, as gpuJumpScenes
    for (const int which : {8, 9}) {
        for (const char* phase : {"Sign", "Wheel"}) {
            AtomLab L;
            benchSetup(L, which, 1040, 713);
            L.p.phase = phase;
            const std::string name = std::string("Vulkan scene ") + (which == 8 ? "11 jump held" : "12 jumps") + ", phase " + phase;
            for (int f = 0; f < 700 && !(L.s.trans && L.jumpDrawn()); f++) { benchFrame(L, which, f, f - 200); L.tick(1.0 / 240); L.step(1.0 / 240); }
            if (!(L.s.trans && L.jumpDrawn())) { check(name + ": a jump is in flight and built", false); continue; }
            GroupDiff total;
            for (const double t : {0.0, 0.3, 0.6, 0.9, 1.15}) {
                L.s.trans->t = t;
                addDiff(total, compareJump(L, vk, name + " at jump time " + std::to_string(t).substr(0, 4)));
            }
            std::printf("%-40s %ld dot-frames: %ld shown/hidden and %ld colour differences from the processor\n", name.c_str(), total.dots, total.shownDiff, total.binDiff);
            check(name + ": every dot's group equals the processor's, except dots within float rounding of a threshold", total.bad == 0 && total.dots > 0);
            check(name + ": differing dots are rare (under 0.05 % of the dot-frames)", double(total.shownDiff + total.binDiff) < 5e-4 * double(total.dots));
        }
    }
}

static void vkOutline(render::IOutlineCompute& vk) {
    const int orbs[] = {orbIndex("4f|z3"), orbIndex("2p|z"), orbIndex("3d|z2"), complexState(orbIndex("4f"), 3), orbIndex("3s"), orbIndex("1s"), orbIndex("2p|x")};
    for (const int orb : orbs) {
        const std::string tag = "Vulkan outline " + stateText(orb) + ": ";
        const Orbital& o = orbitals()[stateOrbital(orb)];
        const double half = 1.15 * kExt[o.n], h = 2 * half / kOutlineGrid, dV = h * h * h, norm = orbitalNorm(orb), scale = dV / norm;
        const std::vector<float> cpuGrid = outlineGridCpu(orb);
        const float cpuLevel = outlineLevelCpu(cpuGrid, dV, norm);
        std::vector<float> g; float lv = 0;
        const int job = vk.beginGrid(outlineDensityOf(orb), kOutlineGrid, float(half), float(scale), float(kOutlineProb));
        const bool done = job > 0 && vkt::poll([&] { return vk.pollGrid(job, g, lv); }) == render::JobState::Done && g.size() == cpuGrid.size();
        check(tag + "the grid job starts and finishes", done);
        if (!done) continue;
        const ValDiff d = compareValues(g, cpuGrid);
        double above = 0; float next = 3.4e38f;
        for (float v : g) { if (v >= lv) above += double(v) * scale; if (v > lv && v < next) next = v; }
        double aboveNext = 0; for (float v : g) if (v >= next) aboveNext += double(v) * scale;
        check(tag + "the density grid equals the processor's within float rounding (2e-4 relative above 0.1 % of the maximum, 2e-6 of the maximum elsewhere)", d.over == 0);
        check(tag + "its 90 % level encloses 0.9 on its own grid and the next higher cell value less", above >= 0.9 - 1e-6 && aboveNext < 0.9 + 1e-6);
        check(tag + "its 90 % level agrees with the processor's to 1e-4 relative", std::fabs(double(lv) / double(cpuLevel) - 1) < 1e-4);
        // the Slice's plane of the same state, against the processor's
        const std::vector<float> cpuPlane = outlinePlaneCpu(orb, half, true, 0.37 * half, 200);
        std::vector<float> pv;
        const int pj = vk.beginPlane(outlineDensityOf(orb), 200, float(half), true, float(0.37 * half));
        const bool pdone = pj > 0 && vkt::poll([&] { return vk.pollPlane(pj, pv); }) == render::JobState::Done && pv.size() == cpuPlane.size();
        check(tag + "the plane job finishes and equals the processor's plane within float rounding", pdone && compareValues(pv, cpuPlane).over == 0);
        std::printf("Vulkan outline %-12s against the processor: worst relative %.2e, level ratio %.8f\n", stateText(orb).c_str(), d.rel, double(lv) / double(cpuLevel));
    }
}

static void vkRocks(render::IRockCompute& vk) {
    const std::int64_t epoch = 1767268800000LL;                                  // 2026-01-01 12:00 UTC, as gpuRocks
    for (const int scale : {1, 8}) {
        orbit::SolarSystem sys;
        sys.loadAt(epoch);
        if (scale != 1) sys.belts = orbit::makeBelts(sys.epochMs, scale);
        orbit::BeltLayer layer;
        const std::string tag = std::string("Vulkan rocks, scene ") + (scale == 1 ? "1" : "6") + " (" + std::to_string(scale * 6000) + " rocks)";
        double worstNear = 0, worstFar = 0;
        long rocks = 0;
        for (const double years : {0.0, 0.25, 1.0, 100.0, 10000.0, 1e5, 1e6, 1e7}) {
            sys.t = years * units::kYear;
            for (const orbit::BeltCamera& cam : {rockCamera(sys, 0.0, true, 0, 0, 0), rockCamera(sys, 1.5, false, 140, -90, 0), rockCamera(sys, 1.0, true, -260, 75, 0.02)}) {
                render::RockFrame f;                                             // (as orbit/belt_layer.cpp makes it)
                f.t = sys.t;
                const double lp = sys.t / units::kYear / 150 * 6.2832;
                f.libPhase = lp - 2 * units::kPi * std::floor(lp / (2 * units::kPi));
                orbit::rockFrameCamera(cam, f);
                for (const orbit::BeltGroup& g : sys.belts) {
                    const render::RockSet set = layer.gpuSet(g);
                    const int slot = vk.slotFor(set);
                    std::vector<float> xy;
                    if (slot < 0 || !vk.dispatch(slot, f) || !vk.readPositions(slot, xy) || int(xy.size()) != 2 * g.n) { check(tag + ": the places of " + g.key + " can be made and read back", false); continue; }
                    for (int k = 0; k < g.n; k++) {
                        double X, Y;
                        orbit::beltScreen(cam, orbit::beltPoint(g, k, sys.t), X, Y);
                        const double vx = xy[std::size_t(2 * k)], vy = xy[std::size_t(2 * k + 1)], err = std::hypot(vx - X, vy - Y), mag = std::max(std::fabs(X), std::fabs(Y));
                        if (mag < 3000) worstNear = std::max(worstNear, err);
                        else worstFar = std::max(worstFar, err / mag);
                        rocks++;
                    }
                }
            }
        }
        vk.keepOnly({});
        std::printf("%s: %ld rock places; against the processor worst %.3g points near the picture, %.3g of the coordinate farther out\n", tag.c_str(), rocks, worstNear, worstFar);
        check(tag + ": every rock within " + std::to_string(kRockPx) + " points of the processor's place near the picture, 3e-4 of its coordinate farther out", rocks > 0 && worstNear <= kRockPx && worstFar <= 3e-4);
    }
}

static void vkSliceJump(render::AtomCard& vk) {
    std::string why;
    check("Vulkan: the Slice's and the jump's programs are made", vk.build(render::AtomCard::kSlice, nullptr, why) && vk.build(render::AtomCard::kJump, nullptr, why));
    long tries = 0;
    bool allOk = true;
    for (const atom::SliceCheckCase& k : atom::sliceCheckCases()) {
        const atom::PlaneSampler ps = atom::planeSampler(k.id, k.Z, k.axis, k.off);
        const render::SliceDotsJob job = atom::sliceJobOf(ps, k.seed, k.tries, k.stride, k.first);
        std::vector<float> res;
        const int j = vk.beginSlice(job);
        if (!(j > 0 && vkt::poll([&] { return vk.pollSlice(j, res); }) == render::JobState::Done)) { allOk = false; continue; }
        const atom::SliceCompare c = atom::compareSlice(ps, k.seed, res, k.tries, k.stride, k.first);
        std::printf("Vulkan Slice Z=%d id=%d axis=%d off=%.4f: %s\n", k.Z, k.id, k.axis, k.off, atom::sliceCompareText(c).c_str());
        allOk = allOk && c.ok();
        tries += k.tries;
    }
    check("Vulkan Slice: every case within the start-up check's limits against the processor (atom/atom_card.h compareSlice)", allOk && tries > 0);
    // 0.91.0: the Slice of a Dirac state (its own program): the same numbers as tryDiracOnPlane, each point within its derived limit (atom_card.h compareDirac)
    check("Vulkan: the Slice's Dirac program is made", vk.build(render::AtomCard::kDirac, nullptr, why));
    bool dOk = true;
    long dTries = 0;
    for (const atom::DiracCase& k : atom::diracCheckCases()) {
        std::vector<float> res;
        const int j = vk.beginDirac(atom::diracJobOf(k));
        if (!(j > 0 && vkt::poll([&] { return vk.pollSlice(j, res); }) == render::JobState::Done)) { dOk = false; continue; }
        const atom::SliceCompare c = atom::compareDirac(k, res);
        std::printf("Vulkan Dirac Slice Z=%d %d%c j2=%d %s axis=%d off=%.4f: %s\n", k.Z, k.n, "spdf"[k.l], k.j2, k.round ? "round" : "lined up", k.axis, k.off, atom::sliceCompareText(c).c_str());
        dOk = dOk && c.ok();
        dTries += k.tries;
    }
    check("Vulkan Dirac Slice: every case within its limits against tryDiracOnPlane with the same numbers (atom/atom_card.h compareDirac)", dOk && dTries > 0);
    // the same statistic the processor's radii pass: the card's radii on the plane follow D(r) / r from |off| on (Kolmogorov-Smirnov, 99 %: 1.63 / sqrt N),
    // F(r) = (pc(r) - pc(|off|)) / (1 - pc(|off|)), for a round state and a j = 1/2 state (no keep test: every attempt is a dot); and every dot on its plane
    for (const atom::DiracCase base : {atom::diracCheckCases()[4], atom::diracCheckCases()[5]}) {
        atom::DiracCase k = base; k.tries = 20000; k.seed = 0x2468ACE1u;
        std::vector<float> res;
        const int j = vk.beginDirac(atom::diracJobOf(k));
        if (!(j > 0 && vkt::poll([&] { return vk.pollSlice(j, res); }) == render::JobState::Done)) { check("Vulkan Dirac Slice statistics: the batch ran", false); continue; }
        const int kappa = atom::diracKappa(k.l, k.j2);
        const double a = std::fabs(k.off), c0 = atom::diracPlaneCdf(k.Z, k.n, kappa, a);
        std::vector<double> rad;
        long onPlane = 0;
        for (int t = 0; t < k.tries; t++) {
            const float* g = &res[std::size_t(4 * t)];
            if (!(g[3] > 0.5f)) continue;
            rad.push_back(std::sqrt(double(g[0]) * g[0] + double(g[1]) * g[1] + double(g[2]) * g[2]));
            onPlane += (k.axis == 2 ? g[2] : g[1]) == float(k.off);
        }
        std::sort(rad.begin(), rad.end());
        double d = 0;
        for (std::size_t i = 0; i < rad.size(); i++) {
            const double F = (atom::diracPlaneCdf(k.Z, k.n, kappa, rad[i]) - c0) / (1 - c0);
            d = std::max({d, std::fabs(F - double(i) / double(rad.size())), std::fabs(F - double(i + 1) / double(rad.size()))});
        }
        const double lim = 1.63 / std::sqrt(double(std::max<std::size_t>(rad.size(), 1)));
        std::printf("Vulkan Dirac Slice Z=%d %d%c j2=%d: %zu dots, KS distance %.5f (99 %% bound %.5f), %ld on the plane\n", k.Z, k.n, "spdf"[k.l], k.j2, rad.size(), d, lim, onPlane);
        check("Vulkan Dirac Slice: the card's radii follow the plane's distribution (Kolmogorov-Smirnov) and every dot lies on its plane", rad.size() == std::size_t(k.tries) && d < lim && onPlane == long(rad.size()));
    }
    // 0.93.0: a parabolic state's cloud (its own program): the same numbers as stark::samplePar, the part exactly and each point within its derived limit
    check("Vulkan: the Stark cloud's program is made", vk.build(render::AtomCard::kStark, nullptr, why));
    bool sOk = true;
    long sTries = 0;
    for (const atom::StarkCase& k : atom::starkCheckCases()) {
        std::vector<float> res;
        const int j = vk.beginStark(atom::starkJobOf(k));
        if (!(j > 0 && vkt::poll([&] { return vk.pollSlice(j, res); }) == render::JobState::Done)) { sOk = false; continue; }
        const atom::SliceCompare c = atom::compareStark(k, res);
        std::printf("Vulkan Stark cloud n=%d (%d, %d, %d): %s\n", k.n, k.n1, k.n2, k.m, atom::sliceCompareText(c).c_str());
        sOk = sOk && c.ok();
        sTries += k.tries;
    }
    check("Vulkan Stark cloud: every point within its limit against stark::samplePar with the same numbers (atom/atom_card.h compareStark)", sOk && sTries > 0);
    // the card's own statistic: <z> of 100,000 of its points against the exact <z> (atom/stark.h meanZ) within 4 standard errors (the spread from the same points)
    for (const atom::StarkCase base : atom::starkCheckCases()) {
        atom::StarkCase k = base; k.tries = 100000; k.seed = 0x13579BDFu;
        std::vector<float> res;
        const int j = vk.beginStark(atom::starkJobOf(k));
        if (!(j > 0 && vkt::poll([&] { return vk.pollSlice(j, res); }) == render::JobState::Done)) { check("Vulkan Stark cloud statistics: the batch ran", false); continue; }
        double sz = 0, szz = 0;
        for (int t = 0; t < k.tries; t++) { const double z = res[std::size_t(4 * t + 2)]; sz += z; szz += z * z; }
        const double N = k.tries, mz = sz / N, se = std::sqrt(std::max(0.0, szz / N - mz * mz) / N);
        const double want = atom::stark::meanZ(atom::stark::parSampler(atom::stark::Par{k.n, k.n1, k.n2, k.m}));
        std::printf("Vulkan Stark cloud n=%d (%d, %d, %d): <z> %.5f, exact %.5f, standard error %.5f\n", k.n, k.n1, k.n2, k.m, mz, want, se);
        check("Vulkan Stark cloud: the card's <z> equals the exact <z> within 4 standard errors", std::fabs(mz - want) <= 4 * se);
    }
    long wrong = 0, dots = 0;
    for (const atom::JumpCheckCase& k : atom::jumpCheckCases()) {
        std::vector<unsigned char> fixedG, want, got;
        atom::jumpCheckFixed(k.jn, fixedG);
        atom::jumpCheckExpected(k.jn, k.shareNew, fixedG, want);
        const std::uint64_t t0 = render::jumpShareThreshold(1 - k.shareNew), t1 = render::jumpShareThreshold(k.shareNew);
        const int j = vk.beginJumpRead(fixedG.data(), k.jn, t0, t1);
        if (!(j > 0 && vkt::poll([&] { return vk.pollJumpRead(j, got); }) == render::JobState::Done) || got.size() != want.size()) { wrong += long(want.size()); continue; }
        for (std::size_t i = 0; i < want.size(); i++) wrong += got[i] != want[i];
        dots += long(want.size());
    }
    std::printf("Vulkan jump groups: %ld dots, %ld differ from the processor's\n", dots, wrong);
    check("Vulkan jump groups: every group exactly the processor's", dots > 0 && wrong == 0);
}

static void vkMolCloud(render::MoleculeCard& vk) {
    std::string why;
    check("Vulkan: the Density cloud's programs are made", vk.build(nullptr, why));
    std::vector<float> box;
    molecule::cloudCheckDots(4096, box);
    long dots = 0;
    bool allOk = true;
    for (const molecule::CloudCam& cam : molecule::cloudCheckCams()) {
        std::vector<float> res;
        const long n = static_cast<long>(box.size() / 3);
        const int j = vk.beginProject(box.data(), n, molecule::cardCam(cam));
        if (!(j > 0 && vkt::poll([&] { return vk.pollProject(j, res); }) == render::JobState::Done)) { allOk = false; continue; }
        const molecule::CloudCompare c = molecule::compareCloud(cam, box.data(), n, res);
        allOk = allOk && c.ok();
        dots += n;
    }
    std::printf("Vulkan Density cloud: %ld dots under %zu cameras\n", dots, molecule::cloudCheckCams().size());
    check("Vulkan Density cloud: every case within the start-up check's limits against the processor (molecule/mol_cloud.h compareCloud)", allOk && dots > 0);
}

// the exposure's grid of the Glow dots (kCloudGridVS + kCloudGridFS) against the processor's sum of the same footprints in double: per cell within the
// float sum's rounding (2^-24 for each footprint added, 6 operations for each footprint's own value)
static void settle(render::VkCards& cards, render::IGroupCompute* g, render::IOutlineCompute* o, render::IRockCompute* r, render::VkGlow* w);
static void vkGlowGrid(render::VkCards& cards) {
    std::string why;
    render::VkGlow* g = render::makeVkGlow(cards, why);
    check("Vulkan: the Glow dots' grid program is made (" + why + ")", g != nullptr);
    if (!g) return;
    std::vector<float> pts;
    std::vector<unsigned char> grp;
    std::uint64_t z = 12345;
    const auto rnd = [&] { z = z * 6364136223846793005ULL + 1442695040888963407ULL; return double(z >> 11) / 9007199254740992.0; };
    for (int i = 0; i < 50000; i++) { pts.push_back(float(2 * rnd() - 1)); pts.push_back(float(2 * rnd() - 1)); pts.push_back(float(2 * rnd() - 1)); grp.push_back(rnd() < 0.2 ? 255 : 3); }
    render::Renderer2D::CloudGpuDraw d;
    d.pts = pts.data(); d.cnt = 50000; d.cap = 50000; d.epoch = 7; d.k = 50000; d.grp = grp.data();
    d.cx = 520; d.cy = 356; d.pxm = 300; d.ext = 1.2f; d.ca = std::cos(0.6f); d.sa = std::sin(0.6f); d.cb = std::cos(1.0f); d.sb = std::sin(1.0f); d.w = 1040; d.h = 713;
    long bytes = 0;
    const int index = render::vkGlowRecord(g, d, VK_NULL_HANDLE, bytes);
    const bool ran = index >= 0 && cards.flush(true) != 0 && !cards.failed();
    check("Vulkan Glow grid: the run is made and read back", ran);
    settle(cards, nullptr, nullptr, nullptr, g);
    if (!ran) { render::dropVkGlow(g); return; }
    // (0.65.0: three frames, the second with 300 dots of the ring replaced, their groups changed (atom/cloud_dots.h keepGroups: only the new dots'
    // groups are sent while the groups' epoch stays), the third with nothing new: a group sent wrong or not sent changes the grid)
    const auto compare = [&](int index, const char* label) {
        const int gw = int(d.w / 8) + 2, gh = int(d.h / 8) + 2;
        std::vector<double> want(std::size_t(gw * gh), 0.0);
        std::vector<long> count(want.size(), 0);
        std::vector<double> slack(want.size(), 0.0);
        for (long i = 0; i < d.cnt; i++) {                                          // kCloudGridVS's formulas, in double
            if (grp[std::size_t(i)] == 255) continue;
            const double x = pts[std::size_t(3 * i)], y = pts[std::size_t(3 * i + 1)], zz = pts[std::size_t(3 * i + 2)];
            const double x1 = x * d.ca - y * d.sa, y1 = x * d.sa + y * d.ca, dep = y1 * d.cb - zz * d.sb, up = y1 * d.sb + zz * d.cb;
            const double f = 1 / (1 + dep / (4 * double(d.ext))), X = d.cx + x1 * d.pxm * f, Y = d.cy - up * d.pxm * f, D = dep / d.ext;
            if (X < 0 || X > d.w || Y < 0 || Y > d.h) continue;
            const double t = std::max(-1.0, std::min(1.0, D)), nr = 0.5 - 0.5 * t, r = 4.6 * (0.8 + 0.4 * nr), wt = 0.3 + 0.9 * nr;
            const int cx = std::max(0, std::min(gw - 1, int(X / 8))), cy = std::max(0, std::min(gh - 1, int(Y / 8)));
            want[std::size_t(cy * gw + cx)] += wt * r * r * 0.785;
            count[std::size_t(cy * gw + cx)]++;
            // a dot within float rounding of a cell's edge may land in the next cell on the card: its footprint is slack for the cells round it
            const double fx = X / 8 - std::floor(X / 8), fy = Y / 8 - std::floor(Y / 8);
            if (fx < 1e-4 || fx > 1 - 1e-4 || fy < 1e-4 || fy > 1 - 1e-4)
                for (int ax = -1; ax <= 1; ax++) for (int ay = -1; ay <= 1; ay++) {
                    const int qx = cx + ax, qy = cy + ay;
                    if (qx >= 0 && qx < gw && qy >= 0 && qy < gh) slack[std::size_t(qy * gw + qx)] += wt * r * r * 0.785;
                }
        }
        const float* cells = cards.cells(index);
        if (!cells) { check(label + std::string(": the grid is read back"), false); return; }
        long bad = 0; double worst = 0;
        for (std::size_t c = 0; c < want.size(); c++) {
            const double lim = want[c] * (double(count[c]) + 6) * 5.96e-8 * 4 + 1e-6 + slack[c];
            const double e = std::fabs(cells[c] - want[c]);
            worst = std::max(worst, want[c] > 0 ? e / want[c] : e);
            if (e > lim) bad++;
        }
        std::printf("%s: %d cells, %ld beyond the float sum's rounding (the worst relative difference %.2e)\n", label, gw * gh, bad, worst);
        check(label + std::string(": every cell equals the processor's sum within the float sum's rounding"), bad == 0);
    };
    compare(index, "Vulkan Glow grid");
    d.grpEpoch = 11;
    const int i1 = render::vkGlowRecord(g, d, VK_NULL_HANDLE, bytes);   // (the same frame again, its groups now with an epoch: sent whole)
    check("Vulkan Glow grid, groups with an epoch: the run is made", i1 >= 0 && cards.flush(true) != 0 && !cards.failed());
    if (i1 >= 0) compare(i1, "Vulkan Glow grid, groups with an epoch");
    for (int i = 0; i < 300; i++) {                                     // dots 0 .. 299 replaced (the ring: k 50000 .. 50299)
        for (int c = 0; c < 3; c++) pts[std::size_t(3 * i + c)] = float(2 * rnd() - 1);
        grp[std::size_t(i)] = grp[std::size_t(i)] == 255 ? 3 : 255;
    }
    d.k = 50300;
    const int i2 = render::vkGlowRecord(g, d, VK_NULL_HANDLE, bytes);
    check("Vulkan Glow grid, 300 new dots (only their groups sent): the run is made", i2 >= 0 && cards.flush(true) != 0 && !cards.failed());
    if (i2 >= 0) compare(i2, "Vulkan Glow grid, 300 new dots (only their groups sent)");
    const int i3 = render::vkGlowRecord(g, d, VK_NULL_HANDLE, bytes);
    check("Vulkan Glow grid, nothing new (no groups sent): the run is made", i3 >= 0 && cards.flush(true) != 0 && !cards.failed());
    if (i3 >= 0) compare(i3, "Vulkan Glow grid, nothing new (no groups sent)");
    render::dropVkGlow(g);
}

// 0.62.0 (Vulkan step 4): the Black Hole Lab's programs on Vulkan. The light paths (kKerrTableCS, double precision): the start-up check against the processor
// with its own limits (spin_card.h cardSelfCheck). (the app's PL_BH_COMPARE=card, check_blackhole.py
// precision, compares the card's whole fine tables with the processor's.) The sky per pixel: its three stages made, the start-up check's two probes and
// its light check (sky_card.h skyCheckOf).
static void vkBlackHole(render::VkCards& cards, bool f64) {
    std::string why;
    if (f64) {
        render::IKerrCompute* K = render::makeVkKerrCompute(cards, VK_NULL_HANDLE, nullptr, why);
        check("Vulkan light paths: the program starts (" + why + ")", K != nullptr);
        if (K) {
            while (K->building()) std::this_thread::sleep_for(std::chrono::milliseconds(20));
            std::string w;
            const int st = render::vkKerrState(K, w);
            check("Vulkan light paths: the pipeline is made (" + w + ")", st == 2);
            if (st == 2) {
                const blackhole::CardCheck c = blackhole::cardSelfCheck(*K);
                std::printf("Vulkan light paths against the processor: %s; math %.1e, edges %d (%.1e), rays %d: dir %.1e b %.1e u %.1e, ends differ %d (%s)\n", c.ok ? "passed" : "FAILED",
                            c.math, c.edges, c.edge, c.rays, c.dir, c.b, c.u, c.endsDiffer, c.why.empty() ? "-" : c.why.c_str());
                check("Vulkan light paths: the start-up check against the processor passes (spin_card.h's limits)", c.ok);
            }
            delete K;
        }
    }
    render::VkBlackHole bh;
    check("Vulkan Black Hole Lab: its programs start (" + why + ")", bh.init(cards, "", why));
    int st = 0;
    for (int stage = 0; stage < 3; stage++) {
        std::string w;
        while ((st = bh.buildSky(stage, nullptr, w)) == 0) std::this_thread::sleep_for(std::chrono::milliseconds(20));
        check(std::string("Vulkan sky: ") + render::VkBlackHole::progName(stage == 0 ? 2 : stage == 1 ? 3 : 4) + " made (" + w + ")", st == 1);
    }
    std::printf("Vulkan Black Hole pipelines: %s\n", bh.pipelineText().c_str());
    if (st == 1) {
        std::vector<float> p0, p1;
        bh.probe(blackhole::skyProbeLens(blackhole::skyCheckCase(0)), blackhole::kSkyCheckW, blackhole::kSkyCheckH, false, &p0);
        bh.probe(blackhole::skyProbeLens(blackhole::skyCheckCase(1)), blackhole::kSkyCheckW, blackhole::kSkyCheckH, false, &p1);
        const std::vector<float>* p[2] = {&p0, &p1};
        const blackhole::SkyCheck c = blackhole::skyCheckOf(p);
        for (int k = 0; k < 2; k++) std::printf("Vulkan sky probe %s\n", blackhole::skyCompareText(blackhole::skyCheckCase(k), c.m[k]).c_str());
        std::printf("Vulkan sky light check: eye %ld pixels, %ld over, worst %.3f of its limit; all light %ld, %ld over, worst %.3f\n", c.light[0].px, c.light[0].bad, c.light[0].worst,
                    c.light[1].px, c.light[1].bad, c.light[1].worst);
        check("Vulkan sky: the start-up check (both probes and the light check) passes (sky_card.h's limits; " + c.why + ")", c.ok);
    }
    bh.destroy();
}

// 0.64.0: Saturn's rings per pixel (kRingsPart through its probe kRingsProbeCS) against the processor's triangles: the app's start-up check, its three
// views and limits (orbit/ring_check.h: at most 1 % of the ring pixels over 8 levels in a view)
static void vkRings(render::RingCard& vk) {
    std::string why;
    check("Vulkan rings: the probe's program is made (" + why + ")", vk.build(why));
    constexpr int w = 400, h = 250;
    std::vector<float> xy;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) { xy.push_back(x + 0.5f); xy.push_back(y + 0.5f); }
    for (const orbit::RingCheckView& v : orbit::ringCheckViews(w, h)) {
        std::vector<float> back, front, cpu, card;
        const int jb = vk.beginProbe(orbit::ringsDraw(v.f, v.bands, true, w, h), xy), jf = vk.beginProbe(orbit::ringsDraw(v.f, v.bands, false, w, h), xy);
        const bool done = jb > 0 && jf > 0 && vkt::poll([&] { return vk.pollProbe(jb, back); }) == render::JobState::Done && vkt::poll([&] { return vk.pollProbe(jf, front); }) == render::JobState::Done;
        orbit::ringPictureCpu(v.f, v.bands, w, h, cpu);
        orbit::ringPictureCard(back, front, w, h, card);
        const orbit::RingCompare c = orbit::compareRingPictures(cpu, card, w, h);
        std::printf("Vulkan rings, %s: %ld of %ld ring pixels over 8 levels, the largest %d\n", v.name, c.over8, c.ringPixels, c.worst);
        check(std::string("Vulkan rings, ") + v.name + ": at most 1 % of the ring pixels over 8 levels from the processor's triangles", done && c.ok() && c.ringPixels > 1000);
    }
}

// 0.64.0: the Classic Cloud 3D dots (kClassicCS) against atom/cloud_dots.cpp projectCloud + buildClassic3D: the app's start-up check (atom/classic_check.h)
static void vkClassic(render::ClassicCard& vk) {
    std::string why;
    check("Vulkan Classic dots: the program is made (" + why + ")", vk.build(why));
    // 0.65.0: and the side view with every electron: its Classic squares, and the Glow's soft dots with their cells (the exposure's grid)
    // and the Slice's dots (Classic, a vertical cut; Glow, a horizontal one)
    const char* names[5] = {"Cloud 3D", "the side view, Classic", "the side view, Glow", "the Slice, Classic", "the Slice, Glow"};
    for (int k = 0; k < 5; k++) {
        const atom::ClassicCase c = k == 0 ? atom::classicCheckCase() : k < 3 ? atom::classicSideCase(k == 2) : atom::classicSliceCase(k == 4, k == 4);
        const int j = vk.beginCheck(atom::classicCaseDraw(c));
        std::vector<std::uint32_t> keys;
        std::vector<float> sq;
        long total = 0;
        const bool done = j > 0 && vkt::poll([&] { return vk.pollCheck(j, keys, sq, total); }) == render::JobState::Done;
        const atom::ClassicCompare cmp = atom::compareClassic(c, keys, sq, total, vk.checkCells());
        std::printf("Vulkan Classic dots, %s: %s\n", names[k], atom::classicCompareText(cmp).c_str());
        check(std::string("Vulkan Classic dots, ") + names[k] + ": every dot's group, band and place, the squares in the processor's order (and the cells), within the start-up check's limits",
              done && cmp.ok() && (k != 2 && k != 4 ? true : cmp.cells > 0));
    }
}

// 0.67.0: the nucleus' Density look (kNucDensCS) against atom_core.cpp projectChargeDensity: the app's start-up check (atom/nucdens_check.h)
static void vkNucDens(render::NucDensCard& vk) {
    std::string why;
    check("Vulkan nucleus density: the program is made (" + why + ")", vk.build(why));
    const float col[4] = {1, 1, 1, 1};
    for (const atom::NucDensCase& c : atom::nucDensCheckCases()) {
        std::vector<float> table, grid, cpu;
        const int j = vk.beginCheck(atom::nucDensDraw(c.d, c.e1, c.e2, c.e3, c.G, c.cellFm, table, 1, 0, 0, 1, col));
        const bool done = j > 0 && vkt::poll([&] { return vk.pollCheck(j, grid); }) == render::JobState::Done;
        atom::projectChargeDensity(c.d, c.e1, c.e2, c.e3, c.G, c.cellFm, cpu);
        const atom::NucDensCompare k = atom::compareNucDens(cpu, grid);
        std::printf("Vulkan nucleus density, %s: %ld of %ld cells over %.0e of the brightest, the largest %.2e\n", c.name, k.over, k.cells, atom::kNucDensLimit, k.worst);
        check(std::string("Vulkan nucleus density, ") + c.name + ": every cell within the start-up check's limit of the processor's", done && k.ok());
    }
}

// 0.68.0: the Orbit Lab's lines (kLinesCS) against the processor's LinePen (orbit/orbit_lines.h): the app's start-up check (orbit/lines_check.h)
static void vkLines(render::LinesCard& vk) {
    std::string why;
    check("Vulkan lines: the program is made (" + why + ")", vk.build(why));
    for (const orbit::LinesCase& c : orbit::linesCheckCases()) {
        std::vector<std::vector<double>> pens;
        orbit::linesCpu(c, pens);
        long total = 0;
        for (const auto& p : pens) total += long(p.size() / 5);
        render::LinesDraw d = orbit::linesDrawOf(c);
        d.checkRoom = int(2 * total + 4096);
        std::vector<float> ends, pts;
        std::vector<std::uint32_t> offs;
        const int j = vk.beginCheck(d);
        const bool done = j > 0 && vkt::poll([&] { return vk.pollCheck(j, ends, offs, pts); }) == render::JobState::Done;
        const orbit::LinesCompare k = orbit::compareLines(c, d, pens, ends, offs, pts);
        std::printf("Vulkan lines, %s: %s\n", c.name, orbit::linesCompareText(k).c_str());
        check(std::string("Vulkan lines, ") + c.name + ": every run's points, in the processor's number, within the start-up check's limits", done && k.ok());
    }
}

// 0.68.0: the Density surface (kSurfaceCS) against the processor's (molecule/surface_draw.h): the app's start-up check (molecule/surface_check.h)
static void vkSurface(render::SurfaceCard& vk) {
    std::string why;
    check("Vulkan surface: the program is made (" + why + ")", vk.build(why));
    const std::vector<molecule::SurfaceCase> cases = molecule::surfaceCheckCases();
    check("Vulkan surface: the check's three surfaces were made (the density files)", cases.size() == 3);
    for (const molecule::SurfaceCase& c : cases) {
        std::vector<float> verts;
        std::vector<std::uint32_t> order;
        const int j = vk.beginCheck(molecule::surfaceDrawOf(c));
        const bool done = j > 0 && vkt::poll([&] { return vk.pollCheck(j, verts, order); }) == render::JobState::Done;
        const molecule::SurfaceCompare k = molecule::compareSurface(c, verts, order);
        std::printf("Vulkan surface, %s: %s\n", c.name.c_str(), molecule::surfaceCompareText(k).c_str());
        check("Vulkan surface, " + c.name + ": every vertex's place and colour, and the order, within the start-up check's limits", done && k.ok());
    }
}

// 0.90.0: the surface made on the card (kSurfaceMakeCS) against the processor's dens::densitySurface (molecule/surface_check.h compareMade): water at
// four phases of its vibrating mode, carbon dioxide and nitrogen (still: no vibrating density) at four phases each; the same triangles in the same order,
// every vertex within its derived limit
static void vkSurfaceMade(render::SurfaceCard& vk) {
    std::string why;
    check("Vulkan made surface: the program is made (" + why + ")", vk.buildMake(why));
    const std::vector<molecule::MadeCase> cases = molecule::surfaceMadeCases(true);
    check("Vulkan made surface: the 12 cases were made (the density files)", cases.size() == 12);
    for (const molecule::MadeCase& c : cases) {
        render::SurfaceMade got;
        const int j = vk.beginMake(c.make);
        const bool done = j > 0 && vkt::poll([&] { return vk.pollMake(j, got); }) == render::JobState::Done;
        const molecule::MadeCompare k = molecule::compareMade(c, got);
        std::printf("Vulkan made surface, %s: %s\n", c.name.c_str(), molecule::madeCompareText(k).c_str());
        check("Vulkan made surface, " + c.name + ": the processor's triangles in its order, every vertex within its limit", done && k.ok());
    }
}

// 0.65.0: as the app's frame does (renderer2d_vk.cpp): a program's first full use ends once its batch has finished, which gives the guards' turn back
// (0.63.0's one-step-at-a-time rule). Without it the next program's first use waited for the turn for ever here (the outline's, rocks' and Glow grid's steps).
static void settle(render::VkCards& cards, render::IGroupCompute* g, render::IOutlineCompute* o, render::IRockCompute* r, render::VkGlow* w) {
    cards.flush(true);
    const int off = render::vkPollFirstUses(cards, g, o, r, w);
    check("Vulkan: the first full use ends (its guard's turn given back)", off == 0 && app::gpuGuardTurn().empty());
    if (!app::gpuGuardTurn().empty()) std::printf("Vulkan: the guards' turn is still held by %s\n", app::gpuGuardTurn().c_str());
}

// 0.90.0: the Glow core's dots found on the card (kGlowNearCS) against the processor's pass over every dot (atom/cloud_dots.h glowDotsNear): hydrogen,
// sodium and uranium in the Glow Cloud 3D (Ultra, zoom 0), and radium's jump with every electron (the atom.jump_all set-up, its two populations, each dot
// asked whether it is shown). The dots found are the same set, in the same order, with the same places, sizes and weights; the capped exposure (the
// grid's exposure, glowExposure, then glowCapExposure at the core) from the card's grid and dots lies within the range its grid's float sums allow:
// each cell within want (n + 6) 4 u of the processor's (n its dots, u = 2^-24; and a dot within rounding of a cell's edge as slack for the cells round
// it, as vkGlowGrid), the exposure worked out from the grid lowered and raised by that much at every cell
static void vkGlowNearCheck(render::VkCards& cards) {
    std::string why;
    render::VkGlow* g = render::makeVkGlow(cards, why);
    check("Vulkan Glow core: the Glow program and its core pass are made (" + why + ")", g != nullptr && render::vkGlowNearOk(g));
    if (!g || !render::vkGlowNearOk(g)) { if (g) render::dropVkGlow(g); return; }
    const double w = 1040, h = 713;
    for (int sc = 0; sc < 4; sc++) {
        static AtomLab L;
        const int Z = sc == 0 ? 1 : sc == 1 ? 11 : sc == 2 ? 92 : heaviestLevelAtom();
        std::vector<float> both;
        const float* pts = nullptr;
        long cnt = 0, cap = 0, jn = 0;
        std::vector<unsigned char> grp;
        if (sc < 3) {
            benchGlowAtom(L, Z, w, h);
            cap = std::min<long>(L.cloudCap(), long(L.s.sg.size()));
            cnt = std::min<long>(L.s.k, cap);
            pts = L.s.pts.data();
            grp.resize(std::size_t(cnt));
            plainGroups(L.usesWheel(), L.s.ph.data(), L.s.sg.data(), cnt, grp.data());
        } else {
            benchSetup(L, kBenchJumpAll, w, h);
            for (int f = 0; f < 4000 && !(L.jumpDrawn() && L.s.trans); f++) { benchFrame(L, kBenchJumpAll, f, f); L.step(1.0 / 60); }
            benchFrame(L, kBenchJumpAll, 0, 0);
            check("Vulkan Glow core: radium's jump is drawn", L.jumpDrawn());
            if (!L.jumpDrawn()) continue;
            jn = L.jumpDots();
            both.assign(L.s.pts.begin(), L.s.pts.begin() + 3 * jn); both.insert(both.end(), L.s.jp.begin(), L.s.jp.begin() + 3 * jn);
            pts = both.data(); cnt = 2 * jn; cap = cnt;
        }
        const char* name = sc == 0 ? "hydrogen" : sc == 1 ? "sodium" : sc == 2 ? "uranium" : "radium's jump";
        const double m = std::min(w, h), pxm = L.pxPerM(m, L.s.szCur) * std::pow(10, L.p.zoom), ext = L.cloudExtM();
        const CloudCamera cam{w / 2, h / 2, pxm, std::cos(L.s.yaw), std::sin(L.s.yaw), std::cos(L.s.pitch), std::sin(L.s.pitch), ext};
        const unsigned char* gp = sc < 3 ? grp.data() : nullptr;
        // the processor's
        std::vector<GlowDot> cpuNear;
        std::vector<int> cpuIdx;
        glowDotsNear(pts, cnt, gp, cam, cam.cx, cam.cy, cpuNear, &cpuIdx);
        // the card's
        render::Renderer2D::CloudGpuDraw d;
        std::vector<unsigned char> grpAll(std::size_t(cnt), 0);
        d.pts = pts; d.cnt = cnt; d.cap = cap; d.epoch = 100 + std::uint64_t(sc); d.k = cnt; d.grp = gp ? gp : grpAll.data();
        d.cx = float(cam.cx); d.cy = float(cam.cy); d.pxm = float(cam.pxm); d.ext = float(cam.ext); d.ca = float(cam.ca); d.sa = float(cam.sa); d.cb = float(cam.cb); d.sb = float(cam.sb); d.w = float(w); d.h = float(h);
        const GlowNearPass q = glowNearPass(cam, cam.cx, cam.cy);
        const float pass[9] = {q.ca, q.sa, q.cb, q.sb, q.k4, q.pxm, q.dx0, q.dy0, q.lim};
        std::copy(pass, pass + 9, d.nearPass);
        d.near = true;
        long bytes = 0;
        const int index = render::vkGlowRecord(g, d, VK_NULL_HANDLE, bytes);
        const bool ran = index >= 0 && cards.flush(true) != 0 && !cards.failed();
        render::Renderer2D::GlowNearDots kept;
        const bool read = ran && render::vkGlowNear(g, render::vkGlowNearBank(g), kept);
        bool placesSame = kept.xyz.size() == 3 * kept.idx.size();               // (the places the card copied, bit for bit)
        for (std::size_t k = 0; placesSame && k < kept.idx.size(); k++)
            for (int c = 0; c < 3; c++) placesSame = placesSame && std::memcmp(&kept.xyz[3 * k + std::size_t(c)], &pts[3 * std::size_t(kept.idx[k]) + std::size_t(c)], 4) == 0;
        std::vector<GlowDot> gpuNear;
        std::vector<int> gpuIdx;
        glowDotsNearAt(kept.xyz.data(), kept.idx, gp, cam, cam.cx, cam.cy, gpuNear, &gpuIdx);
        bool same = gpuIdx == cpuIdx && gpuNear.size() == cpuNear.size();
        for (std::size_t k = 0; same && k < cpuNear.size(); k++)
            same = gpuNear[k].x == cpuNear[k].x && gpuNear[k].y == cpuNear[k].y && gpuNear[k].r == cpuNear[k].r && gpuNear[k].wt == cpuNear[k].wt && gpuNear[k].g == cpuNear[k].g;
        if (sc == 3) {                                                          // (the jump: each found dot asked whether it is shown, on both sides alike)
            const auto shown = [&](std::vector<GlowDot>& D, const std::vector<int>& I) { std::size_t k2 = 0; for (std::size_t k = 0; k < D.size(); k++) { const int i = I[k], pop = i >= jn ? 1 : 0; if (L.jumpShown(pop, i - pop * jn)) D[k2++] = D[k]; } D.resize(k2); };
            shown(cpuNear, cpuIdx); shown(gpuNear, gpuIdx);
        }
        std::printf("Vulkan Glow core, %s: %ld dots; the card kept %zu, the processor's pass found %zu, the card's list then %zu (%zu shown)\n", name, cnt, kept.idx.size(), cpuIdx.size(), gpuIdx.size(), gpuNear.size());
        check(std::string("Vulkan Glow core, ") + name + ": the card's pass ran and was read back, each kept dot's place copied bit for bit", read && placesSame);
        check(std::string("Vulkan Glow core, ") + name + ": the dots found are the processor's, the same set in the same order (not none)", same && !cpuIdx.empty());
        // the exposure: the grid as the card sums it (kCloudGridVS), in double, with each cell's limit
        const int gw = int(d.w / 8) + 2, gh = int(d.h / 8) + 2;
        std::vector<double> want(std::size_t(gw * gh), 0.0), slack(want.size(), 0.0);
        std::vector<long> count(want.size(), 0);
        for (long i = 0; i < cnt; i++) {
            if (d.grp[i] == 255) continue;
            const double x = pts[3 * i], y = pts[3 * i + 1], zz = pts[3 * i + 2];
            const double x1 = x * d.ca - y * d.sa, y1 = x * d.sa + y * d.ca, dep = y1 * d.cb - zz * d.sb, up = y1 * d.sb + zz * d.cb;
            const double f = 1 / (1 + dep / (4 * double(d.ext))), X = d.cx + x1 * d.pxm * f, Y = d.cy - up * d.pxm * f, D = dep / d.ext;
            if (X < 0 || X > d.w || Y < 0 || Y > d.h) continue;
            const double t = std::max(-1.0, std::min(1.0, D)), nr = 0.5 - 0.5 * t, r = 4.6 * (0.8 + 0.4 * nr), wt = 0.3 + 0.9 * nr;
            const int cx = std::max(0, std::min(gw - 1, int(X / 8))), cy = std::max(0, std::min(gh - 1, int(Y / 8)));
            want[std::size_t(cy * gw + cx)] += wt * r * r * 0.785;
            count[std::size_t(cy * gw + cx)]++;
            const double fx = X / 8 - std::floor(X / 8), fy = Y / 8 - std::floor(Y / 8);
            if (fx < 1e-4 || fx > 1 - 1e-4 || fy < 1e-4 || fy > 1 - 1e-4)
                for (int ax = -1; ax <= 1; ax++) for (int ay = -1; ay <= 1; ay++) {
                    const int qx = cx + ax, qy = cy + ay;
                    if (qx >= 0 && qx < gw && qy >= 0 && qy < gh) slack[std::size_t(qy * gw + qx)] += wt * r * r * 0.785;
                }
        }
        const float* cells = ran ? cards.cells(index) : nullptr;
        if (!cells) { check(std::string("Vulkan Glow core, ") + name + ": the grid is read back", false); continue; }
        std::vector<float> cardGrid(cells, cells + want.size()), lo(want.size()), hi(want.size());
        for (std::size_t c = 0; c < want.size(); c++) {
            // (glowExposure ranks the cells above 0, so an empty cell stays empty unless a dot within rounding of an edge may reach it: its slack; a cell
            // with dots and no slack stays above 0)
            const double lim = want[c] * (double(count[c]) + 6) * 5.96e-8 * 4 + slack[c];
            lo[c] = float(slack[c] > 0 ? std::max(0.0, want[c] - lim) : want[c] - lim); hi[c] = float(want[c] + lim);
            if (want[c] > 0 && slack[c] == 0) lo[c] = std::max(lo[c], std::nextafter(0.0f, 1.0f));
        }
        const std::vector<std::array<double, 2>> core{{cam.cx, cam.cy}};
        const auto capped = [&](const std::vector<float>& grid, const std::vector<GlowDot>& D) { double e = 0; if (!glowExposure(grid, cap, false, 1.0, e)) return -1.0; if (!D.empty()) glowCapExposure(D, cap, 1.0, core, e); return e; };
        const double eCard = capped(cardGrid, gpuNear), eLo = capped(hi, cpuNear), eHi = capped(lo, cpuNear);   // (a brighter grid: a lower exposure)
        const double a = std::min(eLo, eHi) * (1 - 4 * 5.96e-8), b = std::max(eLo, eHi) * (1 + 4 * 5.96e-8);
        std::printf("Vulkan Glow core, %s: the capped exposure from the card %.9g, allowed %.9g to %.9g\n", name, eCard, a, b);
        check(std::string("Vulkan Glow core, ") + name + ": the capped exposure from the card is the processor's within its grid's float rounding", eCard > 0 && eCard >= a && eCard <= b);
    }
    settle(cards, nullptr, nullptr, nullptr, g);
    render::dropVkGlow(g);
}

// 0.90.0 (development): PL_VK_PART=vulkanTwins:<name> runs only that program's checks here (vk_compare.cpp want)
static bool twin(const char* name) {
    const char* e = std::getenv("PL_VK_PART");
    if (!e || !*e) return true;
    const std::string list = std::string(",") + e + ",";
    return list.find(",vulkanTwins,") != std::string::npos || list.find(std::string(",vulkanTwins:") + name + ",") != std::string::npos;
}

static void vulkanTwins() {
    app::gpuGuardFrameThread();   // 0.64.0: this thread runs every program's steps in turn (0.63.0's one-step-at-a-time turn: a guard's open step made the next wait for ever)
    vkt::Ctx h;
    std::string why;
    const bool up = vkt::start(h, why);
    check("Vulkan: a device (Mesa's lavapipe here) with a graphics and compute queue (" + why + ")", up);
    if (!up) { vkt::stop(h); return; }
    {
        render::VkCards cards;
        check("Vulkan: the card programs' engine starts (" + why + ")", cards.init(h.c, nullptr, h.f64, why));
        render::IGroupCompute* g = render::makeVkGroupCompute(cards, why);
        check("Vulkan: the groups' program passes its start-up steps (" + why + ")", g != nullptr);
        if (g && twin("groups")) vkGroups(*g);
        settle(cards, g, nullptr, nullptr, nullptr);
        render::IOutlineCompute* o = render::makeVkOutlineCompute(cards, why);
        check("Vulkan: the outline's programs pass their start-up steps (" + why + ")", o != nullptr);
        if (o && twin("outline")) vkOutline(*o);
        settle(cards, nullptr, o, nullptr, nullptr);
        render::IRockCompute* r = render::makeVkRockCompute(cards, why);
        check("Vulkan: the rocks' program passes its start-up steps (double precision here: " + std::string(h.f64 ? "yes" : "no") + "; " + why + ")", r != nullptr || !h.f64);
        if (r && twin("rocks")) vkRocks(*r);
        settle(cards, nullptr, nullptr, r, nullptr);
        render::AtomCard* a = render::makeVkAtomCard(cards);
        if (twin("slicejump")) vkSliceJump(*a);
        render::MoleculeCard* m = render::makeVkMoleculeCard(cards);
        if (twin("molcloud")) vkMolCloud(*m);
        if (twin("glowgrid")) vkGlowGrid(cards);
        if (twin("glownear")) vkGlowNearCheck(cards);                         // 0.90.0
        render::RingCard* rc = render::makeVkRingCard(cards);                   // 0.64.0 (before the Black Hole Lab's, whose end can wait: ROADMAP)
        if (twin("rings")) vkRings(*rc);
        delete rc;
        render::ClassicCard* cc = render::makeVkClassicCard(cards);             // 0.64.0
        if (twin("classic")) vkClassic(*cc);
        delete cc;
        render::NucDensCard* nd = render::makeVkNucDensCard(cards);             // 0.67.0
        if (twin("nucdens")) vkNucDens(*nd);
        delete nd;
        render::LinesCard* ln = render::makeVkLinesCard(cards);                 // 0.68.0
        if (twin("lines")) vkLines(*ln);
        delete ln;
        render::SurfaceCard* sf = render::makeVkSurfaceCard(cards);             // 0.68.0
        if (twin("surface")) vkSurface(*sf);
        if (twin("surface")) vkSurfaceMade(*sf);                                                   // 0.90.0
        delete sf;
        if (twin("blackhole")) vkBlackHole(cards, h.f64);                                            // 0.62.0
        delete g; delete o; delete r; delete a; delete m;
        cards.collect();
    }
    std::printf("Vulkan validation: %d errors\n", vkt::g_errors);
    check("Vulkan: zero validation errors" + std::string(h.validation ? "" : " (the validation layers are not installed here)"), vkt::g_errors == 0);
    vkt::stop(h);
}
