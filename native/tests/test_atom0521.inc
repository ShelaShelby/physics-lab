// 0.52.1: the spectral-line colours from the CIE 1931 observer (render/cie1931.h, atom_core.cpp lamColor), the quark mass bar's nucleon, the phase ring inside a nucleon.
#include "render/cie1931.h"
#include <fstream>
#include <regex>
#include <sstream>

// the CIE 1931 2-degree table as saved (colour-science 0.4.7, data/stars), read here again from the file, not from the app's generated copy
static std::map<int, std::array<double, 3>> savedCie1931() {
    std::map<int, std::array<double, 3>> rows;
    std::ifstream f(PL_DATA_DIR "/stars/colour-science_v0.4.7_cmfs.py");
    std::stringstream ss; ss << f.rdbuf();
    const std::string t = ss.str();
    const std::size_t a = t.find("\"CIE 1931 2 Degree Standard Observer\": {", t.find("DATA_CMFS_STANDARD_OBSERVER"));
    if (a == std::string::npos) return rows;
    const std::string part = t.substr(a, t.find('}', a) - a);
    const std::regex re("(\\d+): \\(([-0-9.e+]+), ([-0-9.e+]+), ([-0-9.e+]+)\\)");
    for (auto it = std::sregex_iterator(part.begin(), part.end(), re); it != std::sregex_iterator(); ++it)
        rows[std::stoi((*it)[1])] = {std::stod((*it)[2]), std::stod((*it)[3]), std::stod((*it)[4])};
    return rows;
}
static std::array<double, 2> xyOf(const std::array<double, 3>& X) { const double s = X[0] + X[1] + X[2]; return {X[0] / s, X[1] / s}; }
static std::array<double, 3> xyzFromLinear(const std::array<double, 3>& r) {   // the inverse of IEC 61966-2-1's matrix, by Cramer's rule
    const auto& M = cie::kSrgbFromXyz;
    const double det = M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) - M[0][1] * (M[1][0] * M[2][2] - M[1][2] * M[2][0]) + M[0][2] * (M[1][0] * M[2][1] - M[1][1] * M[2][0]);
    std::array<double, 3> X{};
    for (int c = 0; c < 3; c++) {
        double m[3][3];
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) m[i][j] = j == c ? r[i] : M[i][j];
        X[c] = (m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0])) / det;
    }
    return X;
}
// the dominant wavelength of a chromaticity: where the line from the D65 white point through it meets the spectral locus (the saved table, 1 nm, 360-700 nm,
// linear between rows; the angle around the white point is unwrapped along the locus)
static double dominantNm(const std::array<double, 2>& xy, const std::map<int, std::array<double, 3>>& T) {
    const auto w = xyOf(cie::d65WhiteXyz());
    std::vector<double> ang;
    for (int l = 360; l <= 700; l++) {
        const auto p = xyOf(T.at(l));
        double a = std::atan2(p[1] - w[1], p[0] - w[0]);
        if (!ang.empty()) while (a - ang.back() > kPi) a -= 2 * kPi;
        if (!ang.empty()) while (a - ang.back() < -kPi) a += 2 * kPi;
        ang.push_back(a);
    }
    const double a0 = std::atan2(xy[1] - w[1], xy[0] - w[0]);
    for (int k = -2; k <= 2; k++) {
        const double a = a0 + 2 * kPi * k;
        for (std::size_t i = 0; i + 1 < ang.size(); i++)
            if ((a - ang[i]) * (a - ang[i + 1]) <= 0 && ang[i] != ang[i + 1]) return 360 + i + (a - ang[i]) / (ang[i + 1] - ang[i]);
    }
    return NAN;
}
static double srgbDecode(double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }

