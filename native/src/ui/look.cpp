#include "look.h"
#include "app/look_options.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <cmath>
#include <cstdlib>
#include <regex>
#include <vector>

#include "fonts.h"
#include "style_draw.h"

namespace ui {

static Look g_look;
static std::string g_applied;   // what was last applied to Dear ImGui (only redone when it changes)

const Look& look() { return g_look; }

bool systemReducedMotion() {
#ifdef _WIN32
    BOOL anim = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &anim, 0)) return !anim;
#endif
    return false;
}

ImVec4 vec(const app::Rgb& c, float a) { return ImVec4(c.r, c.g, c.b, a); }
ImU32 col(const app::Rgb& c, float a) { return ImGui::ColorConvertFloat4ToU32(vec(c, a)); }
app::Rgb mix(const app::Rgb& a, const app::Rgb& b, float f) { return {a.r + (b.r - a.r) * f, a.g + (b.g - a.g) * f, a.b + (b.b - a.b) * f}; }
float corner(float r) { return app::scaledRadius(g_look.c.style, r); }

void pushOn() {
    ImGui::PushStyleColor(ImGuiCol_Button, vec(g_look.w.accentFill));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, vec(g_look.w.accentFill));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, vec(g_look.w.accentFill));
    ImGui::PushStyleColor(ImGuiCol_Text, vec(g_look.w.onacc));
}
void popOn() { ImGui::PopStyleColor(4); }

// ---- the web's #gift pop-up page: a centred card with a big icon, a heading, short paragraphs and a row of buttons ----

bool beginGift(const char* id) {
    const float s = ImGui::GetFontSize() / 16;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x / 2, vp->Pos.y + vp->Size.y / 2), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(360 * s, vp->Size.x - 32), 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18 * s, 22 * s));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, corner(16 * s));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * s, 12 * s));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, vec(g_look.c.card));
    ImGui::PushStyleColor(ImGuiCol_Border, vec(g_look.c.line));
    const bool open = ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove);
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    if (open) panelDecor(app::PanelKind::Floating);
    return open;
}

namespace {
struct Run { std::string text; bool bold; };
// lines of words, wrapped to the window and centred (bold parts in the bold font)
void centred(const std::vector<Run>& runs, float size, ImU32 colour, ImU32 boldColour) {
    struct Word { std::string w; bool bold; float width; };
    const Fonts& F = fonts();
    const float maxW = ImGui::GetContentRegionAvail().x, space = F.regular->CalcTextSizeA(size, FLT_MAX, 0, " ").x;
    std::vector<std::vector<Word>> lines(1);
    std::vector<float> widths(1, 0);
    for (const Run& r : runs) {
        if (r.text == "\n") { lines.emplace_back(); widths.push_back(0); continue; }
        std::size_t a = 0;
        while (a < r.text.size()) {
            std::size_t b = r.text.find(' ', a);
            if (b == std::string::npos) b = r.text.size();
            const std::string w = displayable(r.text.substr(a, b - a), r.bold ? F.bold : F.regular);
            a = b + 1;
            if (w.empty()) continue;
            const float ww = (r.bold ? F.bold : F.regular)->CalcTextSizeA(size, FLT_MAX, 0, w.c_str()).x;
            if (!lines.back().empty() && widths.back() + space + ww > maxW) { lines.emplace_back(); widths.push_back(0); }
            widths.back() += (lines.back().empty() ? 0 : space) + ww;
            lines.back().push_back({w, r.bold, ww});
        }
    }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (std::size_t i = 0; i < lines.size(); i++) {
        const ImVec2 p = ImGui::GetCursorScreenPos();
        float x = p.x + (maxW - widths[i]) / 2;
        for (const Word& w : lines[i]) {
            dl->AddText(w.bold ? F.bold : F.regular, size, ImVec2(x, p.y), w.bold ? boldColour : colour, w.w.c_str());
            x += w.width + space;
        }
        ImGui::Dummy(ImVec2(maxW, size * 1.4f));
        if (i + 1 < lines.size()) ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);
    }
}
std::vector<Run> runsOf(const std::string& html) {        // <b> parts and <br> line breaks
    std::vector<Run> out;
    static const std::regex tag(R"(<(/?)(b|br)>)");
    bool bold = false;
    std::size_t at = 0;
    for (std::sregex_iterator it(html.begin(), html.end(), tag), end; it != end; ++it) {
        if (std::size_t(it->position()) > at) out.push_back({html.substr(at, it->position() - at), bold});
        if ((*it)[2] == "br") out.push_back({"\n", false});
        else bold = (*it)[1].length() == 0;
        at = it->position() + it->length();
    }
    if (at < html.size()) out.push_back({html.substr(at), bold});
    return out;
}
}  // namespace

