#include "app_pages.h"

#include <algorithm>
#include <cfloat>
#include <string>

#include "app/app_data.h"
#include "changelog.h"
#include "fonts.h"
#include "html_view.h"
#include "imgui.h"
#include "look.h"
#include "atom_panel.h"
#include "rich_text.h"
#include "settings.h"

namespace ui {

namespace {

float g_headerH = 0;               // the header's height last frame (window coordinates)

float S() { return ImGui::GetFontSize() / 16 / look().textScale; }   // one web px (the web's sizes are then times the text size)

// "🕰 v8.6.2 · history" with the native app's version
std::string historyLabel() {
    std::string l = app::kHdrHistory.label;
    const std::size_t v = l.find(" v"), e = l.find(' ', v + 2);
    if (v != std::string::npos && e != std::string::npos) l = l.substr(0, v + 2) + appVersion() + l.substr(e);
    return l;
}

// a window like the web's Learn pop-up page (lrnOpen): one at a time, centred, closed with ✕ or Escape
bool beginPage(const char* title, const char* id, bool* open) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(std::min(vp->WorkSize.x * 0.6f, 46 * ImGui::GetFontSize()), vp->WorkSize.y * 0.85f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    const std::string t = displayable(title) + "###" + id;
    const bool shown = ImGui::Begin(t.c_str(), open, ImGuiWindowFlags_NoCollapse);
    if (shown && ImGui::IsKeyPressed(ImGuiKey_Escape) && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) *open = false;
    return shown;
}

// 🌐 the language menu: English, and Estonian greyed out as "Coming soon"
void languageMenu(AppState& app) {
    if (!ImGui::BeginPopup("##lang")) return;
    const float s = S() * look().textScale, w = 230 * s - 12 * s, h = 36 * s;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (const app::LangOpt& L : app::kLangs) {
        const bool soon = *L.soon, on = !soon && app.prefs.lang == "en" && std::string(L.name) == "English";
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::PushID(L.name);
        if (soon) ImGui::BeginDisabled();
        if (ImGui::Selectable("##l", on, 0, ImVec2(w, h)) && !soon) { app.prefs.lang = "en"; ImGui::CloseCurrentPopup(); }
        if (soon) ImGui::EndDisabled();
        ImGui::PopID();
        const float a = soon ? 0.6f : 1;                                   // :disabled{opacity:.6}
        float x = p.x + 10 * s;
        const std::string flag = displayable(L.flag);
        if (!flag.empty()) { dl->AddText(ImGui::GetFont(), 18 * s, ImVec2(x, p.y + (h - 18 * s) / 2), col(look().c.fg, a), flag.c_str()); x += 28 * s; }
        dl->AddText(ImGui::GetFont(), 14 * s, ImVec2(x, p.y + (h - 14 * s) / 2), col(look().c.fg, a), L.name);
        if (soon) {                                                        // .soon: a small rounded chip
            const float fs = 12 * s, tw = fonts().bold->CalcTextSizeA(fs, FLT_MAX, 0, L.soon).x;
            const ImVec2 b(p.x + w - 8 * s, p.y + h / 2 + fs / 2 + 3 * s), c(b.x - tw - 16 * s, p.y + h / 2 - fs / 2 - 3 * s);
            dl->AddRect(c, b, col(look().c.line, a), corner(99));
            dl->AddText(fonts().bold, fs, ImVec2(c.x + 8 * s, c.y + 3 * s), col(look().c.muted, a), L.soon);
        } else if (on) dl->AddText(fonts().bold, 14 * s, ImVec2(p.x + w - 20 * s, p.y + (h - 14 * s) / 2), col(look().c.accent), "✓");
    }
    ImGui::EndPopup();
}

}  // namespace

float headerHeight() { return g_headerH; }

// a header button (.ver): a pill with a thin border, bold small text
bool pillButton(const std::string& label, const char* tip) {
    const float s = S() * look().textScale;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, corner(99));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10 * s, 3 * s));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, vec(look().c.line));
    ImGui::PushFont(fonts().bold, ImGui::GetStyle().FontSizeBase * 13 / 16);
    const bool clicked = ImGui::Button(displayable(label, fonts().bold).c_str());
    ImGui::PopFont();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    if (tip && *tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    return clicked;
}