static void testSpectralColours0521() {
    const auto T = savedCie1931();
    checkTrue("CIE 1931: the saved table is read (360-830 nm, 471 rows)", T.size() == 471 && T.begin()->first == 360 && T.rbegin()->first == 830);
    if (T.size() != 471) return;
    bool same = true;                                                      // the app's one copy is the saved table, row for row
    for (int l = 360; l <= 830; l++) { const auto c = cie::cmf(l); for (int j = 0; j < 3; j++) same = same && c[j] == T.at(l)[j]; }
    checkTrue("CIE 1931: the app's table equals the saved colour-science table at every 1 nm row", same);
    for (int l : {450, 500, 550, 600, 650}) {                              // xy chromaticity of single wavelengths against the table itself
        const auto a = xyOf(cie::cmf(l)), b = xyOf(T.at(l));
        checkNum("CIE xy at " + std::to_string(l) + " nm: x", a[0], b[0], 0, 1e-12);
        checkNum("CIE xy at " + std::to_string(l) + " nm: y", a[1], b[1], 0, 1e-12);
    }
    const auto mid = cie::cmf(550.5);
    checkNum("CIE: 550.5 nm is the mean of the 550 and 551 nm rows (ybar)", mid[1], 0.5 * (T.at(550)[1] + T.at(551)[1]), 0, 1e-15);
    // D65 white: the D65 spectrum through the observer and the sRGB matrix gives equal channels, so its displayable colour is white
    const auto wl = cie::linearSrgb(cie::d65WhiteXyz());
    checkTrue("D65 under the CIE 1931 observer: linear sRGB channels equal within 0.03 % (IEC 61966-2-1's 4-digit matrix)",
              std::fabs(wl[0] - 1) < 3e-4 && std::fabs(wl[1] - 1) < 3e-4 && std::fabs(wl[2] - 1) < 3e-4);
    const auto wxy = xyOf(cie::d65WhiteXyz());
    checkNum("D65 white point x (CIE 15:2004: 0.31272)", wxy[0], 0.31272, 0, 5e-5);
    checkNum("D65 white point y (CIE 15:2004: 0.32903)", wxy[1], 0.32903, 0, 5e-5);
    const double m = std::max({wl[0], wl[1], wl[2]});
    bool white = true;
    for (double v : wl) white = white && int(std::floor(255 * cie::srgbEncode(v / m) + 0.5)) == 255;
    checkTrue("D65 input, scaled to the brightest displayable value, is white (255, 255, 255)", white);
    // every visible line: inside the screen's colours (no channel below 0: one channel exactly 0, white mixed in only as far as needed) and at full brightness
    bool fits = true, full = true;
    for (double l = 380; l <= 780; l += 0.5) {
        const auto r = cie::spectralLinear(l);
        fits = fits && std::min({r[0], r[1], r[2]}) == 0;
        full = full && std::max({r[0], r[1], r[2]}) == 1;
        const Rgb c = lamColor(l);
        full = full && std::max({c.r, c.g, c.b}) == 255;
    }
    checkTrue("spectral colours 380-780 nm: every one has a channel exactly 0 (just enough white mixed in)", fits);
    checkTrue("spectral colours 380-780 nm: every one at its brightest displayable value (largest channel 1, 255)", full);
    // the dominant wavelength of each colour is its line's: the exact colour within 0.05 nm (380-690 nm; above 700 nm the CIE 1931 locus no longer moves:
    // x, y = 0.7347, 0.2653 to 4 digits, so a dominant wavelength there is not defined); the 8-bit colour the screen gets within 3 nm (420-660 nm: towards
    // the ends the locus moves so little per nm that one 8-bit step spans up to 2.5 nm, near 657 nm; within 1.3 nm from 420 to 640 nm)
    double worst = 0, worst8 = 0, at8 = 0;
    for (double l = 380; l <= 690; l += 0.7) {
        const double d = dominantNm(xyOf(xyzFromLinear(cie::spectralLinear(l))), T);
        worst = std::max(worst, std::isfinite(d) ? std::fabs(d - l) : 99.0);
        if (l >= 420 && l <= 660) {
            const Rgb c = lamColor(l);
            const double d8 = dominantNm(xyOf(xyzFromLinear({srgbDecode(c.r / 255.0), srgbDecode(c.g / 255.0), srgbDecode(c.b / 255.0)})), T);
            const double e8 = std::isfinite(d8) ? std::fabs(d8 - l) : 99.0;
            if (e8 > worst8) { worst8 = e8; at8 = l; }
        }
    }
    checkTrue("spectral colours: the dominant wavelength is the line's within 0.05 nm (380-690 nm; worst " + fmt::fixed(worst, 3) + " nm)", worst < 0.05);
    checkTrue("spectral colours, 8-bit: the dominant wavelength is the line's within 3 nm (420-660 nm; worst " + fmt::fixed(worst8, 3) + " nm at " + fmt::fixed(at8, 1) + " nm)", worst8 < 3);
    // hue along the spectrum: blue, then green, then red as the wavelength rises; the hue angle (HSV, red 0, green 120, blue 240, wrapped into (-60, 300]) never rises
    auto hue = [](const std::array<double, 3>& r) {
        const double mx = std::max({r[0], r[1], r[2]}), d = mx - std::min({r[0], r[1], r[2]});
        double h = mx == r[0] ? 60 * std::fmod((r[1] - r[2]) / d + 6, 6.0) : mx == r[1] ? 60 * ((r[2] - r[0]) / d + 2) : 60 * ((r[0] - r[1]) / d + 4);
        return h > 300 ? h - 360 : h;
    };
    bool down = true; double last = 1e9;
    for (double l = 380; l <= 780; l += 1) { const double h = hue(cie::spectralLinear(l)); down = down && h <= last + 1e-4; last = h; }   // (above 700 nm the table's ratios are constant to its digits: the hue wobbles by 6e-6 degrees, rounding only)
    checkTrue("spectral colours: the hue never rises with the wavelength (violet, blue, green, yellow, red)", down);
    auto biggest = [](double l) { const auto r = cie::spectralLinear(l); return r[2] >= r[0] && r[2] >= r[1] ? 2 : r[1] >= r[0] ? 1 : 0; };
    int band = 2; bool order = true;                                       // which channel is largest: blue, then green, then red, each once, in that order
    for (double l = 380; l <= 780; l += 1) { const int b = biggest(l); order = order && b <= band; band = b; }
    checkTrue("spectral colours: blue < green < red along the spectrum (H-beta 486 blue, 532 green, H-alpha 656 red)", order && biggest(486.1) == 2 && biggest(532) == 1 && biggest(656.3) == 0);
    checkTrue("outside the visible range the lines keep the one grey stand-in", lamColor(379.9).r == 0x9a && lamColor(780.1).b == 0xa3);
    checkTrue("the spectrum card says a screen cannot show pure spectral colours and what is done",
              std::string(spectralColourText()).find("cannot show") != std::string::npos && std::string(spectralColourText()).find("white") != std::string::npos);
}

