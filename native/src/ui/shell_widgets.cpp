#include "ui/shell_widgets.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <map>

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
std::string g_glassId;
double g_glassA = 0.95;

const char* typeName(Type t) { return t == Type::Caption ? "caption" : t == Type::Body ? "body" : t == Type::Title ? "title" : "key"; }
ImU32 colOf(const Rgb& c, float a = 1) { return ui::col(c, a); }
float thick() { return std::max(1.5f, static_cast<float>(1.75 * g_f.dpi * g_f.tf * 0.9)); }       // the icon stroke: 1.75 u
}  // namespace

void beginFrame() {
    g_f.dpi = ImGui::GetStyle().FontScaleDpi;
    g_f.tf = std::round(look().textScale * 1000.0) / 1000.0;                     // 1.3f is 1.29999995: 15 x it rounds to 19, not 20 (the 19.5 of DESIGN.md)
    g_f.m = app::shell::metrics(g_f.dpi, g_f.tf);
    g_f.c = &look().c;
    const app::ThemeColors& c = look().c;
    g_f.plain = app::isPlainStyle(c.style);
    g_f.outline = c.style.square || app::contrastRatio(c.line, c.bg) >= 7;          // Monochrome and High contrast
    const std::string key = c.id + (look().effects ? "+" : "-");
    if (key != g_glassId) { g_glassId = key; g_glassA = app::shell::glassOpacity(c); }
    g_f.panelAlpha = static_cast<float>(g_glassA);
}
const Frame& F() { return g_f; }

