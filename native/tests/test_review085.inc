// 0.85.0: the outside review's Orbit Lab fixes (native/PLAN_REVIEW.md R1-R4), against independent references: the inverse-square law, Neckel & Labs 1994's own
// relations (Equations 5 and 6), an independent 2D quadrature of the eclipses, the sunspot records' own disc places, the published Oort cloud model and JPL Horizons.
// Written before the code: none of it builds on 0.84.1 (orbit/true_light.h, render/sun_limb.h, orbit/sunspots.h and the model functions do not exist there),
// and the text checks below fail on 0.84.1's files (the made-up Oort dots and Sun patches, the Sun's card).
#include <fstream>
#include <functional>
#include <sstream>
#include "orbit/true_light.h"
#include "orbit/sunspots.h"
#include "render/sun_limb.h"

namespace t085 {
std::string readFile(const std::string& p) { std::ifstream f(p, std::ios::binary); std::stringstream s; s << f.rdbuf(); return s.str(); }
const std::string kData = PL_DATA_DIR;
const std::string kSrc = kData + "/../src";
// the lines of an extract's block (the "#--- begin <name>" block)
std::vector<std::string> block(const std::string& extract, const std::string& name) {
    std::vector<std::string> out;
    std::istringstream in(readFile(kData + "/" + extract));
    std::string line;
    bool on = false;
    while (std::getline(in, line)) {
        if (line.rfind("#--- begin " + name, 0) == 0) { on = true; continue; }
        if (on && line.rfind("#--- end", 0) == 0) break;
        if (on) out.push_back(line);
    }
    return out;
}
// Gauss-Legendre nodes on [-1, 1] by Newton's method on P_n (independent of render/sun_limb.h's table)
void glNodes(int n, std::vector<double>& x, std::vector<double>& w) {
    x.resize(std::size_t(n)); w.resize(std::size_t(n));
    for (int i = 0; i < n; i++) {
        double z = std::cos(3.14159265358979323846 * (i + 0.75) / (n + 0.5)), pp = 1;
        for (int it = 0; it < 100; it++) {
            double p1 = 1, p2 = 0;
            for (int j = 1; j <= n; j++) { const double p3 = p2; p2 = p1; p1 = ((2 * j - 1) * z * p2 - (j - 1) * p3) / j; }
            pp = n * (z * p1 - p2) / (z * z - 1);
            const double z1 = z; z = z1 - p1 / pp;
            if (std::fabs(z - z1) < 1e-15) break;
        }
        x[std::size_t(i)] = z; w[std::size_t(i)] = 2 / ((1 - z * z) * pp * pp);
    }
}
// Int_a^b f with t in [0, pi], x = a + (b - a)(1 - cos t)/2, 64-point Gauss-Legendre in t
double quad(const std::function<double(double)>& f, double a, double b) {
    static std::vector<double> X, W;
    if (X.empty()) glNodes(64, X, W);
    const double pi = 3.14159265358979323846;
    double s = 0;
    for (std::size_t k = 0; k < X.size(); k++) { const double t = pi / 2 * (X[k] + 1); s += f(a + (b - a) * (1 - std::cos(t)) / 2) * (b - a) * std::sin(t) / 2 * pi / 2 * W[k]; }
    return s;
}
// the covered share of the Sun's light by an independent route: polar coordinates round the OCCULTER's centre (rho, psi), the part inside the Sun's disc
// found exactly for each rho, the polynomial integrated over it; the Sun's radius 1, the occulter's B, the centres T apart (all over the Sun's radius)
double coveredRef(double B, double T, const double A[6]) {
    std::vector<double> cuts = {0, B};
    for (double c : {std::fabs(1 - T), 1 + T}) if (c > 0 && c < B) cuts.push_back(c);
    std::sort(cuts.begin(), cuts.end());
    double tot = 0;
    for (std::size_t i = 0; i + 1 < cuts.size(); i++)
        tot += quad([&](double rho) {
            if (rho <= 0) return 0.0;
            double psi0;
            if (T == 0) psi0 = rho < 1 ? 0 : 3.14159265358979323846;
            else { const double c = (1 - T * T - rho * rho) / (2 * T * rho); psi0 = c >= 1 ? 0 : c <= -1 ? 3.14159265358979323846 : std::acos(c); }
            if (psi0 >= 3.14159265358979323846) return 0.0;
            return rho * 2 * quad([&](double psi) { const double r2 = T * T + rho * rho + 2 * T * rho * std::cos(psi); return render::limbP(A, std::sqrt(std::max(0.0, 1 - r2))); },
                                  psi0, 3.14159265358979323846);
        }, cuts[i], cuts[i + 1]);
    return tot / (3.14159265358979323846 * render::limbFlux(A));
}
// the Sun's geocentric ecliptic longitude (degrees, J2000 equinox), Meeus, Astronomical Algorithms (1998), ch. 25 low-precision formula (0.01 degree)
double sunLongitudeJ2000(double jd) {
    const double T = (jd - 2451545) / 36525, d2r = 3.14159265358979323846 / 180;
    const double L0 = 280.46646 + 36000.76983 * T + 0.0003032 * T * T, M = (357.52911 + 35999.05029 * T - 0.0001537 * T * T) * d2r;
    const double C = (1.914602 - 0.004817 * T - 0.000014 * T * T) * std::sin(M) + (0.019993 - 0.000101 * T) * std::sin(2 * M) + 0.000289 * std::sin(3 * M);
    return L0 + C - 1.397 * T;   // (of date to J2000: the general precession in longitude, 1.397 degrees a century, Meeus ch. 21)
}
double jdOf(int y, int m, double d) { if (m <= 2) { y--; m += 12; } const int A = y / 100, B = 2 - A + A / 4; return std::floor(365.25 * (y + 4716)) + std::floor(30.6001 * (m + 1)) + d + B - 1524.5; }
}  // namespace t085

