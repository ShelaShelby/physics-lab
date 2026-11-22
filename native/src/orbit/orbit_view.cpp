#include "orbit_view.h"
#include "orbit/label_clip.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "format.h"
#include "imgui.h"
#include "alphacen.h"
#include "kepler.h"
#include "lines_check.h"
#include "orbit_lines.h"
#include "far_masses.h"
#include "marker_colors.h"
#include "giant_rings.h"
#include "saturn_rings.h"
#include "ring_check.h"
#include "ring_draw.h"
#include "saturn_profile.h"
#include "shapes.h"
#include "moon_sats.h"
#include "moon_orientation.h"
#include "iau_rotation.h"
#include "sky.h"
#include "spin.h"
#include "sun_facts.h"
#include "units.h"
#include "ui/look.h"
#include "render/shading.h"

namespace ui { void auditPictureText(float px); void auditRect(const std::string& name, float x, float y, float w, float h); }   // ui/shell_test.cpp: the new UI's PL_LOG_SHELL list of picture label sizes and rectangles

namespace orbit {

using namespace units;
using render::Color;
using render::hexColor;
using render::rgba;

namespace {

// The picture's colours: the dark picture's by default; with Stars Off they follow the theme. Set each frame.
Color kBg, kFg, kMuted, kCard, kLine;
Color fromRgb(const app::Rgb& c) { return {c.r, c.g, c.b, 1}; }
void setPictureColours(bool sky, const app::ThemeColors& t) {
    if (sky) { kBg = rgba(4, 6, 13); kFg = rgba(230, 235, 245); kMuted = rgba(154, 166, 189); kCard = rgba(11, 15, 26); kLine = {1, 1, 1, 0.22f}; }
    else { kBg = fromRgb(t.bg); kFg = fromRgb(t.fg); kMuted = fromRgb(t.muted); kCard = fromRgb(t.card); kLine = fromRgb(t.line); }   // the theme's page colours
}
bool g_skyHalo = true;
double g_minTextU = 0, g_minTextPx = 0;   // 0.15 stage 3: the new UI's smallest picture text (0: none)
double g_textScale = 1;   // Settings → Text size   // the thin dark edge round labels is only needed on top of the stars
const Color kHL = hexColor(marks::kHighlight);   // the "look here" pink: nothing else in the picture is this colour

// 0.49.4: the Sun's colour comes from its measured spectrum (sun_facts.h kSunSrgb); the typed star-colour table with no source (5772 K gave (247, 207, 72)) is gone.
static Color sunColour() { return rgba(sunfacts::kSunSrgb[0], sunfacts::kSunSrgb[1], sunfacts::kSunSrgb[2]); }

// A circle far bigger than the picture: is the picture inside it, outside it, or which
// short stretch of angles (screen angles, y down) can be seen?
enum class Win { All, In, Out, Arc };
Win circWin(double W, double H, double x, double y, double r, double& a0, double& a1) {
    const double dg = 1.5 * std::hypot(W, H) / 2 + 20, d = std::hypot(x - W / 2, y - H / 2);
    if (d + dg < r) return Win::In;
    if (d - dg > r) return Win::Out;
    const double th = std::atan2(H / 2 - y, W / 2 - x), dl = std::min(kPi, std::asin(std::min(1.0, dg / r)) + 0.02);
    a0 = th - dl; a1 = th + dl;
    return Win::Arc;
}
std::string fmtLy(double v) { return v < 10 ? fmt::fixed(v, 2) : v < 100 ? fmt::fixed(v, 1) : fmt::commas(std::stod(fmt::precision(v, 3))); }

bool bigR(double W, double H, double r) { return r > 6 * (1.5 * std::hypot(W, H) / 2 + 20); }

struct ScaleBar { double len = 0; std::string main, light; };
// the scale bar: how long a round distance is at this zoom, and how long light takes to cross it (0.73.0: a function, the Alpha Centauri scene uses it too)
ScaleBar scaleBarFor(double px, bool d3) {
    const double kmPerPt = 1 / (px * kKm), uf = fmt::imperial() ? fmt::kMilesPerKm : 1;   // the bar's length is a round number in the chosen unit
    const auto round125 = [](double raw) { const double pw = std::pow(10.0, std::floor(std::log10(raw))), mm = raw / pw; return pw * (mm >= 5 ? 5 : mm >= 2 ? 2 : 1); };
    double ku = round125(100 * kmPerPt * uf), km = ku / uf;
    // 0.22 stage 7: once the label is in AU, the bar is a round number of AU (before: a round number of km or miles written in AU, so 500 million km = 3.34 AU was labelled "3 AU")
    const bool inAU = ku >= 1e6 && km * kKm / kAU >= 0.05;
    if (inAU) { km = round125(100 * kmPerPt * kKm / kAU) * kAU / kKm; ku = km * uf; }
    const double len = km / kmPerPt, au = km * kKm / kAU, sec = km * kKm / kLightSpeed;
    const char* unm = fmt::imperial() ? "mi" : "km";
    const auto nf = [](double v) { return v >= 1 ? fmt::commas(v) : fmt::precision(v, 1); };
    // 0.39.2: a light time under 10 units keeps two figures (10 AU is 1.4 hours, 83 minutes; it said "1 hour", 28 % short); "1" alone is singular
    const auto pl = [&](double v, const char* u) { const std::string n = v < 10 ? fmt::precision(v, 2) : nf(v); return n + " " + u + (n == "1" ? "" : "s"); };
    const std::string main = std::string(d3 ? "≈ " : "") + (ku < 1e6 ? nf(ku) + " " + unm : !inAU ? nf(ku / 1e6) + " million " + unm : nf(au) + " AU");
    const std::string lt = sec < 1 ? nf(sec * 1000) + " ms" : sec < 60 ? pl(sec, "second") : sec < 3600 ? pl(sec / 60, "minute")
                 : sec < 86400 ? pl(sec / 3600, "hour") : sec < 86400 * 365.25 ? pl(sec / 86400, "day") : fmt::precision(sec / 86400 / 365.25, 2) + " years";
    return {len, main, lt};
}



// Text drawn with Dear ImGui on top of the picture. Positions are in picture points with
struct TextPainter {
    ImDrawList* dl;
    ImFont* font;
    double originX, originY, s;

    static float pixels(double size, double s) { return static_cast<float>(std::max(std::max(size, g_minTextU) * g_textScale * s, g_minTextPx)); }   // the new UI's floor; 0 and 0 change nothing
    double width(const std::string& t, double size) const {
        return font->CalcTextSizeA(pixels(size, s), FLT_MAX, 0, t.c_str()).x / s;
    }
    // align: 0 left, 0.5 centre
    // haloColor with alpha < 0: the dark edge for text on the night sky (left out when the stars are off)
    void text(const std::string& t, double x, double y, double size, Color c, double align = 0.5, bool halo = true, Color haloColor = {4 / 255.f, 6 / 255.f, 13 / 255.f, -0.85f}) {
        if (haloColor.a < 0) { if (!g_skyHalo) halo = false; haloColor.a = -haloColor.a; }
        const float px = pixels(size, s);
        if (g_minTextPx > 0) ui::auditPictureText(px);      // the new UI: PL_LOG_SHELL lists the sizes of the picture's own labels (nothing otherwise)
        const float ascent = font->GetFontBaked(px)->Ascent;
        const double w = width(t, size);
        const float X = static_cast<float>(originX + (x - w * align) * s), Y = static_cast<float>(originY + y * s) - ascent;
        if (halo) {                                   // a thin dark edge so text stays readable on anything
            const ImU32 hc = ImGui::ColorConvertFloat4ToU32(ImVec4(haloColor.r, haloColor.g, haloColor.b, haloColor.a * c.a));
            const float o = static_cast<float>(1.2 * s);
            for (int k = 0; k < 8; k++) {
                const float a = k * 0.785398f;
                dl->AddText(font, px, ImVec2(X + o * std::cos(a), Y + o * std::sin(a)), hc, t.c_str());
            }
        }
        dl->AddText(font, px, ImVec2(X, Y), ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, c.a)), t.c_str());
    }
};

// the axis gizmo at (gx, gy): which way x, y and z point on screen (0.73.0: a function, the Alpha Centauri scene uses it too)
void drawAxesGizmo(render::Renderer2D& R, TextPainter& T, double gx, double gy, double ca, double sa, double cb, double sb, double originX, double originY, double uiScale) {
    ui::auditRect("axes", static_cast<float>(originX + (gx - 40) * uiScale), static_cast<float>(originY + (gy - 40) * uiScale), static_cast<float>(80 * uiScale), static_cast<float>(80 * uiScale));   // 0.22: for the automatic checks (no ring label on it)
    const auto axis = [&](double X, double Y, double Z, double& sx, double& sy) { const double x1 = X * ca - Y * sa, y1 = X * sa + Y * ca; sx = 24 * x1; sy = 24 * (y1 * cb - Z * sb); };
    struct Ax { const char* n; double X, Y; Color c; } ax[] = {{"x", 0, 0, hexColor(marks::kAxisX)}, {"y", 0, 0, hexColor(marks::kAxisY)}, {"z", 0, 0, hexColor(marks::kAxisZ)}};
    axis(1, 0, 0, ax[0].X, ax[0].Y); axis(0, 1, 0, ax[1].X, ax[1].Y); axis(0, 0, 1, ax[2].X, ax[2].Y);
    for (const auto& a : ax) {
        if (std::hypot(a.X, a.Y) < 5) {               // pointing at you: a dot
            R.ring(gx, gy, 3.5, 1.5, a.c);
            R.disc(gx, gy, 1.2, a.c);
            T.text(a.n, gx + 11, gy + 4, 11, a.c, 0.5, false);
            continue;
        }
        R.line(gx, gy, gx + a.X, gy - a.Y, 1.5, a.c);
        T.text(a.n, gx + a.X * 1.35, gy - a.Y * 1.35 + 4, 11, a.c, 0.5, false);
    }
}

