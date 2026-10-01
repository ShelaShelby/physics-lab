#include "look.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <cmath>
#include <regex>
#include <vector>

#include "fonts.h"

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
float corner(float r) { return g_look.square ? 0 : r; }

void pushOn() {
    ImGui::PushStyleColor(ImGuiCol_Button, vec(g_look.c.accent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, vec(g_look.c.accent));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, vec(g_look.c.accent));
    ImGui::PushStyleColor(ImGuiCol_Text, vec(g_look.c.onacc));
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

std::string giftPage(const std::string& html) {
    const float s = ImGui::GetFontSize() / 16 / g_look.textScale, ts = g_look.textScale;   // the web's px, times its text size
    static const std::regex block(R"rx(<div class="gi">(.*?)</div>|<h2>(.*?)</h2>|<p( class="rstL")?>(.*?)</p>|<div class="row">(.*?)</div>)rx");
    static const std::regex btn(R"rx(<button type="button"(?: class="(\w+)")? id="(\w+)">([^<]*)</button>)rx");
    std::string clicked;
    for (std::sregex_iterator it(html.begin(), html.end(), block), end; it != end; ++it) {
        const std::smatch& m = *it;
        if (m[1].matched) centred({{m[1], false}}, 44 * s * ts, col(g_look.c.fg), col(g_look.c.fg));
        else if (m[2].matched) centred({{m[2], true}}, 20 * s * ts, col(g_look.c.fg), col(g_look.c.fg));
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
                    const ImVec4 red = vec(app::hexRgb("#d0453b"));
                    ImGui::PushStyleColor(ImGuiCol_Button, red);
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, red);
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, red);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
                }
                if (ImGui::Button(bs[i].label.c_str())) clicked = bs[i].id;
                if (primary || danger) ImGui::PopStyleColor(4);
            }
        }
    }
    return clicked;
}

void drawSynthGlow(ImDrawList* dl, ImVec2 a, ImVec2 b) {
    // radial-gradient(130% 55% at 50% 0%, #3b1466 0%, var(--bg) 62%): soft rings from the top middle
    const app::Rgb glow = app::hexRgb("#3b1466");
    const float w = b.x - a.x, h = b.y - a.y, rx = 1.3f * w * 0.62f, ry = 0.55f * h * 0.62f;
    for (int i = 24; i >= 1; i--) {
        const float t = i / 24.f;
        const ImU32 c = col(glow, 0.09f * (1 - t) + 0.02f);
        dl->PathClear();
        for (int k = 0; k <= 32; k++) {
            const float an = 3.14159265f * k / 32;
            dl->PathLineTo(ImVec2(a.x + w / 2 + rx * t * std::cos(an), a.y + ry * t * std::sin(an)));
        }
        dl->PathFillConvex(c);
    }
}

