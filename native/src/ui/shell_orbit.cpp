// What the Orbit Lab puts in the new UI's chip, toolbar and panels. Every control calls the same code as the old side panel (ui/orbit_actions.h, orbit_panel.cpp): the state is the same.
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "atom/atom_core.h"
#include "imgui.h"
#include "orbit/card_text.h"
#include "orbit/date_text.h"
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

// ---- the toolbar ----
std::vector<ToolItem> orbitTools(AppState& app) {
    std::vector<ToolItem> v;
    ToolItem play;
    play.kind = ToolItem::Button; play.id = "tool.play"; play.tip = app.paused ? "Play (Space)" : "Pause (Space)"; play.icon = app.paused ? Icon::Play : Icon::Pause;
    play.act = [&app] { app.paused = !app.paused; };
    v.push_back(play);
    v.push_back(separator());
    auto menu = [&](const char* id, const char* tip, Icon icon, PanelId panel) { ToolItem t; t.kind = ToolItem::Menu; t.id = id; t.tip = tip; t.icon = icon; t.panel = panel; v.push_back(t); };
    menu("tool.speed", "Time speed", Icon::Clock, PanelId::Speed);
    menu("tool.show", "Show: moons, dwarf planets, comets, belts, planet physics", Icon::Eye, PanelId::Show);
    menu("tool.pick", "Pick a body", Icon::Target, PanelId::Pick);
    menu("tool.scale", "Zoom, sizes and trails", Icon::Ruler, PanelId::Scale);
    v.push_back(separator());
    ToolItem seg;
    seg.kind = ToolItem::Segment; seg.id = "tool.view"; seg.tip = "2D or 3D view; the arrow: view options"; seg.panel = PanelId::View; seg.labels = {"2D", "3D"}; seg.sel = app.set.view3D ? 1 : 0;
    seg.pick = [&app](int i) { app.set.view3D = i == 1; };
    v.push_back(seg);
    v.push_back(separator());
    ToolItem save;
    save.kind = ToolItem::Button; save.id = "tool.save"; save.tip = "Save picture"; save.icon = Icon::Camera; save.act = [&app] { app.shot.requested = true; };
    v.push_back(save);
    return v;
}

std::vector<ToolItem> atomTools(AppState& app) {                // the Atom Lab's toolbar so far: its view and orbital buttons move here in stage 5 (until then: the Old panel and the bar under the picture)
    std::vector<ToolItem> v;
    ToolItem play;
    play.kind = ToolItem::Button; play.id = "tool.play"; play.tip = app.paused ? "Play (Space)" : "Pause (Space)"; play.icon = app.paused ? Icon::Play : Icon::Pause;
    play.act = [&app] { app.paused = !app.paused; };
    v.push_back(play);
    v.push_back(separator());
    ToolItem save;
    save.kind = ToolItem::Button; save.id = "tool.save"; save.tip = "Save picture"; save.icon = Icon::Camera; save.act = [&app] { app.shot.requested = true; };
    v.push_back(save);
    return v;
}

// ---- the chip, the card, the legend ----
ChipInfo chipInfo(AppState& app) {
    ChipInfo c;
    if (app.lab == 1) {                                         // the Atom Lab: the element (the periodic table dropdown comes in stage 5; until then the Old panel has it)
        const atom::AtomParams& a = app.atom.p;
        c.title = std::string(atom::name(a.Z)) + "-" + std::to_string(a.Z + a.N);
        c.sub = std::to_string(a.Z) + " protons  ·  " + std::to_string(a.N) + " neutrons";
        return c;
    }
    const ChipText t = chipText(app);
    c.title = t.date;
    c.sub = t.extra.empty() ? t.speed : t.speed + "  ·  " + t.extra;
    c.today = true; c.opens = true;
    return c;
}

std::string subjectName(const AppState& app) {
    if (app.lab == 1) return atom::name(app.atom.p.Z);
    const orbit::Selection& sel = app.view.sel;
    if (sel.kind == orbit::Selection::None) return "";
    for (const PickGroup& g : pickGroups(app))
        for (const PickItem& it : g.items) if (it.sel == sel) return it.name;
    return "";
}