// The night sky behind everything. It is so far away that it does not move
// when you zoom or move the view, only when you turn the 3D view. Stars are grouped by colour and
// size, and each group is one instanced draw call. lv = how bright the sky is drawn (0–1).
void drawSky(render::Renderer2D& R, TextPainter& T, double W, double H, double ca, double sa, double cb, double sb, ConsMode cons, double lv, const Sky& S = skyData()) {   // 0.73.0: S, the sky seen from where the scene is
    const double cx = W / 2, cy = H / 2, diag = std::hypot(W, H) / 2, K = diag / 1.75;   // K: points per radian away from the middle (100° reaches the corner)
    // a direction -> screen: the angle from straight ahead becomes the distance from the middle (orbit/sky.cpp)
    const auto scr = [&](const Vec3& v, double& X, double& Y) { return skyScreen(v, W, H, ca, sa, cb, sb, X, Y); };
    // The Milky Way (0.26): the Gaia photo of the whole sky.
    static std::vector<BandPoint> band;
    {
        // 0.26 (owner's choice, 5 Nov 2026): the smooth drawn band is replaced by ESA's Gaia all-sky colour map (render/planet_maps.h), drawn per pixel by the
        // renderer through the same map as the stars (orbit/sky.cpp skyViewToGalactic). The band's points are still worked out: its label goes on them.
        milkyWayBand(S, W, H, ca, sa, cb, sb, band);
        double M[9];
        skyViewToGalactic(S, ca, sa, cb, sb, M);
        float Mf[9];
        for (int k = 0; k < 9; k++) Mf[k] = static_cast<float>(M[k]);
        R.skyMap(W, H, K, Mf, lv);
    }
    // stars: 0.50 (review item 5): every star by the one rule of sky.h starDraw (light ∝ 10^(-0.4 m), drawn light ∝ light^0.5), a disc in its colour bucket's colour.
    // Until 0.49.4: three size classes and opacities cut at m 3.4 and 4.6 and a hand-tuned glow below m 2.2, with no stated rule.
    // The Gaia flux map behind (assets/maps) does hold the brightest stars as faint points (Sirius: a peak of 173 over about 94 round it, in the map's grey);
    // the catalogue stars are still drawn over it because the map is drawn dimmed and at about 0.09 degrees per map pixel, while each dot carries the star's
    // own measured magnitude and B-V (and the constellation lines join them).
    static std::vector<float> pos;
    pos.assign(S.stars.size() * 2, -1e9f);
    for (std::size_t i = 0; i < S.stars.size(); i++) {
        double X, Y;
        if (S.stars[i].mag > 30) continue;                // 0.73.0: a star left out of this sky (Alpha Centauri's own A and B, seen from there)
        if (!scr(S.stars[i].dir, X, Y)) continue;
        pos[i * 2] = static_cast<float>(X); pos[i * 2 + 1] = static_cast<float>(Y);
        if (X < -10 || X > W + 10 || Y < -10 || Y > H + 10) continue;
        const StarDraw d = starDraw(S.stars[i].mag);
        const auto& c = S.colours[S.stars[i].bucket];
        R.disc(X, Y, d.radius, rgba(c[0], c[1], c[2], static_cast<float>(d.alpha * lv)));
    }
    if (cons != ConsMode::Off) {                          // constellation lines (Stellarium's modern sky culture)
        const double cl2 = std::max(lv, 0.6);
        const Color lc{115 / 255.f, 155 / 255.f, 235 / 255.f, static_cast<float>(0.42 * cl2)};
        for (const auto& L : S.lines)
            for (const auto& ab : L) {
                const double xa = pos[ab.first * 2], ya = pos[ab.first * 2 + 1], xb = pos[ab.second * 2], yb = pos[ab.second * 2 + 1];
                if (xa < -1e8 || xb < -1e8 || std::hypot(xa - xb, ya - yb) > 0.7 * diag) continue;
                R.line(xa, ya, xb, yb, 0.9, lc);
            }
        if (cons == ConsMode::LinesAndNames) {
            std::vector<std::pair<double, double>> used;
            std::vector<std::array<double, 3>> boxes;       // x, y, half width of every sky label drawn, so the Milky Way's label keeps clear of them
            for (const auto& c : S.cons) {
                double X, Y;
                if (!scr(c.second, X, Y) || X < 20 || X > W - 20 || Y < 20 || Y > H - 20) continue;
                bool clash = false;
                for (const auto& o : used) if (std::fabs(o.first - X) < 60 && std::fabs(o.second - Y) < 14) clash = true;
                if (clash) continue;
                used.push_back({X, Y});
                T.text(c.first, X, Y, 11, Color{150 / 255.f, 178 / 255.f, 240 / 255.f, static_cast<float>(0.78 * cl2)}, 0.5, false);
                boxes.push_back({X, Y, T.width(c.first, 11) / 2});
            }
            const std::size_t firstStar = boxes.size();     // (the star names are checked against each other only)
            for (const auto& nm : S.names) {              // the brightest stars' names
                if (S.stars[nm.first].mag > 1.9f) continue;
                const double x = pos[nm.first * 2], y = pos[nm.first * 2 + 1];
                if (x < 4 || x > W - 50 || y < 12 || y > H - 4) continue;
                const double nw0 = T.width(nm.second, 10);
                bool clash = false;                       // night hunt 0.39.2: a name that would overlap a label already drawn is left out (Rigil Kentaurus and Toliman, alpha Centauri A and B, were
                for (std::size_t bi = firstStar; bi < boxes.size(); bi++) { const auto& b = boxes[bi]; if (std::fabs(b[0] - (x + 7 + nw0 / 2)) < b[2] + nw0 / 2 + 3 && std::fabs(b[1] - (y + 3)) < 9) { clash = true; break; } }   // written on top of each other); the list is brightest first
                if (clash) continue;
                T.text(nm.second, x + 7, y + 3, 10, Color{232 / 255.f, 238 / 255.f, 1, static_cast<float>(0.72 * cl2)}, 0, false);
                const double nw = T.width(nm.second, 10);
                boxes.push_back({x + 7 + nw / 2, y + 3, nw / 2});
            }
            {   // the Milky Way's own label, on the band where it is on screen, at the free spot nearest the middle (skipped if there is none)
                const std::string txt = "The Milky Way (Gaia\u2019s map of its stars)";
                const double hw = T.width(txt, 11) / 2;
                std::vector<std::pair<double, int>> order;
                for (int i = 0; i < static_cast<int>(band.size()); i++) {
                    const BandPoint& p = band[i];
                    if (p.ok && p.fade > 0.95 && p.X - hw > 8 && p.X + hw < W - 8 && p.Y > 20 && p.Y < H - 20) order.push_back({std::hypot(p.X - cx, p.Y - cy), i});
                }
                std::sort(order.begin(), order.end());
                for (const auto& o : order) {
                    const BandPoint& p = band[o.second];
                    bool clash = false;
                    for (const auto& b : boxes) if (std::fabs(b[0] - p.X) < b[2] + hw + 6 && std::fabs(b[1] - p.Y) < 15) { clash = true; break; }
                    if (clash) continue;
                    T.text(txt, p.X, p.Y, 11, Color{178 / 255.f, 192 / 255.f, 1, static_cast<float>(0.8 * cl2)}, 0.5, false);
                    break;
                }
            }
        }
    }
}

}  // namespace

void OrbitView::click(double x, double y) {
    if (sel.kind != Selection::None && !cardHidden) { cardHidden = true; return; }   // the first click only closes the info box
    const Hit* best = nullptr;
    double bd = 1e9;
    for (const Hit& h : hits_) {
        const double d = std::hypot(x - h.x, y - h.y);
        if (d < std::max(h.r + 6, 12.0) && d < bd) { best = &h; bd = d; }
    }
    sel = best ? best->sel : Selection{};
    cardHidden = false;
    if (!best) follow = false;
}

void OrbitView::drag(double dx, double dy, bool rotate) {
    // 3D: drag turns the view (0.73.0: in the Alpha Centauri scene it may turn right over, so you can look back at our Sun, which is towards +z)
    if (rotate) { yaw += dx * 0.01; pitch = std::clamp(pitch + dy * 0.01, acScene ? -3.1 : -1.5, acScene ? 3.1 : 1.5); return; }
    ox += dx; oy += dy;
    follow = false;
}

void OrbitView::zoomChanged(double oldZoom, double newZoom) {   // zoom about the middle of the picture
    const double f = std::pow(10.0, newZoom - oldZoom);
    ox *= f; oy *= f;
}

void OrbitView::resetView() { ox = oy = 0; follow = false; yaw = 0.6; pitch = 1.0; cdep = 0; }

void sunColourBytes(int rgb[3]) { for (int k = 0; k < 3; k++) rgb[k] = sunfacts::kSunSrgb[k]; }

bool OrbitView::selectedScreen(double& x, double& y, double& r) const {
    if (sel.kind == Selection::None) return false;
    for (const Hit& h : hits_) if (h.sel == sel) { x = h.x; y = h.y; r = h.r; return true; }
    return false;
}

void OrbitView::select(Selection s, bool f) {
    sel = s; follow = f; cardHidden = false;
    ox = oy = 0; cdep = 0;
}

// The Journey's caption, top middle of the free part of the picture.
// 0.39.3: it was wrapped by a guessed character width and placed at a fixed height in the middle of the whole window, so it ran
// under the top bar, the Legend chip and the Controls panel (and off the left edge with Extra large text). Now it is wrapped by
// its measured width between the left edge and the panel, and starts below the top bar and any box at the top of the picture.
struct NoteRow { std::string t; double size; Color c; int msg; };   // msg: which note the row belongs to
struct NoteBlock { std::vector<NoteRow> rows; double total = 0, wMax = 0, top = 0; };
struct CaptionLayout { bool on = false; std::vector<std::string> lines; double sz = 13.5, top = 0, cx = -1, lineH = 0, ascent = 0, wMax = 0; };
CaptionLayout layoutCaption(const TextPainter& T, const ViewSettings& set, const std::string& caption, double W, double H) {
    CaptionLayout c;
    c.on = true;
    const double left = 12, right = W - set.insetRight - 12, maxW = std::max(120.0, right - left);
    c.lineH = TextPainter::pixels(c.sz, 1) * 1.4; c.ascent = TextPainter::pixels(c.sz, 1) * 0.8;
    std::string cur, word;
    for (char ch : caption + " ") {
        if (ch != ' ') { word += ch; continue; }
        if (word.empty()) continue;
        if (!cur.empty() && T.width(cur + " " + word, c.sz) > maxW) { c.lines.push_back(cur); cur = word; }
        else cur = cur.empty() ? word : cur + " " + word;
        word.clear();
    }
    if (!cur.empty()) c.lines.push_back(cur);
    c.top = set.insetTop > 0 ? set.insetTop + 6 : 48;
    for (int pass = 0; pass < 2; pass++)
        for (const auto& b : set.blocked)   // a box that starts in the caption's first line (the Legend chip, the fact line): go below it
            if (b[1] <= c.top + c.lineH && b[1] + b[3] > c.top && b[0] < right && b[0] + b[2] > left && b[1] + b[3] < H / 2) c.top = b[1] + b[3] + 6;
    if (set.insetRight > 0) c.cx = (left + right) / 2;   // (the old UI, without the panel: the middle of the picture, worked out in draw)
    for (const std::string& l : c.lines) c.wMax = std::max(c.wMax, T.width(l, c.sz));
    return c;
}

namespace {
struct KeyHash {                                  // FNV-1a over the bytes of each value
    std::uint64_t h = 1469598103934665603ull;
    void bytes(const void* p, std::size_t n) { const unsigned char* c = static_cast<const unsigned char*>(p); for (std::size_t i = 0; i < n; i++) { h ^= c[i]; h *= 1099511628211ull; } }
    template <class T> void add(const T& v) { bytes(&v, sizeof v); }
    void add(const std::string& t) { add(t.size()); bytes(t.data(), t.size()); }
    void add(const Vec3& v) { add(v.x); add(v.y); add(v.z); }
    void add(const app::Rgb& c) { add(c.r); add(c.g); add(c.b); }
};
}  // namespace

std::uint64_t OrbitView::pictureKey(const SolarSystem& sys, const ViewSettings& set, double originX, double originY, double uiScale, double width, double height) const {
    KeyHash k;
    k.add(sel.kind); k.add(sel.bodyId); k.add(sel.moon); k.add(follow); k.add(cardHidden); k.add(ox); k.add(oy); k.add(yaw); k.add(pitch); k.add(cdep); k.add(spin);
    k.add(journey); k.add(caption); k.add(captionAlpha); k.add(panMode); k.add(fx); k.add(cx); k.add(cy);
    k.add(set.view3D); k.add(set.zoom); k.add(set.tspeed); k.add(set.trueScale); k.add(set.fullTrails); k.add(set.moons); k.add(set.dwarfs); k.add(set.comets);
    k.add(set.belts); k.add(set.gpuRocks); k.add(set.sky); k.add(set.cons); k.add(set.realPlanets); k.add(set.exaggerateWobble); k.add(set.relativity); k.add(set.far);
    k.add(set.orbits); k.add(set.alphaCen); k.add(set.paused); k.add(set.lightTheme); k.add(set.theme.id); k.add(set.theme.light); k.add(set.theme.bg); k.add(set.theme.fg); k.add(set.theme.accent);
    k.add(set.textScale); k.add(set.shell); k.add(set.minTextU); k.add(set.minTextPx); k.add(set.reserveBottom); k.add(set.reserveRight); k.add(set.insetTop);
    k.add(set.insetRight); k.add(set.blocked.size()); for (const auto& b : set.blocked) k.bytes(b.data(), sizeof(double) * 4);
    k.add(sys.t); k.add(sys.epochMs); k.add(sys.jumped); k.add(sys.crashed); k.add(sys.escaped); k.add(sys.realPlanets()); k.add(sys.relativity());
    k.add(sys.bodies.size()); k.add(sys.sunTrail.size()); if (!sys.sunTrail.empty()) k.add(sys.sunTrail.back());
    for (const Body& b : sys.bodies) {
        k.add(b.id); k.add(b.pos); k.add(b.drawShift); k.add(b.moons.size()); k.add(b.trail.size());
        if (!b.trail.empty()) { k.add(b.trail.front()); k.add(b.trail.back()); }
    }
    k.add(sys.belts.size());
    k.add(originX); k.add(originY); k.add(uiScale); k.add(width); k.add(height);
    return k.h;
}

