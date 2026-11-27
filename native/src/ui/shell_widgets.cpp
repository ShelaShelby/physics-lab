#include "ui/shell_widgets.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <map>

#include "app/look_options.h"
#include "app/theme_style.h"
#include "fonts.h"
#include "look.h"
#include "style_draw.h"
#include "ui/shell_test.h"

namespace ui::sh {

using app::Rgb;
using app::shell::Rect;

namespace {
Frame g_f;
std::map<std::string, float> g_heights;
std::map<std::string, float> g_scrollMax;                 // (5l) how far a panel could scroll when it was last drawn (0: its content fits, no scrollbar)
std::string g_glassId;
double g_glassA = 0.95;
// 0.15 stage 5m: the look M1 (app/look_options.h) is THE look of the New UI; for now it is applied to the Dark theme (stage 7 does the other themes through tokens).
const app::lookopt::Tokens* g_lo = nullptr;       // this frame's look (0.38: set in every theme, by beginFrame)
app::ThemeColors g_loColors;
bool g_loBuilt = false;

const char* typeName(Type t) { return t == Type::Caption ? "caption" : t == Type::Body ? "body" : t == Type::Title ? "title" : "key"; }
ImU32 colOf(const Rgb& c, float a = 1) { return ui::col(c, a); }
float thick() { return std::max(1.25f, static_cast<float>(1.75 * g_f.dpi * g_f.tf * 0.9 * g_lo->stroke)); }       // the icon stroke: 1.75 u
}  // namespace

void beginFrame() {
    g_f.dpi = ImGui::GetStyle().FontScaleDpi;
    g_f.tf = std::round(look().textScale * 1000.0) / 1000.0;                     // 1.3f is 1.29999995: 15 x it rounds to 19, not 20 (the 19.5 of DESIGN.md)
    g_f.m = app::shell::metrics(g_f.dpi, g_f.tf);
    g_f.c = &look().c;
    const app::ThemeColors& c = look().c;
    g_f.plain = app::isPlainStyle(c.style);
    g_f.outline = app::lookopt::outlineMode(c);          // Monochrome and High contrast
    const std::string key = c.id;
    if (key != g_glassId) { g_glassId = key; g_glassA = app::shell::glassOpacity(c); }
    g_f.panelAlpha = static_cast<float>(g_glassA);
    g_lo = &app::lookopt::forTheme(c);                                          // 0.38: the new look in every theme (Dark: its own palette; the others: their own colours)
    if (!g_loBuilt || g_loColors.id != c.id) { g_loColors = app::lookopt::applyFor(c); g_loBuilt = true; }
    g_f.c = &g_loColors;
    g_f.panelAlpha = c.id == "dark" ? g_lo->panelAlpha : static_cast<float>(g_glassA);   // (the other themes keep the opacity found for them: DECISIONS, glass panels are nearly solid)
    g_f.m.radiusPanel = app::shell::unitPx(g_lo->radiusPanel, g_f.dpi, g_f.tf);
    g_f.m.radiusCtl = app::shell::unitPx(g_lo->radiusCtl, g_f.dpi, g_f.tf);
}
const Frame& F() { return g_f; }
bool lookM1() { return g_lo != nullptr; }

ImU32 cText(float a) { return colOf(g_f.c->fg, a); }
ImU32 cMuted(float a) { return colOf(g_f.c->muted, a); }
ImU32 cDanger(float a) { return colOf(g_f.c->danger, a); }
ImU32 cAccent(float a) { return colOf(g_f.c->accent, a); }
ImU32 cFill(float a) { return colOf(g_f.c->accentFill, a); }
// 0.15 stage 5p (M1), widened to EVERY theme in 5q: a LIT toolbar / top-bar button (and the open chip) is a subtle fill a little away from the bar (the card colour moved 14 % towards the text
// colour: in a dark theme a little lighter, in a light theme a little darker), the icon at full strength (white on a dark theme, the theme's text colour on a light one), no outline, no glow.
// Nothing here is per theme: all of it comes from the theme tokens (card, text, muted). The idle icon is the muted colour moved towards the card until it is at least 3.3:1 under the lit icon
// (WCAG relative luminance), so lit against idle is above 3:1 without colour; the plain muted colour of Dark (#9a9aa3) gives only 2.8. Not used for the Controls panel's segmented buttons and the
// Settings tiles: they keep the chosen light pill. The hover and press of a toolbar / top-bar button are the face moved towards the TEXT colour in every theme (never the accent: the owner's "no blue").
static constexpr float kLitMix = 0.14f, kIdleMix = 0.15f, kIdleVsLit = 3.3f;
static float relLum(const Rgb& c) {
    const auto lin = [](float v) { return v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f); };
    return 0.2126f * lin(c.r) + 0.7152f * lin(c.g) + 0.0722f * lin(c.b);
}
static float wcag(const Rgb& a, const Rgb& b) { const float la = relLum(a), lb = relLum(b); return (std::max(la, lb) + 0.05f) / (std::min(la, lb) + 0.05f); }
static Rgb litIconRgb() { return relLum(g_f.c->card) < 0.18f ? Rgb{1, 1, 1} : g_f.c->fg; }          // a dark theme: white; a light theme: its own text colour
static ImU32 cLitFill() { return colOf(app::mixRgb(g_f.c->card, g_f.c->fg, kLitMix)); }
static ImU32 cLitIcon() { return colOf(litIconRgb()); }
static Rgb idleIconRgb() {
    const Rgb lit = litIconRgb();
    Rgb c = app::mixRgb(g_f.c->muted, g_f.c->card, kIdleMix);
    for (int i = 0; i < 12 && wcag(c, lit) < kIdleVsLit; i++) c = app::mixRgb(c, g_f.c->card, 0.08f);
    return c;
}
static ImU32 cIdleIcon() { return colOf(idleIconRgb()); }
static ImU32 cSliderFill() { return colOf(g_f.c->accentFill); }   // 0.38: the filled part is the accent fill (the owner's new look; 5q made it grey)
static ImU32 cSliderKnob(bool lit) { const bool dark = relLum(g_f.c->card) < 0.18f; return colOf(dark ? (lit ? Rgb{1, 1, 1} : app::mixRgb(g_f.c->fg, Rgb{1, 1, 1}, 0.3f)) : (lit ? app::mixRgb(g_f.c->card, g_f.c->fg, 0.06f) : g_f.c->card)); }
ImU32 cOnFill() { return colOf(app::shell::onAccent(*g_f.c)); }
static Rgb faceRgb();
// 0.38 (the owner's new look, 6 Nov 2026; it replaces 5t's grey pill): a chosen segment or list row is a soft accent tint with a thin accent edge and the text colour on it;
// in the outline themes (Monochrome, High contrast) it is the solid accent fill with its on-fill text, because a tint is too faint there
static Rgb selTintRgb();
static ImU32 cSelTint() { return colOf(selTintRgb()); }
static ImU32 cSelEdge() { return g_f.outline ? colOf(g_f.c->accentFill) : colOf(g_f.c->accent, 0.85f); }   // (85 %: at 60 % the edge was 2.6:1 on the Light face, under the 3:1 of WCAG 1.4.11)
static ImU32 cSelText() { return g_f.outline ? colOf(app::shell::onAccent(*g_f.c)) : colOf(g_f.c->fg); }
static Rgb faceRgb() { const app::ThemeColors& c = *g_f.c; return app::mixRgb(c.card, c.fg, g_lo->faceMix); }
static Rgb selTintRgb() { return g_f.outline ? g_f.c->accentFill : app::mixRgb(faceRgb(), g_f.c->accent, 0.18f); }
ImU32 cFace(float a) { return colOf(faceRgb(), a); }
// 5m part 2: in M1 a hovered / pressed face is the face moved towards the TEXT colour (a light grey, never blue: the owner's "no blue"); the other themes keep their accent tint
static const app::Rgb& tintOf() { return g_f.c->fg; }   // 0.38: hover and press move towards the text colour in every theme
ImU32 cIconHover() { return colOf(app::mixRgb(faceRgb(), g_f.c->fg, g_lo->hoverMix)); }       // 5q: toolbar / top-bar buttons: towards the text colour in every theme
ImU32 cLitFillPub() { return cLitFill(); }
ImU32 cFaceHover() { return colOf(app::mixRgb(faceRgb(), tintOf(), g_lo->hoverMix)); }
ImU32 cFacePress() { return colOf(app::mixRgb(faceRgb(), tintOf(), g_lo->pressMix)); }
ImU32 cEdge() { return g_f.outline ? colOf(g_f.c->fg) : colOf(g_f.c->fg, g_lo->edgeAlpha); }
ImU32 cHair() { return colOf(g_f.c->line); }
ImU32 cPanel() { return colOf(g_f.c->card, g_f.panelAlpha); }
ImU32 cFactText() { return colOf(app::shell::factTextColour(*g_f.c)); }
ImU32 cShadow(float a) { return colOf(app::shell::textShadow(*g_f.c), a); }

