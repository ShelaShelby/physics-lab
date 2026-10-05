#include "app_tabs.h"

#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <random>
#include <string>
#include <vector>

#include "app/app_data.h"
#include "atom_panel.h"
#include "fonts.h"
#include "html_view.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "look.h"
#include "style_draw.h"
#include "orbit/sky.h"

namespace ui {

namespace {

float S() { return ImGui::GetFontSize() / 16 / look().textScale; }   // one web px; sizes below are times the text size
float T() { return look().textScale; }
double now() { return ImGui::GetTime(); }
int64_t nowMs() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }

// a font size for PushFont, in web px (PushFont scales it by the text size itself)
float fontPx(float px) { return ImGui::GetStyle().FontSizeBase * px / 16; }

// ---------------------------------------------------------------- achievement pop-ups (the web's PLAch toast)
struct Toast { const app::AchDef* a; double at; };    // at: when it may start showing
std::deque<Toast> g_toasts;
double g_toastStart = -1;                           // when the front one started sliding in

void queueToast(const std::string& id) {
    for (const app::AchDef& a : app::kAchievements)
        if (id == a.id) g_toasts.push_back({&a, now() + 0.7});   // the web waits 0.7 s, so it does not cover what you just did
}

// ---------------------------------------------------------------- the 💡 facts line
std::vector<int> g_order;                           // the facts in a shuffled order (the web shuffles them on each visit)
int g_pos = -1;
double g_next = 0, g_setAt = 0, g_fadeAt = -1;      // the next change; when the current text appeared; when it began to fade out
bool g_held = false;
int g_modal = 0;                                    // a secret pop-up is open: 1 Synthwave, 2 Monochrome
bool g_first = true;                                // that pop-up is a first find
bool g_monoAsked = false;                           // the guide's "monochrome" word was clicked
bool g_openPending = false;

int utf16Length(const std::string& s) {             // the web's string length (UTF-16 units), for the time a fact stays
    int n = 0;
    for (std::size_t i = 0; i < s.size(); i++) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if ((c & 0xC0) == 0x80) continue;
        n += c >= 0xF0 ? 2 : 1;
    }
    return n;
}

bool isGift() { return g_pos >= 0 && std::string(app::kDyk[g_order[g_pos]]) == "@gift"; }

std::string textOf(const AppState& app, int i) {
    const std::string f = app::kDyk[i];
    if (f != "@gift") return f;
    return app.prefs.unlocked.count("synthwave") ? app::kDykGiftUnlocked : app::kDykGiftNew;
}

void sched(const AppState& app) {                    // waits while you hover it or a pop-up is open; Reduced motion: only on tap
    g_next = g_held || g_modal || look().reduced ? DBL_MAX : now() + (6000 + 55.0 * utf16Length(textOf(app, g_order[g_pos]))) / 1000;
}

void showNext(const AppState& app) {
    if (g_order.empty()) {
        for (int i = 0; i < int(app::count(app::kDyk)); i++) g_order.push_back(i);
        std::mt19937 rng{std::random_device{}()};
        for (int i = int(g_order.size()) - 1; i > 0; i--) std::swap(g_order[i], g_order[std::uniform_int_distribution<int>(0, i)(rng)]);
        if (const char* f = std::getenv("PL_DYK_FIRST")) {                   // for automatic checks: this fact first (26 is the 🎁)
            const auto it = std::find(g_order.begin(), g_order.end(), std::atoi(f));
            if (it != g_order.end()) std::rotate(g_order.begin(), it, it + 1);
        }
    }
    if (g_pos < 0) { g_pos = 0; g_setAt = now() - 1; sched(app); return; }   // the first fact appears at once
    g_fadeAt = now();                                                         // fade out for 0.25 s, then the next one
    g_next = DBL_MAX;
}

void openSecret(AppState& app, int which) {
    if (g_modal) return;
    const char* id = which == 1 ? "synthwave" : "monochrome";
    g_first = !app.prefs.unlocked.count(id);
    if (grantAchievement(app.prefs, id, nowMs())) queueToast(id);
    app.prefs.unlocked.insert(id);
    g_modal = which;
    g_openPending = true;                           // opened at the top level, where appTabsFrame draws it
    g_next = DBL_MAX;
}

