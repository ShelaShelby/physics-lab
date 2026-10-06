#include "ui/reading.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"
#include "ui/fonts.h"
#include "ui/look.h"
#include "ui/orbit_panel.h"
#include "ui/shell_test.h"
#include "ui/shell_widgets.h"
#include "ui/style_draw.h"

namespace ui {

namespace {

struct Head { int level; std::string text; float y; };
struct Reading {
    bool mode = false;
    bool shown = false;               // a page window is open this frame (the column was drawn)
    bool pushedFont = false;
    bool began = false;               // the column window was opened this frame (pageEnd has to End it)
    std::vector<Head> heads, now;     // the headings of the last frame (what the contents list shows) and of this one
    float scrollTo = -1;
    bool contentsOpen = false;
    bool tocFront = false;            // the Contents list was just opened: it comes in front of the page
    int lastFrame = -10;              // the frame this page was last drawn in (a page that was not drawn the frame before has just opened)
    float wheel = 0;                  // the wheel turned over the dimmed picture beside the column: the page scrolls by it
    std::string title;
    float bodyFont = 0;
    int longest = 0, longestCaption = 0;   // the most characters on one line of the Body text, and of the Caption text, this frame
} g;

// 0.15 stage 5g: the keys PageUp / PageDown / Home / End / Up / Down scroll the page that has the focus (the windows have no keyboard navigation of their own), and a wheel turned
// over the dimmed picture beside the reading column scrolls the page too (the old floating windows scroll by the wheel through Dear ImGui itself)
void pageKeys(float wheel) {
    const ImGuiIO& io = ImGui::GetIO();
    const float y = ImGui::GetScrollY(), h = ImGui::GetWindowHeight();
    float to = y;
    if (wheel != 0) to -= wheel * std::trunc(std::min(2.5f * ImGui::GetFontSize(), h * 0.67f));
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) to += h * 0.9f;
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) to -= h * 0.9f;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) to += ImGui::GetFontSize() * 2.5f;
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) to -= ImGui::GetFontSize() * 2.5f;
        if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) to = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_End, false)) to = ImGui::GetScrollMaxY();
    }
    if (to != y) ImGui::SetScrollY(std::max(0.0f, std::min(to, ImGui::GetScrollMaxY())));
}

}  // namespace

void setReadingMode(bool on) { if (!on) { g.shown = false; g.contentsOpen = false; } g.mode = on; }
bool readingShown() { return g.shown; }
float readingBodyFont() { return g.mode && g.shown ? g.bodyFont : 0; }
float readingFloorBody() { return g.mode && g.shown ? static_cast<float>(sh::F().m.body) : 0; }
float readingFloorCaption() { return g.mode && g.shown ? static_cast<float>(sh::F().m.caption) : 0; }
void readingNote(const std::string& line) { auditNote(line); }
void readingLongestLine(int chars, bool body) { if (!g.mode) return; int& l = body ? g.longest : g.longestCaption; if (chars > l) l = chars; }

void noteHeading(int level, const std::string& text) {
    if (!g.mode || !g.shown) return;
    g.now.push_back({level, text, ImGui::GetCursorPosY()});
}

bool pagesOpen(const AppState& app) { return app.showAbout || app.showHistory || app.showLabLog || (app.lab == 0 ? app.learn > 0 : app.atomLearn > 0); }
void closePages(AppState& app) { app.showAbout = app.showHistory = app.showLabLog = false; app.learn = 0; app.atomLearn = 0; }

