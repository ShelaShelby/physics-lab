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
#include "atom/chart.h"
#include "atom/chart_view.h"
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
    double slide = -1;                                    // the Controls panel's slide, 0 closed .. 1 open (stage 5c.1); -1 before the first frame (it starts where the settings say, no slide)
    bool askDefaults = false;
    bool frozen = false;                                  // the user dragged the picture: the centre shift holds still
    orbit::Selection lastSel;
    std::string lastOccluders;
    char query[96] = "";
    int searchSel = 0;
    bool searchFocus = false;
    int queryGen = 0;                                      // (5l) a new search gets a new text box id: Dear ImGui keeps the old words in its own copy of an id it has seen, and they came back in front of the new ones
    bool openThemes = false;                              // search asked for the Theme list
    int openedFrame = -1;                                 // the frame a panel was opened in (its own opening click is not an outside click)
    std::map<PanelId, double> anchorX;                    // the centre x of the button a panel opens from (window pixels)
    bool contentsDummy = false;
    // the fact line (stage 4)
    FactClock fact;                                       // seconds this fact has been on screen (stands still while hovered)
    bool factHover = false;
    std::vector<std::string> recent;                      // the search's recent choices, newest first
    bool recentLoaded = false;
    double anchorFactX = 0;
    AppState* app = nullptr;                              // (stage 5c: the Controls panel's state lives in the settings, reached from the toolbar's callbacks)
    std::string revealSection;                            // search or a toolbar panel's old name asked for this section: unfold it and scroll to it
} S;

const char* panelName(PanelId id) {
    switch (id) {
    case PanelId::Menu: return "menu"; case PanelId::LabMenu: return "labmenu"; case PanelId::Chip: return "chip"; case PanelId::Speed: return "speed";
    case PanelId::Show: return "show"; case PanelId::Pick: return "pick"; case PanelId::Scale: return "scale"; case PanelId::View: return "view";
    case PanelId::More: return "more"; case PanelId::Facts: return "facts"; case PanelId::Data: return "data"; case PanelId::Nucleus: return "nucleus"; case PanelId::Cloud: return "cloud"; case PanelId::None: return "none";
    }
    return "none";
}
// 0.15 stage 5p: the top of the toolbar row and the fact line as of the last frame (window pixels): a panel that hangs from the top bar stops above it, with the readout in its way too
static double g_bottomTop = -1;
bool fromTop(PanelId id) { return id == PanelId::Menu || id == PanelId::LabMenu || id == PanelId::Chip; }
double widthEmOf(PanelId id) {
    if (const double w = orbitPanelWidthEm(id)) return w;
    if (id != PanelId::Chip) if (const double w = atomPanelWidthEm(id)) return w;
    switch (id) { case PanelId::Menu: return 18; case PanelId::LabMenu: return 14; case PanelId::Facts: return 24; case PanelId::More: return 18; case PanelId::Data: return 36; default: return 20; }
}
void toggle(PanelId id) { S.ui.open = S.ui.open == id ? PanelId::None : id; S.ui.search = false; S.openedFrame = ImGui::GetFrameCount(); }
}  // namespace
std::string controlsKey(const AppState& app, const char* what) { return std::string(app.lab == 0 ? "orbit." : app.lab == 2 ? "defense." : "atom.") + what; }
namespace shellimpl {
bool controlsOpen() { return S.app && !panelCollapsed(S.app->prefs.shellPanels, controlsKey(*S.app, "controls"), false); }      // open by default; kept in the settings like the cards' state
void controlsToggle() { if (S.app) S.app->prefs.shellPanels = withPanelState(S.app->prefs.shellPanels, controlsKey(*S.app, "controls"), controlsOpen()); }
const char* controlsSectionOf(PanelId id) {
    switch (id) { case PanelId::Speed: return "speed"; case PanelId::Show: return "show"; case PanelId::Pick: return "pick"; case PanelId::Scale: return "scale"; case PanelId::View: case PanelId::Nucleus: case PanelId::Cloud: return "view"; default: return ""; }
}
void shellOpenPanel(PanelId id) {
    if (const std::string key = controlsSectionOf(id); !key.empty() && S.app) {     // the old toolbar panels are sections of the Controls panel now
        S.app->prefs.shellPanels = withPanelState(S.app->prefs.shellPanels, controlsKey(*S.app, "controls"), false);
        S.app->prefs.shellPanels = withPanelState(S.app->prefs.shellPanels, controlsKey(*S.app, (std::string("ctl.") + key).c_str()), false);
        S.revealSection = key;
        S.ui.open = PanelId::None; S.ui.search = false;
        return;
    }
    S.ui.open = id; S.ui.search = false; S.openedFrame = ImGui::GetFrameCount();
}
}
namespace {
bool mouseIn(const Rect& r) { const ImVec2 mp = ImGui::GetIO().MousePos; return r.contains({mp.x, mp.y}); }
const char* const kTextSizes[4] = {"small", "normal", "large", "xl"};

// ---- the panels that every lab has ----
void menuPanel(AppState& app, sh::Panel& p, bool& close) {
    const Metrics& m = sh::F().m;
    sh::pTitle(p, "Menu");
    if (sh::pRow(p, "menu.save", "Save picture")) { app.shot.requested = true; close = true; }
    if (sh::pRow(p, "menu.history", "What's new (history)")) { app.showHistory = true; close = true; }
    if (sh::pRow(p, "menu.about", "About Physics Lab")) { app.showAbout = true; close = true; }
    if (app.lab != 2 && sh::pRow(p, "menu.changelog", app.lab == 1 ? "Atom Lab changelog" : "Orbit Lab changelog")) { app.showLabLog = true; close = true; }   // (the Defense Lab is new: its history is in What's new)
    if (app.lab != 2 && sh::pRow(p, "menu.defaults", "Put this lab's settings back…")) { S.askDefaults = true; close = true; }
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
    for (int i = 0; i < 3; i++)                                                      // 0.17: the Defense Lab is the third
        if (sh::pRow(p, i == 2 ? "lab.defense" : i ? "lab.atom" : "lab.orbit", i == 2 ? defense::kLabName : app::kLabInfo[i].name, app.lab == i)) { openLab(app, i); close = true; }
}

void factsPanel(AppState& app, sh::Panel& p, bool& /*close*/) {
    const Metrics& m = sh::F().m;
    sh::pTitle(p, "Did you know?");
    sh::pBody(p, factText(app));
    sh::pCaption(p, "No named source is recorded for this fact yet: it came with the web version of the app.");
    const float bw = (p.w - m.s8) / 2;
    const ImVec2 o = sh::here(p);
    if (sh::button("facts.prev", o, ImVec2(bw, static_cast<float>(m.control)), "‹ Previous")) { factStep(app, -1); S.fact.t = 0; }
    if (sh::button("facts.next", ImVec2(o.x + bw + m.s8, o.y), ImVec2(bw, static_cast<float>(m.control)), "Next ›")) { factStep(app, 1); S.fact.t = 0; }
    p.y += m.control + m.s4;
    sh::pCaption(p, std::to_string(factPosition(app) + 1) + " of " + std::to_string(factCount()));
}

void atomDataPanel(AppState& app, sh::Panel& p, bool& close);
// 0.15 stage 5h: the data sheet scrolls inside itself when a tab is taller than its place: the wheel (Dear ImGui's own) and Page Up / Page Down (here); the offset is logged for the automatic checks
void sheetScrollKeys() {
    if (ImGui::GetIO().WantTextInput) return;
    const float page = ImGui::GetWindowHeight() * 0.85f;
    if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) ImGui::SetScrollY(std::min(ImGui::GetScrollMaxY(), ImGui::GetScrollY() + page));
    if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) ImGui::SetScrollY(std::max(0.0f, ImGui::GetScrollY() - page));
    if (shellLogOn()) { char b[64]; std::snprintf(b, sizeof b, "sheetscroll y=%d max=%d", static_cast<int>(ImGui::GetScrollY()), static_cast<int>(ImGui::GetScrollMaxY())); auditNote(b); }
}
// The subject card's "All values": every line of the old card, word for word (its source lines too), in a large panel.
void dataPanel(AppState& app, sh::Panel& p, bool& close) {
    const Metrics& m = sh::F().m;
    if (app.lab == 1) { atomDataPanel(app, p, close); return; }
    orbit::SubjectCard sc;
    if (!subjectCardOf(app, sc)) { close = true; return; }
    const ImVec2 o = sh::here(p);
    const float hb = static_cast<float>(m.iconBtn);
    if (sh::iconButton("data.close", ImVec2(o.x + p.w - hb, o.y), ImVec2(hb, hb), Icon::Close, "Close (Esc)")) close = true;
    sh::text(p.dl, Type::Title, true, o, sh::cText(), sc.name);
    p.y += std::max(sh::lineH(Type::Title), hb) + m.s4;
    sh::pCaption(p, sc.kind);
    sh::pGap(p, m.s8);
    for (const std::string& l : sc.all) sh::pBody(p, l);
    if (shellLogOn()) {                                           // for the automatic checks: the lines the sheet shows, once per change
        static std::string last;
        std::string sig = sc.name;
        for (const std::string& l : sc.all) sig += "\t" + l;
        if (sig != last) { last = sig; std::printf("PLSHEET %s\n", sig.c_str()); std::fflush(stdout); }
    }
}

