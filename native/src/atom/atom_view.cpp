#include "atom_view.h"
#include "chart_draw.h"
#include "cloud_dots.h"
#include "cloud_light.h"
#include "outline.h"
#include "phase_wheel.h"
#include "quark_light.h"
#include "radii.h"

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <cfloat>
#include <cmath>
#include <cstdio>

#include "imgui.h"
#include "orbit/format.h"
#include "ui/fonts.h"

namespace atom {

using render::Color;
using render::hexColor;
using render::rgba;
using Blob = render::Renderer2D::Blob;

Theme theme(bool light) {
    if (light) return {hexColor("#1c1c1a"), hexColor("#6b6b66"), hexColor("#2f6fed"), hexColor("#ffffff"), hexColor("#dcdcd6"), hexColor("#ffffff"), true};
    return {hexColor("#eeeeee"), hexColor("#9a9a94"), hexColor("#6c9cff"), hexColor("#1e1e21"), hexColor("#333338"), hexColor("#ffffff"), false};
}

Theme theme(const app::ThemeColors& c) {
    auto C = [](const app::Rgb& v) { return Color{v.r, v.g, v.b, 1}; };
    return {C(c.fg), C(c.muted), C(c.accent), C(c.card), C(c.line), C(c.onacc), c.light};
}

namespace {

double g_textScale = 1;   // Settings → Text size, for the labels drawn in the picture
const Color kPhaseNeg = hexColor("#f08a3c");       // the − half of the wave (the + half uses the accent colour)

Color withA(Color c, double a) { c.a = static_cast<float>(std::max(0.0, std::min(1.0, a)) * c.a); return c; }
Color rgb3(const double c[3], double a = 1) { return rgba(int(c[0]), int(c[1]), int(c[2]), float(std::max(0.0, a))); }
// "rgb(r,g,b)" or "#rrggbb" -> colour
Color cssColor(const std::string& s) {
    if (!s.empty() && s[0] == '#') return hexColor(s.c_str());
    int r = 128, g = 128, b = 128;
    std::sscanf(s.c_str(), "rgb(%d,%d,%d)", &r, &g, &b);
    return rgba(r, g, b);
}
double clamp01(double x) { return std::max(0.0, std::min(1.0, x)); }
// a canvas-like radial gradient blob with two colour stops (focal circle at (fx, fy), radius f0, in units of r)
Blob grad2(double x, double y, double r, Color c0, Color c1, double fx = 0, double fy = 0, double f0 = 0) {
    Blob b{};
    b.x = float(x); b.y = float(y); b.r = float(r); b.kind = 0; b.fx = float(fx); b.fy = float(fy); b.f0 = float(f0); b.s1 = 0.5f;
    b.c0 = c0; b.c2 = c1;
    b.c1 = {(c0.r + c1.r) / 2, (c0.g + c1.g) / 2, (c0.b + c1.b) / 2, (c0.a + c1.a) / 2};
    return b;
}
Blob grad3(double x, double y, double r, Color c0, double s1, Color c1, Color c2, double fx = 0, double fy = 0, double f0 = 0) {
    Blob b{};
    b.x = float(x); b.y = float(y); b.r = float(r); b.kind = 0; b.fx = float(fx); b.fy = float(fy); b.f0 = float(f0); b.s1 = float(s1);
    b.c0 = c0; b.c1 = c1; b.c2 = c2;
    return b;
}
Blob flat(double x, double y, double r, Color c) { Blob b{}; b.x = float(x); b.y = float(y); b.r = float(r); b.kind = 3; b.c0 = c; return b; }
Blob soft(double x, double y, double r, Color c) { Blob b{}; b.x = float(x); b.y = float(y); b.r = float(r); b.kind = 4; b.c0 = c; return b; }
Blob square(double x, double y, double half, Color c) { Blob b{}; b.x = float(x); b.y = float(y); b.r = float(half); b.kind = 2; b.c0 = c; return b; }

// Text drawn with Dear ImGui on top of the picture; y is the baseline, like the canvas fillText().
// halo: the canvas strokeText() edge the web draws under some labels (in the card colour).
struct Text {
    ImDrawList* dl;
    double ox, oy, s;
    ImFont* font(bool bold) const { return bold ? ui::fonts().bold : ui::fonts().regular; }
    double width(const std::string& t, double size, bool bold = false) const {
        return font(bold)->CalcTextSizeA(float(size * g_textScale * s), FLT_MAX, 0, t.c_str()).x / s;
    }
    // align: 0 left, 0.5 centre, 1 right. middle: y is the text's middle instead of its baseline.
    void draw(const std::string& t0, double x, double y, double size, Color c, double align = 0, const Color* halo = nullptr, bool bold = false, bool middle = false) const {
        if (c.a <= 0.003f) return;
        const std::string t = ui::displayable(t0, font(bold));
        const float px = float(size * g_textScale * s);
        ImFont* f = font(bold);
        const ImFontBaked* bk = f->GetFontBaked(px);
        const double w = width(t, size, bold);
        const float X = float(ox + (x - w * align) * s);
        const float Y = middle ? float(oy + y * s) - (bk->Ascent - bk->Descent) / 2 : float(oy + y * s) - bk->Ascent;
        if (halo) {
            const ImU32 hc = ImGui::ColorConvertFloat4ToU32(ImVec4(halo->r, halo->g, halo->b, halo->a * c.a));
            const float o = float(1.5 * s);
            for (int k = 0; k < 8; k++) dl->AddText(f, px, ImVec2(X + o * std::cos(k * 0.785398f), Y + o * std::sin(k * 0.785398f)), hc, t.c_str());
        }
        dl->AddText(f, px, ImVec2(X, Y), ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, c.a)), t.c_str());
    }
};

// A polyline, optionally dashed (dash lengths in points, as the canvas setLineDash)
struct Pen {
    render::Renderer2D& r;
    double width;
    Color c;
    double on = 0, off = 0;
    bool have = false;
    double px = 0, py = 0, pos = 0;
    void to(double x, double y) {
        if (have) {
            if (on <= 0) r.line(px, py, x, y, width, c);
            else {
                const double len = std::hypot(x - px, y - py), period = on + off;
                double s = 0;
                while (s < len) {
                    const double in = std::fmod(pos, period), step = std::min(len - s, in < on ? on - in : period - in);
                    if (in < on) r.line(px + (x - px) * s / len, py + (y - py) * s / len, px + (x - px) * (s + step) / len, py + (y - py) * (s + step) / len, width, c);
                    s += step; pos += step;
                }
            }
        }
        px = x; py = y; have = true;
    }
};

// the web's sptWrap: break text into lines no wider than maxW, at spaces
std::vector<std::string> wrap(const Text& T, const std::string& text, double maxW, double size) {
    std::vector<std::string> out;
    std::string line;
    std::size_t a = 0;
    for (;;) {
        const std::size_t b = text.find(' ', a);
        const std::string wd = text.substr(a, b == std::string::npos ? std::string::npos : b - a);
        const std::string t = line.empty() ? wd : line + " " + wd;
        if (T.width(t, size) > maxW && !line.empty()) { out.push_back(line); line = wd; } else line = t;
        if (b == std::string::npos) break;
        a = b + 1;
    }
    if (!line.empty()) out.push_back(line);
    return out;
}