// .prev: the little picture of the theme a secret unlocks (Synthwave: a neon sun over a grid; Monochrome: the same in black and white)
void giftPreview(bool mono) {
    const float s = ImGui::GetFontSize() / 16, w = ImGui::GetContentRegionAvail().x, h = 88 * s;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float r = corner(12 * s);
    dl->PushClipRect(p, ImVec2(p.x + w, p.y + h), true);
    if (mono) dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), IM_COL32(255, 255, 255, 255), r);
    else {
        dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), col(app::hexRgb("#120a26")), r);
        for (int i = 10; i >= 1; i--)                                          // radial-gradient(#3b1466 at the top, #120a26 at 70%)
            dl->AddEllipseFilled(ImVec2(p.x + w / 2, p.y), ImVec2(0.6f * w * 0.7f * i / 10, 0.9f * h * 0.7f * i / 10), col(app::hexRgb("#3b1466"), 0.12f));
    }
    // the sun: 48 px, top 12 px (a gradient from #ffd166 to #ff4fd8; Monochrome: a black square)
    const ImVec2 c(p.x + w / 2, p.y + 12 * s + 24 * s);
    if (mono) dl->AddRectFilled(ImVec2(c.x - 24 * s, c.y - 24 * s), ImVec2(c.x + 24 * s, c.y + 24 * s), IM_COL32(0, 0, 0, 255));
    else
        for (int k = 0; k < 24; k++) {                                         // horizontal slices of the circle
            const float y0 = c.y - 24 * s + k * 2 * s, y1 = y0 + 2 * s, ym = (y0 + y1) / 2 - c.y, hw = std::sqrt(std::max(0.f, 24 * s * 24 * s - ym * ym));
            dl->AddRectFilled(ImVec2(c.x - hw, y0), ImVec2(c.x + hw, y1), col(mix(app::hexRgb("#ffd166"), app::hexRgb("#ff4fd8"), (k + 0.5f) / 24)));
        }
    // the grid: the bottom 36 px
    const float gy = p.y + h - 36 * s;
    dl->AddRectFilled(ImVec2(p.x, gy), ImVec2(p.x + w, p.y + h), mono ? IM_COL32(230, 230, 230, 255) : col(app::hexRgb("#1b1035")));
    if (mono) for (float x = 0; x < w; x += 12 * s) dl->AddLine(ImVec2(p.x + x, gy), ImVec2(p.x + x, p.y + h), IM_COL32(0, 0, 0, 255));
    else {
        for (float y = 0; y < 36 * s; y += 9 * s) dl->AddLine(ImVec2(p.x, gy + y), ImVec2(p.x + w, gy + y), col(app::hexRgb("#ff4fd8")));
        for (float x = 0; x < w; x += 16 * s) dl->AddLine(ImVec2(p.x + x, gy), ImVec2(p.x + x, p.y + h), col(app::hexRgb("#ff4fd8"), 0.33f));
    }
    dl->AddLine(ImVec2(p.x, gy), ImVec2(p.x + w, gy), mono ? IM_COL32(0, 0, 0, 255) : col(app::hexRgb("#ff4fd8")));
    dl->PopClipRect();
    dl->AddRect(p, ImVec2(p.x + w, p.y + h), mono ? IM_COL32(0, 0, 0, 255) : col(app::hexRgb("#4a2a85")), r);
    ImGui::Dummy(ImVec2(w, h));
}

