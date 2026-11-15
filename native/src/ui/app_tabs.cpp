#include "app_tabs.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#include "app/app_data.h"
#include "atom_panel.h"
#include "fonts.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "look.h"
#include "style_draw.h"

namespace ui {

namespace {

float S() { return ImGui::GetFontSize() / 16 / look().textScale; }
float T() { return look().textScale; }
double now() { return ImGui::GetTime(); }

float fontPx(float px) { return ImGui::GetStyle().FontSizeBase * px / 16; }

// ---------------------------------------------------------------- the 💡 facts line
std::vector<int> g_order;                           // the facts in a shuffled order
int g_pos = -1;
double g_next = 0, g_setAt = 0, g_fadeAt = -1;      // the next change; when the current text appeared; when it began to fade out
bool g_held = false;

int utf16Length(const std::string& s) {
    int n = 0;
    for (std::size_t i = 0; i < s.size(); i++) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if ((c & 0xC0) == 0x80) continue;
        n += c >= 0xF0 ? 2 : 1;
    }
    return n;
}

// That theme was archived, so the entry is left out of the facts (everywhere).

// 0.22: each fact belongs to the labs it is about, so the Orbit Lab no longer shows carbon-14 nor the Atom Lab sound in space. One mask per entry of app::kDyk, in its order
// (1 Orbit Lab, 2 Atom Lab, 4 Molecule Lab, 8 Black Hole Lab (0.55.0); kAll: any lab). (The Molecule Lab shows the general facts since 0.22 stage 7: its fact line was hidden under its style bar before.)
constexpr unsigned kO = 1, kA = 2, kM = 4, kB = 8, kAll = kO | kA | kM | kB;
constexpr unsigned kFactLabs[] = {
    kO,        // sunlight takes 8 min 20 s
    kO,        // a day on Venus
    kO,        // the Moon drifts away
    kAll,      // lightning, 30,000 K
    kO,        // the Sun's core
    kO | kA,   // a teaspoon of neutron star (a star made of nuclear matter)
    kA | kM,   // helium stays liquid
    kAll,      // absolute zero
    kA,        // iron-56 and nickel-62
    kA,        // bismuth-209
    kA,        // uranium-238
    kA | kM,   // tungsten
    kA | kM,   // gallium
    kA | kM,   // mercury and bromine
    kA,        // proton and electron masses
    kA,        // the nucleus is small
    kA | kM,   // water is densest at 4 °C
    kAll,      // the speed of light
    kO | kB,   // escape speed (0.55.0: and the Black Hole Lab: from a black hole none is enough)
    kO,        // the space station
    kA,        // carbon-14
    kA | kM,   // diamond and graphite
    kO,        // sound in space
    kA | kM,   // hydrogen, the most common element
    kO | kB};  // a light year
static_assert(sizeof(kFactLabs) / sizeof(kFactLabs[0]) == sizeof(app::kDyk) / sizeof(app::kDyk[0]), "one lab mask per fact");
static_assert(sizeof(app::kDykSrc) / sizeof(app::kDykSrc[0]) == sizeof(app::kDyk) / sizeof(app::kDyk[0]), "one source per fact (0.40)");
int g_forced = -1;                                   // a fact chosen in search is shown in any lab, until the next step
bool factFits(int i, int lab) { return i == g_forced || (lab >= 0 && lab < app::kLabCount && (kFactLabs[i] & (1u << lab)) != 0); }
std::string textOf(const AppState&, int i) { return app::kDyk[i]; }

void sched(const AppState& app) {                    // waits while you hover it; Reduced motion: only on tap
    g_next = g_held || look().reduced ? DBL_MAX : now() + (6000 + 55.0 * utf16Length(textOf(app, g_order[g_pos]))) / 1000;
}

void showNext(const AppState& app) {
    if (g_order.empty()) {
        for (int i = 0; i < int(app::count(app::kDyk)); i++) g_order.push_back(i);
        std::mt19937 rng{std::random_device{}()};
        for (int i = int(g_order.size()) - 1; i > 0; i--) std::swap(g_order[i], g_order[std::uniform_int_distribution<int>(0, i)(rng)]);
        if (const char* f = std::getenv("PL_DYK_FIRST")) {                   // for automatic checks: this fact first
            const auto it = std::find(g_order.begin(), g_order.end(), std::atoi(f));
            if (it != g_order.end()) std::rotate(g_order.begin(), it, it + 1);
        }
    }
    if (g_pos < 0) { g_pos = 0; g_setAt = now() - 1; sched(app); return; }   // the first fact appears at once
    g_fadeAt = now();                                                         // fade out for 0.25 s, then the next one
    g_next = DBL_MAX;
}

// ---------------------------------------------------------------- the Learn tab's cards: an icon box and a title, a line, a small line
struct Card { std::string icon, title, text, small; };

float cardHeight(const Card& c, float w) {
    const float s = S() * T(), pad = 12 * s, tw = w - 2 * 14 * s - 48 * s - 12 * s;
    float h = fonts().bold->CalcTextSizeA(15 * s, FLT_MAX, tw, displayable(c.title, fonts().bold).c_str()).y;
    h += 2 * s + ImGui::GetFont()->CalcTextSizeA(13 * s, FLT_MAX, tw, displayable(c.text).c_str()).y * 1.0f;
    if (!c.small.empty()) h += 2 * s + 12 * s * 1.2f;
    return std::max(48 * s, h) + 2 * pad;
}

// draws a card at p (w × h); returns true when clicked (if clickable)
bool drawCard(const Card& c, ImVec2 p, float w, float h, bool clickable, int id) {
    const float s = S() * T(), a = 1;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool hover = false, clicked = false;
    if (clickable) {
        ImGui::SetCursorScreenPos(p);
        ImGui::PushID(id);
        clicked = ImGui::InvisibleButton("##card", ImVec2(w, h));
        hover = ImGui::IsItemHovered();
        ImGui::PopID();
        if (hover) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    cardRect(dl, p, ImVec2(p.x + w, p.y + h), corner(12 * s), hover ? look().w.accent : look().w.line, a);
    const ImVec2 ib(p.x + 14 * s, p.y + (h - 48 * s) / 2);
    faceRect(dl, ib, ImVec2(ib.x + 48 * s, ib.y + 48 * s), look().w.accentFill, a, app::WidgetState::Normal, corner(12 * s));
    std::string icon = displayable(c.icon);
    if (icon.empty() && !c.title.empty()) icon = displayable(c.title.substr(0, 1));   // 0.22 stage 6: a symbol this PC's fonts cannot draw: the title's first letter, never an empty tile
    if (!icon.empty()) {
        const float fs = 26 * s;
        const ImVec2 sz = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0, icon.c_str());
        dl->AddText(ImGui::GetFont(), fs, ImVec2(ib.x + (48 * s - sz.x) / 2, ib.y + (48 * s - sz.y) / 2), col(look().w.onacc, a), icon.c_str());
    }
    const float tx = ib.x + 48 * s + 12 * s, tw = p.x + w - 14 * s - tx;
    const float th = cardHeight(c, w) - 24 * s;
    float y = p.y + (h - th) / 2;
    const std::string title = displayable(c.title, fonts().bold), text = displayable(c.text);
    dl->AddText(fonts().bold, 15 * s, ImVec2(tx, y), col(look().w.fg, a), title.c_str(), nullptr, tw);
    y += fonts().bold->CalcTextSizeA(15 * s, FLT_MAX, tw, title.c_str()).y + 2 * s;
    dl->AddText(ImGui::GetFont(), 13 * s, ImVec2(tx, y), col(look().w.muted, a), text.c_str(), nullptr, tw);
    y += ImGui::GetFont()->CalcTextSizeA(13 * s, FLT_MAX, tw, text.c_str()).y + 2 * s;
    if (!c.small.empty()) dl->AddText(ImGui::GetFont(), 12 * s, ImVec2(tx, y), col(look().w.muted, a), displayable(c.small).c_str());
    return clicked;
}

// a grid of cards (columns at least 290 px, 12 px apart); returns the index clicked, or -1
int cardGrid(const std::vector<Card>& cards, bool clickable, int idBase) {
    const float s = S() * T(), gap = 12 * s, W = ImGui::GetContentRegionAvail().x;
    const int cols = std::max(1, int((W + gap) / (std::min(W, 290 * s) + gap)));
    const float cw = (W - gap * (cols - 1)) / cols;
    ImVec2 p = ImGui::GetCursorScreenPos();
    int clicked = -1;
    for (std::size_t r = 0; r < cards.size(); r += cols) {
        float rh = 0;
        for (std::size_t i = r; i < std::min(cards.size(), r + cols); i++) rh = std::max(rh, cardHeight(cards[i], cw));
        for (std::size_t i = r; i < std::min(cards.size(), r + cols); i++)
            if (drawCard(cards[i], ImVec2(p.x + (i - r) * (cw + gap), p.y), cw, rh, clickable, idBase + int(i))) clicked = int(i);
        p.y += rh + gap;
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y - gap));
    ImGui::Dummy(ImVec2(W, 0));
    return clicked;
}