// ---- text ----
ImFont* font(bool bold) { return bold ? fonts().bold : fonts().regular; }
// stage 5c.1 (the owner: panel text at least 14 px on every theme): while the Controls panel draws, Caption text (13 px) is drawn at Body size (15 px, never under 14); no new size
static bool g_bodyFloor = false;
void bodyFloor(bool on) { g_bodyFloor = on; }
static Type floored(Type t) { return g_bodyFloor && t == Type::Caption ? Type::Body : t; }
float px(Type t) { return static_cast<float>(app::shell::textPx(floored(t), g_f.dpi, g_f.tf)); }
float width(Type t, bool bold, const std::string& s) { return font(bold)->CalcTextSizeA(px(t), FLT_MAX, 0, displayable(s, font(bold)).c_str()).x; }
float lineH(Type t) { return std::round(px(t) * 1.35f); }
void text(ImDrawList* dl, Type t, bool bold, ImVec2 pos, ImU32 col, const std::string& s) {
    ImFont* f = font(bold);
    const std::string d = displayable(s, f);
    dl->AddText(f, px(t), pos, col, d.c_str());
    auditText(typeName(floored(t)), px(t), s);
    if (g_bodyFloor) auditText("ctl", px(t), s);
}
std::vector<std::string> wrapText(Type t, bool bold, const std::string& s, float maxW) {
    std::vector<std::string> lines;
    std::size_t a = 0;
    while (a <= s.size()) {
        std::size_t nl = s.find('\n', a);
        if (nl == std::string::npos) nl = s.size();
        const std::string para = s.substr(a, nl - a);
        a = nl + 1;
        std::string cur;
        std::size_t p = 0;
        while (p <= para.size()) {
            std::size_t q = para.find(' ', p);
            if (q == std::string::npos) q = para.size();
            const std::string w = para.substr(p, q - p);
            p = q + 1;
            if (w.empty() && q != para.size()) continue;
            const std::string tryLine = cur.empty() ? w : cur + " " + w;
            if (!cur.empty() && width(t, bold, tryLine) > maxW + 0.01f) {   // (0.39.2: + a hair for float rounding: a room made exactly as wide as the text, as the fact line does, wrapped its last word in some fonts)
                const std::size_t sp = cur.rfind(' ');
                if (w == "…" && sp != std::string::npos) { lines.push_back(cur.substr(0, sp)); cur = cur.substr(sp + 1) + " …"; }   // stage 5j looks: a lone "…" ("more decay modes") never stands on a line of its own: the word before it comes with it
                else {
                    // 0.39.3: a lone key ("R" of "R reset view", "1 2 3" of "1 2 3 switch lab", "+") never ends a line: it goes down with the words after it
                    const auto loneKey = [](const std::string& l, std::size_t at) { const char k = l[at + 1]; return l.size() == at + 2 && ((k >= 'A' && k <= 'Z') || (k >= '0' && k <= '9') || k == '+'); };
                    std::string head = cur, tail = w;
                    for (std::size_t at = head.rfind(' '); at != std::string::npos && loneKey(head, at); at = head.rfind(' ')) { tail = head.substr(at + 1) + " " + tail; head = head.substr(0, at); }
                    lines.push_back(head); cur = tail;
                }
            } else cur = tryLine;
        }
        lines.push_back(cur);
    }
    return lines;
}
float paragraph(ImDrawList* dl, Type t, bool bold, ImVec2 pos, float maxW, ImU32 col, const std::string& s) {
    const std::vector<std::string> lines = wrapText(t, bold, s, maxW);
    for (std::size_t i = 0; i < lines.size(); i++) text(dl, t, bold, ImVec2(pos.x, pos.y + i * lineH(t)), col, lines[i]);
    return lines.size() * lineH(t);
}
void outlined(ImDrawList* dl, Type t, ImVec2 pos, ImU32 col, ImU32 outline, const std::string& s) {
    ImFont* f = font(false);
    const std::string d = displayable(s, f);
    const float o = std::max(1.0f, static_cast<float>(std::round(g_f.dpi)));                      // 1 u
    for (int k = 0; k < 8; k++) {
        const float a = k * 0.785398f;
        dl->AddText(f, px(t), ImVec2(pos.x + o * std::cos(a), pos.y + o * std::sin(a)), outline, d.c_str());
    }
    dl->AddText(f, px(t), pos, col, d.c_str());
    auditText(typeName(t), px(t), s);
}
void shadowed(ImDrawList* dl, Type t, ImVec2 pos, ImU32 col, float fade, const std::string& s) {
    ImFont* f = font(false);
    const std::string d = displayable(s, f);
    const float o = std::max(1.0f, static_cast<float>(std::round(g_f.dpi)));                      // 1 u
    for (int ring = 2; ring >= 1; ring--)
        for (int k = 0; k < 8; k++) {
            const float a = k * 0.785398f;
            dl->AddText(f, px(t), ImVec2(pos.x + ring * o * std::cos(a), pos.y + ring * o * std::sin(a)), cShadow((ring == 1 ? app::shell::kShadowAlpha : 0.35f) * fade), d.c_str());
        }
    ImVec4 c = ImGui::ColorConvertU32ToFloat4(col);
    c.w *= fade;
    dl->AddText(f, px(t), pos, ImGui::ColorConvertFloat4ToU32(c), d.c_str());
    auditText(typeName(t), px(t), s);
}
float headingPx(const char* level) { return static_cast<float>(level[1] == '1' ? g_f.m.h1 : level[1] == '2' ? g_f.m.h2 : g_f.m.h3); }
void headingText(ImDrawList* dl, const char* level, ImVec2 pos, ImU32 col, const std::string& s) {
    ImFont* f = font(true);
    dl->AddText(f, headingPx(level), pos, col, displayable(s, f).c_str());
    auditText(level, headingPx(level), s);
}

