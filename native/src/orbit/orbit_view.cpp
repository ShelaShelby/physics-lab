#include "orbit_view.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>

#include "format.h"
#include "imgui.h"
#include "kepler.h"
#include "sky.h"
#include "spin.h"
#include "units.h"

namespace orbit {

using namespace units;
using render::Color;
using render::hexColor;
using render::rgba;

namespace {

// Colours of the dark picture (the web version with the night sky on)
// The picture's colours. With the night sky on, the picture is dark whatever the theme (as in the web
// version); with Stars Off they follow the theme (the web's light and dark page colours). Set each frame.
Color kBg, kFg, kMuted, kCard, kLine;
Color fromRgb(const app::Rgb& c) { return {c.r, c.g, c.b, 1}; }
void setPictureColours(bool sky, const app::ThemeColors& t) {
    if (sky) { kBg = rgba(4, 6, 13); kFg = rgba(230, 235, 245); kMuted = rgba(154, 166, 189); kCard = rgba(11, 15, 26); kLine = {1, 1, 1, 0.22f}; }
    else { kBg = fromRgb(t.bg); kFg = fromRgb(t.fg); kMuted = fromRgb(t.muted); kCard = fromRgb(t.card); kLine = fromRgb(t.line); }   // the theme's page colours
}
bool g_skyHalo = true;
double g_textScale = 1;   // Settings → Text size   // the thin dark edge round labels is only needed on top of the stars
const Color kHL = hexColor("#ff2bd6");   // the "look here" pink: nothing else in the picture is this colour

// The Sun's colour from its surface temperature (5,772 K), as in the web's starColor()
Color starColor(double T) {
    static const double st[][4] = {{2500, 217, 72, 43}, {3800, 238, 138, 46}, {5000, 245, 185, 66}, {5800, 247, 208, 72}, {7500, 240, 230, 170}, {10000, 200, 220, 255}};
    if (T <= st[0][0]) return rgba(217, 72, 43);
    for (int i = 1; i < 6; i++)
        if (T <= st[i][0]) {
            const double f = (T - st[i - 1][0]) / (st[i][0] - st[i - 1][0]);
            int c[3];
            for (int k = 0; k < 3; k++) c[k] = static_cast<int>(std::lround(st[i - 1][k + 1] + f * (st[i][k + 1] - st[i - 1][k + 1])));
            return rgba(c[0], c[1], c[2]);
        }
    return rgba(200, 220, 255);
}

// Liang–Barsky: the part of a segment inside a rectangle. Zoomed far in, pieces of a
// trail or an orbit are millions of points long, so only the visible part is drawn.
bool clipSeg(double x0, double y0, double x1, double y1, double xa, double ya, double xb, double yb, double out[4]) {
    const double dx = x1 - x0, dy = y1 - y0;
    if (x0 >= xa && x0 <= xb && x1 >= xa && x1 <= xb && y0 >= ya && y0 <= yb && y1 >= ya && y1 <= yb) {
        out[0] = x0; out[1] = y0; out[2] = x1; out[3] = y1;
        return true;
    }
    double t0 = 0, t1 = 1;
    for (int k = 0; k < 4; k++) {
        const double p = k == 0 ? -dx : k == 1 ? dx : k == 2 ? -dy : dy;
        const double q = k == 0 ? x0 - xa : k == 1 ? xb - x0 : k == 2 ? y0 - ya : yb - y0;
        if (p == 0) { if (q < 0) return false; }
        else {
            const double r = q / p;
            if (p < 0) { if (r > t1) return false; if (r > t0) t0 = r; }
            else { if (r < t0) return false; if (r < t1) t1 = r; }
        }
    }
    out[0] = x0 + t0 * dx; out[1] = y0 + t0 * dy; out[2] = x0 + t1 * dx; out[3] = y0 + t1 * dy;
    return true;
}

// Draws a polyline clipped to the picture (with a 20-point margin), optionally dashed.
struct LinePen {
    render::Renderer2D& r;
    double W, H, width;
    Color c;
    double dashOn = 0, dashOff = 0;   // dash lengths in points (0 = solid)
    bool havePrev = false;
    double px = 0, py = 0, dashPos = 0;

    void to(double x, double y) {
        if (havePrev) {
            double cl[4];
            if (clipSeg(px, py, x, y, -20, -20, W + 20, H + 20, cl)) {
                if (dashOn <= 0) r.line(cl[0], cl[1], cl[2], cl[3], width, c);
                else dashed(cl[0], cl[1], cl[2], cl[3]);
            }
        }
        px = x; py = y; havePrev = true;
    }
    void dashed(double x0, double y0, double x1, double y1) {
        const double len = std::hypot(x1 - x0, y1 - y0), period = dashOn + dashOff;
        double s = 0;
        while (s < len) {
            const double inPeriod = std::fmod(dashPos, period);
            const double step = std::min(len - s, inPeriod < dashOn ? dashOn - inPeriod : period - inPeriod);
            if (inPeriod < dashOn) r.line(x0 + (x1 - x0) * s / len, y0 + (y1 - y0) * s / len, x0 + (x1 - x0) * (s + step) / len, y0 + (y1 - y0) * (s + step) / len, width, c);
            s += step; dashPos += step;
        }
    }
};

// A circle far bigger than the picture: is the picture inside it, outside it, or which
// short stretch of angles (screen angles, y down) can be seen? As in the web's circWin().
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

// A point on screen: x, y (points), depth (m, > 0 towards the viewer) and perspective scale
struct Q { double first, second, dep, f; };

// Text drawn with Dear ImGui on top of the picture. Positions are in picture points with
// y at the text's baseline, like the canvas fillText() of the web version.
struct TextPainter {
    ImDrawList* dl;
    ImFont* font;
    double originX, originY, s;

