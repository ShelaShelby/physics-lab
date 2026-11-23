// 0.75.2: the Orbit Lab's corrections from an outside review, WRITTEN BEFORE THE CODE against references that are not the app's own output:
//  1) a planet's gravity and spin: the spin's push is omega^2 R_eq (omega from NASA's sidereal period, R the equatorial radius); the pull alone is what NASA says you feel plus that push
//     (NASA Planetary Fact Sheets, data/nssdc/<planet>fact.html); it must also sit just above JPL's GM/R^2 (the flattening's extra pull is about 3/2 J2 of it);
//  2) Eris's size: 2 x 1,163 km (Sicardy et al. 2011, the abstract, data/planets/sicardy2011_eris_nature10550.html);
//  3) Simple, running time against a jump: the places of the planets are those of JPL's table of approximate elements (written out here: Kepler's equation by bisection, the
//     textbook rotation) at the date reached, whether time ran there or the date was loaded;
//  4) the card's orbit numbers: for a heliocentric relative state the two-body formulas belong with G(M_sun + m) (JPL DE440 GMs typed here), and Simple's Jupiter has the table's a;
//  7) time past 2100 says so; 8) like with like in the "x Earth" sizes and the dwarf planets' gravity (G typed here: CODATA 2018);
//  B) the barycentre card: the Sun's swing from each planet, (m / M_sun) a;
//  C) the other giants' rings at each point's own slant: Uranus's and Neptune's bands against 1 - exp(-tau / mu) and the single-scattering light written out here.
#include "orbit/ring_bands.h"

namespace review0752 {

std::string joined(const std::vector<std::string>& v) { std::string s; for (const auto& l : v) s += l + "\n"; return s; }
std::string cardOf(const SolarSystem& sys, const char* name) {
    for (const Body& b : sys.bodies) if (b.name == name) { Card c; CardOptions o; cardText(sys, Selection{Selection::Body, b.id}, o, c); return joined(c.lines); }
    return "(no such body)";
}
// the first number after `key` in `s` (commas allowed inside), or -1
double numberAfter(const std::string& s, const std::string& key) {
    const std::size_t i = s.find(key);
    if (i == std::string::npos) return -1;
    std::size_t j = i + key.size();
    while (j < s.size() && !(s[j] >= '0' && s[j] <= '9')) j++;
    std::string n;
    for (; j < s.size() && ((s[j] >= '0' && s[j] <= '9') || s[j] == '.' || s[j] == ','); j++) if (s[j] != ',') n += s[j];
    return n.empty() ? -1 : std::stod(n);
}

// JPL "Keplerian Elements for Approximate Positions of the Major Planets" (data/jpl/approx_pos_2026-11-23.html): the position (m, ecliptic J2000) at a Julian day, written out as
// the page gives it: a, e, I, L, long. peri, long. node with their rates per century; M = L - long.peri; E by bisection (E - e sin E - M rises with E); then the page's rotation.
Vec3 tablePosition(const PlanetData& d, double jd) {
    const double T = (jd - 2451545.0) / 36525, D = units::kPi / 180;
    const double a = d.el[0] + d.el[1] * T, e = d.el[2] + d.el[3] * T, I = (d.el[4] + d.el[5] * T) * D;
    const double L = d.el[6] + d.el[7] * T, wbar = d.el[8] + d.el[9] * T, Om = (d.el[10] + d.el[11] * T) * D;
    double M = std::fmod(L - wbar, 360.0); if (M > 180) M -= 360; if (M < -180) M += 360;
    M *= D;
    double lo = -units::kPi - 1, hi = units::kPi + 1;                      // E - e sin E - M is increasing in E
    for (int k = 0; k < 200; k++) { const double mid = 0.5 * (lo + hi); (mid - e * std::sin(mid) - M < 0 ? lo : hi) = mid; }
    const double E = 0.5 * (lo + hi), w = wbar * D - Om;
    const double xp = a * (std::cos(E) - e), yp = a * std::sqrt(1 - e * e) * std::sin(E);
    const double cw = std::cos(w), sw = std::sin(w), cO = std::cos(Om), sO = std::sin(Om), cI = std::cos(I), sI = std::sin(I);
    const double AU = units::kAU;
    return {AU * ((cw * cO - sw * sO * cI) * xp + (-sw * cO - cw * sO * cI) * yp), AU * ((cw * sO + sw * cO * cI) * xp + (-sw * sO + cw * cO * cI) * yp), AU * ((sw * sI) * xp + (cw * sI) * yp)};
}
const PlanetData* planetNamed(const std::string& n) { for (std::size_t i = 0; i < kPlanetCount; i++) if (n == kPlanets[i].name) return &kPlanets[i]; return nullptr; }
const Body* bodyNamed(const SolarSystem& s, const std::string& n) { for (const Body& b : s.bodies) if (b.name == n) return &b; return nullptr; }

}  // namespace review0752

