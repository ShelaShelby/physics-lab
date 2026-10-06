#include "ui/shell.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "app/app_data.h"
#include "app/display_scale.h"
#include "app/shell.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "orbit/units.h"
#include "ui/app_pages.h"
#include "ui/app_tabs.h"
#include "ui/atom_panel.h"
#include "ui/fonts.h"
#include "ui/look.h"
#include "ui/orbit_actions.h"
#include "ui/reading.h"
#include "ui/settings.h"
#include "ui/shell_internal.h"
#include "ui/shell_test.h"
#include "ui/shell_widgets.h"
#include "ui/style_draw.h"

namespace ui {

namespace {

using namespace app::shell;
using namespace shellimpl;
using sh::Face;
using sh::Icon;
using sh::Type;

struct State {
    UiState ui;
    float topBarH = 0;
    bool askDefaults = false;
    bool frozen = false;                                  // the user dragged the picture: the centre shift holds still
    orbit::Selection lastSel;
    std::string lastOccluders;
    char query[96] = "";
    int searchSel = 0;
    bool searchFocus = false;
    std::map<PanelId, double> anchorX;                    // the centre x of the button a panel opens from (window pixels)
    bool contentsDummy = false;
} S;

const char* panelName(PanelId id) {
    switch (id) {
    case PanelId::Menu: return "menu"; case PanelId::LabMenu: return "labmenu"; case PanelId::Chip: return "chip"; case PanelId::Speed: return "speed";
    case PanelId::Show: return "show"; case PanelId::Pick: return "pick"; case PanelId::Scale: return "scale"; case PanelId::View: return "view";
    case PanelId::More: return "more"; case PanelId::Facts: return "facts"; case PanelId::None: return "none";
    }
    return "none";
}
bool fromTop(PanelId id) { return id == PanelId::Menu || id == PanelId::LabMenu || id == PanelId::Chip || id == PanelId::Facts; }
double widthEmOf(PanelId id) {
    if (const double w = orbitPanelWidthEm(id)) return w;
    switch (id) { case PanelId::Menu: return 18; case PanelId::LabMenu: return 14; case PanelId::Facts: return 22; case PanelId::More: return 18; default: return 20; }
}
void toggle(PanelId id) { S.ui.open = S.ui.open == id ? PanelId::None : id; S.ui.search = false; }
bool mouseIn(const Rect& r) { const ImVec2 mp = ImGui::GetIO().MousePos; return r.contains({mp.x, mp.y}); }
constexpr float kOldPanelUnits = 360.0f;      // the old side column's width, in units (app/display_scale.h: kPanelWidthU)
const char* const kTextSizes[4] = {"small", "normal", "large", "xl"};

// ---- the panels that every lab has ----
void menuPanel(AppState& app, sh::Panel& p, bool& close) {
    const Metrics& m = sh::F().m;
    sh::pTitle(p, "Menu");
    if (sh::pRow(p, "menu.history", "What's new (history)")) { app.showHistory = true; close = true; }
    if (sh::pRow(p, "menu.about", "About Physics Lab")) { app.showAbout = true; close = true; }
    if (sh::pRow(p, "menu.changelog", app.lab == 1 ? "Atom Lab changelog" : "Orbit Lab changelog")) { app.showLabLog = true; close = true; }
    if (sh::pRow(p, "menu.defaults", "Put this lab's settings back…")) { S.askDefaults = true; close = true; }
    sh::pGap(p, m.s8);
    sh::pHeading(p, "Text size");
    int cur = 1;
    for (int i = 0; i < 4; i++) if (app.prefs.textSize == kTextSizes[i]) cur = i;
    const float bw = static_cast<float>(m.control);
    const ImVec2 o = sh::here(p);
    if (sh::button("menu.smaller", o, ImVec2(bw, bw), "A−") && cur > 0) app.prefs.textSize = kTextSizes[cur - 1];
    if (sh::button("menu.larger", ImVec2(o.x + bw + m.s8, o.y), ImVec2(bw, bw), "A+") && cur < 3) app.prefs.textSize = kTextSizes[cur + 1];
    static const char* const names[4] = {"Small", "Normal", "Large", "Extra large"};
    sh::text(p.dl, Type::Body, false, ImVec2(o.x + 2 * bw + 2 * m.s8, o.y + (bw - sh::px(Type::Body)) / 2), sh::cText(), names[cur]);
    p.y += bw + m.s4;
}

void labMenuPanel(AppState& app, sh::Panel& p, bool& close) {
    sh::pTitle(p, "Labs");
    for (int i = 0; i < 2; i++)
        if (sh::pRow(p, i ? "lab.atom" : "lab.orbit", app::kLabInfo[i].name, app.lab == i)) { openLab(app, i); close = true; }
}

void factsPanel(AppState& app, sh::Panel& p) {
    const Metrics& m = sh::F().m;
    sh::pTitle(p, "Did you know?");
    sh::pBody(p, factText(app));
    if (factIsGift(app) && sh::pButton(p, "facts.gift", "Open the surprise", Face::Primary)) factOpenGift(app);
    const float bw = (p.w - m.s8) / 2;
    const ImVec2 o = sh::here(p);
    if (sh::button("facts.prev", o, ImVec2(bw, static_cast<float>(m.control)), "‹ Previous")) factStep(app, -1);
    if (sh::button("facts.next", ImVec2(o.x + bw + m.s8, o.y), ImVec2(bw, static_cast<float>(m.control)), "Next ›")) factStep(app, 1);
    p.y += m.control + m.s4;
    sh::pCaption(p, std::to_string(factPosition(app) + 1) + " of " + std::to_string(factCount()));
}

void morePanel(sh::Panel& p, std::vector<ToolItem>& hidden, bool& close) {
    sh::pTitle(p, "More");
    for (ToolItem& t : hidden) {
        if (t.kind == ToolItem::Sep) continue;
        if (t.kind == ToolItem::Segment) {
            for (int i = 0; i < static_cast<int>(t.labels.size()); i++)
                if (sh::pRow(p, (std::string(t.id) + "." + t.labels[i]).c_str(), t.labels[i] + " view", t.sel == i)) { t.pick(i); close = true; }
            if (sh::pRow(p, (std::string(t.id) + ".opts").c_str(), "View options…")) { S.ui.open = t.panel; close = false; }
            continue;
        }
        if (sh::pRow(p, (std::string(t.id) + ".more").c_str(), t.tip)) {
            if (t.kind == ToolItem::Menu) { S.ui.open = t.panel; close = false; }
            else { t.act(); close = true; }
        }
    }
}

// ---- search (Ctrl+K) ----
struct SearchEntry { SearchItem item; std::function<void()> act; };
std::vector<SearchEntry> searchEntries(AppState& app) {
    std::vector<SearchEntry> v;
    auto add = [&](const char* group, const std::string& label, std::function<void()> f) { v.push_back({{label, group, label}, std::move(f)}); };
    if (app.lab == 0) {
        for (const PickGroup& g : pickGroups(app))
            for (const PickItem& it : g.items) { PickItem copy = it; add("Bodies", it.name, [&app, copy] { pickItem(app, copy); }); }
        add("Views", "2D view", [&app] { app.set.view3D = false; });
        add("Views", "3D view", [&app] { app.set.view3D = true; });
        add("Views", "Reset view", [&app] { resetViewAction(app); });
        add("Views", "Today", [&app] { resetToToday(app); app.dateNote.clear(); });
        add("Settings", "Time speed", [] { S.ui.open = PanelId::Speed; });
        add("Settings", "Date and time", [] { S.ui.open = PanelId::Chip; });
        add("Settings", "Zoom, sizes and trails", [] { S.ui.open = PanelId::Scale; });
        add("Guides", "What am I even looking at?", [&app] { app.learn = 1; app.learnJump = "top"; });
        add("Guides", "Why do we have seasons?", [&app] { app.learn = 2; app.learnJump = "top"; });
        add("Guides", "Timeline: how we understood the Solar System", [&app] { app.learn = 3; app.learnJump = "top"; });
    }
    add("Settings", "Settings", [&app] { app.showSettings = true; });
    add("Settings", "Text size", [] { S.ui.open = PanelId::Menu; });
    add("Guides", "About Physics Lab", [&app] { app.showAbout = true; });
    add("Guides", "History", [&app] { app.showHistory = true; });
    add("Views", "Old panel (all the controls)", [&app] { app.showOldPanel = true; });
    return v;
}

Rect searchPanel(AppState& app, const Frame& fr, const Metrics& m) {
    const Rect r = placePanel(Anchor::Centre, m.em(36), sh::lastHeight("##search", static_cast<float>(m.em(14))), fr, m, fr.window.w / 2);
    sh::Panel p;
    if (!sh::beginPanel("##search", r, p, sh::Shape::Panel, true)) return r;
    std::vector<SearchEntry> entries = searchEntries(app);
    std::vector<SearchItem> items;
    for (const SearchEntry& e : entries) items.push_back(e.item);
    const sh::Frame& F = sh::F();
    ImGui::SetCursorScreenPos(p.origin);
    ImGui::PushFont(nullptr, fontBase(Type::Body, F.dpi, F.tf));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(static_cast<float>(m.s8), (m.control - m.body) / 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ui::corner(static_cast<float>(m.radiusCtl)));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(sh::cFace()));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(sh::cText()));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImGui::ColorConvertU32ToFloat4(sh::cMuted()));
    ImGui::SetNextItemWidth(p.w);
    if (S.searchFocus) { ImGui::SetKeyboardFocusHere(); S.searchFocus = false; }
    const bool enterPressed = ImGui::InputTextWithHint("##searchfield", "Search bodies, views, settings, guides", S.query, sizeof S.query, ImGuiInputTextFlags_EnterReturnsTrue);
    noteItem("search.field");
    auditText("body", ImGui::GetFontSize(), S.query[0] ? S.query : "Search");
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
    ImGui::PopFont();
    p.y += m.control + m.s8;
    const std::vector<int> hits = searchResults(S.query, items, 8);
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) S.searchSel++;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) S.searchSel--;
    S.searchSel = hits.empty() ? 0 : std::min<int>(std::max(S.searchSel, 0), static_cast<int>(hits.size()) - 1);
    int chosen = -1;
    for (std::size_t i = 0; i < hits.size(); i++) {
        const SearchItem& it = items[hits[i]];
        const ImVec2 o = sh::here(p);
        const std::string id = "search.hit" + std::to_string(i);
        if (sh::row(id.c_str(), o, p.w, it.label, static_cast<int>(i) == S.searchSel)) chosen = hits[i];
        const float gw = sh::width(Type::Caption, false, it.group);
        sh::text(p.dl, Type::Caption, false, ImVec2(o.x + p.w - gw - m.s8, o.y + (m.row - sh::px(Type::Caption)) / 2), sh::cMuted(), it.group);
        p.y += m.row;
    }
    if (enterPressed && !hits.empty()) chosen = hits[S.searchSel];
    if (S.query[0] && hits.empty()) sh::pCaption(p, "Nothing found. Try a planet, a setting or a guide.");
    sh::endPanel(p);
    if (chosen >= 0) { entries[chosen].act(); S.ui.search = false; S.query[0] = 0; }
    return r;
}