// ---- icons ----
void drawIcon(ImDrawList* dl, Icon icon, ImVec2 c, float size, ImU32 col) {
    c = ImVec2(std::floor(c.x) + 0.5f, std::floor(c.y) + 0.5f);               // 0.38: on a pixel's centre, so a symmetric icon is drawn the same on both sides (the owner saw uneven pixels close up)
    const float s = size / 2, th = thick();
    auto dot = [&](float x, float y, float r) { dl->AddCircleFilled(ImVec2(std::floor(c.x + x * s) + 0.5f, std::floor(c.y + y * s) + 0.5f), std::max(1.0f, r * s), col, 12); };   // a paint dot, on a pixel centre
    auto P = [&](float x, float y) { return ImVec2(c.x + x * s, c.y + y * s); };
    switch (icon) {
    case Icon::Menu: for (float y : {-0.55f, 0.0f, 0.55f}) dl->AddLine(P(-0.75f, y), P(0.75f, y), col, th); break;
    case Icon::Calc: {   // 0.49: a calculator: the body, the display and a 2 x 2 grid of keys
        dl->AddRect(P(-0.62f, -0.85f), P(0.62f, 0.85f), col, 0.12f * s, 0, th);
        dl->AddRect(P(-0.4f, -0.62f), P(0.4f, -0.22f), col, 0, 0, th * 0.8f);
        for (int r = 0; r < 2; r++) for (int c = 0; c < 2; c++) dl->AddRectFilled(P(-0.38f + 0.48f * c, 0.02f + 0.38f * r), P(-0.1f + 0.48f * c, 0.26f + 0.38f * r), col);
        break;
    }
    case Icon::Plot: {   // 0.51: the Data panel: two axes, three points and the line fitted through them
        dl->AddLine(P(-0.85f, -0.85f), P(-0.85f, 0.85f), col, th);
        dl->AddLine(P(-0.85f, 0.85f), P(0.85f, 0.85f), col, th);
        dl->AddLine(P(-0.6f, 0.55f), P(0.8f, -0.75f), col, th * 0.8f);
        for (const ImVec2 q : {ImVec2(-0.4f, 0.15f), ImVec2(0.05f, 0.05f), ImVec2(0.45f, -0.5f)}) dl->AddCircleFilled(P(q.x, q.y), 0.16f * s, col, 10);
        break;
    }
    case Icon::Search: dl->AddCircle(P(-0.15f, -0.15f), 0.55f * s, col, 24, th); dl->AddLine(P(0.25f, 0.25f), P(0.8f, 0.8f), col, th); break;
    case Icon::Play: dl->AddTriangleFilled(P(-0.4f, -0.65f), P(-0.4f, 0.65f), P(0.7f, 0.0f), col); break;
    case Icon::Pause: dl->AddRectFilled(P(-0.55f, -0.65f), P(-0.15f, 0.65f), col); dl->AddRectFilled(P(0.15f, -0.65f), P(0.55f, 0.65f), col); break;
    case Icon::Clock: dl->AddCircle(c, 0.8f * s, col, 28, th); dl->AddLine(c, P(0, -0.5f), col, th); dl->AddLine(c, P(0.36f, 0.12f), col, th); break;
    case Icon::Eye: {
        ImVec2 top[9], bot[9];
        for (int i = 0; i < 9; i++) { const float x = -0.9f + i * 0.225f, h = 0.5f * (1 - (x / 0.9f) * (x / 0.9f)); top[i] = P(x, -h); bot[8 - i] = P(x, h); }
        ImVec2 all[18];
        for (int i = 0; i < 9; i++) { all[i] = top[i]; all[9 + i] = bot[i]; }
        dl->AddPolyline(all, 18, col, ImDrawFlags_Closed, th);
        dl->AddCircleFilled(c, 0.26f * s, col, 16);
        break;
    }
    case Icon::Target:
        dl->AddCircle(c, 0.62f * s, col, 24, th); dl->AddCircleFilled(c, 0.17f * s, col, 12);
        for (int k = 0; k < 4; k++) { const float a = k * 1.5708f, ca = std::cos(a), sa = std::sin(a); dl->AddLine(P(0.62f * ca, 0.62f * sa), P(0.98f * ca, 0.98f * sa), col, th); }
        break;
    case Icon::Ruler:
        dl->AddRect(P(-0.9f, -0.32f), P(0.9f, 0.32f), col, 0, 0, th);
        for (int k = 0; k < 5; k++) dl->AddLine(P(-0.6f + k * 0.3f, -0.32f), P(-0.6f + k * 0.3f, k % 2 ? -0.05f : 0.08f), col, th * 0.8f);
        break;
    case Icon::Camera:
        dl->AddRect(P(-0.9f, -0.42f), P(0.9f, 0.62f), col, 3, 0, th);
        dl->AddRectFilled(P(-0.3f, -0.66f), P(0.3f, -0.42f), col);
        dl->AddCircle(P(0, 0.1f), 0.32f * s, col, 16, th);
        break;
    case Icon::Swap:
        dl->AddLine(P(-0.8f, -0.3f), P(0.8f, -0.3f), col, th); dl->AddLine(P(0.8f, -0.3f), P(0.45f, -0.62f), col, th); dl->AddLine(P(0.8f, -0.3f), P(0.45f, 0.02f), col, th);
        dl->AddLine(P(0.8f, 0.3f), P(-0.8f, 0.3f), col, th); dl->AddLine(P(-0.8f, 0.3f), P(-0.45f, -0.02f), col, th); dl->AddLine(P(-0.8f, 0.3f), P(-0.45f, 0.62f), col, th);
        break;
    case Icon::Book: dl->AddRect(P(-0.9f, -0.6f), P(0.0f, 0.6f), col, 0, 0, th); dl->AddRect(P(0.0f, -0.6f), P(0.9f, 0.6f), col, 0, 0, th); break;
    case Icon::Bulb: dl->AddCircle(P(0, -0.15f), 0.52f * s, col, 20, th); dl->AddLine(P(-0.25f, 0.55f), P(0.25f, 0.55f), col, th); dl->AddLine(P(-0.18f, 0.8f), P(0.18f, 0.8f), col, th); break;
    case Icon::Palette: {   // 0.38 (the owner chose style C of six on 6 Nov 2026): a painter's palette, its round outline dented at the lower right (the thumb), three paint dots
        // (until 0.37 a full circle with three dots, which read as a face)
        const float R = 0.82f * s, a0 = 1.35f, a1 = 0.15f + 6.2831853f;
        dl->PathArcTo(c, R, a0, a1, 32);
        dl->PathBezierQuadraticCurveTo(P(0.12f, 0.12f), ImVec2(c.x + R * std::cos(a0), c.y + R * std::sin(a0)), 12);
        dl->PathStroke(col, 0, th);                                            // (not Closed: the curve already ends where the arc began; closing added a spur)
        dot(-0.45f, 0.02f, 0.12f); dot(-0.22f, -0.42f, 0.12f); dot(0.22f, -0.42f, 0.12f);
        break;
    }
    case Icon::Gear: {   // 5k: a cog: a ring (the hub hole) and a toothed rim of 8 teeth, drawn in the same thin stroke as the other icons (the old one, a circle with 8 rays, looked like a sun)
        ImVec2 pts[32];
        int n = 0;
        for (int k = 0; k < 8; k++) {
            const float a = k * 0.785398f;
            const float da[4] = {-0.36f, -0.20f, 0.20f, 0.36f}, rr[4] = {0.62f, 0.92f, 0.92f, 0.62f};   // root, tip, tip, root
            for (int j = 0; j < 4; j++) pts[n++] = P(rr[j] * std::cos(a + da[j]), rr[j] * std::sin(a + da[j]));
        }
        dl->AddPolyline(pts, n, col, ImDrawFlags_Closed, th);
        dl->AddCircle(c, 0.27f * s, col, 16, th);
        break;
    }
    case Icon::Caret: dl->AddTriangleFilled(P(-0.45f, -0.22f), P(0.45f, -0.22f), P(0.0f, 0.28f), col); break;
    case Icon::CaretUp: dl->AddTriangleFilled(P(-0.45f, 0.22f), P(0.45f, 0.22f), P(0.0f, -0.28f), col); break;
    case Icon::Close: dl->AddLine(P(-0.6f, -0.6f), P(0.6f, 0.6f), col, th); dl->AddLine(P(0.6f, -0.6f), P(-0.6f, 0.6f), col, th); break;
    case Icon::Calendar:
        dl->AddRect(P(-0.8f, -0.6f), P(0.8f, 0.75f), col, 2, 0, th); dl->AddLine(P(-0.8f, -0.2f), P(0.8f, -0.2f), col, th);
        dl->AddLine(P(-0.4f, -0.85f), P(-0.4f, -0.45f), col, th); dl->AddLine(P(0.4f, -0.85f), P(0.4f, -0.45f), col, th);
        break;
    case Icon::Cube: dl->AddRect(P(-0.6f, -0.45f), P(0.45f, 0.6f), col, 0, 0, th); dl->AddRect(P(-0.3f, -0.75f), P(0.75f, 0.3f), col, 0, 0, th); break;
    case Icon::Dots: for (float x : {-0.6f, 0.0f, 0.6f}) dl->AddCircleFilled(P(x, 0), 0.14f * s, col); break;
    case Icon::Info: dl->AddCircle(c, 0.8f * s, col, 28, th); dl->AddLine(P(0, -0.05f), P(0, 0.45f), col, th); dl->AddCircleFilled(P(0, -0.38f), 0.11f * s, col, 10); break;
    case Icon::Atom:    // a nucleus and one electron on an orbit
        dl->AddEllipse(c, ImVec2(0.95f * s, 0.4f * s), col, -0.5f, 28, th * 0.9f);
        dl->AddCircleFilled(c, 0.2f * s, col, 12); dl->AddCircleFilled(P(0.67f, -0.55f), 0.13f * s, col, 10);
        break;
    case Icon::Nucleus: // a small cluster of nucleons
        for (const ImVec2& q : {ImVec2(-0.28f, -0.25f), ImVec2(0.3f, -0.2f), ImVec2(0.0f, 0.34f), ImVec2(-0.34f, 0.3f), ImVec2(0.36f, 0.32f)}) dl->AddCircle(P(q.x, q.y), 0.3f * s, col, 14, th * 0.8f);
        break;
    case Icon::Cloud:   // scattered dots, denser in the middle
        for (const ImVec2& q : {ImVec2(0.0f, 0.0f), ImVec2(-0.22f, -0.15f), ImVec2(0.2f, 0.12f), ImVec2(0.05f, -0.32f), ImVec2(-0.15f, 0.28f), ImVec2(0.38f, -0.2f), ImVec2(-0.5f, 0.1f), ImVec2(0.55f, 0.4f), ImVec2(-0.45f, -0.5f), ImVec2(0.1f, 0.62f)})
            dl->AddCircleFilled(P(q.x, q.y), 0.11f * s, col, 8);
        break;
    case Icon::Cloud3D: // a cube with dots inside: the cloud in three dimensions
        dl->AddRect(P(-0.7f, -0.4f), P(0.35f, 0.65f), col, 0, 0, th * 0.8f); dl->AddRect(P(-0.35f, -0.75f), P(0.7f, 0.3f), col, 0, 0, th * 0.8f);
        for (const ImVec2& q : {ImVec2(-0.05f, 0.0f), ImVec2(0.12f, -0.18f), ImVec2(-0.2f, 0.18f), ImVec2(0.25f, 0.1f), ImVec2(0.0f, 0.3f)}) dl->AddCircleFilled(P(q.x, q.y), 0.1f * s, col, 8);
        break;
    case Icon::Slice: { // a flat plane seen at an angle with dots on it: a thin cut through the cloud
        const ImVec2 plane[4] = {P(-0.95f, 0.3f), P(-0.4f, -0.45f), P(0.95f, -0.45f), P(0.4f, 0.3f)};
        dl->AddPolyline(plane, 4, col, ImDrawFlags_Closed, th * 0.9f);
        for (const ImVec2& q : {ImVec2(-0.1f, -0.1f), ImVec2(0.2f, -0.2f), ImVec2(0.3f, 0.05f), ImVec2(-0.3f, 0.1f), ImVec2(0.05f, 0.12f)}) dl->AddCircleFilled(P(q.x, q.y), 0.1f * s, col, 8);
        break;
    }
    case Icon::Chart:   // the map of nuclides: a grid of squares, some filled
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) {
            const ImVec2 a = P(-0.8f + i * 0.58f, -0.8f + j * 0.58f), b = P(-0.8f + i * 0.58f + 0.46f, -0.8f + j * 0.58f + 0.46f);
            if (i + j == 2 || (i == 1 && j == 1)) dl->AddRectFilled(a, b, col); else dl->AddRect(a, b, col, 0, 0, th * 0.7f);
        }
        break;
    case Icon::Reset: { // a circular arrow
        dl->PathArcTo(c, 0.7f * s, -0.2f, 4.7f, 24); dl->PathStroke(col, 0, th);
        dl->AddTriangleFilled(P(0.4f, -0.95f), P(0.95f, -0.45f), P(0.35f, -0.35f), col);
        break;
    }
    case Icon::Move:    // four arrows
        dl->AddLine(P(-0.85f, 0), P(0.85f, 0), col, th); dl->AddLine(P(0, -0.85f), P(0, 0.85f), col, th);
        for (int k = 0; k < 4; k++) { const float a = k * 1.5708f, ca = std::cos(a), sa = std::sin(a); dl->AddLine(P(0.85f * ca, 0.85f * sa), P(0.55f * ca - 0.25f * sa, 0.55f * sa + 0.25f * ca), col, th); dl->AddLine(P(0.85f * ca, 0.85f * sa), P(0.55f * ca + 0.25f * sa, 0.55f * sa - 0.25f * ca), col, th); }
        break;
    case Icon::Sliders:   // three sliders: the Controls panel
        for (int k = 0; k < 3; k++) {
            const float y = -0.6f + k * 0.6f, kx = k == 1 ? 0.35f : k == 0 ? -0.35f : 0.1f;
            dl->AddLine(P(-0.9f, y), P(0.9f, y), col, th * 0.9f);
            dl->AddCircleFilled(P(kx, y), 0.2f * s, col, 12);
        }
        break;
    case Icon::CaretLeft: dl->AddTriangleFilled(P(0.22f, -0.45f), P(0.22f, 0.45f), P(-0.28f, 0.0f), col); break;
    case Icon::CaretRight: dl->AddTriangleFilled(P(-0.22f, -0.45f), P(-0.22f, 0.45f), P(0.28f, 0.0f), col); break;
    case Icon::Check: { const ImVec2 pts[3] = {P(-0.55f, 0.0f), P(-0.15f, 0.45f), P(0.6f, -0.45f)}; dl->AddPolyline(pts, 3, col, 0, th * 1.15f); break; }
    }
}

