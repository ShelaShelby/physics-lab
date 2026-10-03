#include "belts.h"

#include <cmath>

#include "format.h"
#include "sky.h"
#include "solar_data.h"
#include "units.h"

namespace orbit {

using namespace units;

double Mulberry32::next() {
    // a line-by-line copy of the web's rng(): 32-bit integer maths with wrap-around
    seed += 0x6D2B79F5u;
    uint32_t t = (seed ^ (seed >> 15)) * (1u | seed);
    t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
    return ((t ^ (t >> 14))) / 4294967296.0;
}

namespace {

struct RockElements {
    double a, e, i;             // AU, -, deg
    double aN = -1;             // AU used for the speed (Trojans: Jupiter's), < 0 = same as a
    double lib = 0;             // libration size, deg (Trojans sway around their point)
    bool hasM = false;          // the mean anomaly is set (Trojans), not random
    double LJ = 0;              // Trojans: Jupiter's mean longitude, deg
    int side = 0;               // Trojans: +60° (L4) or −60° (L5)
};

}  // namespace

std::vector<BeltGroup> makeBelts(int64_t ms, int scale) {
    scale = scale < 1 ? 1 : scale;
    Mulberry32 R(20260929);
    const auto rnd = [&]() { return R.next(); };
    const auto norm = [&]() { const double u = std::sqrt(-2 * std::log(rnd() + 1e-9)); return u * std::cos(2 * kPi * rnd()); };   // normal distribution
    const auto ray = [&](double sg) { return sg * std::sqrt(-2 * std::log(1 - rnd() + 1e-9)); };                              // Rayleigh distribution
    const double T = (ms / 86400000.0 + 2440587.5 - 2451545.0) / 36525;    // Julian centuries since J2000
    const PlanetData& J = kPlanets[4];                                       // Jupiter
    const double LJ = J.el[6] + J.el[7] * T, aJ = J.el[0] + J.el[1] * T, rad = kPi / 180;
    std::vector<BeltGroup> groups;

    const auto build = [&](const char* key, const char* color, int count, auto gen) {
        BeltGroup g;
        g.key = key; g.color = color; g.n = count;
        for (auto* v : {&g.aN, &g.a, &g.e, &g.M0, &g.libA, &g.libPh, &g.Px, &g.Py, &g.Pz, &g.Qx, &g.Qy, &g.Qz}) v->resize(count);
        for (int k = 0; k < count; k++) {
            const RockElements d = gen(k);                                   // same order of random draws as the web
            const double Om = rnd() * 360, om = rnd() * 360, I = d.i * rad;
            const double co = std::cos(om * rad), so = std::sin(om * rad), cO = std::cos(Om * rad), sO = std::sin(Om * rad), cI = std::cos(I), sI = std::sin(I);
            g.aN[k] = static_cast<float>(d.aN >= 0 ? d.aN : d.a);
            g.a[k] = static_cast<float>(d.a);
            g.e[k] = static_cast<float>(d.e);
            g.libA[k] = static_cast<float>(d.lib * rad);
            g.libPh[k] = static_cast<float>(rnd() * 6.283);
            g.M0[k] = static_cast<float>((d.hasM ? d.LJ + d.side - (Om + om) : rnd() * 360) * rad);
            g.Px[k] = static_cast<float>(co * cO - so * sO * cI); g.Py[k] = static_cast<float>(co * sO + so * cO * cI); g.Pz[k] = static_cast<float>(so * sI);
            g.Qx[k] = static_cast<float>(-so * cO - co * sO * cI); g.Qy[k] = static_cast<float>(-so * sO + co * cO * cI); g.Qz[k] = static_cast<float>(co * sI);
        }
        groups.push_back(std::move(g));
    };

    // main belt, 2.1–3.3 AU, with gaps where Jupiter's gravity has cleared the orbits (Kirkwood gaps)
    build("main", "#8a7f72", 1800 * scale, [&](int) {
        double a;
        const auto inGap = [](double a) { return std::fabs(a - 2.5) < 0.03 || std::fabs(a - 2.82) < 0.02 || std::fabs(a - 2.95) < 0.015; };
        do { a = 2.1 + 1.2 * rnd(); } while (inGap(a));
        double e = std::fmin(0.35, ray(0.085));
        if (a * (1 - e) < 1.75) e = 1 - 1.75 / a;                            // never closer to the Sun than 1.75 AU
        RockElements d{a, e, 0};
        d.i = std::fmin(25, ray(4.5));
        return d;
    });
    // Jupiter's Trojans: share Jupiter's orbit, 60° ahead (L4) and behind (L5), swaying around those points
    build("trojan", "#c4783a", 300 * scale, [&](int k) {
        RockElements d{};
        d.a = aJ + 0.05 * norm();
        d.aN = aJ;
        d.e = std::fmin(0.15, ray(0.05));
        d.i = std::fmin(35, ray(9));
        d.lib = 6 + 16 * rnd();
        d.hasM = true; d.LJ = LJ; d.side = k % 2 ? 60 : -60;
        return d;
    });
    // Kuiper belt: the classical belt, Pluto's resonant "plutinos", and a few scattered objects
    build("kuiper", "#4a6f9e", 900 * scale, [&](int) {
        const double r = rnd();
        double a, e, i;
        if (r < 0.6) { a = 41.5 + 6.5 * rnd(); e = std::fmin(0.2, ray(0.05)); i = std::fmin(35, rnd() < 0.6 ? ray(3.5) : ray(12)); }
        else if (r < 0.85) { a = 39.4 + 0.15 * norm(); e = 0.1 + 0.15 * rnd(); i = std::fmin(25, ray(7)); }
        else { a = 48 + 45 * rnd(); e = 0.2 + 0.3 * rnd(); i = std::fmin(40, ray(12)); }
        e = std::fmin(e, 1 - 30.5 / a);                                      // stays outside Neptune's orbit
        return RockElements{a, e, i};
    });
    {   // Oort cloud: a flattened inner (Hills) cloud, 2,000–20,000 AU, and a round outer cloud, 20,000–200,000 AU
        BeltGroup g;
        g.key = "oort"; g.color = "#8c98bb"; g.n = 3000 * scale; g.isStatic = true; g.alpha = 0.55f;
        g.x.resize(g.n); g.y.resize(g.n); g.z.resize(g.n);
        for (int k = 0; k < g.n; k++) {
            const bool inner = k < 800 * scale;
            const double r = (inner ? 2000 * std::pow(10.0, rnd()) : 20000 + 180000 * rnd()) * kAU;
            const double zz = 2 * rnd() - 1, ph = rnd() * 6.2832, sq = std::sqrt(1 - zz * zz);
            g.x[k] = static_cast<float>(r * sq * std::cos(ph));
            g.y[k] = static_cast<float>(r * sq * std::sin(ph));
            g.z[k] = static_cast<float>(r * zz * (inner ? 0.3 : 1));
        }
        groups.push_back(std::move(g));
    }
    return groups;
}

Vec3 beltPoint(const BeltGroup& g, int k, double t) {
    if (g.isStatic) return {g.x[k], g.y[k], g.z[k]};
    // mean anomaly M = M0 + n·t, with n = √(GM/aN³); Trojans also sway with a 150-year period
    const double aN = g.aN[k] * kAU;
    double M = g.M0[k] + std::sqrt(kGMSun / (aN * aN * aN)) * t;
    if (g.libA[k] != 0) M += g.libA[k] * std::sin(t / kYear / 150 * 6.2832 + g.libPh[k]);
    const double e = g.e[k];
    double E = M + e * std::sin(M);                                          // Kepler's equation, 3 Newton steps
    for (int i = 0; i < 3; i++) E += (M - (E - e * std::sin(E))) / (1 - e * std::cos(E));
    const double a = g.a[k] * kAU, xp = a * (std::cos(E) - e), yp = a * std::sqrt(1 - e * e) * std::sin(E);
    return {xp * g.Px[k] + yp * g.Qx[k], xp * g.Py[k] + yp * g.Qy[k], xp * g.Pz[k] + yp * g.Qz[k]};
}

const char* beltTitle(const std::string& key) {
    if (key == "main") return "Asteroid belt";
    if (key == "oort") return "Oort cloud (never seen directly)";
    if (key == "trojan") return "Jupiter's Trojans";
    return "Kuiper belt";
}

static std::vector<std::string> beltLinesRaw(const std::string& key) {
    if (key == "main") return {"Between Mars and Jupiter, about 2.1 – 3.3 AU", "More than 700,000 rocks wider than 1 km", "Total mass: only about 3% of our Moon",
                               "Ceres alone holds 39% of it", "Gaps are where Jupiter's pull clears orbits", "Dots here: 1,800 stand in for the millions"};
    if (key == "oort") {
        const auto years = [](double au) { return fmt::commas(std::stod(fmt::precision(voyagerYearsFromSun(au), 2))); };   // two figures, as the far card does
        const double ly = farDistanceLy("prox");                       // one value: the light years and the AU below both come from it
        return {"Inner part " + fmt::commas(kOortInnerAU) + " – " + fmt::commas(kOortInnerEndAU) + " AU, outer part to about " + fmt::commas(kOortOuterAU) + " AU", "Inferred from comets: the home of long-period comets",
                "Maybe trillions of icy bodies wider than 1 km", "Total mass: roughly 5 Earths (estimates vary a lot)",
                "Voyager 1 (" + fmt::kms(voyagerKms()) + ") would need about " + years(kOortInnerAU) + " years to reach " + fmt::commas(kOortInnerAU) + " AU and about " + years(kOortOuterAU) + " years to reach " + fmt::commas(kOortOuterAU) + " AU",
                "The nearest star, Proxima Centauri, is " + fmt::fixed(ly, 2) + " light years away (" + fmt::commas(std::stod(fmt::precision(ly * units::kLightYearAU, 3))) + " AU)", "Dots here: 3,000 stand in for trillions"};
    }
    if (key == "trojan") return {"They share Jupiter's orbit: 60° ahead and behind", "About a million rocks wider than 1 km", "Some orbits tilt up to 40° from Jupiter's plane",
                                 "Held in place by the Sun and Jupiter together", "Dots here: 300 stand in for the rest"};
    return {"From Neptune's orbit (30 AU) out to about 50 AU", "More than 100,000 icy worlds wider than 100 km", "Total mass: only about 0.02 Earths",
            "Home of Pluto, Haumea and Makemake", "Dots here: 900 stand in for the rest"};
}

// The facts of a belt, and (0.10.5 stage 5, audit row 18) a last line saying that its counts and masses are rough: they come from the web version and no named source was found.
std::vector<std::string> beltLines(const std::string& key) {
    std::vector<std::string> lines = beltLinesRaw(key);
    lines.push_back("The counts and masses above are rough figures (no named source)");
    return lines;
}

std::string oortCaption() {
    const std::string years = fmt::commas(std::stod(fmt::precision(voyagerYearsFromSun(kOortInnerAU), 2)));
    return "The Oort cloud (never seen directly): maybe trillions of icy comets, " + fmt::commas(kOortInnerAU) + " to " + fmt::commas(kOortOuterAU) + " AU out. Voyager 1 would need about " + years + " years to reach it.";
}

}  // namespace orbit
