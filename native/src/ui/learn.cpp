#include "learn.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>

#include "fonts.h"
#include "imgui.h"
#include "look.h"
#include "orbit/card_text.h"
#include "orbit/format.h"
#include "orbit/sky.h"
#include "orbit/spin.h"
#include "orbit/units.h"
#include "rich_text.h"
#include "svg_icon.h"
#include "ui/journey_dates.h"
#include "ui/orbit_panel.h"

namespace ui {

using namespace orbit;
using namespace units;

namespace {

#include "learn_data.inc"

const char* const kTitles[4] = {"", "What am I even looking at?", "Why do we have seasons?", "Timeline of the Solar System"};

// put the live values into a guide's text
std::string fillIn(std::string s, const char* marker, const std::string& value) {
    for (std::size_t at; (at = s.find(marker)) != std::string::npos;) s.replace(at, std::strlen(marker), value);
    return s;
}

// ---- the ▶ buttons of the timeline: fly the lab to a place or a moment (the web's orbGo, orbLook, orbFar, orbDate) ----
const Body* bodyNamed(const AppState& app, const char* name) {
    for (const Body& b : app.sys.bodies) if (b.name == name) return &b;
    return nullptr;
}
void orbGo(AppState& app, const char* name, double zoom, bool moons) {
    app.view.follow = false;
    app.view.sel = Selection{};
    if (name && name[0])
        if (const Body* b = bodyNamed(app, name)) {
            app.view.sel = {Selection::Body, b->id};
            app.view.cardHidden = false;
            app.view.follow = moons;
            if (moons && app.set.trueScale && !b->moons.empty()) {   // true scale: zoom out until the moons' orbits fit
                double a = 0;
                for (const Moon& m : b->moons) a = std::max(a, m.data->aKm);
                zoom = zoomFor(app, a, 0.32 * std::min(app.canvasW, app.canvasH));
            }
        }
    app.view.ox = app.view.oy = 0;
    setZoom(app, zoom);
    app.view.ox = app.view.oy = 0;                         // time speed is left as you set it
}
void orbLook(AppState& app, const char* name, double km) {   // fly to a planet (or comet) and keep it in the middle
    orbGo(app, name, 0, false);
    if (app.view.sel.kind == Selection::None) return;
    app.view.follow = true;
    double easy = 0.8;
    const std::string n = name;
    if (n == "Venus" || n == "Earth") easy = 0.8; else if (n == "Mars") easy = 0.6; else if (n == "Jupiter") easy = 1.3; else if (n == "Saturn") easy = 1.2;
    setZoom(app, app.set.trueScale ? zoomFor(app, km, 100) : easy);
}
void runStep(AppState& app, const TimelineStep& st) {
    journeyStop(app);
    const std::string lab = st.lab;
    if (lab == "go") orbGo(app, st.target, st.zoom, st.moons);
    else if (lab == "look") orbLook(app, st.target, st.km);
    else if (lab == "date") { app.dateNote = jumpToDate(app, st.ms); orbLook(app, st.target, st.km); }
    else if (lab == "far") {
        if (st.ms) app.dateNote = jumpToDate(app, st.ms);
        app.set.far = true;
        FarItem f;
        const int idx = farIndexOf(st.target);
        if (!farItem(app.sys, idx, f)) return;
        app.view.select({Selection::Far, 0, idx}, true);
        setZoom(app, std::log10(13.6 / (f.au * kAU / kSimLength)));   // the same zoom as its Go to tile
        app.view.ox = app.view.oy = 0;
    } else if (lab == "sun") {
        app.view.sel = {Selection::Star}; app.view.follow = true; app.view.cardHidden = false;
        setZoom(app, app.set.trueScale ? zoomFor(app, units::kSunRadius / units::kKm, 90) : 0.8);
    }
}

// ---- the three guides ----
void jumpBar(AppState& app, const LearnSection* secs, int n) {
    if (app.learnJump == "bar") { ImGui::SetScrollHereY(0); app.learnJump.clear(); }
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(ImGui::GetStyle().FramePadding.x, 2));
    const float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    for (int i = 0; i < n; i++) {                          // a row of the headings, wrapped
        const std::string label = displayable(secs[i].title);
        const float w = ImGui::CalcTextSize(label.c_str()).x + ImGui::GetStyle().FramePadding.x * 2;
        if (i > 0 && ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + w < right) ImGui::SameLine();
        ImGui::PushID(i);
        if (ImGui::SmallButton(label.c_str())) app.learnJump = secs[i].key;
        ImGui::PopID();
    }
    ImGui::PopStyleVar();
    ImGui::Spacing();
}

void lookGuide(AppState& app) {
    richText(std::string("<p class=\"lrnP\">") + kLookIntro + "</p>");
    const auto chip = [](const char* label, bool on) {
        if (on) pushOn();
        const bool clicked = ImGui::SmallButton(label);
        if (on) popOn();
        return clicked;
    };
    ImGui::TextDisabled("View");
    if (chip("2D (top-down)", !app.set.view3D)) app.set.view3D = false;
    ImGui::SameLine();
    if (chip("3D (rotatable)", app.set.view3D)) app.set.view3D = true;
    ImGui::TextDisabled("Sizes");
    if (chip("True scale", app.set.trueScale)) app.set.trueScale = true;
    ImGui::SameLine();
    if (chip("Easy to see", !app.set.trueScale)) app.set.trueScale = false;
    ImGui::Spacing();

    Card card;
    CardOptions opt;
    opt.tspeed = app.set.tspeed; opt.trueScale = app.set.trueScale; opt.moonsOn = app.set.moons; opt.beltsOn = app.set.belts; opt.farOn = app.set.far; opt.paused = app.paused;
    const bool hasCard = cardText(app.sys, app.view.sel, opt, card);
    const int variant = (app.set.view3D ? 4 : 0) + (app.set.trueScale ? 0 : 2) + (hasCard ? 1 : 0);
    const LearnSection* secs = kLook[variant];
    jumpBar(app, secs, kLookSectionCount);
    const std::string bary = fmt::fixed(length(app.sys.barycentre()) / kSunRadius, 1);
    for (int i = 0; i < kLookSectionCount; i++) {
        ImGui::Spacing();
        if (app.learnJump == secs[i].key) { ImGui::SetScrollHereY(0); app.learnJump.clear(); }
        richHeading(secs[i].title);
        ImGui::SameLine();
        ImGui::PushID(i);
        if (ImGui::SmallButton("↑")) app.learnJump = "bar";
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Back to the list of headings");
        ImGui::PopID();
        std::string h = secs[i].html;
        h = fillIn(h, "{WHEN}", dateLine(app.sys));
        h = fillIn(h, "{CARD}", card.title);
        h = fillIn(h, "{TSPEED}", tspeedText(app.set.tspeed));
        h = fillIn(h, "{BARY}", bary);
        richText(h);
    }
}

// hours of daylight at a latitude when the Sun is overhead at latitude dec (the Sun's centre at the horizon; air ignored)
double dayLenHours(double lat, double dec) {
    const double R = kPi / 180, x = -std::tan(lat * R) * std::tan(dec * R);
    if (x >= 1) return 0;
    if (x <= -1) return 24;
    return 2 * std::acos(x) / R / 15;
}

// Earth's four places on its orbit, seen from above, the axis always leaning the same way (the web's little drawing)
void seasonsDiagram() {
    const float w = std::min(ImGui::GetContentRegionAvail().x, 340 * ImGui::GetFontSize() / 16), s = w / 240;
    const ImVec2 o = ImGui::GetCursorScreenPos();
    const float ox = o.x + (ImGui::GetContentRegionAvail().x - w) / 2;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 fg = ImGui::GetColorU32(ImGuiCol_Text), faint = ImGui::GetColorU32(ImGuiCol_Text, 0.3f);
    const auto P = [&](float x, float y) { return ImVec2(ox + x * s, o.y + y * s); };
    for (int i = 0; i < 72; i += 2) {                     // the dashed orbit
        const float a0 = i * 3.14159f / 36, a1 = (i + 1) * 3.14159f / 36;
        dl->AddLine(P(120 + 82 * std::cos(a0), 100 + 72 * std::sin(a0)), P(120 + 82 * std::cos(a1), 100 + 72 * std::sin(a1)), faint);
    }
    dl->AddCircleFilled(P(120, 100), 11 * s, IM_COL32(0xf0, 0xb0, 0x20, 255));
    const struct { const char* n; float x, y; } E[] = {{"Dec", 120, 28}, {"Mar", 38, 100}, {"Jun", 120, 172}, {"Sep", 202, 100}};
    for (const auto& e : E) {
        dl->AddCircleFilled(P(e.x, e.y), 9 * s, IM_COL32(0x2f, 0x6f, 0xed, 255));
        dl->AddLine(P(e.x, e.y), P(e.x, e.y - 21), fg, 2 * s);
        dl->AddCircleFilled(P(e.x, e.y - 21), 2.2f * s, fg);
        const std::string n = e.n;
        const float tx = n == "Sep" ? e.x - 13 : n == "Mar" ? e.x + 13 : e.x + 14, ty = n == "Dec" || n == "Jun" ? e.y + 4 : e.y + 20;
        const float tw = ImGui::CalcTextSize(e.n).x * (11 * s / ImGui::GetFontSize());
        dl->AddText(nullptr, 11 * s, P(n == "Sep" ? tx - tw / s : tx, ty - 9), fg, e.n);
    }
    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 190 * s));
}