void updateLook(const app::Prefs& p, bool systemLight, bool systemReduced) {
    Look L;
    L.c = app::themeColors(app::themeUsable(p, p.theme) ? p.theme : "auto", systemLight);
    L.textScale = float(app::textSizeFactor(p.textSize));
    L.reduced = p.motion == "on" || (p.motion == "auto" && systemReduced);
    L.square = L.c.id == "monochrome";
    L.grayscale = L.c.id == "monochrome";
    L.synthGlow = L.c.id == "synthwave";
    // the font: the one picked in Settings; with Theme default, Paper's book font and Monochrome's monospace (the web's --ffd)
    L.fontKey = p.font != "auto" ? p.font : L.c.id == "paper" ? "serif" : L.c.id == "monochrome" ? "mono" : "auto";
    g_look = L;
    useFontFamily(L.fontKey);
    const std::string key = L.c.id + "|" + std::to_string(L.textScale);
    if (key == g_applied) return;
    g_applied = key;
    ImGuiStyle& st = ImGui::GetStyle();
    st.FontScaleMain = L.textScale;
    const float r = L.square ? 0 : 6;
    st.FrameRounding = r * 0.7f; st.GrabRounding = r * 0.5f; st.PopupRounding = r; st.ChildRounding = r; st.TabRounding = r * 0.7f;
    st.ScrollbarRounding = r; st.WindowRounding = 0;
    st.FrameBorderSize = 1;
    const app::ThemeColors& c = L.c;
    ImVec4* k = st.Colors;
    k[ImGuiCol_Text] = vec(c.fg);
    k[ImGuiCol_TextDisabled] = vec(c.muted);
    k[ImGuiCol_WindowBg] = vec(c.bg);
    k[ImGuiCol_ChildBg] = vec(c.bg, 0);
    k[ImGuiCol_PopupBg] = vec(c.card);
    k[ImGuiCol_Border] = vec(c.line);
    k[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    k[ImGuiCol_FrameBg] = vec(mix(c.card, c.fg, 0.07f));
    k[ImGuiCol_FrameBgHovered] = vec(mix(c.card, c.accent, 0.18f));
    k[ImGuiCol_FrameBgActive] = vec(mix(c.card, c.accent, 0.3f));
    k[ImGuiCol_TitleBg] = vec(c.card);
    k[ImGuiCol_TitleBgActive] = vec(mix(c.card, c.accent, 0.15f));
    k[ImGuiCol_TitleBgCollapsed] = vec(c.card);
    k[ImGuiCol_MenuBarBg] = vec(c.card);
    k[ImGuiCol_ScrollbarBg] = vec(c.bg);
    k[ImGuiCol_ScrollbarGrab] = vec(c.line);
    k[ImGuiCol_ScrollbarGrabHovered] = vec(mix(c.line, c.fg, 0.2f));
    k[ImGuiCol_ScrollbarGrabActive] = vec(c.accent);
    k[ImGuiCol_CheckMark] = vec(c.accent);
    k[ImGuiCol_SliderGrab] = vec(c.accent);
    k[ImGuiCol_SliderGrabActive] = vec(mix(c.accent, c.fg, 0.2f));
    k[ImGuiCol_Button] = vec(c.card);
    k[ImGuiCol_ButtonHovered] = vec(mix(c.card, c.accent, 0.22f));
    k[ImGuiCol_ButtonActive] = vec(mix(c.card, c.accent, 0.45f));
    k[ImGuiCol_Header] = vec(mix(c.card, c.accent, 0.15f));
    k[ImGuiCol_HeaderHovered] = vec(mix(c.card, c.accent, 0.25f));
    k[ImGuiCol_HeaderActive] = vec(mix(c.card, c.accent, 0.4f));
    k[ImGuiCol_Separator] = vec(c.line);
    k[ImGuiCol_SeparatorHovered] = vec(c.accent);
    k[ImGuiCol_SeparatorActive] = vec(c.accent);
    k[ImGuiCol_ResizeGrip] = vec(c.line, 0.5f);
    k[ImGuiCol_ResizeGripHovered] = vec(c.accent, 0.6f);
    k[ImGuiCol_ResizeGripActive] = vec(c.accent);
    k[ImGuiCol_Tab] = vec(c.card);
    k[ImGuiCol_TabHovered] = vec(mix(c.card, c.accent, 0.25f));
    k[ImGuiCol_TabSelected] = vec(c.accent);
    k[ImGuiCol_PlotLines] = vec(c.accent);
    k[ImGuiCol_PlotLinesHovered] = vec(c.fg);
    k[ImGuiCol_PlotHistogram] = vec(c.accent);
    k[ImGuiCol_TableBorderStrong] = vec(c.line);
    k[ImGuiCol_TableBorderLight] = vec(c.line);
    k[ImGuiCol_TextSelectedBg] = vec(c.accent, 0.35f);
    k[ImGuiCol_NavCursor] = vec(c.accent);
    k[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.45f);
}

}  // namespace ui