// ---------------------------------------------------------------- cards (.achCard): an icon box and a title, a line, a small line
struct Card { std::string icon, title, text, small; bool locked = false; };

float cardHeight(const Card& c, float w) {
    const float s = S() * T(), pad = 12 * s, tw = w - 2 * 14 * s - 48 * s - 12 * s;
    float h = fonts().bold->CalcTextSizeA(15 * s, FLT_MAX, tw, displayable(c.title, fonts().bold).c_str()).y;
    h += 2 * s + ImGui::GetFont()->CalcTextSizeA(13 * s, FLT_MAX, tw, displayable(c.text).c_str()).y * 1.0f;
    if (!c.small.empty()) h += 2 * s + 12 * s * 1.2f;
    return std::max(48 * s, h) + 2 * pad;
}

// draws a card at p (w × h); returns true when clicked (if clickable)
bool drawCard(const Card& c, ImVec2 p, float w, float h, bool clickable, int id) {
    const float s = S() * T(), a = c.locked ? 0.62f : 1;
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
    cardRect(dl, p, ImVec2(p.x + w, p.y + h), corner(12 * s), hover ? look().c.accent : look().c.line, a);
    const ImVec2 ib(p.x + 14 * s, p.y + (h - 48 * s) / 2);
    faceRect(dl, ib, ImVec2(ib.x + 48 * s, ib.y + 48 * s), c.locked ? look().c.line : look().c.accentFill, a, app::WidgetState::Normal, corner(12 * s));
    const std::string icon = displayable(c.icon);
    if (!icon.empty()) {
        const float fs = 26 * s;
        const ImVec2 sz = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0, icon.c_str());
        dl->AddText(ImGui::GetFont(), fs, ImVec2(ib.x + (48 * s - sz.x) / 2, ib.y + (48 * s - sz.y) / 2), col(c.locked ? look().c.muted : look().c.onacc, a), icon.c_str());
    }
    const float tx = ib.x + 48 * s + 12 * s, tw = p.x + w - 14 * s - tx;
    const float th = cardHeight(c, w) - 24 * s;
    float y = p.y + (h - th) / 2;
    const std::string title = displayable(c.title, fonts().bold), text = displayable(c.text);
    dl->AddText(fonts().bold, 15 * s, ImVec2(tx, y), col(look().c.fg, a), title.c_str(), nullptr, tw);
    y += fonts().bold->CalcTextSizeA(15 * s, FLT_MAX, tw, title.c_str()).y + 2 * s;
    dl->AddText(ImGui::GetFont(), 13 * s, ImVec2(tx, y), col(look().c.muted, a), text.c_str(), nullptr, tw);
    y += ImGui::GetFont()->CalcTextSizeA(13 * s, FLT_MAX, tw, text.c_str()).y + 2 * s;
    if (!c.small.empty()) dl->AddText(ImGui::GetFont(), 12 * s, ImVec2(tx, y), col(look().c.muted, a), displayable(c.small).c_str());
    return clicked;
}

