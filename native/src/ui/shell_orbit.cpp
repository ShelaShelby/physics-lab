// What the Orbit Lab puts in the new UI's chip, toolbar and panels. Every control calls the same code as the old side panel (ui/orbit_actions.h, orbit_panel.cpp): the state is the same.
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "atom/atom_core.h"
#include "atom/atom_tools.h"
#include "imgui.h"
#include "orbit/card_text.h"
#include "orbit/date_text.h"
#include "orbit/marker_colors.h"
#include "orbit/real_errors.h"
#include "orbit/units.h"
#include "ui/fonts.h"
#include "ui/journey_dates.h"
#include "ui/look.h"
#include "ui/orbit_actions.h"
#include "ui/shell_internal.h"
#include "ui/shell_test.h"
#include "ui/style_draw.h"

namespace ui::shellimpl {

using namespace sh;
using app::shell::PanelId;
using app::shell::Metrics;

// ---- the toolbar: view and lab controls only (stage 4: play / pause moved into the chip, Save picture into the menu) ----
std::vector<ToolItem> orbitTools(AppState& app) {
    std::vector<ToolItem> v;
    ToolItem seg;
    seg.kind = ToolItem::Segment; seg.id = "tool.view"; seg.tip = "2D or 3D view"; seg.labels = {"2D", "3D"}; seg.sel = app.set.view3D ? 1 : 0;      // (no caret: the view options are in the Controls panel)
    seg.pick = [&app](int i) { app.set.view3D = i == 1; };
    v.push_back(seg);
    ToolItem reset; reset.kind = ToolItem::Button; reset.id = "tool.reset"; reset.tip = "Reset view: turn the picture back and put it in the middle"; reset.icon = Icon::Reset;
    reset.act = [&app] { resetViewAction(app); };
    v.push_back(reset);
    return v;                                             // (stage 5c.1: the Controls panel opens from its handle on the picture's right edge, not from the toolbar)
}


// ---- the chip, the card, the legend ----
ChipInfo chipInfo(AppState& app) {
    ChipInfo c;
    c.play = true; c.paused = app.paused;
    if (app.lab == 1) {                                         // the Atom Lab: "U · Uranium-238" and "92 protons · 146 neutrons"; a click opens the periodic table (stage 5b)
        const atom::ChipText t = atom::elementChipText(app.atom.p.Z, app.atom.p.N);
        c.title = t.title; c.sub = t.sub;
        c.opens = true;
        return c;
    }
    const ChipText t = chipText(app);
    c.title = t.date;
    c.opens = true;
    if (app.journey.active) { c.sub = "Journey in progress"; c.stop = true; return c; }
    c.sub = t.extra.empty() ? t.speed : t.speed + "  ·  " + t.extra;
    c.today = true;
    return c;
}

std::string subjectName(const AppState& app) {
    if (app.lab == 1) return atom::name(app.atom.p.Z);
    const orbit::Selection& sel = app.view.sel;
    if (sel.kind == orbit::Selection::None) return "";
    static orbit::Selection cachedSel;                       // the name of a selection never changes: looked up once, not 47 strings built every frame
    static std::string cachedName;
    if (cachedSel == sel && !cachedName.empty()) return cachedName;
    for (const PickGroup& g : pickGroups(app))
        for (const PickItem& it : g.items) if (it.sel == sel) { cachedSel = sel; cachedName = it.name; return cachedName; }
    return "";
}

bool subjectCardOf(const AppState& app, orbit::SubjectCard& out) {
    const orbit::Selection& sel = app.view.sel;
    if (sel.kind == orbit::Selection::None || !app.view.cardOk || !(app.view.cardSel == sel)) return false;
    const std::string beltKey = sel.kind == orbit::Selection::Belt && sel.moon >= 0 && sel.moon < static_cast<int>(app.sys.belts.size()) ? app.sys.belts[sel.moon].key : "";
    out = orbit::subjectCard(sel, beltKey, app.view.lastCard);
    return true;
}

bool cardAction(AppState& app, CardAction& out) {
    if (slowMoonsAvailable(app)) { out.id = "card.slowmoons"; out.text = "Slow time to watch moons"; out.act = [&app] { slowMoons(app); }; return true; }
    if (slowSpinAvailable(app)) { out.id = "card.slowspin"; out.text = "Slow time to watch it turn"; out.act = [&app] { slowSpin(app); }; return true; }
    return false;
}

std::vector<LegendEntry> legendEntries(const AppState& app) {
    std::vector<LegendEntry> v;
    const auto hex = [](const int c[3]) { char b[8]; std::snprintf(b, sizeof b, "#%02x%02x%02x", c[0], c[1], c[2]); return std::string(b); };
    std::vector<std::string> planets, dwarfs;
    for (const PickGroup& g : pickGroups(app)) {
        const std::string t = g.title;
        for (const PickItem& it : g.items) {
            if (t == "Star and planets" && it.name != "Sun" && it.name != "Barycentre") planets.push_back(it.color);
            if (t == "Dwarf planets") dwarfs.push_back(it.color);
        }
    }
    int sun[3];
    orbit::sunColourBytes(sun);
    v.push_back({LegendEntry::Disc, {hex(sun)}, "The Sun"});
    v.push_back({LegendEntry::Dots, planets, "Planets: each has its own colour"});
    if (!planets.empty()) v.push_back({LegendEntry::Line, {planets[2 % planets.size()]}, "A planet's orbit and tail, in its colour"});
    if (app.set.dwarfs) v.push_back({LegendEntry::Dots, dwarfs, "Dwarf planets: their own colours"});
    if (app.set.moons) v.push_back({LegendEntry::Disc, {orbit::marks::kMoon}, "A moon"});
    if (app.set.comets) v.push_back({LegendEntry::Dots, {"#ffffff", hex(orbit::marks::kIonTail), hex(orbit::marks::kDustTail)}, "A comet: a bright head, a blue gas tail, a yellow dust tail"});
    if (app.set.belts) {
        std::vector<std::string> cols;
        for (const orbit::BeltGroup& b : app.sys.belts) cols.push_back(b.color);
        v.push_back({LegendEntry::Dots, cols, "A belt: each dot is a rock (a few thousand stand in for millions)"});
    }
    v.push_back({LegendEntry::Cross, {"#ffffff"}, "The barycentre: the balance point of the Solar System"});
    v.push_back({LegendEntry::Band, {hex(orbit::marks::kHabitable)}, "Habitable zone (rough)"});
    v.push_back({LegendEntry::Line, {"#9aa6bd"}, "Distance rings, labelled in AU"});
    if (app.set.far) {
        v.push_back({LegendEntry::Dots, {"#ffffff"}, "A far-away star (its own colour)"});
        v.push_back({LegendEntry::Ring, {orbit::marks::kProbeRing}, "A spacecraft: Voyager 1 and 2"});
        v.push_back({LegendEntry::Ring, {orbit::marks::kObject}, "A far-away object that is not a star"});
    }
    v.push_back({LegendEntry::Ring, {orbit::marks::kHighlight}, "The thing you picked"});
    v.push_back({LegendEntry::Dots, {orbit::marks::kAxisX, orbit::marks::kAxisY, orbit::marks::kAxisZ}, "The little axes: x red, y green, z blue"});
    return v;
}

// ---- the Time panel ----
namespace {

bool inputField(const char* id, char* buf, std::size_t n, const char* hint, float width) {
    const Frame& f = F();
    ImGui::PushFont(nullptr, app::shell::fontBase(app::shell::Type::Body, f.dpi, f.tf));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(static_cast<float>(f.m.s8), (f.m.control - f.m.body) / 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ui::corner(static_cast<float>(f.m.radiusCtl)));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, f.outline ? 1.0f : 0.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(cFace()));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(cText()));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImGui::ColorConvertU32ToFloat4(cMuted()));
    ImGui::SetNextItemWidth(width);
    const bool changed = ImGui::InputTextWithHint(id, hint, buf, n);
    noteItem(id);
    auditText("body", ImGui::GetFontSize(), buf[0] ? buf : hint);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
    ImGui::PopFont();
    return changed;
}