void seasonsGuide(AppState& app) {
    for (const LearnBlock& b : kSeasons) {
        const std::string kind = b.kind;
        if (kind == "p") richText(std::string("<p class=\"lrnP\">") + b.html + "</p>");
        else if (kind == "muted") richText(std::string("<p class=\"lrnP lrnMuted\">") + b.html + "</p>");
        else if (kind == "diagram") seasonsDiagram();
        else if (std::string(b.html) == "moments") {       // jump the lab to a real moment (half an hour after, so the season has just changed)
            int n = 0;
            for (const SeasonMoment& m : kSeasonMoments) {
                if (n++ % 2) ImGui::SameLine();
                const std::string label = std::string(m.name) + "\n" + m.date;
                if (ImGui::Button(label.c_str(), ImVec2((ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / (n % 2 ? 2 : 1), 0))) {
                    int64_t ms;
                    char d[16], t[8];
                    std::snprintf(d, sizeof d, "%04d-%02d-%02d", m.y, m.mo, m.d);
                    std::snprintf(t, sizeof t, "%02d:%02d", m.h, m.mi);
                    if (parseDateTime(d, t, ms)) app.dateNote = jumpToDate(app, ms + 30 * 60000);
                }
            }
            if (wideButton("Today")) { journeyStop(app); resetToToday(app); app.dateNote = "Back to today."; }
        } else if (std::string(b.html) == "live") {       // the lab's own Earth, live
            const Body* e = bodyNamed(app, "Earth");
            EarthSeason q;
            if (!e || !earthSeason(*e, app.sys, q)) { richText("<p class=\"lrnP\">The real Solar System is needed for this (it is on when the lab opens).</p>"); continue; }
            const std::string when = fmt::dateUTC(app.sys.currentUnixMs()), dec = std::fabs(q.dlat) < 0.05 ? "the equator" : fmt::fixed(std::fabs(q.dlat), 1) + "° " + (q.dlat > 0 ? "north" : "south");
            richText("<p class=\"lrnP\"><b>" + when + " in the lab.</b> Northern " + kSeasonNorth[q.k] + ", southern " + kSeasonSouth[q.k] +
                     ". The Sun is straight overhead at " + dec + ". Earth is " + fmt::fixed(q.distAU, 3) + " AU from the Sun (" + fmt::millionKm(q.distAU * 149.6) + ").</p>");
            if (ImGui::BeginTable("daylight", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
                ImGui::TableSetupColumn("Place"); ImGui::TableSetupColumn("Daylight"); ImGui::TableSetupColumn("Noon Sun height");
                ImGui::TableHeadersRow();
                const struct { double la; const char* nm; } rows[] = {{80, "80° N"}, {60, "60° N"}, {30, "30° N"}, {0, "Equator"}, {-30, "30° S"}, {-60, "60° S"}, {-80, "80° S"}};
                for (const auto& r : rows) {
                    const double h = dayLenHours(r.la, q.dlat), hgt = 90 - std::fabs(r.la - q.dlat);
                    const int hh = static_cast<int>(std::floor(h + 1e-9)), mm = static_cast<int>(std::lround((h - hh) * 60));
                    const std::string day = h >= 24 ? "24 h, never sets" : h <= 0 ? "0 h, never rises" : mm == 60 ? std::to_string(hh + 1) + " h 0 min" : std::to_string(hh) + " h " + std::to_string(mm) + " min";
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(r.nm);
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(day.c_str());
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(hgt <= 0 ? "below the horizon" : (fmt::fixed(hgt, 0) + "°").c_str());
                }
                ImGui::EndTable();
            }
            ImGui::Spacing();
        } else if (std::string(b.html) == "planets") {    // each planet's lean, measured live from its real axis
            if (ImGui::BeginTable("planets", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
                ImGui::TableSetupColumn("Planet", ImGuiTableColumnFlags_WidthFixed); ImGui::TableSetupColumn("Lean", ImGuiTableColumnFlags_WidthFixed); ImGui::TableSetupColumn("Seasons");
                ImGui::TableHeadersRow();
                for (const PlanetSeason& p : kPlanetSeasons) {
                    const Body* b2 = bodyNamed(app, p.name);
                    if (!b2) continue;
                    const double t = axisTilt(*b2, app.sys);
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(p.name);
                    ImGui::TableNextColumn(); ImGui::TextUnformatted((fmt::fixed(t, 1) + "°").c_str());
                    ImGui::TableNextColumn(); ImGui::TextWrapped("%s", fillIn(p.text, "{VENUS}", fmt::fixed(180 - t, 1)).c_str());
                }
                ImGui::EndTable();
            }
            ImGui::Spacing();
        }
    }
}

void timeline(AppState& app) {
    richText(std::string("<p class=\"lrnP\">") + kTimelineIntro + "</p>");
    int i = 0;
    for (const TimelineStep& st : kTimeline) {
        ImGui::Separator();
        const float s = ImGui::GetFontSize() / 16.0f;      // the step picture on the left, as in the Atom Lab's timeline (the web's .lrnIc, 60 px wide)
        const ImVec2 o = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(o.x, o.y + 4 * s), ImVec2(o.x + 60 * s, o.y + 64 * s), IM_COL32(120, 140, 200, 31), 12 * s);
        svgIcon(st.svg, ImVec2(o.x + 2 * s, o.y + 6 * s), s);
        ImGui::SetCursorScreenPos(ImVec2(o.x + 72 * s, o.y + 4 * s));
        ImGui::BeginGroup();
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::TextUnformatted(st.when);
        ImGui::PopStyleColor();
        richHeading(st.title);
        richText(std::string("<p class=\"lrnP lrnMuted\">") + st.who + "</p>");
        richText(std::string("<p class=\"lrnP\">") + st.text + "</p>");
        if (st.button[0]) {
            ImGui::PushID(i);
            if (ImGui::Button((std::string("▶ ") + st.button).c_str())) {
                runStep(app, st);
                app.learn = 0;                             // close the guide, so you see the lab
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

void learnButtons(AppState& app) {
    ImGui::SeparatorText("Learn");
    if (wideButton(displayable("❓ What am I even looking at?").c_str())) { app.learn = 1; app.learnJump = "top"; }
    if (wideButton(displayable("🌍 Why do we have seasons?").c_str())) { app.learn = 2; app.learnJump = "top"; }
    if (wideButton(displayable("🕰 Timeline: how we understood the Solar System").c_str())) { app.learn = 3; app.learnJump = "top"; }
}

void learnWindow(AppState& app) {
    if (app.learn <= 0 || app.learn > 3) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(std::min(vp->WorkSize.x * 0.6f, 46 * ImGui::GetFontSize()), vp->WorkSize.y * 0.85f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.4f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    bool open = true;
    const std::string title = std::string(kTitles[app.learn]) + "###learn";   // one window; the title changes with the guide
    if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse)) {
        if (app.learnJump == "top") { ImGui::SetScrollY(0); app.learnJump.clear(); }
        if (const char* sc = std::getenv("PL_LEARN_SCROLL")) ImGui::SetScrollY(float(std::atof(sc)));   // for automatic checks
        ImGui::PushTextWrapPos(0);
        if (app.learn == 1) lookGuide(app);
        else if (app.learn == 2) seasonsGuide(app);
        else timeline(app);
        ImGui::PopTextWrapPos();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) open = false;
    }
    ImGui::End();
    if (!open) app.learn = 0;
}

}  // namespace ui