    double width(const std::string& t, double size) const {
        return font->CalcTextSizeA(static_cast<float>(size * g_textScale * s), FLT_MAX, 0, t.c_str()).x / s;
    }
    // align: 0 left, 0.5 centre
    // haloColor with alpha < 0: the dark edge for text on the night sky (left out when the stars are off)
    void text(const std::string& t, double x, double y, double size, Color c, double align = 0.5, bool halo = true, Color haloColor = {4 / 255.f, 6 / 255.f, 13 / 255.f, -0.85f}) {
        if (haloColor.a < 0) { if (!g_skyHalo) halo = false; haloColor.a = -haloColor.a; }
        const float px = static_cast<float>(size * g_textScale * s);
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

// The night sky behind everything (the web's skyDraw). It is so far away that it does not move
// when you zoom or move the view, only when you turn the 3D view. Stars are grouped by colour and
// size, and each group is one instanced draw call. lv = how bright the sky is drawn (0–1).
void drawSky(render::Renderer2D& R, TextPainter& T, double W, double H, double ca, double sa, double cb, double sb, ConsMode cons, double lv) {
    const Sky& S = skyData();
    const double cx = W / 2, cy = H / 2, diag = std::hypot(W, H) / 2, K = diag / 1.75;   // K: points per radian away from the middle (100° reaches the corner)
    // a direction -> screen: the angle from straight ahead becomes the distance from the middle
    const auto scr = [&](const Vec3& v, double& X, double& Y) {
        const double x1 = v.x * ca - v.y * sa, y1 = v.x * sa + v.y * ca, y2 = y1 * cb - v.z * sb, dep = y1 * sb + v.z * cb;
        const double th = std::acos(std::clamp(-dep, -1.0, 1.0));   // the viewer is on the +depth side and looks the other way
        if (th > 1.75) return false;
        const double rho = std::max(std::hypot(x1, y2), 1e-9), r = th * K;
        X = cx + x1 / rho * r; Y = cy - y2 / rho * r;
        return true;
    };
    {   // the Milky Way: three soft bands along the plane of our galaxy, brightest towards its centre
        const Vec3 A = S.galCentre, P = S.galPole, B = cross(P, A);
        const double wds[3] = {0.46 * K, 0.27 * K, 0.12 * K}, als[3] = {0.03, 0.042, 0.055};
        for (int pass = 0; pass < 3; pass++) {
            double px = 0, py = 0; bool prev = false;
            for (int i = 0; i <= 120; i++) {
                const double l = i / 120.0 * 2 * kPi, c = std::cos(l), sn = std::sin(l);
                double X, Y;
                const bool ok = scr(A * c + B * sn, X, Y);
                if (prev && ok && std::hypot(X - px, Y - py) < 0.7 * diag)
                    R.line(px, py, X, Y, wds[pass], Color{178 / 255.f, 192 / 255.f, 1, static_cast<float>(als[pass] * lv * (0.4 + 0.6 * (0.5 + 0.5 * c)))});
                prev = ok; px = X; py = Y;
            }
        }
    }
    // stars: screen positions, then one draw per colour bucket and size class
    static std::vector<float> pos;
    static std::vector<float> groups[kSkyColourBuckets][3];
    pos.assign(S.stars.size() * 2, -1e9f);
    for (auto& g : groups) for (auto& c : g) c.clear();
    std::vector<int> glow;
    for (std::size_t i = 0; i < S.stars.size(); i++) {
        double X, Y;
        if (!scr(S.stars[i].dir, X, Y)) continue;
        pos[i * 2] = static_cast<float>(X); pos[i * 2 + 1] = static_cast<float>(Y);
        if (X < -4 || X > W + 4 || Y < -4 || Y > H + 4) continue;
        const float m = S.stars[i].mag;
        if (m < 2.2f) { glow.push_back(static_cast<int>(i)); continue; }   // the brightest: drawn below with a glow
        const int cl = m > 4.6f ? 0 : m > 3.4f ? 1 : 2;
        groups[S.stars[i].bucket][cl].push_back(static_cast<float>(X));
        groups[S.stars[i].bucket][cl].push_back(static_cast<float>(Y));
    }
    const double sizes[3] = {1.1, 1.5, 2.1}, alphas[3] = {0.55, 0.78, 0.95};
    const auto col = [&](int k, double a) { const auto& c = S.colours[k]; return rgba(c[0], c[1], c[2], static_cast<float>(a)); };
    for (int k = 0; k < kSkyColourBuckets; k++)
        for (int cl = 0; cl < 3; cl++) R.squares(groups[k][cl].data(), static_cast<int>(groups[k][cl].size() / 2), sizes[cl], col(k, alphas[cl] * lv));
    for (int i : glow) {                                  // a soft glow that grows with the star's brightness
        const double x = pos[i * 2], y = pos[i * 2 + 1], m = S.stars[i].mag, rr = (3.2 + (2.2 - m) * 2.4) * (0.55 + 0.45 * lv);
        const int k = S.stars[i].bucket;
        R.fadeDisc(x, y, rr * 2.6, col(k, 0.25 * lv));
        R.fadeDisc(x, y, rr * 1.0, col(k, 0.35 * lv));
        R.disc(x, y, std::max(1.3, rr * 0.42), col(k, std::min(1.0, 0.25 + lv)));
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
            for (const auto& c : S.cons) {
                double X, Y;
                if (!scr(c.second, X, Y) || X < 20 || X > W - 20 || Y < 20 || Y > H - 20) continue;
                bool clash = false;
                for (const auto& o : used) if (std::fabs(o.first - X) < 60 && std::fabs(o.second - Y) < 14) clash = true;
                if (clash) continue;
                used.push_back({X, Y});
                T.text(c.first, X, Y, 11, Color{150 / 255.f, 178 / 255.f, 240 / 255.f, static_cast<float>(0.78 * cl2)}, 0.5, false);
            }
            for (const auto& nm : S.names) {              // the brightest stars' names
                if (S.stars[nm.first].mag > 1.9f) continue;
                const double x = pos[nm.first * 2], y = pos[nm.first * 2 + 1];
                if (x < 4 || x > W - 50 || y < 12 || y > H - 4) continue;
                T.text(nm.second, x + 7, y + 3, 10, Color{232 / 255.f, 238 / 255.f, 1, static_cast<float>(0.72 * cl2)}, 0, false);
            }
        }
    }
}

}  // namespace

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
    if (rotate) { yaw += dx * 0.01; pitch = std::clamp(pitch + dy * 0.01, -1.5, 1.5); return; }   // 3D: drag turns the view
    ox += dx; oy += dy;
    follow = false;
}

void OrbitView::zoomChanged(double oldZoom, double newZoom) {   // zoom about the middle of the picture
    const double f = std::pow(10.0, newZoom - oldZoom);
    ox *= f; oy *= f;
}

void OrbitView::resetView() { ox = oy = 0; follow = false; yaw = 0.6; pitch = 1.0; cdep = 0; }

void OrbitView::select(Selection s, bool f) {
    sel = s; follow = f; cardHidden = false;
    ox = oy = 0; cdep = 0;
}

void OrbitView::draw(render::Renderer2D& R, ImDrawList* dl, double originX, double originY, double uiScale,
                     const SolarSystem& sys, const ViewSettings& set, double W, double H, double fps) {
    TextPainter T{dl, ImGui::GetFont(), originX, originY, uiScale};
    const double px = std::min(W, H) / 34 * std::pow(10.0, set.zoom) / kSimLength;   // points per metre
    const bool TRUE_SCALE = set.trueScale;
    const Vec3 bc = sys.barycentre();                     // everything is drawn around the barycentre, so the Sun is the one that wobbles

    // the selected thing may be gone (removed, or switched off)
    const auto visible = [&](const Body& b) { return (b.type != BodyKind::Dwarf || set.dwarfs) && (b.type != BodyKind::Comet || set.comets); };
    {
        bool ok = true;
        if (sel.kind == Selection::Body || sel.kind == Selection::Moon) {
            const Body* b = findBody(sys, sel.bodyId);
            ok = b && visible(*b) && (sel.kind != Selection::Moon || (set.moons && sel.moon < static_cast<int>(b->moons.size())));
        }
        if (sel.kind == Selection::Far) { FarItem f; ok = set.far && farItem(sys, sel.moon, f); }
        if (sel.kind == Selection::Belt) ok = set.belts && sel.moon >= 0 && sel.moon < static_cast<int>(sys.belts.size());
        if (!ok) { sel = Selection{}; follow = false; }
    }

    // ---- the camera (the web's base()): turn by yaw about z, tilt by pitch towards the viewer, then a mild perspective ----
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
        return Q{W / 2 + x1 * px * f, H / 2 - y2 * px * f, dep, f};
    };
    const auto flat = [&](double x, double y, double z) {  // far-away stars are seen flat: too far for this little scene's perspective
        x -= bc.x; y -= bc.y; z -= bc.z;
        const double x1 = x * ca - y * sa, y1 = x * sa + y * ca, y2 = y1 * cb - z * sb;
        return Q{W / 2 + x1 * px, H / 2 - y2 * px, cd, 1};
    };
    if (follow && sel.kind == Selection::Far) {           // keep a far-away thing in the middle
        FarItem f;
        if (farItem(sys, sel.moon, f)) {
            if (f.kind != "probe") { cd = cdep = 0; const Q q = flat(f.pos.x, f.pos.y, f.pos.z); ox = W / 2 - q.first; oy = H / 2 - q.second; }
            else { cd = cdep = base(f.pos.x, f.pos.y, f.pos.z).dep; const Q q = base(f.pos.x, f.pos.y, f.pos.z); ox = W / 2 - q.first; oy = H / 2 - q.second; }
        }
    } else if (follow && sel.kind != Selection::None) {   // keep the followed object in the middle
        Vec3 t;
        if (sel.kind == Selection::Bary) t = bc;
        else if (sel.kind == Selection::Body || sel.kind == Selection::Moon) { const Body* b = findBody(sys, sel.bodyId); if (b) t = b->pos; }
        cd = cdep = base(t.x, t.y, t.z).dep;              // depth first, so the followed object sits in the middle with no perspective shift
        const Q q = base(t.x, t.y, t.z);
        ox = W / 2 - q.first; oy = H / 2 - q.second;
    }
    const auto proj = [&](double x, double y, double z) { Q q = base(x, y, z); q.first += ox; q.second += oy; return q; };