void header(AppState& app) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float s = S() * look().textScale, pad = 12 * s;
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSizeConstraints(ImVec2(vp->Size.x, 0), ImVec2(vp->Size.x, FLT_MAX));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, 8 * s));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##header", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::PopStyleVar(2);
    if (look().synthGlow) drawSynthGlow(ImGui::GetWindowDrawList(), vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y));
    const app::LabInfo& L = app::kLabInfo[app.lab];
    const std::string ver = std::string("Update ") + L.update, hist = historyLabel();
    // the buttons' widths, so they can sit on the right of the first line (they move under the title when there is no room)
    ImGui::PushFont(fonts().bold, ImGui::GetStyle().FontSizeBase * 13 / 16);
    float bw = 0;
    for (const std::string& t : {std::string(app::kHdrTheme.label), std::string(app::kLangLabel), std::string(app::kHdrAbout.label),
                                 std::string(app::kHdrSettings.label), hist, ver})
        bw += ImGui::CalcTextSize(displayable(t, fonts().bold).c_str()).x + 20 * s + 2 + 8 * s;
    ImGui::PopFont();
    const float W = vp->Size.x - 2 * pad, x0 = ImGui::GetCursorPosX(), y0 = ImGui::GetCursorPosY();
    // h1: Physics Lab (Synthwave gives it a pink glow)
    ImGui::PushFont(fonts().bold, ImGui::GetStyle().FontSizeBase * 20 / 16);
    const ImVec2 tp = ImGui::GetCursorScreenPos();
    if (look().c.id == "synthwave")
        for (int k = 1; k <= 3; k++) ImGui::GetWindowDrawList()->AddText(ImVec2(tp.x, tp.y), col(app::hexRgb("#ff4fd8"), 0.12f * k), app::kHdrTitle);
    ImGui::TextUnformatted(app::kHdrTitle);
    const float titleW = ImGui::GetItemRectSize().x, titleH = ImGui::GetItemRectSize().y;
    ImGui::PopFont();
    // #sub: the lab's subtitle, between the title and the buttons
    const bool oneLine = W - titleW - bw > 260 * s;
    const float subX = x0 + titleW + 10 * s, subRight = oneLine ? x0 + W - bw - 10 * s : x0 + W;
    ImGui::SetCursorPos(ImVec2(subX, y0 + titleH - ImGui::GetFontSize() * 1.15f));
    ImGui::PushTextWrapPos(subRight);
    ImGui::PushStyleColor(ImGuiCol_Text, vec(look().c.muted));
    ImGui::TextUnformatted(displayable(L.subtitle).c_str());
    ImGui::PopStyleColor();
    ImGui::PopTextWrapPos();
    const float below = ImGui::GetCursorPosY();
    // #hb: 🎨 Theme, 🌐 English, 👤 About, ⚙ Settings, 🕰 history, Update n
    ImGui::SetCursorPos(oneLine ? ImVec2(x0 + W - bw + 8 * s, y0 + 2 * s) : ImVec2(x0, below + 4 * s));
    themeButton(app);
    ImGui::SameLine(0, 8 * s);
    if (pillButton(app::kLangLabel, app::kHdrLang.tip)) ImGui::OpenPopup("##lang");
    languageMenu(app);
    ImGui::SameLine(0, 8 * s);
    if (pillButton(app::kHdrAbout.label, app::kHdrAbout.tip)) app.showAbout = true;
    ImGui::SameLine(0, 8 * s);
    if (pillButton(app::kHdrSettings.label, app::kHdrSettings.tip)) app.showSettings = true;
    ImGui::SameLine(0, 8 * s);
    if (pillButton(hist, app::kHdrHistory.tip)) app.showHistory = true;
    ImGui::SameLine(0, 8 * s);
    if (pillButton(ver, app::kHdrVer.tip)) app.showLabLog = true;
    if (oneLine && ImGui::GetCursorPosY() < below) { ImGui::SetCursorPosY(below); ImGui::Dummy(ImVec2(0, 0)); }
    g_headerH = ImGui::GetWindowHeight();
    ImGui::End();
}

// 👤 About: the web's page word for word, with the labs of the native app, and the native version
void aboutWindow(AppState& app) {
    if (!app.showAbout) return;
    if (beginPage(app::kAboutTitle, "about", &app.showAbout)) {
        htmlView(app::kAboutHtml);
        htmlView(std::string("<p class=\"lrnP lrnMuted\">Native app: Physics Lab (native) ") + appVersion() + ".</p>");
    }
    ImGui::End();
}