static void testReview0752() {
    using namespace review0752;
    const auto has = [](const std::string& hay, const char* w) { return hay.find(w) != std::string::npos; };
    constexpr double kG = 9.80665;
    // ---- 1) gravity and spin: Jupiter, Saturn, Neptune (and Uranus) ----
    {
        struct Row { const char* name; double eqKm, hours, felt, jplPull; };   // NASA fact sheets (saved): equatorial radius, sidereal period, "Acceleration (eq., 1 bar)"; JPL "Equatorial Gravity" (GM/R^2)
        const Row rows[] = {{"Jupiter", 71492, 9.925, 23.12, 24.79}, {"Saturn", 60268, 10.656, 8.96, 10.44}, {"Uranus", 25559, 17.24, 8.69, 8.87}, {"Neptune", 24764, 16.11, 11.00, 11.15}};
        SolarSystem s; s.loadAt(ref::kEpochMs);
        for (const Row& r : rows) {
            const double w = 2 * units::kPi / (r.hours * 3600), push = w * w * r.eqKm * 1000;        // m/s^2
            const std::string c = cardOf(s, r.name);
            const double gAlone = numberAfter(c, "Gravity alone "), gPush = numberAfter(c, "outward push takes off ");
            near(gPush * kG, push, 1.0, 0.06, std::string("gravity card: ") + r.name + "'s spin push is omega^2 R (" + std::to_string(push) + " m/s^2)");
            near(gAlone * kG, r.felt + push, 1.0, 0.06, std::string("gravity card: ") + r.name + "'s pull alone is the felt value plus omega^2 R");
            const double ratio = (r.felt + push) / r.jplPull;
            check(ratio > 0.999 && ratio < 1.035, std::string("gravity card: ") + r.name + "'s pull alone sits just above JPL's GM/R^2 (flattening), ratio " + std::to_string(ratio));
        }
        near(spinPushMS2("Jupiter", 71492e3), 2.2115, 1.0, 0.002, "spinPushMS2: Jupiter's omega^2 R by hand (2 pi / 35,730 s)^2 x 71,492 km = 2.2115 m/s^2");
    }
    // ---- 2) Eris ----
    {
        const DwarfData* eris = nullptr;
        for (std::size_t i = 0; i < kDwarfCount; i++) if (std::string(kDwarfs[i].name) == "Eris") eris = &kDwarfs[i];
        check(eris && eris->diameterKm == 2 * 1163, "Eris: the diameter is 2 x 1,163 km, Sicardy et al. 2011 (the occultation)");
    }
    // ---- 3) Simple: running time against a jump, and against JPL's table ----
    {
        const int64_t start = 946728000000LL;                          // 2000-01-01 12:00:00 UT
        for (double years : {30.0, 50.0, 100.0}) {
            SolarSystem run; run.loadAt(start);
            run.setRealPlanets(false);
            double worstJump = 0, worstTable = 0;
            while (run.t < years * units::kYear) run.step(units::kPhysicsStep, units::kTspeedMax);
            SolarSystem jump; jump.loadAt(run.currentUnixMs());
            const double jd = run.julianDayTDB();
            for (std::size_t i = 0; i < kPlanetCount; i++) {
                const Body* a = bodyNamed(run, kPlanets[i].name); const Body* b = bodyNamed(jump, kPlanets[i].name);
                const Vec3 pa = a->pos - a->drawShift, pb = b->pos - b->drawShift;
                worstJump = std::max(worstJump, length(pa - pb) / 1000);
                worstTable = std::max(worstTable, length(pa - tablePosition(kPlanets[i], jd)) / 1000);
            }
            std::printf("   simple, %.0f years of running: the planets differ from a jump to the same instant by at most %.3f km, from the table's formula by %.3f km\n", years, worstJump, worstTable);
            check(worstJump < 50, "simple: " + std::to_string(int(years)) + " years of running time gives the places of a jump to the same instant (within 50 km)");
            check(worstTable < 50, "simple: " + std::to_string(int(years)) + " years of running time gives the places of JPL's table written out in the test (within 50 km)");
        }
    }
    // ---- 4) the card's orbit numbers: G(M_sun + m) ----
    {
        const double GMsun = 1.32712440041279419e20;                    // JPL DE440 (data: nbody.cpp's source note)
        const struct { const char* name; double GMsystem; } rows[] = {{"Jupiter", 126712764.1e9}, {"Saturn", 37940584.8418e9}};
        for (int real = 0; real < 2; real++) {
            SolarSystem s; s.loadAt(ref::kEpochMs); s.setRealPlanets(real == 1);
            for (const auto& r : rows) {
                const Body* b = bodyNamed(s, r.name);
                const OrbitShape o = orbitOf(*b);
                const double mu = GMsun + r.GMsystem, rr = length(b->pos), v2 = dot(b->vel, b->vel);
                const double aRef = 1 / (2 / rr - v2 / mu), PRef = 2 * units::kPi * std::sqrt(aRef * aRef * aRef / mu) / units::kYear;
                near(o.a, aRef, aRef, 1e-9, std::string(real ? "Real " : "Simple ") + r.name + ": the card's a is the two-body a with G(M_sun + m)");
                near(o.periodYears, PRef, PRef, 1e-9, std::string(real ? "Real " : "Simple ") + r.name + ": the card's period is 2 pi sqrt(a^3 / G(M_sun + m))");
                if (!real) {   // Simple: the planet is put on the table's own ellipse: its a is the table's, and its period the table's mean longitude rate
                    const PlanetData* d = planetNamed(r.name);
                    const double T = (s.julianDayTDB() - 2451545.0) / 36525, aTab = d->el[0] + d->el[1] * T;
                    near(o.a / units::kAU, aTab, 1.0, 2e-6, std::string("Simple ") + r.name + ": the card's a is JPL's table's a (0.75.1 was 0.005 AU too big for Jupiter)");
                    if (std::string(r.name) == "Jupiter") {   // (the table's a and its longitude rate differ by 4e-5 for Jupiter, 8e-5 for Saturn; the Sun-only GM was 4.8e-4 off for Jupiter)
                        const double Ptab = 36525.0 * 360.0 / d->el[7] / 365.25;   // years, from the table's rate of the mean longitude (deg per century)
                        near(o.periodYears, Ptab, Ptab, 1e-4, "Simple Jupiter: the card's year is the table's year (the mean longitude's rate)");
                    }
                }
            }
        }
    }
    // ---- 7) time past 2100 says so ----
    {
        check(outsideRangeNote(kDateMaxMs).empty() && outsideRangeNote(kDateMinMs).empty(), "dates: inside 1800-2100 there is no note");
        check(outsideRangeNote(kDateMaxMs + 86400000LL).find("Past 2100") == 0 && outsideRangeNote(kDateMinMs - 86400000LL).find("Before 1800") == 0, "dates: past 2100 and before 1800 each say so");
    }
    // ---- 8) like with like ----
    {
        SolarSystem s; s.loadAt(ref::kEpochMs);
        const std::string j = cardOf(s, "Jupiter"), e = cardOf(s, "Eris");
        near(numberAfter(j, "Size: "), 71492.0 / 6378.1, 1.0, 0.06, "sizes: Jupiter in Earth's EQUATORIAL radius (6,378.1 km): 11.2");
        near(numberAfter(e, "Size: "), 1163.0 / 6371.0, 1.0, 0.02, "sizes: Eris (a mean radius) in Earth's mean radius (6,371 km): 0.18");
        const double G = 6.67430e-11, gEris = G * 1.66e22 / (1163e3 * 1163e3) / kG;     // CODATA 2018 G; JPL's mass of Eris; Sicardy's radius
        near(numberAfter(e, "gravity "), gEris, gEris, 0.02, "dwarf planet gravity: G M / R^2 over the standard 9.80665 m/s^2 (Eris)");
    }
    // ---- B) the barycentre card ----
    {
        SolarSystem s; s.loadAt(ref::kEpochMs);
        double amp[2] = {0, 0};
        const char* names[2] = {"Jupiter", "Saturn"};
        for (int k = 0; k < 2; k++) { const Body* b = bodyNamed(s, names[k]); amp[k] = b->massEarths / units::kSunInEarthMasses * orbitOf(*b).a / units::kSunRadius; }   // Sun radii
        near(amp[0], 1.07, 1.0, 0.01, "barycentre: Jupiter swings the Sun about 1.07 Sun radii");
        near(amp[1], 0.59, 1.0, 0.02, "barycentre: Saturn swings the Sun about 0.59 Sun radii");
        check(amp[1] / amp[0] > 0.5 && amp[1] / amp[0] < 0.6, "barycentre: Saturn's swing is a bit over half Jupiter's, not 'a bit'");
        Card c; CardOptions o; cardText(s, Selection{Selection::Bary, 0}, o, c);
        const std::string t = joined(c.lines);
        char want[96]; std::snprintf(want, sizeof want, "Jupiter moves the Sun about %.2f Sun radii, Saturn about %.2f", amp[0], amp[1]);
        check(has(t, want), std::string("barycentre card gives the computed swings (") + want + ")");
    }
    // ---- C) the other giants' rings at each point's own slant ----
    {
        namespace rg = orbit::rings;
        for (const char* planet : {"Uranus", "Neptune"}) {
            const rg::GiantRings gr = rg::giantRings(planet);
            // a camera 3 radii above the plane, looking down at the middle: a ring point D along the plane is at mu = h / sqrt(h^2 + D^2), by hand
            const double h = 3.0, F = 900, ppx = 640, ppy = 400;
            RingFrame f = ringFrame(ppx, ppy, F / h, 0, 0, 1, nullptr, 1.0, 0.5, 0.45, 0.74);
            f.ppx = ppx; f.ppy = ppy; f.zr = 1.0 / h; f.wn = 0.01 / h;
            f.mu0 = 0.4; f.sunlit = true;
            const std::vector<RingBand> bands = giantDrawnBands(gr.bands, gr.eqRadiusKm, f.r);
            check(!bands.empty(), std::string(planet) + ": some ring bands are drawn");
            long compared = 0; double worst = 0;
            for (const RingBand& b : bands) {
                {
                    const double rad = 0.5 * (b.a0 + b.a1);
                    for (double ang : {0.3, 2.0, 4.1}) {
                        const double q[3] = {rad * std::cos(ang), rad * std::sin(ang), 0};
                        const double mu = h / std::sqrt(h * h + rad * rad);
                        const RingValue v = ringBandValue(f, b, ringMu(f, q));
                        const double aWant = (1 - std::exp(-b.tau / mu)) * b.cover;
                        const double sh = f.mu0 / (f.mu0 + mu) * (1 - std::exp(-b.tau * (1 / mu + 1 / f.mu0))) / 0.5;     // single scattering, sunlit face
                        const double sWant = b.albedo / 0.5 * sh * b.cover;
                        const double sGot = double(v.alpha) * v.k;
                        worst = std::max(worst, std::fabs(v.alpha - aWant));
                        if (aWant > 1e-3 && sWant / aWant < 1) worst = std::max(worst, std::fabs(sGot - sWant));
                        compared++;
                    }
                }
            }
            check(compared > 0 && worst < 1e-5, std::string(planet) + ": each band's opacity and light at a point's own mu are 1 - exp(-tau / mu) and albedo / 0.5 x the single-scattering light (worst " + std::to_string(worst) + ")");
            // the slant differs across the system: close up, the outer band (larger radius, flatter) is more opaque than the same tau seen face-on
            const RingBand& outer = bands.back();
            const double qIn[3] = {0.01, 0, 0}, qOut[3] = {2.5, 0, 0};
            check(ringBandValue(f, outer, ringMu(f, qOut)).alpha > ringBandValue(f, outer, ringMu(f, qIn)).alpha, std::string(planet) + ": a flatter slant stops more light (the same band, two points)");
        }
        // far-camera limit: from 400 radii the one slant of 0.75.1 (|pz| along the line to the middle) gives the same colours within the card check's limit
        {
            const rg::GiantRings gr = rg::giantRings("Uranus");
            const double Zc = 400;
            RingFrame f = ringFrame(640, 400, 400.0 * 260 / Zc, 0.25, 0.85, 0.46, nullptr, 1.0, 0.5, 0.45, 0.74);
            f.ppx = 640; f.ppy = 400; f.zr = 1.0 / Zc; f.wn = 0.01 / Zc;
            f.mu0 = 0.4; f.sunlit = ringCameraOnPoleSide(f) == true;
            const double mu1 = std::max(std::fabs(f.pole[2]), kRingMuMin);
            const std::vector<RingBand> bands = giantDrawnBands(gr.bands, gr.eqRadiusKm, f.r);
            long ring = 0, over = 0;
            for (double sx = 0; sx < 1280; sx += 2) for (double sy = 0; sy < 800; sy += 2) {
                double q[3];
                if (!ringPointAtPixel(f, sx, sy, q)) continue;
                const double rad = std::hypot(q[0], q[1], q[2]);
                for (const RingBand& b : bands) {
                    if (!(rad >= b.a0 && rad < b.a1)) continue;
                    const RingValue nv = ringBandValue(f, b, ringMu(f, q));
                    const double oa = rg::blocked(b.tau, mu1) * b.cover, os = b.albedo / 0.5 * rg::shine(b.tau, mu1, f.mu0, f.sunlit) * b.cover, ok = oa > 1e-6 ? std::min(1.0, os / oa) : 0.0;
                    const auto level = [](double a, double k, int ch) { return int(std::lround(std::clamp(a * render::shadeEncoded(kRingTint[ch], k), 0.0, 1.0) * 255)); };
                    int d = 0; bool any = false;
                    for (int ch = 0; ch < 3; ch++) { const int x = level(nv.alpha, nv.k, ch), y = level(oa, ok, ch); d = std::max(d, std::abs(x - y)); any = any || x || y; }
                    if (!any) continue;
                    ring++; over += d > 8;
                }
            }
            std::printf("   giant rings, far camera (400 radii): %ld ring pixels, %ld over 8 levels from the one slant of 0.75.1\n", ring, over);
            check(ring > 50 && over * 100 <= ring, "giant rings: from 400 radii away each band is 0.75.1's within the card check's limit (at most 1 % over 8 levels)");
        }
    }
}