void setTime(AppState& app, int hh, int mm) {
    hh = ((hh % 24) + 24) % 24; mm = ((mm % 60) + 60) % 60;
    std::snprintf(app.timeText, sizeof app.timeText, "%02d:%02d", hh, mm);
}

bool g_manual = false;          // the Time panel shows the date, the dial and Go ("Set manually"); also shown whenever a date was jumped to
bool g_moreJumps = false;       // all six history jumps, not the first four
bool g_dateInfo = false;        // the help words behind (i)
char g_pickFilter[48] = "";    // the Pick a body list's filter text (cleared when the panel closes)
constexpr int kJumpsShown = 4;  // the Time panel lists this many history jumps before "More"

}  // namespace

void pickFilterClear() { g_pickFilter[0] = 0; }

void todayAction(AppState& app) { resetToToday(app); app.dateNote.clear(); g_manual = false; }

void timePanel(AppState& app, Panel& p) {
    const Frame& f = F();
    const Metrics& m = f.m;
    {   // the title, with the (i) button at the right end of the same row
        const ImVec2 o = here(p);
        const float hb = static_cast<float>(m.iconBtn);
        if (iconButton("time.info", ImVec2(o.x + p.w - hb, o.y), ImVec2(hb, hb), Icon::Info, "About dates: any date from 1800 to 2100, Greenwich time", g_dateInfo)) g_dateInfo = !g_dateInfo;
        text(p.dl, Type::Title, true, o, cText(), "Date and time");
        p.y += std::max(lineH(Type::Title), hb) + m.s4;
    }
    pBody(p, orbit::dateLine(app.sys));
    const bool manual = app.sys.jumped || g_manual;
    int mode = manual ? 1 : 0;
    if (pSegmented(p, "time.mode", {"Live", "Set manually"}, &mode)) {
        if (mode == 0) { todayAction(app); app.paused = false; }      // Live: from the real date and time now, running at the chosen speed
        else g_manual = true;
    }
    if (g_dateInfo) pCaption(p, orbit::datePickHint());
    if (!manual) pCaption(p, "Live: the lab starts from the real date and time now and runs at the speed below.");
    else {
        // the date and the time of day: the fields are the exact input; the 24-hour dial is a second way to set the hour
        const float goW = static_cast<float>(m.em(4));
        ImVec2 o = here(p);
        inputField("##shelldate", app.dateText, sizeof app.dateText, "YYYY-MM-DD", p.w - goW - m.s8);
        if (button("time.go", ImVec2(o.x + p.w - goW, o.y), ImVec2(goW, static_cast<float>(m.control)), "Go", Face::Primary)) jumpFromBoxes(app);
        p.y += m.control + m.s8;
        int hh = 12, mm = 0;
        if (std::sscanf(app.timeText, "%d:%d", &hh, &mm) != 2) { hh = 12; mm = 0; }
        hh = std::min(23, std::max(0, hh)); mm = std::min(59, std::max(0, mm));
        const float D = static_cast<float>(app::shell::unitPx(112, f.dpi, f.tf)), R = D / 2;
        o = here(p);
        const ImVec2 c(o.x + R, o.y + R);
        ImGui::SetCursorScreenPos(o);
        ImGui::InvisibleButton("time.dial", ImVec2(D, D));
        noteItem("time.dial");
        if (ImGui::IsItemActive()) {
            const ImVec2 mp = ImGui::GetIO().MousePos;
            float a = std::atan2(mp.x - c.x, -(mp.y - c.y));
            if (a < 0) a += 6.2831853f;
            setTime(app, static_cast<int>(std::lround(a / 6.2831853f * 24)) % 24, mm);
            std::sscanf(app.timeText, "%d:%d", &hh, &mm);
        }
        ImDrawList* dl = p.dl;
        dl->AddCircleFilled(c, R, cFace(), 48);
        dl->AddCircle(c, R, cEdge(), 48, 1.0f);
        for (int k = 0; k < 24; k++) {
            const float a = k / 24.0f * 6.2831853f, sa = std::sin(a), ca = -std::cos(a), len = k % 3 == 0 ? 0.14f : 0.07f;
            dl->AddLine(ImVec2(c.x + sa * R * (0.96f - len), c.y + ca * R * (0.96f - len)), ImVec2(c.x + sa * R * 0.96f, c.y + ca * R * 0.96f), cMuted(), 1.0f);
        }
        for (int k = 0; k < 4; k++) {                                              // 0, 6, 12, 18 inside the ticks
            const std::string lab = std::to_string(k * 6);
            const float a = k / 4.0f * 6.2831853f, tw = width(Type::Caption, false, lab);
            text(dl, Type::Caption, false, ImVec2(c.x + std::sin(a) * R * 0.62f - tw / 2, c.y - std::cos(a) * R * 0.62f - px(Type::Caption) / 2), cMuted(), lab);
        }
        const float ha = (hh + mm / 60.0f) / 24.0f * 6.2831853f, ma = mm / 60.0f * 6.2831853f;
        dl->AddLine(c, ImVec2(c.x + std::sin(ma) * R * 0.78f, c.y - std::cos(ma) * R * 0.78f), cMuted(), 1.5f);
        dl->AddLine(c, ImVec2(c.x + std::sin(ha) * R * 0.52f, c.y - std::cos(ha) * R * 0.52f), cAccent(), 3.0f);
        dl->AddCircleFilled(c, 3.0f, cAccent());
        const float x2 = o.x + D + m.s12, w2 = p.w - D - m.s12, bw = static_cast<float>(m.control);
        auto stepper = [&](const char* tag, const char* name, int value, float y) {
            text(dl, Type::Caption, false, ImVec2(x2, y), cMuted(), name);
            const float by = y + lineH(Type::Caption);
            const std::string idm = std::string("time.") + tag + "-", idp = std::string("time.") + tag + "+";
            int d = 0;
            if (button(idm.c_str(), ImVec2(x2, by), ImVec2(bw, bw), "−")) d = -1;
            char b[8];
            std::snprintf(b, sizeof b, "%02d", value);
            text(dl, Type::Body, true, ImVec2(x2 + bw + (w2 - 2 * bw - width(Type::Body, true, b)) / 2, by + (bw - px(Type::Body)) / 2), cText(), b);
            if (button(idp.c_str(), ImVec2(x2 + w2 - bw, by), ImVec2(bw, bw), "+")) d = 1;
            return d;
        };
        const int dh = stepper("hour", "Hour (Greenwich)", hh, o.y);
        const int dm = stepper("minute", "Minute", mm, o.y + lineH(Type::Caption) + bw + m.s8);
        if (dh) setTime(app, hh + dh, mm);
        if (dm) setTime(app, hh, mm + dm);
        p.y += std::max(D, 2.0f * (lineH(Type::Caption) + bw) + m.s8) + m.s4;
    }
    if (!app.dateNote.empty()) pCaption(p, app.dateNote);
    pStat(p, "Time speed", tspeedText(app.set.tspeed));
    float ts = static_cast<float>(app.set.tspeed);
    if (pSlider(p, "time.speed", &ts, static_cast<float>(units::kTspeedMin), static_cast<float>(units::kTspeedMax))) app.set.tspeed = ts;
    pGap(p, m.s4);
    pHeading(p, "Jump to");
    if (pButton(p, "time.journey", app.journey.active ? "■ Stop the journey" : "Journey: Earth to the galactic centre")) {
        if (app.journey.active) journeyStop(app); else journeyStart(app);
    }
    if (ImGui::IsItemHovered()) ui::SetTooltip("An automatic trip that zooms out, step by step, from Earth to the centre of our galaxy.\nTouch anything to stop it; your view and settings come back.");
    int n = 0;
    const DateJump* jumps = dateJumps(n);
    const int last = g_moreJumps ? n : std::min(n, 1 + kJumpsShown);
    for (int i = 1; i < last; i++) {
        const std::string id = std::string("jump.") + jumps[i].date;
        if (pRow(p, id.c_str(), jumps[i].name)) doDateJump(app, i);
        if (ImGui::IsItemHovered()) ui::SetTooltip("%s", jumps[i].tip);
    }
    if (n > 1 + kJumpsShown && pRow(p, "time.more", g_moreJumps ? "Fewer" : "More dates")) g_moreJumps = !g_moreJumps;
}

