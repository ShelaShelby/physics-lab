#include "ui/reading.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    bool clipped = false;             // 5k: the content's clip rect (under the header) was pushed
    bool began = false;               // the column window was opened this frame (pageEnd has to End it)
    std::vector<Head> heads, now;     // the headings of the last frame (what the contents list shows) and of this one
    float scrollTo = -1;
    int jumpHead = -1, jumpFrames = 0;   // 5k: a Contents choice keeps aiming at its heading for a few frames (the page's lines are measured as they are drawn, so the heading moves a little)
    float startLocal = 0;
    bool contentsOpen = false;
    bool tocFront = false;            // the Contents list was just opened: it comes in front of the page
    int lastFrame = -10;              // the frame this page was last drawn in (a page that was not drawn the frame before has just opened)
    int tocPress = -1;                // 5k: the Contents row a press began in (found by its place, not by Dear ImGui's hover)
    float prevY = 0; bool prevValid = false;   // 5k: the page's scroll at the end of the last frame (what the app-level fallback compares with)
    float grab = 0;                   // 5k: where in the scrollbar's thumb it was grabbed
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

// 0.15 stage 5k: the New UI's pages are scrolled by the APP, after the Dear ImGui frame, so a wheel or a key works whichever window Dear ImGui thinks is hovered or focused (the owner's
// PC: About did not scroll with the wheel, click-then-wheel or PageDown; the cloud tests could not reproduce it, so the cause was not found; this path does not depend on it).
// Never twice for one event: if the page moved by itself this frame (Dear ImGui's own wheel), nothing is added.
bool wheelMayScroll(ImGuiWindow* page) {
    ImGuiContext& c = *GImGui;
    if (c.OpenPopupStack.Size > 0) return false;                               // a popup / combo list is open
    const ImGuiWindow* hv = c.HoveredWindow;
    if (!hv) return true;
    if (hv == page || hv->RootWindow == page->RootWindow) return true;
    if (std::strcmp(hv->Name, "##readtoc") == 0) return false;                 // the Contents list under the pointer scrolls itself
    if (hv->Flags & (ImGuiWindowFlags_Popup | ImGuiWindowFlags_Tooltip)) return false;
    return !(hv->ScrollMax.y > 0);                                             // another scrolling panel under the pointer keeps its wheel
}

}  // namespace

void readingFrameEnd() {
    ImGuiWindow* w = g.mode && g.shown && g.lastFrame == ImGui::GetFrameCount() ? ImGui::FindWindowByName("##reading") : nullptr;
    if (!w) { g.prevValid = false; return; }
    const ImGuiIO& io = ImGui::GetIO();
    const float y = w->Scroll.y, mx = w->ScrollMax.y, h = w->InnerRect.GetHeight(), fs = ImGui::GetFontSize();
    const bool moved = g.prevValid && std::fabs(y - g.prevY) > 0.01f;
    float to = y;
    const bool userInput = io.MouseWheel != 0 || ImGui::IsKeyPressed(ImGuiKey_PageDown) || ImGui::IsKeyPressed(ImGuiKey_PageUp) || ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow);
    if (g.scrollTo >= 0) { to = g.scrollTo; g.scrollTo = -1; }                  // a Contents choice: applied here whichever window has the focus
    else if (g.jumpHead >= 0 && g.jumpFrames > 0 && !userInput) {             // ... and it keeps aiming at the heading's place of this frame
        if (g.jumpHead < static_cast<int>(g.heads.size())) to = std::max(0.0f, g.heads[g.jumpHead].y - g.startLocal);
        g.jumpFrames--;
    } else if (mx > 0) {
        g.jumpHead = -1;
        if (io.MouseWheel != 0 && !moved && wheelMayScroll(w)) to -= io.MouseWheel * std::trunc(std::min(2.5f * fs, h * 0.67f));
        if (!io.WantTextInput) {
            if (ImGui::IsKeyPressed(ImGuiKey_PageDown)) to += h * 0.9f;
            if (ImGui::IsKeyPressed(ImGuiKey_PageUp)) to -= h * 0.9f;
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) to += fs * 2.5f;
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) to -= fs * 2.5f;
            if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) to = 0;
            if (ImGui::IsKeyPressed(ImGuiKey_End, false)) to = mx;
        }
    }
    to = std::max(0.0f, std::min(to, mx));
    if (to != y) ImGui::SetScrollY(w, to);                                     // takes effect when the window next begins
    g.prevY = to;
    g.prevValid = true;
}