// ---- panels ----
bool beginPanel(const char* id, const Rect& r, Panel& p, Shape shape, bool scroll, bool solid) {
    const Frame& f = g_f;
    const float pad = shape == Shape::Panel ? static_cast<float>(app::shell::unitPx(g_lo->pad, f.dpi, f.tf)) : 0.0f;
    const float rounding = shape == Shape::Bar || shape == Shape::Overlay || shape == Shape::Clear ? 0.0f : shape == Shape::Pill ? ui::corner(static_cast<float>(f.m.radiusCtl)) : ui::corner(static_cast<float>(f.m.radiusPanel));
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(r.x), static_cast<float>(r.y)), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(r.w), static_cast<float>(r.h)), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1, 1));             // (stage 5c.1: Dear ImGui's 32 px minimum made the 24 u handle wider than its place)
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));              // the glass is drawn by hand below
    const bool shown = ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                    (scroll && shape == Shape::Panel ? 0 : ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse) | (shape == Shape::Panel ? 0 : ImGuiWindowFlags_NoBringToFrontOnFocus) |
                                    (shape == Shape::Overlay ? ImGuiWindowFlags_NoInputs : 0));
    ImGui::PopStyleColor(); ImGui::PopStyleVar(4);
    if (!shown) { ImGui::End(); return false; }
    p.dl = ImGui::GetWindowDrawList();
    const ImVec2 a = ImGui::GetWindowPos(), b(a.x + ImGui::GetWindowWidth(), a.y + ImGui::GetWindowHeight());
    panelDecor(app::PanelKind::Floating, true);                                // (only logs the window for the automatic checks, PL_LOG_WINDOWS: every theme is plain)
    const float reach = 0;                                                      // 0.38: flat, no halo, no shine (the look's tokens are 0); the archived glass themes' decor is gone with them
    if (shape != Shape::Overlay && shape != Shape::Clear) {
        p.dl->AddRectFilled(a, b, solid ? colOf(f.c->card) : cPanel(), rounding);   // (stage 5c.1: the Controls panel is solid: the picture's labels showed through it)
        if (shape == Shape::Bar) p.dl->AddLine(ImVec2(a.x, b.y - 0.5f), ImVec2(b.x, b.y - 0.5f), cEdge(), 1.0f);
        else p.dl->AddRect(a, b, cEdge(), rounding, 0, 1.0f);                  // 0.38: every panel and bar, the toolbar too, has the 1 px edge (5r had none on a pill)
    }
    auditHalo(std::string("panel") + id, a, b, ImVec2(a.x - reach, a.y - reach), ImVec2(b.x + reach, b.y + reach));
    p.origin = ImGui::GetCursorScreenPos();
    p.w = ImGui::GetContentRegionAvail().x;
    p.y = 0;
    p.id = id;
    return true;
}

