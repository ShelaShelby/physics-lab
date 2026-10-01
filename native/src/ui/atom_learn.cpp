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
#include "orbit_panel.h"
#include "rich_text.h"

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
        if (ImGui::Button(t.c_str())) clicked = i;
        ImGui::PopID();
        if (i == on) popOn();
    }
    return clicked;
}

// pick another element or isotope without leaving the guide (web: lrnElemBar)
void elemBar(AppState& app) {
    AtomLab& L = app.atom;
    auto goZ = [&](int z) { z = std::max(1, std::min(118, z)); L.setParam("Z", z); L.setParam("N", defaultN(z)); };
    if (ImGui::Button("◀")) goZ(L.p.Z - 1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("previous element");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x - 8 * S());
    auto label = [](int z) { return std::to_string(z) + " · " + name(z) + " (" + symbol(z) + ")"; };
    if (ImGui::BeginCombo("##elem", label(L.p.Z).c_str())) {
        for (int z = 1; z <= 118; z++) if (ImGui::Selectable(label(z).c_str(), z == L.p.Z)) goZ(z);
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("▶")) goZ(L.p.Z + 1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("next element");
    ImGui::TextDisabled("Neutrons");
    ImGui::SameLine();
    const auto r = nRange(L.p.Z);
    if (ImGui::Button("−")) L.setParam("N", std::max(r.first, std::min(r.second, L.p.N - 1)));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("one fewer neutron");
    ImGui::SameLine();
    ImGui::PushFont(fonts().bold, 0.0f);
    ImGui::Text("%s-%d", name(L.p.Z), L.p.Z + L.p.N);
    ImGui::PopFont();
    ImGui::SameLine();
    if (ImGui::Button("+")) L.setParam("N", std::max(r.first, std::min(r.second, L.p.N + 1)));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("one more neutron");
    ImGui::SameLine();
    if (ImGui::Button("usual")) L.setParam("N", defaultN(L.p.Z));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("the usual isotope");
    ImGui::Separator();
}

// ---------------------------------------------------------------- a tiny SVG painter for the timeline icons
// It knows what the web's 22 icons use: circle, rect, path (M, L, Q, T), text, and a radial gradient fill.
std::string svgAttr(const std::string& tag, const char* name) {
    const std::string key = std::string(" ") + name + "=\"";
    const std::size_t a = tag.find(key);
    if (a == std::string::npos) return "";
    const std::size_t b = tag.find('"', a + key.size());
    return tag.substr(a + key.size(), b - a - key.size());
}
ImVec4 svgColor(const std::string& c) {
    if (c.size() == 7 && c[0] == '#') {
        const unsigned long v = std::strtoul(c.c_str() + 1, nullptr, 16);
        return ImVec4(((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f, 1);
    }
    if (c == "#fff") return ImVec4(1, 1, 1, 1);
    return ImVec4(0.5f, 0.5f, 0.5f, 1);
}
void svgIcon(const std::string& svg, ImVec2 o, float scale) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto P = [&](double x, double y) { return ImVec2(o.x + float(x) * scale, o.y + float(y) * scale); };
    std::map<std::string, std::vector<std::pair<float, ImVec4>>> grads;   // id -> stops (offset, colour with opacity)
    std::string curGrad;
    for (std::size_t i = 0; i < svg.size();) {
        const std::size_t a = svg.find('<', i);
        if (a == std::string::npos) break;
        const std::size_t b = svg.find('>', a);
        const std::string tag = svg.substr(a, b - a + 1);
        i = b + 1;
        const std::string nm = tag.substr(1, tag.find_first_of(" />", 1) - 1);
        auto num = [&](const char* k, double d = 0) { const std::string v = svgAttr(tag, k); return v.empty() ? d : std::atof(v.c_str()); };
        if (nm == "radialGradient") { curGrad = svgAttr(tag, "id"); grads[curGrad].clear(); continue; }
        if (nm == "stop") {
            ImVec4 c = svgColor(svgAttr(tag, "stop-color"));
            c.w = float(num("stop-opacity", 1));
            grads[curGrad].push_back({float(num("offset")), c});
            continue;
        }
        const float op = float(num("opacity", 1));
        const std::string fill = svgAttr(tag, "fill"), stroke = svgAttr(tag, "stroke");
        const float sw = float(num("stroke-width", 1)) * scale;
        const bool dashed = !svgAttr(tag, "stroke-dasharray").empty();
        auto col = [&](const std::string& c) { ImVec4 v = svgColor(c); v.w *= op; return ImGui::ColorConvertFloat4ToU32(v); };
        if (nm == "circle") {
            const ImVec2 c = P(num("cx"), num("cy"));
            const float r = float(num("r")) * scale;
            if (fill.rfind("url(#", 0) == 0) {                            // a radial gradient: rings from the outside in
                const auto& st = grads[fill.substr(5, fill.size() - 6)];
                if (st.size() >= 2)
                    for (int k = 24; k >= 1; k--) {
                        const float t = k / 24.f;
                        const auto& s0 = st.front(); const auto& s1 = st.back();
                        const float f = std::max(0.f, std::min(1.f, (t - s0.first) / std::max(1e-3f, s1.first - s0.first)));
                        ImVec4 v(s0.second.x + (s1.second.x - s0.second.x) * f, s0.second.y + (s1.second.y - s0.second.y) * f, s0.second.z + (s1.second.z - s0.second.z) * f, s0.second.w + (s1.second.w - s0.second.w) * f);
                        dl->AddCircleFilled(c, r * t, ImGui::ColorConvertFloat4ToU32(v));
                    }
            } else if (!fill.empty() && fill != "none") dl->AddCircleFilled(c, r, col(fill));
            else if (fill.empty() && stroke.empty()) dl->AddCircleFilled(c, r, col("#000000"));
            if (!stroke.empty()) {
                if (dashed) for (int k = 0; k < 24; k += 2) dl->PathArcTo(c, r, k * 6.2832f / 24, (k + 1) * 6.2832f / 24, 4), dl->PathStroke(col(stroke), 0, sw);
                else dl->AddCircle(c, r, col(stroke), 0, sw);
            }
        } else if (nm == "rect") {
            const ImVec2 p0 = P(num("x"), num("y")), p1 = P(num("x") + num("width"), num("y") + num("height"));
            if (!fill.empty() && fill != "none") dl->AddRectFilled(p0, p1, col(fill));
            if (!stroke.empty()) dl->AddRect(p0, p1, col(stroke), 0, 0, sw);
        } else if (nm == "path") {
            const std::string d = svgAttr(tag, "d");
            std::vector<double> nums;
            char cmd = 0;
            double cx = 0, cy = 0, qx = 0, qy = 0;
            std::size_t k = 0;
            auto readNums = [&](int n) {
                nums.clear();
                while ((int)nums.size() < n && k < d.size()) {
                    while (k < d.size() && (d[k] == ' ' || d[k] == ',')) k++;
                    char* end = nullptr;
                    nums.push_back(std::strtod(d.c_str() + k, &end));
                    k = end - d.c_str();
                }
            };
            while (k < d.size()) {
                while (k < d.size() && d[k] == ' ') k++;
                if (k < d.size() && std::isalpha(static_cast<unsigned char>(d[k]))) cmd = d[k++];
                if (cmd == 'M') { readNums(2); cx = nums[0]; cy = nums[1]; dl->PathLineTo(P(cx, cy)); }
                else if (cmd == 'L') { readNums(2); cx = nums[0]; cy = nums[1]; dl->PathLineTo(P(cx, cy)); }
                else if (cmd == 'Q') { readNums(4); qx = nums[0]; qy = nums[1]; dl->PathBezierQuadraticCurveTo(P(qx, qy), P(nums[2], nums[3]), 12); cx = nums[2]; cy = nums[3]; }
                else if (cmd == 'T') { readNums(2); qx = 2 * cx - qx; qy = 2 * cy - qy; dl->PathBezierQuadraticCurveTo(P(qx, qy), P(nums[0], nums[1]), 12); cx = nums[0]; cy = nums[1]; }
                else break;
            }
            dl->PathStroke(col(stroke.empty() ? "#7a86a0" : stroke), 0, sw);
        } else if (nm == "text") {
            const std::size_t e = svg.find("</text>", i);
            const std::string t = svg.substr(i, e - i);
            i = e + 7;
            const float fs = float(num("font-size", 12)) * scale;
            ImFont* f = svgAttr(tag, "font-weight") == "700" ? fonts().bold : fonts().regular;
            const ImVec2 ts = f->CalcTextSizeA(fs, FLT_MAX, 0, t.c_str());
            const ImVec2 at = P(num("x"), num("y"));
            dl->AddText(f, fs, ImVec2(at.x - (svgAttr(tag, "text-anchor") == "middle" ? ts.x / 2 : 0), at.y - f->GetFontBaked(fs)->Ascent), col(fill.empty() ? "#000000" : fill), t.c_str());
        }
    }
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
        dl->AddRectFilled(ImVec2(o.x, o.y + 4 * s), ImVec2(o.x + 60 * s, o.y + 64 * s), IM_COL32(120, 140, 200, 31), 12 * s);
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
            if (ImGui::Button((std::string("▶ ") + st.lab).c_str())) {   // close the guide and set the lab to show it
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
    if (ImGui::Button(displayable("❓ What am I even looking at?").c_str(), ImVec2(-1, 0))) { app.atomLearn = 1; app.atomLearnJump = "top"; }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("A plain-words guide to the atom on screen");
}

void atomLearnButtons(AppState& app) {
    ImGui::SeparatorText("Learn");
    const char* labels[4] = {"❓ What am I even looking at?", "📏 Scale: how empty is an atom?", "⚖ Weight: what does an atom weigh?", "🕰 Timeline: how we pictured the atom"};
    for (int i = 0; i < 4; i++)
        if (ImGui::Button(displayable(labels[i]).c_str(), ImVec2(-1, 0))) {
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
    if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse)) {
        if (app.atomLearnJump == "top") { ImGui::SetScrollY(0); app.atomLearnJump.clear(); }
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
