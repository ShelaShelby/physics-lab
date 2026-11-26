// Black Hole Lab (0.55.3): the looks. Sources and every number's place in the paper: looks.h.
#include "blackhole/looks.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "blackhole/disc.h"

namespace blackhole {

const char* lookKey(Look l) { return l == Look::Film ? "film" : "real"; }
bool lookFromKey(const std::string& k, Look& out) {
    if (k == "real") { out = Look::Real; return true; }
    if (k == "film") { out = Look::Film; return true; }
    return false;
}

const FilmChoices& filmChoices() {
    static const FilmChoices c;
    return c;
}

LookRule lookRule(Look l) {
    LookRule r;
    r.look = l;
    if (l != Look::Film) return r;
    const FilmChoices& c = filmChoices();
    r.shifts = false;                                    // Figure 15a, Figure 16's caption
    r.oneTemperature = true; r.discT = c.discT;          // section 4.1.1 p. 26
    r.whiteBalance = true; r.whiteT = c.whiteT;          // App. A.6 p. 43
    r.fixedSpin = true; r.spin = c.spin;                 // section 4.1.1 p. 26
    r.rIn = c.rIn; r.rOut = c.rOut;                      // Figure 13's caption p. 24
    return r;
}

std::array<double, 3> whiteBalanceGains(double whiteT) {
    const std::array<double, 3> w = blackBodyLinear(whiteT);
    const double Y = visibleVsSun(whiteT);               // = 0.2126 R + 0.7152 G + 0.0722 B of w (IEC 61966-2-1's luminance row)
    return {Y / w[0], Y / w[1], Y / w[2]};
}

// 0.55.5: worked out once (the film's numbers are fixed): the exposure's scan asks for them at every point of the disc, and three black-body integrals per
// point made the Film look's scan take seconds (the owner's 1 frame per second). The same numbers as before, bit for bit.
std::array<double, 3> filmDiscLinear() {
    static const std::array<double, 3> v = [] {
        const FilmChoices& c = filmChoices();
        const std::array<double, 3> g = whiteBalanceGains(c.whiteT), b = blackBodyLinear(c.discT);
        return std::array<double, 3>{g[0] * b[0], g[1] * b[1], g[2] * b[2]};
    }();
    return v;
}

double filmDiscY() {
    static const double y = [] {
        const std::array<double, 3> l = filmDiscLinear();
        return 0.2126 * l[0] + 0.7152 * l[1] + 0.0722 * l[2];   // IEC 61966-2-1: the luminance of linear sRGB
    }();
    return y;
}


std::vector<FilmRow> filmRows() {
    const FilmChoices& c = filmChoices();
    char b[4][320];
    std::snprintf(b[0], sizeof b[0], "Spin %.1f (the story needs nearly %.0f: the paper's own disc pictures use %.3f)", c.spin, 1.0, c.paperSpin);
    std::snprintf(b[1], sizeof b[1], "One temperature, %.0f K all over; thin, not feeding; edges %.2f and %.2f GM/c² in the paper's disc figure", c.discT, c.rIn, c.rOut);
    std::snprintf(b[2], sizeof b[2], "The gas at about %.2f c, with gravity, shifts the light's frequency by about %.1f on the approaching side (bluer) and %.1f on the receding side (redder); the approaching side far brighter", c.gasSpeed, c.approachShift, c.recedeShift);
    std::snprintf(b[3], sizeof b[3], "A white balance that draws a %.0f K black body white", c.whiteT);
    return {
        {"A spinning black hole: its shadow flattened on one side, the disc off-centre", b[0],
         "The flattened edge, the extra images beside it and the off-centre disc \"would be too confusing for a mass audience\"", "Section 4.1.1 (p. 26), section 1.2 (p. 5), appendix A.3.1 (p. 39)"},
        {"A feeding disc, hottest near the hole (the standard thin disc)", b[1],
         "An artists' disc, \"very anemic\" so that people near it are not \"fried by X-rays and gamma-rays\"", "Section 4.1.1 (p. 26); the edges: Figure 13's caption (p. 24)"},
        {b[2], "No colour shift and no brightness shift",
         "The true disc \"is exceedingly lopsided, with the hole's shadow barely discernible\"", "Section 4.2 (pp. 26-28), sections 4.1.1-4.1.2, Figures 15a and 15c (p. 27)"},
        {"The eye's colours: true colour (the app's CIE 1931 observer, white D65)", b[3],
         "The film camera's colour (film-stock curves, which the paper does not give: the app uses the CIE observer with this gain)", "Appendix A.6 (p. 43)"},
        {"No lens: nothing between the light and the eye", "Lens flare from the measured blur of the IMAX lenses (a soft glow); not drawn in the app: the measured blur is not published",
         "Film audiences expect pictures through a real camera, for continuity with filmed shots", "Section 4.2 (p. 28), section 4.3.1 (p. 29), Figure 16's caption (p. 28)"},
    };
}

}  // namespace blackhole
