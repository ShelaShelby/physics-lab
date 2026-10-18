#include "app_pages.h"

#include <algorithm>
#include <cfloat>
#include <string>

#include "app/app_data.h"
#include "app_tabs.h"
#include "changelog.h"
#include "app/history.h"
#include "fonts.h"
#include "html_view.h"
#include "imgui.h"
#include "look.h"
#include "style_draw.h"
#include "atom_panel.h"
#include "rich_text.h"
#include "settings.h"
#include "ui/reading.h"

namespace ui {

namespace {


float S() { return ImGui::GetFontSize() / 16 / look().textScale; }   // one web px (the web's sizes are then times the text size)

// a window like the web's Learn pop-up page (lrnOpen): one at a time, centred, closed with ✕ or Escape
bool beginPage(const char* title, const char* id, bool* open) {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(std::min(vp->WorkSize.x * 0.6f, 46 * ImGui::GetFontSize()), vp->WorkSize.y * 0.85f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    const std::string t = displayable(title) + "###" + id;
    const bool shown = pageBegin(t, open);                     // the floating window as before, or (new UI) the reading view
    if (shown && ImGui::IsKeyPressed(ImGuiKey_Escape) && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) *open = false;
    return shown;
}

}  // namespace

// a header button (.ver): a pill with a thin border, bold small text
bool pillButton(const std::string& label, const char* tip) {
    const float s = S() * look().textScale;
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, corner(99));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10 * s, 3 * s));
    ImGui::PushStyleColor(ImGuiCol_Button, glossy() ? vec(look().w.card) : ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_Border, vec(look().w.line));
    ImGui::PushFont(fonts().bold, ImGui::GetStyle().FontSizeBase * 13 / 16);
    const bool clicked = ui::Button(displayable(label, fonts().bold).c_str());
    ImGui::PopFont();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    if (tip && *tip && ImGui::IsItemHovered()) ui::SetTooltip("%s", tip);
    return clicked;
}

// 👤 About: the web's page word for word, with the labs of the native app, and the native version
void aboutWindow(AppState& app) {
    if (!app.showAbout) return;
    if (beginPage(app::kAboutTitle, "about", &app.showAbout)) {
        htmlView(app::kAboutHtml);
        htmlView(std::string("<p class=\"lrnP lrnMuted\">Native app: Physics Lab (native) ") + appVersionLabel() + ".</p>");
        htmlView("<p class=\"lrnP lrnMuted\">Text codes such as &amp;rsquo; are turned into their characters with the list of named character references of the HTML Standard (WHATWG, entities.json); this page is the only text that uses them.</p>");
        htmlView("<p class=\"lrnP lrnMuted\">0.15 stage 5g and 5m: the themes Y2K Ice, Y2K Night, Frutiger Aero and Synthwave were removed at the owner's request (a saved setting that names one gives Dark). No new data.</p>");
        htmlView("<p class=\"lrnP lrnMuted\">The New UI (0.15 stage 3): panel opacity is found with the same W3C WCAG 2.x contrast formula (text, grey text and accent text each at least 4.5:1 over the worst picture that can be behind a panel, white behind a dark panel and black behind a light one); the icons are drawn by the app itself, no icon set; the type scale and sizes were chosen with the owner (docs/ui-0.15/DESIGN.md); no new data.</p>");
    }
    pageEnd();
}

// 🕰 history: the web's intro, then the native app's own updates (newest first), then the web's APP_LOG word for word
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
            dl->AddLine(p, ImVec2(p.x + w, p.y), col(look().w.line));
            const float top = p.y + 12 * s * ts;
            dl->AddRectFilled(ImVec2(p.x, top), ImVec2(p.x + box, top + box), col(look().w.soft, 31/255.f), corner(12 * s * ts));
            const std::string icon = displayable(ic);
            if (!icon.empty()) {
                const float fs = 28 * s * ts;
                const ImVec2 sz = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0, icon.c_str());
                dl->AddText(ImGui::GetFont(), fs, ImVec2(p.x + (box - sz.x) / 2, top + (box - sz.y) / 2), col(look().w.fg), icon.c_str());
            }
            ImGui::SetCursorScreenPos(ImVec2(x1, top));
            ImGui::PushTextWrapPos(p.x + w - ImGui::GetWindowPos().x);   // window-relative
            ImGui::PushFont(fonts().bold, ImGui::GetStyle().FontSizeBase * 13 / 16);
            ImGui::PushStyleColor(ImGuiCol_Text, vec(look().w.accent));
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
                    if (ui::Button(displayable(button).c_str())) { openLab(app, to); app.showHistory = false; }
                    ImGui::PopID();
                }
            }
            const float bottom = std::max(ImGui::GetCursorScreenPos().y, top + box) + 12 * s * ts;
            ImGui::SetCursorScreenPos(ImVec2(p.x, bottom));
            ImGui::Dummy(ImVec2(w, 0));
        };
        // the native app's own updates first, newest first ("you are here" is the native version you are running)
        int i = 0, n = 0;
        richHeading(app::kHistoryNativeHeading);
        const NativeEntry* nat = nativeHistory(n);
        for (int k = 0; k < n; k++, i++)
            item(i, "✨", std::string("Physics Lab (native) ") + (k == 0 ? appVersionLabel() : std::string(nat[k].version)) + (k == 0 ? app::kHistoryHere : ""), nat[k].title, nat[k].text, "", "");
        // then the web's, word for word, with a note above them
        ImGui::Spacing();
        richHeading(app::kHistoryWebHeading);
        htmlView(std::string("<p class=\"lrnP lrnMuted\">") + app::kHistoryWebNote + "</p>");
        for (const app::LogEntry& e : app::kAppLog) { item(i, e.ic, std::string(app::kHistoryYear) + e.v, e.t, e.d, e.labButton, e.lab); i++; }
        htmlView(std::string("<p class=\"lrnP lrnMuted\">") + app::kHistoryNote + "</p>");
    }
    pageEnd();
}

}  // namespace ui
