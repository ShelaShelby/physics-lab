#include "atom_view.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>

#include "chart_draw.h"
#include "classic_check.h"
#include "atom_card_paths.h"
#include "cloud_dots.h"
#include "cloud_light.h"
#include "outline.h"
#include "phase_wheel.h"
#include "quark_light.h"
#include "quark_tomo.h"
#include "radii.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <array>
#include <functional>
#include <memory>
#include <cfloat>
#include <cmath>
#include <cstdio>

#include "imgui.h"
#include "orbit/format.h"
#include "ui/fonts.h"
#include "ui/look.h"
#include "ui/style_draw.h"

namespace ui { void auditNote(const std::string& line); bool shellLogOn(); }   // ui/shell_test.cpp: PL_LOG_SHELL

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
const Color kPhaseNeg = hexColor("#f08a3c");       // the − half of the wave
// 0.38: the + half is a FIXED blue (DECISIONS: science colours stay fixed; the readout says "blue where the wave ψ is +"). Until 0.37 it was the theme's accent, so in Paper
// (accent rust #9a3b1d) both halves were red-brown and in High contrast yellow against orange. The two blues are the Dark and Light themes' accents of 0.37 (unchanged look there).
const Color kPhasePosDark = hexColor("#6c9cff"), kPhasePosLight = hexColor("#2d6ae3");

Color withA(Color c, double a) { c.a = static_cast<float>(std::max(0.0, std::min(1.0, a)) * c.a); return c; }
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
Blob flat(double x, double y, double r, Color c) { Blob b{}; b.x = float(x); b.y = float(y); b.r = float(r); b.kind = 3; b.c0 = c; return b; }
// 0.16 stage 5: a ring and a line as blobs (shader kinds 7 and 8), so they go into the same batch as the dots round them instead of breaking it.
// The same shape as Renderer2D::ring / line: a hard-edged band (no smooth edge), at least 1 point wide (thinner: the alpha is lowered instead).
Blob ringBlob(double x, double y, double r, double wd, Color c) {
    if (wd < 1) { c.a *= float(wd); wd = 1; }
    const double R = r + wd / 2 + 1;
    Blob b{}; b.x = float(x); b.y = float(y); b.r = float(R); b.kind = 7; b.fx = float(r / R); b.f0 = float(wd / 2 / R); b.c0 = c; return b;
}
bool lineBlob(double x0, double y0, double x1, double y1, double wd, Color c, Blob& b) {   // false: too short to draw (as Renderer2D::line)
    if (std::hypot(x1 - x0, y1 - y0) < 1e-9) return false;
    if (wd < 1) { c.a *= float(wd); wd = 1; }
    const double hx = (x1 - x0) / 2, hy = (y1 - y0) / 2, R = std::max(std::fabs(hx), std::fabs(hy)) + wd / 2;
    b = Blob{}; b.x = float(x0 + hx); b.y = float(y0 + hy); b.r = float(R); b.kind = 8; b.fx = float(hx / R); b.fy = float(hy / R); b.f0 = float(wd / 2 / R); b.c0 = c;
    return true;
}

// Text drawn with Dear ImGui on top of the picture; y is the baseline, like the canvas fillText().
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

// a deterministic "random" number from a seed, for the flickering quark details
double rnd(double a) { const double x = std::sin(a * 127.1 + 311.7) * 43758.5453; return x - std::floor(x); }
// 0.16 stage 5: the random numbers of sea pair j in its cycle cyc depend on nothing else (not the nucleon, not the camera), so they are worked out once and kept.
// The values are the very same doubles as before (the same calls in the same order); only the repeats are gone.
struct SeaRnd { double cyc = -1e300, rr, sth, cth, saz, caz, sdth, cdth, sdaz, cdaz, c5; };
const SeaRnd& seaRnd(int j, double cyc) {
    static SeaRnd cache[12][64];
    SeaRnd& e = cache[j][int(std::fmod(std::fabs(cyc), 64.0))];
    if (e.cyc != cyc) {
        const double sd = j * 91.7 + cyc * 13.13;
        const double th = std::acos(1 - 2 * rnd(sd + 1)), az = 2 * kPi * rnd(sd + 2), dth = std::acos(1 - 2 * rnd(sd + 3)), daz = 2 * kPi * rnd(sd + 4);
        e.cyc = cyc; e.rr = 0.62 * std::cbrt(rnd(sd));
        e.sth = std::sin(th); e.cth = std::cos(th); e.saz = std::sin(az); e.caz = std::cos(az);
        e.sdth = std::sin(dth); e.cdth = std::cos(dth); e.sdaz = std::sin(daz); e.cdaz = std::cos(daz);
        e.c5 = rnd(sd + 5);
    }
    return e;
}
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

// 0.22: the Density look: up to which nucleon size on screen (points) the whole nucleus is drawn that way (beyond it: the nucleons, as Fuzzy). (0.54.1: no points any more, the
// density is projected exactly)
constexpr double kDensityLookRn = 40;   // (the whole "Nucleus" layer: from 40 points the quarks start to show, atom_view qa)

// the slowly tumbling triangle of valence quarks (the Simple style; 0.43: the Accurate style draws dots instead, see quark_tomo.h)
struct QPt { char f; double x, y, dep; };
struct QuarkPts { double k, t; QPt P[3]; };   // (0.45: the colour-swap fields, computed but never drawn since 0.42, removed)
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
    return Q;
}


// ---- the cloud's Glow style (0.21: the dots' light and exposure moved to cloud_dots.cpp, glowBlobs, so the Molecule Lab's Realistic cloud is drawn the same way) ----
void drawGlow(render::Renderer2D& R, const std::vector<GlowDot>& D, long cap, bool light, DotColours dc, bool darken, double base, double w, double h, double& expo,
              const std::vector<std::array<double, 2>>* cores = nullptr) {
    static std::vector<Blob> B;
    if (!glowBlobs(D, cap, light, dc, darken, base, w, h, expo, B, cores)) return;
    R.beginCloudLayer();                                  // 0.40: the dots' light adds up in a float picture (in 8 bits the faint dots of a big cloud rounded to nothing)
    R.blobsPaletteTake(B, !light, dc.palette());
    R.endCloudLayer(!light);
    B.clear();
}

}  // namespace

// ---------------------------------------------------------------- the frame
static MarksDrawn g_marks;
const MarksDrawn& marksDrawn() { return g_marks; }
static NucleusStats g_nst;
static long g_qDotsOnScreen = 0;   // 0.45: Accurate quark dots that landed in the window this frame (the legend is shown only when there are some)
static double g_qViewFm = 0;      // 0.45: the window's width in fm at the quark zoom (for the note shown when no dot is in view)
const NucleusStats& nucleusStats() { return g_nst; }
// 0.66.0: the area of the blobs' quads inside a w x h picture (a blob's quad reaches r + 1 from its centre; a line's (kind 8) along its length)
static double coverOf(const render::Renderer2D::Blob* b, int n, double w, double h) {
    double a = 0;
    for (int i = 0; i < n; i++) {
        const double e = b[i].kind == 8 ? double(b[i].r) * (std::max(std::fabs(b[i].fx), std::fabs(b[i].fy)) + b[i].f0) + 1 : b[i].kind == 10 ? double(b[i].r) * std::clamp(double(b[i].fy), 0.0, 1.0) + 1 : double(b[i].r) + 1;
        const double x0 = std::max(0.0, b[i].x - e), x1 = std::min(w, b[i].x + e), y0 = std::max(0.0, b[i].y - e), y1 = std::min(h, b[i].y + e);
        if (x1 > x0 && y1 > y0) a += (x1 - x0) * (y1 - y0);
    }
    return a;
}
static void noteOutline(const char* kind, const Outline& O, int fo, std::size_t segs) { g_marks.outline = kind; g_marks.outlineOrb = fo; g_marks.outlineMass = O.mass; g_marks.outlineReach = O.rmax; g_marks.outlineSegs = segs; }