std::string giftPage(const std::string& html) {
    const float s = ImGui::GetFontSize() / 16 / g_look.textScale, ts = g_look.textScale;   // the web's px, times its text size
    static const std::regex block(R"rx(<div class="gi">(.*?)</div>|<h2[^>]*>(.*?)</h2>|<p( class="rstL")?>(.*?)</p>|<div class="row">(.*?)</div>|<div class="prev( mono)?">)rx");
    static const std::regex btn(R"rx(<button type="button"(?: class="(\w+)")? id="(\w+)">([^<]*)</button>)rx");
    std::string clicked;
    for (std::sregex_iterator it(html.begin(), html.end(), block), end; it != end; ++it) {
        const std::smatch& m = *it;
        if (m[1].matched) centred({{m[1], false}}, 44 * s * ts, col(g_look.c.fg), col(g_look.c.fg));
        else if (m[2].matched) centred({{m[2], true}}, 20 * s * ts, col(g_look.c.fg), col(g_look.c.fg));
        else if (m[6].matched || std::string(m[0]) == "<div class=\"prev\">") giftPreview(m[6].matched);
        else if (m[4].matched) {
            const bool list = m[3].matched;                // .rstL: the list of what gets locked, in the text colour
            centred(runsOf(m[4]), (list ? 15 : 14) * s * ts, col(list ? g_look.c.fg : g_look.c.muted), col(g_look.c.fg));
        } else {
            // the buttons, centred in one row
            const std::string row = m[5];
            struct B { std::string cls, id, label; float w; };
            std::vector<B> bs;
            float total = 0;
            for (std::sregex_iterator b(row.begin(), row.end(), btn), e; b != e; ++b) {
                const std::string label = displayable((*b)[3]);
                const float w = ImGui::CalcTextSize(label.c_str()).x + 2 * ImGui::GetStyle().FramePadding.x;
                bs.push_back({(*b)[1], (*b)[2], label, w});
                total += w + (bs.size() > 1 ? 8 * s : 0);
            }
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.f, (ImGui::GetContentRegionAvail().x - total) / 2));
            for (std::size_t i = 0; i < bs.size(); i++) {
                if (i) ImGui::SameLine(0, 8 * s);
                const bool primary = bs[i].cls == "primary", danger = bs[i].cls == "setDanger";
                if (primary) pushOn();
                if (danger) {                               // .setDanger: red with white text
                    const ImVec4 red = vec(g_look.c.dangerFill);
                    ImGui::PushStyleColor(ImGuiCol_Button, red);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, red);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, red);
                    ImGui::PushStyleColor(ImGuiCol_Text, vec(g_look.c.onDanger));
                }
                if (ui::Button(bs[i].label.c_str())) clicked = bs[i].id;
                if (primary || danger) ImGui::PopStyleColor(4);
            }
        }
    }
    return clicked;
}

void invalidateLook() { g_applied.clear(); }   // 0.15 stage 2: the Dear ImGui style was rebuilt (a new display scale): the next updateLook puts the theme back