// a grid of cards (.achGrid: columns at least 290 px, 12 px apart); returns the index clicked, or -1
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
    ImGui::PushStyleColor(ImGuiCol_Text, vec(look().c.muted));
    ImGui::TextWrapped("%s", displayable(text).c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

// 🏆 the Achievements tab: every achievement of the native app, locked until earned
void achievementsPane(AppState& app) {
    const float s = S() * T();
    int got = 0, total = 0;
    for (const app::AchDef& a : app::kAchievements) if (app::achievementInApp(a.id)) { total++; got += app.prefs.achievements.count(a.id) ? 1 : 0; }
    heading(app::kAchTitle, 20);
    char buf[64];
    std::snprintf(buf, sizeof buf, app::kAchCount, got, total);
    mutedLine(buf, 13);
    // .achBar: an 8 px bar in the accent colour
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float W = ImGui::GetContentRegionAvail().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + W, p.y + 8 * s), col(look().c.line), corner(99));
    if (got) dl->AddRectFilled(p, ImVec2(p.x + W * std::round(100.0f * got / total) / 100, p.y + 8 * s), col(look().c.accent), corner(99));
    ImGui::Dummy(ImVec2(W, 8 * s));
    ImGui::Dummy(ImVec2(0, 4 * s));
    std::vector<Card> cards;
    for (const app::AchDef& a : app::kAchievements) {
        if (!app::achievementInApp(a.id)) continue;
        const auto it = app.prefs.achievements.find(a.id);
        Card c;
        c.locked = it == app.prefs.achievements.end();
        c.icon = c.locked ? app::kAchLockedIcon : a.ic;
        c.title = a.name;
        c.text = c.locked ? a.how : a.done;
        if (!c.locked) c.small = it->second ? std::string(app::kAchUnlockedOn) + app::achievementDate(it->second) : app::kAchUnlockedEarlier;
        cards.push_back(c);
    }
    cardGrid(cards, false, 0);
}

// 📚 the Learn tab: every guide and timeline of the native labs; a click opens it in its lab
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
            if (std::string(c.lab) == sec.lab) { cards.push_back({c.icon, c.title, c.d, "", false}); refs.push_back(&c); }
        const int k = cardGrid(cards, true, base);
        base += 100;
        if (k >= 0) {                                    // the guide opens over the Learn tab, with its lab behind it
            const int lab = std::string(refs[k]->lab) == "Atom Lab" ? 1 : 0;
            if (lab != app.lab) { app.learnBack = app.lab; openLab(app, lab); }
            if (lab == 1) { app.atomLearn = refs[k]->guide; app.atomLearnJump = "top"; }
            else { app.learn = refs[k]->guide; app.learnJump = "top"; }
        }
        ImGui::Dummy(ImVec2(0, 8 * S() * T()));
    }
}

}  // namespace

void grantAchievement(AppState& app, const char* id) {
    if (app::grantAchievement(app.prefs, id, nowMs())) queueToast(id);
}

void secretMono() { g_monoAsked = true; }