void AtomView::draw(render::Renderer2D& R, ImDrawList* dl, double ox, double oy, double uiScale, AtomLab& lab, double w, double h, const app::ThemeColors& colours, double textScale) {
    g_marks = MarksDrawn{};
    const AtomParams& p = lab.p;
    AtomState& s = lab.s;
    const Theme css = theme(colours);
    g_textScale = textScale;
    const Text T{dl, ox, oy, uiScale};
    lab.viewW = w; lab.viewH = h;
    atomCardPaths().frame(R, lab);                                         // 0.57.0: the card's Slice dots and jump groups, made and checked a step a frame (guarded)
    setChartInsets(std::max(lab.insetTop, lab.legendBottom + 6), lab.insetBottom + lab.chartTiltH, lab.insetRight);   // (0.22 stage 6: below the Legend and left of the Controls panel)
    R.rect(0, 0, w, h, css.card);                                          // the canvas background

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
        if (s.showChain) { g_marks.chainNodes = static_cast<int>(lab.decayGraph().nodes.size()); g_marks.chainEdges = lab.decayGraph().edgeCount(); }
        if (!lab.gpuChart) chartCache_.release(R);                         // (switched off: the GPU's copy is let go)
        ChartCache* cache = lab.gpuChart ? &chartCache_ : nullptr;         // 0.13 stage 4 part B: the tiles / blocks and the ground come from a GPU buffer made once; nullptr: made every frame as before
        { std::vector<std::array<double, 4>> av; for (const auto& b : lab.overBoxes) av.push_back({(b[0] - ox) / uiScale, (b[1] - oy) / uiScale, b[2] / uiScale, b[3] / uiScale}); setChartAvoid(av); }
        setChartLegendInPicture(!lab.shellUi);                           // (0.15 stage 5f: the new UI has the legend and the notes in its Legend chip)
        if (flat) drawChart(R, adapter, chartColours, w, h, p.Z, p.N, hz, hn, s.showChain ? &chainOv : nullptr, cache);
        else drawChartScene(R, adapter, chartColours, lab.chartScene(), w, h, p.Z, p.N, hz, hn, s.showChain ? &chainOv : nullptr, cache);
        if (hz) {                                                          // the details of the square under the pointer: a tooltip (it follows the pointer, stays on screen and never hides a square)
            ui::BeginTooltip();
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
    const double cx0 = lab.freeW >= 0 ? lab.freeX + lab.freeW / 2 : w / 2, cy0 = lab.freeW >= 0 ? lab.freeY + lab.freeH / 2 : h / 2, m = lab.fitSpan();   // (new UI, stage 5h: the middle of the free part; 0.22 stage 6: and sized to it)
    g_marks.centre[0] = cx0; g_marks.centre[1] = cy0;
    const double pxm = lab.pxPerM(m, s.szCur) * std::pow(10, p.zoom);    // points per metre: auto-fit × zoom
    const double px = pxm * kA0;                                           // points per Bohr radius
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
        static std::uint64_t allEpoch = 0;                                 // 0.57.0: the jump's two populations put together once per jump, not every frame (neither changes while the jump is drawn)
        const float* PTS = s.pts.data();
        if (jump) {
            const long jn = lab.jumpDots();
            if (allEpoch != s.jumpEpoch || allPts.size() != std::size_t(6 * jn)) {
                allPts.assign(s.pts.begin(), s.pts.begin() + 3 * jn); allPts.insert(allPts.end(), s.jp.begin(), s.jp.begin() + 3 * jn);
                allEpoch = s.jumpEpoch;
            }
            PTS = allPts.data(); cnt = 2 * jn;
        }
        const bool glow = p.cstyle == "Glow";
        std::vector<GlowDot> G;
        G.reserve(glow ? std::size_t(cnt) : 0);
        const Color cP = css.light ? kPhasePosLight : kPhasePosDark, cN = kPhaseNeg;
        const bool wheel = lab.usesWheel();                                // Phase colours: Wheel (or a complex state) colours each dot by its phase, Sign by the sign of the wave
        const DotColours dc = makeDotColours(lab.gpuColours, wheel, css.light, cP, cN);   // GPU colours On (the default): a dot carries its colour group; Off: the 0.12 way, every dot carries its colour
        const int ng = dc.n;
        g_marks.phaseGroups = ng; g_marks.wheel = wheel;
        static std::vector<unsigned char> grp;                             // each dot's colour group
        grp.resize(std::size_t(cnt));
        // each dot's colour group: a jump in flight, a superposition (thinned and coloured from the CURRENT beat phase) or a plain cloud. 0.13 stage 2: on a PC with OpenGL 4.3 (and the
        // "GPU superposition and jumps" switch on) the first two come from a compute shader, else from the CPU functions, which are the reference (255 = not shown now)
        // 0.57.0: a jump with every electron in the Glow Cloud 3D: drawn from the card's cloud buffer with its groups made on the card each frame
        // (atom_card_paths.h), once the card's check on this PC passed; until then, and on any other PC, the processor's path below
        const unsigned jumpBuf = jump && glow && d3 && lab.gpuCloud && dc.gpu && R.cloudGpuOk() ? atomCardPaths().jumpGroupsBuffer(R, lab) : 0;
        // 0.65.0: a cloud's groups kept from frame to frame (only the new dots' made; whole again when the cloud or the Phase colours change); grpEpoch tells
        // the card programs whether the new dots' groups are enough (0: a jump or a superposition, sent whole each frame)
        static GroupsKept grpKept;
        std::uint64_t grpEpoch = 0;
        if (!jumpBuf) grpEpoch = keepGroups(grpKept, lab, R.compute(), cnt, cap, grp.data());
        else grpKept = GroupsKept{};
        const bool outlineOn = p.outline == "On" && !lab.superOn() && !lab.manyOn() && !lab.diracCloudOn() && lab.outlineAlpha() > 0.01;   // (0.49.3: not on a Dirac cloud: the outline is the non-relativistic shape)   // (0.39.1: not with many electrons: the outline is the one-electron orbital's)         // (the 90 % outline is of one orbital: not drawn for a superposition)
        const int fo = lab.outlineStateId();                                // the state on screen (a complex state's own id), not the real fallback fullOrb()
        const double olA = lab.outlineAlpha();                             // the outline fades out as a jump leaves its orbital and back in after landing
        const Color olCol = withA(css.fg, 0.6 * olA);                            // the 90 % outline: thin, the theme's text colour (light on dark pages, dark on light ones)
        const double olUnit = lab.aUnit() / p.Z * pxm;                             // points per Bohr radius at Z = 1 in the outline's own units
        if (sl) {                                                          // slice: the dots made ON the cut plane (0.37; before, a slab 14 % of the view thick, which filled in nodes), seen face-on
            const double size = lab.cloudExtM(), off = lab.cutOffsetM(), th = 2 * size;   // (0.39.1: the many-electron atom's size; kExt / Z would hide its dots of the last planes during a drag)   // th: every dot made on a cut plane is drawn (during a drag some are from the last few planes, replaced within frames); a jump's dot with no place on the plane is put 10^4 a0 away and hidden
            const auto sliceDotsCpu = [&] {
                for (long i = 0; i < cnt; i++) {
                    const double dist = std::fabs((hz ? PTS[3 * i + 2] : PTS[3 * i + 1]) - off);
                    if (dist > th || grp[std::size_t(i)] == 255) continue;
                    const double X = cx + PTS[3 * i] * pxm, Y = cy - (hz ? PTS[3 * i + 1] : PTS[3 * i + 2]) * pxm;
                    if (X < -2 || X > w || Y < -2 || Y > h) continue;
                    if (glow) G.push_back({float(X), float(Y), 4.2f, 1.f, grp[std::size_t(i)]});
                    else B.push_back(squareDot(dc, X - 1 + 1.25, Y - 1 + 1.25, 1.25, grp[std::size_t(i)], 0.6 * cloudDotWeight(cap)));
                }
            };
            // 0.65.0: on the card (Vulkan; kClassicCS's Slice, atom/classic_check.h) once the Classic dots' start-up check passed (it checks the Slice's
            // Classic squares and Glow dots too): every dot written at its own index, in the processor's dot order; the loop above is the reference and the fallback
            const bool sliceCard = !jump && dc.gpu && lab.gpuCloud;
            if (glow) {
                DotColours dcl = dc;
                if (css.light && !wheel) darkenForLightPage(dcl);         // (as glowBlobs does with darken = !wheel)
                bool onCard = false;
                R.beginCloudLayer();
                if (sliceCard) {
                    render::ClassicDraw sd = withGroupsEpoch(sliceDraw(PTS, cnt, cap, s.ptsEpoch, s.k, grp.data(), cx, cy, pxm, w, h, dcl, 1.0, true, hz, off, th), grpEpoch);
                    sd.additive = !css.light;
                    double* ex = &cloudExpo_;
                    const long capL = cap;
                    const bool lightPage = css.light;
                    sd.exposure = [ex, capL, lightPage](const std::vector<float>& counts) {
                        static std::vector<float> grid;
                        glowGridOfCounts(counts, 4.2f, grid);
                        if (!glowExposure(grid, capL, lightPage, 3.0, *ex)) return -1.0;
                        return cloudDotLight(capL, *ex, 3.0, 1.0);
                    };
                    onCard = drawClassicOnCard(R, sd, !atomCardPaths().sliceStarting(lab));
                }
                if (!onCard) {
                    sliceDotsCpu();
                    DotColours dcb = dc;
                    if (glowBlobs(G, cap, css.light, dcb, !wheel, 3.0, w, h, cloudExpo_, B)) R.blobsPaletteTake(B, !css.light, dcb.palette());   // (drawGlow's, in the layer opened above)
                }
                R.endCloudLayer(!css.light);
            } else if (!(sliceCard && drawClassicOnCard(R, withGroupsEpoch(sliceDraw(PTS, cnt, cap, s.ptsEpoch, s.k, grp.data(), cx, cy, pxm, w, h, dc, 0.6 * cloudDotWeight(cap), false, hz, off, th), grpEpoch), !atomCardPaths().sliceStarting(lab)))) {
                sliceDotsCpu();
                R.blobsPaletteTake(B, false, dc.palette());
            }
            B.clear();
            if (outlineOn) {                                               // the contour |psi|^2 = c in the cut plane (cached until the orbital or the cut changes)
                const Outline* O = outlineReady(fo, lab.gpuOutline);       // 0.13 stage 3: nullptr while the GPU is still making the outline (the first time): nothing is drawn yet, and the frame does not wait
                if (O) {
                    const double off0 = p.cut * 0.5 * kExt[orbitals()[stateOrbital(fo)].n];
                    olSlice_.update(*O, fo, hz, off0, lab.gpuOutline);
                    if (olSlice_.shows(fo, hz)) {                          // (the contour of another orbital or plane is never drawn; the cut may be a frame behind while the GPU works)
                        for (const OutSeg2& g : olSlice_.segments()) R.line(cx + g.x0 * olUnit, cy - g.y0 * olUnit, cx + g.x1 * olUnit, cy - g.y1 * olUnit, 1.1, olCol);
                        noteOutline("slice", *O, fo, olSlice_.segments().size());
                    }
                }
            }
            const double gx = w - 44 - lab.insetRight, gy = h - 44 - lab.reserveRight;   // 0.39.2: left of the Controls panel and above the readout (its "x" showed past the panel's edge)                         // axes of the cut plane
            R.line(gx, gy, gx + 26, gy, 1.5, css.muted);
            R.line(gx, gy, gx, gy - 26, 1.5, css.muted);
            T.draw("x", gx + 34, gy + 4, 12, css.muted, 0.5);
            T.draw(hz ? "y" : "z", gx, gy - 32, 12, css.muted, 0.5);
        } else if (!d3) {                                                  // side view: drop y, plot (x, z)
            // Sign: blue where the wave ψ is +, orange where it is −; Wheel: one colour per phase step. The dots are drawn group by group, each group in dot order:
            // the v0.12 scan (a pass over all dots for each colour) with GPU colours Off, a counting sort with them On: the same list.
            // 0.65.0: on the card (Vulkan; kClassicCS's side view, atom/classic_check.h) once the Classic dots' start-up check passed, which checks the side
            // view's Classic squares, Glow dots and Glow cells too: the dots put on the picture, counted, grouped and written there; the processor's path below
            // is the reference and the fallback
            static std::vector<SideDot> side;
            const bool sideCard = !jump && dc.gpu && lab.gpuCloud;
            if (glow) {
                DotColours dcl = dc;
                if (css.light && !wheel) darkenForLightPage(dcl);         // (as glowBlobs does with darken = !wheel)
                bool onCard = false;
                R.beginCloudLayer();
                if (sideCard) {
                    render::ClassicDraw sd = withGroupsEpoch(sideDraw(PTS, cnt, cap, s.ptsEpoch, s.k, grp.data(), cx, cy, pxm, w, h, dcl, 1.0, true), grpEpoch);
                    sd.additive = !css.light;
                    double* ex = &cloudExpo_;
                    const long capL = cap;
                    const bool lightPage = css.light;
                    sd.exposure = [ex, capL, lightPage](const std::vector<float>& counts) {
                        static std::vector<float> grid;
                        sideGlowGrid(counts, grid);
                        if (!glowExposure(grid, capL, lightPage, 1.0, *ex)) return -1.0;
                        return cloudDotLight(capL, *ex, 1.0, 1.0);
                    };
                    onCard = drawClassicOnCard(R, sd);
                }
                if (!onCard) {
                    sideDots(PTS, cnt, grp.data(), ng, cx, cy, pxm, w, h, lab.gpuColours, side);
                    for (const SideDot& d : side) G.push_back({d.x, d.y, 4.4f, 1.f, d.g});
                    DotColours dcb = dc;
                    if (glowBlobs(G, cap, css.light, dcb, !wheel, 1.0, w, h, cloudExpo_, B)) R.blobsPaletteTake(B, !css.light, dcb.palette());   // (drawGlow's, in the layer opened above)
                }
                R.endCloudLayer(!css.light);
            } else if (!(sideCard && drawClassicOnCard(R, withGroupsEpoch(sideDraw(PTS, cnt, cap, s.ptsEpoch, s.k, grp.data(), cx, cy, pxm, w, h, dc, 0.3 * cloudDotWeight(cap), false), grpEpoch)))) {
                sideDots(PTS, cnt, grp.data(), ng, cx, cy, pxm, w, h, lab.gpuColours, side);
                sideBlobs(side, dc, 0.3 * cloudDotWeight(cap), B);               // one reserve, no regrowing (0.13 stage 4 part C)
                R.blobsPaletteTake(B, false, dc.palette());
            }
            B.clear();
            if (outlineOn)                                                 // the shadow of the 90 % region seen from the side
                if (const Outline* O = outlineReady(fo, lab.gpuOutline)) {
                    for (const OutSeg2& g : O->side) R.line(cx + g.x0 * olUnit, cy - g.y0 * olUnit, cx + g.x1 * olUnit, cy - g.y1 * olUnit, 1.1, olCol);
                    noteOutline("side", *O, fo, O->side.size());
                }
        } else {                                                           // rotate by yaw (about z), then pitch, then project
            const double ext = lab.cloudExtM();
            static std::vector<float> PX, PY, PD;
            const bool gpuCloud = glow && lab.gpuCloud && dc.gpu && (!jump || jumpBuf) && R.cloudGpuOk();   // 0.46 (C4): the dots from the GPU buffer, projected and lit there; the same picture
            if (gpuCloud) {
                render::Renderer2D::CloudGpuDraw cd;
                cd.pts = PTS; cd.cnt = cnt; cd.cap = cap; cd.epoch = s.ptsEpoch; cd.k = s.k; cd.grp = grp.data(); cd.grpEpoch = grpEpoch;
                if (jumpBuf) { cd.cap = cnt; cd.k = cnt; cd.epoch = s.jumpEpoch; cd.grp = nullptr; cd.grpBuf = jumpBuf; }   // 0.57.0: both populations, sent once per jump
                cd.cx = float(cx); cd.cy = float(cy); cd.pxm = float(pxm); cd.ext = float(ext); cd.ca = float(ca); cd.sa = float(sa); cd.cb = float(cb); cd.sb = float(sb); cd.w = float(w); cd.h = float(h);
                cd.additive = !css.light;
                DotColours dcl = dc;
                if (css.light && !wheel) darkenForLightPage(dcl);     // (as glowBlobs does with darken = !wheel)
                cd.pal = dcl.palette();
                double* ex = &cloudExpo_;
                const long capL = cap;
                const bool lightPage = css.light;
                // 0.52.1: the core (the nucleus, at the cloud's middle) kept inside the displayable range: the dots covering it, from the CPU copy of the points (glowCapExposure)
                static std::vector<GlowDot> nearCore;
                static std::vector<std::array<double, 2>> core(1);
                core[0] = {cx, cy};
                if (!lightPage && jumpBuf) {                               // 0.57.0: the card made the groups: the few dots over the nucleus asked here whether they are shown
                    static std::vector<int> nearIdx;
                    glowDotsNear(PTS, cnt, nullptr, CloudCamera{cx, cy, pxm, ca, sa, cb, sb, ext}, cx, cy, nearCore, &nearIdx);
                    const long jn = lab.jumpDots();
                    std::size_t keep = 0;
                    for (std::size_t k = 0; k < nearCore.size(); k++) {
                        const int i = nearIdx[k], pop = i >= jn ? 1 : 0;
                        if (lab.jumpShown(pop, i - pop * jn)) nearCore[keep++] = nearCore[k];
                    }
                    nearCore.resize(keep);
                } else if (!lightPage) glowDotsNear(PTS, cnt, grp.data(), CloudCamera{cx, cy, pxm, ca, sa, cb, sb, ext}, cx, cy, nearCore); else nearCore.clear();
                cd.exposure = [ex, capL, lightPage](const std::vector<float>& g) {
                    if (!glowExposure(g, capL, lightPage, 1.0, *ex)) return -1.0;
                    if (!lightPage && !nearCore.empty()) glowCapExposure(nearCore, capL, 1.0, core, *ex);
                    return 1.0 * *ex * cloudDotWeight(capL);
                };
                R.beginCloudLayer();
                R.cloudGpu(cd);
                R.endCloudLayer(!css.light);
                g_marks.cloudGpu = true;
            }
            // 0.64.0: the Classic dots from the card (Vulkan; atom/classic_check.h) once its start-up check passed: projected, banded and put in the processor's order there
            const bool classicCard = !gpuCloud && !glow && !jump && dc.gpu && lab.gpuCloud &&
                                     drawClassicOnCard(R, withGroupsEpoch(classicDraw(PTS, cnt, cap, s.ptsEpoch, s.k, grp.data(), CloudCamera{cx, cy, pxm, ca, sa, cb, sb, ext}, w, h, dc, cloudDotWeight(cap)), grpEpoch));
            if (!gpuCloud && !classicCard)
            projectCloud(PTS, cnt, CloudCamera{cx, cy, pxm, ca, sa, cb, sb, ext}, w, h, PX, PY, PD);   // PD: the depth over the cloud's radius; NaN = off the picture
            if (gpuCloud || classicCard) {
            } else if (glow) {
                // one pass: size and brightness change smoothly with depth (far = smaller, fainter; near = bigger, brighter), no bands
                for (long i = 0; i < cnt; i++) {
                    const float v = PD[i];
                    if (std::isnan(v) || grp[std::size_t(i)] == 255) continue;
                    const float t = std::max(-1.f, std::min(1.f, v));          // -1 nearest, +1 farthest
                    const float near = 0.5f - 0.5f * t;                        // 1 near … 0 far
                    G.push_back({PX[i], PY[i], 4.6f * (0.8f + 0.4f * near), 0.3f + 0.9f * near, grp[std::size_t(i)]});
                }
                const std::vector<std::array<double, 2>> core{{cx, cy}};   // 0.52.1: the nucleus's core kept inside the displayable range (glowCapExposure)
                drawGlow(R, G, cap, css.light, dc, !wheel, 1.0, w, h, cloudExpo_, &core);
            } else {
            // three depth bands (far = faint and small, near = bright and larger: kDepthBand), drawn far to near, inside a band group by group, inside a group in dot order.
            // GPU colours Off: the v0.12 scan, a pass over all dots for each band and colour (72 for the Wheel); On: a counting sort, and the dots carry only their colour group.
            buildClassic3D(PX.data(), PY.data(), PD.data(), grp.data(), cnt, dc, cloudDotWeight(cap), B);
            R.blobsPaletteTake(B, false, dc.palette());
            B.clear();
            }
            const Outline* wireO = outlineOn ? outlineReady(fo, lab.gpuOutline) : nullptr;   // (nullptr while the GPU makes it the first time)
            if (wireO) {                                                   // a faint wireframe of the 90 % surface: its contour lines in planes across the region, turned with the cloud
                noteOutline("wire", *wireO, fo, wireO->wire.size());
                const double um = lab.aUnit() / p.Z;
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
            const double gx = w - 44 - lab.insetRight, gy = h - 44 - lab.reserveRight;   // 0.39.2: left of the Controls panel and above the readout (its "x" showed past the panel's edge)                         // axis gizmo (bottom right)
            const double axes[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
            const char* names[3] = {"x", "y", "z"};
            for (int a = 0; a < 3; a++) {
                const double x1 = axes[a][0] * ca - axes[a][1] * sa, y1 = axes[a][0] * sa + axes[a][1] * ca, up = y1 * sb + axes[a][2] * cb;
                R.line(gx, gy, gx + 26 * x1, gy - 26 * up, 1.5, css.muted);
                T.draw(names[a], gx + 34 * x1, gy - 34 * up + 4, 12, css.muted, 0.5);
            }
        }
        if (wheel && lab.phaseRingHidden()) {}                               // 0.52.1: inside a nucleon the phase ring labels no colour on screen (AtomLab::phaseRingHidden)
        else if (wheel && lab.manyOn() && lab.manySub() < 0 && lab.s.layer != "Atom") {}   // 0.46.1 (outside review): zoomed into the nucleus the electron shells' legend means nothing there: not drawn
        else if (wheel && lab.manyOn() && lab.manySub() < 0) {            // 0.39: the whole atom: its colours are the shells, not a phase. The legend lists the atom's shells in their colours
            const double lx = lab.ringX >= 0 ? lab.ringX : w - 56 - lab.insetRight, ly = lab.ringY >= 0 ? lab.ringY : 86;
            int top = 1;
            for (const hf::Subshell& sh : lab.manyAtom()->sub) if (sh.occ) top = std::max(top, sh.n);
            T.draw("shells", lx, ly - 54, 11, css.muted, 0.5);
            for (int n = 1; n <= top; n++) {
                const PhaseRgb c = phaseRgb(AtomLab::shellPhase(n), css.light);
                T.draw(hf::shellLetter(n), lx - 3.0 * 14 + (n - 1) * 14, ly - 30, 13, Color{c.r, c.g, c.b, 1}, 0.5);
            }
            g_marks.ring[0] = lx - 62; g_marks.ring[1] = ly - 62; g_marks.ring[2] = 124; g_marks.ring[3] = 104;
        } else if (wheel) {                                                // the legend: a ring of the phase colours, with 0, pi/2, pi, 3 pi/2 (the phase of the wave, not a place)
            const double lx = lab.ringX >= 0 ? lab.ringX : w - 56 - lab.insetRight, ly = lab.ringY >= 0 ? lab.ringY : 86, ro = 20;   // top right (bottom left has the ⓘ button and the scale bar, bottom right the axes); new UI (0.15 stage 5e): left of the Controls panel
            for (int i = 0; i < 72; i++) {
                const double a0 = 2 * kPi * i / 72, a1 = 2 * kPi * (i + 1.05) / 72;
                const PhaseRgb c = phaseRgb(a0 + kPi / 72, css.light);
                R.line(lx + ro * std::cos(a0), ly - ro * std::sin(a0), lx + ro * std::cos(a1), ly - ro * std::sin(a1), 6, Color{c.r, c.g, c.b, 1});
            }
            const struct { const char* t; double a; } marks[] = {{"0", 0}, {"π/2", kPi / 2}, {"π", kPi}, {"3π/2", 1.5 * kPi}};
            for (const auto& mk : marks) T.draw(mk.t, lx + 34 * std::cos(mk.a), ly - 34 * std::sin(mk.a) + 4, 11, css.muted, 0.5);
            T.draw("phase of ψ", lx, ly - 54, 11, css.muted, 0.5);
            g_marks.ring[0] = lx - 62; g_marks.ring[1] = ly - 62; g_marks.ring[2] = 124; g_marks.ring[3] = 104;   // (for the automatic checks: the ring with its labels, in points)
        }
    } else {                                                               // Orbit: the circle lies flat (x-y plane), projected so it can be turned
        auto op = [&](double x, double y, double& qx, double& qy) { const double x1 = x * ca - y * sa, y1 = x * sa + y * ca; qx = x1; qy = y1 * sb; };
        if (lab.manyOn()) {                                                // 0.39.1: every electron. Each shell (K, L, M ...) is a ring at the average distance of its electrons (Hartree-Fock),
            // with its electrons spaced evenly on it, in the shell colours of the cloud views. A picture of the shells, not of orbits: real electrons are clouds (the readout says so); the rings
            // stand still (0.50). With Radius: Shells enlarged the radii use the cloud's enlarged-shells mapping (the outer edge true), else they are true.
            const std::vector<AtomLab::Ring> rings = lab.manyRings();
            double lastLabelPx = -1e9;
            for (const AtomLab::Ring& g : rings) {
                const double Rn = lab.drawnRadius(g.r) * kA0;
                if (Rn * pxm > 4000) continue;
                const PhaseRgb c = phaseRgb(AtomLab::shellPhase(g.n), css.light);
                const Color col{c.r, c.g, c.b, 1};
                Pen pen{R, bohr ? 1.5 : 1.1, withA(col, bohr ? 0.75 : 0.5)};
                for (int i = 0; i <= 120; i++) { const double t = i / 120.0 * kPi * 2; double qx, qy; op(Rn * std::cos(t), Rn * std::sin(t), qx, qy); pen.to(cx + qx * pxm, cy - qy * pxm); }
                if (Rn * pxm - lastLabelPx > 15) {                         // a label where the rings are far enough apart: the shell's letter and its electrons
                    double lqx, lqy;
                    op(Rn * std::cos(2.35), Rn * std::sin(2.35), lqx, lqy);
                    const double lx = cx + lqx * pxm, ly = cy - lqy * pxm - 6;
                    if (lx > 24 && lx < w - 24 && ly > 16 && ly < h - 16) { T.draw(std::string(hf::shellLetter(g.n)) + " " + std::to_string(g.electrons), lx, ly, 11, col, 0.5); lastLabelPx = Rn * pxm; }
                }
                for (int k = 0; k < g.electrons; k++) {
                    const double t = lab.ringElectronAngle(g, k);
                    double qx, qy;
                    op(Rn * std::cos(t), Rn * std::sin(t), qx, qy);
                    const double ex = cx + qx * pxm, ey = cy - qy * pxm;
                    R.disc(ex, ey, bohr ? 4.2 : 3.2, col);
                    if (bohr) R.line(ex - 2, ey, ex + 2, ey, 1.2, css.onacc);   // the minus sign
                }
            }
        } else if (bohr) {                                                 // Bohr 1913: the allowed circles n = 1–4 and nothing else; the electron's is bold
            for (int n = 1; n <= AtomLab::kLineMaxN; n++) {   // (0.46: to n = 6)
                const double Rn = double(n) * n / p.Z * lab.aUnit();
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
            const double R0 = double(p.n) * p.n / p.Z * lab.aUnit();
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
            const double gx = w - 44 - lab.insetRight, gy = h - 44 - lab.reserveRight;   // 0.39.2: left of the Controls panel and above the readout (its "x" showed past the panel's edge)
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
    if (!cloud && eBehind && !lab.manyOn()) drawE();                     // (0.39.1: with every electron the rings carry them)

    // the nucleus at its real size: a speck at atom scale, then protons and neutrons, then (deep zoom) quarks
    const double Rpx = nuclearRadius(p.Z, p.N) * fmPx, rn = kNucleonR * fmPx;
    s.densityDrawn = false;
    g_nst = NucleusStats();
    g_qDotsOnScreen = 0;
    g_qViewFm = 0;
    bool shownP = true;                                                    // 0.52.1: the nucleon the quark view shows (for the mass bar): the one dived into, else the one nearest the middle
    const auto nucT0 = std::chrono::steady_clock::now();
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
    } else if ((s.densityDrawn = p.style == "Density" && rn < kDensityLookRn && s.focus < 0 && !(p.Z == 0 && p.N == 1))) {   // (0.46, review A5: a lone neutron has no net charge: no charge cloud, it is drawn as the neutron)   // 0.22: the whole nucleus as its measured charge density (no nucleons: zoom in further for a snapshot of them)
        char key[160];
        std::snprintf(key, sizeof key, "%s|%.6f,%.6f,%.6f|%d", s.nucKey.c_str(), s.nucAxis[0], s.nucAxis[1], s.nucAxis[2], deuteronSpinIndex(p.dspin));
        if (densKey_ != key) { densKey_ = key; densModel_ = nucleusChargeDensity(p.Z, p.N, s.nucAxis[0], s.nucAxis[1], s.nucAxis[2], deuteronSpinIndex(p.dspin)); densGridKey_.clear(); }
        // 0.54.1: drawn as a picture of the projected density, now the exact line-of-sight integral of the same model the points came from (atom_core.h nucleusChargeDensity:
        // the same shape, axis and scaling; projectChargeDensity), no longer 60,000 random points counted on the grid. Each cell is one small square whose opacity is its
        // share of the brightest cell, in the protons' colour (as before). No smoothing any more: the Gaussian of 2.5 cells (about 0.3 fm) was there only to hide the
        // points' counting noise, and the exact integral has none (a blur would only widen the edge). The grid reaches the radius that holds 99.7 % of the charge (as it did
        // for the points); it is made again only when the nucleus, its axis, the view's angles or the grid's size change, so a still picture costs no integration at all.
        const double half = std::max(densModel_.ext997, 0.5) * kFmLab * pxm * 1.05;   // screen half width of the grid (points)
        // 0.22 stage 6: the grid has 128 cells across, more when the nucleus is drawn big (up to 256), so a cell is never much over 2.5 points
        const int G = std::clamp(int(std::ceil(2 * half / 2.5)), 128, 256);
        const double cell = 2 * half / G;
        const int mode = (d3 || isOrb) ? 0 : (cloud && !hz ? 1 : 2);
        char gk[200];
        std::snprintf(gk, sizeof gk, "%d|%d|%.9f,%.9f,%.9f,%.9f", G, mode, ca, sa, cb, sb);
        if (densGridKey_ != gk) {
            densGridKey_ = gk;
            densGridsMade_++;
            // the picture's axes in the lab frame (Proj above): x = (ca, -sa, 0), y = (sa sb, ca sb, cb), the line of sight (sa cb, ca cb, -sb); side view x, z; top view x, y
            double e1[3] = {1, 0, 0}, e2[3] = {0, 0, 1}, e3[3] = {0, 1, 0};
            if (mode == 0) { e1[0] = ca; e1[1] = -sa; e1[2] = 0; e2[0] = sa * sb; e2[1] = ca * sb; e2[2] = cb; e3[0] = sa * cb; e3[1] = ca * cb; e3[2] = -sb; }
            else if (mode == 2) { e2[1] = 1; e2[2] = 0; e3[1] = 0; e3[2] = 1; }
            projectChargeDensity(densModel_, e1, e2, e3, G, std::max(densModel_.ext997, 0.5) * 1.05 * 2 / G, densGrid_);
        }
        const std::vector<float>& grid = densGrid_;
        float peak = 0;
        for (float v : grid) peak = std::max(peak, v);
        densBlobs_.clear();
        const Color col = css.light ? rgba(196, 52, 84) : rgba(255, 110, 135);   // the protons' colour of the other looks: this is their charge
        for (int y = 0; y < G && peak > 0; y++)
            for (int x = 0; x < G; x++) {
                const float v = grid[y * G + x] / peak;
                if (v < 0.01f) continue;
                render::Renderer2D::Blob b{};
                b.x = float(cx - half + (x + 0.5) * cell); b.y = float(cy - half + (y + 0.5) * cell); b.r = float(cell / 2); b.kind = 2;
                b.c0 = col.withAlpha(0.95f * v);
                densBlobs_.push_back(b);
            }
        R.blobs(densBlobs_.data(), int(densBlobs_.size()), false);
    } else {
        struct Item { double x, y, bx, by, dep; bool isP; int i; double ph; bool q = false; double qf = 0; };
        std::vector<Item> list;
        for (int i = 0; i < int(s.nuc.size()); i++) {
            const Nucleon& q = s.nuc[i];
            // 0.46.4 (outside review): no more "small visual jitter" (a 0.05 fm wobble, made up and not said anywhere): the snapshot is drawn still
            Item it{};
            proj(q.x, q.y, q.z, it.x, it.y, it.dep);
            it.bx = it.x; it.by = it.y;
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

        const bool soft = rn >= 16;   // (0.43: the Hard balls style is gone)
        double dNear = INFINITY;                                           // the front-most nucleon you can see
        if (soft)
            for (const Item& u : list) {
                if (!u.q) continue;
                const double X = cx + u.x * pxm, Y = cy - u.y * pxm;
                if (X > -0.6 * rn && X < w + 0.6 * rn && Y > -0.6 * rn && Y < h + 0.6 * rn && u.dep < dNear) dNear = u.dep;
            }
        if (soft && s.focus >= 0)                                          // dived into a nucleon: the camera focuses on it
            for (const Item& u : list) if (u.i == s.focus && u.q) dNear = u.dep;
        if (s.focus >= 0 && s.focus < int(s.nuc.size())) shownP = s.nuc[s.focus].isP;
        else {
            double best = INFINITY;
            for (const Item& u : list) {
                if (!u.q) continue;
                const double dx = cx + u.x * pxm - cx0, dy = cy - u.y * pxm - cy0, d2 = dx * dx + dy * dy;
                if (d2 < best) { best = d2; shownP = u.isP; }
            }
        }
        const double DOF = 2.2 * kFmLab;                                   // past this depth behind the front, things go out of focus
        auto fogOf = [&](double d) {                                       // depth fog: quarks deeper in the nucleus fade back
            return soft && std::isfinite(dNear) ? 0.5 + 0.5 * std::exp(-std::max(0.0, d - dNear) / (1.8 * kFmLab)) : 1.0;
        };
        // Draw calls go in order; the soft layer collects the blurry things (drawn at half resolution)
        // and is laid over the picture before the sharp things.
        // Draw calls keep their order. The soft layer collects the blurry things (drawn at half
        // resolution) and is laid over the picture in one go, before the sharp things.
        struct Batch { std::vector<Blob> b; bool add = false; };
        std::vector<Batch> softOps;                                        // what goes into the soft layer, in order (0.16 stage 5: blob runs only; rings and lines are blobs too)
        Batch softB, mainB;
        auto closeSoft = [&] {
            if (softB.b.empty()) return;
            g_nst.blobs += long(softB.b.size()); g_nst.blobRuns++;
            softOps.push_back(std::move(softB));
            softB = Batch();
        };
        auto flushMain = [&] { if (!mainB.b.empty()) { g_nst.blobs += long(mainB.b.size()); g_nst.blobRuns++; g_nst.cover += coverOf(mainB.b.data(), int(mainB.b.size()), w, h); R.blobs(mainB.b.data(), int(mainB.b.size()), mainB.add); mainB.b.clear(); } };
        struct Ctx { bool soft; };                                         // "a canvas": the main picture or the soft layer
        // 0.16 stage 3: a shape that lies wholly outside the picture is not made at all. The graphics card would clip it away anyway, so the picture is the same,
        // but at full zoom (x10,000,000) almost every blob, ring and line of the nucleons near the view is outside it. e: how far the shape reaches from (x, y),
        // with 2 points to spare for the smooth edge (a blob's quad reaches r + 1).
        const auto outside = [w, h](double x0, double y0, double x1, double y1, double e) { return std::max(x0, x1) + e < 0 || std::min(x0, x1) - e > w || std::max(y0, y1) + e < 0 || std::min(y0, y1) - e > h; };
        auto put = [&](const Ctx& c, const Blob& b, bool add = false) {
            if (outside(b.x, b.y, b.x, b.y, double(b.r) + 2)) return;
            Batch& t = c.soft ? softB : mainB;
            if (t.add != add && !t.b.empty()) { if (c.soft) closeSoft(); else flushMain(); }
            t.add = add;
            t.b.push_back(b);
        };
        auto lineTo = [&](const Ctx& c, double x0, double y0, double x1, double y1, double wd, Color col) {
            if (outside(x0, y0, x1, y1, std::max(wd, 1.0) / 2 + 2)) return;   // (Renderer2D::line: no caps, at least 1 point wide)
            g_nst.lines++;
            Blob b;
            if (lineBlob(x0, y0, x1, y1, wd, col, b)) put(c, b);           // 0.16 stage 5: into the batch, no flush (put's own test cannot drop it: the blob reaches past the line)
        };
        auto ringTo = [&](const Ctx& c, double x, double y, double r, double wd, Color col) {
            if (outside(x, y, x, y, r + std::max(wd, 1.0) / 2 + 2)) return;    // a ring is up to 720 line pieces (Renderer2D::ring): the "+ Sea" antiquarks at full zoom made 311,076 vertices a frame
            g_nst.ringCalls++;
            put(c, ringBlob(x, y, r, wd, col));                             // 0.16 stage 5: one blob in the batch, not up to 720 line pieces and a flush
        };
        std::vector<std::pair<std::string, std::array<double, 4>>> letters;   // u/d letters: x, y, size, alpha (drawn as text on top)
        std::vector<Color> letterCols;
        auto letter = [&](char f, double x, double y, double size, double a, Color c) { letters.push_back({std::string(1, f), {x, y, size, a}}); letterCols.push_back(c); };

        const Ctx MAIN{false}, SOFT{true};
        // Classic quarks: balls with flux tubes, gluon sparks and sea pairs; only into the main picture or the soft layer as blobs
        auto classic = [&](const Ctx& c, const Nucleon& q, double X, double Y, double A) {
            const QuarkPts Q = quarkPts(q, X, Y, rn, now, proj);
            const double k = Q.k, t = Q.t;
            for (const QPt& o : Q.P) {                                     // flux tubes: a Y from each quark to the middle
                // 0.45 (review, finding 2): the soft band is as wide as the lattice tube, radius 0.38 fm (Bissey et al. 2007; it was 0.16 rn = 0.07 fm in radius, about 5 times too thin),
                // so it is faint; the thin line marks the tube's axis
                {   // a soft band: round Gaussians (the shader's soft dots) along the tube, fading out at twice the radius
                    const double len = std::hypot(X - o.x, Y - o.y), step = 0.08 * k;
                    const int n = std::max(2, int(std::ceil(len / std::max(step, 1.0))));
                    for (int j = 0; j <= n; j++) {
                        Blob b{};
                        b.x = float(o.x + (X - o.x) * j / n); b.y = float(o.y + (Y - o.y) * j / n); b.r = float(2 * kFluxTubeRadiusFm * k); b.kind = 4;
                        b.c0 = withA(css.fg, 0.035 * A);
                        put(c, b);
                    }
                }
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
            static const Color seaU = hexColor("#f0a500"), seaD = hexColor("#0e9488");
            if (p.detail == "+ Sea" && !outside(X, Y, X, Y, 0.75 * k + std::max(1.5, 0.028 * rn) + 2.6))   // virtual quark–antiquark pairs flashing in and out (all within 0.62 + 0.13 fm of the middle)
                for (int j = 0; j < 12; j++) {
                    const double cc = t * 0.8 + j * 0.379, cyc = std::floor(cc), ph = cc - cyc, env = std::pow(std::sin(kPi * ph), 2);
                    const SeaRnd& e = seaRnd(j, cyc);
                    double c0x, c0y, c0d, dvx, dvy, dvd;
                    proj(e.rr * e.sth * e.caz, e.rr * e.sth * e.saz, e.rr * e.cth, c0x, c0y, c0d);
                    const double d = 0.05 + 0.08 * ph;
                    proj(d * e.sdth * e.cdaz, d * e.sdth * e.sdaz, d * e.cdth, dvx, dvy, dvd);
                    // 0.45 (review, finding 7): down and up pairs in the ratio of the d-bar and u-bar momentum, ABM11 at 2 GeV (about 1.26 : 1, quark_tomo.h pkSeaDownShare); until 0.44
                    // 1.5, called "inside SeaQuest's range", which holds only for 0.13 < x < 0.45. Strange pairs are not drawn.
                    const Color col = withA(e.c5 < 1 - pkSeaDownShare() ? seaU : seaD, 0.8 * env * A);
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
        // 0.44, Accurate: no balls. A quark has no definite place inside a nucleon, so the picture is a probability cloud of dots, as for the electrons. Each dot is one piece of the
        // nucleon's MOMENTUM, found at a distance b from its centre and carried by one kind of parton, NOT a quark (quark_tomo.h; native/data/qcd/tomography/README.txt): the kind is
        // picked by its share of the momentum (ABM11, 2 GeV), then its momentum fraction x, then b from the measured width at that x (Diehl and Kroll 2013 for the valence quarks,
        // COMPASS and H1 for the sea, J/psi data for the gluons). So fast quarks (large x) sit near the centre and the slow partons spread out: nucleon tomography. Seen along the
        // motion of a very fast nucleon (the only frame where these densities need no model); round about the axis of view, so drawn in the plane of the screen whatever the camera
        // does. Up warm, down cool, gluons grey, antiquarks as rings of their flavour's colour; Valence hides the gluons and the sea, + Gluons hides the sea (strange and charm, 4.5 %,
        // are never drawn). Each dot lives about two seconds, then is drawn again elsewhere. No colour charge (the owner, 8 Nov 2026): a nucleon is a colour singlet.
        // phase 0 = 'all', 1 = 'soft' (nothing since 0.44: the gluons are dots), 2 = 'sharp' (the dots, faded by fog)
        auto field = [&](const Ctx& c, const Nucleon& q, double X, double Y, double A, int pb, int phase, const Ctx& sc, double fog) {
            (void)sc;
            if (phase == 1) return;
            const double k = rn / kNucleonR, t = now + q.ph, sd0 = std::fmod(q.ph * 1000.0, 9973.0);   // k: pixels per fm
            if (outside(X, Y, X, Y, 2.4 * k)) return;                    // nothing can reach the picture (the dots are kept inside 2.4 fm)
            static const double kUp[3] = {255, 196, 128}, kDown[3] = {150, 182, 230}, kGlue[3] = {222, 226, 238};
            // the kinds drawn and their shares of the momentum (a proton's; a neutron swaps up and down)
            const Pk* kKinds = pkDrawOrder();
            const int nk = p.detail == "Valence" ? 2 : p.detail == "+ Gluons" ? 3 : 7;
            double cum[7], tot = 0;
            for (int i = 0; i < nk; i++) cum[i] = (tot += pkShare(kKinds[i]));
            const int qN = int(std::lround(std::min(1200.0, std::max(150.0, 8.0 * pb))));   // dots per nucleon: 1,200 for one nucleon, 360 each for the 45 nearest (about 16,000 at most)
            const double r = std::min(3.0, std::max(1.3, 0.018 * rn)), Af = A * fog;   // a dot marks a place, it is not an object: a few points at any zoom (as the electron dots)
            double cu[3], cd[3], cg[3];
            if (css.light) { lightQuarkColour(kUp, cu); lightQuarkColour(kDown, cd); cg[0] = 84; cg[1] = 88; cg[2] = 104; }
            else for (int i = 0; i < 3; i++) { cu[i] = kUp[i]; cd[i] = kDown[i]; cg[i] = kGlue[i]; }
            for (int j = 0; j < qN; j++) {
                const double cc = t * 0.5 + j * 0.6180339887, cyc = std::floor(cc), ph = cc - cyc, env = std::pow(std::sin(kPi * ph), 2), sd = sd0 + j * 3.17 + cyc * 57.3;
                const double u0 = rnd(sd) * tot;
                int ki = 0;
                while (ki < nk - 1 && u0 > cum[ki]) ki++;
                const Pk kind = kKinds[ki];
                const double x = pkSampleX(kind, rnd(sd + 1)), wb = pkMeanB2(kind, x);   // (wb, not w: w is the window's width)
                const double b = std::min(2.4, std::sqrt(-wb * std::log(std::max(1e-12, 1 - rnd(sd + 2))))), an = 2 * kPi * rnd(sd + 3);   // a 2D Gaussian with <b^2> = w
                const double px = X + b * k * std::cos(an), py = Y + b * k * std::sin(an);
                if (px > -r && px < w + r && py > -r && py < h + r) g_qDotsOnScreen++;
                // 0.45 (review, finding 10): the gluons, 40 % of the momentum, as bright as the quarks (they were dimmer); the antiquark rings, 7 %, thinner and fainter (they caught the eye most)
                if (kind == Pk::G) { put(c, grad2(px, py, r, rgbaC(cg, 0.9 * env * Af), rgbaC(cg, 0))); continue; }
                const bool uType = kind == Pk::Uv || kind == Pk::Usea || kind == Pk::Ubar, up = uType == q.isP;   // a neutron's d quarks have the proton's u profiles (isospin)
                const double* col = up ? cu : cd;
                if (kind == Pk::Ubar || kind == Pk::Dbar) put(c, ringBlob(px, py, 0.9 * r, std::max(0.8, 0.22 * r), rgbaC(col, 0.65 * env * Af)));
                else put(c, grad2(px, py, r, rgbaC(col, 0.9 * env * Af), rgbaC(col, 0)));
            }
        };
        auto quarks = [&](const Ctx& c, const Nucleon& q, double X, double Y, double qai, int pb, double fg) {
            if (p.qstyle == "Simple") classic(c, q, X, Y, qai * fg);
            else field(c, q, X, Y, qai, pb, 0, c, fg);
        };
        // 0.46.5 (owner: as realistic as possible): the nucleons' glows ADD UP, as their densities do, in the float layer of the electron cloud (beginCloudLayer). Until 0.46.4 they were
        // painted back to front, each nearly opaque (0.93), so a front nucleon hid the ones behind it and the spaces between the front ones showed as dark holes; nothing that sees a
        // nucleus (electrons, photons of very short wavelength) is stopped like that: its picture is the sum along the line of sight. One exposure for the whole nucleus: the
        // brightest place (where most nucleon matter lies on one line) is at full strength, so the brightness everywhere is proportional to the summed profiles (no clipping).
        // The glows are drawn at full resolution (until 0.46.4 they went into the half-resolution soft layer, which blurred them on top of their own soft profile).
        if (rn >= 5) {
            struct G { double X, Y, w; };
            std::vector<G> gl;
            std::vector<Blob> glowB;
            // 0.66.0: a nucleon's glows (its rest profile, kind 1, and the Gaussians of its quarks' density, kind 4) in one quad, a glow stack (kind 10): until 0.66.0 a
            // quad each, at full zoom each over the whole picture (atom.nucleus_zoom: 85 quads, 66 pictures' worth of pixels). The same sum, added up in the same layer.
            static const bool oldGlow = std::getenv("PL_NUC_OLDGLOW") != nullptr;   // (the check's reference: tests/check_6c.py nucglow)
            static const PkGlow glows[3] = {pkGlow(2), pkGlow(3), pkGlow(7)};
            const PkGlow& Gl = glows[p.detail == "Valence" ? 0 : p.detail == "+ Gluons" ? 1 : 2];
            double peak = 0;
            for (int i = 0; i < Gl.n; i++) peak += Gl.weight[i] / Gl.w[i];
            const double kf = rn / kNucleonR;                              // pixels per fm
            double Rmax = 1.9 * rn;
            for (int i = 0; i < Gl.n; i++) Rmax = std::max(Rmax, 2 * std::sqrt(Gl.w[i]) * kf);
            float table[4 * render::Renderer2D::kMaxGlow] = {};            // (radius / Rmax, relative strength, 1: kind 1's profile, 0: a Gaussian)
            int tn = 0;
            table[0] = float(1.9 * rn / Rmax); table[1] = 1; table[2] = 1; tn = 1;
            for (int i = 0; i < Gl.n && tn < render::Renderer2D::kMaxGlow; i++, tn++) { table[4 * tn] = float(2 * std::sqrt(Gl.w[i]) * kf / Rmax); table[4 * tn + 1] = float((Gl.weight[i] / Gl.w[i]) / peak); }
            struct Stack { double X, Y; Color tint; double f, g; };       // f: the profile's strength, g: the Gaussians'
            std::vector<Stack> stacks;
            for (const Item& q : list) {
                const double X = cx + q.x * pxm, Y = cy - q.y * pxm;
                if (X < -2 * rn || X > w + 2 * rn || Y < -2 * rn || Y > h + 2 * rn) continue;
                const double qai = qa * q.qf;
                // 0.45 (review, finding 9): this profile is the nucleon at rest, the Accurate dots are the nucleon seen moving very fast; drawn together the dots seemed to fill only its
                // inner half. As the Accurate quarks fade in, the glow changes into the dots' own density (quark_tomo.h pkGlow: a sum of soft Gaussians with the dots' momentum spread).
                const double gq = (p.qstyle == "Accurate" && q.q) ? std::min(1.0, std::max(0.0, qai)) : 0.0;
                const Color tint = q.isP ? rgba(224, 80, 106) : rgba(150, 150, 146);
                const double af = (0.93 - 0.45 * qai) * (1 - gq);
                if (oldGlow) {                                             // (until 0.66.0, and the check's reference: a blob for each glow)
                if (gq < 1) {
                    Blob b{};
                    b.x = float(X); b.y = float(Y); b.r = float(1.9 * rn); b.kind = 1;
                    b.c0 = withA(tint, float(af));
                    glowB.push_back(b);
                }
                if (gq > 0) {
                    for (int i = 0; i < Gl.n; i++) {                       // a Gaussian exp(-b^2 / w): the shader's soft dot (kind 4) of radius 2 sqrt(w)
                        Blob b{};
                        b.x = float(X); b.y = float(Y); b.r = float(2 * std::sqrt(Gl.w[i]) * kf); b.kind = 4;
                        b.c0 = withA(tint, float(0.55 * gq * (Gl.weight[i] / Gl.w[i]) / peak));
                        glowB.push_back(b);
                    }
                }
                } else if (gq < 1 || gq > 0) stacks.push_back({X, Y, tint, gq < 1 ? af : 0.0, gq > 0 ? 0.55 * gq : 0.0});
                gl.push_back({X, Y, af + 0.55 * gq});                       // its strength at its own centre (the kind 4 sum is close to the kind 1 profile there: the exposure only)
            }
            double top = 0;                                                // the summed strength at each nucleon's centre; the largest sets the exposure
            const double ext = 1.9 * rn;
            for (const G& a : gl) {
                double sum = 0;
                for (const G& b : gl) { const double dx = a.X - b.X, dy = a.Y - b.Y; if (std::fabs(dx) < ext && std::fabs(dy) < ext) sum += b.w * render::Renderer2D::fuzzAt(float(std::hypot(dx, dy) / ext)); }
                top = std::max(top, sum);
            }
            const float k = top > 0.95 ? float(0.95 / top) : 1.f;
            for (Blob& b : glowB) b.c0.a *= k;
            if (!stacks.empty()) {
                // the light a stack adds at rho (units of Rmax) from its centre, as the shader sums it; it falls with rho. A stack whose light on the picture stays below
                // its share of half a level (0.5 / 255 shared by all the stacks) is left out, and a stack's quad reaches only as far as its light is above that share:
                // the left-out light is under half a level at every pixel together
                const auto light = [&](const Stack& t, double rho) {
                    double a = 0;
                    for (int i = 0; i < tn; i++) {
                        const double di = rho / table[4 * i];
                        if (table[4 * i + 2] > 0.5) a += t.f * table[4 * i + 1] * render::Renderer2D::fuzzAt(float(di));
                        else if (di <= 1) a += t.g * table[4 * i + 1] * (std::exp(-4 * di * di) - 0.0183156) * 1.018657;
                    }
                    return a * k;
                };
                const double share = 0.5 / 255 / double(stacks.size());
                for (const Stack& t : stacks) {
                    const double nx = std::max(0.0, std::max(-t.X, t.X - w)), ny = std::max(0.0, std::max(-t.Y, t.Y - h));   // the picture's nearest point
                    if (light(t, std::hypot(nx, ny) / Rmax) <= share) { g_nst.culled++; continue; }
                    double lo = 0, hi = 1;
                    if (light(t, 1) > share) lo = 1;
                    else for (int it = 0; it < 24; it++) { const double m = 0.5 * (lo + hi); (light(t, m) > share ? lo : hi) = m; }
                    Blob b{};
                    b.x = float(t.X); b.y = float(t.Y); b.r = float(Rmax); b.kind = 10;
                    b.c0 = withA(t.tint, float(t.g) * k); b.fx = float(t.f) * k; b.fy = float(std::min(1.0, hi));
                    glowB.push_back(b);
                }
            }
            if (!glowB.empty()) {
                g_nst.blobs += long(glowB.size()); g_nst.blobRuns++; g_nst.cover += coverOf(glowB.data(), int(glowB.size()), w, h); g_nst.layers++;
                R.beginCloudLayer();
                if (oldGlow) R.blobs(glowB.data(), int(glowB.size()), true);   // added up in the float layer (colour and coverage)
                else R.glowStacks(glowB.data(), int(glowB.size()), true, table, tn);
                R.endCloudLayer(!css.light);                               // dark page: the light is added; light page: laid over as one sheet
            }
        }
        struct Sharp { const Item* it; double X, Y, qai; int pb; double a0; };
        std::vector<Sharp> sharp;
        if (rn > 0) g_qViewFm = w / (rn / kNucleonR);                     // (0.45: also when every nucleon is off screen)
        for (const Item& q : list) {
            const double X = cx + q.x * pxm, Y = cy - q.y * pxm;
            if (X < -2 * rn || X > w + 2 * rn || Y < -2 * rn || Y > h + 2 * rn) continue;   // off screen
            if (rn >= 5) s.hits.push_back({X, Y, rn, q.dep, q.i});
            const double qai = qa * q.qf;
            const int pb = std::max(45, int(std::lround(700.0 / std::max(nq, 1))));   // its gluon-particle budget
            const bool far = soft && std::isfinite(dNear) && q.dep - dNear > DOF;    // out of focus: whole into the blurred layer
            const Nucleon& nu = s.nuc[q.i];
            if (rn < 5) {                                                  // tiny nucleons: a flat dot is enough
                put(MAIN, flat(X, Y, rn * 1.3, withA(hexColor(q.isP ? "#e0506a" : "#969692"), 0.9)));
            }                                                              // (0.46.5: the fuzzy glow of each nucleon is drawn before this loop, all of them adding up: see glowB)
            if (!q.q) continue;
            if (!soft) quarks(MAIN, nu, X, Y, qai, pb, 1);
            else if (far) quarks(SOFT, nu, X, Y, qai, pb, fogOf(q.dep));
            else { if (p.qstyle != "Simple") field(MAIN, nu, X, Y, qai, pb, 1, SOFT, 1); sharp.push_back({&q, X, Y, qai, pb, 0}); }
        }
        // the soft layer goes on in one go (everything drawn into it so far), then the sharp things on top
        if (soft) {
            flushMain();
            closeSoft();
            R.beginSoftLayer(1.0f);                                     // (0.46.5: full resolution; 0.5 blurred what went in. The far quarks still fade with depth: fogOf)
            for (const Batch& b : softOps) { g_nst.cover += coverOf(b.b.data(), int(b.b.size()), w, h); R.blobs(b.b.data(), int(b.b.size()), b.add); }
            g_nst.layers++;
            R.endSoftLayer();
            for (const Sharp& sh : sharp) {
                const double fg = fogOf(sh.it->dep);
                const Nucleon& nu = s.nuc[sh.it->i];
                if (p.qstyle == "Simple") quarks(MAIN, nu, sh.X, sh.Y, sh.qai, sh.pb, fg);
                else field(MAIN, nu, sh.X, sh.Y, sh.qai, sh.pb, 2, MAIN, fg);
            }
        }
        flushMain();
        for (std::size_t i = 0; i < letters.size(); i++) {
            const auto& L = letters[i].second;
            T.draw(letters[i].first, L[0], L[1], L[2], withA(letterCols[i], L[3]), 0.5);
        }
    }
    g_nst.cpuMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - nucT0).count();
    if (s.layer == "Quarks" && qa >= 0.5) {                               // mass budget: the quarks' own masses are only about 1% of a nucleon's
        // 0.43: left of the Controls panel and above the shell's readout (until 0.42 at the window's right edge, where the new UI's panel hid it)
        const double xr = w - (lab.insetRight > 0 ? 44 : 12) - lab.insetRight, bw = std::min(230.0, 0.5 * (w - lab.insetRight)), bx = xr - bw, by = h - 62 - std::max(lab.reserveRight, lab.insetBottomLeft);   // (and above the fact line at the bottom left, which can reach across)
        T.draw(nucleonMassBarText(shownP), xr, by - 6, 11, css.muted, 1, &css.card);
        R.rect(bx, by, bw, 8, css.line);
        R.rect(bx, by, bw, 8, withA(css.accent, 0.55));
        R.rect(bx, by - 2, std::max(2.0, valenceQuarkShare(shownP) * bw), 12, hexColor("#f0a500"));   // uud 9.0 of 938.3 MeV, udd 11.6 of 939.6 MeV (0.52.1: was 0.0096 for both)
        if (p.qstyle == "Accurate" && g_qDotsOnScreen < 20 && g_qViewFm > 0) {   // 0.45 (review, finding 8): zoomed in so far that (almost) no dot is in the window: say so instead of a legend
            T.draw("zoomed in closer than the quark picture can show: this view is " + fmt::fixed(g_qViewFm, g_qViewFm < 0.1 ? 3 : 2) + " fm across, a proton about 1.7 fm. Zoom out to see its quarks.",
                   xr, by - 26, 11, css.muted, 1, &css.card);
        } else if (p.qstyle == "Accurate") {                               // 0.44: what the dots are (quark_tomo.h)
            std::vector<std::string> key = {"each dot: a piece of the momentum, carried by an up (warm) or a down (cool) quark here, not a quark",
                                            "fast quarks near the centre, slow ones spread out (seen along a very fast nucleon's path)"};
            if (p.detail != "Valence") key.push_back("grey: the gluons (about 40 % of the momentum, probed at 2 GeV)");
            if (p.detail == "+ Sea") key.push_back("rings: the sea's antiquarks (with their sea quarks, dots)");
            for (std::size_t i = 0; i < key.size(); i++) T.draw(key[i], xr, by - 26 - 16.0 * double(key.size() - 1 - i), 11, css.muted, 1, &css.card);
        }
    }
    g_marks.label[0] = g_marks.label[1] = g_marks.label[2] = g_marks.label[3] = 0;
    if (!bohr && (s.layer == "Nucleus" || (s.layer == "Atom" && !lab.shellUi))) {   // labels only while the whole nucleus is in view (0.22 stage 6, new UI: not at the atom's size, where the chip, the card and the info line already name it and the label sat on the cloud's brightest part)
        const double off = std::min(std::max(Rpx * 1.25, 3.0) + 12, h / 2 - 22);
        const Color olHalo = withA(css.card, 0.9);
        const std::string nl = nuclideLabel(p.Z, p.N) + "  ·  " + std::to_string(p.Z) + "p, " + std::to_string(p.N) + "n";
        { const double lw = T.width(nl, 12) + 4; g_marks.label[0] = cx - lw / 2; g_marks.label[1] = cy + off - 6; g_marks.label[2] = lw; g_marks.label[3] = 18; }   // (stage 5h: for the automatic checks; points)
        T.draw(nl, cx, cy + off + 8, 12, css.fg, 0.5, &olHalo);   // with a thin edge in the page colour, so it reads over the cloud dots
    }
    if (lab.shellUi) s.infoBox.reset();                                    // (the new UI: no button in the picture)
    else {   // the ⓘ More / Less info button, bottom left above the scale bar
        const std::string t = lab.moreShown() ? "Less info" : "More info";
        const double ih = 28, ir = 8, bw = T.width(t, 13, true) + 2 * ir + 26, bx = 12, by = h - 40 - ih;
        s.infoBox = Box{bx, by, bw, ih};
        const auto P = [&](double x, double y) { return ImVec2(float(ox + x * uiScale), float(oy + y * uiScale)); };
        const auto U = [](Color c) { return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, c.a)); };
        dl->AddRectFilled(P(bx + 0.5, by + 0.5), P(bx + bw - 0.5, by + ih - 0.5), U(css.card), ui::corner(float(4 * uiScale)));   // (0.38.1: the fill has the edge's 4 px corners, not a pill; square in Monochrome)
        dl->AddRect(P(bx + 0.5, by + 0.5), P(bx + bw - 0.5, by + ih - 0.5), U(css.accent), ui::corner(float(4 * uiScale)), 0, float(1.25 * uiScale));   // (0.38: 4 px, no pill; 0.38.1: square in Monochrome)
        const double icx = bx + 10 + ir, icy = by + ih / 2;
        dl->AddCircle(P(icx, icy), float(ir * uiScale), U(css.accent), 0, float(1.6 * uiScale));                 // the circle
        dl->AddCircleFilled(P(icx, icy - 0.45 * ir), float(1.2 * uiScale), U(css.accent));                     // the dot of the i
        dl->AddLine(P(icx, icy - 0.05 * ir), P(icx, icy + 0.5 * ir), U(css.accent), float(1.6 * uiScale));      // its stem
        T.draw(t, icx + ir + 8, icy + 0.5, 13, css.accent, 0, nullptr, true, true);
    }
    if (!cloud && !eBehind && !lab.manyOn()) drawE();
    if (s.photon) {   // 0.49.2: the photon is a wave train as long as its jump: it streams out of the atom while an emit jump runs, and into it while an absorb jump runs
        const Photon& ph = *s.photon;
        const bool emit = ph.kind == "emit";
        const double dx = std::cos(ph.ang), dy = std::sin(ph.ang), nx = -dy, ny = dx;
        const double Rr = std::min((w / 2 - 30) / std::max(0.01, std::fabs(dx)), (h / 2 - 30) / std::max(0.01, std::fabs(dy)));   // to the picture's edge
        const double v = Rr / std::max(1e-6, ph.dur);                      // screen speed, px/s (slowed down: the real light crosses an atom in about 1e-18 s)
        const double len = std::max(60.0, v * ph.jdur);                    // the train's length on screen: its jump's length (Reduced motion: a short packet)
        const double front = emit ? v * ph.t : Rr - v * ph.t;              // distance of the front from the nucleus (emit: outward; absorb: inward, 0 when its jump starts)
        const double tail = emit ? front - len : front + len;
        const double lo = std::max(0.0, std::min(front, tail)), hi = std::min(Rr, std::max(front, tail));   // the part outside the atom and on the picture
        const double lamPx = std::clamp(3.5 * std::pow(ph.lam / 100.0, 0.35), 2.5, 14.0);   // drawn wavelength: grows with the real one (UV short, infrared long), NOT to scale
        const Color pc = cssColor(ph.col);
        // 0.49.3: the field of ONE photon (atom_sim.h kJumpTauS). Emitted: a sharp front (the decay starts at once) and an amplitude falling as exp(-t / 2 tau) behind it, t the time the part
        // was given out. Absorbed: the time reverse (the photon that is taken in completely): it rises towards its sharp tail. xi: 0 at the tail, 1 at the front. Reduced motion: a short sin^2 packet.
        const double span = ph.jdur > 0 ? ph.jdur / AtomLab::kJumpTauS : 0;
        const auto env = [&](double r) {
            const double xi = (r - tail) / (front - tail);
            if (xi <= 0 || xi >= 1) return 0.0;
            if (span <= 0) return std::pow(std::sin(kPi * xi), 2);
            return std::exp(-0.5 * span * (emit ? 1 - xi : xi));
        };
        if (hi > lo) {
            const double passes[2][2] = {{7, 0.22}, {2.2, 0.95}};
            for (const auto& pa : passes) {
                Pen pen{R, pa[0], withA(pc, pa[1])};
                for (double r = lo; r <= hi + 1e-9; r += 1.5) {
                    const double off = 8 * env(r) * std::sin(2 * kPi * (r - (emit ? 1 : -1) * v * ph.t) / (lamPx * 2.5));   // crests move with the train
                    pen.to(cx + dx * r + nx * off, cy + dy * r + ny * off);
                }
            }
        }
        const double atAtom = env(0.0);                                    // how much of the train is passing the nucleus now: the atom glows in the photon's colour
        if (atAtom > 0.01) R.ring(cx, cy, 14 + 6 * atAtom, 3, withA(pc, 0.55 * atAtom));
        const std::string tag = bandTag(ph.lam);
        const std::string ptxt = (ph.name.empty() ? "" : ph.name + "  ·  ") + fmtLam(ph.lam) + tag + (tag.empty() ? "" : ", invisible to the eye") + "  ·  " + fmtEV(ph.E) + "  ·  wave not to scale";   // (0.22: drawn grey)
        // the label past the train's outer end (not across the wave): left-aligned when the train points right, right-aligned when it points left, kept on the picture
        const double far = hi > lo ? hi : std::clamp(front, 0.0, Rr), ex = cx + dx * far, ey = cy + dy * far;
        const double tw = T.width(ptxt, 12), right = dx >= 0 ? 1 : 0;
        const double lx = ex + (dx >= 0 ? 10 : -10), ly = ey + (dy >= 0 ? 18 : -10);
        T.draw(ptxt, right > 0 ? std::max(8.0, std::min(w - tw - 8, lx)) : std::max(tw + 8, std::min(w - 8, lx)), std::max(96.0, std::min(h - 40, ly)), 12, css.fg, right > 0 ? 0 : 1);
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
    {   // radioactive decay: what flies out, and the recent decays
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
                const double y = h - 84 - lab.insetBottomLeft - double(n - 1 - i) * 16;   // above the ⓘ button and the scale bar (and, new UI, above the toolbar row and the fact line)
                T.draw(lines[first + i].first, 12, y, 12, withA(css.fg, std::min(1.0, (14000 - (nowMs - lines[first + i].second)) / 2000)), 0, &css.card);
            }
        }
    }
    {   // the scale bar: the largest 1-2-5 length that fits, in Å, pm or fm
        const double pxA = pxm * 1e-10;                                    // points per Å
        double bar = 1e-8;
        for (int e = -8; e <= 1; e++) for (int mm : {1, 2, 5}) if (mm * std::pow(10.0, e) * pxA <= 0.3 * w) bar = mm * std::pow(10.0, e);
        auto nf = [](double v) { return numText(v, 2); };
        // 0.46.6 (outside review: a ruler on a stretched picture): with Shells enlarged only the outer edge is at its true distance from the centre, so the bar says so
        const std::string edgeOnly = lab.shellsEnlarged() && lab.s.layer == "Atom" ? "  (true only at the outer edge)" : "";
        const std::string label = (bar >= 0.1 ? nf(bar) + " Å" : bar * 100 >= 1 ? nf(bar * 100) + " pm" : nf(bar * 1e5) + " fm");
        const double yb = h - 18 - lab.insetBottomLeft;                    // (new UI: above the toolbar row and the fact line)
        R.line(12, yb, 12 + bar * pxA, yb, 2, css.muted);
        T.draw(label + "  =  " + metres(bar * 1e-10) + edgeOnly, 12, yb - 6, 12, css.muted);
        { const double lw = std::max(T.width(label + "  =  " + metres(bar * 1e-10) + edgeOnly, 12), bar * pxA); g_marks.scale[0] = 12; g_marks.scale[1] = yb - 20; g_marks.scale[2] = lw; g_marks.scale[3] = 24; }   // (5q: for the automatic checks; points: the label above the bar and the bar)
    }
    g_marks.info[0] = g_marks.info[1] = g_marks.info[2] = g_marks.info[3] = 0;
    lab.infoPlateRight = lab.infoPlateBottom = 0;
    if (!lab.infoHidden) {   // the readout at the top left, on a soft plate
        const double isz = lab.infoSize;                                   // 13 points; the new UI 14 (stage 5h)
        g_marks.infoPx = isz * g_textScale * uiScale;
        const double bottom = h - 44 - lab.insetBottomLeft;                     // 0.25: the plate stays above the scale bar (and the new UI's toolbar row and fact line)
        const bool below = lab.infoBelowDown >= 0 && bottom - (4 + lab.insetTop + lab.infoBelowDown) >= 2 * (isz + 4) + 10;   // no room beside the card: below it, when two rows fit
        const double infoDown = below ? lab.infoBelowDown : lab.infoDown;
        const double maxW = !below && lab.infoRight >= 0 ? std::max(40.0, lab.infoRight - 34) : w - 24 - 56 - lab.insetRight;   // (new UI, stage 5g: the plate stops before the subject card and the phase ring)
        std::vector<std::string> rows;
        for (const std::string& t : lab.info()) {                          // wrap at " · " first
            std::string cur;
            std::size_t a = 0;
            for (;;) {
                const std::size_t b = t.find("  ·  ", a);
                const std::string part = t.substr(a, b == std::string::npos ? std::string::npos : b - a);
                const std::string nx = cur.empty() ? part : cur + "  ·  " + part;
                if (!cur.empty() && T.width(nx, isz) > maxW) { rows.push_back(cur); cur = part; } else cur = nx;
                if (b == std::string::npos) break;
                a = b + std::string("  ·  ").size();   // (0.22 stage 6: 6 bytes, "·" is 2 in UTF-8; it was 5, so every wrapped part began with a stray space)
            }
            rows.push_back(cur);
        }
        for (std::size_t i = 0; i < rows.size(); i++) {                    // a single long part still has to fit: break it at spaces
            if (T.width(rows[i], isz) <= maxW) continue;
            std::string line, rest;
            std::size_t a = 0;
            // 0.39.3: a number and its unit stay together ("0.79 Å", "−13.60 eV": the Å was left alone on the next line): no break at a space after a
            // digit when the next word is short (at most 4 bytes: Å, eV, keV, %), as a unit is
            const auto nextBreak = [&](const std::string& t, std::size_t from) {
                for (std::size_t b = t.find(' ', from); b != std::string::npos; b = t.find(' ', b + 1)) {
                    const std::size_t e = t.find(' ', b + 1);
                    const std::size_t next = (e == std::string::npos ? t.size() : e) - (b + 1);
                    if (!(b > from && std::isdigit(static_cast<unsigned char>(t[b - 1])) && next > 0 && next <= 4 && !std::isdigit(static_cast<unsigned char>(t[b + 1])))) return b;   // (0.46: a unit never starts with a digit: "1s2 2s2 2p6 ..." breaks; it ran under the card)
                }
                return std::string::npos;
            };
            for (;;) {
                const std::size_t b = nextBreak(rows[i], a);
                const std::string wd = rows[i].substr(a, b == std::string::npos ? std::string::npos : b - a);
                const std::string nx = line.empty() ? wd : line + " " + wd;
                if (!line.empty() && T.width(nx, isz) > maxW) { rest = rows[i].substr(a); break; }
                line = nx;
                if (b == std::string::npos) break;
                a = b + 1;
            }
            rows[i] = line;
            if (!rest.empty()) rows.insert(rows.begin() + i + 1, rest);
        }
        if (ui::shellLogOn()) {   // (PL_LOG_SHELL: the rows as wrapped, for the checks; nothing otherwise)
            std::string all = "inforows";
            for (const std::string& r : rows) all += " | " + r;
            ui::auditNote(all);
        }
        // 0.20: in the Nucleus, Nucleon and Quarks views the nucleus fills the picture, and a long info text was drawn over it (U-238). Then the text folds: the rows that fit above
        // the nucleus stay, the rest is behind a "Show n more lines" row that a click opens (and folds again): nothing is lost, the nucleus is clear until the owner asks
        {
            const double top = 4 + lab.insetTop + infoDown, rowH = isz + 4;
            double tw = 0;
            for (const std::string& t : rows) tw = std::max(tw, T.width(t, isz));
            const double bw0 = tw + 20, bh0 = rowH * rows.size() + 10;
            const double qx = std::clamp(cx, 6.0, 6 + bw0), qy = std::clamp(cy, top, top + bh0);   // the plate's point nearest the nucleus centre
            const bool deep = !bohr && s.layer != "Atom", over = deep && std::hypot(qx - cx, qy - cy) < Rpx;
            g_marks.nucleus[0] = cx; g_marks.nucleus[1] = cy; g_marks.nucleus[2] = deep ? Rpx : 0;
            const bool tall = top + bh0 > bottom;                               // (a small window, or the text below the card: it would run under the toolbar)
            g_marks.infoFoldable = over || tall || (deep && lab.infoOpen);
            if (g_marks.infoFoldable && rows.size() > 1) {
                if (lab.infoOpen) rows.push_back("Fold this text (click)");
                else {
                    const double room = tall && !over ? bottom : cy - Rpx - 8;     // the rows that fit above the bottom, or above the nucleus
                    const int fit = static_cast<int>(std::floor((room - top - 10) / rowH)) - 1;   // less the "Show" row
                    const std::size_t keep = static_cast<std::size_t>(std::clamp(fit, 1, static_cast<int>(rows.size()) - 1));
                    const std::size_t more = rows.size() - keep;
                    rows.resize(keep);
                    rows.push_back("Show " + std::to_string(more) + (more == 1 ? " more line" : " more lines") + " (click)");
                    g_marks.infoFolded = static_cast<int>(more);
                }
            }
        }
        bool any = false;
        double mw = 0;
        for (const std::string& t : rows) { if (!t.empty()) any = true; mw = std::max(mw, T.width(t, isz)); }
        if (any) {
            const double bw = mw + 12 + 8, bh = (isz + 4) * rows.size() + 10;
            g_marks.info[0] = 6; g_marks.info[1] = 4 + lab.insetTop + infoDown; g_marks.info[2] = bw; g_marks.info[3] = bh;   // (for the automatic checks: the info plate, in points)
            lab.infoPlateRight = 6 + bw; lab.infoPlateBottom = 4 + lab.insetTop + infoDown + bh;   // 0.46.1: the shell keeps the picture's top below it (next frame)
            dl->AddRectFilled(ImVec2(float(ox + 6 * uiScale), float(oy + (4 + lab.insetTop + infoDown) * uiScale)), ImVec2(float(ox + (6 + bw) * uiScale), float(oy + (4 + lab.insetTop + infoDown + bh) * uiScale)),
                              ImGui::ColorConvertFloat4ToU32(ImVec4(css.card.r, css.card.g, css.card.b, 0.88f)), ui::corner(float(4 * uiScale)));   // 0.38: a 4 px plate with a 1 px edge (the owner's new look)
            dl->AddRect(ImVec2(float(ox + 6 * uiScale), float(oy + (4 + lab.insetTop + infoDown) * uiScale)), ImVec2(float(ox + (6 + bw) * uiScale), float(oy + (4 + lab.insetTop + infoDown + bh) * uiScale)),
                        ImGui::ColorConvertFloat4ToU32(ImVec4(css.fg.r, css.fg.g, css.fg.b, 0.10f)), ui::corner(float(4 * uiScale)), 0, 1.0f);   // (0.38.1: ui::corner, so Monochrome is square)
        }
        // on their see-through panel the rows need no halo (in light themes its 8 copies left faint grey marks between letters)
        for (std::size_t i = 0; i < rows.size(); i++) T.draw(rows[i], 12, 8 + isz + lab.insetTop + infoDown + (isz + 4) * i, isz, g_marks.infoFoldable && i + 1 == rows.size() ? css.accent : i == 0 ? css.fg : css.muted, 0, any ? nullptr : &css.card);   // 0.38: the first row (the state and its energy) in the text colour
    }
}

}  // namespace atom
