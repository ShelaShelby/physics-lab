#include "belts.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "format.h"
#include "sky.h"
#include "time_scale.h"
#include "solar_data.h"
#include "units.h"

namespace orbit {

using namespace units;

double Mulberry32::next() {
    // 32-bit integer maths with wrap-around
    seed += 0x6D2B79F5u;
    uint32_t t = (seed ^ (seed >> 15)) * (1u | seed);
    t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
    return ((t ^ (t >> 14))) / 4294967296.0;
}


std::vector<BeltGroup> makeBelts(int64_t ms, int scale) {
    static std::uint64_t nextSerial = 1;
    scale = scale < 1 ? 1 : scale;
    // The Oort cloud's dots (no one has seen it: they are drawn, not observed). Up to 0.26 the same generator first made the made-up asteroid belts, so
    // the Oort dots took the numbers after those draws; to keep exactly the same dots, the generator starts where it was then: Mulberry32's state
    // only adds 0x6D2B79F5 per draw, so N draws on is seed + N x 0x6D2B79F5. N was counted by running 0.26's makeBelts (its draws before the Oort cloud).
    const auto oldDraws = [](int sc) -> uint32_t { switch (sc) { case 1: return 23150; case 2: return 46351; case 4: return 92665; case 8: return 185377; case 16: return 370837; default: return 23150u * static_cast<uint32_t>(sc); } };
    Mulberry32 R(20260929u + oldDraws(scale) * 0x6D2B79F5u);
    const auto rnd = [&]() { return R.next(); };
    const double rad = kPi / 180;
    std::vector<BeltGroup> groups;

    // 0.27 (owner's choice, 5 Nov 2026): the asteroid belt, the Trojans and the Kuiper belt are real asteroids: the brightest numbered ones of each
    // group from NASA/JPL's Small-Body Database (asteroid_data.inc, tools/make_asteroids.py), each on the fixed two-body ellipse of its record
    // (osculating elements at the record's epoch, mostly mid-2026). Its mean anomaly is carried from the record's epoch to the start date with
    // the record's own mean motion n, and its speed distance aN is the one that gives that n with this lab's GM of the Sun (so it moves at n).
    // Far from the record's date a rock drifts from the real one (Jupiter's pull is not modelled for them), as the dwarf planets do.
    // scale > 1 (the benchmark's stress scene only) repeats the same rocks scale times.
    static const double kRocks[][10] =
#include "asteroid_data.inc"
    ;
    const double jd = timescale::tdbFromUT(ms / 86400000.0 + 2440587.5);   // 0.46.3: the date is UT, the elements' epochs TDB
    const auto build = [&](const char* key, const char* color, int group) {
        BeltGroup g;
        g.key = key; g.color = color; g.serial = nextSerial++;
        std::vector<const double*> recs;
        for (const auto& r : kRocks) if (static_cast<int>(r[0]) == group) recs.push_back(r);
        g.n = static_cast<int>(recs.size()) * scale;
        for (auto* v : {&g.aN, &g.a, &g.e, &g.M0, &g.libA, &g.libPh, &g.Px, &g.Py, &g.Pz, &g.Qx, &g.Qy, &g.Qz}) v->resize(g.n);
        for (int k = 0; k < g.n; k++) {
            const double* r = recs[k % recs.size()];
            const double I = r[3] * rad, Om = r[4] * rad, om = r[5] * rad;
            const double co = std::cos(om), so = std::sin(om), cO = std::cos(Om), sO = std::sin(Om), cI = std::cos(I), sI = std::sin(I);
            const double n = r[8] * rad / kDay;                                         // rad/s
            g.aN[k] = static_cast<float>(std::cbrt(kGMSun / (n * n)) / kAU);
            g.a[k] = static_cast<float>(r[1]);
            g.e[k] = static_cast<float>(r[2]);
            g.libA[k] = 0; g.libPh[k] = 0;
            const double M = std::fmod(r[6] * rad + n * (jd - r[7]) * kDay, 2 * kPi);
            g.M0[k] = static_cast<float>(M);
            g.Px[k] = static_cast<float>(co * cO - so * sO * cI); g.Py[k] = static_cast<float>(co * sO + so * cO * cI); g.Pz[k] = static_cast<float>(so * sI);
            g.Qx[k] = static_cast<float>(-so * cO - co * sO * cI); g.Qy[k] = static_cast<float>(-so * sO + co * cO * cI); g.Qz[k] = static_cast<float>(co * sI);
        }
        groups.push_back(std::move(g));
    };
    build("main", "#8a7f72", 0);
    build("trojan", "#c4783a", 1);
    build("kuiper", "#4a6f9e", 2);
    {   // Oort cloud. 0.85.0 (outside review, PLAN_REVIEW.md R4, the owner's answer of 25 Nov 2026): the dots are a sample of a published model of the cloud, not
        // objects: their distances follow oortModelInverse (belts.h, Vokrouhlicky, Nesvorny & Dones 2019) between NASA's edges; their directions are spread evenly
        // (the model's outer part is "nearly isotropic in space"; its inner part's lean is given only as a figure: belts.h). Until 0.84.1 the distances were
        // made up (evenly spread in log r).
        BeltGroup g;
        g.key = "oort"; g.color = "#8c98bb"; g.n = 3000 * scale; g.isStatic = true; g.alpha = 0.55f; g.serial = nextSerial++;
        g.x.resize(g.n); g.y.resize(g.n); g.z.resize(g.n);
        for (int k = 0; k < g.n; k++) {
            const double r = oortModelInverse(rnd()) * kAU;
            const double zz = 2 * rnd() - 1, ph = rnd() * 6.2832, sq = std::sqrt(1 - zz * zz);
            g.x[k] = static_cast<float>(r * sq * std::cos(ph));
            g.y[k] = static_cast<float>(r * sq * std::sin(ph));
            g.z[k] = static_cast<float>(r * zz);
        }
        groups.push_back(std::move(g));
    }
    return groups;
}

// 0.85.0: the Oort cloud model of belts.h. n(r) = c_k r^-alpha_k on the three parts, continuous at the breaks, normalised over kOortInnerAU..kOortOuterAU.
namespace {
struct OortPart { double r0, r1, alpha, c; };
const std::array<OortPart, 3>& oortParts() {
    static const std::array<OortPart, 3> P = [] {
        std::array<OortPart, 3> p{{{kOortInnerAU, kOortModelBreak1AU, kOortModelAlpha[0], 1}, {kOortModelBreak1AU, kOortModelBreak2AU, kOortModelAlpha[1], 1},
                                    {kOortModelBreak2AU, kOortOuterAU, kOortModelAlpha[2], 1}}};
        p[1].c = p[0].c * std::pow(p[1].r0, p[1].alpha - p[0].alpha);   // c0 r^-a0 = c1 r^-a1 at the break
        p[2].c = p[1].c * std::pow(p[2].r0, p[2].alpha - p[1].alpha);
        return p;
    }();
    return P;
}
double partIntegral(const OortPart& p, double a, double b) {   // Int_a^b c r^-alpha dr
    const double k = 1 - p.alpha;
    return p.c * (std::pow(b, k) - std::pow(a, k)) / k;
}
double oortTotal() { double t = 0; for (const auto& p : oortParts()) t += partIntegral(p, p.r0, p.r1); return t; }
}  // namespace

double oortModelShare(double r1AU, double r2AU) {
    double s = 0;
    for (const auto& p : oortParts()) {
        const double a = std::max(p.r0, r1AU), b = std::min(p.r1, r2AU);
        if (b > a) s += partIntegral(p, a, b);
    }
    return s / oortTotal();
}

double oortModelInverse(double u) {
    double want = std::clamp(u, 0.0, 1.0) * oortTotal();
    for (const auto& p : oortParts()) {
        const double whole = partIntegral(p, p.r0, p.r1);
        if (want <= whole || &p == &oortParts()[2]) {
            const double k = 1 - p.alpha;
            return std::min(p.r1, std::pow(std::pow(p.r0, k) + want * k / p.c, 1 / k));
        }
        want -= whole;
    }
    return kOortOuterAU;
}

double solveKepler(double e, double M) {
    const double twoPi = 2 * kPi;
    M -= twoPi * std::floor(M / twoPi + 0.5);                                // -pi .. pi
    double E = e > 0.8 ? (M < 0 ? -kPi : kPi) : M + e * std::sin(M);
    for (int k = 1; k <= 60; k++) {                                          // Newton's method on f(E) = E - e sin E - M
        const double dE = (M - (E - e * std::sin(E))) / (1 - e * std::cos(E));
        E += dE;
        if (k >= 3 && std::fabs(dE) < 1e-9) break;
    }
    if (std::fabs(E - e * std::sin(E) - M) > 1e-9) {                         // safety net: bisection (f rises with E, so halving cannot miss the one root)
        double lo = -kPi, hi = kPi;
        for (int k = 0; k < 80; k++) { E = (lo + hi) / 2; if (E - e * std::sin(E) - M > 0) hi = E; else lo = E; }
    }
    return E;
}

double beltMeanMotion(const BeltGroup& g, int k) {
    const double aN = g.aN[k] * kAU;
    return std::sqrt(kGMSun / (aN * aN * aN));
}

Vec3 beltPoint(const BeltGroup& g, int k, double t) {
    if (g.isStatic) return {g.x[k], g.y[k], g.z[k]};
    // mean anomaly M = M0 + n·t, with n = √(GM/aN³); 0.75.2: no Trojan sway any more (libA is 0 for every rock: plain fixed ellipses; the line below stays for a nonzero libA)
    double M = g.M0[k] + beltMeanMotion(g, k) * t;
    if (g.libA[k] != 0) M += g.libA[k] * std::sin(t / kYear / 150 * 6.2832 + g.libPh[k]);
    const double e = g.e[k];
    const double E = solveKepler(e, M);                                      // Kepler's equation (see belts.h)
    const double a = g.a[k] * kAU, xp = a * (std::cos(E) - e), yp = a * std::sqrt(1 - e * e) * std::sin(E);
    return {xp * g.Px[k] + yp * g.Qx[k], xp * g.Py[k] + yp * g.Qy[k], xp * g.Pz[k] + yp * g.Qz[k]};
}

void beltScreen(const BeltCamera& c, const Vec3& p, double& X, double& Y) {
    const Q q = c.proj(p.x, p.y, p.z);                 // (behind the camera: -1e9, -1e9)
    X = q.first; Y = q.second;
}

void rockFrameCamera(const BeltCamera& c, render::RockFrame& f) {
    f.bc[0] = c.bc.x; f.bc[1] = c.bc.y; f.bc[2] = c.bc.z;
    f.ca = c.ca; f.sa = c.sa; f.cb = c.cb; f.sb = c.sb; f.px = c.px; f.d3 = c.d3;
    if (c.d3) { f.sx = c.ppx; f.sy = c.ppy; f.d = c.d; f.cd = c.cd; f.lx = c.lx; f.ly = c.ly; f.F = c.F; f.zNear = c.zNear; }
    else { f.sx = c.W / 2 + c.fx + c.ox; f.sy = c.H / 2 + c.oy; }
}

const char* beltTitle(const std::string& key) {
    if (key == "main") return "Asteroid belt";
    if (key == "oort") return "Oort cloud (never seen directly)";
    if (key == "trojan") return "Jupiter's Trojans";
    return "Kuiper belt";
}

// 0.75.2 (outside review): the main belt's count is one number from one saved source in both the card and the Learn page: Tedesco & Desert 2002, AJ 123, 2070, abstract: the cumulative number of main-belt
// asteroids with diameters greater than 1 km is (1.2 +- 0.5) x 10^6 (data/planets/tedesco_desert2002_aj123_2070_iop.html). (The card said "more than 700,000", the lower end of that range; Learn said "millions".)
static std::vector<std::string> beltLinesRaw(const std::string& key) {
    if (key == "main") return {"Between Mars and Jupiter, about 2.1 – 3.3 AU", "About 1.2 million rocks wider than 1 km (give or take 0.5 million)", "Total mass: only about 3% of our Moon",
                               "Ceres alone holds 39% of it", "Gaps are where Jupiter's pull clears orbits", "Dots here: the 1,800 largest known, at their real places (JPL)"};
    if (key == "oort") {
        const auto years = [](double au) { return fmt::commas(std::stod(fmt::precision(voyagerYearsFromSun(au), 2))); };   // two figures, as the far card does
        const double ly = farDistanceLy("prox");                       // one value: the light years and the AU below both come from it
        return {"Inner edge about " + fmt::commas(kOortInnerAU) + " – " + fmt::commas(kOortInnerEdgeFarAU) + " AU, outer edge about " + fmt::commas(kOortOuterEdgeNearAU) + " – " +
                    fmt::commas(kOortOuterAU) + " AU (NASA)", "Inferred from comets: the home of long-period comets",
                "Estimated from models: no object in it has been seen",
                "Hundreds of billions, maybe trillions, of icy bodies (NASA)", "Total mass: roughly 5 Earths (estimates vary a lot)",
                "Voyager 1 (" + fmt::kms(voyagerKms()) + ") would need about " + years(kOortInnerAU) + " years to reach " + fmt::commas(kOortInnerAU) + " AU and about " + years(kOortOuterAU) + " years to reach " + fmt::commas(kOortOuterAU) + " AU",
                "The nearest star, Proxima Centauri, is " + fmt::fixed(ly, 2) + " light years away (" + fmt::commas(std::stod(fmt::precision(ly * units::kLightYearAU, 3))) + " AU)", "Dots here: 3,000 points sampled from a published model of the cloud (how many bodies at each distance: Vokrouhlický, Nesvorný & Dones 2019); they are not objects",
                "The model's inner part (inside 15,000 AU) leans towards the ecliptic; the dots do not show that lean (it is given only as a figure)"};
    }
    if (key == "trojan") return {"They share Jupiter's orbit: 60° ahead and behind", "About a million rocks wider than 1 km", "Some orbits tilt up to 40° from Jupiter's plane",
                                 "Held in place by the Sun and Jupiter together", "Dots here: the 300 largest known, at their real places (JPL)"};
    return {"From Neptune's orbit (30 AU) out to about 50 AU", "More than 100,000 icy worlds wider than 100 km", "Total mass: only about 0.02 Earths",
            "Home of Pluto, Haumea and Makemake", "Dots here: the 900 largest known, at their real places (JPL); some, like Sedna, go far past 50 AU"};
}

std::vector<std::string> beltLines(const std::string& key) {
    std::vector<std::string> lines = beltLinesRaw(key);
    lines.push_back("The counts and masses above are rough figures (no named source)");
    return lines;
}

std::string oortCaption() {
    const std::string years = fmt::commas(std::stod(fmt::precision(voyagerYearsFromSun(kOortInnerAU), 2)));
    return "The Oort cloud (never seen directly): hundreds of billions, maybe trillions, of icy bodies, " + fmt::commas(kOortInnerAU) + " to " + fmt::commas(kOortOuterAU) + " AU out. Voyager 1 would need about " + years + " years to reach it.";
}

}  // namespace orbit