float endPanel(Panel& p) {
    ImGui::SetCursorScreenPos(ImVec2(p.origin.x, p.origin.y + p.y));
    ImGui::Dummy(ImVec2(p.w, 1));
    // 0.15 stage 5l: the padding the window was BEGUN with (beginPanel pushes its own and pops it at once, so the style's value read here was the default 8 px, not the panel's 16 px): every panel
    // reported a height 16 px too small, so each one had a scroll range of 16 px (a scrollbar on a list that fitted) and the cards were clipped by that much.
    const float pad = ImGui::GetCurrentWindow()->WindowPadding.x;
    const float h = p.y + 1 + 2 * pad;
    g_scrollMax[p.id] = ImGui::GetScrollMaxY();
    ImGui::End();
    g_heights[p.id] = h;
    return h;
}
float lastScrollMax(const std::string& id) { const auto it = g_scrollMax.find(id); return it == g_scrollMax.end() ? 0.0f : it->second; }
float lastHeight(const std::string& id, float dflt) { const auto it = g_heights.find(id); return it == g_heights.end() ? dflt : it->second; }

// ---- controls ----
namespace {
struct Hit { bool hovered, active, clicked; };
Hit hit(const char* id, ImVec2 pos, ImVec2 size) {
    ImGui::SetCursorScreenPos(pos);
    Hit h{};
    h.clicked = ImGui::InvisibleButton(id, size);
    h.hovered = ImGui::IsItemHovered();
    h.active = ImGui::IsItemActive();
    noteItem(id);
    if (h.hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    return h;
}
float rc() { return ui::corner(static_cast<float>(g_f.m.radiusCtl)); }
}  // namespace

// (AllowWhenBlockedByActiveItem: while a button inside the rectangle is held, the rectangle is still hovered, so the button is still there when it is let go)
// Every hover paint of the new UI goes through here (stage 5c.1): it is a fill of the widget's own rounded rectangle (or inside it), never a halo round it; the widget's rectangle and
// what was painted are logged for the automatic checks (tests/check_controls.py: within app::shell::kHaloMaxPx).
static void hoverFill(const char* id, ImDrawList* dl, ImVec2 w0, ImVec2 w1, ImVec2 a, ImVec2 b, ImU32 col, float r) {
    dl->AddRectFilled(a, b, col, r);
    const float edge = g_f.outline ? 0.5f : 0.0f;                               // Monochrome / High contrast: the 1 px outline is centred on the widget's edge
    auditHalo(std::string("hover.") + id, w0, w1, ImVec2(std::min(a.x, w0.x - edge), std::min(a.y, w0.y - edge)), ImVec2(std::max(b.x, w1.x + edge), std::max(b.y, w1.y + edge)));
}

bool hoveredRect(ImVec2 pos, ImVec2 size) { return ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseHoveringRect(pos, ImVec2(pos.x + size.x, pos.y + size.y)); }

bool button(const char* id, ImVec2 pos, ImVec2 size, const std::string& label, Face face, bool selected) {
    const Hit h = hit(id, pos, size);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 q(pos.x + size.x, pos.y + size.y);
    const bool filled = face == Face::Primary || selected;
    ImU32 bg = filled ? cFill() : face == Face::Quiet ? (h.hovered ? cFaceHover() : 0) : h.active ? cFacePress() : h.hovered ? cFaceHover() : cFace();
    if (filled && h.hovered) bg = ui::col(app::mixRgb(g_f.c->accentFill, g_f.c->fg, 0.12f));
    if (bg && h.hovered) hoverFill(id, dl, pos, q, pos, q, bg, rc());
    else if (bg) dl->AddRectFilled(pos, q, bg, rc());
    if (face == Face::Secondary && !filled) dl->AddRect(pos, q, g_f.outline ? cEdge() : colOf(g_f.c->fg, 0.14f), rc(), 0, 1.0f);   // 0.38: a secondary button has a 1 px edge
    else if (g_f.outline && face != Face::Quiet) dl->AddRect(pos, q, cEdge(), rc(), 0, 1.0f);
    const float tw = width(Type::Body, false, label);
    text(dl, Type::Body, false, ImVec2(pos.x + (size.x - tw) / 2, pos.y + (size.y - px(Type::Body)) / 2), filled ? cOnFill() : cText(), label);
    return h.clicked;
}

// 0.47.3: a button that cannot be used here (owner decision D5: an atom with no jumps): drawn greyed, never clicked; noted for the automatic checks as a hit area like the others
void disabledButton(const char* id, ImVec2 pos, ImVec2 size, const std::string& label) {
    (void)hit(id, pos, size);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 q(pos.x + size.x, pos.y + size.y);
    dl->AddRect(pos, q, colOf(g_f.c->fg, 0.14f), rc(), 0, 1.0f);
    const float tw = width(Type::Body, false, label);
    text(dl, Type::Body, false, ImVec2(pos.x + (size.x - tw) / 2, pos.y + (size.y - px(Type::Body)) / 2), colOf(g_f.c->fg, 0.45f), label);
}

bool toolToggle(const char* id, ImVec2 pos, ImVec2 size, const std::string& label, bool on) {
    const Hit h = hit(id, pos, size);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 q(pos.x + size.x, pos.y + size.y);
    if (on) dl->AddRectFilled(pos, q, cLitFill(), rc());                       // as iconButton's lit pill (5q: every theme, towards the text colour)
    else if (h.hovered) hoverFill(id, dl, pos, q, pos, q, cFaceHover(), rc());
    const float tw = width(Type::Body, false, label);
    text(dl, Type::Body, false, ImVec2(pos.x + (size.x - tw) / 2, pos.y + (size.y - px(Type::Body)) / 2), cText(), label);
    return h.clicked;
}

bool iconButton(const char* id, ImVec2 pos, ImVec2 size, Icon icon, const char* tip, bool on, float caretW) {
    const Hit h = hit(id, pos, size);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 q(pos.x + size.x, pos.y + size.y);
    const bool pill = on;                                                       // 5q: every theme: a lit button is a subtle fill with a full-strength icon (see cLitFill); a panel button's caret is drawn muted on it
    if (pill) dl->AddRectFilled(pos, q, cLitFill(), rc());
    else if (h.hovered) hoverFill(id, dl, pos, q, pos, q, cIconHover(), rc());
    if (g_f.outline && !pill) dl->AddRect(pos, q, cEdge(), rc(), 0, 1.0f);      // (Monochrome / High contrast draw a box round every plain button; the lit one has none: its fill says it)
    drawIcon(dl, icon, ImVec2(pos.x + (size.x - caretW) / 2, pos.y + size.y / 2), static_cast<float>(app::shell::unitPx(20, g_f.dpi, g_f.tf)), pill ? cLitIcon() : !h.hovered ? cIdleIcon() : cText());   // (stage 5j looks: muted, bright on hover; 5q: in every theme)
    if (caretW > 0) drawIcon(dl, on ? Icon::CaretUp : Icon::Caret, ImVec2(pos.x + size.x - caretW / 2 - g_f.m.s4 * 0.5f, pos.y + size.y / 2), caretW * 0.75f, cMuted());
    if (h.hovered && tip && *tip) ui::SetTooltip("%s", tip);
    return h.clicked;
}

bool iconWordButton(const char* id, ImVec2 pos, ImVec2 size, Icon icon, const std::string& word, const char* tip, bool on) {
    const Hit h = hit(id, pos, size);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 q(pos.x + size.x, pos.y + size.y);
    if (on) dl->AddRectFilled(pos, q, cLitFill(), rc());                       // as iconButton: a lit button is a subtle fill with a full-strength icon
    else if (h.hovered) hoverFill(id, dl, pos, q, pos, q, cIconHover(), rc());
    if (g_f.outline && !on) dl->AddRect(pos, q, cEdge(), rc(), 0, 1.0f);
    const float sq = size.y;
    const ImU32 ic = on ? cLitIcon() : !h.hovered ? cIdleIcon() : cText();
    drawIcon(dl, icon, ImVec2(pos.x + sq / 2, pos.y + size.y / 2), static_cast<float>(app::shell::unitPx(20, g_f.dpi, g_f.tf)), ic);
    text(dl, Type::Body, false, ImVec2(pos.x + sq - g_f.m.s4, pos.y + (size.y - px(Type::Body)) / 2), ic, word);   // (the word in the icon's colour: lit and unlit stay 3:1 apart, the 5p rule)
    if (h.hovered && tip && *tip) ui::SetTooltip("%s", tip);
    return h.clicked;
}

float iconWordWidth(const std::string& word) { return static_cast<float>(g_f.m.toolBtn) - g_f.m.s4 + width(Type::Body, false, word) + g_f.m.s12; }

// ---- 0.25: the "closest to reality" mark ----
static int g_realPart = -1;
static std::string g_realWhy;
void markReal(int part, const std::string& why) { g_realPart = part; g_realWhy = why; }
// the dot: centre c, in the accent colour (on a selected part: the selected text colour, so it stays seen); hovered, it says why. Noted as "real.<id>" for the checks, with whether
// the control's current value is the realistic one
static void drawRealDot(const char* id, ImDrawList* dl, ImVec2 c, ImU32 col, bool chosen, const std::string& why, bool ring = false) {
    const float r = std::max(2.0f, 3.0f * static_cast<float>(g_f.dpi));
    // 0.38: a small square (the owner: no round UI); a check box best left unticked keeps an outline, not a filled mark, so it does not read as "tick this"
    if (ring) dl->AddRect(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col, 0, 0, std::max(1.0f, 1.25f * static_cast<float>(g_f.dpi)));
    else dl->AddRectFilled(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r), col, std::min(1.0f, r / 3));
    const float hr = r + 4.0f * static_cast<float>(g_f.dpi);
    if (ImGui::IsWindowHovered() && std::fabs(ImGui::GetIO().MousePos.x - c.x) <= hr && std::fabs(ImGui::GetIO().MousePos.y - c.y) <= hr) {
        ui::BeginTooltip(); ImGui::PushTextWrapPos(ImGui::GetFontSize() * 24); ImGui::TextUnformatted(("Closest to reality: " + why).c_str()); ImGui::PopTextWrapPos(); ImGui::EndTooltip();
    }
    if (shellLogOn() && ImGui::IsRectVisible(ImVec2(c.x - r, c.y - r), ImVec2(c.x + r, c.y + r))) { auditRect(std::string("real.") + id, c.x - r, c.y - r, 2 * r, 2 * r); auditNote(std::string("realmark.") + id + (chosen ? " on" : " off")); }
}