// The 💡 line under the header: a fact, the counter (3/27) and ›; a tap shows the next one
void factsLine(AppState& app) {
    if (g_pos < 0) { showNext(app); setHtmlAction([](const std::string& a) { if (a == "monoKey") secretMono(); }); }
    const double t = now();
    if (g_fadeAt >= 0 && t - g_fadeAt >= 0.25) {                 // faded out: the next fact
        g_fadeAt = -1;
        g_pos = (g_pos + 1) % int(g_order.size());
        g_setAt = t;
        sched(app);
    }
    if (t >= g_next) showNext(app);
    const float s = S() * T(), W = ImGui::GetContentRegionAvail().x, padX = 12 * s, padY = 8 * s;
    const bool gift = isGift();
    const std::string fact = displayable(textOf(app, g_order[g_pos]));
    const std::string lead = std::string(app::kDykLead) + " ";
    const std::string count = std::to_string(g_pos + 1) + "/" + std::to_string(g_order.size());
    // the text column: between the 💡 and the counter
    const std::string icon = displayable(app::kDykIcon);
    const float iconW = icon.empty() ? 0 : ImGui::GetFont()->CalcTextSizeA(18 * s, FLT_MAX, 0, icon.c_str()).x + 10 * s;
    const float countW = ImGui::GetFont()->CalcTextSizeA(13 * s, FLT_MAX, 0, "00/00").x + 6 * s + ImGui::GetFont()->CalcTextSizeA(18 * s, FLT_MAX, 0, "›").x + 10 * s;
    const float tx0 = padX + iconW, tw = W - tx0 - countW - padX;
    // lay the words out: "Did you know?" in the accent colour, then the fact (the 🎁 line alone, accent and bold)
    struct Word { std::string w; bool lead; float x, y; };
    std::vector<Word> words;
    const float fs = 13.5f * s, lineH = fs * 1.4f;
    float x = 0, y = 0;
    auto add = [&](const std::string& text, bool isLead, ImFont* f) {
        std::size_t a = 0;
        while (a < text.size()) {
            std::size_t b = text.find(' ', a);
            if (b == std::string::npos) b = text.size();
            const std::string w = text.substr(a, b - a);
            a = b + 1;
            if (w.empty()) continue;
            const float ww = f->CalcTextSizeA(fs, FLT_MAX, 0, w.c_str()).x, sp = f->CalcTextSizeA(fs, FLT_MAX, 0, " ").x;
            if (x > 0 && x + ww > tw) { x = 0; y += lineH; }
            words.push_back({w, isLead, x, y});
            x += ww + sp;
        }
    };
    if (!gift) add(lead, true, fonts().bold);
    add(fact, false, gift ? fonts().bold : ImGui::GetFont());
    const float textH = y + lineH, h = std::max(46 * s, textH + 2 * padY);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const bool clicked = ImGui::InvisibleButton("##dyk", ImVec2(W, h));
    const bool hover = ImGui::IsItemHovered();
    if (hover) { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); ImGui::SetTooltip("%s", app::kDykTip); }
    if (hover != g_held) { g_held = hover; if (g_held) g_next = DBL_MAX; else if (g_fadeAt < 0) sched(app); }
    if (clicked) { if (gift) openSecret(app, 1); else if (g_fadeAt < 0) showNext(app); }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    cardRect(dl, p, ImVec2(p.x + W, p.y + h), corner(12 * s), g_held ? look().c.accent : look().c.line);
    if (!icon.empty()) dl->AddText(ImGui::GetFont(), 18 * s, ImVec2(p.x + padX, p.y + (h - 18 * s) / 2), col(look().c.fg), icon.c_str());
    // the text fades out and in over 0.25 s (Reduced motion: no fading)
    float alpha = 1;
    if (!look().reduced) {
        if (g_fadeAt >= 0) alpha = float(std::max(0.0, 1 - (t - g_fadeAt) / 0.25));
        else alpha = float(std::min(1.0, (t - g_setAt) / 0.25));
    }
    const float ty = p.y + (h - textH) / 2;
    for (const Word& w : words) {
        ImFont* f = w.lead || gift ? fonts().bold : ImGui::GetFont();
        dl->AddText(f, fs, ImVec2(p.x + tx0 + w.x, ty + w.y + (lineH - fs) / 2), col(w.lead || gift ? look().c.accent : look().c.fg, alpha), w.w.c_str());
    }
    const float cw = ImGui::GetFont()->CalcTextSizeA(13 * s, FLT_MAX, 0, count.c_str()).x, gw = ImGui::GetFont()->CalcTextSizeA(18 * s, FLT_MAX, 0, "›").x;
    const float rx = p.x + W - padX - gw;
    dl->AddText(ImGui::GetFont(), 18 * s, ImVec2(rx, p.y + (h - 18 * s * 1.15f) / 2), col(look().c.muted), "›");
    dl->AddText(ImGui::GetFont(), 13 * s, ImVec2(rx - 6 * s - cw, p.y + (h - 13 * s) / 2), col(look().c.muted), count.c_str());
}