static void testReview085() {
    using namespace t085;
    // ---------------- R1: the true sunlight ----------------
    {
        for (double d : {0.387, 1.0, 5.2, 30.07, 83.0})
            for (double dE : {1.0, 30.07})
                check(std::fabs(orbit::lightFactor(2 * d, dE, true) / orbit::lightFactor(d, dE, true) - 0.25) < 1e-14, "R1 (a): twice the distance, a quarter of the light (inverse square), d " + std::to_string(d));
        // the same albedo and angle at d and 2d: the linear light drawn (render/shading.h shadeEncoded on black space) is exactly 1/4
        for (double alb : {0.2, 0.5, 0.8}) {
            const double l1 = render::srgbDecode(render::shadeEncoded(alb, 0.7 * orbit::lightFactor(9.58, 9.58, true))), l2 = render::srgbDecode(render::shadeEncoded(alb, 0.7 * orbit::lightFactor(2 * 9.58, 9.58, true)));
            check(std::fabs(l2 / l1 - 0.25) < 1e-9, "R1 (a): the drawn linear light at 2d is 1/4 of that at d (albedo " + std::to_string(alb) + ")");
        }
        for (double dE : {0.39, 1.0, 9.58, 30.07}) {
            check(std::fabs(orbit::exposureFor(dE) * orbit::sunlightAt(dE) - 1) < 1e-14, "R1 (b): the exposure times the sunlight at the followed body is 1");
            check(std::fabs(orbit::lightFactor(dE, dE, true) - 1) < 1e-14, "R1 (b): the followed body is drawn as with even lighting");
        }
        check(orbit::lightFactor(30.07, 1.0, false) == 1.0, "R1: even lighting (the non-realistic choice) leaves every body at 1");
        check(orbit::exposureReadout("Neptune", 30.07, "Earth", 1.0) == "Exposure set for Neptune (30.1 au). Earth gets 900 times more sunlight.", "R1: the readout, Neptune followed: " + orbit::exposureReadout("Neptune", 30.07, "Earth", 1.0));
        check(orbit::exposureReadout("Earth", 1.0, "Neptune", 30.07).find("Neptune gets 900 times less sunlight") != std::string::npos, "R1: the readout, Earth followed");
        // (d) Most realistic sets the true dimming (0.96.0: the list moved from ui/realest.cpp to app/realistic.cpp, which this test program does not link: its text.
        // Corrected on purpose, the same two facts: the list's row, and that its value sets s.trueLight. REPORT 0.96.0)
        const std::string re = readFile(kSrc + "/app/realistic.cpp");
        const std::size_t ap = re.find("bool set(orbit::ViewSettings& v");
        check(ap != std::string::npos && re.find("if (k == \"light\") return pick2(x, \"True sunlight\", \"Even (not realistic)\", v.trueLight);", ap) < re.find("\n}", ap),
              "R1 (d): Most realistic (the list's set) sets True sunlight");
        check(re.find("{\"orbit\", {\"light\", \"True sunlight\"") != std::string::npos, "R1: the realistic choice's dot marks True sunlight");
        // the shaders carry the factor: the planet, the lit ball (the light's length) and the rings
        check(std::string(render::kPlanetFS).find("* sunF * uLightK") != std::string::npos, "R1: kPlanetFS multiplies the light by uLightK");
        check(std::string(render::kFragmentShader).find("float lit = max(dot(n, L), 0.0) * kL;") != std::string::npos, "R1: the lit ball (mode 2) multiplies by its light's length");
        check(std::string(render::kRingsPart).find("* k * uLightK") != std::string::npos, "R1: the rings' shader multiplies by uLightK");
    }
    // ---------------- R2: the Sun's edge, Neckel & Labs 1994 ----------------
    {
        const std::vector<std::string> rows = block("sun/neckel_labs_1994_SoPh153_91.extract.txt", "p. 98, Table I");
        int n = 0;
        for (const std::string& r : rows) {
            double v[9]; int n86, n87; char star[8] = {0};
            double lam;
            if (std::sscanf(r.c_str(), "%lf%7[*] %d %d %lf %lf %lf %lf %lf %lf %lf", &lam, star, &n86, &n87, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6]) != 11 &&
                std::sscanf(r.c_str(), "%lf %d %d %lf %lf %lf %lf %lf %lf %lf", &lam, &n86, &n87, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6]) != 10) continue;
            const auto& row = render::sunlimb::kNl94[n];
            bool same = std::fabs(row[0] - lam) < 1e-9;
            for (int k = 0; k < 7; k++) same = same && std::fabs(row[1 + k] - v[k]) < 1e-9;
            check(same, "R2: sun_limb_data.inc's row " + std::to_string(n) + " is Table I's (the extract)");
            double sum = 0; for (int k = 0; k < 6; k++) sum += v[k];
            const double fi = render::limbFlux(v);
            if (std::fabs(lam - 365.875) > 1e-6) {
                check(std::fabs(sum - 1) <= 3e-5, "R2 (a): Sum A_n = 1 (Equation 5) at " + std::to_string(lam) + " nm: " + std::to_string(sum));
                check(std::fabs(fi - v[6]) <= 6e-5, "R2 (a): Equation (6) gives the printed F/I at " + std::to_string(lam) + " nm: " + std::to_string(fi));
            } else check(std::fabs(sum - 0.99800) < 1e-9 && std::fabs(fi - 0.74354) < 1e-5, "R2: the 365.875 nm row is as printed (sums to 0.99800, F/I 0.74354: REPORT 0.84.1)");
            n++;
        }
        check(n == 30, "R2: Table I's 30 rows read from the extract (" + std::to_string(n) + ")");
        const render::SunLimb& L = render::sunLimb();
        double cmax = 0;
        for (int c = 0; c < 3; c++) { check(std::fabs(render::limbP(L.poly[c], 1) - 1) < 1e-12, "R2 (b): channel " + std::to_string(c) + "'s polynomial is 1 at mu = 1"); cmax = std::max(cmax, L.centre[c]); }
        check(std::fabs(cmax - 1) < 1e-15, "R2: the disc centre's brightest channel is 1 (the screen's white)");
        check(render::limbP(L.poly[0], 0.2) / render::limbP(L.poly[2], 0.2) > 1.2 && render::limbP(L.poly[0], 0.2) < 1, "R2: the edge is darker and redder than the middle");
        // (c) one wavelength's weight at a table wavelength returns that row (normalised to P(1) = 1)
        for (int i : {7, 16, 25}) {
            const auto& row = render::sunlimb::kNl94[i];
            const double nm = row[0], w = 1;
            double out[6]; render::limbFromSamples(1, &nm, &w, out);
            double sum = 0; for (int k = 0; k < 6; k++) sum += row[1 + k];
            bool same = true; for (int k = 0; k < 6; k++) same = same && std::fabs(out[k] - row[1 + k] / sum) < 1e-14;
            check(same, "R2 (c): a single weight at " + std::to_string(nm) + " nm returns that row");
        }
        // (d) the eclipses
        const double U[6] = {1, 0, 0, 0, 0, 0};
        for (const auto& cs : std::vector<std::array<double, 2>>{{1, 0.5}, {0.3, 0.8}, {1.05, 0.3}, {0.5, 1.2}, {2, 2.5}, {0.2, 0}, {0.99, 0.02}, {3, 3.9}}) {
            check(std::fabs(render::sunCoveredLimb(1, cs[0], cs[1], U) - render::sunCovered(1, cs[0], cs[1])) < 1e-9, "R2 (d): an evenly bright Sun gives the circles' overlap (B " + std::to_string(cs[0]) + ", T " + std::to_string(cs[1]) + ")");
            for (int c = 0; c < 3; c++) {
                const double ref = coveredRef(cs[0], cs[1], L.poly[c]), app = render::sunCoveredLimb(0.0046, 0.0046 * cs[0], 0.0046 * cs[1], L.poly[c]);
                check(std::fabs(app - ref) < 1e-6, "R2 (d): the covered share against an independent 2D quadrature, B " + std::to_string(cs[0]) + " T " + std::to_string(cs[1]) + " channel " + std::to_string(c) +
                                                    ": " + std::to_string(app) + " vs " + std::to_string(ref));
            }
        }
        for (int c = 0; c < 3; c++) {
            check(render::sunCoveredLimb(0.0046, 0.003, 0.0076, L.poly[c]) == 0, "R2 (d): no overlap, nothing covered");
            check(std::fabs(render::sunCoveredLimb(0.0046, 0.0046, 0, L.poly[c]) - 1) < 1e-12, "R2 (d): a centred occulter of the Sun's radius covers it all");
            const double cen = render::sunCoveredLimb(1, 0.1, 0, L.poly[c]), edge = render::sunCoveredLimb(1, 0.1, 0.85, L.poly[c]);
            check(cen > 0.01 && edge < 0.01, "R2 (d): limb darkening makes a central transit dim more than an edge one (the even Sun: 0.01 both): " + std::to_string(cen) + ", " + std::to_string(edge));
        }
        const double s0[3] = {0, 0, 0}, far[3] = {0, 0, -50}, L3[3] = {0, 0, 1};
        double out[3]; render::sunlightPastLimb(s0, far, 1, L3, 0.0046, out);
        check(out[0] == 1 && out[1] == 1 && out[2] == 1, "R2 (d): an occulter on the far side: full sunlight");
        const double front[3] = {0, 0, 150};
        render::sunlightPastLimb(s0, front, 1, L3, 0.0046, out);
        check(out[0] == 0 && out[1] == 0 && out[2] == 0, "R2 (d): a ball of 1/150 rad in front of a Sun of 0.0046 rad, centred: total");
        check(std::string(render::kPlanetFS).find("vec3 sunCoveredLimb(") != std::string::npos && std::string(render::kPlanetFS).find("if (uMode == 4)") != std::string::npos,
              "R2: the GPU's eclipse and the Sun's ball use the measured edge");
        const std::string card = readFile(kSrc + "/orbit/card_text.cpp");
        check(card.find("Neckel & Labs 1994") != std::string::npos && card.find("the app's own rule") == std::string::npos, "R2: the Sun's card names Neckel & Labs 1994");
    }
    // ---------------- R3: the sunspots ----------------
    {
        const orbit::SpotTableInfo& T = orbit::sunspotTable();
        check(T.ok, "R3: the built-in sunspot table is read (" + T.why + ")");
        check(T.rows == 255250 && T.first == orbit::spotDayOf(1874, 5, 9) && T.last == orbit::spotDayOf(2026, 11, 24), "R3: its rows and its first and last day (tools/make_sunspot_table.py)");
        check(orbit::kSpotsDay0JD == jdOf(1874, 1, 1), "R3: the table's day 0 is 1874-01-01 00:00 UT (Meeus's Julian day)");
        check(orbit::spotDayOf(1874, 1, 1) == 0 && orbit::spotDayOf(1875, 1, 1) == 365 && orbit::spotDayOf(1992, 1, 2) == T.midnightFrom, "R3: the table's days");
        // (a) rows read back exactly (one 1874, one 1976, one 2016; plus 1950): the extract's lines in format.txt's columns
        int found = 0, want = 0;
        for (const char* y : {"1874", "1950", "1976", "2016"})
            for (int k = 1; k <= 3; k++) {
                const std::vector<std::string> b = block(std::string("sun/spots/g") + y + ".extract.txt", "row " + std::to_string(k));
                if (b.empty()) continue;
                const std::string& l = b[0];
                const int yy = std::stoi(l.substr(0, 4)), mm = std::stoi(l.substr(4, 2)), dd = std::stoi(l.substr(6, 2)), area = std::stoi(l.substr(40, 4));
                const double lon = std::stod(l.substr(57, 5)), lat = std::stod(l.substr(63, 5));
                const int day = orbit::spotDayOf(yy, mm, dd) + (yy >= 1992 ? 1 : 0);
                want++;
                for (const auto& r : orbit::spotRowsOfDay(day))
                    if (r[0] == static_cast<int>(std::lround(lat * 10)) && r[1] == static_cast<int>(std::lround(lon * 10)) % 3600 && r[2] == area) { found++; break; }
            }
        check(want == 7 && found == want, "R3 (a): the saved rows are in the table exactly (" + std::to_string(found) + " of " + std::to_string(want) + ")");
        // the 7 groups of the SRS of 10 May 2024 (part I of the report, 00:00 UT)
        {
            int n = 0, hit = 0;
            const auto day = orbit::spotDayOf(2024, 5, 10);
            const auto rows = orbit::spotRowsOfDay(day);
            for (const std::string& l : block("sun/spots/noaa_srs_20240510.extract.txt", "part I")) {
                char ns, ew; int num, lat, cmd, lo, area;
                if (std::sscanf(l.c_str(), "%d %c%2d%c%2d %d %d", &num, &ns, &lat, &ew, &cmd, &lo, &area) != 7) continue;
                n++;
                for (const auto& r : rows) if (r[0] == (ns == 'N' ? lat : -lat) * 10 && r[1] == lo * 10 && r[2] == area) { hit++; break; }
            }
            const orbit::SpotDay sd = orbit::sunspotsAt(orbit::kSpotsDay0JD + day);
            check(n == 7 && hit == 7 && rows.size() == 7 && sd.status == orbit::SpotDay::Groups && sd.groups.size() == 7, "R3: 10 May 2024 00:00 UT has the report's 7 groups");
        }
        // (b) the places: a group fixed at its Carrington longitude on the Sun's IAU frame (orbit/spin.cpp) is seen from Earth where the record says
        // (its distance from the disc centre in disc radii and its central meridian distance, negative to the east). Earth's direction from Meeus's low-precision
        // Sun. The 1950 rows at their printed times; from 1992 at 24:00 UT of the printed day (REPORT 0.85.0).
        for (const char* y : {"1950", "2016"})
            for (int k = 1; k <= 3; k++) {
                const std::vector<std::string> b = block(std::string("sun/spots/g") + y + ".extract.txt", "row " + std::to_string(k));
                if (b.empty()) continue;
                const std::string& l = b[0];
                const int yy = std::stoi(l.substr(0, 4)), mm = std::stoi(l.substr(4, 2));
                const double dd = std::stod(l.substr(6, 2)) + std::stod(l.substr(8, 4)), dist = std::stod(l.substr(45, 5)), lon = std::stod(l.substr(57, 5)), lat = std::stod(l.substr(63, 5)), cmd = std::stod(l.substr(69, 5));
                const double jd = jdOf(yy, mm, yy >= 1992 ? std::floor(dd) + 1 : dd);
                orbit::SpinFrame F; orbit::spinFrame("Sun", jd, F);
                const double lam = (sunLongitudeJ2000(jd) + 180) * 3.14159265358979323846 / 180;
                const orbit::Vec3 e{std::cos(lam), std::sin(lam), 0};
                double v[3], rho;
                orbit::spotGeometry({lat, lon, 100}, v, rho);
                const orbit::Vec3 V = F.A * v[0] + F.B * v[1] + F.P * v[2];
                const double c = orbit::dot(V, e), dGot = std::sqrt(std::max(0.0, 1 - c * c));
                const double lE = std::atan2(orbit::dot(e, F.B), orbit::dot(e, F.A)) * 180 / 3.14159265358979323846, cGot = std::remainder(lon - lE, 360.0);
                const bool noaa = yy >= 1977;   // NOAA's places in whole degrees; Greenwich's in tenths
                check(std::fabs(dGot - dist) < (noaa ? 0.02 : 0.005) && std::fabs(cGot - cmd) < (noaa ? 1.5 : 0.3),
                      std::string("R3 (b): ") + y + " row " + std::to_string(k) + " drawn where the record says: distance " + std::to_string(dGot) + " (" + std::to_string(dist) + "), CMD " + std::to_string(cGot) + " (" + std::to_string(cmd) + ")");
            }
        // a group at the sub-Earth point is at the disc centre; one 90 degrees west of it is on the west limb
        {
            const double jd = jdOf(2024, 5, 10);
            orbit::SpinFrame F; orbit::spinFrame("Sun", jd, F);
            const double lam = (sunLongitudeJ2000(jd) + 180) * 3.14159265358979323846 / 180;
            const orbit::Vec3 e{std::cos(lam), std::sin(lam), 0};
            const double lE = std::atan2(orbit::dot(e, F.B), orbit::dot(e, F.A)) * 180 / 3.14159265358979323846, bE = std::asin(orbit::dot(e, F.P)) * 180 / 3.14159265358979323846;
            double v[3], rho;
            orbit::spotGeometry({bE, lE, 100}, v, rho);
            check(orbit::dot(F.A * v[0] + F.B * v[1] + F.P * v[2], e) > 1 - 1e-12, "R3 (b): CMD 0 at the sub-Earth latitude: the disc centre");
            orbit::spotGeometry({0, lE + 90, 100}, v, rho);
            const orbit::Vec3 V = F.A * v[0] + F.B * v[1] + F.P * v[2];
            check(std::fabs(orbit::dot(V, e)) < std::fabs(std::sin(bE * 3.14159265358979323846 / 180)) + 1e-9, "R3 (b): CMD 90 lies on the limb (to within the tilt B0 of the Sun's equator)");
            orbit::spotGeometry({0, 0, 1000}, v, rho);
            check(std::fabs(1 - std::cos(rho) - 1e-3) < 1e-15, "R3: a group's cap has its area (1 - cos rho = area x 1e-6)");
        }
        // (c) the sentences
        check(orbit::spotSentence(orbit::sunspotsAt(jdOf(1874, 6, 6.6))) == "No sunspot record for this date", "R3 (c): a missing day (MissingDays.txt, 1874-06-06) shows the sentence");
        check(orbit::spotSentence(orbit::sunspotsAt(jdOf(1850, 1, 1.5))) == "No sunspot record for this date", "R3 (c): before May 1874: no record");
        check(orbit::spotSentence(orbit::sunspotsAt(jdOf(2030, 1, 1.5))) == "No sunspot record for this date", "R3 (c): after the last record");
        check(orbit::sunspotsAt(jdOf(2019, 1, 1)).status == orbit::SpotDay::Spotless, "R3: 1 Jan 2019's report says None: a spotless day");
        check(orbit::sunspotsAt(jdOf(2019, 8, 4)).status == orbit::SpotDay::NoRecord, "R3: 4 Aug 2019's report was served empty: no record");
        // the invented patches are gone
        const std::string ov = readFile(kSrc + "/orbit/orbit_view.cpp");
        check(ov.find("invented places, turning as one solid body") == std::string::npos && ov.find("R.sunBall(") != std::string::npos, "R3: no invented patches on the Sun; the Sun is the measured ball");
    }
    // ---------------- R4: the Oort cloud ----------------
    {
        check(std::fabs(orbit::oortModelShare(orbit::kOortInnerAU, orbit::kOortOuterAU) - 1) < 1e-12 && orbit::oortModelShare(100, 1999) == 0, "R4: the model covers NASA's edges only");
        // the model's power laws, n(r) = dn/dr proportional to r^-alpha (Vokrouhlicky et al. 2019): the local slope from two thin bins in each part
        const auto nAt = [](double r) { const double h = r * 1e-4; return orbit::oortModelShare(r - h, r + h) / (2 * h); };
        for (const auto& pr : std::vector<std::array<double, 3>>{{4000, 9000, 0.72}, {20000, 40000, 1.35}, {60000, 90000, 1.5}}) {
            const double slope = std::log(nAt(pr[1]) / nAt(pr[0])) / std::log(pr[1] / pr[0]);
            check(std::fabs(slope + pr[2]) < 1e-6, "R4: n(r) falls as r^-" + std::to_string(pr[2]) + " between " + std::to_string(pr[0]) + " and " + std::to_string(pr[1]) + " au (" + std::to_string(slope) + ")");
        }
        for (double rb : {orbit::kOortModelBreak1AU, orbit::kOortModelBreak2AU}) {
            const double lo = nAt(rb * (1 - 1e-6) * 0.999999), hi = nAt(rb * 1.000001);
            check(std::fabs(lo / hi - 1) < 1e-3, "R4: n(r) is continuous at " + std::to_string(rb) + " au (the app's joining)");
        }
        for (double u : {0.0, 0.1, 0.5, 0.9, 1.0}) check(std::fabs(orbit::oortModelShare(orbit::kOortInnerAU, orbit::oortModelInverse(u)) - u) < 1e-12, "R4: the inverse of the model's share");
        // the dots against the model: the share of dots in distance bins within the binomial sampling error (4 sigma; the seed is fixed)
        const auto belts = orbit::makeBelts(0, 1);
        const orbit::BeltGroup* oort = nullptr;
        for (const auto& g : belts) if (g.key == "oort") oort = &g;
        check(oort && oort->n == 3000, "R4: the Oort cloud has its 3,000 dots");
        if (oort) {
            const double edges[] = {2000, 3000, 5000, 10000, 15000, 30000, 50000, 100000};
            const int nb = 7;
            std::vector<int> cnt(nb, 0);
            double zsum = 0;
            bool inside = true;
            for (int k = 0; k < oort->n; k++) {
                const double r = std::sqrt(double(oort->x[std::size_t(k)]) * oort->x[std::size_t(k)] + double(oort->y[std::size_t(k)]) * oort->y[std::size_t(k)] + double(oort->z[std::size_t(k)]) * oort->z[std::size_t(k)]) / units::kAU;
                inside = inside && r >= 2000 * (1 - 1e-6) && r <= 100000 * (1 + 1e-6);
                zsum += oort->z[std::size_t(k)] / (r * units::kAU);
                for (int i = 0; i < nb; i++) if (r >= edges[i] && r < edges[i + 1]) cnt[std::size_t(i)]++;
            }
            check(inside, "R4: every dot between NASA's edges");
            for (int i = 0; i < nb; i++) {
                const double p = orbit::oortModelShare(edges[i], edges[i + 1]), got = cnt[std::size_t(i)] / 3000.0, sig = std::sqrt(p * (1 - p) / 3000);
                check(std::fabs(got - p) < 4 * sig, "R4: dots between " + std::to_string(int(edges[i])) + " and " + std::to_string(int(edges[i + 1])) + " au: " + std::to_string(got) + " (model " + std::to_string(p) + ", sigma " + std::to_string(sig) + ")");
            }
            check(std::fabs(zsum / 3000) < 4 * std::sqrt(1.0 / 3 / 3000), "R4: the directions spread evenly (mean of sin(latitude) " + std::to_string(zsum / 3000) + ")");
        }
        // the realistic set shows no invented dots: the texts
        bool madeUp = false;
        for (const auto& l : orbit::beltLines("oort")) madeUp = madeUp || l.find("made up") != std::string::npos;
        check(!madeUp, "R4: the Oort cloud's card no longer says its dots are made up");
        bool says = false; for (const auto& l : orbit::beltLines("oort")) says = says || l.find("Estimated from models: no object in it has been seen") != std::string::npos;
        check(says, "R4: the card says \"Estimated from models: no object in it has been seen\"");
        check(orbit::dateNotes(false, false, false).find("Oort cloud dots are made up") == std::string::npos && orbit::dateNotes(false, false, false).find("published model") != std::string::npos,
              "R4: the date note no longer calls the Oort dots made up");
        // the three objects against JPL Horizons (heliocentric, ecliptic J2000): within 0.12 au in 1990 and 2060 (two-body motion; measured 0.08-0.10 au), within
        // 0.001 au on 2026-11-25 (near JPL's epoch)
        const char* files[3] = {"oort/horizons_90377_1990_2026_2060.extract.txt", "oort/horizons_2012VP113_1990_2026_2060.extract.txt", "oort/horizons_541132_1990_2026_2060.extract.txt"};
        for (int k = 0; k < 3; k++) {
            int rowsRead = 0;
            for (const std::string& l : block(files[k], "the rows")) {
                double jd, x, y, z;
                char date[64];
                if (std::sscanf(l.c_str(), "%lf, %63[^,], %lf, %lf, %lf", &jd, date, &x, &y, &z) != 5) continue;
                const orbit::Vec3 p = orbit::tnoPositionAU(k, jd);
                const double off = std::sqrt((p.x - x) * (p.x - x) + (p.y - y) * (p.y - y) + (p.z - z) * (p.z - z));
                const bool nearEpoch = std::fabs(jd - 2461369.5) < 1;
                check(off < (nearEpoch ? 0.001 : 0.12), "R4: object " + std::to_string(k) + " at JD " + std::to_string(jd) + " within " + (nearEpoch ? "0.001" : "0.12") + " au of JPL Horizons (" + std::to_string(off) + " au)");
                rowsRead++;
            }
            check(rowsRead == 3, "R4: Horizons' three rows read for object " + std::to_string(k));
        }
        // their cards carry the owner's sentence
        for (const char* key : {"sedna", "vp113", "tg387"}) {
            orbit::SolarSystem sys;
            orbit::FarItem f;
            const bool ok = orbit::farItem(sys, orbit::farIndexOf(key), f);
            orbit::Card c;
            if (ok) orbit::farCard(f, c);
            std::string all; for (const auto& l : c.lines) all += l + " ";
            check(ok && all.find("Called inner Oort cloud objects by their discoverers; JPL lists them as trans-Neptunian objects.") != std::string::npos, std::string("R4: ") + key + "'s card has the sentence");
        }
    }
}