// ---- the top bar ----
struct Crumb { std::string text; int kind; };                 // kind: 0 link, 1 the lab (a dropdown), 2 link to the lab's picture, 3 where you are now (not a button)
std::vector<Crumb> crumbs(const AppState& app, int mode) {
    const std::string lab = app::kLabInfo[app.lab].name, subj = subjectName(app);
    std::vector<Crumb> v;
    if (mode == 0) v.push_back({"Physics Lab", 0});
    if (mode <= 1) v.push_back({lab, 1});
    if (app.lab == 0) {
        if (mode == 0) v.push_back({"Solar System", subj.empty() ? 3 : 2});
        if (!subj.empty()) v.push_back({subj, 3});
        else if (mode == 1) v.push_back({"Solar System", 3});
    } else v.push_back({subj, 3});
    return v;
}
double crumbWidth(const std::vector<Crumb>& v, const Metrics& m) {
    double w = 0;
    for (std::size_t i = 0; i < v.size(); i++) {
        w += v[i].kind == 3 ? sh::width(Type::Title, true, v[i].text) : sh::width(Type::Body, false, v[i].text) + 2.0 * m.s8 + (v[i].kind == 1 ? m.caretW : 0);
        if (i + 1 < v.size()) w += m.s16;
    }
    return w;
}

