#include "card_text.h"
#include "alphacen.h"
#include "sun_facts.h"
#include "shapes.h"
#include "moon_sats.h"
#include "iau_rotation.h"

#include <cmath>
#include <cstdio>

#include "format.h"
#include "sky.h"
#include "spin.h"
#include "units.h"
#include "real_errors.h"

namespace orbit {

using namespace units;

// star of mass m (in Suns). For m = 1 these give the Sun's values.
struct StarProps { double m, L, R, T, hzIn, hzOut, frost, life; };
static StarProps sunProps() {
    const double m = 1.0, L = std::pow(m, 3.5), R = m < 1 ? std::pow(m, 0.8) : std::pow(m, 0.57);
    const double T = sunfacts::kSunTeffK * std::pow(L / (R * R), 0.25), sq = std::sqrt(L);   // K
    return {m, L, R, T, sunfacts::hzInnerAU() * sq, sunfacts::hzOuterAU() * sq, 2.7 * sq, 10 * m / L};   // habitable zone (0.49.4: Kopparapu et al. 2014, sun_facts.h) and frost line in AU, life in billion years
}

// Dwarf planets (in Simple; 0.36: in Real only the comets) and comets are on one fixed two-body orbit (audit, 0.10.5 stage 1, row 1: against JPL Horizons the lab's Pluto is about 1.07 AU off in 1800).
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
    c.lines = {"Nucleus: about " + fmt::kmPlain(cm.nucleusKm) + " across" + (cm.nucleusRough ? " (rough: JPL gives no size)" : ""),
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
        c.lines.push_back("Comes back around the year " + std::to_string(static_cast<long long>(std::llround((y + yrs) / 10) * 10)));   // 0.39.2: a year has no thousands comma ("4,361"); to the nearest 10, as the fixed orbit's period is not that exact
    }
    c.lines.push_back(r < 4 ? "Tails drawn for show: their length and shape are not to scale" : "No tails drawn beyond 4 AU (a drawing rule: real comets can be active farther out)");   // 0.48: was "frozen and dark, no tail" beyond 4 AU, false as a rule
    fixedOrbitLines(c);
    // the note, wrapped to about 42 letters a line
    std::string cur, word, note = cm.note;
    note += ' ';
    for (char ch : note) {
        if (ch != ' ') { word += ch; continue; }
        if (word.empty()) continue;
        const std::string tryLine = cur.empty() ? word : cur + " " + word;
        if (tryLine.size() > 42 && !cur.empty()) {
            // 0.39.3: a short number goes down with the word after it ("12 Dec 1992", "26 km"), unless it is the whole line
            const std::size_t sp = cur.rfind(' ');
            const std::string last = sp == std::string::npos ? std::string() : cur.substr(sp + 1);
            const bool shortNumber = !last.empty() && last.size() <= 2 && last.find_first_not_of("0123456789") == std::string::npos;
            if (shortNumber) { c.lines.push_back(cur.substr(0, sp)); cur = last + " " + word; }
            else { c.lines.push_back(cur); cur = word; }
        } else cur = tryLine;
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

    case Selection::ACen:                                  // 0.73.0: the Alpha Centauri scene (orbit/alphacen.cpp)
        if (sel.moon < 0 || sel.moon >= acen::ThingCount) return false;
        acen::acenCard(sel.moon, sys.julianDayTDB(), c);
        return true;

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
                   "Distance from " + b->name + ": " + fmt::km(m.aKm) + " (mean)",   // night hunt 0.39.2: the mean (the semi-major axis), not where the moon is now
                   "Orbit: " + (m.periodDays < 10 ? fmt::fixed(m.periodDays, 2) : fmt::fixed(m.periodDays, 1)) + " days" + (m.retrograde ? ", going backwards" : ""),
                   "Speed: " + fmt::kms(speed, 2),
                   m.note};
        const double ex = opt.moonExaggeration;
        if (ex > 1.3) c.lines.push_back("Orbit drawn about " + fmt::commas(ex >= 100 ? std::round(ex / 10) * 10 : std::round(ex)) + "× too wide, so you can see it");
        Vec3 sp, sv;
        if (satState(m.name, sys.julianDayTDB(), sp, sv))   // 0.27: the 13 big moons at their real places (orbit/moon_sats.h)
            c.lines.push_back("Its place is the real one: an orbit fitted to JPL Horizons, within " + fmt::km(satWorstErrKm(m.name)) + " of it from 1800 to 2100");
        else if (!(b->name == "Earth" && sel.moon == 0))   // any moon outside 1800-2100 rides a fixed Kepler ellipse from a made-up start; our Moon's own lines below say what it is (0.46.3: real in Simple too)
            c.lines.push_back("Its place on the orbit is not the real one: it starts at a made-up angle");
        if (!(b->name == "Earth" && sel.moon == 0)) {   // 0.50 (review item 10): how its ball is drawn
            Vec3 ax[3];
            if (iauBodyAxes(m.name, sys.julianDayTDB(), ax))
                c.lines.push_back("Surface: a black-and-white map (USGS global mosaic), turned by its IAU rotation model; colour not measured here");
            else
                c.lines.push_back(std::string("Drawn as a grey ball: colour not measured here") + (std::string(m.name) == "Titan" ? " (its haze hides the surface in visible light)" : ""));
        }
        if (fast) c.lines.push_back("Goes round " + (lps >= 100 ? fmt::commas(lps) : fmt::fixed(lps, 0)) + " times a second at this Time speed: too fast to see, so the whole orbit is lit");
        if (b->name == "Earth" && sel.moon == 0) {    // 0.10.4 stage 2: Earth's Moon is the real one (ELP2000-82B); 0.46.3: in Simple too
            if (sys.realMoon()) {
                const MoonPhase p = moonPhase(sys.moonGeocentric(), b->pos * -1.0);   // Sun (at the origin) seen from Earth
                c.lines.push_back(std::string("Phase now: ") + p.name + ", " + std::to_string(static_cast<int>(std::lround(100 * p.fraction))) + "% lit");
                c.lines.push_back("It is " + fmt::fixed(p.elongationDeg, 0) + "° from the Sun in the sky. The lit part is (1 − cos of that angle) / 2, to first order (the geocentric elongation, not the exact phase angle)");
                c.lines.push_back("Distance from " + b->name + " right now: " + fmt::km(length(sys.moonGeocentric()) / kKm) + " (the one above is the mean)");
                c.lines.push_back("Real position and phase: ELP2000-82B, a 1988 theory; within " + fmt::fixed(kMoonElpWorstKm, 1) + " km of JPL Horizons at worst at the dates checked (1800 to 1 Jan 2100)");
                for (auto& l : wrapWords("The four main phases (new, first quarter, full, last quarter) are exact; the names in between (crescent, gibbous) use this app’s own cut-offs: 2%, 40%, 60% and 98% lit", 56)) c.lines.push_back(l);
            } else
                c.lines.push_back("Outside 1800 to 1 Jan 2100 the real Moon (ELP2000-82B) is not used: this Moon is on a simple Kepler ellipse with a made-up starting angle, and has no phase here");
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
                   "Surface (rough): about " + fmt::commas(t) + " K  ·  " + fmt::fixed(sp.L, sp.L < 10 ? 2 : 1) + " Suns bright",
                   "Habitable zone (Kopparapu et al. 2014): " + fmt::fixed(sp.hzIn, 2) + " – " + fmt::fixed(sp.hzOut, 2) + " AU",
                   "Frost line (rough): " + fmt::fixed(sp.frost, 1) + " AU (rock inside, giants outside)"};
        for (auto& l : sunSpinLines(sys, opt.tspeed, opt.paused)) c.lines.push_back(l);
        c.lines.push_back("The 25.38 days is JPL's standard rate; the 25 and 34 days are rough");   // Horizons' Sun header: "Adopted sid. rot. per.= 25.38 d"; the equator and pole figures have no named source
        c.lines.push_back("The patches drawn on it are only markers that show the spin, all at that one rate: not real sunspots");   // 0.48 (review): they were not labelled
        c.lines.push_back("Drawn in its measured colour (nearly white); its darker edge is the app's own rule, not yet a measured law");   // 0.49.4: kSunSrgb (sun_facts.h); limb darkening: ROADMAP
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
        const std::string shape = !o.bound ? "escaping the star" : o.e < 0.03 ? "near-circle, " + fmt::fixed(o.rPeri / kAU, 3) + " to " + fmt::fixed(o.rApo / kAU, 3) + " AU" : "ellipse, " + fmt::au(o.rPeri / kAU) + " to " + fmt::au(o.rApo / kAU);
        const std::string mass = Mp < 0.05 ? fmt::kg(Mp * kEarthMassKg) : Mp >= 100 ? fmt::fixed(Mp / 317.8, 1) + " Jupiters"   /* Jupiter = 317.8 Earths, this lab's planet table (NASA Planetary Fact Sheet); was 318 */ : (Mp < 1 ? fmt::fixed(Mp, 2) : fmt::fixed(Mp, 1)) + " Earths";
        const double g = b.gravityG >= 0 ? b.gravityG : Mp / (Rp * Rp);    // surface gravity in g: M/R² in Earth units
        const double vEsc = std::sqrt(2 * kGMSun / o.r);                     // escape speed from the Sun here, m/s
        c.title = b.name + " · " + b.kind;
        c.lines = {"Size: " + fmt::fixed(Rp, Rp < 1 ? 2 : 1) + " × Earth (" + (b.type == BodyKind::Planet ? "equatorial radius " : "radius ") + fmt::km(Rp * 6371) + ")",   // (0.22 stage 6: the planets' radii are equatorial, NASA fact sheet; the dwarf planets' are mean radii)
                   // 0.46.3 (outside review): a planet's card gives what you would feel at the equator (gravity minus the spin's push, NASA), and the pull alone in brackets
                   "Mass: " + mass + "  ·  " + (b.feltG >= 0 ? "you would feel " + fmt::fixed(b.feltG, 2) + " g at the equator" : "gravity " + fmt::fixed(g, g < 0.1 ? 3 : 1) + " g"),
                   "Distance: " + fmt::au(d) + " (" + fmt::millionKm(d * kAU / kKm / 1e6) + ")",
                   "Speed: " + fmt::kms(o.v / kKm) + " (escape here " + fmt::kmsNumber(vEsc / kKm) + ")",
                   "Orbit: " + shape,
                   o.bound ? "One lap: " + fmt::years(o.periodYears) : "One lap: never (it leaves)",
                   b.hasTemp ? "Temperature: " + fmt::celsius(b.tempC) + " (real, " + b.tempNote + ")"
                             : "Temperature: about " + fmt::celsius(std::round(Teq)) + " (an estimate, not measured: a bare ball that reflects 30 % of the sunlight)"};   // (0.25: it read as if the body had no atmosphere; Pluto has a thin one)
        if (b.feltG >= 0) c.lines.insert(c.lines.begin() + 2, "Gravity alone " + fmt::fixed(g, 2) + " g there; the spin's outward push takes off " + fmt::fixed(g - b.feltG, 2) + " g (NASA)");
        if (const BodyShape* sh = bodyShape(b.name.c_str()); sh && sh->polarKm < sh->eqKm) {   // 0.27: the real shape (orbit/shapes.h, NASA fact sheets)
            const double f = (sh->eqKm - sh->polarKm) / sh->eqKm;
            c.lines.push_back("Shape: squashed at the poles, polar radius " + fmt::km(sh->polarKm) + " (" + fmt::fixed(100 * f, f < 0.01 ? 2 : 1) + " % less)");
        }
        if (b.name == "Haumea")   // 0.27: two measurements that disagree; its turning angle is not known here, so it is drawn as a ball
            c.lines.push_back("Shape: an egg, about 2,122 × 1,688 × 1,028 km (Proudfoot et al. 2026; the size above is the ball of the same volume); Ortiz et al. 2017 found the longest axis at least 2,322 km. Drawn as a ball: which way it points is not modelled");
        for (auto& l : spinLines(b, sys, opt.tspeed, opt.paused)) c.lines.push_back(l);   // how it turns (planets and Pluto)
        for (auto& l : seasonLines(b, sys)) c.lines.push_back(l);                        // Earth: which season it is
        if (b.type == BodyKind::Dwarf && sys.realPlanets() && b.name == "Pluto") {   // 0.32: in Real, Pluto is the tenth body of the N-body model (orbit/nbody.h)
            for (auto& l : wrapWords("In Real: pulled by the Sun and planets and pulling them, from JPL's 2026 start. Drawn at the centre of mass of Pluto and its moons; within " +
                                     fmt::commas(double(kRealErrKmPluto[sys.relativity() ? 1 : 0])) + " km of JPL Horizons at worst, 1800-2100", 46)) c.lines.push_back(l);
        } else if (b.type == BodyKind::Dwarf && sys.realPlanets()) {                  // 0.36: the other dwarf planets ride along as test particles (orbit/nbody.h)
            for (int d = 0; d < kNBodyTest; d++)
                if (b.name == kNBodyTestName[d])
                    for (auto& l : wrapWords("In Real: pulled by the Sun, the planets and Pluto (but not by the asteroids), from JPL's 2026 start; too light to pull on them. Within " +
                                             fmt::commas(double(kRealErrKmDwarf[sys.relativity() ? 1 : 0][d])) + " km of JPL Horizons at worst, 1800-2100", 46)) c.lines.push_back(l);
        } else if (b.type == BodyKind::Dwarf) fixedOrbitLines(c);
        c.lines.push_back(b.note);
        if (sys.earthOffBary() && b.name == "Earth")       // 0.10.4 stage 2: Earth is drawn off the Earth-Moon barycentre; its orbit numbers above are the barycentre's
            c.lines.push_back("Earth is drawn off the Earth–Moon balance point by the Moon's pull (about 4,700 km); the speed and orbit above are the balance point's");
        if (!b.moons.empty())
            c.lines.push_back(std::to_string(b.moons.size()) + (b.moons.size() > 1 ? " big moons shown" : " moon shown") + (opt.moonsOn ? "" : " (Moons is off)"));
        return true;
    }
    }
    return false;
}