// the slim scrollbar of a reading page (about 8 px, rounded, muted; wider and brighter under the pointer; draggable; none when the page fits)
static void readingScrollbar() {
    ImGuiWindow* w = ImGui::FindWindowByName("##reading");
    const float mx = w ? w->ScrollMax.y : 0;
    if (!w || mx <= 0.5f) return;
    const sh::Frame& f = sh::F();
    const float bw = 2.0f * static_cast<float>(f.m.s4), hitW = bw * 2.0f, margin = static_cast<float>(f.m.s4);
    const float x0 = w->Pos.x + w->Size.x - margin - hitW, ty0 = w->Pos.y + margin * 2, track = w->Size.y - margin * 4;
    ImGui::SetNextWindowPos(ImVec2(x0, ty0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(hitW, track), ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1, 1));
    const bool open = ImGui::Begin("##readBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar(3);
    if (open) {
        const float view = w->InnerRect.GetHeight(), thumbH = std::min(track, std::max(static_cast<float>(f.m.s4) * 6, track * view / (view + mx)));
        ImGui::SetCursorScreenPos(ImVec2(x0, ty0));
        ImGui::InvisibleButton("##readBarHit", ImVec2(hitW, track));
        const bool hov = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem), act = ImGui::IsItemActive();
        float thumbY = ty0 + (track - thumbH) * (w->Scroll.y / mx);
        const float my = ImGui::GetIO().MousePos.y;
        if (ImGui::IsItemActivated()) g.grab = (my >= thumbY && my <= thumbY + thumbH) ? my - thumbY : thumbH / 2;
        if (act && track > thumbH) {
            const float to = std::max(0.0f, std::min(mx, (my - g.grab - ty0) / (track - thumbH) * mx));
            ImGui::SetScrollY(w, to);
            g.prevY = to;
            thumbY = ty0 + (track - thumbH) * (to / mx);
        }
        const float tw = hov || act ? bw * 1.4f : bw;
        ImU32 c = sh::cMuted();
        c = (c & ~(0xFFu << IM_COL32_A_SHIFT)) | (static_cast<ImU32>(hov || act ? 235 : 150) << IM_COL32_A_SHIFT);
        const ImVec2 a(x0 + hitW - tw, thumbY), b(x0 + hitW, thumbY + thumbH);
        ImGui::GetWindowDrawList()->AddRectFilled(a, b, c, tw / 2);
        auditRect("reading.scrollbar", x0, ty0, hitW, track);
        auditRect("reading.scrollthumb", a.x, a.y, b.x - a.x, b.y - a.y);
    }
    ImGui::End();
}

std::string readingAnchorsNow() {   // 5k: where each Contents entry would put the page now (tests)
    std::string a;
    for (const Head& h : g.heads) a += " " + std::to_string(int(std::max(0.0f, h.y - g.startLocal)));
    return a;
}

bool readingScrollNow(float* y, float* mx) {   // 5k: for the test script (verb pscroll)
    ImGuiWindow* w = g.mode && g.shown ? ImGui::FindWindowByName("##reading") : nullptr;
    if (!w) return false;
    *y = w->Scroll.y; *mx = w->ScrollMax.y;
    return true;
}

std::string readingDiag() {
    ImGuiWindow* w = g.mode && g.shown ? ImGui::FindWindowByName("##reading") : nullptr;
    char b[400];
    std::string s;
    if (w) {
        std::snprintf(b, sizeof b, "reading page: shown, title=\"%s\" scroll y=%.1f max=%.1f flags=0x%x rect=%.1f,%.1f,%.1f,%.1f\n", g.title.c_str(), w->Scroll.y, w->ScrollMax.y, static_cast<unsigned>(w->Flags), w->Pos.x, w->Pos.y, w->Size.x, w->Size.y);
        s += b;
    } else s += "reading page: none open (Old layout or no page)\n";
    s += std::string("dimmed layer: ") + (w ? "yes" : "no") + "\n";
    s += std::string("contents list: ") + (g.contentsOpen ? "open (dropdown)" : (w && ImGui::FindWindowByName("##readtoc") && ImGui::FindWindowByName("##readtoc")->WasActive ? "rail shown" : "closed")) + "\n";
    s += "popups open: " + std::to_string(GImGui->OpenPopupStack.Size) + "\n";
    return s;
}