// The tabs (the web's .seg): 🔬 Simulations, 📚 Learn, 🏆 Achievements
void tabsBar(AppState& app) {
    const float s = S() * T();
    const char* labels[3] = {app::kTabSims, app::kTabLearn, app::kTabAch};
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    float x = p0.x;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushFont(nullptr, fontPx(14));
    const float h = ImGui::GetFontSize() + 14 * s;
    float total = 0;
    for (const char* l : labels) total += ImGui::CalcTextSize(displayable(l).c_str()).x + 28 * s;
    cardRect(dl, p0, ImVec2(p0.x + total, p0.y + h), corner(10 * s), look().c.line, 1, false);
    for (int i = 0; i < 3; i++) {
        const std::string l = displayable(labels[i]);
        const float w = ImGui::CalcTextSize(l.c_str()).x + 28 * s;
        ImGui::SetCursorScreenPos(ImVec2(x, p0.y));
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##tab", ImVec2(w, h))) app.pane = i;
        const bool hover = ImGui::IsItemHovered();
        ImGui::PopID();
        const bool on = app.pane == i;
        if (on) faceRect(dl, ImVec2(x, p0.y), ImVec2(x + w, p0.y + h), look().c.accentFill, 1, app::WidgetState::Normal, corner(10 * s),
                         i == 0 ? ImDrawFlags_RoundCornersLeft : i == 2 ? ImDrawFlags_RoundCornersRight : ImDrawFlags_RoundCornersNone);
        else if (hover) dl->AddRectFilled(ImVec2(x, p0.y), ImVec2(x + w, p0.y + h), col(look().c.line, 0.6f));
        dl->AddText(ImVec2(x + 14 * s, p0.y + 7 * s), col(on ? look().c.onacc : look().c.fg), l.c_str());
        x += w;
    }
    ImGui::PopFont();
    dl->AddRect(p0, ImVec2(p0.x + total, p0.y + h), col(look().c.line), corner(10 * s));
    ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + h));
    ImGui::Dummy(ImVec2(total, 0));
}

// The Learn or Achievements tab, in place of the lab (the web hides the picture and the controls)
void paneWindow(AppState& app, float top) {
    if (!app.pane) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float s = S() * T();
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + top));
    ImGui::SetNextWindowSize(ImVec2(vp->Size.x, vp->Size.y - top));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16 * s, 12 * s));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    pushDockedBg();
    ImGui::Begin("##pane", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_AlwaysVerticalScrollbar);
    ImGui::PopStyleVar(2);
    popDockedBg();
    panelDecor(app::PanelKind::Docked);
    if (look().synthGlow) drawSynthGlow(ImGui::GetWindowDrawList(), vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8 * s, 8 * s));
    if (app.pane == 1) learnPane(app);
    else achievementsPane(app);
    ImGui::PopStyleVar();
    ImGui::End();
}