// 🕰 history: the web's APP_LOG word for word, then the native app's own updates
void historyWindow(AppState& app) {
    if (!app.showHistory) return;
    if (beginPage(app::kHistoryTitle, "history", &app.showHistory)) {
        const float s = S(), ts = look().textScale;
        htmlView(std::string("<p class=\"lrnP\">") + app::kHistoryIntro + "</p>");
        // .lrnTi: an icon box (60 px), then the version line, the title, the text and the ▶ button
        auto item = [&](int i, const std::string& ic, const std::string& year, const char* title, const char* text, const char* button, const char* lab) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float w = ImGui::GetContentRegionAvail().x, box = 60 * s * ts, x1 = p.x + box + 12 * s * ts;
            dl->AddLine(p, ImVec2(p.x + w, p.y), col(look().c.line));
            const float top = p.y + 12 * s * ts;
            dl->AddRectFilled(ImVec2(p.x, top), ImVec2(p.x + box, top + box), IM_COL32(120, 140, 200, 31), corner(12 * s * ts));
            const std::string icon = displayable(ic);
            if (!icon.empty()) {
                const float fs = 28 * s * ts;
                const ImVec2 sz = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0, icon.c_str());
                dl->AddText(ImGui::GetFont(), fs, ImVec2(p.x + (box - sz.x) / 2, top + (box - sz.y) / 2), col(look().c.fg), icon.c_str());
            }
            ImGui::SetCursorScreenPos(ImVec2(x1, top));
            ImGui::PushTextWrapPos(p.x + w - ImGui::GetWindowPos().x);   // window-relative
            ImGui::PushFont(fonts().bold, ImGui::GetStyle().FontSizeBase * 13 / 16);
            ImGui::PushStyleColor(ImGuiCol_Text, vec(look().c.accent));
            ImGui::TextUnformatted(displayable(year, fonts().bold).c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::SetCursorPosX(x1 - ImGui::GetWindowPos().x + ImGui::GetScrollX());
            ImGui::PushFont(fonts().bold, ImGui::GetStyle().FontSizeBase * 15 / 16);
            ImGui::TextUnformatted(displayable(title, fonts().bold).c_str());
            ImGui::PopFont();
            ImGui::SetCursorPosX(x1 - ImGui::GetWindowPos().x + ImGui::GetScrollX());
            ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 13.5f / 16);
            ImGui::TextUnformatted(displayable(text).c_str());
            ImGui::PopFont();
            ImGui::PopTextWrapPos();
            if (*button) {
                const std::string l = lab;
                const int to = l == "Orbit Lab" ? 0 : l == "Atom Lab" ? 1 : -1;
                if (to >= 0) {                                   // only the labs the native app has
                    ImGui::SetCursorPosX(x1 - ImGui::GetWindowPos().x + ImGui::GetScrollX());
                    ImGui::PushID(i);
                    if (ImGui::Button(displayable(button).c_str())) { openLab(app, to); app.showHistory = false; }
                    ImGui::PopID();
                }
            }
            const float bottom = std::max(ImGui::GetCursorScreenPos().y, top + box) + 12 * s * ts;
            ImGui::SetCursorScreenPos(ImVec2(p.x, bottom));
            ImGui::Dummy(ImVec2(w, 0));
        };
        int i = 0;
        for (const app::LogEntry& e : app::kAppLog) { item(i, e.ic, std::string(app::kHistoryYear) + e.v, e.t, e.d, e.labButton, e.lab); i++; }
        htmlView(std::string("<p class=\"lrnP lrnMuted\">") + app::kHistoryNote + "</p>");
        // the native app's own updates, newest first ("you are here" is the native version you are running)
        ImGui::Spacing();
        richHeading("Physics Lab (native)");
        int n = 0;
        const NativeEntry* nat = nativeHistory(n);
        for (int k = 0; k < n; k++, i++)
            item(i, "✨", std::string("Physics Lab (native) ") + nat[k].version + (k == 0 ? app::kHistoryHere : ""), nat[k].title, nat[k].text, "", "");
    }
    ImGui::End();
}

}  // namespace ui