// The Atom Lab's data sheet (stage 5f): tabs Values, Decay and Hint, each the strings of the old panel's card / history / hint line, word for word.
void atomDataPanel(AppState& app, sh::Panel& p, bool& close) {
    const Metrics& m = sh::F().m;
    static int tab = 0;
    const AtomSubject sc = atomSubject(app);
    const ImVec2 o = sh::here(p);
    const float hb = static_cast<float>(m.iconBtn);
    if (sh::iconButton("data.close", ImVec2(o.x + p.w - hb, o.y), ImVec2(hb, hb), Icon::Close, "Close (Esc)")) close = true;
    sh::text(p.dl, Type::Title, true, o, sh::cText(), displayable(sc.name));
    p.y += std::max(sh::lineH(Type::Title), hb) + m.s4;
    sh::pCaption(p, displayable(sc.kind));
    sh::pGap(p, m.s8);
    const std::vector<std::string> labels{"Values", "Decay", "This view"}, names{"data.tab.values", "data.tab.decay", "data.tab.hint"};
    // stage 5h: a second row of tabs for the old panel's last cards (real spectrum, lifetimes, nuclear size, temperature, radial graph), each shown only when the old panel shows it for this subject and view
    static std::string cardTab;                                   // "" = one of the first three tabs
    const std::vector<AtomSheetCard> cards = atomSheetCards(app);
    int ci = -1;
    for (std::size_t i = 0; i < cards.size(); i++) if (cardTab == cards[i].id) ci = static_cast<int>(i);
    if (ci < 0) cardTab.clear();
    int cur = cardTab.empty() ? tab : -1;
    if (sh::pSegmented(p, "data.tabs", labels, &cur, &names)) { tab = cur; cardTab.clear(); ci = -1; }
    std::vector<std::string> clabels, cnames;
    for (const AtomSheetCard& c : cards) { clabels.push_back(c.label); cnames.push_back(std::string("data.tab.") + c.id); }
    int cc = ci;
    if (sh::pSegmented(p, "data.cardtabs", clabels, &cc, &cnames) && cc >= 0) { cardTab = cards[cc].id; ci = cc; }
    sh::pGap(p, m.s4);
    if (!cardTab.empty()) {                                       // the very same card function the old panel calls (atom_panel.cpp), drawn at the sheet's cursor
        ImGui::SetCursorScreenPos(sh::here(p));
        atomSheetCard(app, cardTab);
        p.y = ImGui::GetCursorScreenPos().y - p.origin.y;
        sheetScrollKeys();
        return;
    }
    sheetScrollKeys();
    if (tab == 0) {
        sh::pBody(p, displayable(sc.isotope));
        for (std::size_t i = 0; i < sc.valuesTab.size(); i++) sh::pBody(p, displayable(sc.valuesTab[i]), i == 0);
    } else if (tab == 1) {
        for (const std::string& l : sc.decayTab) sh::pBody(p, displayable(l));
    } else sh::pBody(p, displayable(sc.hint));
    if (shellLogOn()) {                                           // for the automatic checks: every line of the three tabs (the old panel's strings are logged as PLATOMCARD), once per change
        static std::string last;
        std::string sig = sc.name + "\t" + sc.isotope;
        for (const std::string& l : sc.valuesTab) sig += "\t" + l;
        for (const std::string& l : sc.decayTab) sig += "\t" + l;
        sig += "\t" + sc.hint;
        if (sig != last) { last = sig; std::printf("PLSHEET %s\n", sig.c_str()); std::fflush(stdout); }
    }
}

void morePanel(sh::Panel& p, std::vector<ToolItem>& hidden, bool& close) {
    sh::pTitle(p, "More");
    for (ToolItem& t : hidden) {
        if (t.kind == ToolItem::Sep || !t.id[0]) continue;      // (a blank slot has no id)
        if (t.kind == ToolItem::Segment) {
            for (int i = 0; i < static_cast<int>(t.labels.size()); i++)
                if (sh::pRow(p, (std::string(t.id) + "." + t.labels[i]).c_str(), t.labels[i] + " view", t.sel == i)) { t.pick(i); close = true; }
            if (t.panel != PanelId::None && sh::pRow(p, (std::string(t.id) + ".opts").c_str(), "View options…")) { S.ui.open = t.panel; close = false; }
            continue;
        }
        if (t.kind == ToolItem::Split && sh::pRow(p, (std::string(t.id) + ".options").c_str(), t.tip + " (more choices)")) { S.ui.open = t.panel; close = false; continue; }
        if (sh::pRow(p, (std::string(t.id) + ".more").c_str(), t.tip)) {
            if (t.kind == ToolItem::Menu) { S.ui.open = t.panel; close = false; }
            else { t.act(); close = true; }
        }
    }
}

// ---- search (Ctrl+K): bodies, dates, every Orbit Lab setting, the menu's actions, the guides and the facts ----
struct SearchEntry { SearchItem item; std::function<void()> act; };
std::vector<SearchEntry> searchEntries(AppState& app) {
    std::vector<SearchEntry> v;
    const SearchAdd add = [&v](const char* group, const std::string& label, const std::string& id, std::function<void()> f) { v.push_back({{label, group, id}, std::move(f)}); };
    if (app.lab == 0) {
        orbitSearch(app, add);
        add("Guides", "What am I even looking at?", "guide:look", [&app] { app.learn = 1; app.learnJump = "top"; });
        add("Guides", "Why do we have seasons?", "guide:seasons", [&app] { app.learn = 2; app.learnJump = "top"; });
        add("Guides", "Timeline: how we understood the Solar System", "guide:timeline", [&app] { app.learn = 3; app.learnJump = "top"; });
    }
    add("Menu", "Save picture", "menu:save", [&app] { app.shot.requested = true; });
    add("Menu", "Settings", "menu:settings", [&app] { app.showSettings = true; });
    add("Menu", "About Physics Lab", "menu:about", [&app] { app.showAbout = true; });
    add("Menu", "What's new (history)", "menu:history", [&app] { app.showHistory = true; });
    add("Menu", app.lab == 1 ? "Atom Lab changelog" : "Orbit Lab changelog", "menu:changelog", [&app] { app.showLabLog = true; });
    add("Menu", "Put this lab's settings back…", "menu:defaults", [] { S.askDefaults = true; });
    add("Menu", "Text size", "menu:textsize", [] { shellOpenPanel(PanelId::Menu); });
    add("Menu", "Theme", "menu:theme", [] { S.openThemes = true; });
    add("Menu", "Learn", "menu:learn", [&app] { app.pane = 1; });
    add("Menu", "Achievements", "menu:ach", [&app] { app.pane = 2; });
    add("Menu", app.lab == 0 ? "Go to the Atom Lab" : "Go to the Orbit Lab", "menu:swap", [&app] { openLab(app, app.lab == 0 ? 1 : 0); });
    add("Menu", app.prefs.facts == "on" ? "Facts: turn off" : "Facts: turn on", "menu:facts", [&app] { app.prefs.facts = app.prefs.facts == "on" ? "off" : "on"; });
    add("Menu", "Legend", "menu:legend", [&app] { app.prefs.shellPanels = withPanelState(app.prefs.shellPanels, std::string(app.lab == 0 ? "orbit." : "atom.") + "legend", !panelCollapsed(app.prefs.shellPanels, std::string(app.lab == 0 ? "orbit." : "atom.") + "legend", true)); });
    for (const SettingRow& r : settingRows())                                       // 0.15 stage 5l: EVERY row of the Settings window, from the window's own list (ui/settings.h), after the menu's own entries so that "text size" still lists the menu's quick text-size chooser first; each opens Settings at its row
        if (settingRowShown(r, app)) { const std::string id = r.id; add("Settings", r.label, "setrow:" + id, [&app, id] { settingsGoto(app, id.c_str()); }); }
    for (int i = 0; i < factCount(); i++) {
        const std::string t = factTextAt(i);
        if (t.empty()) continue;
        add("Facts", t, "fact:" + std::to_string(i), [&app, i] { factShow(app, i); S.fact.t = 0; shellOpenPanel(PanelId::Facts); });
    }
    return v;
}

Rect searchPanel(AppState& app, const Frame& fr, const Metrics& m, const std::vector<Rect>& cards) {
    Rect r = placePanel(Anchor::Centre, m.em(36), sh::lastHeight("##search", static_cast<float>(m.em(14))), fr, m, fr.window.w / 2);
    for (const Rect& c : cards) r = sideStep(r, c, fr.window, m.s12, m.s12);
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
    const std::string fieldId = "##searchfield" + std::to_string(S.queryGen);
    const bool enterPressed = ImGui::InputTextWithHint(fieldId.c_str(), "Search bodies, dates, settings, menu, guides, facts", S.query, sizeof S.query, ImGuiInputTextFlags_EnterReturnsTrue);
    noteItem("search.field");
    auditText("body", ImGui::GetFontSize(), S.query[0] ? S.query : "Search");
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
    ImGui::PopFont();
    p.y += m.control + m.s8;
    // 0.15 stage 5l: the list follows the text. An empty (or blank) box shows NO results, only the hint below; the recent choices (before: listed as results when the box was empty, so the thing just
    // chosen came back after the text was deleted, and looked stuck) now only rank equal matches higher.
    const bool blank = std::all_of(S.query, S.query + std::strlen(S.query), [](char c) { return c == ' '; });
    const std::vector<int> hits = blank ? std::vector<int>() : searchResults(S.query, items, 8, S.recent);
    if (shellLogOn()) {
        std::string sig = std::string("query=") + S.query + " hits=";
        for (int h : hits) sig += items[h].label + "|";
        static std::string last;
        if (sig != last) { last = sig; std::printf("PLSEARCH %s\n", sig.c_str()); }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) S.searchSel++;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) S.searchSel--;
    S.searchSel = hits.empty() ? 0 : std::min<int>(std::max(S.searchSel, 0), static_cast<int>(hits.size()) - 1);
    int chosen = -1;
    if (!hits.empty()) auditNote("searchhit " + items[hits[0]].label);
    auditNote("searchcount " + std::to_string(hits.size()));
    for (std::size_t i = 0; i < hits.size(); i++) {
        const SearchItem& it = items[hits[i]];
        const ImVec2 o = sh::here(p);
        const std::string id = "search.hit" + std::to_string(i);
        const float gw = sh::width(Type::Caption, false, it.group);
        std::string label = it.label;                                              // a long label (a fact) is cut with "…" so it leaves room for its group
        const float room = p.w - gw - 4.0f * m.s8 - m.s12;
        while (sh::width(Type::Body, false, label) > room && label.size() > 4) { label.pop_back(); while (!label.empty() && (static_cast<unsigned char>(label.back()) & 0xC0) == 0x80) label.pop_back(); label.pop_back(); }
        if (label != it.label) label += "…";
        if (sh::row(id.c_str(), o, p.w, label, static_cast<int>(i) == S.searchSel)) chosen = hits[i];
        sh::text(p.dl, Type::Caption, false, ImVec2(o.x + p.w - gw - m.s8, o.y + (m.row - sh::px(Type::Caption)) / 2), sh::cMuted(), it.group);
        p.y += m.row;
    }
    if (enterPressed && !hits.empty()) chosen = hits[S.searchSel];
    if (blank && hits.empty()) sh::pCaption(p, "Type to search: a planet or moon, a date, a setting, Save picture, a guide, a fact. Choices you made before rank higher.");
    if (!blank && hits.empty()) sh::pCaption(p, "Nothing found. Try a planet, a setting or a guide.");
    sh::endPanel(p);
    if (chosen >= 0) {
        S.recent = pushRecent(S.recent, items[chosen].id);
        app.prefs.recent = packRecent(S.recent);
        S.ui.search = false; S.query[0] = 0; S.queryGen++;
        entries[chosen].act();
    }
    return r;
}