// Every frame: the achievement triggers, the guide that goes back to its lab, the secret pop-ups and the achievement pop-ups
void appTabsFrame(AppState& app) {
    // Galactic Center: Sagittarius A* picked in the Orbit Lab (the web checks it in the lab's tick)
    static const int sgra = orbit::farIndexOf("sgra");
    if (app.lab == 0 && !app.pane && app.view.sel.kind == orbit::Selection::Far && app.view.sel.moon == sgra) grantAchievement(app, "galactic");
    // Half-Life: a nucleus decayed on its own
    static int seen = 0;
    if (app.atom.s.decays > seen) grantAchievement(app, "decay");
    seen = app.atom.s.decays;
    // a guide opened from the Learn tab in another lab: when it closes, that lab goes away again (the web's lrn.after)
    if (app.learnBack >= 0 && app.learn == 0 && app.atomLearn == 0) { app.lab = app.learnBack; app.learnBack = -1; }

    // the secret pop-ups: 🎁 Synthwave (from the facts line) and Monochrome (the guide's word)
    if (g_monoAsked) { g_monoAsked = false; openSecret(app, 2); }
    if (g_openPending) { g_openPending = false; ImGui::OpenPopup("##secret"); }
    if (g_modal && beginGift("##secret")) {
        const bool synth = g_modal == 1;
        const std::string id = giftPage((synth ? app::kGiftSynth : app::kGiftMono)[g_first ? 0 : 1]);
        bool close = id == (synth ? "giftX" : "monoX") || ImGui::IsKeyPressed(ImGuiKey_Escape);
        if (id == (synth ? "giftUse" : "monoUse")) { app.prefs.theme = synth ? "synthwave" : "monochrome"; close = true; }
        if (close) { g_modal = 0; ImGui::CloseCurrentPopup(); if (g_pos >= 0 && g_fadeAt < 0) sched(app); }
        ImGui::EndPopup();
    } else if (g_modal && !ImGui::IsPopupOpen("##secret")) { g_modal = 0; if (g_pos >= 0) sched(app); }

    // the achievement pop-ups, one at a time at the bottom right: slide in, stay 5.2 s, fade out (Reduced motion: fade only)
    if (g_toasts.empty() || now() < g_toasts.front().at) return;
    if (g_toastStart < 0) g_toastStart = now();
    const double age = now() - g_toastStart;
    if (age > 5.2 + 0.4) { g_toasts.pop_front(); g_toastStart = -1; return; }
    const bool rm = look().reduced;
    const double in = rm ? 0.2 : 0.35;
    float a = float(std::min(1.0, age / in)), dy = 0;
    if (!rm) { const double u = std::min(1.0, age / in); dy = float(24 * (1 - (1 - std::pow(1 - u, 3)))); }   // ease out
    if (age > 5.2) { a = float(std::max(0.0, 1 - (age - 5.2) / 0.4)); dy = rm ? 0 : float(24 * (age - 5.2) / 0.4); }
    const float s = S() * T();
    const app::AchDef& A = *g_toasts.front().a;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float w = std::min(320 * s, vp->Size.x - 32 * s);
    ImGui::SetNextWindowPos(ImVec2(vp->Pos.x + vp->Size.x - 16 * s, vp->Pos.y + vp->Size.y - 16 * s + dy), ImGuiCond_Always, ImVec2(1, 1));
    ImGui::SetNextWindowSize(ImVec2(w, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, a);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, corner(12 * s));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10 * s, 10 * s));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, vec(look().c.card));
    ImGui::PushStyleColor(ImGuiCol_Border, vec(look().c.line));
    ImGui::Begin("##achToast", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings |
                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
    panelDecor(app::PanelKind::Floating);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float tx = p.x + 42 * s + 10 * s, tw = p.x + w - 24 * s - tx;
    const std::string lead = [&] { std::string u = app::kAchPopLead; for (char& c : u) c = char(std::toupper(static_cast<unsigned char>(c))); return u; }();
    ImGui::SetCursorScreenPos(ImVec2(tx, p.y));
    ImGui::PushTextWrapPos(tx + tw - ImGui::GetWindowPos().x);
    ImGui::PushFont(nullptr, fontPx(11));
    ImGui::PushStyleColor(ImGuiCol_Text, vec(look().c.muted));
    ImGui::TextUnformatted(lead.c_str());
    ImGui::PopFont();
    ImGui::PushFont(fonts().bold, fontPx(14));
    ImGui::PushStyleColor(ImGuiCol_Text, vec(look().c.fg));
    ImGui::SetCursorPosX(tx - ImGui::GetWindowPos().x);
    ImGui::TextUnformatted(displayable(A.name, fonts().bold).c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::PushFont(nullptr, fontPx(13));
    ImGui::SetCursorPosX(tx - ImGui::GetWindowPos().x);
    ImGui::TextUnformatted(displayable(A.done).c_str());
    ImGui::PopFont();
    ImGui::PopStyleColor();
    ImGui::PopTextWrapPos();
    const float bottom = ImGui::GetCursorScreenPos().y;
    const float iy = std::max(p.y, (p.y + bottom - 42 * s) / 2 - 2 * s);
    faceRect(dl, ImVec2(p.x, iy), ImVec2(p.x + 42 * s, iy + 42 * s), look().c.accentFill, a, app::WidgetState::Normal, corner(10 * s));
    const std::string icon = displayable(A.ic);
    if (!icon.empty()) {
        const ImVec2 sz = ImGui::GetFont()->CalcTextSizeA(22 * s, FLT_MAX, 0, icon.c_str());
        dl->AddText(ImGui::GetFont(), 22 * s, ImVec2(p.x + (42 * s - sz.x) / 2, iy + (42 * s - sz.y) / 2), col(look().c.onacc, a), icon.c_str());
    }
    if (bottom < iy + 42 * s) { ImGui::SetCursorScreenPos(ImVec2(p.x, iy + 42 * s)); ImGui::Dummy(ImVec2(0, 0)); }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
}

}  // namespace ui
