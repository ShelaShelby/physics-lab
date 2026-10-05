#include "atom_learn.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "atom/atom_learn.h"
#include "fonts.h"
#include "html_view.h"
#include "imgui.h"
#include "look.h"
#include "style_draw.h"
#include "orbit_panel.h"
#include "rich_text.h"
#include "svg_icon.h"

namespace ui {

using namespace atom;

namespace {

float S() { return ImGui::GetFontSize() / 16.0f; }
const char* const kTitles[5] = {"", "What am I even looking at?", "Scale · how empty is an atom?", "Weight · what does an atom weigh?", "Timeline of the atom"};

// a row of buttons, the chosen one highlighted (the web's lrnChips / lrnTabs); returns the one clicked or -1
int chipRow(const std::vector<std::string>& labels, int on) {
    int clicked = -1;
    const float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    for (int i = 0; i < int(labels.size()); i++) {
        const std::string t = displayable(labels[i]);
        const float w = ImGui::CalcTextSize(t.c_str()).x + ImGui::GetStyle().FramePadding.x * 2;
        if (i > 0 && ImGui::GetItemRectMax().x + 6 * S() + w < right) ImGui::SameLine(0, 6 * S());
        if (i == on) pushOn();
        ImGui::PushID(i);
        if (ui::Button(t.c_str())) clicked = i;
        ImGui::PopID();
        if (i == on) popOn();
    }
    return clicked;
}

// pick another element or isotope without leaving the guide (web: lrnElemBar)
void elemBar(AppState& app) {
    AtomLab& L = app.atom;
    auto goZ = [&](int z) { z = std::max(1, std::min(118, z)); L.setParam("Z", z); L.setParam("N", defaultN(z)); };
    if (ui::Button("◀")) goZ(L.p.Z - 1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("previous element");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x - 8 * S());
    auto label = [](int z) { return std::to_string(z) + " · " + name(z) + " (" + symbol(z) + ")"; };
    if (ImGui::BeginCombo("##elem", label(L.p.Z).c_str())) {
        for (int z = 1; z <= 118; z++) if (ImGui::Selectable(label(z).c_str(), z == L.p.Z)) goZ(z);
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ui::Button("▶")) goZ(L.p.Z + 1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("next element");
    ImGui::TextDisabled("Neutrons");
    ImGui::SameLine();
    const auto r = nRange(L.p.Z);
    if (ui::Button("−")) L.setParam("N", std::max(r.first, std::min(r.second, L.p.N - 1)));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("one fewer neutron");
    ImGui::SameLine();
    ImGui::PushFont(fonts().bold, 0.0f);
    ImGui::Text("%s-%d", name(L.p.Z), L.p.Z + L.p.N);
    ImGui::PopFont();
    ImGui::SameLine();
    if (ui::Button("+")) L.setParam("N", std::max(r.first, std::min(r.second, L.p.N + 1)));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("one more neutron");
    ImGui::SameLine();
    if (ui::Button("usual")) L.setParam("N", defaultN(L.p.Z));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("the usual isotope");
    ImGui::Separator();
}

// ---------------------------------------------------------------- the four guides
void lookGuide(AppState& app) {
    AtomLab& L = app.atom;
    richText(std::string("<p class=\"lrnP\">") + kLookIntro + "</p>");
    elemBar(app);
    richText("<p class=\"lrnP lrnMuted\">Which picture is on screen?</p>");
    const std::vector<std::string> views = {"Orbit", "Cloud", "Cloud 3D", "Slice"};
    const int on = int(std::find(views.begin(), views.end(), L.p.view) - views.begin());
    const int v = chipRow(views, on);
    if (v >= 0) L.setParam("view", views[v]);
    std::string live;
    for (const std::string& t : L.info()) live += (live.empty() ? "" : "<br>") + t;
    const auto secs = lookSections(L.p, live);
    // the row of headings: tap one to jump to it
    if (app.atomLearnJump == "bar") { ImGui::SetScrollHereY(0); app.atomLearnJump.clear(); }
    ImGui::Spacing();
    std::vector<std::string> titles;
    for (const auto& s : secs) titles.push_back(s.title);
    const int j = chipRow(titles, -1);
    if (j >= 0) app.atomLearnJump = secs[j].key;
    for (std::size_t i = 0; i < secs.size(); i++) {
        ImGui::Spacing();
        ImGui::Separator();
        if (app.atomLearnJump == secs[i].key) { ImGui::SetScrollHereY(0); app.atomLearnJump.clear(); }
        richHeading(secs[i].title);
        ImGui::SameLine();
        ImGui::PushID(int(i));
        if (ImGui::SmallButton("↑")) app.atomLearnJump = "bar";
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Back to the list of headings");
        ImGui::PopID();
        htmlView(secs[i].html);
    }
}

void scaleGuide(AppState& app) {
    elemBar(app);
    const std::vector<std::string> tabs = {"📏 If the nucleus were big", "🔟 Powers of ten"};
    const int t = chipRow(tabs, app.scaleTab == "ten" ? 1 : 0);
    if (t >= 0) app.scaleTab = t ? "ten" : "scale";
    htmlView(scaleHtml(app.atom.p, app.scaleTab, app.scaleObj), [&] {
        std::vector<std::string> objs;
        for (const char* o : kScaleObjects) objs.push_back(std::string(o).substr(2));   // "a pea" -> "pea"
        const int o = chipRow(objs, app.scaleObj);
        if (o >= 0) app.scaleObj = o;
    });
}

void massGuide(AppState& app) {
    elemBar(app);
    const char* keys[5] = {"parts", "miss", "count", "proton", "ladder"};
    const std::vector<std::string> tabs = {"⚖ The parts", "🔍 Missing mass", "🔢 How many atoms?", "🧩 Inside a proton", "🪜 Mass ladder"};
    int on = 0;
    for (int i = 0; i < 5; i++) if (app.massTab == keys[i]) on = i;
    const int t = chipRow(tabs, on);
    if (t >= 0) app.massTab = keys[t];
    htmlView(massHtml(app.atom.p, app.massTab));
}

void timelineGuide(AppState& app) {
    richText(std::string("<p class=\"lrnP\">") + kTimelineIntro + "</p>");
    int i = 0;
    for (const TimelineStep& st : timelineSteps()) {
        ImGui::Separator();
        const float s = S();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 o = ImGui::GetCursorScreenPos();
        dl->AddRectFilled(ImVec2(o.x, o.y + 4 * s), ImVec2(o.x + 60 * s, o.y + 64 * s), col(look().c.soft, 31/255.f), 12 * s);
        svgIcon(st.svg, ImVec2(o.x + 2 * s, o.y + 6 * s), s);
        ImGui::SetCursorScreenPos(ImVec2(o.x + 72 * s, o.y + 4 * s));
        ImGui::BeginGroup();
        ImGui::PushTextWrapPos(0);
        ImGui::PushFont(fonts().bold, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, vec(look().c.accent));   // the year, in the accent colour (.lrnY)
        ImGui::TextUnformatted(st.y);
        ImGui::PopStyleColor();
        ImGui::TextUnformatted(st.t);
        ImGui::PopFont();
        ImGui::PopTextWrapPos();
        richText(std::string("<p class=\"lrnP lrnMuted\">") + st.who + "</p>", false, 0.85f, 0.1f);
        richText(std::string("<p class=\"lrnP\">") + st.d + "</p>", false, 0.9f, 0.3f);
        if (*st.lab) {
            ImGui::PushID(i);
            if (ui::Button((std::string("▶ ") + st.lab).c_str())) {   // close the guide and set the lab to show it
                for (const auto& [k, v] : st.set) {
                    char* end = nullptr;
                    const double d = std::strtod(v.c_str(), &end);
                    if (end && *end == 0) app.atom.setParam(k, d); else app.atom.setParam(k, v);
                }
                app.atomLearn = 0;
            }
            ImGui::PopID();
        }
        ImGui::EndGroup();
        ImGui::SetCursorScreenPos(ImVec2(o.x, std::max(ImGui::GetItemRectMax().y, o.y + 64 * s) + 8 * s));
        ImGui::Dummy(ImVec2(0, 0));
        i++;
    }
}

}  // namespace

void atomHelpButton(AppState& app) {
    if (wideButton(displayable("❓ What am I even looking at?").c_str(), "atomHelp")) { app.atomLearn = 1; app.atomLearnJump = "top"; }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("A plain-words guide to the atom on screen");
}

void atomLearnButtons(AppState& app) {
    ImGui::SeparatorText("Learn");
    const char* labels[4] = {"❓ What am I even looking at?", "📏 Scale: how empty is an atom?", "⚖ Weight: what does an atom weigh?", "🕰 Timeline: how we pictured the atom"};
    for (int i = 0; i < 4; i++)
        if (wideButton(displayable(labels[i]).c_str(), (std::string("atomLearn") + char('1' + i)).c_str())) {
            app.atomLearn = i + 1; app.atomLearnJump = "top";
            app.scaleTab = "scale"; app.scaleObj = 3; app.massTab = "parts";   // each opening starts like the web's
        }
}

void atomLearnWindow(AppState& app) {
    if (app.atomLearn <= 0 || app.atomLearn > 4) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(std::min(vp->WorkSize.x * 0.6f, 44 * ImGui::GetFontSize()), vp->WorkSize.y * 0.85f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.4f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    bool open = true;
    const std::string title = std::string(kTitles[app.atomLearn]) + "###atomlearn";
    pushFloatStyle();
    const bool guideShown = ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse);
    popFloatStyle();
    if (guideShown) {
        panelDecor(app::PanelKind::Floating);
        if (app.atomLearnJump == "top") { ImGui::SetScrollY(0); app.atomLearnJump.clear(); }
        if (const char* sc = std::getenv("PL_LEARN_SCROLL")) ImGui::SetScrollY(float(std::atof(sc)));   // for automatic checks
        ImGui::PushTextWrapPos(0);
        if (app.atomLearn == 1) lookGuide(app);
        else if (app.atomLearn == 2) scaleGuide(app);
        else if (app.atomLearn == 3) massGuide(app);
        else timelineGuide(app);
        ImGui::PopTextWrapPos();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) open = false;
    }
    ImGui::End();
    if (!open) app.atomLearn = 0;
}

}  // namespace ui
