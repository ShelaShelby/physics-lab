#include "card_text.h"

#include <cmath>

#include "format.h"
#include "sky.h"
#include "spin.h"
#include "units.h"

namespace orbit {

using namespace units;

// The Sun as the web version describes it: rough main-sequence rules of thumb for a
// star of mass m (in Suns). For m = 1 these give the Sun's values.
struct StarProps { double m, L, R, T, hzIn, hzOut, frost, life; };
static StarProps sunProps() {
    const double m = 1.0, L = std::pow(m, 3.5), R = m < 1 ? std::pow(m, 0.8) : std::pow(m, 0.57);
    const double T = 5772 * std::pow(L / (R * R), 0.25), sq = std::sqrt(L);   // K
    return {m, L, R, T, 0.95 * sq, 1.67 * sq, 2.7 * sq, 10 * m / L};          // habitable zone and frost line in AU, life in billion years
}

// Dwarf planets and comets are on one fixed two-body orbit (audit, 0.10.5 stage 1, row 1: against JPL Horizons the lab's Pluto is about 1.07 AU off in 1800).
static void fixedOrbitLines(Card& c) {
    for (auto& l : wrapWords("A simple orbit fixed at one date: the further the date is from it, the further this is from its real place", 46)) c.lines.push_back(l);
}

const Body* findBody(const SolarSystem& sys, int id) {
    for (const Body& b : sys.bodies)
        if (b.id == id) return &b;
    return nullptr;
}

static void cometCard(const SolarSystem& sys, const Body& b, Card& c) {
    const CometData& cm = *b.comet;
    const double r = length(b.pos) / kAU;                  // AU
    const double v = length(b.vel) / kKm;                  // km/s
    const double jd = sys.julianDay();
    const double k = std::floor((jd - cm.perihelionJD) / cm.periodDays);
    const double last = cm.perihelionJD + k * cm.periodDays, next = last + cm.periodDays, yrs = cm.periodDays / 365.25;
    const auto jdDate = [](double j) { return fmt::dateUTC(static_cast<int64_t>(std::llround((j - 2440587.5) * 86400000.0))); };
    const double a = cm.qAU / (1 - cm.e), Q = a * (1 + cm.e);   // AU
    c.title = cm.title;
    c.lines = {"Nucleus: about " + fmt::kmPlain(cm.nucleusKm) + " across",
               "Distance from the Sun now: " + (r < 10 ? fmt::fixed(r, 2) : fmt::commas(r)) + " AU (" + fmt::millionKm(r * kAU / kKm / 1e6) + ")",
               "Speed now: " + fmt::kms(v),
               "Closest to the Sun: " + fmt::fixed(cm.qAU, 2) + " AU  ·  farthest: " + (Q < 10 ? fmt::fixed(Q, 1) : fmt::commas(Q)) + " AU",
               "One trip round the Sun: " + (yrs < 10 ? fmt::fixed(yrs, 1) : fmt::commas(yrs)) + " years"};
    if (yrs < 200) {
        c.lines.push_back("Last closest approach: " + jdDate(last));
        c.lines.push_back("Next: " + jdDate(next));
    } else {
        c.lines.push_back("Closest approach: " + jdDate(cm.perihelionJD));
        const int y = fmt::yearUTC(static_cast<int64_t>(std::llround((cm.perihelionJD - 2440587.5) * 86400000.0)));
        c.lines.push_back("Comes back around the year " + fmt::commas(y + yrs));
    }
    c.lines.push_back(r < 4 ? "Warmed by the Sun: glowing head and two tails" : "Far from the Sun: frozen and dark, no tail");
    fixedOrbitLines(c);
    // the note, wrapped to about 42 letters a line
    std::string cur, word, note = cm.note;
    note += ' ';
    for (char ch : note) {
        if (ch != ' ') { word += ch; continue; }
        if (word.empty()) continue;
        const std::string tryLine = cur.empty() ? word : cur + " " + word;
        if (tryLine.size() > 42 && !cur.empty()) { c.lines.push_back(cur); cur = word; } else cur = tryLine;
        word.clear();
    }
    if (!cur.empty()) c.lines.push_back(cur);
}

static bool cardText0(const SolarSystem& sys, const Selection& sel, const CardOptions& opt, Card& c) {
    c = Card{};
    const StarProps sp = sunProps();
    switch (sel.kind) {
    case Selection::None:
        return false;

    case Selection::Far: {
        FarItem f;
        if (!opt.farOn || !farItem(sys, sel.moon, f)) return false;
        farCard(f, c);
        return true;
    }

    case Selection::Belt: {
        if (!opt.beltsOn || sel.moon < 0 || sel.moon >= static_cast<int>(sys.belts.size())) return false;
        const std::string& key = sys.belts[sel.moon].key;
        c.title = beltTitle(key);
        c.lines = beltLines(key);
        return true;
    }

    case Selection::Moon: {
        const Body* b = findBody(sys, sel.bodyId);
        if (!b || sel.moon < 0 || sel.moon >= static_cast<int>(b->moons.size())) return false;
        const MoonData& m = *b->moons[sel.moon].data;
        const double daysPerSecond = kSimTime / kDay * std::pow(10.0, opt.tspeed);   // simulated days per real second
        const double lps = daysPerSecond / m.periodDays;                             // laps per real second
        const bool fast = lps > 8;
        const double speed = 2 * kPi * m.aKm / (m.periodDays * 86400);               // km/s, circular-orbit speed
        c.title = std::string(m.name) + " · moon of " + b->name;
        c.lines = {"Radius: " + fmt::kmPlain(m.radiusKm) + (std::string(m.name) == "Moon" ? std::string() : " (" + fmt::fixed(m.radiusKm / 1737.4, m.radiusKm < 100 ? 3 : 2) + " × our Moon)"),
                   "Distance from " + b->name + ": " + fmt::km(m.aKm),
                   "Orbit: " + (m.periodDays < 10 ? fmt::fixed(m.periodDays, 2) : fmt::fixed(m.periodDays, 1)) + " days" + (m.retrograde ? ", going backwards" : ""),
                   "Speed: " + fmt::kms(speed, 2),
                   m.note};
        const double ex = opt.moonExaggeration;
        if (ex > 1.3) c.lines.push_back("Orbit drawn about " + fmt::commas(ex >= 100 ? std::round(ex / 10) * 10 : std::round(ex)) + "× too wide, so you can see it");
        if (fast) c.lines.push_back("Goes round " + (lps >= 100 ? fmt::commas(lps) : fmt::fixed(lps, 0)) + " times a second at this Time speed: too fast to see, so the whole orbit is lit");
        if (sys.realPlanets() && b->name == "Earth" && sel.moon == 0) {    // 0.10.4 stage 2: Earth's Moon in Real mode is the real one (ELP2000-82B), the other moons are not
            if (sys.realMoon()) {
                const MoonPhase p = moonPhase(sys.moonGeocentric(), b->pos * -1.0);   // Sun (at the origin) seen from Earth
                c.lines.push_back(std::string("Phase now: ") + p.name + ", " + std::to_string(static_cast<int>(std::lround(100 * p.fraction))) + "% lit");
                c.lines.push_back("It is " + fmt::fixed(p.elongationDeg, 0) + "° from the Sun in the sky. The lit part is (1 − cos of that angle) / 2, to first order (the geocentric elongation, not the exact phase angle)");
                c.lines.push_back("Distance from " + b->name + " right now: " + fmt::km(length(sys.moonGeocentric()) / kKm) + " (the one above is the mean)");
                c.lines.push_back("Real position and phase: ELP2000-82B, a 1988 theory; within " + fmt::fixed(kMoonElpWorstKm, 1) + " km of JPL Horizons at worst at the dates checked (1800 to 1 Jan 2100)");
                for (auto& l : wrapWords("The four main phases (new, first quarter, full, last quarter) are exact; the names in between (crescent, gibbous) use this app’s own cut-offs: 2, 40, 60 and 98 % lit", 56)) c.lines.push_back(l);
            } else
                c.lines.push_back("Outside 1800 to 1 Jan 2100 the real Moon (ELP2000-82B) is not used: this Moon is on a simple Kepler ellipse with a made-up starting angle, as in Simple, and has no phase here");
        }
        return true;
    }

    case Selection::Bary: {
        const Vec3 bc = sys.barycentre();
        const double dR = length(bc) / kSunRadius, drawn = kCrashRadius / kSunRadius;
        c.title = "Barycentre of the Solar System";
        c.lines = {"The point the Sun and all planets balance around",
                   "They orbit this point, not the Sun’s centre",
                   "Now " + fmt::fixed(dR, 1) + " Sun radii from the Sun’s centre",
                   "Ranges from inside the Sun to about 2 Sun radii out",
                   "Mostly Jupiter’s pull, a bit Saturn’s",
                   !opt.trueScale ? "Here the Sun is drawn about " + fmt::commas(drawn) + "× too big" : "The Sun is drawn at its true size here"};
        return true;
    }

    case Selection::Star: {
        const double t = std::round(sp.T / 10) * 10;
        c.title = "Sun · G-type star";
        c.lines = {"Mass: 1 solar mass (M☉), " + fmt::fixed(sp.m * 1.989, 2) + "×10³⁰ kg",
                   "Radius: " + fmt::fixed(sp.R, 2) + " Suns (" + fmt::km(sp.R * kSunRadius / kKm) + ")",
                   "Surface: about " + fmt::commas(t) + " K  ·  " + fmt::fixed(sp.L, sp.L < 10 ? 2 : 1) + " Suns bright",
                   "Habitable zone (rough): " + fmt::fixed(sp.hzIn, 2) + " – " + fmt::fixed(sp.hzOut, 2) + " AU",
                   "Frost line (rough): " + fmt::fixed(sp.frost, 1) + " AU (rock inside, giants outside)"};
        for (auto& l : sunSpinLines(sys, opt.tspeed, opt.paused)) c.lines.push_back(l);
        c.lines.push_back("Wobbles a little, mostly pulled by Jupiter");
        c.lines.push_back("Lifetime (rough): about " + (sp.life < 1 ? fmt::fixed(sp.life, 1) : fmt::commas(sp.life)) + " billion years");
        c.lines.push_back("Rough main-sequence estimates");
        return true;
    }

    case Selection::Body: {
        const Body* bp = findBody(sys, sel.bodyId);
        if (!bp) return false;
        const Body& b = *bp;
        if (b.comet) { cometCard(sys, b, c); return true; }
        const OrbitShape o = orbitOf(b);
        const double d = o.r / kAU;                                          // AU
        const double Teq = 254.6 * std::pow(sp.L, 0.25) / std::sqrt(d) - 273.15;   // equilibrium temperature with no atmosphere, °C
        const double Rp = b.radius / kEarthRadius, Mp = b.massEarths;        // in Earth radii and Earth masses
        const std::string shape = !o.bound ? "escaping the star" : o.e < 0.03 ? "near-circle" : "ellipse, " + fmt::au(o.rPeri / kAU) + " to " + fmt::au(o.rApo / kAU);
        const std::string mass = Mp < 0.05 ? fmt::kg(Mp * kEarthMassKg) : Mp >= 100 ? fmt::fixed(Mp / 318, 1) + " Jupiters" : (Mp < 1 ? fmt::fixed(Mp, 2) : fmt::fixed(Mp, 1)) + " Earths";
        const double g = b.gravityG >= 0 ? b.gravityG : Mp / (Rp * Rp);    // surface gravity in g: M/R² in Earth units
        const double vEsc = std::sqrt(2 * kGMSun / o.r);                     // escape speed from the Sun here, m/s
        c.title = b.name + " · " + b.kind;
        c.lines = {"Size: " + fmt::fixed(Rp, Rp < 1 ? 2 : 1) + " × Earth (radius " + fmt::km(Rp * 6371) + ")",
                   "Mass: " + mass + "  ·  gravity " + fmt::fixed(g, g < 0.1 ? 3 : 1) + " g" + (b.gravityG >= 0 ? " at the equator" : ""),
                   "Distance: " + fmt::au(d) + " (" + fmt::millionKm(d * kAU / kKm / 1e6) + ")",
                   "Speed: " + fmt::kms(o.v / kKm) + " (escape here " + fmt::kmsNumber(vEsc / kKm) + ")",
                   "Orbit: " + shape,
                   o.bound ? "One lap: " + fmt::years(o.periodYears) : "One lap: never (it leaves)",
                   b.hasTemp ? "Temperature: " + fmt::celsius(b.tempC) + " (real, " + b.tempNote + ")"
                             : "Temperature: " + fmt::celsius(std::round(Teq)) + " (no atmosphere; assumes 30 % reflected)"};
        for (auto& l : spinLines(b, sys, opt.tspeed, opt.paused)) c.lines.push_back(l);   // how it turns (planets and Pluto)
        for (auto& l : seasonLines(b, sys)) c.lines.push_back(l);                        // Earth: which season it is
        if (b.type == BodyKind::Dwarf) fixedOrbitLines(c);
        c.lines.push_back(b.note);
        if (sys.realMoon() && b.name == "Earth")           // 0.10.4 stage 2: Earth is drawn off the Earth-Moon barycentre; its orbit numbers above are the barycentre's
            c.lines.push_back("Real: Earth is drawn off the Earth–Moon balance point by the Moon's pull (about 4,700 km); the speed and orbit above are the balance point's");
        if (!b.moons.empty())
            c.lines.push_back(std::to_string(b.moons.size()) + (b.moons.size() > 1 ? " big moons shown" : " moon shown") + (opt.moonsOn ? "" : " (Moons is off)"));
        return true;
    }
    }
    return false;
}

// The card with every distance, speed and temperature in the chosen units: the numbers worked out
// here already follow the switch, and km, km/h and °C written inside the data's notes are turned too.
bool cardText(const SolarSystem& sys, const Selection& sel, const CardOptions& opt, Card& c) {
    if (!cardText0(sys, sel, opt, c)) return false;
    if (fmt::imperial()) {
        c.title = fmt::unitText(c.title);
        for (auto& l : c.lines) l = fmt::unitText(l);
    }
    return true;
}

}  // namespace orbit
