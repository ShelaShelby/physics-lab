#include "orbit_view.h"
#include "lagrange.h"
#include "orbit/star_light.h"
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
#include "far_occlusion.h"
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
#include "sunspots.h"
#include "sun_photo.h"
#include "render/sun_surface.h"
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include "true_light.h"
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
// 0.75.0: L, the lens: 2D the map of orbit/sky.cpp skyScreen (the angle from straight ahead becomes the distance from the middle, 100 degrees at the corner);
// 3D the scene's own pinhole camera (orbit/camera.h), so the sky and the planets move together when the view turns.
// 0.86.0: trueWhite > 0 (Lighting: True sunlight): the screen's white in units of the Sun's surface luminance (orbit/star_light.h sunlitWhite). Each star then gets
// its true light on that exposure (skyStarDot) and is drawn only where it reaches the screen's faintest step; the Gaia map, which has no calibrated brightness
// (blackhole/light.h), is not drawn. trueWhite 0 (Even): the sky exactly as before.
void drawSky(render::Renderer2D& R, TextPainter& T, double W, double H, double ca, double sa, double cb, double sb, ConsMode cons, double lv, const Sky& S, const SkyLens& SL, double trueWhite = 0) {   // 0.73.0: S, the sky seen from where the scene is
    const double cx = W / 2, cy = H / 2, diag = std::hypot(W, H) / 2, K = diag / 1.75;   // K: points per radian away from the middle (100° reaches the corner)
    // a direction -> screen (orbit/sky.cpp skyScreenLens)
    const auto scr = [&](const Vec3& v, double& X, double& Y) { return skyScreenLens(v, W, H, ca, sa, cb, sb, SL, X, Y); };
    // The Milky Way (0.26): the Gaia photo of the whole sky.
    static std::vector<BandPoint> band;
    {
        // 0.26 (owner's choice, 5 Nov 2026): the smooth drawn band is replaced by ESA's Gaia all-sky colour map (render/planet_maps.h), drawn per pixel by the
        // renderer through the same map as the stars (orbit/sky.cpp skyViewToGalactic). The band's points are still worked out: its label goes on them.
        milkyWayBand(S, W, H, ca, sa, cb, sb, band, SL);
        double M[9];
        skyViewToGalactic(S, ca, sa, cb, sb, M);
        float Mf[9];
        for (int k = 0; k < 9; k++) Mf[k] = static_cast<float>(M[k]);
        if (trueWhite > 0) {}                             // 0.86.0: not on the exposure: not drawn (its pixels have no calibrated brightness)
        else if (SL.pinhole) R.skyMapPinhole(W, H, SL.ppx, SL.ppy, SL.F, Mf, lv);   // 0.75.0: each pixel's direction by the pinhole turned back
        else R.skyMap(W, H, K, Mf, lv);
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
        static const bool logStars = std::getenv("PL_LOG_STARS") != nullptr;   // 0.75.0, for automatic checks only: where the stars brighter than 6 are drawn
        if (logStars && S.stars[i].mag < 6) std::printf("PLSTARPX %zu %.3f %.3f\n", i, X, Y);
        // 0.86.0: one dot rule (orbit/star_light.h skyStarDot): Even exactly starDraw x lv; True sunlight the star's true light on the exposure
        const double ptSr = trueWhite > 0 ? (SL.pinhole ? pinholePointSr(SL.F, SL.ppx, SL.ppy, X, Y) : mapPointSr(K, cx, cy, X, Y)) : 0.0;
        const SkyDot d = skyStarDot(S.stars[i].mag, lv, trueWhite, ptSr);
        if (!d.drawn) continue;
        const auto& c = S.colours[S.stars[i].bucket];
        R.disc(X, Y, d.radius, rgba(c[0], c[1], c[2], static_cast<float>(d.alpha)));
    }
    if (cons != ConsMode::Off) {                          // constellation lines (Stellarium's modern sky culture)
        const double cl2 = std::max(lv, 0.6);
        const Color lc{115 / 255.f, 155 / 255.f, 235 / 255.f, static_cast<float>(0.42 * cl2)};
        for (const auto& L : S.lines)
            for (const auto& ab : L) {
                const double xa = pos[ab.first * 2], ya = pos[ab.first * 2 + 1], xb = pos[ab.second * 2], yb = pos[ab.second * 2 + 1];
                if (xa < -1e8 || xb < -1e8) continue;
                if (SL.pinhole) {   // 0.75.0: a great circle is a straight line through the pinhole: the part on the picture
                    double cl[4];
                    if (clipSeg(xa, ya, xb, yb, -20, -20, W + 20, H + 20, cl)) R.line(cl[0], cl[1], cl[2], cl[3], 0.9, lc);
                    continue;
                }
                if (std::hypot(xa - xb, ya - yb) > 0.7 * diag) continue;
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
    if (sel.kind != Selection::None && !cardHidden) { cardHidden = true; return; }   // (the old UI's in-picture card: the first click only closes it; the new UI has none)
    const Hit* best = nullptr;
    double bd = 1e9;
    for (const Hit& h : hits_) {
        const double d = std::hypot(x - h.x, y - h.y);
        if (d < std::max(h.r + 6, 12.0) && d < bd) { best = &h; bd = d; }
    }
    if (!best) return;                                    // 0.74.0: a click on empty space changes nothing (before: it let go of what was followed, and time carried it off the picture)
    if (follow && !(best->sel == sel)) { pick(best->sel); return; }   // followed: the camera glides to the clicked thing
    if (!follow && best->sel.kind != Selection::Belt) { pick(best->sel); return; }   // 0.75.0: not following: the camera flies to it too (a pick; a belt is only selected)
    sel = best->sel;
    cardHidden = false;
}

void OrbitView::drag(double dx, double dy, bool rotate) {
    // 3D: drag turns the view (0.73.0: in the Alpha Centauri scene it may turn right over, so you can look back at our Sun, which is towards +z)
    if (rotate) {
        const double y0 = yaw, p0 = pitch;
        double dYaw = 0, dPitch = 0;
        dragAngles(dx, dy, dYaw, dPitch);                 // (0.82.0: orbit/camera_motion.h, shared with the Black Hole Lab)
        yaw += dYaw; pitch = std::clamp(pitch + dPitch, acScene ? -3.1 : -kPitchMax, acScene ? 3.1 : kPitchMax);
        turnYaw_ += yaw - y0; turnPitch_ += pitch - p0; turned_ = true;
        return;
    }
    if (held_) { ux += dx; uy += dy; return; }            // 0.74.0: a followed thing stays followed; it is held where the slide put it (holdAt keeps that in the free part)
    ox += dx; oy += dy;                                   // nothing followed: the barycentre moves with the pointer
}

void OrbitView::zoomChanged(double oldZoom, double newZoom) {   // zoom about the middle of the picture
    const double f = std::pow(10.0, newZoom - oldZoom);
    ox *= f; oy *= f;
}

void OrbitView::resetView() {
    ox = oy = 0; ux = uy = 0; follow = false; yaw = 0.6; pitch = 1.0; cdep = 0;
    glide_ = glidePending_ = false; spinYaw_ = spinPitch_ = 0;
}

void OrbitView::pick(Selection s) {
    if (reduceMotion || journey) { select(s, true); return; }   // Reduce motion: at once
    sel = s; follow = true; cardHidden = false; ux = uy = 0;
    glidePending_ = true; glide_ = false; glideS_ = 0; glideSel_ = s;   // (the glide starts in the next drawing, from where the thing is on the screen then)
}

void OrbitView::grab() { pressed_ = true; spinYaw_ = spinPitch_ = 0; }

void OrbitView::release() {
    pressed_ = false;
    momentumRelease(spinYaw_, spinPitch_, reduceMotion);   // (0.82.0: orbit/camera_motion.h, shared with the Black Hole Lab)
}

void OrbitView::tick(double dt, const ViewSettings& set) {
    // a turn or tilt set by someone else since the last tick (Learn, the Journey, a benchmark scene, PL_TILT) stops the momentum
    if ((!turned_ && (yaw != heldYaw_ || pitch != heldPitch_)) || reduceMotion || journey || !set.view3D) { if (!pressed_) spinYaw_ = spinPitch_ = 0; }
    if (pressed_) {                                       // the drag's speed, smoothed over a few frames (a held, still pointer brings it to 0)
        momentumFollow(spinYaw_, spinPitch_, turnYaw_, turnPitch_, dt);
    } else if (spinYaw_ != 0 || spinPitch_ != 0) {        // a released turn coasts, fading; it ends exactly (0 below kMomentumStop)
        yaw += spinYaw_ * dt;
        pitch = std::clamp(pitch + spinPitch_ * dt, acScene ? -3.1 : -kPitchMax, acScene ? 3.1 : kPitchMax);
        momentumFade(spinYaw_, spinPitch_, dt);
    }
    turnYaw_ = turnPitch_ = 0; turned_ = false;
    if (set.view3D && spin) yaw += 0.15 * dt;             // ⟳ Spin: the 3D view turns slowly
    if (glide_) { glideS_ += dt / kGlideSeconds; if (glideS_ >= 1) glide_ = false; }   // the glide ends exactly: then the plain follow
    heldYaw_ = yaw; heldPitch_ = pitch;
}

namespace { double smooth01(double s) { s = std::clamp(s, 0.0, 1.0); return s * s * (3 - 2 * s); } }

// 0.75.0: the closest zoom on a followed body keeps the camera outside it: d = focal / px >= 1.5 R (3D at True scale; Easy to see draws no true size).
// px = min(W, H) / 34 x 10^zoom / kSimLength, so 10^zoom <= focal / (1.5 R) x 34 kSimLength / min(W, H).
double OrbitView::zoomCapFor(const SolarSystem& sys, const ViewSettings& set, double W, double H) const {
    if (!set.view3D || !follow || W <= 0 || H <= 0) return kZoomMax;
    double R = 0;
    if (set.alphaCen) {                                   // the Alpha Centauri scene: its stars are drawn at their true size
        if (sel.kind == Selection::ACen && (sel.moon == acen::StarA || sel.moon == acen::StarB || sel.moon == acen::Proxima)) R = acen::starFacts(sel.moon - acen::StarA).radius * kSunRadius;
        return R > 0 ? std::min(kZoomMax, std::log10(focalFor(H) / (1.5 * R) * 34 * kSimLength / std::min(W, H))) : kZoomMax;
    }
    if (!set.trueScale) return kZoomMax;
    if (sel.kind == Selection::Star) R = kSunRadius;
    else if (sel.kind == Selection::Body || sel.kind == Selection::Moon) {
        const Body* b = findBody(sys, sel.bodyId);
        if (!b) return kZoomMax;
        R = b->radius;
        if (sel.kind == Selection::Moon) { if (!set.moons || sel.moon < 0 || sel.moon >= static_cast<int>(b->moons.size())) return kZoomMax; R = b->moons[std::size_t(sel.moon)].data->radiusKm * kKm; }
    }
    if (!(R > 0)) return kZoomMax;
    const double cap = std::log10(focalFor(H) / (1.5 * R) * 34 * kSimLength / std::min(W, H));
    return std::min(kZoomMax, cap);
}

double OrbitView::holdDepth(double dep) {
    if (glidePending_) glideDep0_ = cdep;
    if (!glide_ && !glidePending_) return dep;
    const double e = glidePending_ ? 0 : smooth01(glideS_);
    return glideDep0_ + (dep - glideDep0_) * e;
}

void OrbitView::holdAt(double qx, double qy, double W, double H, const ViewSettings& set) {
    held_ = true;
    const double bx = W / 2 + fx + cx, by = H / 2 + cy, m = 24;   // the held place without the slide; the slide keeps it 24 points inside the free part
    const double xl = m - bx, xh = W - set.insetRight - m - bx, yl = set.insetTop + m - by, yh = H - set.reserveBottom - m - by;
    if (xl <= xh) ux = std::clamp(ux, xl, xh);
    if (yl <= yh) uy = std::clamp(uy, yl, yh);
    const double hx = bx + ux, hy = by + uy;
    if (glidePending_) { glideDX_ = qx + ox - hx; glideDY_ = qy + oy - hy; glidePending_ = false; glide_ = true; glideS_ = 0; }   // where it is on the screen now
    const double k = glide_ ? 1 - smooth01(glideS_) : 0;
    ox = hx - qx + k * glideDX_; oy = hy - qy + k * glideDY_;
}

void sunColourBytes(int rgb[3]) { for (int k = 0; k < 3; k++) rgb[k] = sunfacts::kSunSrgb[k]; }

bool OrbitView::selectedScreen(double& x, double& y, double& r) const {
    if (sel.kind == Selection::None) return false;
    for (const Hit& h : hits_) if (h.sel == sel) { x = h.x; y = h.y; r = h.r; return true; }
    return false;
}

void OrbitView::select(Selection s, bool f) {
    sel = s; follow = f; cardHidden = false;
    ox = oy = 0; ux = uy = 0; cdep = 0;
    glide_ = glidePending_ = false; spinYaw_ = spinPitch_ = 0;   // 0.74.0: a direct set: at once
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

const char* const OrbitView::kPictureKeyParts[OrbitView::kKeyParts] = {"the selection, follow and camera (yaw, pitch, the held place, the glide)", "the view settings (zoom, switches, time speed)",
    "the theme and the text and panel insets", "the system's time and state", "the bodies' places, trails and moons", "the picture's place and size"};

std::uint64_t OrbitView::pictureKey(const SolarSystem& sys, const ViewSettings& set, double originX, double originY, double uiScale, double width, double height,
                                    std::uint64_t* parts) const {
    // 0.75.1: each group is hashed on its own (parts[i], named in kPictureKeyParts) and the key joins them, so the app can say which part changed from frame to frame
    std::uint64_t part[kKeyParts] = {0, 0, 0, 0, 0, 0};
    KeyHash k;
    k.add(sel.kind); k.add(sel.bodyId); k.add(sel.moon); k.add(follow); k.add(cardHidden); k.add(ox); k.add(oy); k.add(yaw); k.add(pitch); k.add(cdep); k.add(spin);
    k.add(journey); k.add(caption); k.add(captionAlpha); k.add(panMode); k.add(fx); k.add(cx); k.add(cy); k.add(ux); k.add(uy);
    part[0] = k.h; k = KeyHash();
    k.add(set.view3D); k.add(set.zoom); k.add(set.tspeed); k.add(set.trueScale); k.add(set.fullTrails); k.add(set.moons); k.add(set.dwarfs); k.add(set.comets);
    k.add(set.belts); k.add(set.gpuRocks); k.add(set.sky); k.add(set.cons); k.add(set.realPlanets); k.add(set.exaggerateWobble); k.add(set.relativity); k.add(set.far);
    k.add(set.orbits); k.add(set.alphaCen); k.add(set.paused); k.add(set.trueLight); k.add(set.overlay.version); k.add(set.overlay.lagrange); k.add(set.overlay.transfer);
    part[1] = k.h; k = KeyHash();
    k.add(set.lightTheme); k.add(set.theme.id); k.add(set.theme.light); k.add(set.theme.bg); k.add(set.theme.fg); k.add(set.theme.accent);
    k.add(set.textScale); k.add(set.shell); k.add(set.minTextU); k.add(set.minTextPx); k.add(set.reserveBottom); k.add(set.reserveRight); k.add(set.insetTop);
    k.add(set.insetRight); k.add(set.blocked.size()); for (const auto& b : set.blocked) k.bytes(b.data(), sizeof(double) * 4);
    part[2] = k.h; k = KeyHash();
    k.add(sys.t); k.add(sys.epochMs); k.add(sys.jumped); k.add(sys.crashed); k.add(sys.escaped); k.add(sys.realPlanets()); k.add(sys.relativity());
    k.add(sys.bodies.size()); k.add(sys.sunTrail.size()); if (!sys.sunTrail.empty()) k.add(sys.sunTrail.back());
    part[3] = k.h; k = KeyHash();
    for (const Body& b : sys.bodies) {
        k.add(b.id); k.add(b.pos); k.add(b.drawShift); k.add(b.moons.size()); k.add(b.trail.size());
        if (!b.trail.empty()) { k.add(b.trail.front()); k.add(b.trail.back()); }
    }
    k.add(sys.belts.size());
    part[4] = k.h; k = KeyHash();
    k.add(originX); k.add(originY); k.add(uiScale); k.add(width); k.add(height);
    part[5] = k.h;
    KeyHash all;
    for (int i = 0; i < kKeyParts; i++) { all.add(part[i]); if (parts) parts[i] = part[i]; }
    return all.h;
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
    const bool TRUE_SCALE = set.trueScale;
    const Vec3 bc = sys.barycentre();                     // everything is drawn around the barycentre, so the Sun is the one that wobbles

    // the selected thing may be gone (removed, or switched off): orbit/card_text.cpp, tested
    const auto visible = [&](const Body& b) { return (b.type != BodyKind::Dwarf || set.dwarfs) && (b.type != BodyKind::Comet || set.comets); };
    if (!selectionStillValid(sys, sel, SelectionSwitches{set.dwarfs, set.comets, set.moons, set.belts, set.far})) { sel = Selection{}; follow = false; }
    // 0.75.0: the closest zoom keeps the camera outside a followed body (3D, True scale): d >= 1.5 of its radius (zoomCapFor); the panel keeps the zoom there too
    zoomCap_ = zoomCapFor(sys, set, W, H);
    const double px = std::min(W, H) / 34 * std::pow(10.0, std::min(set.zoom, zoomCap_)) / kSimLength;   // points per metre (at the target's distance, 3D)

    // ---- the camera (0.75.0: orbit/camera.h): turn by yaw about z, tilt by pitch towards the viewer; 3D: a pinhole camera with a kFieldOfViewDeg field,
    // looking at the target from the distance that shows the zoom's span at the target (zooming in moves the camera closer). Until 0.74.0 the camera sat at
    // max(90, 3 x 34 / zoom) sim lengths with its scale clamped at 0.05 of that distance: three times the span, nearly flat, and nothing loomed.
    const bool d3 = set.view3D;
    const double yawA = d3 ? yaw + kPi : 0, pitchA = d3 ? -pitch : 0;   // 3D the right way up: the ecliptic north pole points up the screen
    const double ca = std::cos(yawA), sa = std::sin(yawA), cb = std::cos(pitchA), sb = std::sin(pitchA);
    ViewCam cam;                                          // (cd, ox, oy and the principal point are set after the follow below: finish())
    cam.bc = bc; cam.ca = ca; cam.sa = sa; cam.cb = cb; cam.sb = sb; cam.px = px; cam.W = W; cam.H = H; cam.fx = fx; cam.d3 = d3;
    double cd = cdep;                                     // depth of the look-at plane (the followed thing's)
    const double grow = !TRUE_SCALE && set.zoom > 0 ? std::min(3.2, std::pow(10.0, 0.5 * set.zoom)) : 1;   // Easy to see: a little bigger as you zoom in
    const auto drawnRadius = [&](const Body& b) { return TRUE_SCALE ? b.radius * px : (3 + 1.6 * std::sqrt(b.radius / kEarthRadius)) * grow; };
    // 0.73.1: ONE place for a moon: the drawing (the moons' loop) and the follow block below both call moonAt, so the camera is always on the moon where it is drawn.
    // kf is the perspective scale of the planet; the camera cannot know it before it is centred on the moon, so for the followed moon's planet it is 1 (moonKf), in both.
    struct MoonPlace { double dW, dW2, xr, yr, sgn, ct, st, aM; bool fast, realM, realS, realP; Vec3 rh, ph, sp, sv, pos; };
    const auto moonDisp = [&](const Body& b, std::size_t j, double kf) { return drawnRadius(b) * kf + 7 + 13 * std::log(std::max(b.moons[j].data->aKm / (b.radius / kKm), 1.2)); };   // drawn distance, points
    const auto moonAt = [&](const Body& b, std::size_t j, double kf) {
        MoonPlace r;
        const MoonData& m = *b.moons[j].data;
        const double dps = kSimTime / kDay * std::pow(10.0, set.tspeed);   // simulated days per real second
        r.aM = m.aKm * kKm;                           // real distance, m
        r.dW = std::max(moonDisp(b, j, kf) / (px * kf), TRUE_SCALE ? r.aM : 0.0);   // drawn distance, m
        const double tl = m.tiltDeg * kPi / 180;
        r.ct = std::cos(tl); r.st = std::sin(tl);
        r.fast = dps / m.periodDays > 8;              // a lap in under ~8 frames would flicker: the whole orbit is lit instead
        // mean anomaly grows by 2π per period; Kepler's equation by 3 Newton steps (moon orbits are nearly round)
        const double Mn = b.moons[j].phase + (r.fast ? 0 : (m.retrograde ? -1 : 1) * 2 * kPi * (sys.t / kDay) / m.periodDays);
        double E = Mn + m.e * std::sin(Mn);
        for (int i = 0; i < 3; i++) E += (Mn - (E - m.e * std::sin(E))) / (1 - m.e * std::cos(E));
        r.xr = std::cos(E) - m.e; r.yr = std::sqrt(1 - m.e * m.e) * std::sin(E); r.sgn = m.retrograde ? -1 : 1;
        // 0.10.4 stage 2 (0.46.3: in Simple too): Earth's Moon is at its real direction and phase (ELP2000-82B). It is drawn at the same exaggerated distance as any moon (dW, the label
        // "Orbit drawn about N× too wide" is unchanged), scaled by the Moon's real distance over its mean one (so perigee and apogee show, and at true scale it is the real distance); the ring is the Moon's
        // instantaneous orbit plane (geocentric position x velocity) through the Moon, so the Moon is always on it. Outside 1800-2100 (0.96.0) the same, on the precessing ellipse fitted to ELP (sys.moonPlaced()).
        r.realM = sys.moonPlaced() && b.name == "Earth" && j == 0;
        r.dW2 = r.dW;
        if (r.realM) { const Vec3 g = sys.moonGeocentric(); r.rh = g * (1 / length(g)); r.ph = cross(sys.moonOrbitNormal(), r.rh); r.dW2 = r.dW * length(g) / r.aM; }
        // 0.27: the 13 big moons at their real places (orbit/moon_sats.h, fitted to JPL Horizons), drawn as our real Moon: same direction, the drawn distance
        // scaled by the real one over the mean one, on the ring of its present orbit plane. In both Simple and Real (they are not part of the planets' physics).
        r.realS = !r.realM && !(b.name == "Earth" && j == 0) && satPlace(m.name, sys.julianDayTDB(), r.sp, r.sv);   // 0.88.0: outside 1800-2100 JPL's mean elements (orbit/moon_sats.h)
        if (r.realS) { r.rh = r.sp * (1 / length(r.sp)); Vec3 nn = cross(r.sp, r.sv); nn = nn * (1 / length(nn)); r.ph = cross(nn, r.rh); r.dW2 = r.dW * length(r.sp) / m.aKm; }
        r.realP = r.realM || r.realS;
        const double Y = r.dW * r.yr * r.sgn;
        r.pos = r.realP ? b.pos + r.rh * r.dW2 : Vec3{b.pos.x + r.dW * r.xr, b.pos.y + Y * r.ct, b.pos.z + Y * r.st};
        return r;
    };
    held_ = false;
    if (gliding() && (!(sel == glideSel_) || !follow || journey)) glide_ = glidePending_ = false;   // 0.74.0: the selection was set directly: the glide is dropped
    const bool followMoon = follow && set.moons && sel.kind == Selection::Moon;
    const auto moonKf = [&](const Body& b, double pcf) { return followMoon && b.id == sel.bodyId ? 1.0 : (d3 ? pcf : 1.0); };
    if (follow && sel.kind == Selection::Far) {           // keep a far-away thing in the middle
        FarItem f;
        if (farItem(sys, sel.moon, f)) {
            // 0.75.0: any far thing as a probe: the camera looks at it from the zoom's distance (until 0.74.0 a star was held flat, at the barycentre's depth)
            cd = cdep = holdDepth(cam.depth(f.pos.x, f.pos.y, f.pos.z));
            double qx = 0, qy = 0;
            cam.plane(f.pos.x, f.pos.y, f.pos.z, qx, qy);
            holdAt(qx, qy, W, H, set);
        }
    } else if (follow && sel.kind != Selection::None) {   // keep the followed object in the middle
        Vec3 t;
        if (sel.kind == Selection::Bary) t = bc;
        else if (sel.kind == Selection::Body || sel.kind == Selection::Moon) {
            const Body* b = findBody(sys, sel.bodyId);
            if (b) {
                t = b->pos;
                // a clicked moon is followed where it is DRAWN (0.73.1)
                if (followMoon && sel.moon >= 0 && sel.moon < static_cast<int>(b->moons.size())) t = moonAt(*b, static_cast<std::size_t>(sel.moon), 1.0).pos;
            }
        }
        cd = cdep = holdDepth(cam.depth(t.x, t.y, t.z));  // the look-at plane through it (0.74.0: a glide: on the way)
        double qx = 0, qy = 0;
        cam.plane(t.x, t.y, t.z, qx, qy);
        holdAt(qx, qy, W, H, set);                        // 0.74.0: at the held place (the middle, the centre shift, the user's slide; a glide: on the way)
    }
    // 0.75.0: the camera's axis goes through the held place (the principal point): a followed thing is looked at straight on; in the look-at plane the picture is
    // the 0.74.0 map (ox, oy keep their meaning there: the slide, the glide and the zoom about the middle are unchanged)
    cam.cd = cd; cam.ox = ox; cam.oy = oy; cam.ppx = W / 2 + fx + cx + ux; cam.ppy = H / 2 + cy + uy;
    cam.finish();
    const auto proj = [&](double x, double y, double z) { return cam.proj(x, y, z); };
    // 0.68.0: the same camera for the trails, orbits and distance rings on either path (orbit/orbit_lines.h)
    const LineCam& lcam = cam;
    const bool cardLines = linesOnCard(R);                // their guarded steps advance here (orbit/lines_check.h); true: the card draws them this frame
    LinesPoints linesPts;
    linesPts.card = cardLines ? R.linesCard() : nullptr;
    struct LinesDone { LinesPoints& p; ~LinesDone() { if (p.card) p.card->frameDone(); } } linesDone{linesPts};   // (0.90.0: at the end of the frame's lines, the moons' too)

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
    drawnSel_ = DrawnSel{};
    // 0.85.0 (R1): the true sunlight. The camera's exposure is set for the followed body or moon (its distance from the Sun); nothing followed: 1 au.
    double expAU = kTrueLightRefAU;
    std::string expName;
    if (set.trueLight && follow && (sel.kind == Selection::Body || sel.kind == Selection::Moon)) {
        if (const Body* fb = findBody(sys, sel.bodyId)) {
            expAU = std::max(length(fb->pos), 1.0) / kAU;
            expName = sel.kind == Selection::Moon && sel.moon >= 0 && sel.moon < static_cast<int>(fb->moons.size()) ? std::string(fb->moons[std::size_t(sel.moon)].data->name) : fb->name;
        }
    }
    if (set.sky != SkyMode::Off) {                        // the night sky: dim close up, brighter as you zoom out
        const double zt = std::clamp(-set.zoom / 3.5, 0.0, 1.0), lv = set.sky == SkyMode::Full ? 0.45 + 0.55 * zt : 0.16 + 0.6 * zt;
        SkyLens lens;                                     // 0.75.0: 3D the scene's pinhole camera; 2D the map as before
        if (d3) { lens.pinhole = true; lens.ppx = cam.ppx; lens.ppy = cam.ppy; lens.F = cam.F; }
        // 0.86.0: True sunlight: the stars on the planets' exposure (orbit/star_light.h sunlitWhite); Even: lv from the zoom, as before
        drawSky(R, T, W, H, ca, sa, cb, sb, set.cons, lv, skyData(), lens, set.trueLight ? sunlitWhite(expAU) : 0.0);
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

    struct Label { std::string name; double x, y; bool selected, small, force; bool gc = false; int prio = 5; };   // prio (0.23 stage 2): 1 planet, 2 dwarf planet, 3 comet, 4 moon, 5 the rest: who keeps its place when labels crowd
    std::vector<Label> labels;
    // ---- far-away things: real stars, clusters, the galactic centre and the Voyagers at their true direction and distance ----
    // (0.75.0: drawn here, before the Sun, the orbits and the bodies: they are far beyond all of them, so with the sky's lens a star behind Saturn is hidden by it)
    bool massShown = false;                               // a star's mass label is on screen: the note about masses is shown too
    // 0.77.0 (outside review item 4): drawn after the bodies (below, drawFar()), each marker hidden by a ball in front of it or dimmed by a ring's own transmission
    // (orbit/far_occlusion.h). Until 0.76.0 they were drawn first and their labels last, so a marker and its name showed through Saturn's ball and rings.
    FarOccluders farOcc;
    const auto drawFar = [&]() {
    if (set.far) {
        const auto fmtLyS = [](double v) { return fmtLy(v); };
        // 0.75.0: in front of the camera and on the picture (until 0.74.0: depth - cd < 0.9 D, the old camera's distance)
        const auto inView = [&](const Q& q) { return q.f > 0 && q.first > -30 && q.first < W + 30 && q.second > -30 && q.second < H + 30; };
        const Color gold = hexColor(marks::kProbeRing);
        for (int i = 0; i < farCount(); i++) {
            FarItem f;
            if (!farItem(sys, i, f)) continue;
            const Selection s{Selection::Far, 0, i};
            const bool on = sel == s, probe = f.kind == "probe", tno = f.kind == "tno";
            if ((probe || tno) && !(set.zoom <= -1.5 || on)) continue;   // (0.85.0: Sedna and the others too, at 75 to 90 AU now)   // the Voyagers are far beyond the planets: shown once you zoom out (or pick one)
            if (probe) {                                  // its path since about Neptune (a straight line), and where it left the Sun's wind
                const Vec3 a = f.dir * (40 * kAU), h = f.dir * (f.probe->cross * kAU);
                const Q qa = proj(a.x, a.y, a.z), qb = proj(f.pos.x, f.pos.y, f.pos.z), hq = proj(h.x, h.y, h.z);
                LinePen pen{R, W, H, 1.0, gold.withAlpha(0.5f)};
                pen.cam = &cam;
                pen.to(qa); pen.to(qb);
                if (inView(hq)) {
                    R.ring(hq.first, hq.second, 3.5, 1, gold.withAlpha(0.9f));
                    if (f.probe->cross * kAU * px > 40)
                        labels.push_back({f.name + " left the Sun’s wind here (" + std::to_string(f.probe->crossYear) + ")", hq.first, hq.second + 14, false, true, false});
                }
            }
            const Q q = proj(f.pos.x, f.pos.y, f.pos.z);   // 0.75.0: every far thing by the one camera (3D: its direction from the camera, with the sky's stars)
            if (!inView(q)) continue;
            const double seenD = farSeen(farOcc, q.first, q.second);   // 0.77.0: behind a ball (0) or a ring (its transmission)
            static const bool logFar = std::getenv("PL_LOG_FAR") != nullptr;   // (for automatic checks: each far marker's place and the share of it seen)
            if (logFar) std::printf("PLFAR %s %.2f %.2f %.6f\n", f.key.c_str(), q.first, q.second, seenD);
            if (seenD < 0.01) continue;                    // hidden: no marker, no name, no click
            const float seen = static_cast<float>(seenD);
            const auto dim = [seen](Color c) { return c.withAlpha(c.a * seen); };
            if (probe) { R.disc(q.first, q.second, 2.6, dim(Color{1, 1, 1, 1})); R.ring(q.first, q.second, 6, 1, dim(gold)); }
            else if (f.kind == "bh") { R.disc(q.first, q.second, 5.3, dim(Color{1, 1, 1, 1})); R.disc(q.first, q.second, 4.5, dim(Color{0, 0, 0, 1})); }
            else if (tno) { R.disc(q.first, q.second, 2.2, dim(hexColor(marks::kObject))); R.ring(q.first, q.second, 5, 1, dim(Color{1, 1, 1, 0.45f})); }   // 0.85.0
            else if (f.kind == "object") { R.ring(q.first, q.second, 6, 1, dim(hexColor(marks::kObject))); R.disc(q.first, q.second, 1.6, dim(hexColor(marks::kObject))); }
            else { const double bv = farBV(f.far->key); const std::array<int, 3> c = std::isnan(bv) ? std::array<int, 3>{255, 255, 255} : bvRGB(bv); R.disc(q.first, q.second, 2.8, dim(rgba(c[0], c[1], c[2]))); R.ring(q.first, q.second, 6, 1, dim(Color{1, 1, 1, 0.45f})); }
            if (on) hiRing(q.first, q.second, 11);
            if (f.kind == "bh") {
                labels.push_back({"Galactic centre · " + fmtLyS(f.ly) + " light years", q.first, q.second - 11, on, false, true, true});
                labels.push_back({f.name + " (black hole)", q.first, q.second + 19, on, false, true, true});
                const std::string mt = farMassText(f.key);                  // published mass (far_masses.h), under the name
                if (!mt.empty()) { labels.push_back({mt, q.first, q.second + 32, on, true, false}); massShown = true; }
            } else {
                labels.push_back({f.name + " · " + (probe || tno ? fmt::commas(f.au) + " AU" : fmtLyS(f.ly) + " light years"), q.first, q.second - 11, on, true, false});
                const std::string mt = probe || tno ? std::string() : farMassText(f.key);
                if (!mt.empty()) { labels.push_back({mt, q.first, q.second + 19, on, true, false}); massShown = true; }
            }
            hits_.push_back({s, q.first, q.second, 10});
        }
    }
    };


    {   // habitable zone: a faint green band
        // 0.23 stage 2: it fades out once the band is far wider than the picture (zoomed in on Earth the whole picture was green)
        const double bandPx = (hzOut - hzIn) * kAU * px, fade = std::clamp(1.0 - (bandPx / std::max(W, 1.0) - 1.5) / 3.0, 0.0, 1.0);
        const Color band = {marks::kHabitable[0] / 255.f, marks::kHabitable[1] / 255.f, marks::kHabitable[2] / 255.f, static_cast<float>(0.13 * fade)};
        const auto quad = [&](const Q& a, const Q& b, const Q& c, const Q& e) {   // 0.75.0: cut at the near plane (3D)
            const Q pq[4] = {a, b, c, e};
            clipPolygonNear(cam, pq, 4, [&](const Q& u, const Q& v, const Q& w) { R.triangle(u.first, u.second, band, v.first, v.second, band, w.first, w.second, band); });
        };
        if (d3 && bigR(W, H, hzOut * kAU * px)) {         // 0.75.0, 3D zoomed far in: the strip of the band under the look-at point, as a flat quad in space (its edges the
            // band's tangents there: over L they leave the arc by L^2 / (2 r), far under a point at a zoom where it is this big), cut at the near plane
            double lx0 = 0, ly0 = 0;                      // the look-at point (the camera's axis at the look-at plane), turned back
            { const double x1 = cam.lx, y2 = cam.ly, dep = cam.cd, y1 = y2 * cb + dep * sb; lx0 = x1 * ca + y1 * sa + bc.x; ly0 = -x1 * sa + y1 * ca + bc.y; }
            const double r0 = std::max(std::hypot(lx0, ly0), 1e-9), ux0 = lx0 / r0, uy0 = ly0 / r0, tx = -uy0, ty = ux0, L = std::max(60 * cam.d, 6000 / px);
            const double ri = hzIn * kAU, ro = hzOut * kAU;
            quad(proj(ux0 * ri - tx * L, uy0 * ri - ty * L, 0), proj(ux0 * ri + tx * L, uy0 * ri + ty * L, 0), proj(ux0 * ro + tx * L, uy0 * ro + ty * L, 0), proj(ux0 * ro - tx * L, uy0 * ro - ty * L, 0));
        } else if (bigR(W, H, hzOut * kAU * px)) {        // zoomed far in: draw just the strip of it that crosses the picture
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
                quad(a, b, c, e);
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
        const BeltCamera& bcam = cam;                     // 0.75.0: the same camera (orbit/camera.h)
        beltLayer_.draw(R, sys, bcam, sel.kind == Selection::Belt ? sel.moon : -1, kHL, set.gpuRocks);
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
        if (!(onCard && (tc.runs.empty() || R.lines(tc)))) {   // the CPU's lines (the GPU's are off, not ready or out of room)
            for (const Body* bp : vis) trailOf(*bp, false);
            if (set.orbits) for (const Body* bp : vis) if (!bp->comet) orbitOfBody(*bp, false);
        }
    }
    {                                                     // the comets' whole orbits, thin and dotted (0.90.0: on the card as dashed runs when its lines are on)
        render::LinesDraw cl;
        cl.cam = cardLines ? linesCamera(lcam) : render::LinesCamera{};
        bool onCard = cardLines;
        const auto cometOf = [&](const Body& b, bool card) {
            const bool on = sel.kind == Selection::Body && sel.bodyId == b.id;
            cometLocalPath(*b.comet, sys.julianDayTDB(), px, cpts);
            const std::vector<PathPart> parts{{1, static_cast<int>(cpts.size()) - 1, hexColor(b.color, on ? 0.85f : 0.35f)}};
            if (card) return addPathRuns(linesPts, cl, (std::uint64_t(5) << 56) | std::uint64_t(std::uint32_t(b.id)), cpts, parts, on ? 1.6 : 1.0, 2, 4);
            drawPathCpu(R, lcam, cpts, parts, on ? 1.6 : 1.0, 2, 4);
            return true;
        };
        for (const Body* bp : vis) if (bp->comet && onCard && !cometOf(*bp, true)) onCard = false;
        if (!(onCard && (cl.runs.empty() || R.lines(cl)))) for (const Body* bp : vis) if (bp->comet) cometOf(*bp, false);
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
    // 0.85.0 (R1): the true sunlight. The camera's exposure is set for the followed body or moon (its distance from the Sun); nothing followed: 1 au.
    // Each ball and ring system is drawn with lightAt(its place): (d_exposure / d)^2 (orbit/true_light.h); even lighting: 1.
    // (0.86.0: expAU and expName are set before the sky, which takes the same exposure)
    const auto lightAt = [&](const Vec3& p) { return lightFactor(std::max(length(p), 1.0) / kAU, expAU, set.trueLight); };
    exposureText.clear();
    if (set.trueLight) {
        const std::string other = expName == "Earth" ? "Neptune" : "Earth";
        double dOther = 0;
        for (const Body& ob : sys.bodies) if (ob.name == other) dOther = length(ob.pos) / kAU;
        exposureText = exposureReadout(expName, expAU, other, dOther);
    }
    double bodyLightK = 1;                                // the ball and rings being drawn (set in the bodies' loop)
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
    // the turning axis as a dashed line through the ball, "+" (0.75.2: was "N") at the end it turns counter-clockwise around (right-hand rule: for Venus and Uranus, which turn backwards, that end is the IAU's SOUTH pole)
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
            T.text("+", x + ux * L + ux / n * 9, y + uy * L + uy / n * 9 + 4, 11, Color{1, 1, 1, 1});
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
    // 0.75.0: pq, the planet's place (3D: the rings are put on the screen through the pinhole: ring_draw.h RingFrame zr, wn), r its rings' radius unit on the screen
    const auto drawRings = [&](const RingSet& rs, const SpinCtx* c, const Q& pq, double r, const Light3& sun, double sunElev, bool back) {
        const double x = pq.first, y = pq.second;
        if (rs.bands.empty() || r < 1) return;
        // the pole on the screen (right, up, towards you); without a spin model, a fixed tilt as before
        const double px0 = c ? c->px : std::sin(-0.22) * 0.94, py0 = c ? c->py : std::cos(-0.22) * 0.94, pz0 = c ? c->pz : 0.34;
        const double mu0 = std::max(std::fabs(sunElev), 1e-3);
        // 0.46.7: the real Sun (right, up, towards you), as the ball; 0.27: the planet's real shape (orbit/shapes.h): in its own frame (K) with the pole axis
        // stretched by eq / polar radius the body is a ball of radius 1. 0.64.0: the geometry and the triangles in orbit/ring_draw.h (shared with the card's check)
        RingFrame fr = ringFrame(x, y, r, px0, py0, pz0, c ? c->K : nullptr, c ? polarRatio(rs.name) : 1.0, sun.x, -sun.y, sun.z);
        // 0.75.0: the pinhole (3D): a ring point q is at depth Zc (1 - zr qz), zr = (the radius unit, m) / Zc = r / focal; the near plane over Zc. The rings' light and
        // opacity are each part's own slant since 0.75.1 (Saturn) and 0.75.2 (the other giants: ringMu per triangle corner).
        if (d3) { fr.ppx = cam.ppx; fr.ppy = cam.ppy; fr.zr = r / cam.F; fr.wn = cam.zNear / pq.Z; }
        // 0.75.1: Saturn's rings (from the profile) take each point's own slant ringMu(fr, q) and the face the camera sees (in 3D the camera's side of the plane; the map: pz);
        // 0.75.2: so do the other ring systems (their bands carry tau and albedo: ring_draw.h RingBand.tauBand)
        fr.mu0 = mu0; fr.sunlit = ringCameraOnPoleSide(fr) == (sunElev >= 0);
        if (rs.profile && back) {                           // (the checks, PL_RINGS_LOG=1: this frame's ring plane, camera and Sun, for the ui check's own pinhole arithmetic)
            static const bool logRings = std::getenv("PL_RINGS_LOG") != nullptr;
            if (logRings) {
                std::printf("PLRINGFRAME %.6f %.6f %.6f | %.9f %.9f %.9f | %.9f %.9f %.9f | %.6f %.6f %.9f %.9f | %.9f %.9f %.9f | %.9f %d | %.9f %.9f %.9f %.9f %.9f %.9f %.9f %.9f %.9f | %.9f | %.9f %.9f %.9f\n",
                            fr.x, fr.y, fr.r, fr.e1[0], fr.e1[1], fr.e1[2], fr.e2[0], fr.e2[1], fr.e2[2], fr.ppx, fr.ppy, fr.zr, fr.wn, fr.pole[0], fr.pole[1], fr.pole[2], fr.mu0, fr.sunlit ? 1 : 0,
                            fr.K[0], fr.K[1], fr.K[2], fr.K[3], fr.K[4], fr.K[5], fr.K[6], fr.K[7], fr.K[8], fr.sz, fr.Lb[0], fr.Lb[1], fr.Lb[2]);
                std::fflush(stdout);
            }
        }
        // 0.51.2: Saturn from Cassini's profile (UVIS, beta Cen 2008; orbit/saturn_profile.h): bins of at least a pixel (and at most 700 bins over the whole
        // profile, for speed), each with the area-averaged blocked share and light of the 10 km measurements in it; neighbours that look the same are merged
        // (0.64.0: orbit/ring_check.cpp saturnDrawnBands, kept while its inputs stay the same).
        static std::vector<RingBand> drawnOther;
        const std::vector<RingBand>* drawnP = &drawnOther;
        if (rs.profile) drawnP = &saturnDrawnBands(rs.eqKm, r);
        else {
            drawnOther = giantDrawnBands(rs.bands, rs.eqKm, r);   // 0.75.2: orbit/ring_bands.h
        }
        const std::vector<RingBand>& drawn = *drawnP;
        if (back) farOcc.rings.push_back({fr, drawn});   // 0.77.0: what dims a far marker behind the rings
        // 0.64.0: Saturn's rings on the graphics card (Vulkan; render/shaders.h kRingsFS: the same chords, bands and shadow per pixel) once its start-up check passed
        if (rs.profile && drawRingsOnCard(R, fr, drawn, back, W, H, bodyLightK)) return;
        for (const RingBand& b : drawn) {
            // 0.50 (review item 7): ONE drawn tint (0.91, 0.83, 0.66) for every ring system (Saturn's, Jupiter's, Uranus's, Neptune's): not measured. The Cassini VIMS
            // papers found (Filacchione et al. 2012, arXiv:1203.6230; Hedman et al. 2013, arXiv:1210.4727) only plot the spectra; no table to compute a colour from.
            // Learn says so; ROADMAP: source needed.
            // 0.50 (review item 3): the ring's light k multiplies the tint in LINEAR light (the tint decoded with IEC 61966-2-1), then the result is encoded (render/shading.h)
            const double kk = b.k * bodyLightK;                // 0.85.0: times the sunlight factor
            const Color litC{static_cast<float>(render::shadeEncoded(kRingTint[0], kk)), static_cast<float>(render::shadeEncoded(kRingTint[1], kk)), static_cast<float>(render::shadeEncoded(kRingTint[2], kk)), b.alpha};
            const Color darkC{0, 0, 0, b.alpha};
            if (b.i0 >= 0 || b.tauBand) {   // 0.75.1: a profile band (0.75.2: and a band of the other giants' rings) on the processor's path: its opacity and light worked out at each corner's own slant, blended across the triangle
                ringTriangles(fr, b, back, W, H, [&](double x0, double y0, double x1, double y1, double x2, double y2, bool lit, const double* q0, const double* q1, const double* q2) {
                    const double* qs[3] = {q0, q1, q2};
                    Color cc[3];
                    bool faint = true;
                    for (int c = 0; c < 3; c++) {
                        const RingValue v = ringBandValue(fr, b, ringMu(fr, qs[c]));
                        faint = faint && v.faint;
                        const double vk = v.k * bodyLightK;
                        cc[c] = lit ? Color{static_cast<float>(render::shadeEncoded(kRingTint[0], vk)), static_cast<float>(render::shadeEncoded(kRingTint[1], vk)), static_cast<float>(render::shadeEncoded(kRingTint[2], vk)), v.alpha}
                                    : Color{0, 0, 0, v.alpha};
                    }
                    if (!faint) R.triangle(x0, y0, cc[0], x1, y1, cc[1], x2, y2, cc[2]);
                });
                continue;
            }
            ringTriangles(fr, b, back, W, H, [&](double x0, double y0, double x1, double y1, double x2, double y2, bool lit, const double*, const double*, const double*) {
                const Color& cc = lit ? litC : darkC;
                R.triangle(x0, y0, cc, x1, y1, cc, x2, y2, cc);
            });
        }
    };

    // ---- the Sun ----
    // 0.75.0, 3D: the Sun's outline from the camera (True scale; orbit/camera.h), its drawn size times d / Z (Easy to see); behind the camera: none (0, off the picture)
    const double sr = d3 && !(star.f > 0) ? 0.0
                    : TRUE_SCALE ? (d3 ? std::min(sphereScreenRadius(cam, star, kSunRadius), 8 * std::hypot(W, H)) : kSunRadius * px) : std::max(kCrashRadius * px, 3.5) * (d3 ? star.f : 1.0);
    const bool sunTiny = TRUE_SCALE && sr < 2.5;
    const double srd = TRUE_SCALE ? std::max(sr, 5.0) : sr;
    SpinCtx sunSpin;
    const bool hasSunSpin = spinCtx("Sun", sunSpin);
    if (hasSunSpin) spinAxis("Sun", sunSpin, star.first, star.second, sr, true);
    // 0.49.4: no glow round the Sun. The real white-light corona is a millionth to a billionth of the disc's brightness (NASA, data/sun/README.txt): it cannot show
    // next to the disc. Until 0.49.3 a yellow glow out to 3 solar radii was drawn here (no source, no label).
    // 0.85.0 (outside review, PLAN_REVIEW.md R2, R3): the Sun as a ball, its edge by Neckel & Labs 1994 (render/sun_limb.h), and the measured sunspot groups of the
    // shown date as outlines (orbit/sunspots.h), fixed at their Carrington longitudes on the Sun's IAU frame. Until 0.84.1 the edge was the lab's own blend and
    // 16 invented patches marked the spin.
    // 0.94.0: the surface (render/sun_surface.h): the groups filled (umbra and penumbra), the granulation, and NASA SDO/HMI's photo of the day when the Sun is
    // close up (orbit/sun_photo.h: loaded on a worker, the drawn surface meanwhile). Until 0.93.0 the groups were outlines on a smooth ball.
    {
        std::vector<float> spotData, spotB;
        render::Renderer2D::SunSurface surf;
        const double jd = sys.julianDay();
        if (hasSunSpin && sr > 9) {
            const SpotDay sd = sunspotsAt(jd);
            for (const SpotGroup& g : sd.groups) {
                if (static_cast<int>(spotData.size()) >= 4 * render::Renderer2D::kMaxSunSpots) break;
                double v[3], rho;
                spotGeometry(g, v, rho);
                const render::SunSpotDraw s = render::spotDraw(v, rho, g.area);
                spotData.insert(spotData.end(), {static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2]), static_cast<float>(rho)});
                spotB.insert(spotB.end(), {static_cast<float>(s.rhoU), static_cast<float>(s.pen)});
            }
            surf.spotsB = spotB.data();
            surf.gran = true;
            surf.cellsPerRad = render::kSunRadiusMm / render::kGranCellMm;
            surf.gridPerRad = render::kSunRadiusMm / render::kGranGridMm;
            const render::GranTime gt = render::granTime(jd);
            surf.granK = gt.k; surf.granF = gt.f;
        }
        static const bool photoDirSet = [] {
            char* dir = SDL_GetPrefPath("PhysicsLab", "PhysicsLab");
            SunPhotoSource::get().setCacheDir(dir ? std::string(dir) + "sun_photos" : std::string());
            if (dir) SDL_free(dir);
            return true;
        }();
        (void)photoDirSet;
        SunPhotoSource& ps = SunPhotoSource::get();
        ps.want(jd, hasSunSpin && sr >= kSunPhotoMinPx);
        if (const SunPhoto* p = hasSunSpin && sr >= kSunPhotoMinPx ? ps.shown() : nullptr) {
            surf.photoVersion = ps.version();
            surf.photoGrey = &p->grey;
            surf.photoW = p->w; surf.photoH = p->h;
            for (int i = 0; i < 3; i++) { surf.photoO[i] = static_cast<float>(p->O[i]); surf.photoWest[i] = static_cast<float>(p->W[i]); surf.photoNorth[i] = static_cast<float>(p->N[i]); }
            surf.photoGeo[0] = static_cast<float>(p->cx); surf.photoGeo[1] = static_cast<float>(p->cy); surf.photoGeo[2] = static_cast<float>(p->rpx);
            for (int i = 0; i < 6; i++) surf.photoFit[i] = static_cast<float>(p->fit[i] / 255.0);
            surf.photoLog = ps.cacheDir() + "/" + photoDayKey(jd);
        }
        setSunSurfaceCardLines(ps.cardLines(jd));
        R.sunBall(star.first, star.second, sr, hasSunSpin ? sunSpin.K : nullptr, spotData.data(), static_cast<int>(spotData.size() / 4), &surf);
    }
    if (hasSunSpin) spinAxis("Sun", sunSpin, star.first, star.second, sr, false);
    if (sunTiny) R.ring(star.first, star.second, 7, 1.6, sunCol.withAlpha(0.9f));   // smaller than a pixel: a ring shows where it is
    if (sel.kind == Selection::Star) hiRing(star.first, star.second, srd + (sunTiny ? 8 : 6));
    hits_.push_back({{Selection::Star}, star.first, star.second, std::max(srd, sunTiny ? 10.0 : 0.0)});
    if (srd > 1) { FarBall fb; fb.x = star.first; fb.y = star.second; fb.r = srd; farOcc.balls.push_back(fb); }   // 0.77.0: the Sun hides what is behind it

    bool baryShown = false; Q baryQ{};                    // 0.46.7: where the barycentre's crosshair is, so the Sun's label keeps off it
    {   // the barycentre: a crosshair in a ring (hidden while it would sit on the Sun's centre)
        const Q bq = proj(bc.x, bc.y, bc.z);
        const bool on = sel.kind == Selection::Bary;
        if ((!d3 || bq.f > 0) && (std::hypot(bq.first - star.first, bq.second - star.second) > 5 || on || (sys.realPlanets() && set.exaggerateWobble))) {
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

    // 0.92.0: the Lagrange points of a pair and a transfer orbit, from the Data panel (orbit/lagrange.h, orbit/lambert.h). The pair's two bodies are where the picture draws them, so for a moon
    // (whose orbit the picture draws wider than it is) the points are those of the drawn pair; the Data panel's table gives the real distances.
    if (set.overlay.lagrange && !set.overlay.light.empty()) {
        const Overlay& O = set.overlay;
        const std::string pn = O.heavy == "Sun" ? O.light : O.heavy;
        const Body* pb = nullptr;
        for (const Body& b : sys.bodies) if (b.name == pn) pb = &b;
        Vec3 heavy{0, 0, 0}, light{0, 0, 0}, nrm{0, 0, 1};
        bool ok = false;
        if (pb && O.heavy == "Sun") { light = pb->pos; nrm = cross(pb->pos, pb->vel); ok = true; }
        else if (pb) {
            for (std::size_t j = 0; j < pb->moons.size(); j++)
                if (O.light == pb->moons[j].data->name) {
                    const MoonPlace pl = moonAt(*pb, j, 1.0);
                    heavy = pb->pos; light = pl.pos;
                    nrm = pl.realP ? cross(pl.rh, pl.ph) : Vec3{0, -pl.st, pl.ct} * pl.sgn;
                    ok = true;
                }
        }
        const double sep = ok ? length(light - heavy) : 0, nn = length(nrm);
        if (ok && sep > 0 && nn > 0) {
            const Vec3 e = (light - heavy) * (1 / sep), perp = cross(nrm * (1 / nn), e);
            const lagrange::Points P = lagrange::points(O.mu);
            for (int k = 0; k < 5; k++) {
                const Vec3 w = heavy + e * ((P.x[k] + O.mu) * sep) + perp * (P.y[k] * sep);
                const Q q = proj(w.x, w.y, w.z);
                if (d3 && q.f <= 0) continue;
                const Color c = k >= 3 ? Color{0.45f, 0.85f, 0.55f, 1} : Color{1.0f, 0.78f, 0.35f, 1};
                R.disc(q.first, q.second, 3.6, Color{0, 0, 0, 0.6f});
                R.ring(q.first, q.second, 5.5, 1.6, c);
                R.disc(q.first, q.second, 2.0, c);
                labels.push_back({"L" + std::to_string(k + 1), q.first + 11, q.second - 9, false, true, true, false, 1});
            }
        }
    }
    if (set.overlay.transfer && set.overlay.arc.size() > 1) {
        const Overlay& O = set.overlay;
        const Color c{0.35f, 0.8f, 1.0f, 1};
        Q prev = proj(O.arc[0].x, O.arc[0].y, O.arc[0].z);
        for (std::size_t i = 1; i < O.arc.size(); i++) {
            const Q q = proj(O.arc[i].x, O.arc[i].y, O.arc[i].z);
            if (!d3 || (q.f > 0 && prev.f > 0)) { R.line(prev.first, prev.second, q.first, q.second, 3.4, Color{0, 0, 0, 0.5f}); R.line(prev.first, prev.second, q.first, q.second, 1.8, c); }
            prev = q;
        }
        for (int e = 0; e < 2; e++) {
            const Vec3& w = e ? O.arrPos : O.depPos;
            const Q q = proj(w.x, w.y, w.z);
            if (d3 && q.f <= 0) continue;
            R.ring(q.first, q.second, 6, 1.8, c);
            labels.push_back({e ? "Arrival" : "Departure", q.first + 9, q.second + 14, false, true, true, false, 1});
        }
    }

    if (sys.realPlanets()) {   // Real: the Sun's own path round the barycentre (it circles it, mostly because of Jupiter), kSunWobbleScale times too big when "Exaggerate" is on
        const double k = set.exaggerateWobble ? kSunWobbleScale : 1;
        const auto& tr = sys.sunTrail;
        const int n = static_cast<int>(tr.size());
        // fades in 4 steps, oldest faintest, like the planets' trails (0.90.0: on the card when its lines are on)
        static std::vector<Vec3> sunPts;
        sunPts.resize(std::size_t(n));
        for (int i = 0; i < n; i++) sunPts[std::size_t(i)] = Vec3{bc.x + k * tr[i].x, bc.y + k * tr[i].y, bc.z + k * tr[i].z};
        std::vector<PathPart> parts;
        for (int part = 0; part < 4 && n > 1; part++) {   // (part i0 .. i1 - 1: its pieces i0 + 1 .. i1 - 1)
            const int i0 = part * n / 4, i1 = std::min(n, (part + 1) * n / 4 + 1);
            parts.push_back({i0 + 1, i1 - 1, sunCol.withAlpha(0.15f + 0.2f * part)});
        }
        render::LinesDraw sl;
        sl.cam = cardLines ? linesCamera(lcam) : render::LinesCamera{};
        if (!(cardLines && addPathRuns(linesPts, sl, std::uint64_t(6) << 56, sunPts, parts, 1.5, 0, 0) && (sl.runs.empty() || R.lines(sl)))) drawPathCpu(R, lcam, sunPts, parts, 1.5, 0, 0);
        if (set.exaggerateWobble) {                           // the Sun's place on the magnified path: a ring (the Sun itself is drawn at its true place)
            const Q sq = proj(bc.x * (1 - k), bc.y * (1 - k), bc.z * (1 - k));   // the Sun is at -bc from the barycentre: bc + k * (-bc)
            R.ring(sq.first, sq.second, 5, 1.6, sunCol.withAlpha(0.9f));
            labels.push_back({"Sun's path ×" + std::to_string(static_cast<int>(kSunWobbleScale)) + ": exaggerated, not to scale", sq.first, sq.second + 16, false, true, false});
        }
    }

    // 0.51.2: eclipse shadows. What can hide the Sun from a point of a ball, at the REAL places (the moons are drawn much farther out than they are, so a moon's shadow
    // on its planet is where it really falls, not under the drawn moon): Earth's Moon (ELP2000-82B) on Earth, Earth on the Moon, the 13 big moons (orbit/moon_sats.h)
    // on their planet (the 4 nearest the line to the Sun) and their planet on them. Every shadow-caster is a ball of its equatorial radius (Jupiter's and Saturn's
    // flattening is left out of their shadows); the Sun a disc of 695,700 km with its measured edge darkening per channel (0.85.0: render/sun_limb.h, Neckel & Labs 1994). Earth also gets its air (render/shading.h earthAirLight).
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
    if (set.comets) tailLayer_.prepare(R, sys, set.gpuRocks);   // 0.78.0: every comet's grains brought to the time now (orbit/tail_layer.h)
    std::vector<const Body*> order = vis;
    std::stable_sort(order.begin(), order.end(), [&](const Body* a, const Body* b) { return cam.depth(a->pos.x, a->pos.y, a->pos.z) < cam.depth(b->pos.x, b->pos.y, b->pos.z); });   // far ones first
    for (const Body* bp : order) {
        const Body& b = *bp;
        const Q q = proj(b.pos.x, b.pos.y, b.pos.z);
        static const bool logBehind = std::getenv("PL_LOG_BODYPX") != nullptr;
        if (d3 && !(q.f > 0)) {                           // 0.75.0: behind the camera (or nearer than the near plane): not drawn
            if (logBehind) std::printf("PLBODYPX %s -1e9 -1e9 0 0 0 0 %.6g %.6g %.6g\n", b.name.c_str(), q.X, q.Y, q.Z);   // (for automatic checks: its camera-space place)
            continue;
        }
        // in 3D, nearer things look bigger: 0.75.0, True scale: a ball's outline, focal tan(asin(R / L)) (orbit/camera.h); Easy to see: its drawn size times d / Z
        const double rad = d3 && TRUE_SCALE ? sphereScreenRadius(cam, q, b.radius) : drawnRadius(b) * (d3 ? q.f : 1);
        if (rad > 8 * std::hypot(W, H)) continue;         // (the camera is at or inside it: it would cover the whole picture many times over; left out)
        const bool tiny = TRUE_SCALE && rad < 2.5;
        const double rd = tiny ? 5 : rad;
        const bool on = sel.kind == Selection::Body && sel.bodyId == b.id;
        if (b.comet) {   // a glowing head; 0.78.0: the tails calculated (orbit/comet_tail.h, orbit/tail_layer.h). The dust: grains on Kepler orbits around a Sun of
            // GM (1 - beta) GM (Finson & Probstein 1968), placed by the GPU (kGrainsCS) or the CPU. The gas tail: along the solar wind as the comet meets it
            // (400 km/s, NASA MSFC), drawn as long as the dust tail (its length is not calculated). The head's glow, the 4 AU switch-on and each grain's
            // brightness are drawing choices; the legend and the card say so.
            const double r = length(b.pos), rA = r / kAU, act = std::clamp((4 - rA) / 3.2, 0.0, 1.0);
            tailLayer_.drawDust(R, b, cam, set.gpuRocks, rgba(marks::kDustTail[0], marks::kDustTail[1], marks::kDustTail[2]).withAlpha(0.3f), 1.4);
            Vec3 gd;
            double L = 0;
            if (tailLayer_.gasTail(b, gd, L)) {
                const Q e = proj(b.pos.x + gd.x * L, b.pos.y + gd.y * L, b.pos.z + gd.z * L);
                const double ex = e.first - q.first, ey = e.second - q.second, ln = std::hypot(ex, ey);
                if (ln >= 3 && (!d3 || e.f > 0)) {
                    const double nx = -ey / ln, ny = ex / ln, w2 = std::min(10.0, ln * 0.04 + 1.5);
                    const Color col = rgba(marks::kIonTail[0], marks::kIonTail[1], marks::kIonTail[2]);
                    const Color c0 = col.withAlpha(0.8f), c1 = col.withAlpha(0);
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
            pen.cam = &cam;
            pen.to(q); pen.to(g);
            if (g.f > 0) R.disc(g.first, g.second, 2, hexColor(b.color));
        }
        const Light3 sun = sunLight(b.pos);                 // 0.46.7: the real Sun
        bodyLightK = lightAt(b.pos);                        // 0.85.0 (R1)
        static const bool logPx = std::getenv("PL_LOG_BODYPX") != nullptr;   // 0.50, for automatic checks only: where each body is drawn (x, y, radius in pixels) and the light's direction
        if (logPx) std::printf("PLBODYPX %s %.2f %.2f %.3f %.4f %.4f %.4f %.6g %.6g %.6g\n", b.name.c_str(), q.first, q.second, rad, sun.x, sun.y, sun.z, q.X, q.Y, q.Z);   // (0.75.0: + its camera-space place, m)
        // (0.75.0: with the rings' reach, 2.4 radii; a camera close by, within 3 radii in 3D: always, its rings may pass beside it)
        const double reach = std::max(rad, 2.4 * (d3 && TRUE_SCALE ? cam.F * b.radius / q.Z : rad));
        if ((d3 && TRUE_SCALE && q.Z < 3 * b.radius) || (q.first + reach > -12 && q.first - reach < W + 12 && q.second + reach > -12 && q.second - reach < H + 12)) {
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
            // 0.75.0: the rings' radius unit on the screen: at true scale in 3D focal R / Z (the ball's outline, rad, is focal tan(asin(R / L)): larger close up)
            const double ringR = d3 && TRUE_SCALE ? cam.F * b.radius / q.Z : rad;
            drawRings(ringsHere, spins ? &sc : nullptr, q, ringR, sun, sunElev, true);
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
                    render::Renderer2D::PlanetLight pl = planetLight(b);
                    pl.k = static_cast<float>(bodyLightK);
                    R.planet(q.first, q.second, rad, ball, sun.x, sun.y, sun.z, sc.K, render::mapForBody(b.name), nb ? rb : nullptr, nb, 1.0, polarRatio(b.name.c_str()), false, &pl);   // 0.27: its real shape (orbit/shapes.h)
                    { FarBall fb; fb.x = q.first; fb.y = q.second; fb.r = rad; fb.hasK = true; std::copy(sc.K, sc.K + 9, fb.K); fb.sz = polarRatio(b.name.c_str()); farOcc.balls.push_back(fb); }   // 0.77.0
                }
                spinAxis(b.name, sc, q.first, q.second, rad, false);
            } else {
                R.sphere(q.first, q.second, rad, ball, sun.x, sun.y, sun.z, bodyLightK);   // lit from the Sun
                if (rad > 1) { FarBall fb; fb.x = q.first; fb.y = q.second; fb.r = rad; farOcc.balls.push_back(fb); }   // 0.77.0: a ball drawn without its spin (a circle)
            }
            drawRings(ringsHere, spins ? &sc : nullptr, q, ringR, sun, sunElev, false);
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
    static const bool logPxM = std::getenv("PL_LOG_BODYPX") != nullptr;   // 0.75.0, for automatic checks only: each moon's place, radius and camera-space place
    if (set.moons) for (const Body* bp : order) {
        const Body& b = *bp;
        if (b.moons.empty()) continue;
        const Q pc = proj(b.pos.x, b.pos.y, b.pos.z);
        const double kf = moonKf(b, pc.f), distPx = length(b.pos) * px;   // true distance from the Sun: the same from every viewing angle
        std::vector<double> disp;
        double maxDisp = 0;
        for (std::size_t j = 0; j < b.moons.size(); j++) { disp.push_back(moonDisp(b, j, kf)); maxDisp = std::max(maxDisp, disp.back()); }
        const bool here = (sel.kind == Selection::Body || sel.kind == Selection::Moon) && sel.bodyId == b.id;
        if (!here && maxDisp > 0.15 * distPx) continue;
        // 0.90.0: the moons' orbits as ellipses (orbit/orbit_lines.h LineEllipse: the circle about the planet, seen tilted, drawn finely enough where it reaches
        // the picture as the planets' orbits are), all of this planet's in one draw on the card before its moons, else each by the processor before its moon
        const auto moonEllipse = [&](const MoonPlace& pl) {
            LineEllipse el;
            el.F = b.pos;
            if (pl.realP) { el.P = pl.rh; el.Q = pl.ph; el.a = el.b = pl.dW2; }
            else { el.P = Vec3{1, 0, 0}; el.Q = Vec3{0, pl.ct, pl.st}; el.a = el.b = pl.dW; }
            el.e = 0; el.dashed = false;
            return el;
        };
        const auto moonPass = [&](std::size_t j, int pass, double& width) {
            const bool on = sel.kind == Selection::Moon && sel.bodyId == b.id && sel.moon == static_cast<int>(j);
            width = pass ? 3.0 : 1.0;
            return pass ? (on ? kHL : hexColor(b.color)).withAlpha(0.55f) : hexColor(b.color, on ? 0.55f : 0.28f);
        };
        bool moonsCard = false;
        if (cardLines) {
            render::LinesDraw ml;
            ml.cam = linesCamera(lcam);
            bool ok = true;
            for (std::size_t j = 0; j < b.moons.size() && ok; j++) {
                const MoonPlace pl = moonAt(b, j, kf);
                const LineEllipse el = moonEllipse(pl);
                for (int pass = 0; pass < (pl.fast ? 2 : 1) && ok; pass++) {
                    double wd = 1;
                    const Color c = moonPass(j, pass, wd);
                    ok = addEllipseRuns(linesPts, ml, (std::uint64_t(4) << 56) | (std::uint64_t(std::uint32_t(b.id)) << 16) | std::uint64_t(j), el, wd, c, cpts);
                }
            }
            moonsCard = ok && (ml.runs.empty() || R.lines(ml));
        }   // hidden while it would crowd the map: zoom in or click the planet (0.22 stage 6: 0.15, was 0.3: at the whole-system zoom Saturn's and Uranus's widened moon orbits drew rings a third of the way to the Sun)
        for (std::size_t j = 0; j < b.moons.size(); j++) {
            const MoonData& m = *b.moons[j].data;
            const MoonPlace pl0 = moonAt(b, j, kf);        // the same place the follow block centres on
            const double aM = pl0.aM, dW = pl0.dW, ct = pl0.ct, st = pl0.st, xr = pl0.xr, yr = pl0.yr, sgn = pl0.sgn;   // (0.90.0: dW2, realP and ph only in moonEllipse)
            const bool fast = pl0.fast, realM = pl0.realM, realS = pl0.realS;
            const Vec3 rh = pl0.rh, sp = pl0.sp;
            const bool on = sel.kind == Selection::Moon && sel.bodyId == b.id && sel.moon == static_cast<int>(j);
            if (!moonsCard) {   // the orbit (seen from above, the tilted circle looks like an ellipse), by the processor
                const LineEllipse el = moonEllipse(pl0);
                for (int pass = 0; pass < (fast ? 2 : 1); pass++) {
                    double wd = 1;
                    const Color c = moonPass(j, pass, wd);
                    drawEllipseCpu(R, lcam, el, wd, c, cpts);
                }
            }
            const Q mp = proj(pl0.pos.x, pl0.pos.y, pl0.pos.z);
            if (d3 && !(mp.f > 0)) continue;              // 0.75.0: behind the camera: not drawn
            // 0.75.0: True scale in 3D: the ball's outline from the camera (orbit/camera.h sphereScreenRadius); until 0.74.0 its radius times px, the same at any depth
            const double mr = TRUE_SCALE ? (d3 ? sphereScreenRadius(cam, mp, m.radiusKm * kKm) : m.radiusKm * kKm * px) : 1.6 + 0.9 * std::max(0.0, std::log10(m.radiusKm / 50));
            if (mr > 8 * std::hypot(W, H)) continue;      // (the camera is at or inside it)
            if (logPxM) std::printf("PLMOONPX %s %.3f %.3f %.4f %.6g %.6g %.6g %.6g %.6g %.6g %.6g\n", m.name, mp.first, mp.second, mr, mp.X, mp.Y, mp.Z, m.radiusKm * kKm, cam.F, cam.ppx, cam.ppy);
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
                    pl.k = static_cast<float>(lightAt(b.pos));       // 0.85.0: the planet's distance (the moon's own differs by under 0.3 %)
                    if (realM && sys.realMoon()) addOcc(pl, scr(sys.moonGeocentric() * -1.0, m.radiusKm * kKm), b.radius / (m.radiusKm * kKm));
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
                    pl.k = static_cast<float>(lightAt(b.pos));
                    addOcc(pl, scr(sp * (-kKm), m.radiusKm * kKm), b.radius / (m.radiusKm * kKm));
                    R.planet(mp.first, mp.second, mr, hexColor(marks::kMoon), sun.x, sun.y, sun.z, K, render::mapForBody(m.name), nullptr, 0, 1.0, 1.0, false, &pl);
                } else   // no map (Titan: its haze hides the surface in visible light; Enceladus: only an airbrushed map; Titania, Oberon, Deimos: none): a grey ball, the card says "colour not measured here"
                    R.sphere(mp.first, mp.second, mr, hexColor(marks::kMoon), sun.x, sun.y, sun.z, lightAt(b.pos));
                if (mtiny) { R.fadeDisc(mp.first, mp.second, 5.5, hexColor(marks::kMoon, 0.3f)); R.disc(mp.first, mp.second, 2.0, hexColor(marks::kMoon)); }   // 0.23 stage 2: a dot with a soft glow, as the tiny planets (was a faint grey ring)
                if (on) hiRing(mp.first, mp.second, md + 5);
            }
            if (on) drawnSel_ = {true, mp.first - (W / 2 + fx + cx), mp.second - (H / 2 + cy), pc.first - (W / 2 + fx + cx), pc.second - (H / 2 + cy)};
            exag_[{b.id, static_cast<int>(j)}] = dW / aM;
            hits_.push_back({{Selection::Moon, b.id, static_cast<int>(j)}, mp.first, mp.second, std::max(md, mtiny ? 9.0 : 0.0)});
            if (!mtiny && md > 1) { FarBall fb; fb.x = mp.first; fb.y = mp.second; fb.r = md; farOcc.balls.push_back(fb); }   // 0.77.0
            if (here) { labels.push_back({m.name, mp.first, mp.second - md - 4, on, true, false}); labels.back().prio = 4; }
        }
    }

    drawFar();   // 0.77.0: the far markers, after every ball and ring that can hide them

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
        // 0.75.0, 3D: the zoom's span holds at the look-at plane: the followed thing's distance (until 0.74.0 the near-flat view made it about true everywhere)
        std::string where;
        if (d3) {
            std::string who;
            if (follow && sel.kind == Selection::Star) who = "the Sun";
            else if (follow && sel.kind == Selection::Bary) who = "the barycentre";
            else if (follow && (sel.kind == Selection::Body || sel.kind == Selection::Moon)) {
                if (const Body* b = findBody(sys, sel.bodyId)) who = sel.kind == Selection::Moon && sel.moon >= 0 && sel.moon < static_cast<int>(b->moons.size()) ? b->moons[std::size_t(sel.moon)].data->name : b->name;
            } else if (follow && sel.kind == Selection::Far) { FarItem fi; if (farItem(sys, sel.moon, fi)) who = fi.name; }
            else if (cdep == 0) who = "the barycentre";
            where = who.empty() ? "at the picture's middle distance" : "at " + who + "’s distance";
        }
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
        scaleWhere = where;                                                    // 0.75.0 (3D): where the bar's length holds
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
            if (!where.empty()) T.text(where, x0, y + 28, 10, kMuted, 0, true, kCard);
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