bool checkbox(const char* id, ImVec2 pos, float w, const std::string& label, bool* v) {
    const int realPart = g_realPart; const std::string realWhy = g_realWhy; g_realPart = -1;
    const float rowH = static_cast<float>(g_f.m.row), box = static_cast<float>(g_f.m.check);
    const Hit h = hit(id, pos, ImVec2(w, rowH));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 b0(pos.x, pos.y + (rowH - box) / 2), b1(b0.x + box, b0.y + box);
    const float r = ui::corner(3.0f * g_f.dpi);                                  // (0.38: 3 px)
    // 0.38: a ticked box is the accent fill with its on-fill tick (the owner's new look; 5t made it grey)
    if (*v) { const ImU32 on = h.hovered ? colOf(app::mixRgb(g_f.c->accentFill, g_f.c->fg, 0.12f)) : cFill(); if (h.hovered) hoverFill(id, dl, pos, ImVec2(pos.x + w, pos.y + rowH), b0, b1, on, r); else dl->AddRectFilled(b0, b1, on, r); drawIcon(dl, Icon::Check, ImVec2((b0.x + b1.x) / 2, (b0.y + b1.y) / 2), box * 0.85f, cOnFill());
        if (shellLogOn() && ImGui::IsRectVisible(b0, b1)) auditRect(std::string("check.") + id + ".box", b0.x, b0.y, box, box); }   // 5t: for the automatic checks (where a checked box is)
    else { if (h.hovered) hoverFill(id, dl, pos, ImVec2(pos.x + w, pos.y + rowH), b0, b1, cFaceHover(), r); else dl->AddRectFilled(b0, b1, cFace(), r); dl->AddRect(b0, b1, cMuted(), r, 0, 1.5f); }
    text(dl, Type::Body, false, ImVec2(b1.x + g_f.m.s8, pos.y + (rowH - px(Type::Body)) / 2), cText(), label);
    if (h.clicked) *v = !*v;
    if (realPart >= 0) drawRealDot(id, dl, ImVec2(b1.x + g_f.m.s8 + width(Type::Body, false, label) + g_f.m.s8, pos.y + rowH / 2), colOf(g_f.c->accent), *v == (realPart == 1),
                                   (realPart == 1 ? "ticked. " : "unticked. ") + realWhy, realPart == 0);
    return h.clicked;
}