// ---- the top bar ----
struct Crumb { std::string text; int kind; };                 // kind: 0 link, 1 the lab (a dropdown), 2 link to the lab's picture, 3 where you are now (not a button)
std::vector<Crumb> crumbs(const AppState& app, int mode) {
    const std::string lab = app.lab == 2 ? defense::kLabName : app::kLabInfo[app.lab].name, subj = subjectName(app);
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

// ---- the fact line (stage 4): one short line at the bottom left of the picture ----
double factInterval() {
    static const double v = [] { const char* e = std::getenv("PL_FACT_INTERVAL"); return e ? std::max(0.5, std::atof(e)) : kFactInterval; }();     // (for the automatic checks)
    return v;
}

// Draws it and returns its rectangle (empty: not shown this frame). It is left out while it would sit on a card, a panel or the selected body; a click opens the whole fact, the arrows
// (on hover) step. The text keeps its size; only its opacity changes.
Rect factLine(AppState& app, const Frame& fr, const Metrics& m, const std::vector<Rect>& occ, double dpi, double dt) {
    std::string txt = factText(app);
    const double textW = sh::width(Type::Caption, false, txt);
    // (stage 5d) a fact too long for one line wraps to two; the rectangle is as wide as for one line and only as tall as the lines need. "…" only when two lines are still too short.
    const float arrowsW = static_cast<float>(2 * m.row + m.s4);
    float room = static_cast<float>(factLineRect(fr, m, textW).w) - arrowsW - 2.0f * m.s4;
    std::vector<std::string> lines = sh::wrapText(Type::Caption, false, txt, room);
    // 0.15 stage 5o: beside the toolbar the room can be too short for the whole fact in two lines (the Atom Lab with the Controls panel open at 1280 x 800 cut "…the surface of the Sun" to "…hotter than the…"):
    // then it takes the row above the toolbar, as it already does at small windows (the scale bar and decay lines rise with it: bottomLeftTop below).
    bool above = false;
    if (lines.size() > 2) {
        const float room2 = static_cast<float>(factLineRect(fr, m, textW, 0.0, true).w) - arrowsW - 2.0f * m.s4;
        const std::vector<std::string> lines2 = sh::wrapText(Type::Caption, false, txt, room2);
        if (lines2.size() < lines.size()) { above = true; room = room2; lines = lines2; }
    }
    const int nLines = std::max(1, std::min(2, static_cast<int>(lines.size())));
    const Rect r = factLineRect(fr, m, textW, nLines > 1 ? nLines * sh::lineH(Type::Caption) : 0.0, above);
    bool blocked = false;
    for (const Rect& o : occ) if (r.intersects(o)) blocked = true;
    if (!blocked && app.lab == 0) {
        double bx = 0, by = 0, br = 0;
        if (app.view.selectedScreen(bx, by, br) && hiddenBy({bx * dpi, by * dpi}, std::max(br * dpi, static_cast<double>(m.s8)), {r}, m.s12)) blocked = true;
    }
    if (blocked) { S.factHover = false; return Rect{0, 0, 0, 0}; }
    sh::Panel p;
    if (!sh::beginPanel("##shellFact", r, p, sh::Shape::Clear, false)) return Rect{0, 0, 0, 0};
    const float textAreaW = static_cast<float>(r.w) - arrowsW, h = static_cast<float>(r.h);
    const bool hover = sh::hoveredRect(p.origin, ImVec2(static_cast<float>(r.w), h));
    S.factHover = hover;
    if (factTick(S.fact, dt, hover, look().reduced, factInterval(), kFactFade)) factStep(app, 1);
    const float alpha = look().reduced ? 1.0f : static_cast<float>(factAlpha(S.fact.t, factInterval(), kFactFade));
    if (factText(app) != txt) { txt = factText(app); lines = sh::wrapText(Type::Caption, false, txt, room); }    // (it stepped this frame: the new one, in the same rectangle)
    if (lines.empty()) lines.push_back("");
    if (lines.size() > 2) {                                // more than two lines: the second ends with "…" (the click opens the whole fact)
        std::string rest = lines[1];
        for (std::size_t i = 2; i < lines.size(); i++) rest += " " + lines[i];
        while (sh::width(Type::Caption, false, rest + "…") > room && rest.size() > 4) { rest.pop_back(); while (!rest.empty() && (static_cast<unsigned char>(rest.back()) & 0xC0) == 0x80) rest.pop_back(); }
        while (!rest.empty() && rest.back() == ' ') rest.pop_back();
        lines.resize(2); lines[1] = rest + "…";
    }
    std::string shown = lines[0];
    for (std::size_t i = 1; i < lines.size(); i++) shown += " " + lines[i];      // (for the log: one line)
    ImGui::SetCursorScreenPos(p.origin);
    if (ImGui::InvisibleButton("fact.line", ImVec2(textAreaW, h))) { S.anchorX[PanelId::Facts] = r.x + textAreaW / 2; toggle(PanelId::Facts); }
    noteItem("fact.line");
    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    {   // one or two lines, centred on the rectangle's height together
        const float lh = sh::lineH(Type::Caption), top = p.origin.y + (h - lh * static_cast<float>(lines.size())) / 2 + (lh - sh::px(Type::Caption)) / 2;
        for (std::size_t i = 0; i < lines.size(); i++) sh::shadowed(p.dl, Type::Caption, ImVec2(p.origin.x + m.s4, top + lh * static_cast<float>(i)), sh::cFactText(), alpha, lines[i]);
    }
    if (hover) {
        const float bw = static_cast<float>(m.row), by = (h - bw) / 2;
        if (sh::button("fact.prev", ImVec2(p.origin.x + textAreaW, p.origin.y + by), ImVec2(bw, bw), "‹", Face::Quiet)) { factStep(app, -1); S.fact.t = 0; }
        if (sh::button("fact.next", ImVec2(p.origin.x + textAreaW + bw + m.s4, p.origin.y + by), ImVec2(bw, bw), "›", Face::Quiet)) { factStep(app, 1); S.fact.t = 0; }
    }
    ImGui::End();
    auditNote("factline " + shown);
    auditNote("factlines " + std::to_string(lines.size()) + (lines.back().size() >= 3 && lines.back().compare(lines.back().size() - 3, 3, "…") == 0 ? " cut" : " whole"));
    char b[64];
    std::snprintf(b, sizeof b, "factalpha %.3f t=%.2f", alpha, S.fact.t);
    auditNote(b);
    return r;
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
    S.app = &app;
    if (!S.recentLoaded) { S.recent = unpackRecent(app.prefs.recent); S.recentLoaded = true; }
    setReadingMode(true);
    const sh::Frame& SF = sh::F();
    const Metrics& m = SF.m;
    ImGuiIO& io = ImGui::GetIO();
    const double W = winW, H = winH;
    const float dpi = static_cast<float>(SF.dpi);

    // ---- keys: Esc closes the topmost thing, Ctrl+K opens search (Space is routed by main.cpp through shellSpace) ----
    S.ui.reading = pagesOpen(app) || app.pane != 0;
    bool escHandled = false;                              // (5l) Esc already closed something this frame: the Legend then stays
    if (!app.benchRunning) {
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && applyKey(S.ui, Key::Escape, io.WantTextInput, true).consumed) {
            escHandled = true;
            if (!S.ui.reading) { closePages(app); app.pane = 0; }
            io.ClearInputKeys();
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_K, false)) { applyKey(S.ui, Key::CtrlK, io.WantTextInput, true); S.query[0] = 0; S.searchSel = 0; S.searchFocus = true; S.queryGen++; }   // (5l: ClearActiveID: Ctrl+K with the box already open cleared our text but not the text box's own copy, so the old words came back in front of the new ones)
    }

    // ---- what the bars hold, and how big that makes them ----
    std::vector<ToolItem> tools = app.lab == 0 ? orbitTools(app) : app.lab == 2 ? std::vector<ToolItem>{} : atomTools(app);   // (the Defense Lab has its build bar instead: shell_defense.cpp)
    const double gap = m.s4;
    const double segW = std::max(sh::width(Type::Body, false, "2D"), sh::width(Type::Body, false, "3D")) + 2.0 * m.s12;
    std::vector<double> iw;
    for (const ToolItem& t : tools)
        iw.push_back(t.kind == ToolItem::Sep ? m.s12 : t.kind == ToolItem::Menu || t.kind == ToolItem::Split ? m.toolBtn + m.caretW : t.kind == ToolItem::Segment ? 2 * segW + gap + (t.panel != PanelId::None ? m.caretW : 0) : m.toolBtn);
    const int count = static_cast<int>(tools.size());
    const bool hasToolbar = count > 0;
    const bool paneOpen = app.pane != 0;                  // the Learn or Achievements page covers the lab: only the top bar stays
    // the Controls panel slides in and out over app::shell::kSlideSeconds (stage 5c.1; at once with Settings → Motion reduced). While it moves or is open, the layout is the open one (the toolbar and
    // the readout stay left of it), so nothing slides under it; the only per-frame work is this step and the panel drawn at its place.
    const bool wantOpen = controlsOpen();
    if (S.slide < 0) S.slide = wantOpen ? 1 : 0;
    else if (!paneOpen) S.slide = slideStep(S.slide, io.DeltaTime, wantOpen, look().reduced);
    const double ease = slideEase(S.slide);
    const double dockW = S.slide > 0 && !paneOpen ? dockWidth(m, W) : 0;
    const double freeW0 = dockW > 0 ? W - m.s12 - dockW : W;     // the width left of the Controls panel: the toolbar is centred in it
    const int shown = toolbarVisible(iw, gap, m.toolBtn, freeW0, m);
    double tw = 2.0 * m.s8;
    for (int i = 0; i < shown; i++) tw += iw[i] + (i ? gap : 0);
    if (shown < count) tw += (shown ? gap : 0) + m.toolBtn;
    if (!hasToolbar) tw = m.toolBtn;
    const std::vector<std::string> ro = readoutLines(app, fps);
    double rw = 0;
    for (const std::string& l : ro) rw = std::max(rw, static_cast<double>(sh::width(Type::Body, false, l)));
    const double rh = ro.empty() ? 0.0 : ro.size() * sh::lineH(Type::Body) + 2.0 * m.s4;       // (stage 5c: Body size on a pill, padded 8 u and 4 u)
    // 0.15 stage 5l: the toolbar is centred on the scene: the x the picture is drawn around. Orbit Lab: the Sun's x, the middle of the window (orbit_view.cpp draws at W / 2). Atom Lab: the middle of the
    // free part of the picture (atom_view.cpp: freeX + freeW / 2), as worked out in the last frame (-1 on the first frame: the window's middle). With the Controls panel open the toolbar moves left only as
    // far as it must to stay clear of it. (This replaces the stage 5g fixed left edge, which centred the widest toolbar, so the Orbit Lab's shorter one sat off to the left.)
    const double sceneCx = app.lab == 1 && app.atom.freeW >= 0 ? (app.atom.freeX + app.atom.freeW / 2) * dpi : W / 2;
    const Frame fr = makeFrame(W, H, m, tw, rw + 2.0 * m.s8, rh, dockW, 0, hasToolbar ? sceneCx : -1);
    S.topBarH = static_cast<float>(m.topBar);

    // the top bar's pieces
    const ChipInfo chip = chipInfo(app);
    const double todayW = sh::width(Type::Body, false, "Today") + 2.0 * m.s12;
    const double stopW = sh::width(Type::Body, false, "■ Stop") + 2.0 * m.s12;
    // The chip keeps one width while time runs (its words change every frame: "3.2 years after ..."), so the play and Today buttons inside it do not slide under the pointer: it is as wide
    // as the longest words it normally holds, and only grows past that.
    const double refTitle = sh::width(Type::Title, true, "30 Sep 2026, 12:00 UT"), refSub = sh::width(Type::Caption, false, "1 s = 29.1 days  ·  12.5 years after 30 Sep 2026");
    const double chipW = 2.0 * m.s16 + std::max({static_cast<double>(sh::width(Type::Title, true, chip.title)), static_cast<double>(sh::width(Type::Caption, false, chip.sub)), app.lab == 0 ? static_cast<double>(refTitle) : 0.0, app.lab == 0 ? static_cast<double>(refSub) : 0.0}) + (chip.today ? m.s8 + todayW : chip.stop ? m.s8 + stopW : 0) + (chip.play ? m.s4 + m.control : 0);
    const int nIcons = 6;
    TopBarWidths tbw;
    tbw.menu = m.iconBtn;
    tbw.crumbFull = crumbWidth(crumbs(app, 0), m); tbw.crumbShort = crumbWidth(crumbs(app, 1), m); tbw.crumbIcon = m.toolBtn + m.caretW;
    tbw.chip = chipW;
    tbw.right = nIcons * m.iconBtn + (nIcons - 1) * gap;
    const TopBarPlan plan = planTopBar(W, m, tbw);

    std::vector<Rect> occ{fr.topBar};
    if (fr.dock.w > 0) occ.push_back(fr.dock);            // the Controls panel is an occluder like the others: the centre shift, the fact line and the dropdowns keep clear of it
    std::vector<Rect> cards;                              // the floating cards (the Legend, the subject card): a panel that opens over one steps aside
    if (hasToolbar) occ.push_back(fr.toolbar);
    std::string log;

    // 0.15 stage 5q: the Element picker of the Atom Lab HOVERS over the picture: it is not an occluder (nothing is re-centred for it) and the atom's free rectangle is kept as it was while it is open.
    // The subject card is left out while the picker (as of the last frame) would cover it, as with the Legend. (The Orbit Lab's chip panel is unchanged.)
    static Rect pickerPrev{0, 0, 0, 0};
    static double freeSaved[4] = {0, 0, 0, 0}; static bool freeSavedOk = false;
    Rect pickerNow{0, 0, 0, 0};
    Rect legendOpenR{0, 0, 0, 0};                         // (5l) where the Legend is when it is open (w == 0: not open)
    std::string legendOpenName;
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
            if (std::strcmp(key, "card") == 0 && legendOpenR.w > 0 && r.intersects(legendOpenR)) return Rect{r.right(), r.y, 0, 0};   // 5m part 2: the subject card never covers an open Legend: it is left out while the Legend is open
            if (std::strcmp(key, "card") == 0 && app.lab == 1 && pickerPrev.w > 0 && r.intersects(pickerPrev)) return Rect{r.right(), r.y, 0, 0};   // 5q: ... and while the Element picker would cover it
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
            if (std::strcmp(key, "legend") == 0) { legendOpenR = r; legendOpenName = name; }         // (5l) the open Legend: a click anywhere else, or Esc, folds it
            if (std::strcmp(key, "card") == 0 && app.lab == 1 && pickerPrev.w > 0 && r.intersects(pickerPrev)) return Rect{r.right(), r.y, 0, 0};   // 5q: (see above)
            if (std::strcmp(key, "card") == 0 && legendOpenR.w > 0 && r.intersects(legendOpenR)) return Rect{r.right(), r.y, 0, 0};   // 5m part 2: (the Legend and the cards are drawn Legend first) no room for both: the Legend is the one the owner just opened
            if (!sh::beginPanel(wid.c_str(), r, p, sh::Shape::Panel, true)) return r;
            sh::text(p.dl, Type::Title, true, sh::here(p), sh::cText(), title);
            const float hb = static_cast<float>(m.iconBtn);
            if (sh::iconButton((std::string("card.") + key + ".collapse").c_str(), ImVec2(p.origin.x + p.w - hb, p.origin.y - m.s4 * 0.0f), ImVec2(hb, hb), Icon::CaretUp, "Collapse to its title"))
                app.prefs.shellPanels = withPanelState(app.prefs.shellPanels, name, true);
            p.y += std::max(sh::lineH(Type::Title), hb) + m.s4;
            sh::bodyFloor(true);                                    // (stage 5f: small text in the cards is drawn at Body size, never under 14 px, as in the Controls panel)
            body(p);
            sh::bodyFloor(false);
            sh::endPanel(p);
        }
        occ.push_back(r);
        cards.push_back(r);
        auditRect(std::string("card.") + name + (collapsed ? ".collapsed" : ""), static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h));
        auditNote(std::string("cardtitle ") + name + " " + title + (collapsed ? " collapsed" : " open"));
        return r;
    };
    const bool pane = paneOpen;
    if (app.lab == 0 && !pane) {
        card("legend", "Legend", Anchor::TopLeft, 22, true, [&](sh::Panel& p) {
            const std::vector<LegendEntry> legend = legendEntries(app);              // (worked out only while the legend is open)
            for (const LegendEntry& e : legend) {
                const ImVec2 o = sh::here(p);
                const float rowH = static_cast<float>(m.row), sw = static_cast<float>(m.s24 + m.s16);          // the swatch's width
                const float cy = o.y + rowH / 2;
                const auto col = [](const std::string& hex, float a = 1) { return ui::col(app::hexRgb(hex.c_str()), a); };
                const float r = static_cast<float>(m.s4) + 1;
                float x = o.x + m.s4;
                switch (e.marker) {
                case LegendEntry::Disc: p.dl->AddCircleFilled(ImVec2(o.x + sw / 2, cy), r * 1.3f, col(e.hex[0]), 16); break;
                case LegendEntry::Ring: p.dl->AddCircle(ImVec2(o.x + sw / 2, cy), r * 1.5f, col(e.hex[0]), 16, 2.0f); break;
                case LegendEntry::Line: p.dl->AddLine(ImVec2(o.x + m.s4, cy), ImVec2(o.x + sw - m.s4, cy), col(e.hex[0], 0.85f), 2.0f); break;
                case LegendEntry::Band: p.dl->AddRectFilled(ImVec2(o.x + m.s4, cy - r * 1.4f), ImVec2(o.x + sw - m.s4, cy + r * 1.4f), col(e.hex[0], 0.55f)); break;
                case LegendEntry::Cross:
                    p.dl->AddCircle(ImVec2(o.x + sw / 2, cy), r * 1.4f, col(e.hex[0]), 16, 1.5f);
                    p.dl->AddLine(ImVec2(o.x + sw / 2 - r * 2, cy), ImVec2(o.x + sw / 2 + r * 2, cy), col(e.hex[0]), 1.5f);
                    p.dl->AddLine(ImVec2(o.x + sw / 2, cy - r * 2), ImVec2(o.x + sw / 2, cy + r * 2), col(e.hex[0]), 1.5f);
                    break;
                case LegendEntry::Dots: {
                    const int n = static_cast<int>(std::min<std::size_t>(e.hex.size(), 5));
                    const float step = n > 1 ? (sw - 2 * r - m.s4) / (n - 1) : 0;
                    for (int k = 0; k < n; k++) p.dl->AddCircleFilled(ImVec2(x + r + k * step, cy), r, col(e.hex[k]), 12);
                    break;
                }
                }
                const float tx = o.x + sw + m.s8, avail = p.w - sw - m.s8;
                const float ph = sh::paragraph(p.dl, Type::Body, false, ImVec2(tx, o.y + (rowH - sh::px(Type::Body)) / 2), avail, sh::cText(), e.text);
                p.y += std::max(rowH, ph + m.s4);
            }
        });
        orbit::SubjectCard sc;
        if (subjectCardOf(app, sc))
            card("card", sc.name.c_str(), Anchor::TopRight, 20, false, [&](sh::Panel& p) {
                if (!sc.kind.empty()) sh::pCaption(p, sc.kind);
                {   // the ONE big number (Key size; a very wide one drops to Title so it never leaves the card)
                    const bool big = sh::width(Type::Key, true, sc.keyValue) <= p.w;
                    const Type kt = big ? Type::Key : Type::Title;
                    sh::text(p.dl, kt, true, sh::here(p), sh::cText(), sc.keyValue);
                    p.y += sh::lineH(kt);
                    sh::pCaption(p, sc.keyCaption);
                }
                sh::pGap(p, m.s4);
                for (const orbit::SubjectValue& v : sc.values) { if (v.label.empty()) sh::pBody(p, v.value, true); else sh::pStat(p, v.label, v.value); }
                CardAction act;
                if (cardAction(app, act) && sh::pButton(p, act.id, act.text, Face::Quiet)) act.act();
                if (sh::pButton(p, "card.all", "All values", Face::Secondary, S.ui.open == PanelId::Data)) toggle(PanelId::Data);
            });
        else if (S.ui.open == PanelId::Data) S.ui.open = PanelId::None;       // nothing selected: no data sheet
    }
    if (app.lab == 1 && !pane && app.atom.chartOn()) {    // 0.15 stage 5f: the chart's legend and notes (the picture's own copies are left out in the new UI); nothing else in the Atom Lab has a legend to list
        card("legend", "Legend", Anchor::TopLeft, 22, true, [&](sh::Panel& p) {
            static bool notes = false;
            sh::pCaption(p, "Colour: how it decays (an empty square: no such nuclide seen)");
            int rows = 0;
            std::string ends;
            for (int c = 0; c < atom::kChartClasses; c++) {
                if (atom::chartClassCount(atom::ChartClass(c)) == 0) continue;
                const atom::ChartClassInfo& ci = atom::chartClassInfo(atom::ChartClass(c));
                const ImVec2 o = sh::here(p);
                const float rowH = static_cast<float>(m.row), sw = static_cast<float>(m.s24), sq = static_cast<float>(m.s12);
                const ImU32 colr = *ci.hex ? ui::col(app::hexRgb(ci.hex), 1.0f) : sh::cText();      // ("" = the theme's text colour: the stable nuclides)
                p.dl->AddRectFilled(ImVec2(o.x + m.s4, o.y + (rowH - sq) / 2), ImVec2(o.x + m.s4 + sq, o.y + (rowH - sq) / 2 + sq), colr);
                const std::string words = displayable(std::string(ci.words) + "  (" + std::to_string(atom::chartClassCount(atom::ChartClass(c))) + ")");
                const float ph = sh::paragraph(p.dl, Type::Body, false, ImVec2(o.x + sw + m.s8, o.y + (rowH - sh::px(Type::Body)) / 2), p.w - sw - m.s8, sh::cText(), words);
                p.y += std::max(rowH, ph + m.s4);
                rows++;
                ends = "(" + std::to_string(atom::chartClassCount(atom::ChartClass(c))) + ")";
            }
            sh::pGap(p, m.s4);
            sh::pCaption(p, displayable(atom::chartMagicWords()));
            if (!app.atom.s.chartView.flat()) sh::pCaption(p, displayable(atom::chartHeightWords(app.atom.s.chartView.height)));
            if (sh::pRow(p, "legend.notes", notes ? "Notes about the chart  (hide)" : "Notes about the chart  (show)")) notes = !notes;
            if (notes) { sh::pCaption(p, displayable(atom::chartValleyWords())); sh::pCaption(p, displayable(atom::chartIslandWords())); }
            if (shellLogOn()) auditNote("atomlegend rows=" + std::to_string(rows) + " ends=" + ends);
        });
    }
    // 0.15 stage 5l: the Legend folds back to its title on a click anywhere else (the picture, the bars, another window), and on Esc; a click inside it keeps it open. (Before: only its arrow closed it.)
    // A click is judged by where the pointer is on the press, so it does not matter in which frame the pointer arrived.
    if (legendOpenR.w > 0 && !app.benchRunning) {
        // "on the Legend" = the window under the pointer IS the Legend's own (not just its rectangle: at a small size the subject card can lie over part of it, and a click on that card is outside)
        const ImGuiWindow* hw = ImGui::GetCurrentContext()->HoveredWindow;
        const bool onLegend = hw && hw->RootWindow && ("##card_" + legendOpenName) == hw->RootWindow->Name;
        const bool outside = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !onLegend;
        const bool esc = ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !escHandled && !io.WantTextInput && !S.ui.search && S.ui.open == PanelId::None;
        if (outside || esc) app.prefs.shellPanels = withPanelState(app.prefs.shellPanels, legendOpenName, true);
    }
    if (app.lab == 1 && !pane) {                          // 0.15 stage 5f: the Atom Lab's subject card (the chosen isotope): the same strings as the old panel's element card, isotope line and hint
        const AtomSubject sc = atomSubject(app);
        const Rect atomCardR = card("card", displayable(sc.name).c_str(), Anchor::TopRight, 20, false, [&](sh::Panel& p) {
            if (!sc.kind.empty()) sh::pCaption(p, displayable(sc.kind));
            {
                const bool big = sh::width(Type::Key, true, sc.keyValue) <= p.w;
                const Type kt = big ? Type::Key : Type::Title;
                sh::text(p.dl, kt, true, sh::here(p), sh::cText(), sc.keyValue);
                p.y += sh::lineH(kt);
                sh::pCaption(p, sc.keyCaption);
            }
            sh::pGap(p, m.s4);
            for (const auto& v : sc.values) { if (v.first.empty()) sh::pBody(p, displayable(v.second), true); else sh::pStat(p, v.first, v.second); }
            sh::pCaption(p, displayable(sc.isotope));
            if (sh::pButton(p, "card.more", app.atom.moreShown() ? "Less info" : "More info", Face::Quiet)) { app.atom.s.more = !app.atom.moreShown(); app.atom.moreStored = app.atom.s.more; }   // (the old bar's / picture's button)
            if (sh::pButton(p, "card.all", "All values", Face::Secondary, S.ui.open == PanelId::Data)) toggle(PanelId::Data);
        });
        // 0.15 stage 5g: the picture's phase ring (the Wheel colours' legend) never sits under the card: below it when the card is a collapsed title, left of it when the card is open
        // (below it when there is no room left of it). Its size: about 62 points each side of its centre, 62 above and 42 below (atom_view.cpp).
        {
            const double dp = dpi, half = 62, gapPt = 8, cardL = atomCardR.x / dp, cardR = atomCardR.right() / dp, cardT = atomCardR.y / dp, cardB = atomCardR.bottom() / dp;
            const bool collapsedCard = panelCollapsed(app.prefs.shellPanels, "atom.card", false);
            if (!collapsedCard && cardL - gapPt - 2 * half >= 12) { app.atom.ringX = cardL - gapPt - half; app.atom.ringY = cardT + half; }   
            else { app.atom.ringX = cardR - half; app.atom.ringY = cardB + gapPt + half; }
            const bool ringShown = !app.atom.chartOn() && app.atom.p.view != "Orbit" && app.atom.usesWheel();
            const bool ringLeft = ringShown && app.atom.ringX < cardL;
            const bool roomBeside = !ringLeft || app.atom.ringX - half >= 220;                           // 220 points: enough for the info text to wrap sensibly beside the ring
            app.atom.infoRight = ringLeft && roomBeside ? app.atom.ringX - half : cardL;                // the info text at the top left stops before the card (and the ring)
            app.atom.infoDown = ringLeft && !roomBeside ? app.atom.ringY + 42 + gapPt - 4 - (m.topBar / dp + 4) : 0;   // no room beside the ring: the text goes below it (the ring sits at the top, left of the card)
        }
    }

    // ---- the toolbar ----
    if (!pane && hasToolbar) {
        sh::Panel tb;
        if (sh::beginPanel("##shellTool", fr.toolbar, tb, sh::Shape::Pill, false)) {
            float x = tb.origin.x + m.s8;
            const float y = tb.origin.y + (m.toolbarH - m.toolBtn) / 2.0f;
            for (int i = 0; i < shown; i++) {
                ToolItem& t = tools[i];
                const float w = static_cast<float>(iw[i]);
                if (shellLogOn() && t.kind != ToolItem::Sep && t.id[0]) auditRect(std::string("toolbtn.") + t.id, x, y, w, static_cast<float>(m.toolBtn));   // (for the automatic checks: where each button is)
                if (t.kind == ToolItem::Sep) {
                    tb.dl->AddLine(ImVec2(x + w / 2, y + m.s4), ImVec2(x + w / 2, y + m.toolBtn - m.s4), sh::cEdge(), 1.0f);
                } else if (t.kind == ToolItem::Button) {
                    if (t.id[0] && sh::iconButton(t.id, ImVec2(x, y), ImVec2(static_cast<float>(m.toolBtn), static_cast<float>(m.toolBtn)), t.icon, t.tip.c_str(), t.on)) t.act();
                } else if (t.kind == ToolItem::Split) {
                    if (sh::iconButton(t.id, ImVec2(x, y), ImVec2(static_cast<float>(m.toolBtn), static_cast<float>(m.toolBtn)), t.icon, t.tip.c_str(), t.on)) t.act();
                    const float cx = x + static_cast<float>(m.toolBtn);
                    S.anchorX[t.panel] = x + w / 2;
                    if (sh::iconButton((std::string(t.id) + ".caret").c_str(), ImVec2(cx, y), ImVec2(static_cast<float>(m.caretW), static_cast<float>(m.toolBtn)), Icon::Caret, "More choices", S.ui.open == t.panel)) toggle(t.panel);
                } else if (t.kind == ToolItem::Menu) {
                    S.anchorX[t.panel] = x + w / 2;
                    if (sh::iconButton(t.id, ImVec2(x, y), ImVec2(w, static_cast<float>(m.toolBtn)), t.icon, t.tip.c_str(), S.ui.open == t.panel, static_cast<float>(m.caretW))) toggle(t.panel);
                } else {
                    const float sy = y + (m.toolBtn - m.row) / 2.0f;
                    int sel = t.sel;
                    if (sh::segmented(t.id, ImVec2(x, sy), static_cast<float>(2 * segW), t.labels, &sel, nullptr, true)) t.pick(sel);
                    noteRect((std::string(t.id) + ".2d").c_str(), x, sy, static_cast<float>(segW), static_cast<float>(m.row));
                    noteRect((std::string(t.id) + ".3d").c_str(), x + static_cast<float>(segW), sy, static_cast<float>(segW), static_cast<float>(m.row));
                    if (shellLogOn()) { auditRect(std::string("toolseg.") + t.id + ".2d", x, sy, static_cast<float>(segW), static_cast<float>(m.row)); auditRect(std::string("toolseg.") + t.id + ".3d", x + static_cast<float>(segW), sy, static_cast<float>(segW), static_cast<float>(m.row)); }   // (5p: for the automatic checks of the lit look)
                    if (t.panel != PanelId::None) {
                        const float cx = x + static_cast<float>(2 * segW + gap);
                        S.anchorX[t.panel] = cx + m.caretW / 2.0;
                        if (sh::iconButton((std::string(t.id) + ".caret").c_str(), ImVec2(cx, y), ImVec2(static_cast<float>(m.caretW), static_cast<float>(m.toolBtn)), Icon::Caret, "View options", S.ui.open == t.panel)) toggle(t.panel);
                    }
                }
                x += w + static_cast<float>(gap);
            }
            if (shown < count) {
                S.anchorX[PanelId::More] = x + m.toolBtn / 2.0;
                if (sh::iconButton("tool.more", ImVec2(x, y), ImVec2(static_cast<float>(m.toolBtn), static_cast<float>(m.toolBtn)), Icon::Dots, "More", S.ui.open == PanelId::More)) toggle(PanelId::More);
            }
            if (shellLogOn()) { std::string on; for (int i = 0; i < shown; i++) if (tools[i].kind == ToolItem::Button && tools[i].id[0] && tools[i].on) on += std::string(on.empty() ? "" : ",") + tools[i].id; auditNote("toolon " + on); }   // (5l: which buttons are lit, for the tests)
            sh::endPanel(tb);
        }
        auditRect("toolbar", static_cast<float>(fr.toolbar.x), static_cast<float>(fr.toolbar.y), static_cast<float>(fr.toolbar.w), static_cast<float>(fr.toolbar.h));
    }

    // ---- 0.15 stage 5n: the chart's tilt slider "flat <-> 3D": a pill the width of the toolbar, directly above it (so its centre is the toolbar's centre), in the Chart view ONLY. It drives the same
    // tilt as the Controls panel's "Tilt" slider (AtomLab::setParam("tilt"): 0 = the flat chart, 1 = the full landscape). ----
    if (!pane && hasToolbar && app.lab == 1 && app.atom.chartOn()) {
        const Rect tr{fr.toolbar.x, fr.toolbar.y - m.s8 - m.toolbarH, fr.toolbar.w, static_cast<double>(m.toolbarH)};
        sh::Panel tp;
        if (sh::beginPanel("##shellTilt", tr, tp, sh::Shape::Pill, false)) {
            const float y0 = tp.origin.y + (m.toolbarH - m.row) / 2.0f, ty = y0 + (m.row - sh::px(Type::Body)) / 2.0f;
            const float lw = sh::width(Type::Body, false, "flat"), rw = sh::width(Type::Body, false, "3D");
            const float x0 = tp.origin.x + m.s16, sx = x0 + lw + m.s8, sw = static_cast<float>(tr.w) - 2.0f * m.s16 - lw - rw - 2.0f * m.s8;
            sh::text(tp.dl, Type::Body, false, ImVec2(x0, ty), sh::cMuted(), "flat");
            sh::text(tp.dl, Type::Body, false, ImVec2(sx + sw + m.s8, ty), sh::cMuted(), "3D");
            float v = static_cast<float>(app.atom.s.chartView.tilt);
            if (sw > 10 && sh::slider("tilt.slider", ImVec2(sx, y0), sw, &v, 0.0f, 1.0f)) app.atom.setParam("tilt", std::round(static_cast<double>(v) * 100.0) / 100.0);
            sh::endPanel(tp);
        }
        occ.push_back(tr);
        auditRect("tiltslider", static_cast<float>(tr.x), static_cast<float>(tr.y), static_cast<float>(tr.w), static_cast<float>(tr.h));
    }

    // ---- 0.17: the Defense Lab's build bar, bottom centre (where the other labs have their toolbar) ----
    if (!pane && app.lab == 2) { const Rect br = defenseBar(app, fr, m); if (br.w > 0) { occ.push_back(br); auditRect("defensebar", static_cast<float>(br.x), static_cast<float>(br.y), static_cast<float>(br.w), static_cast<float>(br.h)); } }

    // ---- the readout, bottom right: Body size on a soft dark pill (stage 5c; before it was Caption with an outline and was hard to read over lines and stars). Its own window, so a saved picture does not carry it ----
    if (!pane && !ro.empty()) {
        sh::Panel ropanel;
        if (sh::beginPanel("##shellRO", fr.readout, ropanel, sh::Shape::Overlay, false)) {
            const ImVec2 a(static_cast<float>(fr.readout.x), static_cast<float>(fr.readout.y)), b(static_cast<float>(fr.readout.right()), static_cast<float>(fr.readout.bottom()));
            ropanel.dl->AddRectFilled(a, b, ui::col(app::Rgb{0, 0, 0}, kReadoutAlpha), static_cast<float>(ro.size() > 1 ? m.radiusPanel : fr.readout.h / 2));
            const ImU32 tc = ui::col(readoutText(*sh::F().c), 1.0f);
            for (std::size_t i = 0; i < ro.size(); i++) {
                const float tw2 = sh::width(Type::Body, false, ro[i]);
                sh::text(ropanel.dl, Type::Body, false, ImVec2(b.x - m.s8 - tw2, a.y + m.s4 + i * sh::lineH(Type::Body)), tc, ro[i]);
            }
            auditText("readout", sh::px(Type::Body), ro[0]);
            ImGui::End();
        }
        auditRect("readout", static_cast<float>(fr.readout.x), static_cast<float>(fr.readout.y), static_cast<float>(fr.readout.w), static_cast<float>(fr.readout.h));
    }

    // ---- the Controls panel (stage 5c, owner's decision of 6 Oct 2026): every simulation option in ONE docked panel on the right, in sections that fold. It floats over the full-window picture, under the
    // top bar, full height, about 22 em wide, and scrolls inside. The bottom toolbar and the readout sit left of it. The chip's dropdowns and the app's settings are not in it. ----
    if (fr.dock.w > 0) {
        sh::Panel cp;
        const std::string openKey = controlsKey(app, "controls");
        const Rect pr = slidRect(fr.dock, W, ease);         // where it is drawn: the dock itself once the slide is over
        if (sh::beginPanel("##shellControls", pr, cp, sh::Shape::Panel, true, true)) {
            const float hb = static_cast<float>(m.iconBtn);
            {   // the title and its close button
                const ImVec2 o = sh::here(cp);
                if (sh::iconButton("ctl.close", ImVec2(o.x + cp.w - hb, o.y), ImVec2(hb, hb), Icon::Close, "Close the Controls panel (the handle on the picture's right edge opens it again)")) app.prefs.shellPanels = withPanelState(app.prefs.shellPanels, openKey, true);
                sh::text(cp.dl, Type::Title, true, o, sh::cText(), "Controls");
                cp.y += std::max(sh::lineH(Type::Title), hb) + m.s4;
            }
            std::string shownSections = "ctlsections";          // (for the automatic checks: which sections were drawn, in order)
            const SectionFn section = [&](const char* key, const char* title, bool defOpen) {
                shownSections += std::string(" ") + key;
                const std::string name = controlsKey(app, (std::string("ctl.") + key).c_str());
                const bool was = panelCollapsed(app.prefs.shellPanels, name, !defOpen);
                bool collapsed = was;
                const bool revealed = S.revealSection == key;
                if (revealed) collapsed = false;
                const ImVec2 o = sh::here(cp);
                if (cp.y > m.s16 + hb) { cp.y += m.s4 - cp.gap; sh::pRule(cp); }      // (stage 5c.1: every section starts the same distance below the last item of the one before)
                // (stage 5c.1) every header the same: its words on the content's left edge, its arrow (one size) ending on the content's right edge; the hover runs 8 u into the padding
                const ImVec2 h = sh::here(cp);
                if (sh::row((std::string("ctl.sec.") + key).c_str(), ImVec2(h.x - m.s8, h.y), cp.w + 2.0f * m.s8, title, false)) collapsed = !collapsed;
                sh::drawIcon(cp.dl, collapsed ? Icon::Caret : Icon::CaretUp, ImVec2(h.x + cp.w - m.caretW * 0.375f, h.y + m.row / 2.0f), m.caretW * 0.75f, sh::cMuted());
                if (h.y >= pr.y && h.y + m.row <= pr.bottom()) auditRect(std::string("ctlhead.") + key, h.x, h.y, cp.w, static_cast<float>(m.row));   // (only the headers in view)
                (void)o;
                if (revealed) { ImGui::SetScrollY(ImGui::GetScrollY() + h.y - static_cast<float>(fr.dock.y) - m.s12); S.revealSection.clear(); }
                else if (was && !collapsed) ImGui::SetScrollY(std::max(0.0f, ImGui::GetScrollY() + h.y - static_cast<float>(fr.dock.y) - m.s12));   // (stage 5g) a section the owner has just opened comes into view: its header at the top, so its rows are not below the fold
                cp.y += m.row + m.s4;
                if (collapsed != was) app.prefs.shellPanels = withPanelState(app.prefs.shellPanels, name, collapsed);
                return !collapsed;
            };
            collectPanelItems(true);
            sh::bodyFloor(true);
            if (app.lab == 0) orbitControls(app, cp, section); else if (app.lab == 2) defenseControls(app, cp, section); else atomControls(app, cp, section);
            sh::bodyFloor(false);
            collectPanelItems(false);
            auditNote(shownSections);
            std::string want;
            if (takeReveal(want)) {
                float r[4];
                if (itemRect(want, r)) {
                    const float cur = ImGui::GetScrollY(), to = std::min(ImGui::GetScrollMaxY(), std::max(0.0f, cur + r[1] - static_cast<float>(fr.dock.y) - static_cast<float>(fr.dock.h) * 0.3f));
                    ImGui::SetScrollY(to);
                    shiftItem(want, -(to - cur));
                }
            }
            const float need = sh::endPanel(cp);
            { char nb[96]; std::snprintf(nb, sizeof nb, "panelneed controls need=%.0f have=%.0f", need, static_cast<float>(fr.dock.h)); auditNote(nb); }
        }
        noteScrollArea(static_cast<float>(pr.x), static_cast<float>(pr.y), static_cast<float>(pr.w), static_cast<float>(pr.h));
        auditRect("panel.controls", static_cast<float>(pr.x), static_cast<float>(pr.y), static_cast<float>(pr.w), static_cast<float>(pr.h));
        auditRect("dock.controls", static_cast<float>(fr.dock.x), static_cast<float>(fr.dock.y), static_cast<float>(fr.dock.w), static_cast<float>(fr.dock.h));
        if (S.slide >= 1) auditNote("panelopen controls");
    } else noteScrollArea(0, 0, 0, 0);

    // ---- the handle (stage 5c.1, owner's findings after trying 5c): a small tab on the picture's right edge, riding on the panel's left edge and centred on its height; its arrow points left
    // (open) while the panel is closed and right (close) while it is open. It is in front of every window, and by its place never under the panel, the top bar or the toolbar. ----
    if (!paneOpen) {
        const Rect hr = handleRect(dockRect(W, H, m, dockWidth(m, W)), W, m, ease);
        sh::Panel hp;
        if (sh::beginPanel("##shellHandle", hr, hp, sh::Shape::Clear, false)) {     // (stage 5g: flat and modern in every theme: a slim rounded pill with a thin edge and one chevron; no gradient, gloss, bevel, glow or shadow)
            // (5k: no BringWindowToDisplayFront any more: the handle was drawn THROUGH an open Settings / Benchmark window; it stays where it was made, below a window opened after it, and by its place it is never under a panel)
            const ImVec2 a(static_cast<float>(hr.x), static_cast<float>(hr.y)), b(static_cast<float>(hr.right()), static_cast<float>(hr.bottom()));
            ImGui::SetCursorScreenPos(a);
            const bool clicked = ImGui::InvisibleButton("ctl.handle", ImVec2(static_cast<float>(hr.w), static_cast<float>(hr.h)));
            const bool hov = ImGui::IsItemHovered();
            noteItem("ctl.handle");
            if (hov) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); ui::SetTooltip("%s", wantOpen ? "Close the Controls panel" : "Open the Controls panel"); }
            const float rr = static_cast<float>(hr.w) / 2;
            const app::Rgb card = sh::F().c->card, fg = sh::F().c->fg;
            hp.dl->AddRectFilled(a, b, ui::col(hov ? app::mixRgb(card, fg, 0.10f) : card, 1.0f), rr);      // hover: a slightly different fill inside the pill (its own shape, never a halo: the 2 px rule)
            hp.dl->AddRect(a, b, sh::cEdge(), rr, 0, 1.0f);
            const float cx = (a.x + b.x) / 2, cy = (a.y + b.y) / 2, k = static_cast<float>(hr.w) * 0.2f, dir = wantOpen ? 1.0f : -1.0f;   // the chevron points right (close) while the panel is open, left (open) while it is closed
            const ImVec2 chev[3] = {ImVec2(cx - dir * k * 0.5f, cy - k), ImVec2(cx + dir * k * 0.5f, cy), ImVec2(cx - dir * k * 0.5f, cy + k)};
            hp.dl->AddPolyline(chev, 3, hov ? sh::cText() : sh::cMuted(), 0, std::max(1.5f, static_cast<float>(m.s4) * 0.4f));
            if (hov) auditHalo("hover.ctl.handle", a, b, a, b);
            if (clicked) controlsToggle();
            ImGui::End();
        }
        auditRect("handle", static_cast<float>(hr.x), static_cast<float>(hr.y), static_cast<float>(hr.w), static_cast<float>(hr.h));
        auditNote(wantOpen ? "handle open" : "handle closed");
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
                if (chip.opens) ImGui::SetNextItemAllowOverlap();                    // the Today, Stop and play buttons sit on the chip and take their own clicks
                const bool hit = chip.opens && ImGui::InvisibleButton("top.chip", ImVec2(static_cast<float>(c.w), static_cast<float>(c.h)));
                if (chip.opens) noteItem("top.chip");
                const bool hov = chip.opens && ImGui::IsItemHovered();
                bar.dl->AddRectFilled(a, b, open ? sh::cLitFillPub() : hov ? sh::cIconHover() : sh::cFace(), static_cast<float>(c.h) / 2);
                if (sh::F().outline && !open) bar.dl->AddRect(a, b, sh::cEdge(), static_cast<float>(c.h) / 2, 0, 1.0f);
                const float ty = a.y + (static_cast<float>(c.h) - sh::px(Type::Title) - sh::px(Type::Caption)) / 2;
                const float tx = a.x + m.s16 + (chip.play ? static_cast<float>(m.control + m.s4) : 0.0f);
                sh::text(bar.dl, Type::Title, true, ImVec2(tx, ty), sh::cText(), chip.title);
                sh::text(bar.dl, Type::Caption, false, ImVec2(tx, ty + sh::px(Type::Title)), sh::cMuted(), chip.sub);
                bar.dl->PopClipRect();
                S.anchorX[PanelId::Chip] = c.x + c.w / 2;
                if (hit) toggle(PanelId::Chip);
                if (chip.today) {
                    const ImVec2 tpos(static_cast<float>(c.right()) - m.s8 - static_cast<float>(todayW), static_cast<float>(c.y) + (static_cast<float>(c.h) - m.control) / 2);
                    if (sh::button("top.today", tpos, ImVec2(static_cast<float>(todayW), static_cast<float>(m.control)), "Today", Face::Quiet)) todayAction(app);
                }
                if (chip.stop) {
                    const ImVec2 spos(static_cast<float>(c.right()) - m.s8 - static_cast<float>(stopW), static_cast<float>(c.y) + (static_cast<float>(c.h) - m.control) / 2);
                    if (sh::button("top.stop", spos, ImVec2(static_cast<float>(stopW), static_cast<float>(m.control)), "■ Stop", Face::Quiet)) journeyStop(app);
                }
                if (chip.play) {   // play / pause: the owner's note of 6 Oct 2026 (it was on the toolbar, which felt like a phone's media bar); Space does the same
                    const ImVec2 ppos(a.x + m.s4 + m.s4, static_cast<float>(c.y) + (static_cast<float>(c.h) - m.control) / 2);
                    if (sh::iconButton("top.play", ppos, ImVec2(static_cast<float>(m.control), static_cast<float>(m.control)), chip.paused ? Icon::Play : Icon::Pause, chip.paused ? "Play (Space)" : "Pause (Space)")) app.paused = !app.paused;
                }
                auditRect("chip", a.x, a.y, static_cast<float>(c.w), static_cast<float>(c.h));
            }
            // the right side: search, the other lab, Learn, Achievements, 💡, Theme, Settings
            float rx = static_cast<float>(W) - m.s12;
            const auto icon = [&](const char* id, Icon ic, const char* tip, bool on) {
                rx -= ib;
                const bool c = sh::iconButton(id, ImVec2(rx, y0), ImVec2(ib, ib), ic, tip, on);
                auditRect(std::string("topicon.") + (id + 4), rx, y0, static_cast<float>(ib), static_cast<float>(ib));   // (5k: for the tests; id is "top.<name>")
                rx -= static_cast<float>(gap);
                return c;
            };
            if (icon("top.settings", Icon::Gear, "Settings", app.showSettings)) app.showSettings = !app.showSettings;   // (5k: a second click closes it)
            const float themeX = rx - ib;
            // (5k: a second click on the button closes the list. While the list is open Dear ImGui does not hand the click to the button under it, so the press is found by its place)
            static bool eatThemeClick = false;
            const float themeBx = rx - ib;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsMouseHoveringRect(ImVec2(themeBx, y0), ImVec2(themeBx + ib, y0 + ib), false) && ImGui::IsPopupOpen("##themes")) { ImGui::ClosePopupToLevel(0, true); eatThemeClick = true; }
            bool themeClick = icon("top.theme", Icon::Palette, "Theme", false);
            if (themeClick && eatThemeClick) themeClick = false;
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) eatThemeClick = false;
            if (themeClick || S.openThemes) { ImGui::OpenPopup("##themes"); S.openThemes = false; }
            { const ImVec2 hang(themeBx + static_cast<float>(ib), static_cast<float>(m.topBar) + static_cast<float>(m.s4)); themePicker(app, &hang); }   // 5q: below the top bar
            (void)themeX;
            if (icon("top.ach", Icon::Trophy, "Achievements", app.pane == 2)) app.pane = app.pane == 2 ? 0 : 2;
            if (icon("top.learn", Icon::Book, "Learn", app.pane == 1)) app.pane = app.pane == 1 ? 0 : 1;
            if (icon("top.swap", Icon::Swap, app.lab == 0 ? "Go to the Atom Lab" : "Go to the Orbit Lab", false)) openLab(app, app.lab == 0 ? 1 : 0);
            if (icon("top.search", Icon::Search, "Search (Ctrl+K)", S.ui.search)) { if (S.ui.search) S.ui.search = false; else { S.ui.search = true; S.ui.open = PanelId::None; S.query[0] = 0; S.searchSel = 0; S.searchFocus = true; S.queryGen++; } }   // (5k: a second click closes it)
            if (S.askDefaults && app.lab == 2) S.askDefaults = false;               // (the Defense Lab has no settings to put back: its planet is the save file)
            if (S.askDefaults) { ImGui::OpenPopup(app.lab == 1 ? "Put Atom Lab back?" : "Put Orbit Lab back?"); S.askDefaults = false; }
            defaultsPopups(app);
            sh::endPanel(bar);
        }
        auditRect("topbar", 0, 0, static_cast<float>(W), static_cast<float>(m.topBar));
        auditNote(std::string("crumbmode ") + std::to_string(plan.crumbMode));
    }

    // ---- the open dropdown or toolbar panel (one at a time) ----
    { static PanelId prevOpen = PanelId::None; if (prevOpen == PanelId::Pick && S.ui.open != PanelId::Pick) pickFilterClear(); prevOpen = S.ui.open; }
    notePopupArea(0, 0, 0, 0);                                                    // (5q: set again below while a dropdown panel is open)
    Rect openRect{0, 0, 0, 0};
    if (S.ui.open != PanelId::None) {
        const PanelId id = S.ui.open;
        const bool orbitOnly = orbitPanelWidthEm(id) > 0 && id != PanelId::Chip, atomOnly = atomPanelWidthEm(id) > 0 && id != PanelId::Chip;   // (the chip belongs to both labs)
        if ((orbitOnly && app.lab != 0) || (atomOnly && app.lab != 1)) S.ui.open = PanelId::None;
        else {
            const std::string wid = std::string("##panel_") + panelName(id);
            const double ax = S.anchorX.count(id) ? S.anchorX[id] : W / 2;
            openRect = placePanel(id == PanelId::Data ? Anchor::Centre : fromTop(id) ? Anchor::UnderChip : Anchor::AboveToolbar, m.em(id == PanelId::Chip && app.lab == 1 ? atomPanelWidthEm(id) : widthEmOf(id)), sh::lastHeight(wid, static_cast<float>(m.em(10))), fr, m, ax);
            const bool hoverPicker = id == PanelId::Chip && app.lab == 1;           // 5q: the Atom Lab's Element picker hovers: it may step sideways round a card (its own position only), and where it still covers the card the card is left out
            if (id != PanelId::LabMenu)                                           // (5l: the Labs dropdown stays under its button and is drawn over a card; the other panels step aside)
                for (const Rect& c : cards) openRect = sideStep(openRect, c, fr.window, m.s12, m.s12);
            if (fromTop(id) && id != PanelId::LabMenu) {                              // 5p: the Element picker (and the other panels from the top bar) never reach the fact line, the toolbar or the FPS pill
                double limit = fr.toolbar.y;
                if (g_bottomTop > 0) limit = std::min(limit, g_bottomTop);
                if (fr.readout.w > 0 && fr.readout.x < openRect.right() && fr.readout.right() > openRect.x) limit = std::min(limit, fr.readout.y);
                if (hoverPicker && g_bottomTop > 0) limit = std::min(limit, g_bottomTop - 42.0 * dpi);   // 5q: and above the scale bar's label (it stands 22 pt above the fact line / toolbar row, its label 20 pt over it)
                openRect.h = std::max<double>(m.em(6), std::min(openRect.h, limit - m.s12 - openRect.y));
            }
            if (id == PanelId::Data) openRect.x = std::max<double>(m.s12, std::min(openRect.x, (fr.freeW > 0 ? fr.freeW : fr.window.w) - m.s12 - openRect.w));   // stage 5i: the side step never pushes the sheet under the Controls panel
            sh::Panel p;
            // 5l: a scrollbar only when the content truly does not fit the room (it needed more than the room it was given last frame); the Labs dropdown is opaque and in front of everything
            const float needLast = sh::lastHeight(wid, -1.0f);
            const bool canScroll = needLast < 0 || needLast > static_cast<float>(openRect.h) + 1.0f;
            if (sh::beginPanel(wid.c_str(), openRect, p, sh::Shape::Panel, canScroll, id == PanelId::Data || id == PanelId::LabMenu)) {   // (stage 5g: the data sheet is solid in the plain themes: the picture's labels showed through it)
                if (id == PanelId::LabMenu) ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
                if (id == PanelId::Data) auditNote("datasheet solid");
                bool close = false;
                if (!buildOrbitPanel(id, app, p, close) && !buildAtomPanel(id, app, p, close)) {
                    if (id == PanelId::Menu) menuPanel(app, p, close);
                    else if (id == PanelId::LabMenu) labMenuPanel(app, p, close);
                    else if (id == PanelId::Facts) factsPanel(app, p, close);
                    else if (id == PanelId::Data) { sh::bodyFloor(true); dataPanel(app, p, close); sh::bodyFloor(false); }
                    else if (id == PanelId::More) { std::vector<ToolItem> hidden(tools.begin() + shown, tools.end()); morePanel(p, hidden, close); }
                }
                { std::string want; float ir[4];                                              // 5q: a scripted click on a control scrolled out of this panel: scroll it in (the same as the Controls panel does)
                  if (takeRevealPopup(want) && itemRect(want, ir)) {
                      const float cur = ImGui::GetScrollY(), to = std::min(ImGui::GetScrollMaxY(), std::max(0.0f, cur + ir[1] - static_cast<float>(openRect.y) - static_cast<float>(openRect.h) * 0.3f));
                      ImGui::SetScrollY(to);
                      shiftItem(want, -(to - cur));
                  } }
                const float need = sh::endPanel(p);
                { char nb[96]; std::snprintf(nb, sizeof nb, "panelneed %s need=%.0f have=%.0f scroll=%.0f", panelName(id), need, static_cast<float>(openRect.h), sh::lastScrollMax(wid)); auditNote(nb); }
                if (close && S.ui.open == id) S.ui.open = PanelId::None;
            }
            if (hoverPicker) pickerNow = openRect; else occ.push_back(openRect);   // 5q: a hovering picker is not an occluder
            notePopupArea(static_cast<float>(openRect.x), static_cast<float>(openRect.y), static_cast<float>(openRect.w), static_cast<float>(openRect.h));
            auditRect(std::string("panel.") + panelName(id), static_cast<float>(openRect.x), static_cast<float>(openRect.y), static_cast<float>(openRect.w), static_cast<float>(openRect.h));
            auditNote(std::string("panelopen ") + panelName(id));
            // a click anywhere else closes it (a click on the bars is their own business)
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && S.openedFrame != ImGui::GetFrameCount() && !mouseIn(openRect) && !mouseIn(fr.topBar) && !mouseIn(fr.toolbar)) S.ui.open = PanelId::None;
        }
    }
    // ---- search ----
    if (S.ui.search) {
        const Rect r = searchPanel(app, fr, m, cards);
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
    // ---- the fact line (Settings → Facts) ----
    double bottomLeftTop = fr.toolbar.y;                  // (stage 5d) the top of what the new UI puts at the picture's bottom left: the toolbar row and the fact line
    if (!pane && app.prefs.facts == "on" && !app.journey.active && S.ui.open != PanelId::Facts) {   // (stage 5d: the Atom Lab too; its scale bar and decay lines now sit above the toolbar row and this line)
        const Rect fl = factLine(app, fr, m, occ, dpi, dt);
        if (fl.w > 0) { occ.push_back(fl); auditRect("factline", static_cast<float>(fl.x), static_cast<float>(fl.y), static_cast<float>(fl.w), static_cast<float>(fl.h)); bottomLeftTop = std::min(bottomLeftTop, fl.y); }
    }
    g_bottomTop = bottomLeftTop;
    pickerPrev = pickerNow;                                // 5q: (the card, next frame, is left out where this covers it)
    app.atom.insetBottomLeft = (fr.window.bottom() - bottomLeftTop) / dpi + 4;       // points (main.cpp sets it to 0 when the new UI is off)
    if (app.lab == 1) {                                   // 0.15 stage 5h: the atom is centred in the part of the picture the bars, the Controls panel and the subject card leave free (the label under the nucleus too)
        const bool pickerOpen = S.ui.open == PanelId::Chip && freeSavedOk;      // 5q: with the picker open the picture, toolbar, FPS pill and scale bar keep the place they had
        const Rect fr2 = pickerOpen ? Rect{freeSaved[0], freeSaved[1], freeSaved[2], freeSaved[3]} : atomFreeRect(fr.window, occ, bottomLeftTop);
        if (!pickerOpen) { freeSaved[0] = fr2.x; freeSaved[1] = fr2.y; freeSaved[2] = fr2.w; freeSaved[3] = fr2.h; freeSavedOk = true; }   // (5p: not through the toolbar's own place: the toolbar is centred on this, and it flipped every frame with the picker open)
        app.atom.freeX = fr2.x / dpi; app.atom.freeY = fr2.y / dpi; app.atom.freeW = fr2.w / dpi; app.atom.freeH = fr2.h / dpi;
        app.atom.infoSize = 14;                           // the info text at the top left: the body size (14), not 13
        app.atom.infoHidden = S.ui.open == PanelId::Chip;  // (5p: the Element picker covers that corner)
    }

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
    app.set.insetTop = fr.topBar.h / dpi + 4;                         // (stage 5c: the picture draws no ring label under the top bar or the Controls panel)
    app.set.insetRight = (W - fr.freeW) / dpi;
    app.set.reserveRight = (H - fr.readout.y) / dpi + 8;
    for (const Rect& r : occ) { char b[96]; std::snprintf(b, sizeof b, "occ%zu", static_cast<std::size_t>(&r - &occ[0])); auditRect(b, static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h)); }
    auditNote("metrics body=" + std::to_string(m.body) + " caption=" + std::to_string(m.caption) + " title=" + std::to_string(m.title) + " key=" + std::to_string(m.key) + " control=" + std::to_string(m.control) + " topbar=" + std::to_string(m.topBar));
}

}  // namespace ui