// a deterministic "random" number from a seed (the web's rnd), for the flickering quark details
double rnd(double a) { const double x = std::sin(a * 127.1 + 311.7) * 43758.5453; return x - std::floor(x); }
double smooth(double x) { return x * x * (3 - 2 * x); }
const double (&kCCol)[3][3] = kColourCharge;    // colour charge: red, green, blue
const int kSwaps[6][2] = {{0, 1}, {1, 2}, {0, 1}, {1, 2}, {0, 1}, {1, 2}};      // gluon exchanges: two quarks trade colours
struct Perms { int p[7][3]; };
const Perms& perms() {
    static Perms P = [] {
        Perms o{};
        o.p[0][0] = 0; o.p[0][1] = 1; o.p[0][2] = 2;
        for (int k = 0; k < 6; k++) {
            for (int i = 0; i < 3; i++) o.p[k + 1][i] = o.p[k][i];
            std::swap(o.p[k + 1][kSwaps[k][0]], o.p[k + 1][kSwaps[k][1]]);
        }
        return o;
    }();
    return P;
}
void mixC(const double a[3], const double b[3], double f, double out[3]) { for (int i = 0; i < 3; i++) out[i] = a[i] + (b[i] - a[i]) * f; }
// the soft particle colours: 48 along the red-green-blue edges, plus a warm white, a dark grey and a light grey
struct Pal { double c[51][3]; };
const Pal& pal() {
    static Pal P = [] {
        Pal o{};
        const int pairs[3][2] = {{0, 1}, {1, 2}, {2, 0}};
        int n = 0;
        for (const auto& pr : pairs) for (int f = 0; f < 16; f++) mixC(kCCol[pr[0]], kCCol[pr[1]], f / 16.0, o.c[n++]);
        const double extra[3][3] = {{255, 244, 225}, {40, 40, 44}, {238, 238, 238}};
        for (const auto& e : extra) { for (int i = 0; i < 3; i++) o.c[n][i] = e[i]; n++; }
        return o;
    }();
    return P;
}
int palIndex(const double c[3]) {
    int best = 0;
    double bd = 1e9;
    for (int i = 0; i < 48; i++) {
        const double* p = pal().c[i];
        const double d = (p[0] - c[0]) * (p[0] - c[0]) + (p[1] - c[1]) * (p[1] - c[1]) + (p[2] - c[2]) * (p[2] - c[2]);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}
// web: rgba(c, al) uses whole-number colour channels
Color rgbaC(const double c[3], double al) { return rgba(int(c[0]), int(c[1]), int(c[2]), float(std::max(0.0, std::min(1.0, al)))); }

// screen-oriented (x, y up, depth) of a point in the nucleus's frame
struct Proj {
    int mode;                  // 0: turnable (Cloud 3D, Orbit), 1: side view (x, z), 2: top view (x, y)
    double ca, sa, cb, sb;
    void operator()(double X, double Y, double Z3, double& x, double& y, double& dep) const {
        if (mode == 0) { const double x1 = X * ca - Y * sa, y1 = X * sa + Y * ca; x = x1; y = y1 * sb + Z3 * cb; dep = y1 * cb - Z3 * sb; }
        else if (mode == 1) { x = X; y = Z3; dep = 0; }
        else { x = X; y = Y; dep = 0; }
    }
};

// the slowly tumbling triangle of valence quarks and their colour charges (web: quarkPts)
struct QPt { char f; double x, y, dep; };
struct QuarkPts { double k, t; QPt P[3]; double cols[3][3]; double u; int kk; int sw[2]; };
QuarkPts quarkPts(const Nucleon& q, double X, double Y, double rn, double now, const Proj& proj) {
    QuarkPts Q{};
    Q.k = rn / kNucleonR; Q.t = now + q.ph;
    const char flav[3] = {'u', q.isP ? 'u' : 'd', 'd'};
    double rx[3], ry[3], rz[3];
    for (int i = 0; i < 3; i++) {
        const double t = Q.t, a = t * 0.6 + i * 2.094 + 0.5 * std::sin(t * 0.9 + i * 1.3), r = 0.30 + 0.12 * std::sin(t * 1.3 + i * 2.1);
        rx[i] = r * std::cos(a); ry[i] = r * std::sin(a); rz[i] = 0.22 * std::sin(t * 0.7 + i * 4.1);
    }
    const double mx = (rx[0] + rx[1] + rx[2]) / 3, my = (ry[0] + ry[1] + ry[2]) / 3, mz = (rz[0] + rz[1] + rz[2]) / 3;
    const double al = Q.t * 0.35, be = Q.t * 0.23, ca = std::cos(al), sa = std::sin(al), cb = std::cos(be), sb = std::sin(be);
    for (int i = 0; i < 3; i++) {   // slow tumble so every view shows the triangle
        const double x = rx[i] - mx, y = ry[i] - my, z = rz[i] - mz, y1 = y * ca - z * sa, z1 = y * sa + z * ca, x2 = x * cb - y1 * sb, y2 = x * sb + y1 * cb;
        double vx, vy, vd;
        proj(x2, y2, z1, vx, vy, vd);
        Q.P[i] = {flav[i], X + vx * Q.k, Y - vy * Q.k, vd};
    }
    // colour charge: every 1.1 s two quarks swap colours (a gluon passes between them)
    const double tt = (now + q.ph * 0.37) / 1.1, e = std::floor(tt);
    Q.u = tt - e;
    Q.kk = ((int(std::fmod(e, 6.0)) % 6) + 6) % 6;
    const double wgt = Q.u > 0.65 ? smooth((Q.u - 0.65) / 0.35) : 0;
    for (int i = 0; i < 3; i++) mixC(kCCol[perms().p[Q.kk][i]], kCCol[perms().p[Q.kk + 1][i]], wgt, Q.cols[i]);
    Q.sw[0] = kSwaps[Q.kk][0]; Q.sw[1] = kSwaps[Q.kk][1];
    return Q;
}


// ---- the cloud's Glow style ----
// One dot of the cloud on screen: its place, size, depth weight (1 = the plain dot) and the colour group (sign or phase) of the wave there.
struct GlowDot { float x, y, r, wt; unsigned char g; };   // g: the dot's colour group (Sign: 0 = +, 1 = −; Wheel: the phase bin)

// Draws the dots as soft round dots. Dark pages: the light adds up (additive), so dense places glow and thin ones fade. Light pages: ordinary
// alpha blending with darker, more saturated colours (opacity builds up as 1 - exp(-light)). `base` is the brightest a single dot may be.
// The exposure is chosen from the brightest 8 × 8 pixel place so it stays just under white; it is one number for all dots.
void drawGlow(render::Renderer2D& R, const std::vector<GlowDot>& D, long cap, bool light, DotColours dc, bool darken, double base, double w, double h, double& expo) {
    if (D.empty()) return;
    constexpr int kCell = 8;
    const int gw = int(w / kCell) + 2, gh = int(h / kCell) + 2;
    static std::vector<float> grid;
    grid.assign(std::size_t(gw) * gh, 0.f);
    for (const GlowDot& d : D) {                                           // the light one dot puts into its cell: its depth weight × footprint (pi r² / 4 of a Gaussian)
        const int cx = std::max(0, std::min(gw - 1, int(d.x / kCell))), cy = std::max(0, std::min(gh - 1, int(d.y / kCell)));
        float& g = grid[std::size_t(cy) * gw + cx];
        g += d.wt * d.r * d.r * 0.785f;
    }
    static std::vector<float> used;                                        // the brightest places, ignoring the top 0.5 % of cells (single random clumps)
    used.clear();
    for (float g : grid) if (g > 0) used.push_back(g);
    const std::size_t top = used.size() - 1 - std::min<std::size_t>(used.size() - 1, used.size() / 200);
    std::nth_element(used.begin(), used.begin() + top, used.end());
    const float peak = used[top];
    const double summed = double(peak) / (kCell * kCell) * cloudDotWeight(cap);   // per unit of dot light, as if the cloud had 12,000 dots
    const double e = cloudExposure(summed, light ? 2.4 : 1.0, base);
    expo = expo <= 0 || e < expo ? e : std::exp(0.85 * std::log(expo) + 0.15 * std::log(e));   // never brighter than the picture can show; slowly brighter again
    if (light && darken) {                                                 // the Sign colours on a light page: darker, more saturated (the wheel has its own light-page colours)
        const Color cP = dc.c[0], cN = dc.c[1];
        dc.c[0] = Color{cP.r * 0.85f, cP.g * 0.85f, cP.b * 0.85f, 1}; dc.c[1] = Color{cN.r * 0.85f, cN.g * 0.62f, cN.b * 0.45f, 1};
    }
    static std::vector<Blob> B;
    B.clear();
    B.reserve(D.size());
    for (const GlowDot& d : D) B.push_back(softDot(dc, d.x, d.y, d.r, d.g, cloudDotLight(cap, expo, base, d.wt)));   // (GPU colours: the dot carries its group, the shader picks the colour)
    R.blobsPaletteTake(B, !light, dc.palette());
    B.clear();
}

}  // namespace