bool pageBegin(const std::string& title, bool* open) {
    if (!g.mode) {
        pushFloatStyle();
        const bool shown = ImGui::Begin(title.c_str(), open, ImGuiWindowFlags_NoCollapse);
        popFloatStyle();
        if (shown) { panelDecor(app::PanelKind::Floating); pageKeys(0); auditNote("pagescroll y=" + std::to_string(int(ImGui::GetScrollY())) + " max=" + std::to_string(int(ImGui::GetScrollMaxY()))); }   // for automatic checks (the wheel moves the page)
        return shown;
    }
    // ---- the reading view ----
    const sh::Frame& f = sh::F();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const double W = vp->Size.x, H = vp->Size.y;
    const app::shell::ReadingGeom geom = app::shell::readingGeometry(W, H, f.m);
    // a click on the dimmed picture closes the page (and never reaches the picture)
    ImGui::SetNextWindowPos(ImVec2(0, static_cast<float>(f.m.topBar)), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(W), static_cast<float>(H - f.m.topBar)), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##readBlock", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing);
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    ImGui::BringWindowToDisplayBack(ImGui::GetCurrentWindow());                  // behind the bars, the toolbar and the cards: only the picture is dimmed
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0, static_cast<float>(f.m.topBar)), ImVec2(static_cast<float>(W), static_cast<float>(H)), IM_COL32(0, 0, 0, 153));   // the picture dims to 60 % black (in this window, so a saved picture is not dimmed)
    ImGui::SetCursorScreenPos(ImVec2(0, static_cast<float>(f.m.topBar)));
    if (ImGui::InvisibleButton("##readClose", ImVec2(static_cast<float>(W), static_cast<float>(H - f.m.topBar)))) *open = false;
    g.wheel = ImGui::IsItemHovered() ? ImGui::GetIO().MouseWheel : 0;
    ImGui::End();

    sh::Panel p;
    app::shell::Rect col = geom.column;
    g.shown = true;
    g.now.clear();
    g.longest = g.longestCaption = 0;
    g.title = title.substr(0, title.find("###"));
    if (g.lastFrame != ImGui::GetFrameCount() - 1) ImGui::SetNextWindowFocus();   // a page that has just opened has the focus, so its keys work at once (stage 5g)
    g.lastFrame = ImGui::GetFrameCount();
    const bool shown = sh::beginPanel("##reading", col, p, sh::Shape::Panel, true);
    g.began = shown;
    if (!shown) return false;
    g.bodyFont = 16.0f * f.m.body / 14.0f;                                       // a page's text sizes are in "web points" times font/16: 14 pt text = the Body size
    // the header: the title (H1) and a close button; under it, when there is no rail, the Contents button
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float pad = static_cast<float>(f.m.s16), btn = static_cast<float>(f.m.iconBtn);
    const ImVec2 o = ImGui::GetWindowPos();
    const float titleRight = o.x + static_cast<float>(col.w) - pad - btn - f.m.s8;
    if (sh::iconButton("##readX", ImVec2(o.x + static_cast<float>(col.w) - pad - btn, o.y + pad), ImVec2(btn, btn), sh::Icon::Close, "Close (Esc)")) *open = false;
    {   // the title, one line (H1); a long one is cut at the space left
        std::string t = g.title;
        while (t.size() > 3 && sh::font(true)->CalcTextSizeA(sh::headingPx("h1"), FLT_MAX, 0, t.c_str()).x > titleRight - (o.x + pad)) t.resize(t.size() - 1);
        if (t != g.title) t += "…";
        sh::headingText(dl, "h1", ImVec2(o.x + pad, o.y + pad + (btn - sh::headingPx("h1")) / 2), sh::cText(), t);
    }
    float headerBottom = o.y + pad + btn;
    if (!geom.rail && !g.heads.empty()) {
        const float cw = sh::width(sh::Type::Body, false, "Contents") + 2.0f * f.m.s12 + f.m.caretW;
        const float cy = headerBottom + f.m.s4;
        if (sh::button("##readContents", ImVec2(o.x + pad, cy), ImVec2(cw, static_cast<float>(f.m.control)), "")) { g.contentsOpen = !g.contentsOpen; g.tocFront = g.contentsOpen; }
        sh::text(dl, sh::Type::Body, false, ImVec2(o.x + pad + f.m.s12, cy + (f.m.control - sh::px(sh::Type::Body)) / 2), sh::cText(), "Contents");
        sh::drawIcon(dl, g.contentsOpen ? sh::Icon::CaretUp : sh::Icon::Caret, ImVec2(o.x + pad + cw - f.m.s8 - f.m.caretW / 2.0f, cy + f.m.control / 2.0f), static_cast<float>(f.m.caretW) * 0.75f, sh::cMuted());
        headerBottom = cy + f.m.control;
    }
    ImGui::SetCursorScreenPos(ImVec2(p.origin.x, headerBottom + f.m.s12));
    ImGui::Dummy(ImVec2(1, 1));
    // the contents rail, or the menu under the button
    if (geom.rail || g.contentsOpen) {
        app::shell::Rect r = geom.rail ? geom.toc : app::shell::Rect{o.x + pad, o.y + pad + btn + f.m.s4 + f.m.control + f.m.s4, static_cast<double>(std::min<double>(f.m.em(24), col.w - 2.0 * pad)), static_cast<double>(std::min<double>(H * 0.6, 40.0 * f.m.row))};
        if (!geom.rail) r = app::shell::clampInto(r, app::shell::Rect{0, 0, W, H});
        sh::Panel tp;
        if (g.tocFront) { ImGui::SetNextWindowFocus(); g.tocFront = false; }   // (stage 5g: the click on the Contents button had brought the page in front of the list, so its second opening could not be clicked)
        if (sh::beginPanel("##readtoc", r, tp, sh::Shape::Panel, true)) {
            auditRect(geom.rail ? "reading.toc" : "reading.contents", static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h));
            sh::text(tp.dl, sh::Type::Caption, false, tp.origin, sh::cMuted(), "Contents");
            tp.y += sh::lineH(sh::Type::Caption) + f.m.s8;
            int k = 0;
            for (const Head& h : g.heads) {
                const float indent = (h.level - 1) * static_cast<float>(f.m.s12);
                const std::string id = "##toc" + std::to_string(k++);
                if (sh::row(id.c_str(), ImVec2(tp.origin.x + indent, tp.origin.y + tp.y), tp.w - indent, h.text, false)) { g.scrollTo = std::max(0.0f, h.y - static_cast<float>(f.m.s8)); g.contentsOpen = false; }
                tp.y += f.m.row;
            }
            sh::endPanel(tp);
        }
    }
    if (g.scrollTo >= 0) { ImGui::SetScrollY(g.scrollTo); g.scrollTo = -1; }
    else pageKeys(g.wheel);
    ImGui::PushFont(nullptr, g.bodyFont / (f.dpi * f.tf));                       // the pages' own sizes (S() = font / 16 of the web) now mean the Body size
    g.pushedFont = true;
    auditNote("pagescroll y=" + std::to_string(int(ImGui::GetScrollY())) + " max=" + std::to_string(int(ImGui::GetScrollMaxY())));
    auditRect("reading.column", static_cast<float>(col.x), static_cast<float>(col.y), static_cast<float>(col.w), static_cast<float>(col.h));
    return true;
}

void pageEnd() {
    if (g.mode && g.pushedFont) { ImGui::PopFont(); g.pushedFont = false; }
    if (g.mode) {
        if (g.began) { g.heads = g.now; auditNote("readmax " + std::to_string(g.longest) + " " + std::to_string(g.longestCaption));
            auditNote("toc " + std::to_string(g.now.size())); ImGui::End(); }                          // the headings this frame saw are the contents of the next
        g.began = false;
        return;
    }
    ImGui::End();
}

}  // namespace ui