static void testNucleonShown0521() {
    // the valence quarks' masses, typed in here again: PDG 2024 u 2.16, d 4.70 MeV/c^2; CODATA 2022 m_p 938.272 089 43, m_n 939.565 421 94 MeV/c^2
    checkNum("quark mass bar, proton (uud): 9.02 / 938.272 MeV", valenceQuarkShare(true), (2 * 2.16 + 4.70) / 938.27208943, 1e-12);
    checkNum("quark mass bar, neutron (udd): 11.56 / 939.565 MeV", valenceQuarkShare(false), (2.16 + 2 * 4.70) / 939.56542194, 1e-12);
    const std::string p = nucleonMassBarText(true), n = nucleonMassBarText(false);
    checkTrue("quark mass bar: a proton's says proton, a neutron's says neutron (and not proton)",
              p.find("proton") != std::string::npos && n.find("neutron") != std::string::npos && n.find("proton") == std::string::npos);
    atom::AtomLab L;
    bool rule = true;
    for (const char* layer : {"Atom", "Nucleus"}) { L.s.layer = layer; rule = rule && !L.phaseRingHidden(); }
    for (const char* layer : {"Nucleon", "Quarks"}) { L.s.layer = layer; rule = rule && L.phaseRingHidden(); }
    checkTrue("the phase ring shows at the Atom and Nucleus zooms and is hidden at the Nucleon and Quarks zooms", rule);
}

static void testOuterDenser0521() {
    // the outer electrons get a quarter of the dots plus their true share x of the rest: (1/4 + 3/4 x) / x times x
    checkNum("denser factor, sodium (1 outer electron of 11): 3.5 (as the alkali line since 0.48)", AtomLab::denserFactor(1, 11), 3.5, 1e-12);
    checkNum("denser factor, magnesium (2 outer electrons of 12): (0.25 + 0.75 / 6) * 6", AtomLab::denserFactor(2, 12), 2.25, 1e-12);
    AtomLab L; L.p = AtomParams(); L.reducedMotion = true; L.restart(); L.setParam("view", std::string("Cloud 3D")); L.setParam("Z", 12.0);
    bool said = false;
    for (const std::string& t : L.info()) said = said || (t.find("× denser than their true share, so their cloud and its jumps stand out") != std::string::npos);
    checkTrue("magnesium, a level atom: the main line says how much denser the outer electrons' dots are drawn and why", L.levelOn() && said);
}

static void testGlowCore0521() {
    // a cloud with a dense core: 400 dots on one point, 2,000 spread out; the 8-point-cell exposure leaves the core above 1 (clipped white), the cap brings it to 1
    std::vector<GlowDot> D;
    for (int i = 0; i < 400; i++) D.push_back({100.f, 100.f, 4.6f, 1.f, 0});
    for (int i = 0; i < 2000; i++) D.push_back({float(20 + (i * 37) % 160), float(20 + (i * 91) % 160), 4.6f, 0.5f, 0});
    DotColours dc = makeDotColours(true, false, false, Color{1, 1, 1, 1}, Color{1, 1, 1, 1});
    std::vector<render::Renderer2D::Blob> B0, B1;
    double e0 = 0, e1 = 0;
    glowBlobs(D, 12000, false, dc, false, 1.0, 200, 200, e0, B0);
    const std::vector<std::array<double, 2>> core{{100, 100}};
    glowBlobs(D, 12000, false, dc, false, 1.0, 200, 200, e1, B1, &core);
    checkTrue("glow core: without the cap the core is over the displayable range (clipped white)", glowSumAt(D, 12000, e0, 1.0, 100, 100) > 1.5);
    checkNum("glow core: with the cap the core is exactly at the brightest displayable value", glowSumAt(D, 12000, e1, 1.0, 100, 100), 1.0, 1e-9);
    checkTrue("glow core: one exposure for every dot, so the dots' relative brightness is kept", B0.size() == B1.size() && B1.size() > 2000 &&
              std::fabs(B1[0].c0.a / B1[2000].c0.a - B0[0].c0.a / B0[2000].c0.a) < 1e-6);
}