void setReadingMode(bool on) { if (!on) { g.shown = false; g.contentsOpen = false; } g.mode = on; }
bool readingModeOn() { return g.mode; }
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
    // 5k, THE CAUSE of "About does not scroll" on the owner's PC: the page's content used to start at a fixed SCREEN position (SetCursorScreenPos), so the scroll offset changed (the logged
    // number moved, the cloud tests only read that number) but the text never moved on the screen. Now the content starts at a fixed place in the window's own (scrolling) coordinates, and
    // it is clipped below the header so it slides under the title and the buttons.
    const float startLocal = headerBottom + f.m.s12 - o.y;
    g.startLocal = startLocal;
    ImGui::SetCursorPos(ImVec2(pad, startLocal));
    ImGui::Dummy(ImVec2(1, 1));
    // the contents rail, or the menu under the button
    if (geom.rail || g.contentsOpen) {
        app::shell::Rect r = geom.rail ? geom.toc : app::shell::Rect{o.x + pad, o.y + pad + btn + f.m.s4 + f.m.control + f.m.s4, static_cast<double>(std::min<double>(f.m.em(24), col.w - 2.0 * pad)), static_cast<double>(std::min<double>(H * 0.6, 40.0 * f.m.row))};
        if (!geom.rail) r = app::shell::clampInto(r, app::shell::Rect{0, 0, W, H});
        sh::Panel tp;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) g.tocPress = -1;
        if (g.tocFront) { ImGui::SetNextWindowFocus(); g.tocFront = false; }   // (stage 5g: the click on the Contents button had brought the page in front of the list, so its second opening could not be clicked)
        if (sh::beginPanel("##readtoc", r, tp, sh::Shape::Panel, true)) {
            ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());       // (5k: the list is always in front of the page, whatever was clicked last)
            auditRect(geom.rail ? "reading.toc" : "reading.contents", static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.w), static_cast<float>(r.h));
            sh::text(tp.dl, sh::Type::Caption, false, tp.origin, sh::cMuted(), "Contents");
            tp.y += sh::lineH(sh::Type::Caption) + f.m.s8;
            int k = 0;
            for (const Head& h : g.heads) {
                const float indent = (h.level - 1) * static_cast<float>(f.m.s12);
                const std::string id = "##toc" + std::to_string(k++);
                const ImVec2 rp(tp.origin.x + indent, tp.origin.y + tp.y);
                auditRect("reading.tocrow." + std::to_string(k - 1), rp.x, rp.y, tp.w - indent, static_cast<float>(f.m.row));
                bool chosen = sh::row(id.c_str(), rp, tp.w - indent, h.text, false);
                {   // (5k) the same choice found by the row's place: a press and a release in the row, whichever window Dear ImGui calls hovered
                    const ImVec2 mp = ImGui::GetIO().MousePos;
                    const bool inRow = mp.x >= rp.x && mp.x < rp.x + tp.w - indent && mp.y >= rp.y && mp.y < rp.y + f.m.row && mp.x >= r.x && mp.x < r.x + r.w && mp.y >= r.y && mp.y < r.y + r.h;
                    if (inRow && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) g.tocPress = k - 1;
                    if (inRow && ImGui::IsMouseReleased(ImGuiMouseButton_Left) && g.tocPress == k - 1) chosen = true;
                }
                if (chosen) { g.scrollTo = std::max(0.0f, h.y - startLocal); g.jumpHead = k - 1; g.jumpFrames = 8; g.contentsOpen = false; g.tocPress = -1; auditNote("tocjump " + std::to_string(k - 1)); }
                tp.y += f.m.row;
            }
            sh::endPanel(tp);
        }
    }
    ImGui::PushClipRect(ImVec2(o.x, headerBottom + f.m.s4), ImVec2(o.x + static_cast<float>(col.w), o.y + static_cast<float>(col.h)), true);   // the content slides under the header (5k)
    g.clipped = true;
    ImGui::PushFont(nullptr, g.bodyFont / (f.dpi * f.tf));                       // the pages' own sizes (S() = font / 16 of the web) now mean the Body size
    g.pushedFont = true;
    auditNote("pagescroll y=" + std::to_string(int(ImGui::GetScrollY())) + " max=" + std::to_string(int(ImGui::GetScrollMaxY())));
    {   std::string a = "tocanchors";   // the place each Contents entry jumps to (5k: for the tests)
        for (const Head& h : g.heads) a += " " + std::to_string(int(std::max(0.0f, h.y - startLocal)));
        auditNote(a); }
    auditRect("reading.column", static_cast<float>(col.x), static_cast<float>(col.y), static_cast<float>(col.w), static_cast<float>(col.h));
    return true;
}

void pageEnd() {
    if (g.mode && g.pushedFont) { ImGui::PopFont(); g.pushedFont = false; }
    if (g.mode && g.clipped) { ImGui::PopClipRect(); g.clipped = false; }
    if (g.mode) {
        if (g.began) { g.heads = g.now; auditNote("readmax " + std::to_string(g.longest) + " " + std::to_string(g.longestCaption));
            auditNote("toc " + std::to_string(g.now.size())); ImGui::End(); readingScrollbar(); }                          // the headings this frame saw are the contents of the next
        g.began = false;
        return;
    }
    ImGui::End();
}

}  // namespace ui