std::string subjectKind(const AppState& app) {
    const orbit::Selection& sel = app.view.sel;
    {   // the card's own title is "Earth · rocky planet (home)": the part after the dot is the kind
        orbit::CardOptions o;
        o.paused = app.paused; o.tspeed = app.set.tspeed; o.trueScale = app.set.trueScale; o.moonsOn = app.set.moons; o.beltsOn = app.set.belts; o.farOn = app.set.far;
        orbit::Card card;
        if (sel.kind != orbit::Selection::None && orbit::cardText(app.sys, sel, o, card)) { const std::size_t d = card.title.find(" · "); if (d != std::string::npos) return card.title.substr(d + std::string(" · ").size()); }
    }
    if (sel.kind == orbit::Selection::Body) { if (const orbit::Body* b = orbit::findBody(app.sys, sel.bodyId)) return b->kind; }
    if (sel.kind == orbit::Selection::Moon) { if (const orbit::Body* b = orbit::findBody(app.sys, sel.bodyId)) return "moon of " + b->name; }
    if (sel.kind == orbit::Selection::Star) return "star";
    if (sel.kind == orbit::Selection::Bary) return "balance point of the Solar System";
    return "";
}

bool subjectLines(const AppState& app, std::string& title, std::vector<std::string>& lines) {
    if (app.view.sel.kind == orbit::Selection::None) return false;
    orbit::CardOptions o;
    o.paused = app.paused; o.tspeed = app.set.tspeed; o.trueScale = app.set.trueScale; o.moonsOn = app.set.moons; o.beltsOn = app.set.belts; o.farOn = app.set.far;
    orbit::Card card;
    if (!orbit::cardText(app.sys, app.view.sel, o, card)) return false;
    title = card.title;
    { const std::size_t d = title.find(" · "); if (d != std::string::npos) title = title.substr(0, d); }   // the name; the kind after the dot is the card's one-line kind
    lines = card.lines;
    return true;
}

void legendRows(const AppState& app, std::vector<std::pair<std::string, std::string>>& rows) {
    for (const PickGroup& g : pickGroups(app)) {
        const bool shown = std::string(g.title) == "Dwarf planets" ? app.set.dwarfs : std::string(g.title) == "Famous comets" ? app.set.comets : std::string(g.title) == "Moons" ? app.set.moons
                         : std::string(g.title).rfind("Belts", 0) == 0 ? app.set.belts : std::string(g.title) == "Far away" ? app.set.far : true;
        if (!shown || g.items.empty()) continue;
        rows.push_back({g.items.front().color, std::string(g.title) + "  " + std::to_string(g.items.size())});
    }
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

}  // namespace

void timePanel(AppState& app, Panel& p) {
    const Frame& f = F();
    const Metrics& m = f.m;
    pTitle(p, "Date and time");
    pBody(p, orbit::dateLine(app.sys));
    if (pButton(p, "time.today", "Today", Face::Primary)) { resetToToday(app); app.dateNote.clear(); }
    pGap(p, m.s8);
    pHeading(p, "Set the date and time (Greenwich time)");
    ImVec2 o = here(p);
    inputField("##shelldate", app.dateText, sizeof app.dateText, "YYYY-MM-DD", p.w);
    p.y += m.control + m.s8;
    // the time of day: a 24-hour dial (one hand for the hour, a thinner one for the minutes) and two fields; the fields are the exact input
    int hh = 12, mm = 0;
    if (std::sscanf(app.timeText, "%d:%d", &hh, &mm) != 2) { hh = 12; mm = 0; }
    hh = std::min(23, std::max(0, hh)); mm = std::min(59, std::max(0, mm));
    const float D = static_cast<float>(app::shell::unitPx(120, f.dpi, f.tf)), R = D / 2;
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
    // the two fields: − value +
    const float x2 = o.x + D + m.s12, w2 = p.w - D - m.s12, bw = static_cast<float>(m.control);
    auto stepper = [&](const char* tag, const char* name, int value, int lo, int hi, float y) {
        text(dl, Type::Caption, false, ImVec2(x2, y), cMuted(), name);
        const float by = y + lineH(Type::Caption);
        const std::string idm = std::string("time.") + tag + "-", idp = std::string("time.") + tag + "+";
        int d = 0;
        if (button(idm.c_str(), ImVec2(x2, by), ImVec2(bw, bw), "−")) d = -1;
        char b[8];
        std::snprintf(b, sizeof b, "%02d", value);
        text(dl, Type::Body, true, ImVec2(x2 + bw + (w2 - 2 * bw - width(Type::Body, true, b)) / 2, by + (bw - px(Type::Body)) / 2), cText(), b);
        if (button(idp.c_str(), ImVec2(x2 + w2 - bw, by), ImVec2(bw, bw), "+")) d = 1;
        (void)lo; (void)hi;
        return d;
    };
    const int dh = stepper("hour", "Hour", hh, 0, 23, o.y);
    const int dm = stepper("minute", "Minute", mm, 0, 59, o.y + lineH(Type::Caption) + bw + m.s12);
    if (dh) setTime(app, hh + dh, mm);
    if (dm) setTime(app, hh, mm + dm);
    p.y += D + m.s8;
    if (pButton(p, "time.go", "Go to this date", Face::Primary)) jumpFromBoxes(app);
    pGap(p, m.s8);
    pHeading(p, "Time speed");
    pBody(p, tspeedText(app.set.tspeed));
    float ts = static_cast<float>(app.set.tspeed);
    if (pSlider(p, "time.speed", &ts, static_cast<float>(units::kTspeedMin), static_cast<float>(units::kTspeedMax))) app.set.tspeed = ts;
    pGap(p, m.s8);
    pHeading(p, "Jump to");
    if (pButton(p, "time.journey", app.journey.active ? "■ Stop the journey" : "Journey: Earth to the galactic centre")) {
        if (app.journey.active) journeyStop(app); else journeyStart(app);
    }
    int n = 0;
    const DateJump* jumps = dateJumps(n);
    for (int i = 1; i < n; i++) {
        const std::string id = std::string("jump.") + jumps[i].date;
        if (pRow(p, id.c_str(), jumps[i].name)) doDateJump(app, i);
        if (ImGui::IsItemHovered()) ui::SetTooltip("%s", jumps[i].tip);
    }
    pGap(p, m.s4);
    pCaption(p, app.dateNote.empty() ? orbit::datePickHint() : app.dateNote);
}

