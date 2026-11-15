#include "app_pages.h"

#include <algorithm>
#include <cfloat>
#include <string>

#include "app/app_data.h"
#include "app_tabs.h"
#include "changelog.h"
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


float S() { return ImGui::GetFontSize() / 16 / look().textScale; }

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

// 👤 About: the page, then the version
void aboutWindow(AppState& app) {
    if (!app.showAbout) { setHtmlFocus(""); return; }   // (0.40: a fact's source stays highlighted only while About is open)
    if (beginPage(app::kAboutTitle, "about", &app.showAbout)) {
        htmlView(app::kAboutHtml);
        htmlView(std::string("<p class=\"lrnP lrnMuted\">Physics Lab ") + appVersionLabel() + ".</p>");
        // 0.22 stage 6: the sources of how the screen is made, in plain words (was: notes with stage numbers and the removed themes)
        htmlView("<p class=\"lrnP lrnMuted\">How the screen is made: panels are see-through only as far as the W3C WCAG 2.x contrast formula allows (text at least 4.5:1 over the brightest or darkest picture behind it). The icons are drawn by the app itself. Character names such as &amp;rsquo; are read with the HTML Standard's list (WHATWG, entities.json).</p>");
    }
    pageEnd();
}

// 🕰 history: the intro, then the app's updates, newest first
void historyWindow(AppState& app) {
    if (!app.showHistory) return;
    if (beginPage(app::kHistoryTitle, "history", &app.showHistory)) {
        const float s = S(), ts = look().textScale;
        htmlView(std::string("<p class=\"lrnP\">") + app::kHistoryIntro + "</p>");
        // .lrnTi: an icon box (60 px), then the version line, the title and the text
        auto item = [&](const std::string& ic, const std::string& year, const char* title, const char* text) {   // (0.50.3: the never-used "open that lab" button is gone)
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
            const float bottom = std::max(ImGui::GetCursorScreenPos().y, top + box) + 12 * s * ts;
            ImGui::SetCursorScreenPos(ImVec2(p.x, bottom));
            ImGui::Dummy(ImVec2(w, 0));
        };
        // the app's updates, newest first ("you are here" is the version you are running)
        int n = 0;
        const NativeEntry* nat = nativeHistory(n);
        for (int k = 0; k < n; k++)
            item("✨", std::string("Physics Lab ") + (k == 0 ? appVersionLabel() : std::string(nat[k].version)) + (k == 0 ? app::kHistoryHere : ""), nat[k].title, nat[k].text);
    }
    pageEnd();
}

}  // namespace ui