ImU32 cText(float a) { return colOf(g_f.c->fg, a); }
ImU32 cMuted(float a) { return colOf(g_f.c->muted, a); }
ImU32 cAccent(float a) { return colOf(g_f.c->accent, a); }
ImU32 cFill(float a) { return colOf(g_f.c->accentFill, a); }
ImU32 cOnFill() { return colOf(app::shell::onAccent(*g_f.c)); }
static Rgb faceRgb() { const app::ThemeColors& c = *g_f.c; return c.style.button.set ? c.button : app::mixRgb(c.card, c.fg, 0.10f); }
ImU32 cFace(float a) { return colOf(faceRgb(), a); }
ImU32 cFaceHover() { return colOf(app::mixRgb(faceRgb(), g_f.c->accent, 0.18f)); }
ImU32 cFacePress() { return colOf(app::mixRgb(faceRgb(), g_f.c->accent, 0.30f)); }
ImU32 cEdge() { return g_f.outline ? colOf(g_f.c->fg) : colOf(g_f.c->fg, app::shell::kEdgeAlpha); }
ImU32 cHair() { return colOf(g_f.c->line); }
ImU32 cPanel() { return colOf(g_f.c->card, g_f.panelAlpha); }
ImU32 cBackdrop(float a) { return colOf(g_f.c->bg, a); }
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
            if (!cur.empty() && width(t, bold, tryLine) > maxW) { lines.push_back(cur); cur = w; } else cur = tryLine;
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
    const float s = size / 2, th = thick();
    auto P = [&](float x, float y) { return ImVec2(c.x + x * s, c.y + y * s); };
    switch (icon) {
    case Icon::Menu: for (float y : {-0.55f, 0.0f, 0.55f}) dl->AddLine(P(-0.75f, y), P(0.75f, y), col, th); break;
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
    case Icon::Trophy: {
        const ImVec2 cup[5] = {P(-0.55f, -0.65f), P(0.55f, -0.65f), P(0.42f, 0.1f), P(0.0f, 0.36f), P(-0.42f, 0.1f)};
        dl->AddPolyline(cup, 5, col, ImDrawFlags_Closed, th);
        dl->AddLine(P(0, 0.36f), P(0, 0.7f), col, th); dl->AddLine(P(-0.36f, 0.7f), P(0.36f, 0.7f), col, th);
        break;
    }
    case Icon::Bulb: dl->AddCircle(P(0, -0.15f), 0.52f * s, col, 20, th); dl->AddLine(P(-0.25f, 0.55f), P(0.25f, 0.55f), col, th); dl->AddLine(P(-0.18f, 0.8f), P(0.18f, 0.8f), col, th); break;
    case Icon::Palette:
        dl->AddCircle(c, 0.82f * s, col, 24, th);
        dl->AddCircleFilled(P(-0.3f, -0.2f), 0.12f * s, col); dl->AddCircleFilled(P(0.1f, -0.38f), 0.12f * s, col); dl->AddCircleFilled(P(0.38f, 0.0f), 0.12f * s, col);
        break;
    case Icon::Gear:
        dl->AddCircle(c, 0.42f * s, col, 16, th);
        for (int k = 0; k < 8; k++) { const float a = k * 0.785398f, ca = std::cos(a), sa = std::sin(a); dl->AddLine(P(0.62f * ca, 0.62f * sa), P(0.92f * ca, 0.92f * sa), col, th * 1.2f); }
        break;
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
    const float pad = shape == Shape::Panel ? static_cast<float>(f.m.s16) : 0.0f;
    const float rounding = shape == Shape::Bar || shape == Shape::Overlay || shape == Shape::Clear ? 0.0f : shape == Shape::Pill ? static_cast<float>(r.h / 2) : ui::corner(static_cast<float>(f.m.radiusPanel));
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(r.x), static_cast<float>(r.y)), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(r.w), static_cast<float>(r.h)), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, rounding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1, 1));             // (stage 5c.1: Dear ImGui's 32 px minimum made the 24 u handle wider than its place)
    const bool docked = !f.plain && shape == Shape::Bar;                       // Aero / Y2K: the top bar is a docked panel, so the page behind it comes from the stage 2 page cache
    if (docked) pushDockedBg();
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));              // the glass is drawn by hand below
    const bool shown = ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                    (scroll && shape == Shape::Panel ? 0 : ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse) | (shape == Shape::Panel ? 0 : ImGuiWindowFlags_NoBringToFrontOnFocus) |
                                    (shape == Shape::Overlay ? ImGuiWindowFlags_NoInputs : 0));
    ImGui::PopStyleColor(); ImGui::PopStyleVar(4); if (docked) popDockedBg();
    if (!shown) { ImGui::End(); return false; }
    p.dl = ImGui::GetWindowDrawList();
    const ImVec2 a = ImGui::GetWindowPos(), b(a.x + ImGui::GetWindowWidth(), a.y + ImGui::GetWindowHeight());
    float reach = 0;                                                            // how far a shadow or glow reaches outside the panel (stage 5c.1: at most app::shell::kHaloMaxPx)
    if (docked) panelDecor(app::PanelKind::Docked);
    else if (!f.plain && shape != Shape::Overlay && shape != Shape::Clear) reach = panelDecor(app::PanelKind::Floating, true);   // Aero and Y2K: their own glass, rim and textures; the glow hugs the edge
    if (shape == Shape::Overlay || shape == Shape::Clear) {
    } else if (f.plain) {
        if (shape != Shape::Bar) {                                              // a soft edge of at most kHaloMaxPx round the panel (black at low opacity, two layers), only outside its own edge (stage 5c.1:
            const float big = static_cast<float>(app::shell::kHaloMaxPx);         // it was a 16 u drop shadow that spread over the neighbours)
            const ImVec2 outside[4][2] = {{ImVec2(a.x - big, a.y - big), ImVec2(b.x + big, a.y)}, {ImVec2(a.x - big, b.y), ImVec2(b.x + big, b.y + big)},
                                          {ImVec2(a.x - big, a.y), ImVec2(a.x, b.y)}, {ImVec2(b.x, a.y), ImVec2(b.x + big, b.y)}};
            for (const auto& o : outside) {                                     // only outside the panel: under the glass it would show through and make it darker than its opacity says
                p.dl->PushClipRect(o[0], o[1], false);
                for (int k = 2; k >= 1; k--) p.dl->AddRectFilled(ImVec2(a.x - k * big * 0.5f, a.y - k * big * 0.5f), ImVec2(b.x + k * big * 0.5f, b.y + k * big * 0.5f), IM_COL32(0, 0, 0, 20), rounding + k * big * 0.5f);
                p.dl->PopClipRect();
            }
            reach = big;
        }
        p.dl->AddRectFilled(a, b, solid ? colOf(f.c->card) : cPanel(), rounding);   // (stage 5c.1: the Controls panel is solid: the picture's labels showed through it)
        const float shine = static_cast<float>(f.m.s12 * 3 + f.m.s4);          // a faint top-lit gradient: the text colour at 6 % fading out over about 40 u
        const ImU32 top = ui::col(f.c->fg, app::shell::kShineAlpha), none = ui::col(f.c->fg, 0);
        p.dl->PushClipRect(a, ImVec2(b.x, std::min(b.y, a.y + shine)), true);
        p.dl->AddRectFilledMultiColor(ImVec2(a.x + rounding * 0.5f, a.y), ImVec2(b.x - rounding * 0.5f, std::min(b.y, a.y + shine)), top, top, none, none);
        p.dl->PopClipRect();
        if (shape == Shape::Bar) p.dl->AddLine(ImVec2(a.x, b.y - 0.5f), ImVec2(b.x, b.y - 0.5f), cEdge(), 1.0f);
        else p.dl->AddRect(a, b, cEdge(), rounding, 0, 1.0f);
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
    const float pad = ImGui::GetStyle().WindowPadding.x;
    const float h = p.y + 1 + 2 * pad;
    ImGui::End();
    g_heights[p.id] = h;
    return h;
}
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
    if (g_f.outline && face != Face::Quiet) dl->AddRect(pos, q, cEdge(), rc(), 0, 1.0f);
    const float tw = width(Type::Body, false, label);
    text(dl, Type::Body, false, ImVec2(pos.x + (size.x - tw) / 2, pos.y + (size.y - px(Type::Body)) / 2), filled ? cOnFill() : cText(), label);
    return h.clicked;
}