void OrbitView::draw(render::Renderer2D& R, ImDrawList* dl, double originX, double originY, double uiScale,
                     const SolarSystem& sys, const ViewSettings& setIn, double W, double H) {
    acScene = setIn.alphaCen;
    if (setIn.alphaCen) { drawAlphaCen(R, dl, originX, originY, uiScale, sys, setIn, W, H); return; }   // 0.73.0: the Alpha Centauri scene (alphacen_view.inc)
    TextPainter T{dl, ImGui::GetFont(), originX, originY, uiScale};
    const CaptionLayout cap = journey && captionAlpha > 0.01 && !caption.empty() ? layoutCaption(T, setIn, caption, W, H) : CaptionLayout{};
    // 0.39.3: the caption's box joins the boxes no label is drawn under (a star's "Mass:" line ran through it)
    ViewSettings setLocal = setIn;     // (a copy: the caption's box and the bottom-left notes' box are added to its blocked boxes below)
    if (cap.on && cap.cx >= 0) setLocal.blocked.push_back({cap.cx - cap.wMax / 2 - 4, cap.top - 2, cap.wMax + 8, cap.lines.size() * cap.lineH + 4});
    const ViewSettings& set = setLocal;
    const double px = std::min(W, H) / 34 * std::pow(10.0, set.zoom) / kSimLength;   // points per metre
    const bool TRUE_SCALE = set.trueScale;
    const Vec3 bc = sys.barycentre();                     // everything is drawn around the barycentre, so the Sun is the one that wobbles

    // the selected thing may be gone (removed, or switched off): orbit/card_text.cpp, tested
    const auto visible = [&](const Body& b) { return (b.type != BodyKind::Dwarf || set.dwarfs) && (b.type != BodyKind::Comet || set.comets); };
    if (!selectionStillValid(sys, sel, SelectionSwitches{set.dwarfs, set.comets, set.moons, set.belts, set.far})) { sel = Selection{}; follow = false; }

    // ---- the camera: turn by yaw about z, tilt by pitch towards the viewer, then a mild perspective ----
    const bool d3 = set.view3D;
    const double yawA = d3 ? yaw + kPi : 0, pitchA = d3 ? -pitch : 0;   // 3D the right way up: the ecliptic north pole points up the screen
    const double ca = std::cos(yawA), sa = std::sin(yawA), cb = std::cos(pitchA), sb = std::sin(pitchA);
    const double D = std::max(90.0, 3 * 34 / std::pow(10.0, set.zoom)) * kSimLength;   // camera distance, m: grows with the visible span
    double cd = cdep;                                     // depth of the middle of the view
    // screen position of a point (m): x, y in points; dep > 0 is towards the viewer; f is the perspective scale
    const auto base = [&](double x, double y, double z) {
        x -= bc.x; y -= bc.y; z -= bc.z;
        const double x1 = x * ca - y * sa, y1 = x * sa + y * ca, y2 = y1 * cb - z * sb, dep = y1 * sb + z * cb;
        const double f = d3 ? D / std::max(D - (dep - cd), 0.05 * D) : 1;
        return Q{W / 2 + fx + x1 * px * f, H / 2 - y2 * px * f, dep, f};
    };
    const auto flat = [&](double x, double y, double z) {  // far-away stars are seen flat: too far for this little scene's perspective
        x -= bc.x; y -= bc.y; z -= bc.z;
        const double x1 = x * ca - y * sa, y1 = x * sa + y * ca, y2 = y1 * cb - z * sb;
        return Q{W / 2 + fx + x1 * px, H / 2 - y2 * px, cd, 1};
    };
    if (follow && sel.kind == Selection::Far) {           // keep a far-away thing in the middle
        FarItem f;
        if (farItem(sys, sel.moon, f)) {
            if (f.kind != "probe") { cd = cdep = 0; const Q q = flat(f.pos.x, f.pos.y, f.pos.z); ox = W / 2 + fx + cx - q.first; oy = H / 2 + cy - q.second; }
            else { cd = cdep = base(f.pos.x, f.pos.y, f.pos.z).dep; const Q q = base(f.pos.x, f.pos.y, f.pos.z); ox = W / 2 + fx + cx - q.first; oy = H / 2 + cy - q.second; }
        }
    } else if (follow && sel.kind != Selection::None) {   // keep the followed object in the middle
        Vec3 t;
        if (sel.kind == Selection::Bary) t = bc;
        else if (sel.kind == Selection::Body || sel.kind == Selection::Moon) { const Body* b = findBody(sys, sel.bodyId); if (b) t = b->pos; }
        cd = cdep = base(t.x, t.y, t.z).dep;              // depth first, so the followed object sits in the middle with no perspective shift
        const Q q = base(t.x, t.y, t.z);
        ox = W / 2 + fx + cx - q.first; oy = H / 2 + cy - q.second;
    }
    const auto proj = [&](double x, double y, double z) { Q q = base(x, y, z); q.first += ox; q.second += oy; return q; };
    // 0.68.0: the same camera as numbers (orbit/orbit_lines.h LineCam: base() and proj() as above), for the trails, orbits and distance rings on either path
    LineCam lcam;
    lcam.bc = bc; lcam.ca = ca; lcam.sa = sa; lcam.cb = cb; lcam.sb = sb; lcam.px = px; lcam.D = D; lcam.cd = cd; lcam.W = W; lcam.H = H; lcam.fx = fx; lcam.ox = ox; lcam.oy = oy; lcam.d3 = d3;
    const bool cardLines = linesOnCard(R);                // their guarded steps advance here (orbit/lines_check.h); true: the card draws them this frame
    LinesPoints linesPts;
    linesPts.card = cardLines ? R.linesCard() : nullptr;

    setPictureColours(set.sky != SkyMode::Off, set.theme);
    g_textScale = set.textScale;
    g_minTextU = set.minTextU; g_minTextPx = set.minTextPx;
    if (set.shell) cardHidden = true;                     // no in-picture card in the new UI: a click is a click
    g_skyHalo = set.sky != SkyMode::Off;
    R.rect(0, 0, W, H, kBg);
    // 0.50: a lit ball's "no light" is drawn in the colour of empty space, so a night side is the sky's colour (render/shading.h). On a light page (a light theme with
    // Stars Off) the page is not a picture of dark space: there the night side stays black.
    R.setSpace(0.2126f * kBg.r + 0.7152f * kBg.g + 0.0722f * kBg.b < 0.25f ? kBg : Color{0, 0, 0, 1});
    hits_.clear();
    if (set.sky != SkyMode::Off) {                        // the night sky: dim close up, brighter as you zoom out
        const double zt = std::clamp(-set.zoom / 3.5, 0.0, 1.0), lv = set.sky == SkyMode::Full ? 0.45 + 0.55 * zt : 0.16 + 0.6 * zt;
        drawSky(R, T, W, H, ca, sa, cb, sb, set.cons, lv);
    }

    // the Sun as a star of 1 solar mass
    const double hzIn = sunfacts::hzInnerAU(), hzOut = sunfacts::hzOuterAU();   // habitable zone, AU: Kopparapu et al. 2014 (sun_facts.h; 0.49.4, was 0.95 - 1.67 with no source)
    const Color sunCol = sunColour();
    const Q star = proj(0, 0, 0);
    ui::auditRect("sun", static_cast<float>(originX + (star.first - 1) * uiScale), static_cast<float>(originY + (star.second - 1) * uiScale), static_cast<float>(2 * uiScale), static_cast<float>(2 * uiScale));   // 0.22: for the automatic checks (where the Sun is)
    const auto ringPoints = [&](double cx, double cy, double rPx, std::vector<std::pair<double, double>>& pts) {   // a circle on screen (only the visible bit of a giant one)
        pts.clear();
        double a0 = 0, a1 = 2 * kPi;
        if (bigR(W, H, rPx)) {
            const Win w = circWin(W, H, cx, cy, rPx, a0, a1);
            if (w != Win::Arc) return;
        }
        const int n = (a1 - a0 < 6.2) ? static_cast<int>(std::min(400.0, std::max(8.0, std::ceil((a1 - a0) * rPx / 6))))
                                      : static_cast<int>(std::min(720.0, std::max(48.0, std::ceil(rPx / 4))));
        for (int i = 0; i <= n; i++) { const double a = a0 + (a1 - a0) * i / n; pts.push_back({cx + rPx * std::cos(a), cy + rPx * std::sin(a)}); }
    };
    std::vector<std::pair<double, double>> pts;
    const auto hiRing = [&](double x, double y, double r) {   // a pink ring with a dark edge, shows on any background
        ringPoints(x, y, r, pts);
        for (int pass = 0; pass < 2; pass++) {
            LinePen pen{R, W, H, pass ? 2.0 : 4.0, pass ? kHL : Color{0, 0, 0, 0.55f}};   // (0.23 stage 2: thinner, 2 points, was 3)
            for (auto& p : pts) pen.to(p.first, p.second);
        }
    };

    {   // habitable zone: a faint green band
        // 0.23 stage 2: it fades out once the band is far wider than the picture (zoomed in on Earth the whole picture was green)
        const double bandPx = (hzOut - hzIn) * kAU * px, fade = std::clamp(1.0 - (bandPx / std::max(W, 1.0) - 1.5) / 3.0, 0.0, 1.0);
        const Color band = {marks::kHabitable[0] / 255.f, marks::kHabitable[1] / 255.f, marks::kHabitable[2] / 255.f, static_cast<float>(0.13 * fade)};
        if (bigR(W, H, hzOut * kAU * px)) {               // zoomed far in: draw just the strip of it that crosses the picture
            // the point of the ecliptic under the middle of the picture, and the band's direction there
            const double wx = -ox / px, wy = (oy / px) / std::max(std::fabs(cb), 0.05);
            const double X0 = wx * ca + wy * sa + bc.x, Y0 = -wx * sa + wy * ca + bc.y, r0 = std::max(std::hypot(X0, Y0), 1e-9), ux = X0 / r0, uy = Y0 / r0;
            const double nx0 = ux * ca - uy * sa, ny0 = -(ux * sa + uy * ca) * cb, nl = std::max(std::hypot(nx0, ny0), 1e-9), nx = nx0 / nl, ny = ny0 / nl, k = px * nl;
            const double tx = -ny, ty = nx, cx0 = W / 2 + fx, cy0 = H / 2, L = 4000;
            const double dI = std::clamp((hzIn * kAU - r0) * k, -L, L), dO = std::clamp((hzOut * kAU - r0) * k, -L, L);
            if (dO > dI) {
                const double ax = cx0 + nx * dI - tx * L, ay = cy0 + ny * dI - ty * L, bx = cx0 + nx * dI + tx * L, by = cy0 + ny * dI + ty * L;
                const double cx = cx0 + nx * dO + tx * L, cy = cy0 + ny * dO + ty * L, ex = cx0 + nx * dO - tx * L, ey = cy0 + ny * dO - ty * L;
                R.triangle(ax, ay, band, bx, by, band, cx, cy, band);
                R.triangle(ax, ay, band, cx, cy, band, ex, ey, band);
            }
        } else {
            const double ri = hzIn * kAU, ro = hzOut * kAU;  // m, in the ecliptic plane round the Sun
            for (int i = 0; i < 96; i++) {
                const double t0 = i / 96.0 * 2 * kPi, t1 = (i + 1) / 96.0 * 2 * kPi;
                const Q a = proj(ro * std::cos(t0), ro * std::sin(t0), 0), b = proj(ro * std::cos(t1), ro * std::sin(t1), 0);
                const Q c = proj(ri * std::cos(t1), ri * std::sin(t1), 0), e = proj(ri * std::cos(t0), ri * std::sin(t0), 0);
                R.triangle(a.first, a.second, band, b.first, b.second, band, c.first, c.second, band);
                R.triangle(a.first, a.second, band, c.first, c.second, band, e.first, e.second, band);
            }
        }
        const Q q = proj(hzOut * kAU * std::cos(2.4), hzOut * kAU * std::sin(2.4), 0);
        if (q.second > 60 && q.second < H + 20 && q.first > -60 && q.first < W + 60 && hzOut * kAU * px > 70)
            T.text("habitable zone (Kopparapu 2014)", q.first, q.second - 3, 10, hexColor("#3d9a5c", 0.85f), 0.5, true);
    }

    {   // distance rings, labelled in AU
        static const double steps[] = {0.5, 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000,
                                       1e6, 2e6, 5e6, 1e7, 2e7, 5e7, 1e8, 2e8, 5e8, 1e9, 2e9, 5e9, 1e10};
        double stepAU = 1e10;
        for (double v : steps) if (v * kAU * px >= 55) { stepAU = v; break; }
        const double diag = std::hypot(W, H);
        render::LinesDraw ringsCard;                       // 0.68.0: on the card when its lines are on (one draw after the loop: nothing else is drawn between them)
        ringsCard.cam = cardLines ? linesCamera(lcam) : render::LinesCamera{};
        bool ringsOk = cardLines;
        std::vector<double> ringXYZ;
        for (int k = 1; k * stepAU * kAU * px <= diag && k < 60; k++) {
            const double Rm = k * stepAU * kAU;
            ringPointsXYZ(Rm, ringXYZ);
            if (ringsOk && !addRingRuns(linesPts, ringsCard, (std::uint64_t(1) << 56) | std::uint64_t(k), ringXYZ, kMuted.withAlpha(0.22f))) {
                ringsOk = false;                           // (no room on the card: the rings so far and the rest on the processor, in order)
                std::vector<double> before;
                for (int j = 1; j < k; j++) { ringPointsXYZ(j * stepAU * kAU, before); drawRingCpu(R, lcam, before, kMuted.withAlpha(0.22f)); }
            }
            if (!ringsOk) drawRingCpu(R, lcam, ringXYZ, kMuted.withAlpha(0.22f));
            const Q q = proj(Rm * std::cos(-0.8), Rm * std::sin(-0.8), 0);
            const double au = k * stepAU;
            const double v = std::round(au * 10) / 10;
            std::string label = (v == std::floor(v) ? fmt::commas(v) : fmt::fixed(v, 1)) + " AU";
            if (au >= 30000) label += " · " + fmtLy(au / kLightYearAU) + " light years";
            if (set.shell) {                                                      // (stage 5c: no label under the bars or the Controls panel; the rings themselves are still drawn)
                const LabelBox lb = labelBox(q.first, q.second + 3, T.width(label, 10), 10, 0.5);
                const double gx = W - 40 - set.insetRight, gy = H - 44 - set.reserveRight;   // 0.22: nor on the axes (x y z) at the bottom right: the labels run down-right, straight into them (the same place as the gizmo below)
                const bool onAxes = lb.x1 > gx - 40 && lb.x0 < gx + 40 && lb.y1 > gy - 40 && lb.y0 < gy + 40;
                bool underBox = false;                             // 0.22 stage 6: nor under the readout or another floating box
                for (const auto& b : set.blocked) if (lb.x1 > b[0] && lb.x0 < b[0] + b[2] && lb.y1 > b[1] && lb.y0 < b[1] + b[3]) underBox = true;
                const bool clear = labelClear(lb, W, H, {set.insetTop, set.reserveBottom, set.insetRight}) && !onAxes && !underBox;
                ui::auditRect(std::string(clear ? "ringlabel." : "ringskip.") + std::to_string(k), static_cast<float>(originX + lb.x0 * uiScale), static_cast<float>(originY + lb.y0 * uiScale), static_cast<float>((lb.x1 - lb.x0) * uiScale), static_cast<float>((lb.y1 - lb.y0) * uiScale));
                if (!clear) continue;
            }
            T.text(label, q.first, q.second + 3, 10, kMuted.withAlpha(0.75f));
        }
        if (ringsOk && !ringsCard.runs.empty() && !R.lines(ringsCard)) for (int k = 1; k * stepAU * kAU * px <= diag && k < 60; k++) { ringPointsXYZ(k * stepAU * kAU, ringXYZ); drawRingCpu(R, lcam, ringXYZ, kMuted.withAlpha(0.22f)); }
    }

    if (set.belts && !sys.belts.empty()) {   // belts: thousands of rocks on Kepler orbits, one instanced draw call per belt (the places: the CPU, or from 0.13 stage 4 the GPU, orbit/belt_layer.h)
        BeltCamera cam;
        cam.bx = bc.x; cam.by = bc.y; cam.bz = bc.z; cam.ca = ca; cam.sa = sa; cam.cb = cb; cam.sb = sb; cam.px = px; cam.W = W; cam.H = H; cam.D = D; cam.cd = cd; cam.ox = ox + fx; cam.oy = oy; cam.d3 = d3;
        beltLayer_.draw(R, sys, cam, sel.kind == Selection::Belt ? sel.moon : -1, kHL, set.gpuRocks);
    }

    std::vector<const Body*> vis;
    for (const Body& b : sys.bodies) if (visible(b)) vis.push_back(&b);

    // trails: a short tail behind each object (or every lap, with Full orbit trails); then (0.23 stage 2) the planets' and dwarf planets' whole orbits, faint,
    // in their colour (the selected one brighter). orbit/orbit_lines.h draws both on the processor; 0.68.0: on the card when its lines are on (one draw)
    std::vector<Vec3> cpts;
    {
        render::LinesDraw tc;
        tc.cam = cardLines ? linesCamera(lcam) : render::LinesCamera{};
        bool onCard = cardLines;
        const auto trailOf = [&](const Body& b, bool card) {
            if (b.trail.empty()) return true;
            const bool on = sel.kind == Selection::Body && sel.bodyId == b.id;
            const int st0 = trailStart(b.trail, b.pos, set.fullTrails);
            Color cols[4];
            for (int part = 0; part < 4; part++) cols[part] = hexColor(b.color, 0.15f + 0.2f * part);
            if (card) return addTrailRuns(linesPts, tc, (std::uint64_t(2) << 56) | std::uint64_t(std::uint32_t(b.id)), b.trail, b.pos, st0, on ? 2.0 : 1.5, cols);
            drawTrailCpu(R, lcam, b.trail, b.pos, st0, on ? 2.0 : 1.5, cols);
            return true;
        };
        const auto orbitOfBody = [&](const Body& b, bool card) {
            const bool on = sel.kind == Selection::Body && sel.bodyId == b.id;
            const Color col = hexColor(b.color, on ? 0.7f : b.type == BodyKind::Planet ? 0.32f : 0.22f);
            if (card) return addOrbitRuns(linesPts, tc, (std::uint64_t(3) << 56) | std::uint64_t(std::uint32_t(b.id)), b, on ? 1.5 : 1.0, col, cpts);
            drawOrbitCpu(R, lcam, b, on ? 1.5 : 1.0, col, cpts);
            return true;
        };
        for (const Body* bp : vis) if (onCard && !trailOf(*bp, true)) onCard = false;
        if (set.orbits) for (const Body* bp : vis) if (!bp->comet && onCard && !orbitOfBody(*bp, true)) onCard = false;
        if (!(onCard && (tc.runs.empty() || R.lines(tc)))) {   // the processor's lines (OpenGL; the card's off, not ready or out of room)
            for (const Body* bp : vis) trailOf(*bp, false);
            if (set.orbits) for (const Body* bp : vis) if (!bp->comet) orbitOfBody(*bp, false);
        }
        if (linesPts.card) linesPts.card->frameDone();
    }
    for (const Body* bp : vis) if (bp->comet) {         // the comets' whole orbits, thin and dotted
        const bool on = sel.kind == Selection::Body && sel.bodyId == bp->id;
        cometLocalPath(*bp->comet, sys.julianDayTDB(), px, cpts);
        LinePen pen{R, W, H, on ? 1.6 : 1.0, hexColor(bp->color, on ? 0.85f : 0.35f), 2, 4};
        for (const Vec3& p : cpts) { const Q q = proj(p.x, p.y, p.z); pen.to(q.first, q.second); }
    }

    // ---- spin: how each planet is turned right now, seen on this screen ----
    // The screen's right, up and towards-you directions in ecliptic coordinates (the camera above):
    const Vec3 VX{ca, -sa, 0}, VY{sa * cb, ca * cb, -sb}, VZ{sa * sb, ca * sb, cb};
    // 0.46.7 (outside review): the real direction of the Sun (at the origin) from a body at p, on the screen: right, down, towards you. The balls, the rings' shadow and
    // their brightness all use it (until 0.46.6 the balls used the screen direction plus a fixed 0.35 towards you: wrong phases in 3D, every planet a little gibbous from above).
    // Seen along the screen's depth axis (VZ) for every body: the view's mild perspective shifts the true line of sight by at most a few degrees.
    struct Light3 { double x, y, z; };
    const auto sunLight = [&](const Vec3& p) {
        const double n = length(p);
        if (n < 1.0) return Light3{0, 0, 1};
        const Vec3 u = p * (-1.0 / n);
        return Light3{dot(u, VX), -dot(u, VY), dot(u, VZ)};
    };
    struct SpinCtx { float K[9]; double px, py, pz; };   // K: screen -> planet frame; (px, py, pz): the pole on screen
    const double jdNow = sys.julianDay(), jd0 = sys.epochMs / 86400000.0 + 2440587.5;
    const auto spinCtx = [&](const std::string& name, SpinCtx& c) {
        // too fast to see (over 1.2 turns a second while time runs): held still, turned as on the start date
        const bool fast = turnsPerSecond(name, set.tspeed) > 1.2 && !set.paused;
        SpinFrame f;
        if (!spinFrame(name, fast ? jd0 : jdNow, f)) return false;
        const Vec3 axes[3] = {f.A, f.B, f.P}, V[3] = {VX, VY, VZ};
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) c.K[3 * i + j] = static_cast<float>(dot(V[j], axes[i]));
        c.px = dot(f.P, VX); c.py = dot(f.P, VY); c.pz = dot(f.P, VZ);
        return true;
    };
    // the turning axis as a dashed line through the ball, N at the end it turns counter-clockwise around (right-hand rule)
    const auto spinAxis = [&](const std::string& name, const SpinCtx& c, double x, double y, double r, bool back) {
        if (r < 9 || bigR(W, H, r)) return;
        const double sg = spinRateDegPerDay(name) < 0 ? -1 : 1, ux = sg * c.px, uy = -sg * c.py, uz = sg * c.pz, n = std::hypot(ux, uy);
        if (n < 0.03) return;
        const double L = r * 1.32, half = (back == (uz > 0)) ? -1 : 1;   // back: the half pointing away from you (hidden behind the ball)
        for (int pass = 0; pass < 2; pass++) {
            LinePen pen{R, W, H, pass ? 1.4 : 3.2, pass ? Color{1, 1, 1, 0.92f} : Color{0, 0, 0, 0.55f}, 5, 3};
            pen.to(x, y); pen.to(x + half * ux * L, y + half * uy * L);
        }
        if (!back && r >= 13 && !(uz < 0 && L * n < r * 1.02))
            T.text("N", x + ux * L + ux / n * 9, y + uy * L + uy / n * 9 + 4, 11, Color{1, 1, 1, 1});
    };
    // The rings of the giant planets (0.26): half behind the ball (back) and half in front, in the planet's equator, tilted with its real axis.
    // Saturn: NASA's Saturnian Rings Fact Sheet (orbit/saturn_rings.h); Jupiter, Uranus, Neptune: NASA's ring fact sheets (orbit/giant_rings.h).
    // Each ring stops 1 - exp(-tau / mu) of the light behind it, seen at mu = |sin(elevation)|, and is as bright as its albedo (Saturn's B ring,
    // 0.5, is the full tint; the beige tint itself is drawn, not measured). A ring narrower than a pixel is drawn one pixel wide with its
    // coverage (true width / drawn width) folded into its opacity. Where the planet's shadow falls on a ring (the ray from the ring to the Sun,
    // the same light direction as the planet's shading, meets the ball) the ring gets no sunlight and is drawn black (it still stops light).
    struct RingSet { double eqKm; std::vector<rings::Band> bands; const char* name = ""; bool profile = false; };   // profile (0.51.2): Saturn, drawn from Cassini's profile
    const auto ringSet = [&](const std::string& name) {
        RingSet rs;
        if (const BodyShape* sh = bodyShape(name.c_str())) rs.name = sh->name;
        if (name == "Saturn") {
            rs.eqKm = rings::kSaturnEqRadiusKm;
            for (const auto& g : rings::kRings) rs.bands.push_back({g.innerKm, g.outerKm, 0.5 * (g.tauLo + g.tauHi), 0.5 * (g.albedoLo + g.albedoHi)});
            rs.bands.push_back({rings::kFRingKm, rings::kFRingKm, rings::kFRingTau, rings::kFRingAlbedo});   // the F ring: narrower than 1,000 km, a hairline
            rs.profile = true;   // 0.51.2: drawn from Cassini's measured profile (orbit/saturn_profile.h); the bands above stay for the cards and the tests
        } else { const rings::GiantRings g = rings::giantRings(name); rs.eqKm = g.eqRadiusKm; rs.bands = g.bands; }
        return rs;
    };
    // Brightness (0.26): a ring is a thin layer of particles, so its light is worked out with the single-scattering formulas for a slab of
    // optical depth tau (Chandrasekhar 1960, Radiative Transfer), seen at mu = |sin(elevation of the viewer)| and lit at mu0 = |sin(elevation of
    // the real Sun)| above the ring plane: on the sunlit face, mu0 / (mu0 + mu) (1 - exp(-tau (1/mu + 1/mu0))); seen from the dark face, the light
    // that comes through, mu0 / (mu0 - mu) (exp(-tau/mu0) - exp(-tau/mu)). Scaled by the albedo / 0.5 so that an opaque ring of albedo 0.5 seen
    // face-on in full sunlight (formula value 0.5) is the full tint. So the thick B ring is bright from the sunlit side and dark from the other,
    // where the thin C ring and the Cassini Division glow, and every ring fades when the Sun is in the ring plane (Saturn's equinox, May 2025).
    // sunElev: sin of the real Sun's elevation above the ring plane (signed: + on the side the pole points to).
    const auto drawRings = [&](const RingSet& rs, const SpinCtx* c, double x, double y, double r, const Light3& sun, double sunElev, bool back) {
        if (rs.bands.empty() || r < 1) return;
        // the pole on the screen (right, up, towards you); without a spin model, a fixed tilt as before
        const double px0 = c ? c->px : std::sin(-0.22) * 0.94, py0 = c ? c->py : std::cos(-0.22) * 0.94, pz0 = c ? c->pz : 0.34;
        const double pz = pz0 / std::sqrt(px0 * px0 + py0 * py0 + pz0 * pz0);
        const double mu = std::max(std::fabs(pz), 0.004), mu0 = std::max(std::fabs(sunElev), 1e-3);
        const bool sunlitFace = (pz >= 0) == (sunElev >= 0);   // you see the face the Sun lights
        const auto slab = [&](double tau) { return rings::shine(tau, mu, mu0, sunlitFace); };   // orbit/saturn_rings.h, tested
        // 0.46.7: the real Sun (right, up, towards you), as the ball; 0.27: the planet's real shape (orbit/shapes.h): in its own frame (K) with the pole axis
        // stretched by eq / polar radius the body is a ball of radius 1. 0.64.0: the geometry and the triangles in orbit/ring_draw.h (shared with the card's check)
        const RingFrame fr = ringFrame(x, y, r, px0, py0, pz0, c ? c->K : nullptr, c ? polarRatio(rs.name) : 1.0, sun.x, -sun.y, sun.z);
        // 0.51.2: Saturn from Cassini's profile (UVIS, beta Cen 2008; orbit/saturn_profile.h): bins of at least a pixel (and at most 700 bins over the whole
        // profile, for speed), each with the area-averaged blocked share and light of the 10 km measurements in it; neighbours that look the same are merged
        // (0.64.0: orbit/ring_check.cpp saturnDrawnBands, kept while its inputs stay the same).
        static std::vector<RingBand> drawnOther;
        const std::vector<RingBand>* drawnP = &drawnOther;
        if (rs.profile) drawnP = &saturnDrawnBands(rs.eqKm, r, mu, mu0, sunlitFace);
        else {
            drawnOther.clear();
            for (const auto& bd : rs.bands) {
                const double rin = bd.innerKm / rs.eqKm, rout = bd.outerKm / rs.eqKm, mid = 0.5 * (rin + rout);
                const double wPx = (rout - rin) * r, drawPx = std::max(wPx, 1.0), cover = wPx / drawPx;
                const float alpha = static_cast<float>(rings::blocked(bd.tau, mu) * cover);
                const double shine = bd.albedo / 0.5 * slab(bd.tau) * cover;   // the light the ring sends you, 1 = the full tint
                if (alpha < 0.002f && shine < 0.002) continue;      // too faint to show at all (Jupiter's main ring, Neptune's dusty rings)
                const float k = static_cast<float>(alpha > 1e-6f ? std::min(1.0, shine / alpha) : 0.0);   // drawn colour x opacity = the light
                drawnOther.push_back({mid - 0.5 * drawPx / r, mid + 0.5 * drawPx / r, alpha, k});
            }
        }
        const std::vector<RingBand>& drawn = *drawnP;
        // 0.64.0: Saturn's rings on the graphics card (Vulkan; render/shaders.h kRingsFS: the same chords, bands and shadow per pixel) once its start-up check passed
        if (rs.profile && drawRingsOnCard(R, fr, drawn, back, W, H)) return;
        for (const RingBand& b : drawn) {
            // 0.50 (review item 7): ONE drawn tint (0.91, 0.83, 0.66) for every ring system (Saturn's, Jupiter's, Uranus's, Neptune's): not measured. The Cassini VIMS
            // papers found (Filacchione et al. 2012, arXiv:1203.6230; Hedman et al. 2013, arXiv:1210.4727) only plot the spectra; no table to compute a colour from.
            // Learn says so; ROADMAP: source needed.
            // 0.50 (review item 3): the ring's light k multiplies the tint in LINEAR light (the tint decoded with IEC 61966-2-1), then the result is encoded (render/shading.h)
            const Color litC{static_cast<float>(render::shadeEncoded(kRingTint[0], b.k)), static_cast<float>(render::shadeEncoded(kRingTint[1], b.k)), static_cast<float>(render::shadeEncoded(kRingTint[2], b.k)), b.alpha};
            const Color darkC{0, 0, 0, b.alpha};
            ringTriangles(fr, b, back, W, H, [&](double x0, double y0, double x1, double y1, double x2, double y2, bool lit) {
                const Color& cc = lit ? litC : darkC;
                R.triangle(x0, y0, cc, x1, y1, cc, x2, y2, cc);
            });
        }
    };

    // ---- the Sun ----
    struct Label { std::string name; double x, y; bool selected, small, force; bool gc = false; int prio = 5; };   // prio (0.23 stage 2): 1 planet, 2 dwarf planet, 3 comet, 4 moon, 5 the rest: who keeps its place when labels crowd
    std::vector<Label> labels;
    const double sr = TRUE_SCALE ? kSunRadius * px : std::max(kCrashRadius * px, 3.5);
    const bool sunTiny = TRUE_SCALE && sr < 2.5;
    const double srd = TRUE_SCALE ? std::max(sr, 5.0) : sr;
    SpinCtx sunSpin;
    const bool hasSunSpin = spinCtx("Sun", sunSpin);
    if (hasSunSpin) spinAxis("Sun", sunSpin, star.first, star.second, sr, true);
    // 0.49.4: no glow round the Sun. The real white-light corona is a millionth to a billionth of the disc's brightness (NASA, data/sun/README.txt): it cannot show
    // next to the disc. Until 0.49.3 a yellow glow out to 3 solar radii was drawn here (no source, no label).
    R.sunDisc(star.first, star.second, sr, sunCol);
    if (sr > 9) {   // soft bright and orange patches, markers only, so the spin can be seen: invented places, turning as one solid body at the 25.38-day rate (the card says so; the real Sun turns faster at its equator)
        const auto hsh = [](double n) { const double v = std::sin(n * 127.1 + 311.7) * 43758.5453; return v - std::floor(v); };
        const double x = star.first, y = star.second;
        for (int i = 0; i < 16; i++) {
            const double lon = hsh(i * 3 + 60) * 6.283, lat = (hsh(i * 3 + 61) - .5) * 1.7, cl = std::cos(lat), b0 = cl * std::cos(lon), b1 = cl * std::sin(lon), b2 = std::sin(lat);
            double X, Y, mu;
            if (hasSunSpin) { const float* K = sunSpin.K; X = K[0] * b0 + K[3] * b1 + K[6] * b2; Y = K[1] * b0 + K[4] * b1 + K[7] * b2; mu = K[2] * b0 + K[5] * b1 + K[8] * b2; }
            else { X = b1; Y = b2; mu = b0; }               // no real spin known: a still ball
            if (mu <= 0.05) continue;
            const double sx = x + X * sr, sy = y - Y * sr, sz = sr * (.14 + .12 * hsh(i + 70)), rr = sz * 1.4;
            if (sx + rr < 0 || sx - rr > W || sy + rr < 0 || sy - rr > H) continue;
            const bool bright = i % 2 == 0;
            const float a = float((bright ? .22 : .16) * std::min(1.0, mu * 2));
            const Color c0 = bright ? rgba(255, 248, 190, a) : rgba(215, 110, 20, a), c1 = c0.withAlpha(0);
            // squashed along the direction to the edge of the disc; points outside the disc are pulled onto its edge
            const double ang = std::atan2(-Y, X), ca2 = std::cos(ang), sa2 = std::sin(ang), sq = std::max(mu, .25);
            const auto edge = [&](double t, double& ex, double& ey) {
                const double u = rr * std::cos(t) * sq, v = rr * std::sin(t);
                ex = sx + u * ca2 - v * sa2; ey = sy + u * sa2 + v * ca2;
                const double d = std::hypot(ex - x, ey - y);
                if (d > sr) { ex = x + (ex - x) * sr / d; ey = y + (ey - y) * sr / d; }
            };
            const int n = 28;
            double px0, py0;
            edge(0, px0, py0);
            for (int k = 1; k <= n; k++) {
                double px1, py1;
                edge(2 * kPi * k / n, px1, py1);
                R.triangle(sx, sy, c0, px0, py0, c1, px1, py1, c1);
                px0 = px1; py0 = py1;
            }
        }
    }
    if (hasSunSpin) spinAxis("Sun", sunSpin, star.first, star.second, sr, false);
    if (sunTiny) R.ring(star.first, star.second, 7, 1.6, sunCol.withAlpha(0.9f));   // smaller than a pixel: a ring shows where it is
    if (sel.kind == Selection::Star) hiRing(star.first, star.second, srd + (sunTiny ? 8 : 6));
    hits_.push_back({{Selection::Star}, star.first, star.second, std::max(srd, sunTiny ? 10.0 : 0.0)});

    bool baryShown = false; Q baryQ{};                    // 0.46.7: where the barycentre's crosshair is, so the Sun's label keeps off it
    {   // the barycentre: a crosshair in a ring (hidden while it would sit on the Sun's centre)
        const Q bq = proj(bc.x, bc.y, bc.z);
        const bool on = sel.kind == Selection::Bary;
        if (std::hypot(bq.first - star.first, bq.second - star.second) > 5 || on || (sys.realPlanets() && set.exaggerateWobble)) {
            for (int pass = 0; pass < 2; pass++) {
                const Color c = pass ? (on ? kHL : Color{1, 1, 1, 1}) : Color{0, 0, 0, 0.6f};
                const double w = pass ? 1.8 : 4;
                R.ring(bq.first, bq.second, 6, w, c);
                R.line(bq.first - 10, bq.second, bq.first + 10, bq.second, w, c);
                R.line(bq.first, bq.second - 10, bq.first, bq.second + 10, w, c);
            }
            hits_.push_back({{Selection::Bary}, bq.first, bq.second, 12});
            baryShown = true; baryQ = bq;
            labels.push_back({"Barycentre", bq.first, bq.second + 22, on, false, false});
        }
    }

    if (sys.realPlanets()) {   // Real: the Sun's own path round the barycentre (it circles it, mostly because of Jupiter), kSunWobbleScale times too big when "Exaggerate" is on
        const double k = set.exaggerateWobble ? kSunWobbleScale : 1;
        const auto& tr = sys.sunTrail;
        const int n = static_cast<int>(tr.size());
        for (int part = 0; part < 4 && n > 1; part++) {   // fades in 4 steps, oldest faintest, like the planets' trails
            const int i0 = part * n / 4, i1 = std::min(n, (part + 1) * n / 4 + 1);
            LinePen pen{R, W, H, 1.5, sunCol.withAlpha(0.15f + 0.2f * part)};
            for (int i = i0; i < i1; i++) { const Q q = proj(bc.x + k * tr[i].x, bc.y + k * tr[i].y, bc.z + k * tr[i].z); pen.to(q.first, q.second); }
        }
        if (set.exaggerateWobble) {                           // the Sun's place on the magnified path: a ring (the Sun itself is drawn at its true place)
            const Q sq = proj(bc.x * (1 - k), bc.y * (1 - k), bc.z * (1 - k));   // the Sun is at -bc from the barycentre: bc + k * (-bc)
            R.ring(sq.first, sq.second, 5, 1.6, sunCol.withAlpha(0.9f));
            labels.push_back({"Sun's path ×" + std::to_string(static_cast<int>(kSunWobbleScale)) + ": exaggerated, not to scale", sq.first, sq.second + 16, false, true, false});
        }
    }

    // 0.51.2: eclipse shadows. What can hide the Sun from a point of a ball, at the REAL places (the moons are drawn much farther out than they are, so a moon's shadow
    // on its planet is where it really falls, not under the drawn moon): Earth's Moon (ELP2000-82B) on Earth, Earth on the Moon, the 13 big moons (orbit/moon_sats.h)
    // on their planet (the 4 nearest the line to the Sun) and their planet on them. Every shadow-caster is a ball of its equatorial radius (Jupiter's and Saturn's
    // flattening is left out of their shadows); the Sun an evenly bright disc of 695,700 km. Earth also gets its air (render/shading.h earthAirLight).
    // Not drawn in a shadow: the moons without a map (grey balls) and the moons' shadows on each other. Light that Earth's air bends into its shadow (the red
    // eclipsed Moon) is left out (Learn says so).
    const double jdShadow = sys.julianDayTDB();
    const auto scr = [&](const Vec3& d, double unit) { return Vec3{dot(d, VX) / unit, dot(d, VY) / unit, dot(d, VZ) / unit}; };
    const auto sunAngAt = [&](const Vec3& p) { return static_cast<float>(std::asin(std::min(1.0, kSunRadius / std::max(length(p), kSunRadius)))); };
    const auto addOcc = [](render::Renderer2D::PlanetLight& pl, const Vec3& o, double ro) {
        if (pl.nOcc >= 4) return;
        float* q = pl.occ + 4 * pl.nOcc++;
        q[0] = static_cast<float>(o.x); q[1] = static_cast<float>(o.y); q[2] = static_cast<float>(o.z); q[3] = static_cast<float>(ro);
    };
    const auto planetLight = [&](const Body& b) {
        render::Renderer2D::PlanetLight pl;
        pl.sunAng = sunAngAt(b.pos);
        pl.air = b.name == "Earth";
        const double R0 = std::max(b.radius, 1.0);
        const Vec3 toSun = b.pos * (-1.0 / std::max(length(b.pos), 1.0));
        if (b.name == "Earth") {
            if (sys.realMoon() && !b.moons.empty()) addOcc(pl, scr(sys.moonGeocentric(), R0), b.moons[0].data->radiusKm * kKm / R0);
            if (std::getenv("PL_LOG_BODYPX") && pl.nOcc) { const Vec3 m = sys.moonGeocentric(); std::printf("PLOCC Earth %.4f %.4f %.4f %.5f sun %.6f jd %.5f gamma %.4f\n", pl.occ[0], pl.occ[1], pl.occ[2], pl.occ[3], pl.sunAng, jdShadow, length(m - toSun * dot(m, toSun)) / R0); }   // for automatic checks only
            return pl;
        }
        struct C { Vec3 d; double r, perp; };
        std::vector<C> cs;
        for (const Moon& m : b.moons) {
            Vec3 sp, sv;
            if (!satState(m.data->name, jdShadow, sp, sv)) continue;
            const Vec3 d = sp * kKm;
            const double along = dot(d, toSun);
            if (along <= 0) continue;                                   // behind the planet as seen from the Sun: no shadow on it
            cs.push_back({d, m.data->radiusKm * kKm, length(d - toSun * along)});
        }
        std::sort(cs.begin(), cs.end(), [](const C& a, const C& c) { return a.perp < c.perp; });
        for (const C& c : cs) addOcc(pl, scr(c.d, R0), c.r / R0);
        if (std::getenv("PL_LOG_BODYPX")) for (int i = 0; i < pl.nOcc; i++) std::printf("PLOCC %s %.4f %.4f %.4f %.5f sun %.6f\n", b.name.c_str(), pl.occ[4 * i], pl.occ[4 * i + 1], pl.occ[4 * i + 2], pl.occ[4 * i + 3], pl.sunAng);   // for automatic checks only
        return pl;
    };
    // ---- planets, dwarf planets and comets (far ones first) ----
    std::vector<const Body*> order = vis;
    std::stable_sort(order.begin(), order.end(), [&](const Body* a, const Body* b) { return base(a->pos.x, a->pos.y, a->pos.z).dep < base(b->pos.x, b->pos.y, b->pos.z).dep; });   // far ones first
    const double grow = !TRUE_SCALE && set.zoom > 0 ? std::min(3.2, std::pow(10.0, 0.5 * set.zoom)) : 1;   // Easy to see: a little bigger as you zoom in
    const auto drawnRadius = [&](const Body& b) { return TRUE_SCALE ? b.radius * px : (3 + 1.6 * std::sqrt(b.radius / kEarthRadius)) * grow; };
    for (const Body* bp : order) {
        const Body& b = *bp;
        const Q q = proj(b.pos.x, b.pos.y, b.pos.z);
        const double rad = drawnRadius(b) * (d3 ? q.f : 1);     // in 3D, nearer things look bigger
        const bool tiny = TRUE_SCALE && rad < 2.5;
        const double rd = tiny ? 5 : rad;
        const bool on = sel.kind == Selection::Body && sel.bodyId == b.id;
        if (b.comet) {   // a glowing head and two straight tails pointing away from the Sun (blue gas tail; yellow dust tail bent back along the path). Drawn for show only:
            // the 4 AU switch-on, the length scale (CometData::tail) and the dust tail's direction are drawing choices, not measurements; the legend and the card say so
            const double r = length(b.pos), rA = r / kAU, act = std::clamp((4 - rA) / 3.2, 0.0, 1.0);
            const double L = b.comet->tail * act * std::min(1.5, 1 / rA) * kAU;   // tail length, m
            const Vec3 u = b.pos * (1 / r);
            const double vm = std::max(length(b.vel), 1e-9);
            if (L * px > 4) {
                const Vec3 dirs[2] = {u, u - b.vel * (0.7 / vm)};
                const double lens[2] = {1, 0.8}, wds[2] = {0.05, 0.09};
                const Color cols[2] = {rgba(marks::kIonTail[0], marks::kIonTail[1], marks::kIonTail[2]), rgba(marks::kDustTail[0], marks::kDustTail[1], marks::kDustTail[2])};
                for (int k = 0; k < 2; k++) {
                    const Vec3 d = dirs[k] * (1 / std::max(length(dirs[k]), 1e-12));
                    const Q e = proj(b.pos.x + d.x * L * lens[k], b.pos.y + d.y * L * lens[k], b.pos.z + d.z * L * lens[k]);
                    const double ex = e.first - q.first, ey = e.second - q.second, ln = std::hypot(ex, ey);
                    if (ln < 3) continue;
                    const double nx = -ey / ln, ny = ex / ln, w2 = std::min(14.0, ln * wds[k] + 2);
                    const Color c0 = cols[k].withAlpha(0.85f), c1 = cols[k].withAlpha(0);
                    R.triangle(q.first + nx, q.second + ny, c0, e.first + nx * w2, e.second + ny * w2, c1, e.first - nx * w2, e.second - ny * w2, c1);
                    R.triangle(q.first + nx, q.second + ny, c0, e.first - nx * w2, e.second - ny * w2, c1, q.first - nx, q.second - ny, c0);
                }
            }
            const double cr = 3 + 8 * act;
            R.fadeDisc(q.first, q.second, cr, Color{1, 1, 1, static_cast<float>(0.5 + 0.4 * act)});
            R.disc(q.first, q.second, std::max(1.3, rad), Color{1, 1, 1, 1});
            R.ring(q.first, q.second, 6 + 3 * act, 1.6, hexColor(b.color, 0.9f));
            if (on) hiRing(q.first, q.second, 10 + 3 * act);
            labels.push_back({b.name, q.first, q.second - 13 - 3 * act, on, false, false}); labels.back().prio = 3;
            hits_.push_back({{Selection::Body, b.id}, q.first, q.second, 12});
            continue;
        }
        if (d3 && std::fabs(b.pos.z) > 0.05 * kSimLength) {   // 3D: a dashed drop line to the ecliptic plane shows the height
            const Q g = proj(b.pos.x, b.pos.y, 0);
            LinePen pen{R, W, H, 1.0, hexColor(b.color, 0.35f), 3, 3};
            pen.to(q.first, q.second); pen.to(g.first, g.second);
            R.disc(g.first, g.second, 2, hexColor(b.color));
        }
        const Light3 sun = sunLight(b.pos);                 // 0.46.7: the real Sun
        static const bool logPx = std::getenv("PL_LOG_BODYPX") != nullptr;   // 0.50, for automatic checks only: where each body is drawn (x, y, radius in pixels) and the light's direction
        if (logPx) std::printf("PLBODYPX %s %.2f %.2f %.3f %.4f %.4f %.4f\n", b.name.c_str(), q.first, q.second, rad, sun.x, sun.y, sun.z);
        if (q.first + rad > -12 && q.first - rad < W + 12 && q.second + rad > -12 && q.second - rad < H + 12) {
            SpinCtx sc;
            const bool spins = rad > 3 && spinCtx(b.name, sc);     // big enough to see it turn
            const RingSet ringsHere = ringSet(b.name);        // empty for a body without rings
            double sunElev = 0.34;                            // the Sun's elevation above the ring plane (sin): from the real pole and the real direction of the Sun
            if (!ringsHere.bands.empty() && spins) {
                const Vec3 P = VX * sc.px + VY * sc.py + VZ * sc.pz, toSun = b.pos * (-1.0 / std::max(length(b.pos), 1.0));
                sunElev = dot(P, toSun) / std::max(length(P), 1e-12);
            }
            // 0.50 (review item 2): no blue "edge of air" round Earth any more. It was a ring from 0.96 R to 1.16 R, 50 % blue, all the way round (about 1,100 - 1,300 km thick
            // on screen, and lit on the night side too), with no source. The air's scale height is 8.5 km (NSSDC Earth fact sheet, data/nssdc/earthfact.html): under a pixel at
            // every zoom the app has. Removed as 0.49.4 removed the Sun's glow; a computed limb (Rayleigh scattering, sunlit side only) is a ROADMAP line.
            drawRings(ringsHere, spins ? &sc : nullptr, q.first, q.second, rad, sun, sunElev, true);
            // 0.26: the ball's colour. A planet with a map uses the map (Saturn, Uranus and Neptune have OPAL maps since 0.26, assets/maps/README.md); one without (Venus) its measured colour (render::measuredColour,
            // Mallama et al. 2017); anything else its marker colour.
            int mc[3];
            const Color ball = render::mapForBody(b.name) == render::Map::None && render::measuredColour(b.name, mc) ? rgba(mc[0], mc[1], mc[2]) : hexColor(b.color);
            if (spins) {
                spinAxis(b.name, sc, q.first, q.second, rad, true);
                {   // 0.26: the rings that are wide and thick enough to cast a visible shadow on the ball (Saturn's main rings)
                    float rb[3 * render::Renderer2D::kMaxRingBands]; int nb = 0;
                    if (ringsHere.profile)   // 0.51.2: Saturn: 32 bands of Cassini's profile, each with its mean transmission along the Sun's slanted path
                    {   // 0.64.0: kept while the Sun's elevation stays the same (the 32 bands walk the whole profile)
                        static float kept[3 * render::Renderer2D::kMaxRingBands]; static int keptN = 0; static double keptMu0 = -1, keptEq = -1;
                        if (std::fabs(sunElev) != keptMu0 || ringsHere.eqKm != keptEq) { keptN = rings::saturnShadowBands(std::fabs(sunElev), ringsHere.eqKm, render::Renderer2D::kMaxRingBands, kept); keptMu0 = std::fabs(sunElev); keptEq = ringsHere.eqKm; }
                        nb = keptN; std::copy(kept, kept + 3 * keptN, rb);
                    }
                    else
                        for (const auto& bd : ringsHere.bands)
                            if (nb < 8 && bd.tau >= 0.01 && (bd.outerKm - bd.innerKm) / ringsHere.eqKm >= 0.005) { rb[3 * nb] = float(bd.innerKm / ringsHere.eqKm); rb[3 * nb + 1] = float(bd.outerKm / ringsHere.eqKm); rb[3 * nb + 2] = float(bd.tau); nb++; }
                    const render::Renderer2D::PlanetLight pl = planetLight(b);
                    R.planet(q.first, q.second, rad, ball, sun.x, sun.y, sun.z, sc.K, render::mapForBody(b.name), nb ? rb : nullptr, nb, 1.0, polarRatio(b.name.c_str()), false, &pl);   // 0.27: its real shape (orbit/shapes.h)
                }
                spinAxis(b.name, sc, q.first, q.second, rad, false);
            } else
                R.sphere(q.first, q.second, rad, ball, sun.x, sun.y, sun.z);   // lit from the Sun
            drawRings(ringsHere, spins ? &sc : nullptr, q.first, q.second, rad, sun, sunElev, false);
        }
        if (tiny) {   // 0.52.1: in its measured colour where it has one (render::pointColour), as its ball close up; it was the hand-picked marker colour
            int pc[3]; render::pointColour(b.name, b.color, pc);
            const Color dot = rgba(pc[0], pc[1], pc[2]);
            R.fadeDisc(q.first, q.second, 7, dot.withAlpha(0.35f)); R.disc(q.first, q.second, 2.6, dot);
        }   // 0.23 stage 2: smaller than a pixel at true scale: a dot in its colour with a soft glow (was a hollow ring)
        if (on) hiRing(q.first, q.second, rd + (tiny ? 5 : 6));
        labels.push_back({b.name, q.first, q.second - rd - 5, on, false, false}); labels.back().prio = b.type == BodyKind::Planet ? 1 : 2;
        hits_.push_back({{Selection::Body, b.id}, q.first, q.second, std::max(rd, tiny ? 9.0 : 0.0)});
    }

    // ---- moons: drawn on their real Kepler ellipse, but much wider than real until you zoom far in ----
    if (set.moons) for (const Body* bp : order) {
        const Body& b = *bp;
        if (b.moons.empty()) continue;
        const Q pc = proj(b.pos.x, b.pos.y, b.pos.z);
        const double kf = d3 ? pc.f : 1, rad = drawnRadius(b) * kf, rKm = b.radius / kKm, distPx = length(b.pos) * px;   // true distance from the Sun: the same from every viewing angle
        std::vector<double> disp;
        double maxDisp = 0;
        for (const Moon& m : b.moons) { disp.push_back(rad + 7 + 13 * std::log(std::max(m.data->aKm / rKm, 1.2))); maxDisp = std::max(maxDisp, disp.back()); }
        const bool here = (sel.kind == Selection::Body || sel.kind == Selection::Moon) && sel.bodyId == b.id;
        if (!here && maxDisp > 0.15 * distPx) continue;   // hidden while it would crowd the map: zoom in or click the planet (0.22 stage 6: 0.15, was 0.3: at the whole-system zoom Saturn's and Uranus's widened moon orbits drew rings a third of the way to the Sun)
        const double dps = kSimTime / kDay * std::pow(10.0, set.tspeed);   // simulated days per real second
        for (std::size_t j = 0; j < b.moons.size(); j++) {
            const MoonData& m = *b.moons[j].data;
            const double aM = m.aKm * kKm;                // real distance, m
            const double dW = std::max(disp[j] / (px * kf), TRUE_SCALE ? aM : 0.0);   // drawn distance, m
            const double tl = m.tiltDeg * kPi / 180, ct = std::cos(tl), st = std::sin(tl);
            const bool fast = dps / m.periodDays > 8;     // a lap in under ~8 frames would flicker: the whole orbit is lit instead
            const bool on = sel.kind == Selection::Moon && sel.bodyId == b.id && sel.moon == static_cast<int>(j);
            // mean anomaly grows by 2π per period; Kepler's equation by 3 Newton steps (moon orbits are nearly round)
            const double Mn = b.moons[j].phase + (fast ? 0 : (m.retrograde ? -1 : 1) * 2 * kPi * (sys.t / kDay) / m.periodDays);
            double E = Mn + m.e * std::sin(Mn);
            for (int i = 0; i < 3; i++) E += (Mn - (E - m.e * std::sin(E))) / (1 - m.e * std::cos(E));
            const double xr = std::cos(E) - m.e, yr = std::sqrt(1 - m.e * m.e) * std::sin(E), sgn = m.retrograde ? -1 : 1;
            // 0.10.4 stage 2 (0.46.3: in Simple too): Earth's Moon is at its real direction and phase (ELP2000-82B). It is drawn at the same exaggerated distance as any moon (dW, the label
            // "Orbit drawn about N× too wide" is unchanged), scaled by the Moon's real distance over its mean one (so perigee and apogee show, and at true scale it is the real distance); the ring is the Moon's
            // instantaneous orbit plane (geocentric position x velocity) through the Moon, so the Moon is always on it. Outside 1800-2100 it is on its Kepler ellipse, as moons without data are.
            const bool realM = sys.realMoon() && b.name == "Earth" && j == 0;
            Vec3 rh, ph;
            double dW2 = dW;
            if (realM) { const Vec3 g = sys.moonGeocentric(); rh = g * (1 / length(g)); ph = cross(sys.moonOrbitNormal(), rh); dW2 = dW * length(g) / aM; }
            // 0.27: the 13 big moons at their real places (orbit/moon_sats.h, fitted to JPL Horizons), drawn as our real Moon: same direction, the drawn distance
            // scaled by the real one over the mean one, on the ring of its present orbit plane. In both Simple and Real (they are not part of the planets' physics).
            Vec3 sp, sv;
            const bool realS = !realM && !(b.name == "Earth" && j == 0) && satState(m.name, sys.julianDayTDB(), sp, sv);
            if (realS) { rh = sp * (1 / length(sp)); Vec3 nn = cross(sp, sv); nn = nn * (1 / length(nn)); ph = cross(nn, rh); dW2 = dW * length(sp) / m.aKm; }
            const bool realP = realM || realS;
            {   // the orbit (seen from above, the tilted circle looks like an ellipse)
                const double rPx = dW2 * px;
                double t0 = 0, t1 = 2 * kPi;
                int NN = static_cast<int>(std::min(720.0, std::max(48.0, std::ceil(rPx / 4))));
                bool drawIt = true;
                if (!d3 && !realP && bigR(W, H, rPx)) {   // a giant flat orbit: only the visible stretch is worked out (not for the real Moon's ring: its plane is not the ecliptic)
                    double a0, a1;
                    const Win w = circWin(W, H, pc.first, pc.second, rPx, a0, a1);
                    if (w != Win::Arc) drawIt = false;
                    else { t0 = -a1; t1 = -a0; NN = static_cast<int>(std::min(400.0, std::max(8.0, std::ceil((t1 - t0) * rPx / 6)))); }
                }
                if (drawIt) {
                    for (int pass = 0; pass < (fast ? 2 : 1); pass++) {
                        const Color c = pass ? (on ? kHL : hexColor(b.color)).withAlpha(0.55f) : hexColor(b.color, on ? 0.55f : 0.28f);
                        LinePen pen{R, W, H, pass ? 3.0 : 1.0, c};
                        for (int i = 0; i <= NN; i++) {
                            const double t = t0 + (t1 - t0) * i / NN, Y = dW * std::sin(t);
                            const Vec3 P = realP ? b.pos + (rh * std::cos(t) + ph * std::sin(t)) * dW2 : Vec3{b.pos.x + dW * std::cos(t), b.pos.y + Y * ct, b.pos.z + Y * st};
                            const Q qq = proj(P.x, P.y, P.z);
                            pen.to(qq.first, qq.second);
                        }
                    }
                }
            }
            const double Y = dW * yr * sgn;
            const Q mp = realP ? proj(b.pos.x + dW2 * rh.x, b.pos.y + dW2 * rh.y, b.pos.z + dW2 * rh.z) : proj(b.pos.x + dW * xr, b.pos.y + Y * ct, b.pos.z + Y * st);
            const double mr = TRUE_SCALE ? m.radiusKm * kKm * px : 1.6 + 0.9 * std::max(0.0, std::log10(m.radiusKm / 50));
            const bool mtiny = TRUE_SCALE && mr < 1.2;
            const double md = mtiny ? 3.5 : mr;
            if (!fast) {
                const Light3 sun = sunLight(b.pos);         // 0.46.7: the real Sun, seen from the planet (the moon's own offset changes it by under 0.2 degrees)
                R.disc(mp.first, mp.second, mr + 0.8, kCard);
                if (b.name == "Earth" && j == 0 && mr > 3 && R.hasMap(render::Map::Moon)) {
                    // 0.26: our Moon with its map (NASA SVS CGI Moon Kit). 0.50 (review item 9): from 1800 to 2100 (where its ELP place is used) it is turned by the IAU
                    // rotation model (orbit/moon_orientation.h): its real tilted axis and the libration, the slow rocking that shows a little more than half; the drawn
                    // sub-Earth point is within 0.006 deg of JPL Horizons (tests/test_orbit.cpp). Outside those years: its 0° meridian faces Earth, its axis upright to the ecliptic.
                    const Vec3 out = realM ? rh : Vec3{xr, yr * sgn * ct, yr * sgn * st}, P{0, 0, 1};
                    Vec3 A = out * -1.0;
                    A = A - P * dot(A, P);
                    A = A * (1 / std::max(length(A), 1e-12));
                    Vec3 axes[3] = {A, cross(P, A), P};
                    if (realM) moonBodyAxes(sys.julianDayTDB(), axes);
                    const Vec3 V[3] = {VX, VY, VZ};
                    float K[9];
                    for (int r2 = 0; r2 < 3; r2++) for (int c2 = 0; c2 < 3; c2++) K[3 * r2 + c2] = static_cast<float>(dot(V[c2], axes[r2]));
                    render::Renderer2D::PlanetLight pl;   // 0.51.2: Earth's shadow (lunar eclipses), at the real places
                    pl.sunAng = sunAngAt(b.pos);
                    if (realM) addOcc(pl, scr(sys.moonGeocentric() * -1.0, m.radiusKm * kKm), b.radius / (m.radiusKm * kKm));
                    R.planet(mp.first, mp.second, mr, hexColor(marks::kMoon), sun.x, sun.y, sun.z, K, render::Map::Moon, nullptr, 0, 1.0, polarRatio("Moon"), true, &pl);   // 0.50: McEwen's lunar law
                } else if (realS && mr > 3 && R.hasMap(render::mapForBody(m.name))) {
                    // 0.50 (review item 10): a moon with a global map (USGS mosaics, black and white: assets/maps/README.md), turned by its IAU rotation model
                    // (orbit/iau_rotation.h, NAIF pck00011); Lambert shading in linear light.
                    Vec3 axes[3];
                    satDrawnAxes(m.name, sys.julianDayTDB(), sp, axes);
                    const Vec3 V[3] = {VX, VY, VZ};
                    float K[9];
                    for (int r2 = 0; r2 < 3; r2++) for (int c2 = 0; c2 < 3; c2++) K[3 * r2 + c2] = static_cast<float>(dot(V[c2], axes[r2]));
                    render::Renderer2D::PlanetLight pl;   // 0.51.2: the planet's shadow (the moon eclipsed), at the real place (sp, km from the planet)
                    pl.sunAng = sunAngAt(b.pos);
                    addOcc(pl, scr(sp * (-kKm), m.radiusKm * kKm), b.radius / (m.radiusKm * kKm));
                    R.planet(mp.first, mp.second, mr, hexColor(marks::kMoon), sun.x, sun.y, sun.z, K, render::mapForBody(m.name), nullptr, 0, 1.0, 1.0, false, &pl);
                } else   // no map (Titan: its haze hides the surface in visible light; Enceladus: only an airbrushed map; Titania, Oberon, Deimos: none): a grey ball, the card says "colour not measured here"
                    R.sphere(mp.first, mp.second, mr, hexColor(marks::kMoon), sun.x, sun.y, sun.z);
                if (mtiny) { R.fadeDisc(mp.first, mp.second, 5.5, hexColor(marks::kMoon, 0.3f)); R.disc(mp.first, mp.second, 2.0, hexColor(marks::kMoon)); }   // 0.23 stage 2: a dot with a soft glow, as the tiny planets (was a faint grey ring)
                if (on) hiRing(mp.first, mp.second, md + 5);
            }
            exag_[{b.id, static_cast<int>(j)}] = dW / aM;
            hits_.push_back({{Selection::Moon, b.id, static_cast<int>(j)}, mp.first, mp.second, std::max(md, mtiny ? 9.0 : 0.0)});
            if (here) { labels.push_back({m.name, mp.first, mp.second - md - 4, on, true, false}); labels.back().prio = 4; }
        }
    }

    // ---- far-away things: real stars, clusters, the galactic centre and the Voyagers at their true direction and distance ----
    bool massShown = false;                               // a star's mass label is on screen: the note about masses is shown too
    if (set.far) {
        const auto fmtLyS = [](double v) { return fmtLy(v); };
        const auto inView = [&](const Q& q) { return q.dep - cd < 0.9 * D && q.first > -30 && q.first < W + 30 && q.second > -30 && q.second < H + 30; };
        const Color gold = hexColor(marks::kProbeRing);
        for (int i = 0; i < farCount(); i++) {
            FarItem f;
            if (!farItem(sys, i, f)) continue;
            const Selection s{Selection::Far, 0, i};
            const bool on = sel == s, probe = f.kind == "probe";
            if (probe && !(set.zoom <= -1.5 || on)) continue;   // the Voyagers are far beyond the planets: shown once you zoom out (or pick one)
            if (probe) {                                  // its path since about Neptune (a straight line), and where it left the Sun's wind
                const Vec3 a = f.dir * (40 * kAU), h = f.dir * (f.probe->cross * kAU);
                const Q qa = proj(a.x, a.y, a.z), qb = proj(f.pos.x, f.pos.y, f.pos.z), hq = proj(h.x, h.y, h.z);
                LinePen pen{R, W, H, 1.0, gold.withAlpha(0.5f)};
                pen.to(qa.first, qa.second); pen.to(qb.first, qb.second);
                if (inView(hq)) {
                    R.ring(hq.first, hq.second, 3.5, 1, gold.withAlpha(0.9f));
                    if (f.probe->cross * kAU * px > 40)
                        labels.push_back({f.name + " left the Sun’s wind here (" + std::to_string(f.probe->crossYear) + ")", hq.first, hq.second + 14, false, true, false});
                }
            }
            Q q = probe ? proj(f.pos.x, f.pos.y, f.pos.z) : flat(f.pos.x, f.pos.y, f.pos.z);
            if (!probe) { q.first += ox; q.second += oy; }
            if (!inView(q)) continue;
            if (probe) { R.disc(q.first, q.second, 2.6, Color{1, 1, 1, 1}); R.ring(q.first, q.second, 6, 1, gold); }
            else if (f.kind == "bh") { R.disc(q.first, q.second, 5.3, Color{1, 1, 1, 1}); R.disc(q.first, q.second, 4.5, Color{0, 0, 0, 1}); }
            else if (f.kind == "object") { R.ring(q.first, q.second, 6, 1, hexColor(marks::kObject)); R.disc(q.first, q.second, 1.6, hexColor(marks::kObject)); }
            else { const double bv = farBV(f.far->key); const std::array<int, 3> c = std::isnan(bv) ? std::array<int, 3>{255, 255, 255} : bvRGB(bv); R.disc(q.first, q.second, 2.8, rgba(c[0], c[1], c[2])); R.ring(q.first, q.second, 6, 1, Color{1, 1, 1, 0.45f}); }
            if (on) hiRing(q.first, q.second, 11);
            if (f.kind == "bh") {
                labels.push_back({"Galactic centre · " + fmtLyS(f.ly) + " light years", q.first, q.second - 11, on, false, true, true});
                labels.push_back({f.name + " (black hole)", q.first, q.second + 19, on, false, true, true});
                const std::string mt = farMassText(f.key);                  // published mass (far_masses.h), under the name
                if (!mt.empty()) { labels.push_back({mt, q.first, q.second + 32, on, true, false}); massShown = true; }
            } else {
                labels.push_back({f.name + " · " + (probe ? fmt::commas(f.au) + " AU" : fmtLyS(f.ly) + " light years"), q.first, q.second - 11, on, true, false});
                const std::string mt = probe ? std::string() : farMassText(f.key);
                if (!mt.empty()) { labels.push_back({mt, q.first, q.second + 19, on, true, false}); massShown = true; }
            }
            hits_.push_back({s, q.first, q.second, 10});
        }
    }

    // ---- belt names (clickable) ----
    struct BeltLabel { std::string name; double x, y; bool on; };
    std::vector<BeltLabel> beltLabels;
    if (set.belts) {
        const auto put = [&](const char* key, double rAU, double ang, const char* name) {
            for (std::size_t gi = 0; gi < sys.belts.size(); gi++) {
                if (sys.belts[gi].key != key || rAU * kAU * px < 70) continue;
                const Q q = proj(rAU * kAU * std::cos(ang), rAU * kAU * std::sin(ang), 0);
                const Selection s{Selection::Belt, 0, static_cast<int>(gi)};
                beltLabels.push_back({name, q.first, q.second + 4, sel == s});
                hits_.push_back({s, q.first, q.second + 4 - 5, 28});
            }
        };
        put("main", 3.35, 3.7, "Asteroid belt");
        put("kuiper", 50, 3.9, "Kuiper belt");
        put("oort", 60000, 3.9, "Oort cloud");
        for (const Body& b : sys.bodies)
            if (b.name == "Jupiter") put("trojan", 5.2, std::atan2(b.pos.y, b.pos.x) + kPi / 3, "Jupiter's Trojans");
    }

    // ---- the notes at the bottom left: "Zoomed far out ..." and, above it, the line about the star masses ----
    // 0.39.3: they were wrapped by a guessed character width with a fixed 14-point line, so with Extra large text the lines ran into each other,
    // under the readout and under the Controls panel. Now they are wrapped by their measured width, spaced by their size, the block moves up
    // above any box at the bottom (the readout, the toolbar), and no label is drawn under it. (Drawn with the scale bar, below.)
    NoteBlock notes;
    {
        const double nLeft = 14, nMaxW = std::max(120.0, W - set.insetRight - 84 - nLeft);   // (clear of the axes gizmo and the panel's handle: 80 points)
        int msgNo = 0;
        const auto wrapRows = [&](const std::string& msg, double size, Color c) {
            std::string cur, word;
            for (char ch : msg + " ") {
                if (ch != ' ') { word += ch; continue; }
                if (word.empty()) continue;
                const std::string t = cur.empty() ? word : cur + " " + word;
                if (!cur.empty() && T.width(t, size) > nMaxW) { notes.rows.push_back({cur, size, c, msgNo}); cur = word; } else cur = t;
                word.clear();
            }
            if (!cur.empty()) notes.rows.push_back({cur, size, c, msgNo});
            msgNo++;
        };
        const double spanAU = W / px / kAU, spanLy = spanAU / kLightYearAU;
        if (massShown) wrapRows("Star masses are published estimates, in solar masses (M☉); some are uncertain.", 10, kMuted);
        if (spanAU >= 1500) {                             // zoomed far out: say where you are
            const std::string sp2 = spanLy >= 1 ? fmtLy(spanLy) + " light years" : fmt::commas(std::stod(fmt::precision(spanAU, 2))) + " AU (" + fmt::fixed(spanLy, 2) + " light years)";
            wrapRows("Zoomed far out: this picture is about " + sp2 + " across, so the whole Solar System is now just the ringed dot (the Sun).", 11, kFg);
        }
        // the block moves up above the boxes at the bottom; if it then runs into a box higher up (the subject card), the top rows (the masses line
        // first) are left out until it fits, or the notes are not drawn
        while (!notes.rows.empty()) {
            notes.total = notes.wMax = 0;
            for (const NoteRow& r : notes.rows) { notes.total += TextPainter::pixels(r.size, 1) * 1.3; notes.wMax = std::max(notes.wMax, T.width(r.t, r.size)); }
            double bottom = H - 24 - set.reserveBottom - 30;
            const auto across = [&](const std::array<double, 4>& b, double bt) { return b[0] < nLeft + notes.wMax && b[0] + b[2] > nLeft && b[1] < bt && b[1] + b[3] > bt - notes.total; };
            for (int pass = 0; pass < 2; pass++)
                for (const auto& b : set.blocked)
                    if (across(b, bottom) && b[1] > H / 2) bottom = b[1] - 4;
            bool hit = bottom - notes.total < set.insetTop;
            for (const auto& b : set.blocked) if (across(b, bottom)) hit = true;
            notes.top = bottom - notes.total;
            if (!hit) break;
            const int first = notes.rows.front().msg;
            notes.rows.erase(std::remove_if(notes.rows.begin(), notes.rows.end(), [first](const NoteRow& r) { return r.msg == first; }), notes.rows.end());
            notes.total = 0;
        }
        if (notes.total > 0) {
            if (set.shell) setLocal.blocked.push_back({nLeft - 2, notes.top - 2, notes.wMax + 4, notes.total + 4});
        }
    }

    // ---- labels: the selected one first; crowded labels are skipped ----
    {
        std::vector<std::array<double, 3>> placed;        // x, y, width of labels already drawn
        const double rightEdge = W - set.insetRight;          // (the Controls panel covers the picture's right side)
        const auto put = [&](const std::string& t, double x, double y, double size, Color c, bool force) {
            const double wd = T.width(t, size) + 4;
            if (x < -2 || x > rightEdge + 2) return;                  // 0.23 stage 2: its thing is off the picture or under the panel
            if (rightEdge - wd > 8) x = std::clamp(x, 4 + wd / 2, rightEdge - 4 - wd / 2);   // a label near an edge slides in instead of being cut ("ptune")
            for (const auto& b : set.blocked)                  // 0.22 stage 6: never under a bar or a floating box (also a forced label: it could not be read there)
                if (x + wd / 2 > b[0] && x - wd / 2 < b[0] + b[2] && y + 0.3 * size > b[1] && y - 0.95 * size < b[1] + b[3]) return;
            if (!force)
                for (const auto& o : placed)
                    if (std::fabs(o[0] - x) < (o[2] + wd) / 2 && std::fabs(o[1] - y) < size + 3) return;   // (0.22 stage 6: the text's own height + 3, was 13 for every size)
            placed.push_back({x, y, wd});
            T.text(t, x, y, size, c);
        };
        double sunLy = star.second - srd - (sunTiny ? 11 : 8);
        if (baryShown) {   // 0.46.7 (outside review): not on the barycentre's crosshair (10 px round it) or its label (22 px below it): then under the Sun
            const double half = T.width("Sun", 12) / 2 + 2 + std::max(12.0, T.width("Barycentre", 12) / 2 + 2);
            const auto onBary = [&](double ly) { return std::fabs(baryQ.first - star.first) < half && ly + 4 > baryQ.second - 12 && ly - 12 < baryQ.second + 26; };
            const double below = star.second + srd + (sunTiny ? 21 : 18);
            if (onBary(sunLy) && !onBary(below)) sunLy = below;
        }
        put("Sun", star.first, sunLy, 12, kFg, true);   // 0.22: just its name
        std::stable_sort(labels.begin(), labels.end(), [](const Label& a, const Label& b) { return a.selected != b.selected ? a.selected > b.selected : a.prio < b.prio; });   // (0.23 stage 2: then planets, dwarf planets, comets, moons)
        int nearSun = 0;
        const auto putBody = [&](const Label& l) {
            if (!(l.selected || l.force) && set.shell && std::hypot(l.x - star.first, l.y - star.second) < 36) { nearSun++; return; }
            put(l.name, l.x, l.y, l.gc ? 11 : l.small ? 10 : 12, kFg, l.selected || l.force);
        };
        for (const Label& l : labels) if (l.selected || l.prio <= 1) putBody(l);   // 0.23 stage 2: the planets before the belt names (a belt name hid Mars)
        for (const BeltLabel& l : beltLabels) {
            if (l.on) {                                   // the selected belt: white text on a pink box
                const double tw = T.width(l.name, 12);
                const ImVec2 a(static_cast<float>(originX + (l.x - tw / 2 - 6) * uiScale), static_cast<float>(originY + (l.y - 12) * uiScale));
                const ImVec2 b(static_cast<float>(originX + (l.x + tw / 2 + 6) * uiScale), static_cast<float>(originY + (l.y + 6) * uiScale));
                dl->AddRectFilled(a, b, ImGui::ColorConvertFloat4ToU32(ImVec4(kHL.r, kHL.g, kHL.b, 1)), ui::corner(static_cast<float>(4 * uiScale)));   // 0.38.1: a 4 px box, not a pill (the owner's new look; square in Monochrome)
                T.text(l.name, l.x, l.y, 12, Color{1, 1, 1, 1}, 0.5, false);
                placed.push_back({l.x, l.y, tw + 12});
            } else put(l.name, l.x, l.y, 10, kMuted, false);
        }
        // 0.22 stage 6: labels of bodies drawn right next to the Sun (zoomed out) are left out, with one hint instead of a pile of names on top of each other
        for (const Label& l : labels) if (!(l.selected || l.prio <= 1)) putBody(l);
        if (nearSun >= 2) put("inner planets: zoom in", star.first, star.second + srd + 30, 10, kMuted, false);
    }

    // ---- info card, bottom right (the first click closes it) ----
    double gizY = H - 44 - set.reserveRight, barY = H - 24 - set.reserveBottom;   // (the new UI keeps the readout and the toolbar clear)                  // the x y axes and the scale bar sit above the card when there is one
    Card card;
    CardOptions copt;
    copt.paused = set.paused; copt.tspeed = set.tspeed; copt.trueScale = TRUE_SCALE; copt.moonsOn = set.moons; copt.beltsOn = set.belts;
    if (sel.kind == Selection::Moon) { auto it = exag_.find({sel.bodyId, sel.moon}); copt.moonExaggeration = it == exag_.end() ? 1 : it->second; }
    if (set.shell) {                                      // the new UI's subject card is made from this same text (orbit/subject_card.h): worked out once per frame, here
        cardOk = sel.kind != Selection::None && !journey && cardText(sys, sel, copt, card);
        if (cardOk) { lastCard = std::move(card); cardSel = sel; }
    } else
    if (sel.kind != Selection::None && !cardHidden && !journey && cardText(sys, sel, copt, card)) {
        static const bool logCard = std::getenv("PL_LOG_CARD") != nullptr;      // for the automatic checks: print the card the old layout draws, once per change
        if (logCard) {
            static std::string lastLogged;
            std::string sig = card.title;
            for (const auto& l : card.lines) sig += "\t" + l;
            if (sig != lastLogged) { lastLogged = sig; std::printf("PLCARD %s\n", sig.c_str()); std::fflush(stdout); }
        }
        const double k = std::max(0.74, std::min(1.0, W / 620)), LH = 13.5 * k, F = 10.5 * k, FT = 11 * k;
        double cw = T.width(card.title, FT);
        for (const auto& l : card.lines) cw = std::max(cw, T.width(l, F));
        cw = std::min(cw + 20 * k, W - 16);
        const double ch = 26 * k + LH * (card.lines.size() + 1) + 6 * k, x0 = W - cw - 8, y0 = H - ch - 8;
        gizY = y0 - 30; barY = y0 - 26;
        // drawn with Dear ImGui so it sits above the picture's text
        const ImVec2 a(static_cast<float>(originX + x0 * uiScale), static_cast<float>(originY + y0 * uiScale));
        const ImVec2 b(static_cast<float>(originX + (x0 + cw) * uiScale), static_cast<float>(originY + (y0 + ch) * uiScale));
        dl->AddRectFilled(a, b, ImGui::ColorConvertFloat4ToU32(ImVec4(kCard.r, kCard.g, kCard.b, 0.84f)), static_cast<float>(9 * k * uiScale));
        dl->AddRect(a, b, ImGui::ColorConvertFloat4ToU32(ImVec4(kLine.r, kLine.g, kLine.b, kLine.a)), static_cast<float>(9 * k * uiScale));
        T.text(card.title, x0 + 10 * k, y0 + 17 * k, FT, kFg, 0, false);
        for (std::size_t i = 0; i < card.lines.size(); i++) T.text(card.lines[i], x0 + 10 * k, y0 + 32 * k + LH * i, F, kMuted, 0, false);
        T.text("click anywhere to close this box", x0 + 10 * k, y0 + 32 * k + LH * card.lines.size() + 2, 9.5 * k, kMuted.withAlpha(0.7f), 0, false);
    }

    {   // scale bar: how long a round distance is at this zoom, and how long light takes to cross it
        const ScaleBar sbar = scaleBarFor(px, d3);
        const double len = sbar.len;
        const std::string& main = sbar.main;
        const std::string& lt = sbar.light;
        if (notes.total > 0) {             // the notes at the bottom left (laid out before the labels)
            double y = notes.top + (set.shell ? 0 : barY - (H - 24 - set.reserveBottom));
            ui::auditRect("farnote", static_cast<float>(originX + 14 * uiScale), static_cast<float>(originY + y * uiScale), static_cast<float>(notes.wMax * uiScale), static_cast<float>(notes.total * uiScale));
            for (const NoteRow& r : notes.rows) {
                const double lh = TextPainter::pixels(r.size, 1) * 1.3;
                T.text(r.t, 14, y + lh * 0.8, r.size, r.c, 0, true, kCard);
                y += lh;
            }
        }
        const double x0 = 14, y = barY;
        scaleText = main; lightText = "light takes " + lt; scaleLen = len;     // the new UI shows these in its readout
        for (int pass = 0; pass < 2 && !set.shell; pass++) {
            const Color c = pass ? kFg : kCard;
            const double w = pass ? 1.6 : 5;
            R.line(x0, y - 4, x0, y + 4, w, c);
            R.line(x0, y, x0 + len, y, w, c);
            R.line(x0 + len, y - 4, x0 + len, y + 4, w, c);
        }
        if (!set.shell) {
            T.text(main, x0, y - 8, 11, kFg, 0, true, kCard);
            T.text("light takes " + lt, x0, y + 15, 10, kMuted, 0, true, kCard);
        }
    }

    drawAxesGizmo(R, T, W - 40 - set.insetRight, gizY, ca, sa, cb, sb, originX, originY, uiScale);   // axis gizmo: which way x, y and z point on screen

    if (cap.on) {   // the Journey's caption (laid out at the start of draw, so the labels keep clear of it)
        const double cx = cap.cx >= 0 ? cap.cx : W / 2 + fx;
        ui::auditRect("journeycap", static_cast<float>(originX + (cx - cap.wMax / 2) * uiScale), static_cast<float>(originY + cap.top * uiScale), static_cast<float>(cap.wMax * uiScale), static_cast<float>(cap.lines.size() * cap.lineH * uiScale));
        for (std::size_t i = 0; i < cap.lines.size(); i++)
            T.text(cap.lines[i], cx, cap.top + cap.ascent + i * cap.lineH, cap.sz, Color{1, 1, 1, static_cast<float>(captionAlpha)}, 0.5, true, Color{4 / 255.f, 6 / 255.f, 13 / 255.f, 0.92f});
    }

    {   // top left: the date, which moves on with the simulated time (the frames-per-second counter is drawn by main.cpp, for every lab)
        if (!set.shell) T.text(dateLine(sys), 12, 20, 13, kFg, 0, true);
    }
}

#include "alphacen_view.inc"

}  // namespace orbit