bool segmented(const char* id, ImVec2 pos, float w, const std::vector<std::string>& labels, int* cur, const std::vector<std::string>* names, bool bar) {
    const int realPart = g_realPart; const std::string realWhy = g_realWhy; g_realPart = -1;
    const bool lit = bar;                                                      // 5p (5q: every theme): the bottom toolbar's 2D / 3D segment is a toolbar button: it is lit like the others
    const float rowH = static_cast<float>(g_f.m.row);
    const Hit h = hit(id, pos, ImVec2(w, rowH));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const int n = static_cast<int>(labels.size());
    // equal parts; when a label does not fit its part (stage 5c: "Lines and names" in the 22 em Controls panel) the parts are as wide as their labels need, scaled to fill the row
    std::vector<float> edge(n + 1, 0.0f), need(n, 0.0f);
    float sum = 0;
    bool fits = true;
    for (int i = 0; i < n; i++) { need[i] = width(Type::Body, false, labels[i]) + 2.0f * g_f.m.s8; sum += need[i]; if (need[i] > w / n) fits = false; }
    for (int i = 0; i < n; i++) edge[i + 1] = edge[i] + (fits ? w / n : w * need[i] / sum);
    dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + rowH), cFace(), rc());
    if (g_f.outline) dl->AddRect(pos, ImVec2(pos.x + w, pos.y + rowH), cEdge(), rc(), 0, 1.0f);
    bool changed = false;
    if (h.clicked) {
        const float mx = ImGui::GetIO().MousePos.x - pos.x;
        int i = n - 1;
        for (int k = 0; k < n; k++) if (mx < edge[k + 1]) { i = k; break; }
        if (i != *cur) { *cur = i; changed = true; }
    }
    for (int i = 0; i < n; i++) {
        const ImVec2 a(pos.x + edge[i], pos.y), b(pos.x + edge[i + 1], pos.y + rowH);
        if (names && i < static_cast<int>(names->size())) noteRect((*names)[i].c_str(), a.x, a.y, b.x - a.x, b.y - a.y);
        const bool on = i == *cur;
        if (on && lit) dl->AddRectFilled(ImVec2(a.x + 2, a.y + 2), ImVec2(b.x - 2, b.y - 2), cLitFill(), std::max(0.0f, rc() - 1));
        else if (on) { dl->AddRectFilled(ImVec2(a.x + 2, a.y + 2), ImVec2(b.x - 2, b.y - 2), cSelTint(), std::max(0.0f, rc() - 1));   // 0.38: the soft accent tint with a thin accent edge
                       dl->AddRect(ImVec2(a.x + 2, a.y + 2), ImVec2(b.x - 2, b.y - 2), cSelEdge(), std::max(0.0f, rc() - 1), 0, 1.0f); }
        if (shellLogOn() && !bar && ImGui::IsRectVisible(a, b)) auditRect(std::string("seg.") + id + (on ? ".sel" : ".off") + std::to_string(i), a.x + 2, a.y + 2, b.x - a.x - 4, rowH - 4);   // 5t: for the automatic checks
        else if (h.hovered && ImGui::GetIO().MousePos.x >= a.x && ImGui::GetIO().MousePos.x < b.x) hoverFill(id, dl, pos, ImVec2(pos.x + w, pos.y + rowH), ImVec2(a.x + 2, a.y + 2), ImVec2(b.x - 2, b.y - 2), cFaceHover(), std::max(0.0f, rc() - 1));
        const float tw = width(Type::Body, false, labels[i]);
        text(dl, Type::Body, false, ImVec2(a.x + (b.x - a.x - tw) / 2, pos.y + (rowH - px(Type::Body)) / 2), on ? (lit ? cLitIcon() : cSelText()) : (lit ? cIdleIcon() : cMuted()), labels[i]);   // (0.38: the parts not chosen are muted, the chosen one the text colour)
        if (i == realPart) {                                                    // the dot in the part's top-right corner
            const float r = std::max(2.0f, 3.0f * static_cast<float>(g_f.dpi)), in = 3.0f + r + 3.0f * static_cast<float>(g_f.dpi);   // (0.38.1: 3 px from the chosen part's edge, was 2: it touched the 1 px accent edge)
            drawRealDot(id, dl, ImVec2(b.x - in, a.y + in), lit ? (on ? cLitIcon() : cIdleIcon()) : on && g_f.outline ? cSelText() : colOf(g_f.c->accent), on, realWhy);   // (a toolbar segment: its labels' colours, so the lit part stays the brightest)
        }
    }
    return changed;
}

static int g_sliderHoverFrame = -10;
bool sliderHoveredRecently() { return g_sliderHoverFrame >= ImGui::GetFrameCount() - 1; }
bool slider(const char* id, ImVec2 pos, float w, float* v, float lo, float hi) {
    const float rowH = static_cast<float>(g_f.m.row), pad = 2.0f, grab = ImGui::GetStyle().GrabMinSize;     // (Dear ImGui's own slider: 2 px of padding and a grab of GrabMinSize; the same mapping, so one value means one place)
    const Hit h = hit(id, pos, ImVec2(w, rowH));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float x0 = pos.x + pad + grab / 2, x1 = pos.x + w - pad - grab / 2;
    bool changed = false;
    if (h.active && x1 > x0) {                                                  // the value follows the pointer while it is down (and on the press)
        const float t = std::min(1.0f, std::max(0.0f, (ImGui::GetIO().MousePos.x - x0) / (x1 - x0)));
        const float nv = lo + t * (hi - lo);
        if (nv != *v) { *v = nv; changed = true; }
    }
    if (h.hovered) {                                                             // arrow keys and the wheel move it by a fiftieth
        g_sliderHoverFrame = ImGui::GetFrameCount();
        const float step = (hi - lo) / 50;
        const float wheel = ImGui::GetIO().MouseWheel;
        float nv = *v;
        const bool keys = !ImGui::GetIO().WantTextInput;                         // 0.50.3 (outside review): not while typing in a text box
        if (keys && ImGui::IsKeyPressed(ImGuiKey_RightArrow)) nv += step;
        if (keys && ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) nv -= step;
        if (wheel != 0) nv += wheel * step;
        nv = std::min(hi, std::max(lo, nv));
        if (nv != *v) { *v = nv; changed = true; }
    }
    const float t = hi > lo ? std::min(1.0f, std::max(0.0f, (*v - lo) / (hi - lo))) : 0;
    const float cy = pos.y + rowH / 2, track = static_cast<float>(app::shell::unitPx(3, g_f.dpi, g_f.tf)), tx = x0 + t * (x1 - x0), rad = static_cast<float>(app::shell::unitPx(7, g_f.dpi, g_f.tf));
    dl->AddRectFilled(ImVec2(x0, cy - track / 2), ImVec2(x1, cy + track / 2), colOf(app::mixRgb(g_f.c->card, g_f.c->fg, 0.18f)), ui::corner(track / 2));
    if (shellLogOn() && ImGui::IsRectVisible(ImVec2(x0, cy - rad), ImVec2(x1, cy + rad))) { auditRect(std::string("slider.") + id + ".fill", x0, cy - track / 2, tx - x0, track); auditRect(std::string("slider.") + id + ".knob", tx - rad, cy - rad, 2 * rad, 2 * rad); }   // 5q: for the automatic checks (where the grey fill and knob are)
    dl->AddRectFilled(ImVec2(x0, cy - track / 2), ImVec2(tx, cy + track / 2), cSliderFill(), ui::corner(track / 2));
    // 0.38: the thumb is an upright bar (12 x 18 u, 3 px corners), light, with a dark 1 px edge so it reads on any track (the owner's new look; it was a round knob)
    const float kw = static_cast<float>(app::shell::unitPx(6, g_f.dpi, g_f.tf)), kh = static_cast<float>(app::shell::unitPx(9, g_f.dpi, g_f.tf));
    dl->AddRectFilled(ImVec2(tx - kw, cy - kh), ImVec2(tx + kw, cy + kh), cSliderKnob(h.active || h.hovered), ui::corner(3.0f * static_cast<float>(g_f.dpi)));
    dl->AddRect(ImVec2(tx - kw, cy - kh), ImVec2(tx + kw, cy + kh), relLum(g_f.c->card) < 0.18f ? IM_COL32(0, 0, 0, 160) : colOf(app::mixRgb(g_f.c->card, g_f.c->fg, 0.45f)), ui::corner(3.0f * static_cast<float>(g_f.dpi)), 0, 1.0f);
    return changed;
}