// ---- the toolbar's panels ----
double orbitPanelWidthEm(PanelId id) {
    switch (id) {
    case PanelId::Chip: return 24;
    default: return 0;
    }
}

bool buildOrbitPanel(PanelId id, AppState& app, Panel& p, bool& /*closeAfter*/) {
    switch (id) {
    case PanelId::Chip: if (app.lab != 0) return false; timePanel(app, p); return true;
    default: return false;
    }
}

// ---- the Controls panel (stage 5c): what the Show, Pick, Scale, view and Time speed panels held, same controls, same words, same tooltips, in folding sections ----
void orbitControls(AppState& app, Panel& p, const SectionFn& section) {
    const Metrics& m = F().m;
    orbit::ViewSettings& set = app.set;
    bool closeAfter = false;                                   // (the Pick list used it to close its dropdown; the docked panel stays open)
    (void)closeAfter;
    if (section("speed", "Time speed", true)) {
        pBody(p, tspeedText(set.tspeed));
        float ts = static_cast<float>(set.tspeed);
        if (pSlider(p, "speed.slider", &ts, static_cast<float>(units::kTspeedMin), static_cast<float>(units::kTspeedMax))) set.tspeed = ts;
        pCaption(p, "Space: play / pause");
    }
    if (section("show", "Show", true)) {
        static bool notes = false;
        pCheck(p, "show.moons", "Moons", &set.moons);
        pCheck(p, "show.dwarfs", "Dwarf planets", &set.dwarfs);
        pCheck(p, "show.comets", "Famous comets", &set.comets);
        pCheck(p, "show.belts", "Belts and Oort cloud", &set.belts);
        pCheck(p, "show.far", "Far-away things (zoom out to find them)", &set.far);
        pGap(p, m.s8);
        pHeading(p, "Sky: stars in the background");
        int sky = static_cast<int>(set.sky);
        if (pSegmented(p, "show.sky", {"Soft", "Full", "Off"}, &sky)) set.sky = static_cast<orbit::SkyMode>(sky);
        pCaption(p, sky == 0 ? "dim close up, brighter as you zoom out" : sky == 1 ? "always bright" : "plain background");
        if (set.sky != orbit::SkyMode::Off) {
            pHeading(p, "Constellations");
            int c = static_cast<int>(set.cons);
            if (pSegmented(p, "show.cons", {"Off", "Lines", "Lines and names"}, &c)) set.cons = static_cast<orbit::ConsMode>(c);
        }
        pGap(p, m.s4);
        pHeading(p, "Planet physics");
        int real = set.realPlanets ? 1 : 0;
        if (pSegmented(p, "show.physics", {"Simple", "Real"}, &real)) set.realPlanets = real == 1;
        if (set.realPlanets) {
            pCheck(p, "show.gr", "General relativity (1PN)", &set.relativity);
            if (ImGui::IsItemHovered()) { ui::BeginTooltip(); ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26); ImGui::TextUnformatted(relativityTip().c_str()); ImGui::PopTextWrapPos(); ImGui::EndTooltip(); }
            pCheck(p, "show.wobble", "Exaggerate the Sun's wobble (×50)", &set.exaggerateWobble);
            if (ImGui::IsItemHovered()) { ui::BeginTooltip(); ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26); ImGui::TextUnformatted(wobbleTip().c_str()); ImGui::PopTextWrapPos(); ImGui::EndTooltip(); }
        }
        if (pRow(p, "show.notes", notes ? "Notes about this choice  (hide)" : "Notes about this choice  (show)")) notes = !notes;
        if (notes) pCaption(p, set.realPlanets ? orbit::realNoteText(set.relativity) : orbit::simpleNoteText());
    }
    if (section("pick", "Pick a body", false)) {
        char* filter = g_pickFilter;
        pCaption(p, "Click one to fly there. It stays in the middle of the picture. Your time speed is not changed.");
        {
            const ImVec2 o = here(p);
            ImGui::SetCursorScreenPos(o);
            inputField("##pickfilter", filter, sizeof g_pickFilter, "Filter the list", p.w);
            p.y += m.control + m.s4; p.gap = m.s4;
        }
        auto lower = [](std::string x) { for (char& ch : x) if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a'); return x; };
        const std::string q = lower(filter);
        for (const PickGroup& g : pickGroups(app)) {
            bool any = false;
            for (const PickItem& it : g.items) if (q.empty() || lower(it.name).find(q) != std::string::npos) any = true;
            if (!any) continue;
            pGap(p, m.s4);
            pHeading(p, g.title);
            for (const PickItem& it : g.items) {
                if (!q.empty() && lower(it.name).find(q) == std::string::npos) continue;
                const std::string rid = "pick." + it.name;
                if (pRow(p, rid.c_str(), it.name, app.view.sel == it.sel, it.color)) { pickItem(app, it); filter[0] = 0; }
                if (!it.tip.empty() && ImGui::IsItemHovered()) ui::SetTooltip("%s", it.tip.c_str());
            }
        }
        pGap(p, m.s8);
        pCaption(p, "Real orbits from NASA/JPL data. Moons show when you zoom in or click their planet (until you zoom in a lot, their orbits are drawn wider than real so they can be seen).");
    }
    if (section("scale", "Zoom, sizes and trails", true)) {
        pBody(p, "Zoom: " + zoomLabel(set.zoom));
        float z = static_cast<float>(set.zoom);
        if (pSlider(p, "scale.zoom", &z, static_cast<float>(units::kZoomMin), static_cast<float>(units::kZoomMax))) setZoom(app, z);
        pCaption(p, "Mouse wheel zooms, drag moves the view");
        pGap(p, m.s8);
        pHeading(p, "Sizes");
        int sizes = set.trueScale ? 0 : 1;
        if (pSegmented(p, "scale.sizes", {"True scale", "Easy to see"}, &sizes)) set.trueScale = sizes == 0;
        pCaption(p, sizes == 0 ? "planets and Sun as big as they really are" : "planets drawn bigger");
        pHeading(p, "Trails");
        int trails = set.fullTrails ? 1 : 0;
        if (pSegmented(p, "scale.trails", {"Short tail", "Full orbit trails"}, &trails)) set.fullTrails = trails == 1;
        pCaption(p, trails == 0 ? "a little tail behind each object" : "every object's whole path");
    }
    if (section("view", "View options", true)) {
        if (set.view3D) {
            bool pan = app.view.panMode;
            if (pCheck(p, "view.pan", "Drag moves the view", &pan)) app.view.panMode = pan;
            bool spin = app.view.spin;
            if (pCheck(p, "view.spin", "Spin", &spin)) app.view.spin = spin;
            pCaption(p, "Drag rotates the view unless this is ticked. Right-drag or Shift-drag always moves the view.");
        }
        if (app.view.sel.kind != orbit::Selection::None && app.view.sel.kind != orbit::Selection::Belt) {
            bool fol = app.view.follow;
            if (pCheck(p, "view.follow", "Follow the selected body", &fol)) app.view.follow = fol;
        }
        if (pButton(p, "view.reset", "↺ Reset view")) resetViewAction(app);
    }
}

