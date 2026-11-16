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
    r.glow = true;                                       // ours, in place of the flare of Figure 16 / section 4.3.1
    r.fixedSpin = true; r.spin = c.spin;                 // section 4.1.1 p. 26
    r.rIn = c.rIn; r.rOut = c.rOut;                      // Figure 13's caption p. 24
    return r;
}

std::array<double, 3> whiteBalanceGains(double whiteT) {
    const std::array<double, 3> w = blackBodyLinear(whiteT);
    const double Y = visibleVsSun(whiteT);               // = 0.2126 R + 0.7152 G + 0.0722 B of w (IEC 61966-2-1's luminance row)
    return {Y / w[0], Y / w[1], Y / w[2]};
}

std::array<double, 3> filmDiscLinear() {
    const FilmChoices& c = filmChoices();
    const std::array<double, 3> g = whiteBalanceGains(c.whiteT), b = blackBodyLinear(c.discT);
    return {g[0] * b[0], g[1] * b[1], g[2] * b[2]};
}

double filmDiscY() {
    const std::array<double, 3> l = filmDiscLinear();
    return 0.2126 * l[0] + 0.7152 * l[1] + 0.0722 * l[2];   // IEC 61966-2-1: the luminance of linear sRGB
}

double glowSigmaBlocks(int H) { return kGlowSigma * H / kGlowBlock; }
int glowRadiusBlocks(int H) { return std::max(1, static_cast<int>(std::ceil(3 * glowSigmaBlocks(H)))); }

std::vector<double> glowReference(const std::vector<double>& lin, int W, int H) {
    const int gw = (W + kGlowBlock - 1) / kGlowBlock, gh = (H + kGlowBlock - 1) / kGlowBlock;
    std::vector<double> src(static_cast<std::size_t>(gw) * gh * 3, 0.0), tmp(src.size(), 0.0), blur(src.size(), 0.0);
    for (int by = 0; by < gh; by++)
        for (int bx = 0; bx < gw; bx++) {
            double acc[3] = {0, 0, 0};
            int n = 0;
            for (int y = by * kGlowBlock; y < std::min(H, (by + 1) * kGlowBlock); y++)
                for (int x = bx * kGlowBlock; x < std::min(W, (bx + 1) * kGlowBlock); x++) {
                    for (int c = 0; c < 3; c++) acc[c] += std::max(lin[(static_cast<std::size_t>(y) * W + x) * 3 + c] - kGlowThreshold, 0.0);
                    n++;
                }
            for (int c = 0; c < 3; c++) src[(static_cast<std::size_t>(by) * gw + bx) * 3 + c] = acc[c] / n;
        }
    const double sg = glowSigmaBlocks(H);
    const int R = glowRadiusBlocks(H);
    std::vector<double> w(static_cast<std::size_t>(2 * R + 1));
    double sum = 0;
    for (int k = -R; k <= R; k++) sum += (w[static_cast<std::size_t>(k + R)] = std::exp(-0.5 * k * k / (sg * sg)));
    for (double& v : w) v /= sum;
    for (int y = 0; y < gh; y++)                         // rows
        for (int x = 0; x < gw; x++)
            for (int k = -R; k <= R; k++) {
                const int xx = x + k;
                if (xx < 0 || xx >= gw) continue;
                for (int c = 0; c < 3; c++) tmp[(static_cast<std::size_t>(y) * gw + x) * 3 + c] += w[static_cast<std::size_t>(k + R)] * src[(static_cast<std::size_t>(y) * gw + xx) * 3 + c];
            }
    for (int y = 0; y < gh; y++)                         // columns
        for (int x = 0; x < gw; x++)
            for (int k = -R; k <= R; k++) {
                const int yy = y + k;
                if (yy < 0 || yy >= gh) continue;
                for (int c = 0; c < 3; c++) blur[(static_cast<std::size_t>(y) * gw + x) * 3 + c] += w[static_cast<std::size_t>(k + R)] * tmp[(static_cast<std::size_t>(yy) * gw + x) * 3 + c];
            }
    std::vector<double> out(lin.size(), 0.0);            // bilinear between the blocks' centres (indices clamped at the picture's edges)
    for (int y = 0; y < H; y++) {
        const double qy = (y + 0.5) / kGlowBlock - 0.5;
        const int y0 = static_cast<int>(std::floor(qy));
        const double fy = qy - y0;
        const int ya = std::clamp(y0, 0, gh - 1), yb = std::clamp(y0 + 1, 0, gh - 1);
        for (int x = 0; x < W; x++) {
            const double qx = (x + 0.5) / kGlowBlock - 0.5;
            const int x0 = static_cast<int>(std::floor(qx));
            const double fx = qx - x0;
            const int xa = std::clamp(x0, 0, gw - 1), xb = std::clamp(x0 + 1, 0, gw - 1);
            for (int c = 0; c < 3; c++) {
                const auto B = [&](int xx, int yy) { return blur[(static_cast<std::size_t>(yy) * gw + xx) * 3 + c]; };
                out[(static_cast<std::size_t>(y) * W + x) * 3 + c] = (1 - fy) * ((1 - fx) * B(xa, ya) + fx * B(xb, ya)) + fy * ((1 - fx) * B(xa, yb) + fx * B(xb, yb));
            }
        }
    }
    return out;
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
        {"No lens: nothing between the light and the eye", "Lens flare from the measured blur of the IMAX lenses (a soft glow); in the app: our own glow, in place of the film's lens flare",
         "Film audiences expect pictures through a real camera, for continuity with filmed shots", "Section 4.2 (p. 28), section 4.3.1 (p. 29), Figure 16's caption (p. 28)"},
    };
}

}  // namespace blackhole