bool row(const char* id, ImVec2 pos, float w, const std::string& label, bool selected, const char* dotHex) {
    const float rowH = static_cast<float>(g_f.m.row);
    const Hit h = hit(id, pos, ImVec2(w, rowH));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // stage 5j looks: a selected row is ONE solid pill (fill colour, dark / white text, regular weight); a section header of the Controls panel is small muted text; hover = a soft pill
    const bool sectionHead = std::strncmp(id, "ctl.sec.", 8) == 0;
    if (selected) { dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + rowH), cSelTint(), rc()); dl->AddRect(pos, ImVec2(pos.x + w, pos.y + rowH), cSelEdge(), rc(), 0, 1.0f); }   // 0.38: as a chosen segment
    else if (h.hovered) hoverFill(id, dl, pos, ImVec2(pos.x + w, pos.y + rowH), pos, ImVec2(pos.x + w, pos.y + rowH), cFaceHover(), rc());
    else if (selected) dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + rowH), cFace(), rc());
    float x = pos.x + g_f.m.s8;
    if (dotHex) {
        const app::Rgb c = app::hexRgb(dotHex);
        const float d = static_cast<float>(g_f.m.s12);
        dl->AddCircleFilled(ImVec2(x + d / 2, pos.y + rowH / 2), d / 2, ui::col(c), 16);
        x += d + g_f.m.s8;
    }
    text(dl, Type::Body, false, ImVec2(x, pos.y + (rowH - px(Type::Body)) / 2), selected ? cSelText() : sectionHead ? cMuted() : cText(), label);
    return h.clicked;
}

// ---- flowing content ----
ImVec2 here(const Panel& p) { return ImVec2(p.origin.x, p.origin.y + p.y); }
void pGap(Panel& p, int px) { p.y += static_cast<float>(px); p.gap += static_cast<float>(px); }
void pTitle(Panel& p, const std::string& s) { text(p.dl, Type::Title, true, here(p), cText(), s); p.y += lineH(Type::Title) + g_f.m.s4; p.gap = g_f.m.s4; }
void pHeading(Panel& p, const std::string& s) { text(p.dl, Type::Caption, false, here(p), cMuted(), s); p.y += lineH(Type::Caption) + g_f.m.s4; p.gap = g_f.m.s4; }
void pCaption(Panel& p, const std::string& s, bool muted) { p.y += paragraph(p.dl, Type::Caption, false, here(p), p.w, muted ? cMuted() : cText(), s) + g_f.m.s4; p.gap = g_f.m.s4; }
void pBody(Panel& p, const std::string& s, bool muted) { p.y += paragraph(p.dl, Type::Body, false, here(p), p.w, muted ? cMuted() : cText(), s) + g_f.m.s4; p.gap = g_f.m.s4; }
void pText(Panel& p, Type t, bool bold, ImU32 col, const std::string& s) { p.y += paragraph(p.dl, t, bold, here(p), p.w, col, s) + g_f.m.s4; p.gap = g_f.m.s4; }
void pStat(Panel& p, const std::string& label, const std::string& value) {
    const float lw = width(Type::Body, false, label), vw = width(Type::Body, false, value), gap = static_cast<float>(g_f.m.s12);
    if (lw + vw + gap <= p.w) {
        const ImVec2 o = here(p);
        text(p.dl, Type::Body, false, o, cMuted(), label);
        text(p.dl, Type::Body, false, ImVec2(o.x + p.w - vw, o.y), cText(), value);
        p.y += lineH(Type::Body) + g_f.m.s4;
    } else {
        text(p.dl, Type::Body, false, here(p), cMuted(), label);
        p.y += lineH(Type::Body);
        p.y += paragraph(p.dl, Type::Body, false, here(p), p.w, cText(), value) + g_f.m.s4;
    }
    p.gap = g_f.m.s4;
}
bool pCheck(Panel& p, const char* id, const std::string& label, bool* v) { const bool r = checkbox(id, here(p), p.w, label, v); p.y += g_f.m.row; p.gap = 0; return r; }
bool pSegmented(Panel& p, const char* id, const std::vector<std::string>& labels, int* cur, const std::vector<std::string>* names) { const bool r = segmented(id, here(p), p.w, labels, cur, names); p.y += g_f.m.row + g_f.m.s4; p.gap = g_f.m.s4; return r; }
bool pSlider(Panel& p, const char* id, float* v, float lo, float hi) { const bool r = slider(id, here(p), p.w, v, lo, hi); p.y += g_f.m.row; p.gap = 0; return r; }
bool pButton(Panel& p, const char* id, const std::string& label, Face face, bool selected) { const bool r = button(id, here(p), ImVec2(p.w, static_cast<float>(g_f.m.control)), label, face, selected); p.y += g_f.m.control + g_f.m.s4; p.gap = g_f.m.s4; return r; }
bool pRow(Panel& p, const char* id, const std::string& label, bool selected, const char* dotHex) { const float in = g_bodyFloor ? static_cast<float>(g_f.m.s8) : 0.0f;   // (stage 5c.1: in the Controls panel its words sit on the content's left edge; the hover runs into the padding)
    const bool r = row(id, ImVec2(here(p).x - in, here(p).y), p.w + 2 * in, label, selected, dotHex); p.y += g_f.m.row; p.gap = 0; return r; }
bool pRowPart(Panel& p, const char* id, float x, float w, const std::string& label, bool selected, const char* dotHex) {
    const float in = g_bodyFloor ? static_cast<float>(g_f.m.s8) : 0.0f;
    const float l = x <= 0 ? in : 0.0f, r = x + w >= p.w ? in : 0.0f;
    return row(id, ImVec2(here(p).x + x - l, here(p).y), w + l + r, label, selected, dotHex);
}
void pRowEnd(Panel& p) { p.y += g_f.m.row; p.gap = 0; }
void pRule(Panel& p) { const ImVec2 a = here(p); p.dl->AddLine(ImVec2(a.x, a.y + g_f.m.s4), ImVec2(a.x + p.w, a.y + g_f.m.s4), cHair()); p.y += 2 * g_f.m.s4 + 1; p.gap = 0; }

}  // namespace ui::sh