void updateLook(const app::Prefs& p, bool systemLight, bool systemReduced) {
    Look L;
    L.c = app::themeColors(app::themeUsable(p, p.theme) ? p.theme : "auto", systemLight);
    L.textScale = float(app::textSizeFactor(p.textSize));
    L.reduced = p.motion == "on" || (p.motion == "auto" && systemReduced);
    const app::ThemeStyle& ts = L.c.style;                     // 0.14: what used to be tested as `id == "monochrome"` and so on is data of the theme
    L.square = ts.square;
    L.grayscale = ts.grayscale;
    // the font: the one picked in Settings; with Theme default, the theme's own (Paper's book font, Monochrome's monospace, Aero's Segoe UI when the PC has it)
    L.fontKey = app::fontKeyFor(ts, p.font, fontFamilyAvailable(app::fontFamilyKeyOf(ts)));
    {   // 0.15 stage 5m: the New UI's look M1 (Dark, font Auto): its own font (Inter); if it did not load, nothing changes
        const app::lookopt::Tokens* lt = &app::lookopt::m1();
        if (lt && L.c.id == "dark" && L.fontKey == "auto" && fontFamilyAvailable(lt->font)) L.fontKey = lt->font;
        L.m1 = lt && L.c.id == "dark";                   // 0.15 stage 5n: the windows follow M1 too (colours: Look::w)
        L.w = L.m1 ? app::lookopt::apply(*lt, L.c) : L.c;
    }
    g_look = L;
    useFontFamily(L.fontKey);
    const std::string key = L.c.id + (L.m1 ? "|m1|" : "|")  + std::to_string(L.textScale) + "|" + std::to_string(ImGui::GetStyle().FontScaleDpi);   // 0.15 stage 2: the display scale can change while the app runs
    if (key == g_applied) return;
    g_applied = key;
    ImGuiStyle& st = ImGui::GetStyle();
    st.FontScaleMain = L.textScale;
    const app::lookopt::Tokens& t1 = app::lookopt::m1();
    const float r = L.square ? 0 : 6 * ts.roundScale;
    st.FrameRounding = r * 0.7f; st.GrabRounding = r * 0.5f; st.PopupRounding = r; st.ChildRounding = r; st.TabRounding = r * 0.7f;
    st.ScrollbarRounding = r; st.WindowRounding = L.m1 ? t1.radiusPanel * st.FontScaleDpi : 0;   // (5n: M1 windows have the panels' rounded corners)
    st.FrameBorderSize = ts.panel.borderWidth;
    st.GrabMinSize = 16.0f * st.FontScaleDpi * L.textScale;   // a thumb that is easy to see and grab
    const app::ThemeColors& c = L.w;
    ImVec4* k = st.Colors;
    const app::Rgb tint = L.m1 ? c.fg : c.accent;               // 5n: in M1 a hovered / pressed face is moved towards the text colour (grey), never the accent (blue)
    const float hov = L.m1 ? t1.hoverMix : 0, prs = L.m1 ? t1.pressMix : 0;
    k[ImGuiCol_Text] = vec(c.fg);
    k[ImGuiCol_TextDisabled] = vec(c.muted);
    k[ImGuiCol_WindowBg] = vec(c.bg);                         // every window is solid; only a docked panel of a glass theme is made see-through (pushDockedBg)
    k[ImGuiCol_ChildBg] = vec(c.bg, 0);
    k[ImGuiCol_PopupBg] = vec(app::floatingSurface(c, c.bg, 0.5f, true));   // a pop-up list is one solid colour (a glass theme: the glass over the page's middle)
    k[ImGuiCol_Border] = vec(mix(c.line, c.fg, ts.panel.borderFgMix));
    k[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    k[ImGuiCol_FrameBg] = vec(mix(c.card, c.fg, 0.16f));
    k[ImGuiCol_FrameBgHovered] = vec(L.m1 ? mix(mix(c.card, c.fg, 0.16f), tint, hov) : mix(c.card, c.accent, 0.18f));
    k[ImGuiCol_FrameBgActive] = vec(L.m1 ? mix(mix(c.card, c.fg, 0.16f), tint, prs) : mix(c.card, c.accent, 0.3f));
    k[ImGuiCol_TitleBg] = vec(c.card);
    k[ImGuiCol_TitleBgActive] = vec(L.m1 ? c.card : mix(c.card, c.accentFill, 0.15f));
    k[ImGuiCol_TitleBgCollapsed] = vec(c.card);
    if (ts.panel.title.set)                                   // 0.14 stage 2: one flat colour that ui/style_draw.cpp finds and paints over with the bar's gradient (all three states)
        k[ImGuiCol_TitleBg] = k[ImGuiCol_TitleBgActive] = k[ImGuiCol_TitleBgCollapsed] = vec(ts.panel.title.top);
    k[ImGuiCol_MenuBarBg] = vec(c.card);
    k[ImGuiCol_ScrollbarBg] = vec(c.bg);
    k[ImGuiCol_ScrollbarGrab] = vec(c.line);
    k[ImGuiCol_ScrollbarGrabHovered] = vec(mix(c.line, c.fg, 0.2f));
    k[ImGuiCol_ScrollbarGrabActive] = vec(c.accent);
    k[ImGuiCol_CheckMark] = vec(L.m1 ? c.fg : c.accent);
    k[ImGuiCol_SliderGrab] = vec(mix(c.accent, c.fg, 0.1f));
    k[ImGuiCol_SliderGrabActive] = vec(mix(c.accent, c.fg, 0.2f));
    k[ImGuiCol_Button] = vec(c.button);                       // the card colour unless the theme has its own face colour (Aero's aqua, Y2K's silver)
    k[ImGuiCol_ButtonHovered] = vec(L.m1 ? mix(mix(c.button, c.fg, t1.faceMix), tint, hov) : mix(c.button, c.accent, ts.widget.hoverTint));
    k[ImGuiCol_ButtonActive] = vec(L.m1 ? mix(mix(c.button, c.fg, t1.faceMix), tint, prs) : mix(c.button, c.accent, ts.widget.pressTint));
    if (L.m1) k[ImGuiCol_Button] = vec(mix(c.button, c.fg, t1.faceMix));   // 5n: a button's face as in the shell's controls
    k[ImGuiCol_Header] = vec(mix(c.card, c.accentFill, L.m1 ? 0.10f : 0.15f));
    k[ImGuiCol_HeaderHovered] = vec(mix(c.card, c.accentFill, L.m1 ? 0.14f : 0.25f));
    k[ImGuiCol_HeaderActive] = vec(mix(c.card, c.accentFill, L.m1 ? 0.20f : 0.4f));
    k[ImGuiCol_Separator] = vec(c.line);
    k[ImGuiCol_SeparatorHovered] = vec(c.accent);
    k[ImGuiCol_SeparatorActive] = vec(c.accent);
    k[ImGuiCol_ResizeGrip] = vec(c.line, 0.5f);
    k[ImGuiCol_ResizeGripHovered] = vec(c.accent, 0.6f);
    k[ImGuiCol_ResizeGripActive] = vec(c.accent);
    k[ImGuiCol_Tab] = vec(c.card);
    k[ImGuiCol_TabHovered] = vec(mix(c.card, tint, L.m1 ? 0.14f : 0.25f));
    k[ImGuiCol_TabSelected] = vec(c.accentFill);
    k[ImGuiCol_PlotLines] = vec(c.accent);
    k[ImGuiCol_PlotLinesHovered] = vec(c.fg);
    k[ImGuiCol_PlotHistogram] = vec(c.accent);
    k[ImGuiCol_TableBorderStrong] = vec(c.line);
    k[ImGuiCol_TableBorderLight] = vec(c.line);
    k[ImGuiCol_TextSelectedBg] = vec(c.accent, 0.35f);
    k[ImGuiCol_NavCursor] = vec(c.accent);
    k[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.45f);
    if (L.m1) {                                                // 5n: M1 windows are the panels' card colour (flat), tables and headers are quiet greys
        k[ImGuiCol_WindowBg] = vec(c.card);
        k[ImGuiCol_PopupBg] = vec(c.card);
        k[ImGuiCol_ScrollbarBg] = vec(c.card);
        k[ImGuiCol_TableHeaderBg] = vec(mix(c.card, c.fg, 0.08f));
        k[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
        k[ImGuiCol_TableRowBgAlt] = vec(mix(c.card, c.fg, 0.035f));
        k[ImGuiCol_Border] = vec(mix(c.card, c.fg, t1.edgeAlpha));   // the 1 px hairline of the panels (the text colour at 8 %)
    }
}

}  // namespace ui