void heading(const char* text, float px) {
    ImGui::PushFont(fonts().bold, fontPx(px));
    ImGui::TextUnformatted(displayable(text, fonts().bold).c_str());
    ImGui::PopFont();
}

void mutedLine(const char* text, float px) {
    ImGui::PushFont(nullptr, fontPx(px));
    ImGui::PushStyleColor(ImGuiCol_Text, vec(look().w.muted));
    ImGui::TextWrapped("%s", displayable(text).c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

// 📚 the Learn tab: every guide and timeline of the labs; a click opens it in its lab
void learnPane(AppState& app) {
    heading(app::kLearnTitle, 20);
    mutedLine(app::kLearnIntro, 13);
    ImGui::Dummy(ImVec2(0, 6 * S() * T()));
    int base = 0;
    for (const app::LearnSection& sec : app::kLearnSections) {
        heading(sec.heading, 16);
        std::vector<Card> cards;
        std::vector<const app::LearnCard*> refs;
        for (const app::LearnCard& c : app::kLearnCards)
            if (std::string(c.lab) == sec.lab) { cards.push_back({c.icon, c.title, c.d, ""}); refs.push_back(&c); }
        const int k = cardGrid(cards, true, base);
        base += 100;
        if (k >= 0) {                                    // the guide opens over the Learn tab, with its lab behind it
            const int lab = app::labIndex(refs[k]->lab);   // (0.55.0: from the lab's name; it was a three-way choice)
            if (lab != app.lab) { app.learnBack = app.lab; openLab(app, lab); }
            if (lab == 1) { app.atomLearn = refs[k]->guide; app.atomLearnJump = "top"; }
            else if (lab == molecule::kLabIndex) { app.molLearn = refs[k]->guide; app.molLearnJump = "top"; }
            else if (lab == blackhole::kLabIndex) { app.bhLearn = refs[k]->guide; app.bhLearnJump = "top"; }
            else { app.learn = refs[k]->guide; app.learnJump = "top"; }
        }
        ImGui::Dummy(ImVec2(0, 8 * S() * T()));
    }
}

}  // namespace

// The 💡 line under the header: a fact, the counter (3/27) and ›; a tap shows the next one
// ---- 0.15 stage 3: the new UI's 💡 card uses the same facts, in the same shuffled order ----
int factCount() { return int(app::count(app::kDyk)); }
int factPosition(AppState& app) {
    if (g_pos < 0) showNext(app);
    for (int k = 0; k < int(g_order.size()) && !factFits(g_order[g_pos], app.lab); k++) g_pos = (g_pos + 1) % int(g_order.size());   // 0.22: another lab was opened: the next fact of this one
    return g_pos;
}
std::string factText(AppState& app) { return textOf(app, g_order[factPosition(app)]); }
void factStep(AppState& app, int dir) {
    factPosition(app);
    g_forced = -1;
    for (int k = 0; k < int(g_order.size()); k++) { g_pos = (g_pos + dir + int(g_order.size())) % int(g_order.size()); if (factFits(g_order[g_pos], app.lab)) break; }   // 0.22: only this lab's facts
    g_next = DBL_MAX; g_fadeAt = -1;
}
int factLabCount(const AppState& app) { int n = 0; for (int i : g_order) if (factFits(i, app.lab)) n++; return n; }
int factLabPosition(AppState& app) { const int at = factPosition(app); int n = 0; for (int k = 0; k < at; k++) if (factFits(g_order[k], app.lab)) n++; return n; }
std::string factSourceLabel(AppState& app) { return app::kDykSrc[g_order[factPosition(app)]].label; }
std::string factSourceAbout(AppState& app) { return app::kDykSrc[g_order[factPosition(app)]].about; }
std::string factTextAt(int index) { if (index < 0 || index >= factCount()) return ""; return app::kDyk[index]; }
void factShow(AppState& app, int index) {
    factPosition(app);
    for (int i = 0; i < int(g_order.size()); i++) if (g_order[i] == index) { g_pos = i; g_forced = index; g_next = DBL_MAX; g_fadeAt = -1; return; }
}

// The Learn tab, in place of the lab
void paneWindow(AppState& app, float top) {
    if (!app.pane) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float s = S() * T();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + top));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, vp->Size.y - top));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16 * s, 12 * s));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##pane", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::PopStyleVar(2);
    panelDecor(app::PanelKind::Docked);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * s, 8 * s));
    learnPane(app);
    ImGui::PopStyleVar();
    ImGui::End();
}

// Every frame: the guide that goes back to its lab
void appTabsFrame(AppState& app) {
    // a guide opened from the Learn tab in another lab: when it closes, that lab goes away again
    if (app.learnBack >= 0 && (app.lab == 0 ? app.learn : app.lab == molecule::kLabIndex ? app.molLearn : app.lab == blackhole::kLabIndex ? app.bhLearn : app.atomLearn) == 0) { app.lab = app.learnBack; app.learnBack = -1; }   // (0.25: only this lab's guide: one left open in another lab kept you here)
}

}  // namespace ui