// ---------------------------------------------------------------- the frame
void AtomView::draw(render::Renderer2D& R, ImDrawList* dl, double ox, double oy, double uiScale, AtomLab& lab, double w, double h, const app::ThemeColors& colours, double textScale) {
    const AtomParams& p = lab.p;
    AtomState& s = lab.s;
    const Theme css = theme(colours);
    g_textScale = textScale;
    const Text T{dl, ox, oy, uiScale};
    lab.viewW = w; lab.viewH = h;
    R.rect(0, 0, w, h, css.card);                                          // the canvas background (the web: var(--card))

    if (lab.chartOn()) {                                                   // the chart of nuclides (0.11 stage 2): a map of all nuclides, not one atom
        struct ChartTextAdapter : ChartText {
            const Text& t;
            explicit ChartTextAdapter(const Text& tt) : t(tt) {}
            double width(const std::string& str, double size, bool bold) const override { return t.width(str, size, bold); }
            void draw(const std::string& str, double x, double y, double size, Color c, double align, bool bold) const override { t.draw(str, x, y, size, c, align, nullptr, bold, false); }
        } adapter(T);
        int hz = 0, hn = 0;                                                // the tile under the pointer, if the pointer is over the picture and no pop-up has it
        const ImVec2 mp = ImGui::GetMousePos();
        const double mx = (mp.x - ox) / uiScale, my = (mp.y - oy) / uiScale;
        const bool flat = s.chartView.flat();                              // exactly stage 2's picture: its own drawing and grid lookup; otherwise the landscape's blocks (topmost drawn block under the pointer)
        if (!ImGui::GetIO().WantCaptureMouse && mx >= 0 && my >= 0 && mx < w && my < h) { if (flat) chartTileAt(chartLayout(w, h), mx, my, hz, hn); else lab.chartScene().pick(mx, my, hz, hn); }
        const ChartColours chartColours{css.fg, css.muted, css.accent, css.card, css.line};
        const ChainOverlay chainOv{s.showChain ? &lab.decayGraph() : nullptr, &s.walk};   // the decay chain of the chosen nuclide (0.12 stage 1): the graph is built once per chosen nuclide
        if (!lab.gpuChart) chartCache_.release(R);                         // (switched off: the GPU's copy is let go)
        ChartCache* cache = lab.gpuChart ? &chartCache_ : nullptr;         // 0.13 stage 4 part B: the tiles / blocks and the ground come from a GPU buffer made once; nullptr: made every frame as before
        if (flat) drawChart(R, adapter, chartColours, w, h, p.Z, p.N, hz, hn, s.showChain ? &chainOv : nullptr, cache);
        else drawChartScene(R, adapter, chartColours, lab.chartScene(), w, h, p.Z, p.N, hz, hn, s.showChain ? &chainOv : nullptr, cache);
        if (hz) {                                                          // the details of the square under the pointer: a tooltip (it follows the pointer, stays on screen and never hides a square)
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26);
            const std::vector<std::string> info = chartHover(hz, hn);
            for (const std::string& line : info) ImGui::TextUnformatted(ui::displayable(line, ImGui::GetFont()).c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        s.hits.clear(); s.layer = "Atom";
        return;
    }

    const bool cloud = p.view != "Orbit", d3 = p.view == "Cloud 3D", sl = p.view == "Slice", hz = sl && p.cutAxis == "Horizontal";
    const double cx0 = w / 2, cy0 = h / 2, m = std::min(w, h);
    const double pxm = lab.pxPerM(m, s.szCur) * std::pow(10, p.zoom);    // points per metre: auto-fit × zoom
    const double px = pxm * kA0;                                           // points per Bohr radius (the web's px)
    const bool isOrb = p.view == "Orbit", bohr = isOrb && p.ostyle == "Bohr 1913";
    const double yw = isOrb ? s.oyaw : s.yaw, pt = isOrb ? s.opitch : s.pitch;   // the Orbit view has its own angles
    const double ca = std::cos(yw), sa = std::sin(yw), cb = std::cos(pt), sb = std::sin(pt);
    lab.ensureNucleus();
    if (quarkKey_ != s.nucKey) { quarkKey_ = s.nucKey; qset_.clear(); qf_.clear(); }
    const double fmPx = pxm * kFmLab;                                     // points per fm
    const double now = lab.nowMs() / 1000;
    s.hits.clear();
    const Proj proj{(d3 || isOrb) ? 0 : (cloud && !hz ? 1 : 2), ca, sa, cb, sb};
    double fpx, fpy, fpd;
    proj(s.fc.x, s.fc.y, s.fc.z, fpx, fpy, fpd);                           // the camera follows the focused nucleon
    const double cx = cx0 - fpx * pxm + s.pan.x * pxm, cy = cy0 + fpy * pxm + s.pan.y * pxm;
    std::vector<Blob>& B = blobs_;                                         // kept from frame to frame (0.13 stage 4 part C): the 48,000 blobs of a big cloud are not allocated again every frame
    B.clear();

    if (cloud) {
        const long cap = std::min<long>(lab.cloudCap(), long(s.sg.size()));
        long cnt = std::min<long>(s.k, cap);
        const bool jump = lab.jumpDrawn();                                 // a photon jump: the old dots and the new ones are drawn together, each shown with its own odds (while its dots are still being built, in the first few frames, the cloud is drawn as it was)
        static std::vector<float> allPts;
        const float* PTS = s.pts.data();
        if (jump) {
            const long jn = lab.jumpDots();
            allPts.assign(s.pts.begin(), s.pts.begin() + 3 * jn); allPts.insert(allPts.end(), s.jp.begin(), s.jp.begin() + 3 * jn);
            PTS = allPts.data(); cnt = 2 * jn;
        }
        const bool glow = p.cstyle == "Glow";
        std::vector<GlowDot> G;
        G.reserve(glow ? std::size_t(cnt) : 0);
        const Color cP = css.accent, cN = kPhaseNeg;
        const bool wheel = lab.usesWheel();                                // Phase colours: Wheel (or a complex state) colours each dot by its phase, Sign by the sign of the wave
        const DotColours dc = makeDotColours(lab.gpuColours, wheel, css.light, cP, cN);   // GPU colours On (the default): a dot carries its colour group; Off: the 0.12 way, every dot carries its colour
        const int ng = dc.n;
        static std::vector<unsigned char> grp;                             // each dot's colour group
        grp.resize(std::size_t(cnt));
        // each dot's colour group: a jump in flight, a superposition (thinned and coloured from the CURRENT beat phase) or a plain cloud. 0.13 stage 2: on a PC with OpenGL 4.3 (and the
        // "GPU superposition and jumps" switch on) the first two come from a compute shader, else from the CPU functions, which are the reference (255 = not shown now)
        fillGroups(lab, R.compute(), cnt, grp.data());
        const bool outlineOn = p.outline == "On" && !lab.superOn() && lab.outlineAlpha() > 0.01;         // (the 90 % outline is of one orbital: not drawn for a superposition)
        const int fo = lab.outlineStateId();                                // the state on screen (a complex state's own id), not the real fallback fullOrb()
        const double olA = lab.outlineAlpha();                             // the outline fades out as a jump leaves its orbital and back in after landing
        const Color olCol = withA(css.fg, 0.6 * olA);                            // the 90 % outline: thin, the theme's text colour (light on dark pages, dark on light ones)
        const double olUnit = kA0 / p.Z * pxm;                             // points per Bohr radius at Z = 1 in the outline's own units
        if (sl) {                                                          // slice: only dots inside a thin slab around the cut plane, seen face-on
            const double size = kExt[lab.sizeN()] / p.Z * kA0, off = lab.cutOffsetM(), th = 0.07 * size;
            for (long i = 0; i < cnt; i++) {
                const double dist = std::fabs((hz ? PTS[3 * i + 2] : PTS[3 * i + 1]) - off);
                if (dist > th || grp[std::size_t(i)] == 255) continue;
                const double X = cx + PTS[3 * i] * pxm, Y = cy - (hz ? PTS[3 * i + 1] : PTS[3 * i + 2]) * pxm;
                if (X < -2 || X > w || Y < -2 || Y > h) continue;
                if (glow) G.push_back({float(X), float(Y), 4.2f, float(1 - 0.6 * dist / th), grp[std::size_t(i)]});     // dots nearer the cut plane are brighter, as before
                else B.push_back(squareDot(dc, X - 1 + 1.25, Y - 1 + 1.25, 1.25, grp[std::size_t(i)], 0.6 * (1 - 0.6 * dist / th) * cloudDotWeight(cap)));
            }
            if (glow) drawGlow(R, G, cap, css.light, dc, !wheel, 3.0, w, h, cloudExpo_);
            else R.blobsPaletteTake(B, false, dc.palette());
            B.clear();
            if (outlineOn) {                                               // the contour |psi|^2 = c in the cut plane (cached until the orbital or the cut changes)
                const Outline* O = outlineReady(fo, lab.gpuOutline);       // 0.13 stage 3: nullptr while the GPU is still making the outline (the first time): nothing is drawn yet, and the frame does not wait
                if (O) {
                    const double off0 = p.cut * 0.5 * kExt[orbitals()[stateOrbital(fo)].n];
                    olSlice_.update(*O, fo, hz, off0, lab.gpuOutline);
                    if (olSlice_.shows(fo, hz))                            // (the contour of another orbital or plane is never drawn; the cut may be a frame behind while the GPU works)
                        for (const OutSeg2& g : olSlice_.segments()) R.line(cx + g.x0 * olUnit, cy - g.y0 * olUnit, cx + g.x1 * olUnit, cy - g.y1 * olUnit, 1.1, olCol);
                }
            }
            const double gx = w - 44, gy = h - 44;                         // axes of the cut plane
            R.line(gx, gy, gx + 26, gy, 1.5, css.muted);
            R.line(gx, gy, gx, gy - 26, 1.5, css.muted);
            T.draw("x", gx + 34, gy + 4, 12, css.muted, 0.5);
            T.draw(hz ? "y" : "z", gx, gy - 32, 12, css.muted, 0.5);
        } else if (!d3) {                                                  // side view: drop y, plot (x, z)
            // Sign: blue where the wave ψ is +, orange where it is −; Wheel: one colour per phase step. The dots are drawn group by group, each group in dot order:
            // the v0.12 scan (a pass over all dots for each colour) with GPU colours Off, a counting sort with them On: the same list.
            static std::vector<SideDot> side;
            sideDots(PTS, cnt, grp.data(), ng, cx, cy, pxm, w, h, lab.gpuColours, side);
            if (glow) {
                for (const SideDot& d : side) G.push_back({d.x, d.y, 4.4f, 1.f, d.g});
                drawGlow(R, G, cap, css.light, dc, !wheel, 1.0, w, h, cloudExpo_);
            } else {
                sideBlobs(side, dc, 0.3 * cloudDotWeight(cap), B);               // one reserve, no regrowing (0.13 stage 4 part C)
                R.blobsPaletteTake(B, false, dc.palette());
            }
            B.clear();
            if (outlineOn)                                                 // the shadow of the 90 % region seen from the side
                if (const Outline* O = outlineReady(fo, lab.gpuOutline))
                for (const OutSeg2& g : O->side) R.line(cx + g.x0 * olUnit, cy - g.y0 * olUnit, cx + g.x1 * olUnit, cy - g.y1 * olUnit, 1.1, olCol);
        } else {                                                           // rotate by yaw (about z), then pitch, then project
            const double ext = kExt[lab.sizeN()] / p.Z * kA0;
            static std::vector<float> PX, PY, PD;
            projectCloud(PTS, cnt, CloudCamera{cx, cy, pxm, ca, sa, cb, sb, ext}, w, h, PX, PY, PD);   // PD: the depth over the cloud's radius; NaN = off the picture
            if (glow) {
                // one pass: size and brightness change smoothly with depth (far = smaller, fainter; near = bigger, brighter), no bands
                for (long i = 0; i < cnt; i++) {
                    const float v = PD[i];
                    if (std::isnan(v) || grp[std::size_t(i)] == 255) continue;
                    const float t = std::max(-1.f, std::min(1.f, v));          // -1 nearest, +1 farthest
                    const float near = 0.5f - 0.5f * t;                        // 1 near … 0 far
                    G.push_back({PX[i], PY[i], 4.6f * (0.8f + 0.4f * near), 0.3f + 0.9f * near, grp[std::size_t(i)]});
                }
                drawGlow(R, G, cap, css.light, dc, !wheel, 1.0, w, h, cloudExpo_);
            } else {
            // three depth bands (far = faint and small, near = bright and larger: kDepthBand), drawn far to near, inside a band group by group, inside a group in dot order.
            // GPU colours Off: the v0.12 scan, a pass over all dots for each band and colour (72 for the Wheel); On: a counting sort, and the dots carry only their colour group.
            buildClassic3D(PX.data(), PY.data(), PD.data(), grp.data(), cnt, dc, cloudDotWeight(cap), B);
            R.blobsPaletteTake(B, false, dc.palette());
            B.clear();
            }
            const Outline* wireO = outlineOn ? outlineReady(fo, lab.gpuOutline) : nullptr;   // (nullptr while the GPU makes it the first time)
            if (wireO) {                                                   // a faint wireframe of the 90 % surface: its contour lines in planes across the region, turned with the cloud
                const double um = kA0 / p.Z;
                const auto pr = [&](double x, double y, double z, double& X, double& Y, double& t) {
                    x *= um; y *= um; z *= um;
                    const double x1 = x * ca - y * sa, y1 = x * sa + y * ca, dep = y1 * cb - z * sb, up = y1 * sb + z * cb, f = 1 / (1 + dep / (4 * ext));
                    X = cx + x1 * pxm * f; Y = cy - up * pxm * f; t = std::max(-1.0, std::min(1.0, dep / ext));
                };
                for (const OutSeg3& g : wireO->wire) {
                    double X0, Y0, t0, X1, Y1, t1;
                    pr(g.x0, g.y0, g.z0, X0, Y0, t0); pr(g.x1, g.y1, g.z1, X1, Y1, t1);
                    R.line(X0, Y0, X1, Y1, 0.9, withA(css.fg, olA * (0.42 - 0.22 * 0.5 * (t0 + t1 + 2) / 2)));        // nearer lines are a little stronger
                }
            }
            const double gx = w - 44, gy = h - 44;                         // axis gizmo (bottom right)
            const double axes[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
            const char* names[3] = {"x", "y", "z"};
            for (int a = 0; a < 3; a++) {
                const double x1 = axes[a][0] * ca - axes[a][1] * sa, y1 = axes[a][0] * sa + axes[a][1] * ca, up = y1 * sb + axes[a][2] * cb;
                R.line(gx, gy, gx + 26 * x1, gy - 26 * up, 1.5, css.muted);
                T.draw(names[a], gx + 34 * x1, gy - 34 * up + 4, 12, css.muted, 0.5);
            }
        }
        if (wheel) {                                                       // the legend: a ring of the phase colours, with 0, pi/2, pi, 3 pi/2 (the phase of the wave, not a place)
            const double lx = w - 56, ly = 86, ro = 20;               // top right (bottom left has the ⓘ button and the scale bar, bottom right the axes)
            for (int i = 0; i < 72; i++) {
                const double a0 = 2 * kPi * i / 72, a1 = 2 * kPi * (i + 1.05) / 72;
                const PhaseRgb c = phaseRgb(a0 + kPi / 72, css.light);
                R.line(lx + ro * std::cos(a0), ly - ro * std::sin(a0), lx + ro * std::cos(a1), ly - ro * std::sin(a1), 6, Color{c.r, c.g, c.b, 1});
            }
            const struct { const char* t; double a; } marks[] = {{"0", 0}, {"π/2", kPi / 2}, {"π", kPi}, {"3π/2", 1.5 * kPi}};
            for (const auto& mk : marks) T.draw(mk.t, lx + 34 * std::cos(mk.a), ly - 34 * std::sin(mk.a) + 4, 11, css.muted, 0.5);
            T.draw("phase of ψ", lx, ly - 54, 11, css.muted, 0.5);
        }
    } else {                                                               // Orbit: the circle lies flat (x-y plane), projected so it can be turned
        auto op = [&](double x, double y, double& qx, double& qy) { const double x1 = x * ca - y * sa, y1 = x * sa + y * ca; qx = x1; qy = y1 * sb; };
        if (bohr) {                                                        // Bohr 1913: the allowed circles n = 1–4 and nothing else; the electron's is bold
            for (int n = 1; n <= 4; n++) {
                const double Rn = double(n) * n / p.Z * kA0;
                if (Rn * pxm > 4000) continue;
                const bool on = n == p.n;
                Pen pen{R, on ? 2.0 : 1.2, withA(on ? css.accent : css.muted, on ? 0.95 : 0.5)};
                for (int i = 0; i <= 120; i++) { const double t = i / 120.0 * kPi * 2; double qx, qy; op(Rn * std::cos(t), Rn * std::sin(t), qx, qy); pen.to(cx + qx * pxm, cy - qy * pxm); }
                double lqx, lqy;
                op(Rn * std::cos(2.35), Rn * std::sin(2.35), lqx, lqy);
                const double lx = cx + lqx * pxm, ly = cy - lqy * pxm - 4;
                if (lx > 24 && lx < w - 24 && ly > 16 && ly < h - 16) T.draw("n = " + std::to_string(n), lx, ly, 11, withA(on ? css.accent : css.muted, on ? 1 : 0.75), 0.5);
            }
        } else {
            const double R0 = double(p.n) * p.n / p.Z * kA0;
            if (R0 * pxm < 4000) {                                         // the orbit's circle, dashed
                Pen pen{R, 1, withA(css.muted, 0.25), 3, 5};
                for (int i = 0; i <= 96; i++) { const double t = i / 96.0 * kPi * 2; double qx, qy; op(R0 * std::cos(t), R0 * std::sin(t), qx, qy); pen.to(cx + qx * pxm, cy - qy * pxm); }
            }
            if (px < 2e5) {                                                // the trail
                Pen pen{R, 1.5, withA(css.accent, 0.7)};
                for (const auto& [x, y] : s.trail) { double qx, qy; op(x, y, qx, qy); pen.to(cx + qx * pxm, cy - qy * pxm); }
            }
        }
        if (std::fabs(s.oyaw) > 0.02 || std::fabs(s.opitch - kPi / 2) > 0.02) {   // turned away from straight down: show the axes
            const double gx = w - 44, gy = h - 44;
            double qx, qy;
            op(1, 0, qx, qy); R.line(gx, gy, gx + 26 * qx, gy - 26 * qy, 1.5, css.muted); T.draw("x", gx + 34 * qx, gy - 34 * qy + 4, 12, css.muted, 0.5);
            op(0, 1, qx, qy); R.line(gx, gy, gx + 26 * qx, gy - 26 * qy, 1.5, css.muted); T.draw("y", gx + 34 * qx, gy - 34 * qy + 4, 12, css.muted, 0.5);
            R.line(gx, gy, gx, gy - 26 * cb, 1.5, css.muted); T.draw("z", gx, gy - 34 * cb + 4, 12, css.muted, 0.5);
        }
    }
    // the electron (Orbit views)
    double eX = 0, eY = 0, eDep = 0;
    if (isOrb) { const double x1 = s.x * ca - s.y * sa, y1 = s.x * sa + s.y * ca; eX = x1; eY = y1 * sb; eDep = y1 * cb; }
    auto drawE = [&] {
        const double ex = cx + eX * pxm, ey = cy - eY * pxm;
        R.disc(ex, ey, bohr ? 6.5 : 5, css.accent);
        if (bohr) R.line(ex - 3, ey, ex + 3, ey, 1.6, css.onacc);         // the minus sign: the electron's negative charge
        if (bohr && s.flash) { const double u = std::min(1.0, *s.flash / 0.6); R.ring(ex, ey, 8 + 34 * u, 2.5, withA(css.accent, 0.85 * (1 - u))); }   // a flash where it lands
        T.draw(bohr ? "electron" : "Electron", ex, ey - (bohr ? 13 : 12), 12, css.fg, 0.5);
    };
    const bool eBehind = isOrb && eDep > 0;                                // on the far side of the nucleus: drawn first, so the nucleus covers it
    if (!cloud && eBehind) drawE();

    // the nucleus at its real size: a speck at atom scale, then protons and neutrons, then (deep zoom) quarks
    const double Rpx = nuclearRadius(p.Z, p.N) * fmPx, rn = kNucleonR * fmPx;
    const double qa = clamp01((rn - 40) / 120);                            // 0 = solid nucleon balls, 1 = full quark view
    s.layer = Rpx < 3 ? "Atom" : rn >= 110 ? "Quarks" : (p.Z + p.N > 1 ? Rpx < 0.45 * m : rn < 40) ? "Nucleus" : "Nucleon";
    if (bohr) {                                                            // Bohr 1913: the nucleus is one small positive ball, NOT to scale
        s.layer = "Atom";
        const double br = 7.5 + 2 * std::min(4.0, std::log2(p.Z + 1.0));
        R.disc(cx, cy, br, hexColor("#d63a58"));
        R.line(cx - br * 0.45, cy, cx + br * 0.45, cy, 1.8, rgba(255, 255, 255));
        R.line(cx, cy - br * 0.45, cx, cy + br * 0.45, 1.8, rgba(255, 255, 255));
        if (px / p.Z > 48) T.draw("nucleus  +" + std::to_string(p.Z), cx, cy + br + 15, 12, css.fg, 0.5);
    } else if (rn < 1.2) {
        R.disc(cx, cy, 1.6, css.fg);
    } else {
        struct Item { double x, y, bx, by, dep; bool isP; int i; double ph; bool q = false; double qf = 0; };
        std::vector<Item> list;
        for (int i = 0; i < int(s.nuc.size()); i++) {
            const Nucleon& q = s.nuc[i];
            const double j = 0.05 * std::sin(now * 3 + q.ph) * kFmLab;   // a small visual jitter (0.05 fm)
            Item it{};
            double d;
            proj(q.x + j, q.y + 0.7 * j, q.z - 0.5 * j, it.x, it.y, d);
            proj(q.x, q.y, q.z, it.bx, it.by, it.dep);                    // the true place: drawing order and quark choice do not flicker
            it.isP = q.isP; it.i = i; it.ph = q.ph;
            list.push_back(it);
        }
        std::stable_sort(list.begin(), list.end(), [](const Item& a, const Item& b) { return a.dep > b.dep; });   // far ones first
        int nq = 0;                                                        // quarks only for the 45 nearest on-screen nucleons
        if (qa > 0) {
            std::vector<const Item*> on;
            for (const Item& u : list) {
                const double X = cx + u.bx * pxm, Y = cy - u.by * pxm;
                if (X > -1.1 * rn && X < w + 1.1 * rn && Y > -1.1 * rn && Y < h + 1.1 * rn) on.push_back(&u);
            }
            std::reverse(on.begin(), on.end());                            // nearest first
            auto has = [](const std::vector<int>& v, int i) { return std::find(v.begin(), v.end(), i) != v.end(); };
            std::vector<int> next;
            for (std::size_t r = 0; r < on.size(); r++)                    // keep those that already show quarks while they stay near the front
                if (r < 60 && has(qset_, on[r]->i) && next.size() < 45) next.push_back(on[r]->i);
            if (s.focus >= 0 && std::any_of(on.begin(), on.end(), [&](const Item* u) { return u->i == s.focus; })) {   // the nucleon you dived into always shows its quarks
                if (next.size() >= 45 && !has(next, s.focus)) next.erase(next.begin());
                if (!has(next, s.focus)) next.push_back(s.focus);
            }
            for (const Item* u : on) { if (next.size() >= 45) break; if (!has(next, u->i)) next.push_back(u->i); }
            qset_ = next; nq = int(next.size());
            const double dq = qNow_ ? std::min(0.1, std::max(0.0, now - qNow_)) : 1, rate = 1 - std::exp(-7 * dq);   // quarks fade in and out (about 0.3 s)
            qNow_ = now;
            if (qf_.size() != s.nuc.size()) qf_.assign(s.nuc.size(), 0.f);
            for (Item& u : list) {
                const double t = has(next, u.i) ? 1 : 0, f = qf_[u.i] + (t - qf_[u.i]) * rate;
                qf_[u.i] = float((t == 0 && f < 0.02) ? 0 : f);
                if (qf_[u.i] > 0) { u.q = true; u.qf = qf_[u.i]; }
            }
        } else { qset_.clear(); qf_.clear(); qNow_ = 0; }

        const bool hard = p.style == "Hard balls", soft = rn >= 16 && (!hard || qa > 0);
        double dNear = INFINITY;                                           // the front-most nucleon you can see
        if (soft)
            for (const Item& u : list) {
                if (!u.q) continue;
                const double X = cx + u.x * pxm, Y = cy - u.y * pxm;
                if (X > -0.6 * rn && X < w + 0.6 * rn && Y > -0.6 * rn && Y < h + 0.6 * rn && u.dep < dNear) dNear = u.dep;
            }
        if (soft && s.focus >= 0)                                          // dived into a nucleon: the camera focuses on it
            for (const Item& u : list) if (u.i == s.focus && u.q) dNear = u.dep;
        const bool cineQ = p.qstyle == "Cinematic";
        const double DOF = (cineQ ? 1.5 : 2.2) * kFmLab;                   // past this depth behind the front, things go out of focus
        auto fogOf = [&](double d) {                                       // depth fog: quarks deeper in the nucleus fade back
            return soft && std::isfinite(dNear) ? (cineQ ? 0.3 : 0.5) + (cineQ ? 0.7 : 0.5) * std::exp(-std::max(0.0, d - dNear) / ((cineQ ? 1.1 : 1.8) * kFmLab)) : 1.0;
        };
        // Draw calls go in order; the soft layer collects the blurry things (drawn at half resolution)
        // and is laid over the picture before the sharp things.
        // Draw calls keep their order. The soft layer collects the blurry things (drawn at half
        // resolution) and is laid over the picture in one go, before the sharp things.
        std::vector<std::function<void()>> softOps;                        // what goes into the soft layer, in order
        struct Batch { std::vector<Blob> b; bool add = false; };
        Batch softB, mainB;
        auto closeSoft = [&] {
            if (softB.b.empty()) return;
            auto b = std::make_shared<Batch>(std::move(softB));
            softOps.push_back([&R, b] { R.blobs(b->b.data(), int(b->b.size()), b->add); });
            softB = Batch();
        };
        auto flushMain = [&] { if (!mainB.b.empty()) { R.blobs(mainB.b.data(), int(mainB.b.size()), mainB.add); mainB.b.clear(); } };
        struct Ctx { bool soft; };                                         // "a canvas": the main picture or the soft layer
        auto put = [&](const Ctx& c, const Blob& b, bool add = false) {
            Batch& t = c.soft ? softB : mainB;
            if (t.add != add && !t.b.empty()) { if (c.soft) closeSoft(); else flushMain(); }
            t.add = add;
            t.b.push_back(b);
        };
        auto lineTo = [&](const Ctx& c, double x0, double y0, double x1, double y1, double wd, Color col) {
            if (c.soft) { closeSoft(); softOps.push_back([&R, x0, y0, x1, y1, wd, col] { R.line(x0, y0, x1, y1, wd, col); }); }
            else { flushMain(); R.line(x0, y0, x1, y1, wd, col); }
        };
        auto ringTo = [&](const Ctx& c, double x, double y, double r, double wd, Color col) {
            if (c.soft) { closeSoft(); softOps.push_back([&R, x, y, r, wd, col] { R.ring(x, y, r, wd, col); }); }
            else { flushMain(); R.ring(x, y, r, wd, col); }
        };
        std::vector<std::pair<std::string, std::array<double, 4>>> letters;   // u/d letters: x, y, size, alpha (drawn as text on top)
        std::vector<Color> letterCols;
        auto letter = [&](char f, double x, double y, double size, double a, Color c) { letters.push_back({std::string(1, f), {x, y, size, a}}); letterCols.push_back(c); };

        const Ctx MAIN{false}, SOFT{true};
        if (soft && cineQ && qa > 0 && !css.light) {                       // Cinematic on a dark theme: one dark backdrop over the whole nucleus (light themes: none, see lightQuark)
            const double Rr = Rpx + 1.1 * rn;
            const Color dk = rgba(8, 10, 24, float(0.9 * qa));
            put(SOFT, grad3(cx, cy, Rr, dk, std::max(0.0, 1 - 1.1 * rn / Rr), dk, rgba(8, 10, 24, 0)));
        }
        auto ball = [&](const Ctx& c, double X, double Y, bool isP, double al) {
            put(c, grad2(X, Y, rn, withA(hexColor(isP ? "#ff9db0" : "#c4c4be"), al), withA(hexColor(isP ? "#d63a58" : "#7a7a75"), al), -0.3, -0.3, 0.1));
        };
        // Classic quarks: balls with flux tubes, gluon sparks and sea pairs (web: drawQuarks); only into the main picture or the soft layer as blobs
        auto classic = [&](const Ctx& c, const Nucleon& q, double X, double Y, double A) {
            const QuarkPts Q = quarkPts(q, X, Y, rn, now, proj);
            const double k = Q.k, t = Q.t;
            for (const QPt& o : Q.P) {                                     // flux tubes: a Y from each quark to the middle
                lineTo(c, o.x, o.y, X, Y, 0.16 * rn, withA(css.fg, 0.08 * A));
                lineTo(c, o.x, o.y, X, Y, std::max(1.5, 0.03 * rn), withA(css.fg, 0.35 * A));
                put(c, flat(o.x, o.y, std::max(1.5, 0.03 * rn) / 2, withA(css.fg, 0.35 * A)));   // round caps
            }
            if (p.detail != "Valence")                                     // gluon sparks running along the tubes
                for (int i = 0; i < 3; i++) {
                    const QPt& o = Q.P[i];
                    for (int n = 0; n < 7; n++) {
                        const double u = std::fmod(t * 0.9 + n / 7.0 + i * 0.13, 1.0), wob = std::sin(t * 9 + n * 5 + i * 3) * 0.05 * rn;
                        put(c, flat(o.x + (X - o.x) * u + wob, o.y + (Y - o.y) * u - 0.6 * wob, std::max(1.0, 0.012 * rn), withA(css.fg, 0.8 * A * std::sin(kPi * u))));
                    }
                }
            if (p.detail == "+ Sea")                                       // virtual quark–antiquark pairs flashing in and out
                for (int j = 0; j < 12; j++) {
                    const double cc = t * 0.8 + j * 0.379, cyc = std::floor(cc), ph = cc - cyc, env = std::pow(std::sin(kPi * ph), 2), sd = j * 91.7 + cyc * 13.13;
                    const double rr = 0.62 * std::cbrt(rnd(sd)), th = std::acos(1 - 2 * rnd(sd + 1)), az = 2 * kPi * rnd(sd + 2);
                    double c0x, c0y, c0d, dvx, dvy, dvd;
                    proj(rr * std::sin(th) * std::cos(az), rr * std::sin(th) * std::sin(az), rr * std::cos(th), c0x, c0y, c0d);
                    const double dth = std::acos(1 - 2 * rnd(sd + 3)), daz = 2 * kPi * rnd(sd + 4), d = 0.05 + 0.08 * ph;
                    proj(d * std::sin(dth) * std::cos(daz), d * std::sin(dth) * std::sin(daz), d * std::cos(dth), dvx, dvy, dvd);
                    const Color col = withA(hexColor(rnd(sd + 5) < 0.6 ? "#f0a500" : "#0e9488"), 0.8 * env * A);
                    const double r = std::max(1.5, 0.028 * rn);
                    put(c, flat(X + (c0x + dvx) * k, Y - (c0y + dvy) * k, r, col));      // quark: filled
                    ringTo(c, X + (c0x - dvx) * k, Y - (c0y - dvy) * k, r, 1.2, col);   // antiquark: a ring
                }
            int ord[3] = {0, 1, 2};                                        // the three valence quarks, far ones first
            std::stable_sort(ord, ord + 3, [&](int a, int b) { return Q.P[a].dep > Q.P[b].dep; });
            for (int i : ord) {
                const QPt& o = Q.P[i];
                const double r = std::max(4.0, 0.075 * rn);
                put(c, grad2(o.x, o.y, r, withA(hexColor(o.f == 'u' ? "#ffe08a" : "#8ff0e4"), A), withA(hexColor(o.f == 'u' ? "#f0a500" : "#0e9488"), A), -0.3, -0.3, 0.1));
                if (rn > 90 && !c.soft) letter(o.f, o.x, o.y - r - 4, std::max(12.0, std::min(20.0, 0.09 * rn)), A, css.fg);
            }
        };
        // On a light theme the colours of the quarks and their glow are made darker and more saturated (mixed 35 % with black) and drawn with ordinary
        // alpha blending, so they read on a light page with no dark backdrop; on a dark theme they are exactly as they were (cloud_light.h does the same for the cloud).
        const auto lightQuark = [&](const double in[3], double out[3]) { double t[3] = {in[0], in[1], in[2]}; if (css.light) lightQuarkColour(t, out); else for (int i = 0; i < 3; i++) out[i] = t[i]; };
        // Accurate / Cinematic quarks: colour charge, gluon exchange, a flickering gluon field (web: drawQuarksField).
        // phase 0 = 'all', 1 = 'soft' (blurry parts into sc), 2 = 'sharp' (crisp parts, faded by fog)
        auto field = [&](const Ctx& c, const Nucleon& q, double X, double Y, double A, int pb, int phase, const Ctx& sc, double fog) {
            const bool cine = p.qstyle == "Cinematic";
            const bool add = cine && !css.light;                           // Cinematic adds up light, which only shows on a dark page; on a light theme it is drawn with ordinary blending instead
            const QuarkPts Q = quarkPts(q, X, Y, rn, now, proj);
            const double k = Q.k, t = Q.t;
            if (phase != 2) {
                if (add) put(sc, grad3(X, Y, 1.05 * rn, rgba(8, 10, 24, float(0.9 * A)), 0.6, rgba(8, 10, 24, float(0.78 * A)), rgba(8, 10, 24, 0)));   // dark backdrop
                if (p.detail != "Valence") {                               // gluon field: a flickering cloud along the three tubes and round the middle
                    const int legN = int(std::lround(std::min(std::max(24.0, std::min(130.0, rn * 0.5)) * (cine ? 1.4 : 1), 0.85 * pb / 3)));
                    const int haloN = int(std::lround(std::min(46.0, 0.15 * pb)));
                    const double al0 = (cine ? 0.36 : 0.22) * A, sz0 = std::max(1.0, (cine ? 0.024 : 0.014) * rn);
                    struct FP { double x, y, a, s; int g; };
                    std::vector<FP> F;
                    for (int leg = 0; leg < 3; leg++) {
                        const QPt& o = Q.P[leg];
                        const double dx = X - o.x, dy = Y - o.y, L = std::hypot(dx, dy) > 0 ? std::hypot(dx, dy) : 1, nx = -dy / L, ny = dx / L;
                        for (int j = 0; j < legN; j++) {
                            const double sd = leg * 977 + j * 13.7 + q.ph * 31, cc = t * 0.9 + rnd(sd) * 4, cyc = std::floor(cc), ph = cc - cyc, s2 = sd + cyc * 17.3;
                            const double uu = rnd(s2), side = (rnd(s2 + 1) + rnd(s2 + 2) + rnd(s2 + 3) - 1.5) * (0.1 + 0.12 * std::sin(kPi * uu)) * rn;   // fatter in the middle of the tube
                            F.push_back({o.x + dx * uu + nx * side, o.y + dy * uu + ny * side, std::pow(std::sin(kPi * ph), 2) * al0 * (0.5 + 0.5 * rnd(s2 + 4)), sz0 * (0.6 + rnd(s2 + 5)), uu > 0.7 ? 3 : leg});
                        }
                    }
                    for (int j = 0; j < haloN; j++) {                      // a fainter cloud filling the whole nucleon
                        const double sd = 5000 + j * 11.3 + q.ph * 17, cc = t * 0.7 + rnd(sd) * 4, cyc = std::floor(cc), ph = cc - cyc, s2 = sd + cyc * 29.1;
                        const double rr = 0.3 * rn * std::sqrt(-2 * std::log(1 - 0.999 * rnd(s2))), an = 6.2832 * rnd(s2 + 1);
                        F.push_back({X + rr * std::cos(an), Y + rr * std::sin(an), std::pow(std::sin(kPi * ph), 2) * al0 * 0.6, sz0 * (0.8 + rnd(s2 + 2)), 3});
                    }
                    const int idx[4] = {palIndex(Q.cols[0]), palIndex(Q.cols[1]), palIndex(Q.cols[2]), add ? 48 : css.light ? 49 : 50};
                    for (int g = 0; g < 4; g++)
                        for (const FP& f : F) {
                            if (f.g != g) continue;
                            double pc[3];
                            lightQuark(pal().c[idx[g]], pc);
                            const Color c0 = rgb3(pc, f.a), c1 = rgb3(pc, 0.5 * f.a), c2 = rgb3(pc, 0);
                            put(sc, grad3(f.x, f.y, f.s * 1.7, c0, 0.4, c1, c2), add);
                        }
                }
                for (int i = 0; i < 3; i++) {                              // each quark's halo
                    const QPt& o = Q.P[i];
                    const double r = cine ? std::max(4.0, 0.06 * rn) : std::max(2.2, 0.03 * rn), Rr = (cine ? 5.5 : 4.5) * r;
                    double hc[3];
                    lightQuark(Q.cols[i], hc);
                    put(sc, grad2(o.x, o.y, Rr, rgbaC(hc, (cine ? 0.6 : 0.4) * A), rgbaC(hc, 0)), add);
                }
            }
            if (phase == 1) return;
            const double Af = A * fog;
            if (p.detail != "Valence" && Q.u > 0.5) {                      // the gluon carrying a colour from one quark to another
                const QPt &a = Q.P[Q.sw[0]], &b = Q.P[Q.sw[1]];
                const double f = (Q.u - 0.5) / 0.5;
                for (int mm = 0; mm < 6; mm++) {
                    const double ff = std::max(0.0, f - mm * 0.05);
                    double col[3];
                    mixC(kCCol[perms().p[Q.kk][Q.sw[0]]], kCCol[perms().p[Q.kk][Q.sw[1]]], ff, col);
                    lightQuark(col, col);
                    const double r = std::max(1.5, (cine ? 0.034 : 0.022) * rn) * (1 - mm * 0.12);
                    put(c, flat(a.x + (b.x - a.x) * ff, a.y + (b.y - a.y) * ff, r, rgbaC(col, Af * (1 - mm / 6.0))), add);
                }
            }
            if (p.detail == "+ Sea")                                       // sea pairs: a colour and its anticolour
                for (int j = 0; j < 12; j++) {
                    const double cc = t * 0.8 + j * 0.379, cyc = std::floor(cc), ph = cc - cyc, env = std::pow(std::sin(kPi * ph), 2), sd = j * 91.7 + cyc * 13.13;
                    const double rr = 0.62 * std::cbrt(rnd(sd)), th = std::acos(1 - 2 * rnd(sd + 1)), az = 2 * kPi * rnd(sd + 2);
                    double c0x, c0y, c0d, dvx, dvy, dvd;
                    proj(rr * std::sin(th) * std::cos(az), rr * std::sin(th) * std::sin(az), rr * std::cos(th), c0x, c0y, c0d);
                    const double dth = std::acos(1 - 2 * rnd(sd + 3)), daz = 2 * kPi * rnd(sd + 4), d = 0.05 + 0.08 * ph;
                    proj(d * std::sin(dth) * std::cos(daz), d * std::sin(dth) * std::sin(daz), d * std::cos(dth), dvx, dvy, dvd);
                    const double* col0 = kCCol[int(std::floor(rnd(sd + 5) * 3))];
                    const double anti0[3] = {255 - col0[0], 255 - col0[1], 255 - col0[2]};
                    double col[3], anti[3];
                    lightQuark(col0, col); lightQuark(anti0, anti);
                    const double r = std::max(1.5, (cine ? 0.026 : 0.02) * rn), al = 0.85 * env * Af;
                    put(c, flat(X + (c0x + dvx) * k, Y - (c0y + dvy) * k, r, rgbaC(col, al)), add);
                    const double rx = X + (c0x - dvx) * k, ry = Y - (c0y - dvy) * k;
                    if (add) {                                     // Cinematic adds light ("lighter"): the 1.2 px ring as an added-up blob, peaked on the circle
                        const Color ac = rgbaC(anti, al), R0 = ac.withAlpha(0);
                        const double Rb = r + 1.2;
                        put(c, grad3(rx, ry, Rb, R0, r / Rb, ac, R0, 0, 0, std::max(0.0, (r - 1.2) / Rb)), true);
                    } else ringTo(c, rx, ry, r, 1.2, rgbaC(anti, al));
                }
            int ord[3] = {0, 1, 2};                                        // the three valence quarks: points, far ones first
            std::stable_sort(ord, ord + 3, [&](int a, int b) { return Q.P[a].dep > Q.P[b].dep; });
            for (int i : ord) {
                const QPt& o = Q.P[i];
                double cc[3];
                lightQuark(Q.cols[i], cc);
                if (cine) {
                    const double r = std::max(4.0, 0.06 * rn);
                    double wh[3], dk[3];
                    const double white[3] = {255, 255, 255}, black[3] = {0, 0, 0};
                    mixC(cc, white, 0.85, wh); mixC(cc, black, 0.5, dk);
                    put(c, grad3(o.x, o.y, r, rgbaC(wh, Af), 0.5, rgbaC(cc, Af), rgbaC(dk, Af), -0.3, -0.3, 0.05));
                } else put(c, flat(o.x, o.y, std::max(2.2, 0.03 * rn), rgbaC(cc, Af)));
                if (rn > 90 && !c.soft) letter(o.f, o.x, o.y - std::max(4.0, 0.06 * rn) - 6, std::max(12.0, std::min(20.0, 0.09 * rn)), Af, add ? rgba(255, 255, 255) : css.fg);
            }
        };
        auto quarks = [&](const Ctx& c, const Nucleon& q, double X, double Y, double qai, int pb, double fg) {
            if (p.qstyle == "Classic") classic(c, q, X, Y, qai * fg);
            else field(c, q, X, Y, qai, pb, 0, c, fg);
        };
        struct Sharp { const Item* it; double X, Y, qai; int pb; double a0; };
        std::vector<Sharp> sharp;
        for (const Item& q : list) {
            const double X = cx + q.x * pxm, Y = cy - q.y * pxm;
            if (X < -2 * rn || X > w + 2 * rn || Y < -2 * rn || Y > h + 2 * rn) continue;   // off screen
            if (rn >= 5) s.hits.push_back({X, Y, rn, q.dep, q.i});
            const double qai = qa * q.qf, a0 = 0.93 - 0.72 * qai;
            const int pb = std::max(45, int(std::lround(700.0 / std::max(nq, 1))));   // its gluon-particle budget
            const bool far = soft && std::isfinite(dNear) && q.dep - dNear > DOF;    // out of focus: whole into the blurred layer
            const Nucleon& nu = s.nuc[q.i];
            if (rn < 5) {                                                  // tiny nucleons: a flat dot is enough
                put(MAIN, flat(X, Y, rn * 1.3, withA(hexColor(q.isP ? "#e0506a" : "#969692"), 0.9)));
            } else if (hard) {                                             // hard balls: sharp in focus, blurred out of focus
                if (!soft) { ball(MAIN, X, Y, q.isP, a0); if (q.q) quarks(MAIN, nu, X, Y, qai, pb, 1); }
                else if (far) { ball(SOFT, X, Y, q.isP, a0 * fogOf(q.dep)); if (q.q) quarks(SOFT, nu, X, Y, qai, pb, fogOf(q.dep)); }
                else sharp.push_back({&q, X, Y, qai, pb, a0});
                continue;
            } else {                                                       // fuzzy: the real density profile; overlapping nucleons add up
                const double ext = 1.9 * rn, af = 0.93 - 0.45 * qai;
                Blob b{};
                b.x = float(X); b.y = float(Y); b.r = float(ext); b.kind = 1;
                b.c0 = q.isP ? rgba(224, 80, 106, float(af)) : rgba(150, 150, 146, float(af));
                put(soft ? SOFT : MAIN, b);
            }
            if (!q.q) continue;
            if (!soft) quarks(MAIN, nu, X, Y, qai, pb, 1);
            else if (far) quarks(SOFT, nu, X, Y, qai, pb, fogOf(q.dep));
            else { if (p.qstyle != "Classic") field(MAIN, nu, X, Y, qai, pb, 1, SOFT, 1); sharp.push_back({&q, X, Y, qai, pb, 0}); }
        }
        // the soft layer goes on in one go (everything drawn into it so far), then the sharp things on top
        if (soft) {
            flushMain();
            closeSoft();
            R.beginSoftLayer(0.5f);
            for (const auto& op : softOps) op();
            R.endSoftLayer();
            for (const Sharp& sh : sharp) {
                const double fg = fogOf(sh.it->dep);
                const Nucleon& nu = s.nuc[sh.it->i];
                if (hard) { ball(MAIN, sh.X, sh.Y, sh.it->isP, sh.a0); if (sh.it->q) quarks(MAIN, nu, sh.X, sh.Y, sh.qai, sh.pb, fg); }
                else if (p.qstyle == "Classic") quarks(MAIN, nu, sh.X, sh.Y, sh.qai, sh.pb, fg);
                else field(MAIN, nu, sh.X, sh.Y, sh.qai, sh.pb, 2, MAIN, fg);
            }
        }
        flushMain();
        for (std::size_t i = 0; i < letters.size(); i++) {
            const auto& L = letters[i].second;
            T.draw(letters[i].first, L[0], L[1], L[2], withA(letterCols[i], L[3]), 0.5);
        }
    }
    if (s.layer == "Quarks" && qa >= 0.5) {                               // mass budget: the quarks' own masses are only about 1% of a nucleon's
        const double bw = std::min(230.0, 0.5 * w), bx = w - 12 - bw, by = h - 62;
        T.draw("mass of a proton: about 1% quark masses, 99% strong-force energy", w - 12, by - 6, 11, css.muted, 1, &css.card);
        R.rect(bx, by, bw, 8, css.line);
        R.rect(bx, by, bw, 8, withA(css.accent, 0.55));
        R.rect(bx, by - 2, std::max(2.0, 0.0096 * bw), 12, hexColor("#f0a500"));   // 9 of 938 MeV
    }
    if (!bohr && (s.layer == "Atom" || s.layer == "Nucleus")) {           // labels only while the whole nucleus is in view
        const double off = std::min(std::max(Rpx * 1.25, 3.0) + 12, h / 2 - 22);
        const Color olHalo = withA(css.card, 0.9);
        T.draw(nuclideLabel(p.Z, p.N) + "  ·  " + std::to_string(p.Z) + "p, " + std::to_string(p.N) + "n", cx, cy + off + 8, 12, css.fg, 0.5, &olHalo);   // with a thin edge in the page colour, so it reads over the cloud dots
    }
    {   // the ⓘ More / Less info button, bottom left above the scale bar
        const std::string t = lab.moreShown() ? "Less info" : "More info";
        const double ih = 28, ir = 8, bw = T.width(t, 13, true) + 2 * ir + 26, bx = 12, by = h - 40 - ih;
        s.infoBox = Box{bx, by, bw, ih};
        const auto P = [&](double x, double y) { return ImVec2(float(ox + x * uiScale), float(oy + y * uiScale)); };
        const auto U = [](Color c) { return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, c.a)); };
        dl->AddRectFilled(P(bx + 0.5, by + 0.5), P(bx + bw - 0.5, by + ih - 0.5), U(css.card), float(ih / 2 * uiScale));
        dl->AddRect(P(bx + 0.5, by + 0.5), P(bx + bw - 0.5, by + ih - 0.5), U(css.accent), float(ih / 2 * uiScale), 0, float(1.25 * uiScale));
        const double icx = bx + 10 + ir, icy = by + ih / 2;
        dl->AddCircle(P(icx, icy), float(ir * uiScale), U(css.accent), 0, float(1.6 * uiScale));                 // the circle
        dl->AddCircleFilled(P(icx, icy - 0.45 * ir), float(1.2 * uiScale), U(css.accent));                     // the dot of the i
        dl->AddLine(P(icx, icy - 0.05 * ir), P(icx, icy + 0.5 * ir), U(css.accent), float(1.6 * uiScale));      // its stem
        T.draw(t, icx + ir + 8, icy + 0.5, 13, css.accent, 0, nullptr, true, true);
    }
    if (!cloud && !eBehind) drawE();
    if (s.photon) {                                                        // the photon: a travelling wave packet (outward when emitted, inward when absorbed)
        const Photon& ph = *s.photon;
        const double u = std::min(1.0, ph.t / ph.dur), sg = ph.kind == "emit" ? 1 : -1;
        const double dx = std::cos(ph.ang), dy = std::sin(ph.ang);
        const double Rr = std::min((w / 2 - 30) / std::max(0.01, std::fabs(dx)), (h / 2 - 30) / std::max(0.01, std::fabs(dy)));   // to the picture's edge
        const double dd = (sg > 0 ? u : 1 - u) * Rr, nx = -dy, ny = dx, bx = cx0 + dx * dd, by = cy0 + dy * dd;
        const Color pc = cssColor(ph.col);
        const double passes[2][2] = {{7, 0.22}, {2.2, 0.95}};
        for (const auto& pa : passes) {
            Pen pen{R, pa[0], withA(pc, pa[1])};
            for (double k = -36; k <= 36; k += 1.5) {
                const double off = 8 * std::exp(-std::pow(k / 16, 2)) * std::sin((k - sg * ph.t * 70) / 1.9);
                pen.to(bx + dx * k + nx * off, by + dy * k + ny * off);
            }
        }
        const std::string tag = bandTag(ph.lam);
        const std::string ptxt = (ph.name.empty() ? "" : ph.name + "  ·  ") + fmtLam(ph.lam) + tag + "  ·  " + fmtEV(ph.E);
        const double hw = T.width(ptxt, 12) / 2 + 8;                       // keep the whole label on the picture
        T.draw(ptxt, std::max(hw, std::min(w - hw, bx + nx * 24)), std::max(96.0, std::min(h - 40, by + ny * 24)), 12, css.fg, 0.5);
    }
    if (!s.lines.empty()) {                                                // spectral lines seen so far, on a log wavelength strip
        bool wide = false;
        for (const SeenLine& L : s.lines) if (L.lam < 1) wide = true;
        const double sw = std::min(240.0, 0.4 * w), sx = w - 12 - sw - (d3 ? 70 : 0), sy = h - 22, L0 = wide ? std::log(0.005) : 0, LS = std::log(3000.0) - L0;
        T.draw(wide ? "spectral lines, 0.005–3000 nm (log)" : "spectral lines, 1–3000 nm (log)", sx, sy - 8, 11, css.muted);
        for (int i = 0; i < int(sw); i++) {
            const double lam = std::exp(L0 + i / sw * LS);
            const bool vis = isVisibleNm(lam);
            R.rect(sx + i, sy, 1, 10, vis ? cssColor(lamColorCss(lam)).withAlpha(0.85f) : withA(css.muted, 0.15));
        }
        for (const SeenLine& L : s.lines) R.rect(sx + clamp01((std::log(L.lam) - L0) / LS) * sw - 1, sy - 3, 2, 16, cssColor(L.col));
    }
    {   // radioactive decay: what flies out, and the recent decays (web: decDraw)
        const double nowMs = lab.nowMs();
        if (s.decFx) {
            const double u = (nowMs - s.decFx->t0) / 1600;
            if (u >= 1) s.decFx.reset();
            else {
                const double e = 1 - std::pow(1 - u, 3), Rr = e * 0.42 * std::min(w, h);
                R.ring(cx, cy, 6 + 60 * e, 2, withA(hexColor("#f0c040"), std::max(0.0, 1 - u * 1.4)));   // flash
                for (const FlyingShot& sh : s.decFx->shots) {
                    const double x = cx + std::cos(sh.a) * Rr, y = cy + std::sin(sh.a) * Rr, al = std::min(1.0, 2 * (1 - u));
                    const Color col = withA(hexColor(sh.q.col.c_str()), al);
                    Pen pen{R, 1, col, sh.q.label.rfind("ν", 0) == 0 ? 3.0 : 0.0, 3};
                    pen.to(cx + std::cos(sh.a) * Rr * 0.55, cy + std::sin(sh.a) * Rr * 0.55);
                    pen.to(x, y);
                    R.disc(x, y, sh.q.size, col);
                    T.draw(sh.q.label, x + sh.q.size + 3, y + 4, 12, withA(css.fg, al), 0, &css.card);
                }
            }
        }
        std::vector<const LogLine*> log;
        for (const LogLine& L : s.decLog) if (nowMs - L.t < 14000) log.push_back(&L);
        if (!log.empty()) {
            std::vector<std::pair<std::string, double>> lines;
            for (std::size_t i = log.size() > 3 ? log.size() - 3 : 0; i < log.size(); i++)
                for (const std::string& t : wrap(T, log[i]->txt, w - 24, 12)) lines.push_back({t, log[i]->t});
            const std::size_t first = lines.size() > 6 ? lines.size() - 6 : 0, n = lines.size() - first;
            for (std::size_t i = 0; i < n; i++) {
                const double y = h - 84 - double(n - 1 - i) * 16;          // above the ⓘ button and the scale bar
                T.draw(lines[first + i].first, 12, y, 12, withA(css.fg, std::min(1.0, (14000 - (nowMs - lines[first + i].second)) / 2000)), 0, &css.card);
            }
        }
    }
    {   // the scale bar: the largest 1-2-5 length that fits, in Å, pm or fm
        const double pxA = pxm * 1e-10;                                    // points per Å
        double bar = 1e-8;
        for (int e = -8; e <= 1; e++) for (int mm : {1, 2, 5}) if (mm * std::pow(10.0, e) * pxA <= 0.3 * w) bar = mm * std::pow(10.0, e);
        auto nf = [](double v) { return jsNum(v, 2); };
        const std::string label = bar >= 0.1 ? nf(bar) + " Å" : bar * 100 >= 1 ? nf(bar * 100) + " pm" : nf(bar * 1e5) + " fm";
        R.line(12, h - 18, 12 + bar * pxA, h - 18, 2, css.muted);
        T.draw(label + "  =  " + metres(bar * 1e-10), 12, h - 24, 12, css.muted);
    }
    {   // the readout at the top left, on a soft plate (the web engine's info lines)
        const double maxW = w - 24 - 56;
        std::vector<std::string> rows;
        for (const std::string& t : lab.info()) {                          // wrap at " · " first
            std::string cur;
            std::size_t a = 0;
            for (;;) {
                const std::size_t b = t.find("  ·  ", a);
                const std::string part = t.substr(a, b == std::string::npos ? std::string::npos : b - a);
                const std::string nx = cur.empty() ? part : cur + "  ·  " + part;
                if (!cur.empty() && T.width(nx, 13) > maxW) { rows.push_back(cur); cur = part; } else cur = nx;
                if (b == std::string::npos) break;
                a = b + 5;
            }
            rows.push_back(cur);
        }
        for (std::size_t i = 0; i < rows.size(); i++) {                    // a single long part still has to fit: break it at spaces
            if (T.width(rows[i], 13) <= maxW) continue;
            std::string line, rest;
            std::size_t a = 0;
            for (;;) {
                const std::size_t b = rows[i].find(' ', a);
                const std::string wd = rows[i].substr(a, b == std::string::npos ? std::string::npos : b - a);
                const std::string nx = line.empty() ? wd : line + " " + wd;
                if (!line.empty() && T.width(nx, 13) > maxW) { rest = rows[i].substr(a); break; }
                line = nx;
                if (b == std::string::npos) break;
                a = b + 1;
            }
            rows[i] = line;
            if (!rest.empty()) rows.insert(rows.begin() + i + 1, rest);
        }
        bool any = false;
        double mw = 0;
        for (const std::string& t : rows) { if (!t.empty()) any = true; mw = std::max(mw, T.width(t, 13)); }
        if (any) {
            const double bw = mw + 12 + 8, bh = 17.0 * rows.size() + 10;
            dl->AddRectFilled(ImVec2(float(ox + 6 * uiScale), float(oy + 4 * uiScale)), ImVec2(float(ox + (6 + bw) * uiScale), float(oy + (4 + bh) * uiScale)),
                              ImGui::ColorConvertFloat4ToU32(ImVec4(css.card.r, css.card.g, css.card.b, 0.72f)), float(8 * uiScale));
        }
        // on their see-through panel the rows need no halo (in light themes its 8 copies left faint grey marks between letters)
        for (std::size_t i = 0; i < rows.size(); i++) T.draw(rows[i], 12, 21 + 17.0 * i, 13, css.muted, 0, any ? nullptr : &css.card);
    }
}

}  // namespace atom