std::vector<std::string> readoutLines(const AppState& app, double fps) {
    std::vector<std::string> v;
    if (app.lab == 0) {
        v.push_back("Zoom " + zoomLabel(app.set.zoom));
        if (!app.view.scaleText.empty()) v.push_back("Scale " + app.view.scaleText);
        if (!app.view.lightText.empty()) v.push_back(app.view.lightText);
    }
    if (fps > 0) { char b[24]; std::snprintf(b, sizeof b, "%.0f fps", fps); v.push_back(b); }
    return v;
}

}  // namespace

bool newUiOn(const AppState& app) {
    static const char* env = std::getenv("PL_NEWUI");
    if (env) return std::string(env) == "on";
    return app.prefs.newUi == "on";
}
float shellTopBarH() { return S.topBarH; }
void shellNoteDrag() { S.frozen = true; }
bool shellSpace(AppState& app, bool textInput) {
    const KeyOutcome o = applyKey(S.ui, Key::Space, textInput, true);
    if (o.togglePlay) app.paused = !app.paused;
    return o.consumed;
}

void shellFrame(AppState& app, int winW, int winH, double fps, double dt) {
    sh::beginFrame();
    auditFrame();
    setReadingMode(true);
    const sh::Frame& SF = sh::F();
    const Metrics& m = SF.m;
    ImGuiIO& io = ImGui::GetIO();
    const double W = winW, H = winH;
    const float dpi = static_cast<float>(SF.dpi);

    // ---- keys: Esc closes the topmost thing, Ctrl+K opens search (Space is routed by main.cpp through shellSpace) ----
    S.ui.reading = pagesOpen(app) || app.pane != 0;
    S.ui.oldPanel = app.showOldPanel;
    if (!app.benchRunning) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && applyKey(S.ui, Key::Escape, io.WantTextInput, true).consumed) {
            app.showOldPanel = S.ui.oldPanel;
            if (!S.ui.reading) { closePages(app); app.pane = 0; }
            io.ClearInputKeys();
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_K, false)) { applyKey(S.ui, Key::CtrlK, io.WantTextInput, true); S.query[0] = 0; S.searchSel = 0; S.searchFocus = true; }
    }

    // ---- what the bars hold, and how big that makes them ----
    std::vector<ToolItem> tools = app.lab == 0 ? orbitTools(app) : atomTools(app);
    const double gap = m.s4;
    const double segW = std::max(sh::width(Type::Body, false, "2D"), sh::width(Type::Body, false, "3D")) + 2.0 * m.s12;
    std::vector<double> iw;
    for (const ToolItem& t : tools)
        iw.push_back(t.kind == ToolItem::Sep ? m.s12 : t.kind == ToolItem::Menu ? m.toolBtn + m.caretW : t.kind == ToolItem::Segment ? 2 * segW + gap + m.caretW : m.toolBtn);
    const int count = static_cast<int>(tools.size());
    const int shown = toolbarVisible(iw, gap, m.toolBtn, W, m);
    double tw = 2.0 * m.s8;
    for (int i = 0; i < shown; i++) tw += iw[i] + (i ? gap : 0);
    if (shown < count) tw += (shown ? gap : 0) + m.toolBtn;
    const std::vector<std::string> ro = readoutLines(app, fps);
    double rw = 0;
    for (const std::string& l : ro) rw = std::max(rw, static_cast<double>(sh::width(Type::Caption, false, l)));
    const double rh = ro.size() * sh::lineH(Type::Caption);
    const Frame fr = makeFrame(W, H, m, tw, rw + 2.0 * m.s4, rh);
    S.topBarH = static_cast<float>(m.topBar);

    // the top bar's pieces
    const ChipInfo chip = chipInfo(app);
    const double todayW = sh::width(Type::Body, false, "Today") + 2.0 * m.s12;
    const double chipW = 2.0 * m.s16 + std::max(sh::width(Type::Title, true, chip.title), sh::width(Type::Caption, false, chip.sub)) + (chip.today ? m.s8 + todayW : 0);
    const double oldW = sh::width(Type::Body, false, "Old panel") + 2.0 * m.s12;
    const int nIcons = 7;
    TopBarWidths tbw;
    tbw.menu = m.iconBtn;
    tbw.crumbFull = crumbWidth(crumbs(app, 0), m); tbw.crumbShort = crumbWidth(crumbs(app, 1), m); tbw.crumbIcon = m.toolBtn + m.caretW;
    tbw.chip = chipW;
    tbw.right = nIcons * m.iconBtn + (nIcons - 1) * gap + m.s8 + oldW;
    const TopBarPlan plan = planTopBar(W, m, tbw);

    std::vector<Rect> occ{fr.topBar, fr.toolbar};
    std::string log;

    // ---- the floating cards: the subject card (top right) and the legend (top left); each collapses to its title ----
    const auto card = [&](const char* key, const char* title, Anchor anchor, double emW, bool defCollapsed, const std::function<void(sh::Panel&)>& body) {
        const std::string name = std::string(app.lab == 0 ? "orbit." : "atom.") + key;
        const bool collapsed = panelCollapsed(app.prefs.shellPanels, name, defCollapsed);
        const double titleW = sh::width(Type::Title, true, title);
        const std::string wid = std::string("##card_") + name;
        sh::Panel p;
        Rect r;
        if (collapsed) {
            r = placePanel(anchor, collapsedWidth(titleW, m), m.control, fr, m, 0);
            if (!sh::beginPanel(wid.c_str(), r, p, sh::Shape::Pill, false)) return r;
            const bool hover = sh::hoveredRect(ImVec2(static_cast<float>(r.x), static_cast<float>(r.y)), ImVec2(static_cast<float>(r.w), static_cast<float>(r.h)));
            (void)hover;
            sh::text(p.dl, Type::Title, true, ImVec2(p.origin.x + m.s12, p.origin.y + (r.h - sh::px(Type::Title)) / 2), sh::cText(), title);
            sh::drawIcon(p.dl, Icon::Caret, ImVec2(p.origin.x + static_cast<float>(r.w) - m.s12 - m.caretW / 2.0f, p.origin.y + static_cast<float>(r.h) / 2), m.caretW * 0.75f, sh::cMuted());
            ImGui::SetCursorScreenPos(p.origin);
            if (ImGui::InvisibleButton((wid + "##t").c_str(), ImVec2(static_cast<float>(r.w), static_cast<float>(r.h)))) app.prefs.shellPanels = withPanelState(app.prefs.shellPanels, name, false);
            noteItem((std::string("card.") + key).c_str());
            ImGui::End();
        } else {
            r = placePanel(anchor, m.em(emW), sh::lastHeight(wid, static_cast<float>(m.em(8))), fr, m, 0);
            if (!sh::beginPanel(wid.c_str(), r, p, sh::Shape::Panel, true)) return r;
            sh::text(p.dl, Type::Title, true, sh::here(p), sh::cText(), title);
            const float hb = static_cast<float>(m.iconBtn);
            if (sh::iconButton((std::string("card.") + key + ".collapse").c_str(), ImVec2(p.origin.x + p.w - hb, p.origin.y - m.s4 * 0.0f), ImVec2(hb, hb), Icon::CaretUp, "Collapse to its title"))
                app.prefs.shellPanels = withPanelState(app.prefs.shellPanels, name, true);
            p.y += std::max(sh::lineH(Type::Title), hb) + m.s4;
            body(p);
            sh::endPanel(p);
        }
        occ.push_back(r);
        auditRect(std::string("card.") + name + (collapsed ? ".collapsed" : ""), static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h));
        auditNote(std::string("cardtitle ") + name + " " + title + (collapsed ? " collapsed" : " open"));
        return r;
    };
    const bool pane = app.pane != 0;                      // the Learn or Achievements page covers the lab: only the top bar stays
    if (app.lab == 0 && !pane) {
        std::vector<std::pair<std::string, std::string>> rows;
        legendRows(app, rows);
        card("legend", "Legend", Anchor::TopLeft, 16, true, [&](sh::Panel& p) {
            for (std::size_t i = 0; i < rows.size(); i++) sh::pRow(p, ("legend." + std::to_string(i)).c_str(), rows[i].second, false, rows[i].first.c_str());
        });
        std::string title;
        std::vector<std::string> lines;
        if (subjectLines(app, title, lines))
            card("card", title.c_str(), Anchor::TopRight, 20, false, [&](sh::Panel& p) {
                const std::string kind = subjectKind(app);
                if (!kind.empty()) sh::pCaption(p, kind);
                for (const std::string& l : lines) sh::pBody(p, l);
            });
    }

    // ---- the toolbar ----
    if (!pane) {
        sh::Panel tb;
        if (sh::beginPanel("##shellTool", fr.toolbar, tb, sh::Shape::Pill, false)) {
            float x = tb.origin.x + m.s8;
            const float y = tb.origin.y + (m.toolbarH - m.toolBtn) / 2.0f;
            for (int i = 0; i < shown; i++) {
                ToolItem& t = tools[i];
                const float w = static_cast<float>(iw[i]);
                if (t.kind == ToolItem::Sep) {
                    tb.dl->AddLine(ImVec2(x + w / 2, y + m.s4), ImVec2(x + w / 2, y + m.toolBtn - m.s4), sh::cEdge(), 1.0f);
                } else if (t.kind == ToolItem::Button) {
                    if (sh::iconButton(t.id, ImVec2(x, y), ImVec2(static_cast<float>(m.toolBtn), static_cast<float>(m.toolBtn)), t.icon, t.tip.c_str(), t.on)) t.act();
                } else if (t.kind == ToolItem::Menu) {
                    S.anchorX[t.panel] = x + w / 2;
                    if (sh::iconButton(t.id, ImVec2(x, y), ImVec2(w, static_cast<float>(m.toolBtn)), t.icon, t.tip.c_str(), S.ui.open == t.panel, static_cast<float>(m.caretW))) toggle(t.panel);
                } else {
                    const float sy = y + (m.toolBtn - m.row) / 2.0f;
                    int sel = t.sel;
                    if (sh::segmented(t.id, ImVec2(x, sy), static_cast<float>(2 * segW), t.labels, &sel)) t.pick(sel);
                    noteRect((std::string(t.id) + ".2d").c_str(), x, sy, static_cast<float>(segW), static_cast<float>(m.row));
                    noteRect((std::string(t.id) + ".3d").c_str(), x + static_cast<float>(segW), sy, static_cast<float>(segW), static_cast<float>(m.row));
                    const float cx = x + static_cast<float>(2 * segW + gap);
                    S.anchorX[t.panel] = cx + m.caretW / 2.0;
                    if (sh::iconButton((std::string(t.id) + ".caret").c_str(), ImVec2(cx, y), ImVec2(static_cast<float>(m.caretW), static_cast<float>(m.toolBtn)), Icon::Caret, "View options", S.ui.open == t.panel)) toggle(t.panel);
                }
                x += w + static_cast<float>(gap);
            }
            if (shown < count) {
                S.anchorX[PanelId::More] = x + m.toolBtn / 2.0;
                if (sh::iconButton("tool.more", ImVec2(x, y), ImVec2(static_cast<float>(m.toolBtn), static_cast<float>(m.toolBtn)), Icon::Dots, "More", S.ui.open == PanelId::More)) toggle(PanelId::More);
            }
            sh::endPanel(tb);
        }
        auditRect("toolbar", static_cast<float>(fr.toolbar.x), static_cast<float>(fr.toolbar.y), static_cast<float>(fr.toolbar.w), static_cast<float>(fr.toolbar.h));
    }

    // ---- the readout, bottom right: no box, an outline on the text so it reads over anything (its own window, so a saved picture does not carry it) ----
    if (!pane) {
        sh::Panel ropanel;
        if (sh::beginPanel("##shellRO", fr.readout, ropanel, sh::Shape::Overlay, false)) {
            for (std::size_t i = 0; i < ro.size(); i++) {
                const float tw2 = sh::width(Type::Caption, false, ro[i]);
                sh::outlined(ropanel.dl, Type::Caption, ImVec2(ropanel.origin.x + static_cast<float>(fr.readout.w) - tw2 - m.s4, ropanel.origin.y + i * sh::lineH(Type::Caption)), sh::cText(), sh::cBackdrop(0.9f), ro[i]);
            }
            ImGui::End();
        }
        auditRect("readout", static_cast<float>(fr.readout.x), static_cast<float>(fr.readout.y), static_cast<float>(fr.readout.w), static_cast<float>(fr.readout.h));
    }

    // ---- the top bar ----
    {
        sh::Panel bar;
        if (sh::beginPanel("##shellTop", fr.topBar, bar, sh::Shape::Bar, false)) {
            const float y0 = (m.topBar - m.iconBtn) / 2.0f, ib = static_cast<float>(m.iconBtn);
            float x = static_cast<float>(m.s12);
            S.anchorX[PanelId::Menu] = x + ib / 2.0;
            if (sh::iconButton("top.menu", ImVec2(x, y0), ImVec2(ib, ib), Icon::Menu, "Menu", S.ui.open == PanelId::Menu)) toggle(PanelId::Menu);
            x += ib + m.s8;
            // the breadcrumbs
            const std::vector<Crumb> cr = plan.crumbMode == 2 ? std::vector<Crumb>{} : crumbs(app, plan.crumbMode);
            if (plan.crumbMode == 2) {
                const float bw = static_cast<float>(m.toolBtn + m.caretW);
                S.anchorX[PanelId::LabMenu] = x + bw / 2.0;
                if (sh::iconButton("top.lab", ImVec2(x, (m.topBar - m.toolBtn) / 2.0f), ImVec2(bw, static_cast<float>(m.toolBtn)), Icon::Cube, "Labs", S.ui.open == PanelId::LabMenu, static_cast<float>(m.caretW))) toggle(PanelId::LabMenu);
            }
            for (std::size_t i = 0; i < cr.size(); i++) {
                const Crumb& c = cr[i];
                if (c.kind == 3) {
                    sh::text(bar.dl, Type::Title, true, ImVec2(x, (m.topBar - sh::px(Type::Title)) / 2), sh::cText(), c.text);
                    x += sh::width(Type::Title, true, c.text);
                } else {
                    const float bw = static_cast<float>(sh::width(Type::Body, false, c.text) + 2.0 * m.s8 + (c.kind == 1 ? m.caretW : 0));
                    const ImVec2 pos(x, (m.topBar - m.control) / 2.0f);
                    const std::string id = c.kind == 1 ? "top.lab" : c.kind == 0 ? "top.physics" : "top.subject";
                    const bool clicked = sh::button(id.c_str(), pos, ImVec2(bw, static_cast<float>(m.control)), c.text, Face::Quiet, c.kind == 1 && S.ui.open == PanelId::LabMenu);
                    if (c.kind == 1) {
                        sh::drawIcon(bar.dl, Icon::Caret, ImVec2(x + bw - m.s8 - m.caretW / 2.0f + m.s4, pos.y + m.control / 2.0f), m.caretW * 0.75f, sh::cMuted());
                        S.anchorX[PanelId::LabMenu] = x + bw / 2.0;
                    }
                    if (clicked) {
                        if (c.kind == 0) app.showAbout = true;
                        else if (c.kind == 1) toggle(PanelId::LabMenu);
                        else { app.view.sel = orbit::Selection{}; app.view.follow = false; }
                    }
                    x += bw;
                }
                if (i + 1 < cr.size()) { sh::text(bar.dl, Type::Body, false, ImVec2(x + (m.s16 - sh::width(Type::Body, false, "›")) / 2, (m.topBar - sh::px(Type::Body)) / 2), sh::cMuted(), "›"); x += m.s16; }
            }
            // the chip
            {
                const Rect c = plan.chip;
                const ImVec2 a(static_cast<float>(c.x), static_cast<float>(c.y)), b(static_cast<float>(c.right()), static_cast<float>(c.bottom()));
                bar.dl->PushClipRect(a, b, true);
                const bool open = S.ui.open == PanelId::Chip;
                ImGui::SetCursorScreenPos(a);
                if (chip.opens) ImGui::SetNextItemAllowOverlap();                    // the Today button sits on the chip and takes its own clicks
                const bool hit = chip.opens && ImGui::InvisibleButton("top.chip", ImVec2(static_cast<float>(c.w), static_cast<float>(c.h)));
                if (chip.opens) noteItem("top.chip");
                const bool hov = chip.opens && ImGui::IsItemHovered();
                bar.dl->AddRectFilled(a, b, open ? sh::cFacePress() : hov ? sh::cFaceHover() : sh::cFace(), static_cast<float>(c.h) / 2);
                if (sh::F().outline) bar.dl->AddRect(a, b, sh::cEdge(), static_cast<float>(c.h) / 2, 0, 1.0f);
                const float ty = a.y + (static_cast<float>(c.h) - sh::px(Type::Title) - sh::px(Type::Caption)) / 2;
                sh::text(bar.dl, Type::Title, true, ImVec2(a.x + m.s16, ty), sh::cText(), chip.title);
                sh::text(bar.dl, Type::Caption, false, ImVec2(a.x + m.s16, ty + sh::px(Type::Title)), sh::cMuted(), chip.sub);
                bar.dl->PopClipRect();
                S.anchorX[PanelId::Chip] = c.x + c.w / 2;
                if (hit) toggle(PanelId::Chip);
                if (chip.today) {
                    const ImVec2 tpos(static_cast<float>(c.right()) - m.s8 - static_cast<float>(todayW), static_cast<float>(c.y) + (static_cast<float>(c.h) - m.control) / 2);
                    if (sh::button("top.today", tpos, ImVec2(static_cast<float>(todayW), static_cast<float>(m.control)), "Today", Face::Quiet)) { resetToToday(app); app.dateNote.clear(); }
                }
                auditRect("chip", a.x, a.y, static_cast<float>(c.w), static_cast<float>(c.h));
            }
            // the right side: search, the other lab, Learn, Achievements, 💡, Theme, Settings, and the temporary Old panel
            float rx = static_cast<float>(W) - m.s12 - static_cast<float>(oldW);
            if (sh::button("top.old", ImVec2(rx, (m.topBar - m.control) / 2.0f), ImVec2(static_cast<float>(oldW), static_cast<float>(m.control)), "Old panel", Face::Secondary, app.showOldPanel)) app.showOldPanel = !app.showOldPanel;
            rx -= m.s8;
            const auto icon = [&](const char* id, Icon ic, const char* tip, bool on) {
                rx -= ib;
                const bool c = sh::iconButton(id, ImVec2(rx, y0), ImVec2(ib, ib), ic, tip, on);
                rx -= static_cast<float>(gap);
                return c;
            };
            if (icon("top.settings", Icon::Gear, "Settings", app.showSettings)) app.showSettings = true;
            const float themeX = rx - ib;
            if (icon("top.theme", Icon::Palette, "Theme", false)) ImGui::OpenPopup("##themes");
            themePicker(app);
            (void)themeX;
            S.anchorX[PanelId::Facts] = rx - ib / 2.0;
            if (icon("top.facts", Icon::Bulb, "Did you know?", S.ui.open == PanelId::Facts)) toggle(PanelId::Facts);
            if (icon("top.ach", Icon::Trophy, "Achievements", app.pane == 2)) app.pane = app.pane == 2 ? 0 : 2;
            if (icon("top.learn", Icon::Book, "Learn", app.pane == 1)) app.pane = app.pane == 1 ? 0 : 1;
            if (icon("top.swap", Icon::Swap, app.lab == 0 ? "Go to the Atom Lab" : "Go to the Orbit Lab", false)) openLab(app, 1 - app.lab);
            if (icon("top.search", Icon::Search, "Search (Ctrl+K)", S.ui.search)) { S.ui.search = true; S.ui.open = PanelId::None; S.query[0] = 0; S.searchSel = 0; S.searchFocus = true; }
            if (S.askDefaults) { ImGui::OpenPopup(app.lab == 1 ? "Put Atom Lab back?" : "Put Orbit Lab back?"); S.askDefaults = false; }
            defaultsPopups(app);
            sh::endPanel(bar);
        }
        auditRect("topbar", 0, 0, static_cast<float>(W), static_cast<float>(m.topBar));
        auditNote(std::string("crumbmode ") + std::to_string(plan.crumbMode));
    }

    // ---- the open dropdown or toolbar panel (one at a time) ----
    Rect openRect{0, 0, 0, 0};
    if (S.ui.open != PanelId::None) {
        const PanelId id = S.ui.open;
        const bool orbitOnly = orbitPanelWidthEm(id) > 0;
        if (orbitOnly && app.lab != 0) S.ui.open = PanelId::None;
        else {
            const std::string wid = std::string("##panel_") + panelName(id);
            const double ax = S.anchorX.count(id) ? S.anchorX[id] : W / 2;
            openRect = placePanel(fromTop(id) ? Anchor::UnderChip : Anchor::AboveToolbar, m.em(widthEmOf(id)), sh::lastHeight(wid, static_cast<float>(m.em(10))), fr, m, ax);
            sh::Panel p;
            if (sh::beginPanel(wid.c_str(), openRect, p, sh::Shape::Panel, true)) {
                bool close = false;
                if (!buildOrbitPanel(id, app, p, close)) {
                    if (id == PanelId::Menu) menuPanel(app, p, close);
                    else if (id == PanelId::LabMenu) labMenuPanel(app, p, close);
                    else if (id == PanelId::Facts) factsPanel(app, p);
                    else if (id == PanelId::More) { std::vector<ToolItem> hidden(tools.begin() + shown, tools.end()); morePanel(p, hidden, close); }
                }
                sh::endPanel(p);
                if (close && S.ui.open == id) S.ui.open = PanelId::None;
            }
            occ.push_back(openRect);
            auditRect(std::string("panel.") + panelName(id), static_cast<float>(openRect.x), static_cast<float>(openRect.y), static_cast<float>(openRect.w), static_cast<float>(openRect.h));
            auditNote(std::string("panelopen ") + panelName(id));
            // a click anywhere else closes it (a click on the bars is their own business)
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !mouseIn(openRect) && !mouseIn(fr.topBar) && !mouseIn(fr.toolbar)) S.ui.open = PanelId::None;
        }
    }
    // ---- search ----
    if (S.ui.search) {
        const Rect r = searchPanel(app, fr, m);
        occ.push_back(r);
        auditRect("panel.search", static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h));
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !mouseIn(r) && !mouseIn(fr.topBar)) S.ui.search = false;
    }
    // ---- a saved picture's note, above the toolbar ----
    if (!app.shot.note.empty() && ImGui::GetTime() < app.shot.noteUntil) {
        const Rect r = placePanel(Anchor::AboveToolbar, m.em(24), sh::lastHeight("##toast", static_cast<float>(m.em(4))), fr, m, W / 2);
        sh::Panel p;
        if (sh::beginPanel("##toast", r, p, sh::Shape::Panel, false)) { sh::pBody(p, app.shot.note, app.shot.failed ? false : true); sh::endPanel(p); }
    }
    // ---- the temporary Old panel: today's side column, floating, until stages 4 and 5 move its controls ----
    if (app.showOldPanel) {
        const float pw = std::min(kOldPanelUnits * dpi, static_cast<float>(W - 2.0 * m.s12)), top = static_cast<float>(fr.topBar.bottom() + m.s12);
        const float ph = static_cast<float>(H) - top - static_cast<float>(m.s12);
        const float px0 = static_cast<float>(W) - m.s12 - pw;
        if (app.lab == 0) orbitPanelFloating(app, &app.showOldPanel, px0, top, pw, ph); else atomPanelFloating(app, &app.showOldPanel, px0, top, pw, ph);
        if (ImGuiWindow* w = ImGui::FindWindowByName("Old panel###oldpanel"))
            if (w->WasActive) { const Rect r{w->Pos.x, w->Pos.y, w->Size.x, w->Size.y}; occ.push_back(r); auditRect("panel.old", static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h)); }
    }
    if (app.lab == 1) atomBigTable(app);

    // ---- the centre shift: the selected body is kept out from under the cards and panels (DESIGN.md section 6) ----
    static const bool shiftOff = std::getenv("PL_SHIFT") && std::string(std::getenv("PL_SHIFT")) == "off";   // for the automatic checks: the shift switched off
    if (app.lab == 0) {
        orbit::OrbitView& v = app.view;
        std::string sig;
        for (const Rect& r : occ) { char b[96]; std::snprintf(b, sizeof b, "%.0f,%.0f,%.0f,%.0f;", r.x, r.y, r.w, r.h); sig += b; }
        if (!(v.sel == S.lastSel)) { S.lastSel = v.sel; v.cx = v.cy = 0; S.frozen = false; }
        if (sig != S.lastOccluders) { S.lastOccluders = sig; S.frozen = false; }
        double bx = 0, by = 0, br = 0;
        Pt desired{0, 0};
        if (!shiftOff && !S.frozen && !app.pane && !app.journey.active && v.selectedScreen(bx, by, br)) {
            const Pt natural{bx * dpi - v.cx * dpi, by * dpi - v.cy * dpi};      // where it would be with no shift
            Pt target;
            const Rect win{0, 0, W, H};
            if (centreShiftTarget(natural, std::max(br * dpi, static_cast<double>(m.s8)), win, occ, m.s24, target)) desired = {target.x - natural.x, target.y - natural.y};
        }
        if (!S.frozen) {
            const double nx = easeToward(v.cx, desired.x / dpi, dt, 0.3, look().reduced), ny = easeToward(v.cy, desired.y / dpi, dt, 0.3, look().reduced);
            if (!v.follow) { v.ox += nx - v.cx; v.oy += ny - v.cy; }               // a followed body is placed from cx, cy by the picture; others are moved by the same amount
            v.cx = nx; v.cy = ny;
        }
        double sx = 0, sy = 0, sr = 0;
        if (v.selectedScreen(sx, sy, sr)) { char b[128]; std::snprintf(b, sizeof b, "body %.1f %.1f %.1f", sx * dpi, sy * dpi, sr * dpi); auditNote(b); }
    }

    // ---- how the picture is drawn now (the window, no overlays that moved into the bars) ----
    app.set.shell = true;
    app.set.minTextU = typeDef(Type::Caption).u; app.set.minTextPx = typeDef(Type::Caption).floorPx;
    app.set.reserveBottom = (H - fr.toolbar.y) / dpi + 8;
    app.set.reserveRight = (H - fr.readout.y) / dpi + 8;
    for (const Rect& r : occ) { char b[96]; std::snprintf(b, sizeof b, "occ%zu", static_cast<std::size_t>(&r - &occ[0])); auditRect(b, static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h)); }
    auditNote("metrics body=" + std::to_string(m.body) + " caption=" + std::to_string(m.caption) + " title=" + std::to_string(m.title) + " key=" + std::to_string(m.key) + " control=" + std::to_string(m.control) + " topbar=" + std::to_string(m.topBar));
}

}  // namespace ui