bool iconButton(const char* id, ImVec2 pos, ImVec2 size, Icon icon, const char* tip, bool on, float caretW) {
    const Hit h = hit(id, pos, size);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 q(pos.x + size.x, pos.y + size.y);
    if (on) dl->AddRectFilled(pos, q, cFacePress(), rc());
    else if (h.hovered) hoverFill(id, dl, pos, q, pos, q, cFaceHover(), rc());
    if (g_f.outline) dl->AddRect(pos, q, cEdge(), rc(), 0, 1.0f);
    drawIcon(dl, icon, ImVec2(pos.x + (size.x - caretW) / 2, pos.y + size.y / 2), static_cast<float>(app::shell::unitPx(20, g_f.dpi, g_f.tf)), cText());
    if (caretW > 0) drawIcon(dl, on ? Icon::CaretUp : Icon::Caret, ImVec2(pos.x + size.x - caretW / 2 - g_f.m.s4 * 0.5f, pos.y + size.y / 2), caretW * 0.75f, cMuted());
    if (h.hovered && tip && *tip) ui::SetTooltip("%s", tip);
    return h.clicked;
}

bool checkbox(const char* id, ImVec2 pos, float w, const std::string& label, bool* v) {
    const float rowH = static_cast<float>(g_f.m.row), box = static_cast<float>(g_f.m.check);
    const Hit h = hit(id, pos, ImVec2(w, rowH));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 b0(pos.x, pos.y + (rowH - box) / 2), b1(b0.x + box, b0.y + box);
    const float r = ui::corner(3.0f * g_f.dpi);
    if (*v) { if (h.hovered) hoverFill(id, dl, pos, ImVec2(pos.x + w, pos.y + rowH), b0, b1, cFill(), r); else dl->AddRectFilled(b0, b1, cFill(), r); drawIcon(dl, Icon::Check, ImVec2((b0.x + b1.x) / 2, (b0.y + b1.y) / 2), box * 0.85f, cOnFill()); }
    else { if (h.hovered) hoverFill(id, dl, pos, ImVec2(pos.x + w, pos.y + rowH), b0, b1, cFaceHover(), r); else dl->AddRectFilled(b0, b1, cFace(), r); dl->AddRect(b0, b1, cMuted(), r, 0, 1.5f); }
    text(dl, Type::Body, false, ImVec2(b1.x + g_f.m.s8, pos.y + (rowH - px(Type::Body)) / 2), cText(), label);
    if (h.clicked) *v = !*v;
    return h.clicked;
}