// ---- the toolbar's panels ----
double orbitPanelWidthEm(PanelId id) {
    switch (id) {
    case PanelId::Chip: return 24;
    case PanelId::Speed: return 20;
    case PanelId::Show: return 22;
    case PanelId::Pick: return 22;
    case PanelId::Scale: return 22;
    case PanelId::View: return 20;
    default: return 0;
    }
}

bool buildOrbitPanel(PanelId id, AppState& app, Panel& p, bool& closeAfter) {
    const Metrics& m = F().m;
    orbit::ViewSettings& set = app.set;
    switch (id) {
    case PanelId::Chip: timePanel(app, p); return true;
    case PanelId::Speed: {
        pTitle(p, "Time speed");
        pBody(p, tspeedText(set.tspeed));
        float ts = static_cast<float>(set.tspeed);
        if (pSlider(p, "speed.slider", &ts, static_cast<float>(units::kTspeedMin), static_cast<float>(units::kTspeedMax))) set.tspeed = ts;
        pCaption(p, "Space: play / pause");
        return true;
    }
    case PanelId::Show: {
        static bool notes = false;
        pTitle(p, "Show");
        pCheck(p, "show.moons", "Moons", &set.moons);
        pCheck(p, "show.dwarfs", "Dwarf planets", &set.dwarfs);
        pCheck(p, "show.comets", "Famous comets", &set.comets);
        pCheck(p, "show.belts", "Belts and Oort cloud", &set.belts);
        pGap(p, m.s8);
        pHeading(p, "Planet physics");
        int real = set.realPlanets ? 1 : 0;
        if (pSegmented(p, "show.physics", {"Simple", "Real"}, &real)) set.realPlanets = real == 1;
        if (set.realPlanets) {
            pCheck(p, "show.gr", "General relativity (1PN)", &set.relativity);
            pCheck(p, "show.wobble", "Exaggerate the Sun's wobble (×50)", &set.exaggerateWobble);
        }
        if (pRow(p, "show.notes", notes ? "Notes about this choice  (hide)" : "Notes about this choice  (show)")) notes = !notes;
        if (notes) pCaption(p, set.realPlanets ? orbit::realNoteText(set.relativity) : orbit::simpleNoteText());
        return true;
    }
    case PanelId::Pick: {
        pTitle(p, "Pick a body");
        pCaption(p, "Click one to fly there. It stays in the middle of the picture. Your time speed is not changed.");
        for (const PickGroup& g : pickGroups(app)) {
            pGap(p, m.s4);
            pHeading(p, g.title);
            for (const PickItem& it : g.items) {
                const std::string rid = "pick." + it.name;
                if (pRow(p, rid.c_str(), it.name, app.view.sel == it.sel, it.color)) { pickItem(app, it); closeAfter = true; }
                if (!it.tip.empty() && ImGui::IsItemHovered()) ui::SetTooltip("%s", it.tip.c_str());
            }
        }
        return true;
    }
    case PanelId::Scale: {
        pTitle(p, "Zoom, sizes and trails");
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
        return true;
    }
    case PanelId::View: {
        pTitle(p, "View options");
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
        return true;
    }
    default: return false;
    }
}

}  // namespace ui::shellimpl