    setPictureColours(set.sky != SkyMode::Off, set.theme);
    g_textScale = set.textScale;
    g_skyHalo = set.sky != SkyMode::Off;
    R.rect(0, 0, W, H, kBg);
    hits_.clear();
    if (set.sky != SkyMode::Off) {                        // the night sky: dim close up, brighter as you zoom out
        const double zt = std::clamp(-set.zoom / 3.5, 0.0, 1.0), lv = set.sky == SkyMode::Full ? 0.45 + 0.55 * zt : 0.16 + 0.6 * zt;
        drawSky(R, T, W, H, ca, sa, cb, sb, set.cons, lv);
    }

    // the Sun as a star of 1 solar mass (the web's starProps for GM = 100)
    const double hzIn = 0.95, hzOut = 1.67;               // habitable zone, AU
    const Color sunCol = starColor(5772);
    const Q star = proj(0, 0, 0);
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
            LinePen pen{R, W, H, pass ? 3.0 : 6.0, pass ? kHL : Color{0, 0, 0, 0.65f}};
            for (auto& p : pts) pen.to(p.first, p.second);
        }
    };

    {   // habitable zone: a faint green band
        const Color band = {70 / 255.f, 180 / 255.f, 100 / 255.f, 0.13f};
        if (bigR(W, H, hzOut * kAU * px)) {               // zoomed far in: draw just the strip of it that crosses the picture
            // the point of the ecliptic under the middle of the picture, and the band's direction there
            const double wx = -ox / px, wy = (oy / px) / std::max(std::fabs(cb), 0.05);
            const double X0 = wx * ca + wy * sa + bc.x, Y0 = -wx * sa + wy * ca + bc.y, r0 = std::max(std::hypot(X0, Y0), 1e-9), ux = X0 / r0, uy = Y0 / r0;
            const double nx0 = ux * ca - uy * sa, ny0 = -(ux * sa + uy * ca) * cb, nl = std::max(std::hypot(nx0, ny0), 1e-9), nx = nx0 / nl, ny = ny0 / nl, k = px * nl;
            const double tx = -ny, ty = nx, cx0 = W / 2, cy0 = H / 2, L = 4000;
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
            T.text("habitable zone", q.first, q.second - 3, 10, hexColor("#3d9a5c", 0.85f), 0.5, true);
    }

    {   // distance rings, labelled in AU
        static const double steps[] = {0.5, 1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000, 200000, 500000,
                                       1e6, 2e6, 5e6, 1e7, 2e7, 5e7, 1e8, 2e8, 5e8, 1e9, 2e9, 5e9, 1e10};
        double stepAU = 1e10;
        for (double v : steps) if (v * kAU * px >= 55) { stepAU = v; break; }
        const double diag = std::hypot(W, H);
        for (int k = 1; k * stepAU * kAU * px <= diag && k < 60; k++) {
            const double Rm = k * stepAU * kAU;
            LinePen pen{R, W, H, 1.0, kMuted.withAlpha(0.22f)};
            for (int i = 0; i <= 96; i++) { const double t = i / 96.0 * 2 * kPi; const Q q = proj(Rm * std::cos(t), Rm * std::sin(t), 0); pen.to(q.first, q.second); }
            const Q q = proj(Rm * std::cos(-0.8), Rm * std::sin(-0.8), 0);
            const double au = k * stepAU;
            const double v = std::round(au * 10) / 10;
            std::string label = (v == std::floor(v) ? fmt::commas(v) : fmt::fixed(v, 1)) + " AU";
            if (au >= 30000) label += " · " + fmtLy(au / kLightYearAU) + " light years";
            T.text(label, q.first, q.second + 3, 10, kMuted.withAlpha(0.75f));
        }
    }

    if (set.belts && !sys.belts.empty()) {   // belts: thousands of rocks on Kepler orbits, one instanced draw call per belt
        if (beltT_ != sys.t || beltEpoch_ != sys.epochMs || beltPos_.size() != sys.belts.size()) {
            beltPos_.resize(sys.belts.size());
            for (std::size_t gi = 0; gi < sys.belts.size(); gi++) {
                const BeltGroup& g = sys.belts[gi];
                beltPos_[gi].resize(g.n);
                for (int k = 0; k < g.n; k++) beltPos_[gi][k] = beltPoint(g, k, sys.t);
            }
            beltT_ = sys.t; beltEpoch_ = sys.epochMs;
        }
        for (std::size_t gi = 0; gi < sys.belts.size(); gi++) {
            const BeltGroup& g = sys.belts[gi];
            const bool on = sel.kind == Selection::Belt && sel.moon == static_cast<int>(gi);   // the selected belt: bigger dots, in the look-here colour
            beltScreen_.clear();
            for (const Vec3& p : beltPos_[gi]) {
                const Q q = proj(p.x, p.y, p.z);
                if (q.first > -2 && q.first < W + 2 && q.second > -2 && q.second < H + 2) { beltScreen_.push_back(static_cast<float>(q.first)); beltScreen_.push_back(static_cast<float>(q.second)); }
            }
            R.squares(beltScreen_.data(), static_cast<int>(beltScreen_.size() / 2), on ? 2.6 : 1.4, on ? kHL : hexColor(g.color, g.alpha));
        }
    }

    std::vector<const Body*> vis;
    for (const Body& b : sys.bodies) if (visible(b)) vis.push_back(&b);

    for (const Body* bp : vis) {   // trails: a short tail behind each object (or every lap, with Full orbit trails)
        const Body& b = *bp;
        const auto& tr = b.trail;
        const int n = static_cast<int>(tr.size());
        if (n == 0) continue;
        int st0 = 0;
        if (!set.fullTrails && n > 1) {                   // only the last stretch, about 0.6 of the distance to the Sun
            const double lim = 0.6 * std::max(length(b.pos), kSimLength);
            double acc = length(b.pos - tr[n - 1]);
            st0 = n - 1;
            while (st0 > 0 && acc < lim) { acc += length(tr[st0] - tr[st0 - 1]); st0--; }
        }
        const int nn = n - st0;
        const bool on = sel.kind == Selection::Body && sel.bodyId == b.id;
        for (int part = 0; part < 4; part++) {           // fades in 4 steps, oldest faintest
            const int i0 = st0 + part * nn / 4, i1 = std::min(n, st0 + (part + 1) * nn / 4 + 1);
            LinePen pen{R, W, H, on ? 2.0 : 1.5, hexColor(b.color, 0.15f + 0.2f * part)};
            for (int i = i0; i < i1; i++) { const Q q = proj(tr[i].x, tr[i].y, tr[i].z); pen.to(q.first, q.second); }
            if (part == 3) { const Q q = proj(b.pos.x, b.pos.y, b.pos.z); pen.to(q.first, q.second); }   // always end at the body itself
        }
    }

    std::vector<Vec3> cpts;
    for (const Body* bp : vis) if (bp->comet) {         // the comets' whole orbits, thin and dotted
        const bool on = sel.kind == Selection::Body && sel.bodyId == bp->id;
        cometLocalPath(*bp->comet, sys.julianDay(), px, cpts);
        LinePen pen{R, W, H, on ? 1.6 : 1.0, hexColor(bp->color, on ? 0.85f : 0.35f), 2, 4};
        for (const Vec3& p : cpts) { const Q q = proj(p.x, p.y, p.z); pen.to(q.first, q.second); }
    }

    // ---- spin: how each planet is turned right now, seen on this screen ----
    // The screen's right, up and towards-you directions in ecliptic coordinates (the camera above):
    const Vec3 VX{ca, -sa, 0}, VY{sa * cb, ca * cb, -sb}, VZ{sa * sb, ca * sb, cb};
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
    // Saturn's rings: half behind the ball (back) and half in front, tilted with its real axis
    const auto saturnRings = [&](const SpinCtx* c, double x, double y, double r, bool back) {
        const double rot = c ? (std::hypot(c->px, c->py) > 1e-4 ? std::atan2(c->px, c->py) : 0) : -0.22;
        const double flat = c ? std::max(std::fabs(c->pz), 0.004) : 0.34;
        const bool upper = back == (c ? c->pz > 0 : true);    // the far side of the rings is the side towards the pole if the pole leans at you
        const double a0 = upper ? kPi : 0, cr = std::cos(rot), srt = std::sin(rot);
        const struct { double in, out; Color c; } bands[] = {{1.24, 1.52, rgba(140, 120, 95, .38f)}, {1.54, 1.94, rgba(232, 212, 168, .9f)}, {1.99, 2.26, rgba(206, 186, 146, .7f)}};
        const auto pt = [&](double rho, double th, double& X, double& Y) {   // ring plane -> screen (as the web's canvas transform)
            const double u = r * rho * std::cos(th), v = r * flat * rho * std::sin(th);
            X = x + cr * u - srt * v; Y = y + srt * u + cr * v;
        };
        for (const auto& bd : bands)
            for (int i = 0; i < 48; i++) {
                const double t0 = a0 + kPi * i / 48, t1 = a0 + kPi * (i + 1) / 48;
                double ax, ay, bx, by, cx2, cy2, dx2, dy2;
                pt(bd.in, t0, ax, ay); pt(bd.out, t0, bx, by); pt(bd.out, t1, cx2, cy2); pt(bd.in, t1, dx2, dy2);
                R.triangle(ax, ay, bd.c, bx, by, bd.c, cx2, cy2, bd.c);
                R.triangle(ax, ay, bd.c, cx2, cy2, bd.c, dx2, dy2, bd.c);
            }
    };

    // ---- the Sun ----
    struct Label { std::string name; double x, y; bool selected, small, force; bool gc = false; };
    std::vector<Label> labels;
    const double sr = TRUE_SCALE ? kSunRadius * px : std::max(kCrashRadius * px, 3.5);
    const bool sunTiny = TRUE_SCALE && sr < 2.5;
    const double srd = TRUE_SCALE ? std::max(sr, 5.0) : sr;
    SpinCtx sunSpin;
    const bool hasSunSpin = spinCtx("Sun", sunSpin);
    if (hasSunSpin) spinAxis("Sun", sunSpin, star.first, star.second, sr, true);
    R.glow(star.first, star.second, sr * 3, sunCol);
    R.sunDisc(star.first, star.second, sr, sunCol);
    if (hasSunSpin) spinAxis("Sun", sunSpin, star.first, star.second, sr, false);
    if (sunTiny) R.ring(star.first, star.second, 7, 1.6, sunCol.withAlpha(0.9f));   // smaller than a pixel: a ring shows where it is
    if (sel.kind == Selection::Star) hiRing(star.first, star.second, srd + (sunTiny ? 8 : 6));
    hits_.push_back({{Selection::Star}, star.first, star.second, std::max(srd, sunTiny ? 10.0 : 0.0)});

    {   // the barycentre: a crosshair in a ring (hidden while it would sit on the Sun's centre)
        const Q bq = proj(bc.x, bc.y, bc.z);
        const bool on = sel.kind == Selection::Bary;
        if (std::hypot(bq.first - star.first, bq.second - star.second) > 5 || on) {
            for (int pass = 0; pass < 2; pass++) {
                const Color c = pass ? (on ? kHL : Color{1, 1, 1, 1}) : Color{0, 0, 0, 0.6f};
                const double w = pass ? 1.8 : 4;
                R.ring(bq.first, bq.second, 6, w, c);
                R.line(bq.first - 10, bq.second, bq.first + 10, bq.second, w, c);
                R.line(bq.first, bq.second - 10, bq.first, bq.second + 10, w, c);
            }
            hits_.push_back({{Selection::Bary}, bq.first, bq.second, 12});
            labels.push_back({"Barycentre", bq.first, bq.second + 22, on, false, false});
        }
    }

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
        if (b.comet) {   // a glowing head and two tails (blue gas tail straight, yellow dust tail curved), pointing away from the Sun
            const double r = length(b.pos), rA = r / kAU, act = std::clamp((4 - rA) / 3.2, 0.0, 1.0);
            const double L = b.comet->tail * act * std::min(1.5, 1 / rA) * kAU;   // tail length, m
            const Vec3 u = b.pos * (1 / r);
            const double vm = std::max(length(b.vel), 1e-9);
            if (L * px > 4) {
                const Vec3 dirs[2] = {u, u - b.vel * (0.7 / vm)};
                const double lens[2] = {1, 0.8}, wds[2] = {0.05, 0.09};
                const Color cols[2] = {rgba(140, 205, 255), rgba(255, 232, 180)};
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
            labels.push_back({b.name, q.first, q.second - 13 - 3 * act, on, false, false});
            hits_.push_back({{Selection::Body, b.id}, q.first, q.second, 12});
            continue;
        }
        if (d3 && std::fabs(b.pos.z) > 0.05 * kSimLength) {   // 3D: a dashed drop line to the ecliptic plane shows the height
            const Q g = proj(b.pos.x, b.pos.y, 0);
            LinePen pen{R, W, H, 1.0, hexColor(b.color, 0.35f), 3, 3};
            pen.to(q.first, q.second); pen.to(g.first, g.second);
            R.disc(g.first, g.second, 2, hexColor(b.color));
        }
        const double dx = star.first - q.first, dy = star.second - q.second, dlen = std::max(std::hypot(dx, dy), 1e-9);
        if (q.first + rad > -12 && q.first - rad < W + 12 && q.second + rad > -12 && q.second - rad < H + 12) {
            SpinCtx sc;
            const bool spins = rad > 3 && spinCtx(b.name, sc);     // big enough to see it turn
            const bool saturn = b.name == "Saturn";
            if (b.name == "Earth" && rad > 3)                      // a thin blue edge of air
                for (int i = 0; i < 64; i++) {
                    const double t0 = 2 * kPi * i / 64, t1 = 2 * kPi * (i + 1) / 64, r0 = rad * 0.96, r1 = rad * 1.16;
                    const Color in = rgba(120, 180, 255, 0.5f), out = rgba(120, 180, 255, 0);
                    const double ax = q.first + r0 * std::cos(t0), ay = q.second + r0 * std::sin(t0), bx = q.first + r1 * std::cos(t0), by = q.second + r1 * std::sin(t0);
                    const double cx2 = q.first + r1 * std::cos(t1), cy2 = q.second + r1 * std::sin(t1), ex = q.first + r0 * std::cos(t1), ey = q.second + r0 * std::sin(t1);
                    R.triangle(ax, ay, in, bx, by, out, cx2, cy2, out);
                    R.triangle(ax, ay, in, cx2, cy2, out, ex, ey, in);
                }
            if (saturn) saturnRings(spins ? &sc : nullptr, q.first, q.second, rad, true);
            if (spins) {
                spinAxis(b.name, sc, q.first, q.second, rad, true);
                R.planet(q.first, q.second, rad, hexColor(b.color), dx / dlen, dy / dlen, sc.K, b.name == "Earth");
                spinAxis(b.name, sc, q.first, q.second, rad, false);
            } else
                R.sphere(q.first, q.second, rad, hexColor(b.color), dx / dlen, dy / dlen);   // lit from the Sun
            if (saturn) saturnRings(spins ? &sc : nullptr, q.first, q.second, rad, false);
        }
        if (tiny) R.ring(q.first, q.second, 5, 1.6, hexColor(b.color, 0.9f));
        if (on) hiRing(q.first, q.second, rd + (tiny ? 5 : 6));
        labels.push_back({b.name, q.first, q.second - rd - 5, on, false, false});
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
        if (!here && maxDisp > 0.3 * distPx) continue;    // hidden while it would crowd the map: zoom in or click the planet
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
            {   // the orbit (seen from above, the tilted circle looks like an ellipse)
                const double rPx = dW * px;
                double t0 = 0, t1 = 2 * kPi;
                int NN = static_cast<int>(std::min(720.0, std::max(48.0, std::ceil(rPx / 4))));
                bool drawIt = true;
                if (!d3 && bigR(W, H, rPx)) {             // a giant flat orbit: only the visible stretch is worked out
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
                            const Q qq = proj(b.pos.x + dW * std::cos(t), b.pos.y + Y * ct, b.pos.z + Y * st);
                            pen.to(qq.first, qq.second);
                        }
                    }
                }
            }
            const double Y = dW * yr * sgn;
            const Q mp = proj(b.pos.x + dW * xr, b.pos.y + Y * ct, b.pos.z + Y * st);
            const double mr = TRUE_SCALE ? m.radiusKm * kKm * px : 1.6 + 0.9 * std::max(0.0, std::log10(m.radiusKm / 50));
            const bool mtiny = TRUE_SCALE && mr < 1.2;
            const double md = mtiny ? 3.5 : mr;
            if (!fast) {
                const double dx = star.first - mp.first, dy = star.second - mp.second, dlen = std::max(std::hypot(dx, dy), 1e-9);
                R.disc(mp.first, mp.second, mr + 0.8, kCard);
                R.sphere(mp.first, mp.second, mr, hexColor("#a4a4a4"), dx / dlen, dy / dlen);
                if (mtiny) R.ring(mp.first, mp.second, 3.5, 1.4, hexColor("#9b9b9b", 0.9f));
                if (on) hiRing(mp.first, mp.second, md + 5);
            }
            exag_[{b.id, static_cast<int>(j)}] = dW / aM;
            hits_.push_back({{Selection::Moon, b.id, static_cast<int>(j)}, mp.first, mp.second, std::max(md, mtiny ? 9.0 : 0.0)});
            if (here) labels.push_back({m.name, mp.first, mp.second - md - 4, on, true, false});
        }
    }

    // ---- far-away things: real stars, clusters, the galactic centre and the Voyagers at their true direction and distance ----
    if (set.far) {
        const auto fmtLyS = [](double v) { return fmtLy(v); };
        const auto inView = [&](const Q& q) { return q.dep - cd < 0.9 * D && q.first > -30 && q.first < W + 30 && q.second > -30 && q.second < H + 30; };
        const Color gold = hexColor("#ffd27a");
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
            else if (f.kind == "object") { R.ring(q.first, q.second, 6, 1, hexColor("#8fd0ff")); R.disc(q.first, q.second, 1.6, hexColor("#8fd0ff")); }
            else { const auto c = bvRGB(spBV(f.far->sp)); R.disc(q.first, q.second, 2.8, rgba(c[0], c[1], c[2])); R.ring(q.first, q.second, 6, 1, Color{1, 1, 1, 0.45f}); }
            if (on) hiRing(q.first, q.second, 11);
            if (f.kind == "bh") {
                labels.push_back({"Galactic centre · " + fmtLyS(f.ly) + " light years", q.first, q.second - 11, on, false, true, true});
                labels.push_back({f.name + " (black hole)", q.first, q.second + 19, on, false, true, true});
            } else
                labels.push_back({f.name + " · " + (probe ? fmt::commas(f.au) + " AU" : fmtLyS(f.ly) + " light years"), q.first, q.second - 11, on, true, false});
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

    // ---- labels: the selected one first; crowded labels are skipped ----
    {
        std::vector<std::array<double, 3>> placed;        // x, y, width of labels already drawn
        const auto put = [&](const std::string& t, double x, double y, double size, Color c, bool force) {
            const double wd = T.width(t, size) + 4;
            if (!force)
                for (const auto& o : placed)
                    if (std::fabs(o[0] - x) < (o[2] + wd) / 2 && std::fabs(o[1] - y) < 13) return;
            placed.push_back({x, y, wd});
            T.text(t, x, y, size, c);
        };
        put("Sun · 1 solar mass (M☉)", star.first, star.second - srd - (sunTiny ? 11 : 8), 12, kFg, true);
        std::stable_sort(labels.begin(), labels.end(), [](const Label& a, const Label& b) { return a.selected > b.selected; });
        for (const BeltLabel& l : beltLabels) {
            if (l.on) {                                   // the selected belt: white text on a pink pill
                const double tw = T.width(l.name, 12);
                const ImVec2 a(static_cast<float>(originX + (l.x - tw / 2 - 6) * uiScale), static_cast<float>(originY + (l.y - 12) * uiScale));
                const ImVec2 b(static_cast<float>(originX + (l.x + tw / 2 + 6) * uiScale), static_cast<float>(originY + (l.y + 6) * uiScale));
                dl->AddRectFilled(a, b, ImGui::ColorConvertFloat4ToU32(ImVec4(kHL.r, kHL.g, kHL.b, 1)), static_cast<float>(9 * uiScale));
                T.text(l.name, l.x, l.y, 12, Color{1, 1, 1, 1}, 0.5, false);
                placed.push_back({l.x, l.y, tw + 12});
            } else put(l.name, l.x, l.y, 10, kMuted, false);
        }
        for (const Label& l : labels) put(l.name, l.x, l.y, l.gc ? 11 : l.small ? 10 : 12, kFg, l.selected || l.force);
    }

    // ---- info card, bottom right (the first click closes it) ----
    double gizY = H - 44, barY = H - 24;                  // the x y axes and the scale bar sit above the card when there is one
    Card card;
    CardOptions copt;
    copt.paused = set.paused; copt.tspeed = set.tspeed; copt.trueScale = TRUE_SCALE; copt.moonsOn = set.moons; copt.beltsOn = set.belts;
    if (sel.kind == Selection::Moon) { auto it = exag_.find({sel.bodyId, sel.moon}); copt.moonExaggeration = it == exag_.end() ? 1 : it->second; }
    if (sel.kind != Selection::None && !cardHidden && !journey && cardText(sys, sel, copt, card)) {
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
        const double kmPerPt = 1 / (px * kKm), uf = fmt::imperial() ? fmt::kMilesPerKm : 1;   // the bar's length is a round number in the chosen unit
        const double raw = 100 * kmPerPt * uf, pw = std::pow(10.0, std::floor(std::log10(raw))), mm = raw / pw;
        const double ku = pw * (mm >= 5 ? 5 : mm >= 2 ? 2 : 1), km = ku / uf, len = km / kmPerPt, au = km * kKm / kAU, sec = km * kKm / kLightSpeed;
        const char* unm = fmt::imperial() ? "mi" : "km";
        const auto nf = [](double v) { return v >= 1 ? fmt::commas(v) : fmt::precision(v, 1); };
        const auto pl = [&](double v, const char* u) { return nf(v) + " " + u + (std::llround(v) == 1 && v >= 0.95 ? "" : "s"); };
        const std::string main = std::string(d3 ? "≈ " : "") + (ku < 1e6 ? nf(ku) + " " + unm : au < 0.05 ? nf(ku / 1e6) + " million " + unm : nf(au) + " AU");
        const std::string lt = sec < 1 ? nf(sec * 1000) + " ms" : sec < 60 ? pl(sec, "second") : sec < 3600 ? pl(sec / 60, "minute")
                             : sec < 86400 ? pl(sec / 3600, "hour") : sec < 86400 * 365.25 ? pl(sec / 86400, "day") : fmt::precision(sec / 86400 / 365.25, 2) + " years";
        const double spanAU = W / px / kAU, spanLy = spanAU / kLightYearAU;
        if (spanAU >= 1500) {                             // zoomed far out: say where you are
            const std::string sp2 = spanLy >= 1 ? fmtLy(spanLy) + " light years" : fmt::commas(std::stod(fmt::precision(spanAU, 2))) + " AU (" + fmt::fixed(spanLy, 2) + " light years)";
            const std::string msg = "Zoomed far out: this picture is about " + sp2 + " across, so the whole Solar System is now just the ringed dot (the Sun).";
            // wrap to the picture width
            const std::size_t maxChars = static_cast<std::size_t>(std::max(24.0, std::floor((W - 28) / 5.6)));
            std::vector<std::string> lines; std::string cur, word;
            for (char ch : msg + " ") {
                if (ch != ' ') { word += ch; continue; }
                const std::string t = cur.empty() ? word : cur + " " + word;
                if (t.size() > maxChars && !cur.empty()) { lines.push_back(cur); cur = word; } else cur = t;
                word.clear();
            }
            if (!cur.empty()) lines.push_back(cur);
            for (std::size_t i = 0; i < lines.size(); i++) T.text(lines[i], 14, barY - 34 - (lines.size() - 1 - i) * 14.0, 11, kFg, 0, true, kCard);
        }
        const double x0 = 14, y = barY;
        for (int pass = 0; pass < 2; pass++) {
            const Color c = pass ? kFg : kCard;
            const double w = pass ? 1.6 : 5;
            R.line(x0, y - 4, x0, y + 4, w, c);
            R.line(x0, y, x0 + len, y, w, c);
            R.line(x0 + len, y - 4, x0 + len, y + 4, w, c);
        }
        T.text(main, x0, y - 8, 11, kFg, 0, true, kCard);
        T.text("light takes " + lt, x0, y + 15, 10, kMuted, 0, true, kCard);
    }

    {   // axis gizmo: which way x, y and z point on screen
        const double gx = W - 40, gy = gizY;
        const auto axis = [&](double X, double Y, double Z, double& sx, double& sy) { const double x1 = X * ca - Y * sa, y1 = X * sa + Y * ca; sx = 24 * x1; sy = 24 * (y1 * cb - Z * sb); };
        struct Ax { const char* n; double X, Y; Color c; } ax[] = {{"x", 0, 0, hexColor("#e5484d")}, {"y", 0, 0, hexColor("#30a46c")}, {"z", 0, 0, hexColor("#3e63dd")}};
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

    if (journey && captionAlpha > 0.01 && !caption.empty()) {   // the Journey's caption, top middle
        const std::size_t width = static_cast<std::size_t>(std::max(20.0, std::floor((W - 44) / 7.9)));
        std::vector<std::string> lines;
        {
            std::string cur, word;
            for (char ch : caption + " ") {
                if (ch != ' ') { word += ch; continue; }
                if (word.empty()) continue;
                if (!cur.empty() && (cur + " " + word).size() > width) { lines.push_back(cur); cur = word; }
                else cur = cur.empty() ? word : cur + " " + word;
                word.clear();
            }
            if (!cur.empty()) lines.push_back(cur);
        }
        for (std::size_t i = 0; i < lines.size(); i++)
            T.text(lines[i], W / 2, 62 + i * 19.0, 13.5, Color{1, 1, 1, static_cast<float>(captionAlpha)}, 0.5, true, Color{4 / 255.f, 6 / 255.f, 13 / 255.f, 0.92f});
    }

    {   // top left: the date, which moves on with the simulated time; top right: frames per second
        T.text(dateLine(sys), 12, 20, 13, kFg, 0, true);
        T.text(fmt::fixed(fps, 0) + " fps", W - 12, 20, 11, kMuted, 1, true);
    }
}

}  // namespace orbit