// ---- search: everything the Orbit Lab can do ----
void orbitSearch(AppState& app, const SearchAdd& add) {
    for (const PickGroup& g : pickGroups(app))
        for (const PickItem& it : g.items) { PickItem copy = it; add("Bodies", it.name, "body:" + it.name, [&app, copy] { pickItem(app, copy); }); }
    int n = 0;
    const DateJump* jumps = dateJumps(n);
    for (int i = 1; i < n; i++) add("Dates", jumps[i].name, std::string("date:") + jumps[i].date, [&app, i] { doDateJump(app, i); });
    add("Dates", "Today: back to the real date", "date:today", [&app] { todayAction(app); });
    add("Dates", "Live: run from now", "date:live", [&app] { todayAction(app); app.paused = false; });
    add("Dates", "Set the date and time by hand", "date:manual", [] { g_manual = true; shellOpenPanel(PanelId::Chip); });
    add("Dates", "Journey: Earth to the galactic centre", "date:journey", [&app] { if (!app.journey.active) journeyStart(app); });
    if (app.journey.active) add("Dates", "Stop the journey", "date:stopjourney", [&app] { journeyStop(app); });
    orbit::ViewSettings& s = app.set;
    const auto toggle = [&](const char* group, const char* name, const char* id, bool* v) {
        add(group, std::string(name) + (*v ? ": turn off" : ": turn on"), id, [v] { *v = !*v; });
    };
    add("Time", app.paused ? "Play" : "Pause", "set:play", [&app] { app.paused = !app.paused; });
    add("Time", "Time speed", "set:speed", [] { shellOpenPanel(PanelId::Speed); });
    add("Time", "Date and time", "set:time", [] { shellOpenPanel(PanelId::Chip); });
    toggle("Show", "Moons", "set:moons", &s.moons);
    toggle("Show", "Dwarf planets", "set:dwarfs", &s.dwarfs);
    toggle("Show", "Famous comets", "set:comets", &s.comets);
    toggle("Show", "Belts and Oort cloud", "set:belts", &s.belts);
    toggle("Show", "Far-away things", "set:far", &s.far);
    add("Show", "Stars: Soft", "set:sky.soft", [&s] { s.sky = orbit::SkyMode::Soft; });
    add("Show", "Stars: Full", "set:sky.full", [&s] { s.sky = orbit::SkyMode::Full; });
    add("Show", "Stars: Off", "set:sky.off", [&s] { s.sky = orbit::SkyMode::Off; });
    add("Show", "Constellations: Off", "set:cons.off", [&s] { s.cons = orbit::ConsMode::Off; });
    add("Show", "Constellations: Lines", "set:cons.lines", [&s] { s.sky = s.sky == orbit::SkyMode::Off ? orbit::SkyMode::Soft : s.sky; s.cons = orbit::ConsMode::Lines; });
    add("Show", "Constellations: Lines and names", "set:cons.names", [&s] { s.sky = s.sky == orbit::SkyMode::Off ? orbit::SkyMode::Soft : s.sky; s.cons = orbit::ConsMode::LinesAndNames; });
    add("Physics", "Planet physics: Simple", "set:simple", [&s] { s.realPlanets = false; });
    add("Physics", "Planet physics: Real", "set:real", [&s] { s.realPlanets = true; });
    toggle("Physics", "General relativity (1PN)", "set:gr", &s.relativity);
    toggle("Physics", "Exaggerate the Sun's wobble (×50)", "set:wobble", &s.exaggerateWobble);
    add("Scale", "Zoom", "set:zoom", [] { shellOpenPanel(PanelId::Scale); });
    add("Scale", "Sizes: True scale", "set:true", [&s] { s.trueScale = true; });
    add("Scale", "Sizes: Easy to see", "set:easy", [&s] { s.trueScale = false; });
    add("Scale", "Trails: Short tail", "set:trail.short", [&s] { s.fullTrails = false; });
    add("Scale", "Trails: Full orbit trails", "set:trail.full", [&s] { s.fullTrails = true; });
    add("View", "2D view", "set:2d", [&s] { s.view3D = false; });
    add("View", "3D view", "set:3d", [&s] { s.view3D = true; });
    add("View", "Spin the 3D view", "set:spin", [&app] { app.view.spin = !app.view.spin; app.set.view3D = true; });
    add("View", "Drag moves the view", "set:pan", [&app] { app.view.panMode = !app.view.panMode; });
    add("View", "Follow the selected body", "set:follow", [&app] { if (app.view.sel.kind != orbit::Selection::None) app.view.follow = !app.view.follow; });
    add("View", "Reset view", "set:resetview", [&app] { resetViewAction(app); });
    add("View", "Pick a body: the list", "set:picklist", [] { shellOpenPanel(PanelId::Pick); });
    if (slowMoonsAvailable(app)) add("View", "Slow time to watch moons", "set:slowmoons", [&app] { slowMoons(app); });
    if (slowSpinAvailable(app)) add("View", "Slow time to watch it turn", "set:slowspin", [&app] { slowSpin(app); });
}

}  // namespace ui::shellimpl