bool segmented(const char* id, ImVec2 pos, float w, const std::vector<std::string>& labels, int* cur, const std::vector<std::string>* names) {
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
        if (on) dl->AddRectFilled(ImVec2(a.x + 2, a.y + 2), ImVec2(b.x - 2, b.y - 2), cFill(), std::max(0.0f, rc() - 1));
        else if (h.hovered && ImGui::GetIO().MousePos.x >= a.x && ImGui::GetIO().MousePos.x < b.x) hoverFill(id, dl, pos, ImVec2(pos.x + w, pos.y + rowH), ImVec2(a.x + 2, a.y + 2), ImVec2(b.x - 2, b.y - 2), cFaceHover(), std::max(0.0f, rc() - 1));
        const float tw = width(Type::Body, false, labels[i]);
        text(dl, Type::Body, false, ImVec2(a.x + (b.x - a.x - tw) / 2, pos.y + (rowH - px(Type::Body)) / 2), on ? cOnFill() : cText(), labels[i]);
    }
    return changed;
}

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
        const float step = (hi - lo) / 50;
        const float wheel = ImGui::GetIO().MouseWheel;
        float nv = *v;
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) nv += step;
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) nv -= step;
        if (wheel != 0) nv += wheel * step;
        nv = std::min(hi, std::max(lo, nv));
        if (nv != *v) { *v = nv; changed = true; }
    }
    const float t = hi > lo ? std::min(1.0f, std::max(0.0f, (*v - lo) / (hi - lo))) : 0;
    const float cy = pos.y + rowH / 2, track = static_cast<float>(app::shell::unitPx(4, g_f.dpi, g_f.tf)), tx = x0 + t * (x1 - x0), rad = static_cast<float>(app::shell::unitPx(7, g_f.dpi, g_f.tf));
    dl->AddRectFilled(ImVec2(x0, cy - track / 2), ImVec2(x1, cy + track / 2), cHair(), track / 2);
    dl->AddRectFilled(ImVec2(x0, cy - track / 2), ImVec2(tx, cy + track / 2), cAccent(), track / 2);
    dl->AddCircleFilled(ImVec2(tx, cy), rad, h.active || h.hovered ? cAccent() : cFill());
    dl->AddCircle(ImVec2(tx, cy), rad, cEdge(), 20, 1.0f);
    return changed;
}

bool row(const char* id, ImVec2 pos, float w, const std::string& label, bool selected, const char* dotHex) {
    const float rowH = static_cast<float>(g_f.m.row);
    const Hit h = hit(id, pos, ImVec2(w, rowH));
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (h.hovered) hoverFill(id, dl, pos, ImVec2(pos.x + w, pos.y + rowH), pos, ImVec2(pos.x + w, pos.y + rowH), cFaceHover(), rc());
    else if (selected) dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + rowH), cFace(), rc());
    float x = pos.x + g_f.m.s8;
    if (dotHex) {
        const app::Rgb c = app::hexRgb(dotHex);
        const float d = static_cast<float>(g_f.m.s12);
        dl->AddCircleFilled(ImVec2(x + d / 2, pos.y + rowH / 2), d / 2, ui::col(c), 16);
        x += d + g_f.m.s8;
    }
    text(dl, Type::Body, selected, ImVec2(x, pos.y + (rowH - px(Type::Body)) / 2), selected ? cAccent() : cText(), label);
    return h.clicked;
}

// ---- flowing content ----
ImVec2 here(const Panel& p) { return ImVec2(p.origin.x, p.origin.y + p.y); }
void pGap(Panel& p, int px) { p.y += static_cast<float>(px); p.gap += static_cast<float>(px); }
void pTitle(Panel& p, const std::string& s) { text(p.dl, Type::Title, true, here(p), cText(), s); p.y += lineH(Type::Title) + g_f.m.s4; p.gap = g_f.m.s4; }
void pHeading(Panel& p, const std::string& s) { text(p.dl, Type::Caption, false, here(p), cMuted(), s); p.y += lineH(Type::Caption) + g_f.m.s4; p.gap = g_f.m.s4; }
void pCaption(Panel& p, const std::string& s, bool muted) { p.y += paragraph(p.dl, Type::Caption, false, here(p), p.w, muted ? cMuted() : cText(), s) + g_f.m.s4; p.gap = g_f.m.s4; }
void pBody(Panel& p, const std::string& s, bool muted) { p.y += paragraph(p.dl, Type::Body, false, here(p), p.w, muted ? cMuted() : cText(), s) + g_f.m.s4; p.gap = g_f.m.s4; }
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
void pRule(Panel& p) { const ImVec2 a = here(p); p.dl->AddLine(ImVec2(a.x, a.y + g_f.m.s4), ImVec2(a.x + p.w, a.y + g_f.m.s4), cHair()); p.y += 2 * g_f.m.s4 + 1; p.gap = 0; }

}  // namespace ui::sh