bool selectionStillValid(const SolarSystem& sys, const Selection& sel, const SelectionSwitches& sw) {
    const auto visible = [&](const Body& b) { return (b.type != BodyKind::Dwarf || sw.dwarfs) && (b.type != BodyKind::Comet || sw.comets); };
    if (sel.kind == Selection::Body || sel.kind == Selection::Moon) {
        const Body* b = findBody(sys, sel.bodyId);
        return b && visible(*b) && (sel.kind != Selection::Moon || (sw.moons && sel.moon >= 0 && sel.moon < static_cast<int>(b->moons.size())));
    }
    if (sel.kind == Selection::Far) { FarItem f; return sw.far && farItem(sys, sel.moon, f); }
    if (sel.kind == Selection::Belt) return sw.belts && sel.moon >= 0 && sel.moon < static_cast<int>(sys.belts.size());
    if (sel.kind == Selection::ACen) return sel.moon >= 0 && sel.moon < acen::ThingCount;
    return true;
}

std::string dateLine(const SolarSystem& sys) {
    const double yrs = sys.t / kYear;
    std::string epoch = fmt::dateUTC(sys.epochMs);
    if (sys.jumped) {                                     // a picked date also shows its time of day (Greenwich time)
        const int64_t m = ((sys.epochMs / 60000) % 1440 + 1440) % 1440;
        char hm[16];
        std::snprintf(hm, sizeof hm, ", %02d:%02d UT", static_cast<int>(m / 60), static_cast<int>(m % 60));
        epoch += hm;
    }
    return yrs * 365.25 < 1 ? (sys.jumped ? "Solar System on " : "Solar System today, ") + epoch
                            : "Solar System on " + fmt::simDate(sys.epochMs, yrs) + "  ·  " + fmt::years(yrs) + " after " + epoch;
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
