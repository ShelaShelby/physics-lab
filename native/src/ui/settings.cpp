#include "settings.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <map>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <regex>
#include <string>

#include "app/app_data.h"
#include "fonts.h"
#include "html_view.h"
#include "imgui.h"
#include "app_pages.h"
#include "look.h"
#include "orbit/format.h"
#include "orbit/units.h"
#include "rich_text.h"
#include "ui/journey_dates.h"
#include "ui/orbit_panel.h"

namespace ui {

namespace {

// the settings file, in the user's own app-data folder (Windows: %APPDATA%\PhysicsLab\PhysicsLab\)
std::string settingsPath() {
    if (const char* t = std::getenv("PL_SETTINGS")) return t;   // for automatic checks: another file
    char* dir = SDL_GetPrefPath("PhysicsLab", "PhysicsLab");
    if (!dir) return "";
    std::string p = std::string(dir) + "settings.txt";
    SDL_free(dir);
    return p;
}

std::string g_written;            // what the file holds now
double g_changedAt = -1;          // when the settings last changed (s), -1 = nothing waiting to be written

// Atom Lab back to how it starts: every setting at its default and a fresh atom (the web's load(lab, fresh));
// the More / Less info choice and the Temperature slider are kept, as on the web
void putBackAtomDefaults(AppState& app) {
    app.atom.p = atom::AtomParams{};
    app.atom.restart();
    app.atomLearn = 0;
}

// Orbit Lab back to how it starts: today's Solar System, the starting view and every setting at its default
void putBackDefaults(AppState& app) {
    journeyStop(app);
    app.set = orbit::ViewSettings{};
    app.view = orbit::OrbitView{};
    app.dateNote.clear();
    app.learnJump.clear();
    resetToToday(app);
}

// ---- each lab's settings, as text for the file ----
std::string num(double v) { char b[40]; std::snprintf(b, sizeof b, "%.17g", v); return b; }
const char* const kSky[3] = {"soft", "full", "off"};
const char* const kCons[3] = {"off", "lines", "names"};

void labsToPrefs(const AppState& app, app::Prefs& p) {
    const orbit::ViewSettings& v = app.set;
    p.orbit = {{"view3D", v.view3D ? "1" : "0"}, {"zoom", num(v.zoom)}, {"tspeed", num(v.tspeed)}, {"trueScale", v.trueScale ? "1" : "0"},
               {"fullTrails", v.fullTrails ? "1" : "0"}, {"moons", v.moons ? "1" : "0"}, {"dwarfs", v.dwarfs ? "1" : "0"}, {"comets", v.comets ? "1" : "0"},
               {"belts", v.belts ? "1" : "0"}, {"sky", kSky[int(v.sky)]}, {"cons", kCons[int(v.cons)]}, {"far", v.far ? "1" : "0"}};
    const atom::AtomParams& a = app.atom.p;
    p.atom = {{"scale", a.scale}, {"zoom", num(a.zoom)}, {"qstyle", a.qstyle}, {"detail", a.detail}, {"style", a.style}, {"Z", std::to_string(a.Z)},
              {"N", std::to_string(a.N)}, {"view", a.view}, {"ostyle", a.ostyle}, {"orb", a.orb}, {"pAx", a.pAx}, {"dAx", a.dAx}, {"fAx", a.fAx},
              {"cutAxis", a.cutAxis}, {"cut", num(a.cut)}, {"n", std::to_string(a.n)}, {"speed", std::to_string(a.speed)}};
    p.atomMore = app.atom.moreStored;
    p.lab = app.lab == 1 ? "Atom Lab" : "Orbit Lab";
    p.units = fmt::imperial() ? "imperial" : "metric";
}

// a saved value, checked against what the control still allows (the web's restored())
bool readNum(const std::map<std::string, std::string>& m, const char* k, double lo, double hi, double& out) {
    const auto it = m.find(k);
    if (it == m.end()) return false;
    char* end = nullptr;
    const double v = std::strtod(it->second.c_str(), &end);
    if (!end || *end || it->second.empty() || !std::isfinite(v)) return false;
    out = std::max(lo, std::min(hi, v));
    return true;
}
void readBool(const std::map<std::string, std::string>& m, const char* k, bool& out) {
    const auto it = m.find(k);
    if (it != m.end() && (it->second == "1" || it->second == "0")) out = it->second == "1";
}

void prefsToLabs(AppState& app, const app::Prefs& p) {
    orbit::ViewSettings& v = app.set;
    double d;
    readBool(p.orbit, "view3D", v.view3D);
    if (readNum(p.orbit, "zoom", -9, 5.5, d)) v.zoom = d;
    if (readNum(p.orbit, "tspeed", units::kTspeedMin, units::kTspeedMax, d)) v.tspeed = d;
    readBool(p.orbit, "trueScale", v.trueScale); readBool(p.orbit, "fullTrails", v.fullTrails); readBool(p.orbit, "moons", v.moons);
    readBool(p.orbit, "dwarfs", v.dwarfs); readBool(p.orbit, "comets", v.comets); readBool(p.orbit, "belts", v.belts); readBool(p.orbit, "far", v.far);
    for (int i = 0; i < 3; i++) {
        if (p.orbit.count("sky") && p.orbit.at("sky") == kSky[i]) v.sky = orbit::SkyMode(i);
        if (p.orbit.count("cons") && p.orbit.at("cons") == kCons[i]) v.cons = orbit::ConsMode(i);
    }
    atom::AtomParams a;
    for (const char* k : {"scale", "qstyle", "detail", "style", "view", "ostyle", "orb", "pAx", "dAx", "fAx", "cutAxis"}) {
        const auto it = p.atom.find(k);
        if (it == p.atom.end() || !atom::AtomLab::validChoice(k, it->second)) continue;
        std::string& f = std::string(k) == "scale" ? a.scale : std::string(k) == "qstyle" ? a.qstyle : std::string(k) == "detail" ? a.detail
                       : std::string(k) == "style" ? a.style : std::string(k) == "view" ? a.view : std::string(k) == "ostyle" ? a.ostyle
                       : std::string(k) == "orb" ? a.orb : std::string(k) == "pAx" ? a.pAx : std::string(k) == "dAx" ? a.dAx
                       : std::string(k) == "fAx" ? a.fAx : a.cutAxis;
        f = it->second;
    }
    if (readNum(p.atom, "zoom", 0, 7, d)) a.zoom = d;
    if (readNum(p.atom, "Z", 1, 118, d)) { a.Z = int(d); a.N = atom::defaultN(a.Z); }
    if (readNum(p.atom, "N", atom::nRange(a.Z).first, atom::nRange(a.Z).second, d)) a.N = int(d);
    if (readNum(p.atom, "cut", -1, 1, d)) a.cut = d;
    if (readNum(p.atom, "n", 1, 4, d)) a.n = int(d);
    if (readNum(p.atom, "speed", 1, 30, d)) a.speed = int(d);
    app.atom.p = a;
    app.atom.moreStored = p.atomMore;
    app.atom.restart();
    app.lab = p.lab == "Atom Lab" ? 1 : 0;
    fmt::setImperial(p.units == "imperial");
}

// a "setOpt" tile: a big Aa (in the given font and size) over a small name; returns true when clicked
bool optionTile(const char* id, const std::string& name, bool on, ImFont* font, float aaSize, float width, const std::string& sub = "") {
    ImGui::PushID(id);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float h = sub.empty() && font ? 62 * ImGui::GetFontSize() / 16 : 46 * ImGui::GetFontSize() / 16;
    if (on) pushOn();
    const bool clicked = ImGui::Button("##opt", ImVec2(width, h));
    if (on) popOn();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 fg = on ? col(look().c.onacc) : col(look().c.fg), mu = on ? col(look().c.onacc) : col(look().c.muted);
    if (font) {
        const ImVec2 a = font->CalcTextSizeA(aaSize, FLT_MAX, 0, "Aa");
        dl->AddText(font, aaSize, ImVec2(p.x + (width - a.x) / 2, p.y + 6 * ImGui::GetFontSize() / 16), fg, "Aa");
        const std::string n = displayable(name);
        const float ns = ImGui::GetFontSize() * 0.8f;
        const ImVec2 t = ImGui::GetFont()->CalcTextSizeA(ns, FLT_MAX, 0, n.c_str());
        dl->AddText(ImGui::GetFont(), ns, ImVec2(p.x + (width - t.x) / 2, p.y + h - t.y - 6), mu, n.c_str());
    } else {
        const float fs = ImGui::GetFontSize();
        const ImVec2 t = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0, name.c_str());
        dl->AddText(fonts().bold, fs, ImVec2(p.x + 10, p.y + 6), fg, name.c_str());
        dl->AddText(ImGui::GetFont(), fs * 0.8f, ImVec2(p.x + 10, p.y + 8 + t.y), mu, sub.c_str());
    }
    ImGui::PopID();
    return clicked;
}

// the reset pop-ups: 0 none, 1 "Reset your progress?" (or "Nothing to reset"), 2 "Progress reset"
int g_resetStage = 0, g_resetThemes = 0, g_resetAch = 0;
bool g_resetDone = false;
std::string g_unlockMsg;

// the web's .setDone line: bold text on a rounded box in the line colour
void doneLine(const std::string& text) {
    const float s = ImGui::GetFontSize() / 16, padX = 10 * s, padY = 8 * s, w = ImGui::GetContentRegionAvail().x;
    const std::string t = displayable(text, fonts().bold);
    const ImVec2 size = fonts().bold->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, w - 2 * padX, t.c_str());
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + size.y + 2 * padY), col(look().c.line), corner(10 * s));
    dl->AddText(fonts().bold, ImGui::GetFontSize(), ImVec2(p.x + padX, p.y + padY), col(look().c.fg), t.c_str(), nullptr, w - 2 * padX);
    ImGui::Dummy(ImVec2(w, size.y + 2 * padY));
}

}  // namespace

void loadSettings(AppState& app) {
    const std::string path = settingsPath();
    std::string text;
    if (FILE* f = path.empty() ? nullptr : std::fopen(path.c_str(), "rb")) {
        char buf[4096];
        std::size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
        std::fclose(f);
    }
    app.prefs = app::loadPrefs(text);
    prefsToLabs(app, app.prefs);
    labsToPrefs(app, app.prefs);
    g_written = app::savePrefs(app.prefs);
}

void saveSettingsWhenChanged(AppState& app, double nowS) {
    labsToPrefs(app, app.prefs);
    const std::string text = app::savePrefs(app.prefs);
    if (text == g_written) { g_changedAt = -1; return; }
    if (g_changedAt < 0) g_changedAt = nowS;
    if (nowS - g_changedAt < 0.25) return;                    // written a moment after the last change, like the web's remember()
    const std::string path = settingsPath();
    if (path.empty()) return;
    if (FILE* f = std::fopen(path.c_str(), "wb")) {
        std::fwrite(text.data(), 1, text.size(), f);
        std::fclose(f);
        g_written = text;
        g_changedAt = -1;
    }
}

void setMotion(AppState& app, const std::string& k) {        // the web's setMotion: turning reduced motion on pauses the lab
    const bool was = look().reduced;
    app.prefs.motion = k;
    const bool now = k == "on" || (k == "auto" && systemReducedMotion());
    if (now && !was) { app.paused = true; if (app.lab == 1) app.atom.s.spin = false; }
}

void settingsWindow(AppState& app) {
    if (!app.showSettings) return;
    app::Prefs& P = app.prefs;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(std::min(30 * ImGui::GetFontSize(), vp->WorkSize.x * 0.95f), std::min(40 * ImGui::GetFontSize(), vp->WorkSize.y * 0.92f)), ImGuiCond_Appearing);   // never bigger than the window (small window, big text)
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::Begin((std::string(app::kSettingsTitle) + "###settings").c_str(), &app.showSettings, ImGuiWindowFlags_NoCollapse)) {
        const float avail = ImGui::GetContentRegionAvail().x, gap = ImGui::GetStyle().ItemSpacing.x;
        auto tiles = [&](int n) { return (avail - gap * (n - 1)) / n; };
        // 🔠 Text size
        richHeading(displayable(app::kSetH[0]));
        for (int i = 0; i < 4; i++) {
            if (i) ImGui::SameLine();
            const app::SizeOpt& o = app::kTextSizes[i];
            if (optionTile(o.key, o.name, P.textSize == o.key, fonts().regular, float(o.factor * 17) * ImGui::GetFontSize() / 16 / look().textScale, tiles(4)))
                P.textSize = o.key;
        }
        richText(std::string("<p class=\"lrnP lrnMuted\">") + app::kSizeNote + "</p>");
        richText(std::string("<p class=\"lrnP\">") + app::kSizeSample + "</p>");
        // 🔤 Font
        richHeading(displayable(app::kSetH[1]));
        for (int i = 0; i < 5; i++) {
            if (i) ImGui::SameLine();
            const app::FontOpt& o = app::kFonts[i];
            const std::string fam = std::string(o.key) == "auto" ? (look().c.id == "paper" ? "serif" : look().c.id == "monochrome" ? "mono" : "auto") : o.key;
            if (optionTile(o.key, o.name, P.font == o.key, fontFamily(fam).regular, 17 * ImGui::GetFontSize() / 16, tiles(5))) P.font = o.key;
        }
        // 📏 Units
        richHeading(displayable(app::kSetH[2]));
        for (int i = 0; i < 2; i++) {
            if (i) ImGui::SameLine();
            const bool on = (i == 1) == fmt::imperial();
            if (optionTile(i ? "imperial" : "metric", app::kUnitOpts[i].name, on, nullptr, 0, tiles(2), app::kUnitOpts[i].sub)) fmt::setImperial(i == 1);
        }
        richText(std::string("<p class=\"lrnP lrnMuted\">") + app::kUnitsNote + "</p>");
        // 🌀 Motion
        richHeading(displayable(app::kSetH[3]));
        const char* motions[3] = {"auto", "on", "off"};
        for (int i = 0; i < 3; i++) {
            if (i) ImGui::SameLine();
            if (optionTile(motions[i], app::kMotionOpts[i].name, P.motion == motions[i], nullptr, 0, tiles(3), app::kMotionOpts[i].sub)) setMotion(app, motions[i]);
        }
        richText(std::string("<p class=\"lrnP lrnMuted\">") + app::kMotionNote + "</p>");
        // 🔒 Progress
        richHeading(displayable(app::kSetH[4]));
        if (optionTile("reset", app::kResetRow.name, false, nullptr, 0, avail, app::kResetRow.sub)) {
            g_resetThemes = app::unlockedThemeCount(P); g_resetAch = app::earnedAchievementCount(P); g_resetStage = 1;
            ImGui::OpenPopup("##reset");
        }
        if (g_resetDone) doneLine(app::kResetDoneLine);
        // 🛠 Developer
        richHeading(displayable(app::kSetH[5]));
        if (optionTile("unlock", app::kUnlockRow.name, false, nullptr, 0, avail, app::kUnlockRow.sub)) {
            const auto [t, a] = app::unlockEverything(P);
            g_unlockMsg = app::kUnlockMsg[std::min(t, 2)][std::min(a, 5)];
        }
        if (!g_unlockMsg.empty()) doneLine(g_unlockMsg);
        // the reset pop-ups (first the warning, then a clear "it worked")
        if (beginGift("##reset")) {
            const int t = std::min(g_resetThemes, 2), a = std::min(g_resetAch, 5);
            const std::string html = g_resetStage == 2 ? app::kResetDone[t][a] : (t + a == 0 ? app::kResetNothing : app::kResetAsk[t][a]);
            const std::string id = giftPage(html);
            if (id == "rstYes") { app::resetProgress(P); g_resetDone = true; g_unlockMsg.clear(); g_resetStage = 2; }
            else if (id == "rstNo" || id == "rstOk" || ImGui::IsKeyPressed(ImGuiKey_Escape)) { g_resetStage = 0; ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
    }
    ImGui::End();
    if (!app.showSettings) { g_resetDone = false; g_unlockMsg.clear(); }   // the Done lines belong to this visit of the page
}

// 🎨 Theme: the web's theme menu (a swatch, the name and its small line, ✓ on the one in use; the two
// secret themes are "???" with a hint until they are unlocked)
void themeButton(AppState& app) {
    if (pillButton(app::kHdrTheme.label, app::kHdrTheme.tip)) ImGui::OpenPopup("##themes");
    if (!ImGui::BeginPopup("##themes")) return;
    const float s = ImGui::GetFontSize() / 16, h = 40 * s;
    float w = 240 * s;                                   // wide enough for the longest line
    for (int i = 0; i < 2; i++) w = std::max(w, 70 * s + ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize() * 0.8f, FLT_MAX, 0, displayable(app::kThemeLocked[i]).c_str()).x);
    for (const app::ThemeDef& t : app::kThemes) w = std::max(w, 70 * s + ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize() * 0.8f, FLT_MAX, 0, displayable(t.sub).c_str()).x);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    int lockedIdx = 0;
    for (const app::ThemeDef& t : app::kThemes) {
        const bool locked = t.secret && !app.prefs.unlocked.count(t.id), on = app.prefs.theme == t.id;
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::PushID(t.id);
        if (locked) ImGui::BeginDisabled();
        if (ImGui::Selectable("##t", on, 0, ImVec2(w, h)) && !locked) { app.prefs.theme = t.id; ImGui::CloseCurrentPopup(); }
        if (locked) ImGui::EndDisabled();
        ImGui::PopID();
        // the swatch: the theme's background with its accent; Auto is half light, half dark
        const ImVec2 a(p.x + 4 * s, p.y + 6 * s), b(a.x + 28 * s, a.y + 28 * s);
        if (locked) {
            dl->AddRectFilled(a, b, col(look().c.line), corner(7 * s));
            const ImVec2 q = fonts().bold->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, 0, "?");
            dl->AddText(fonts().bold, ImGui::GetFontSize(), ImVec2((a.x + b.x - q.x) / 2, (a.y + b.y - q.y) / 2), col(look().c.muted), "?");
        } else if (*t.swatchBg) {
            dl->AddRectFilled(a, b, col(app::hexRgb(t.swatchBg)), corner(7 * s));
            dl->AddCircleFilled(ImVec2(b.x - 8 * s, b.y - 8 * s), 5 * s, col(app::hexRgb(t.swatchAccent)));
        } else {
            dl->AddTriangleFilled(a, ImVec2(b.x, a.y), ImVec2(a.x, b.y), col(app::hexRgb("#f7f7f5")));
            dl->AddTriangleFilled(ImVec2(b.x, a.y), b, ImVec2(a.x, b.y), col(app::hexRgb("#151517")));
        }
        dl->AddRect(a, b, col(look().c.line), corner(7 * s));
        const std::string name = locked ? "???" : t.name, sub = locked ? app::kThemeLocked[lockedIdx] : t.sub;
        dl->AddText(fonts().bold, ImGui::GetFontSize(), ImVec2(b.x + 10 * s, p.y + 4 * s), col(look().c.fg, locked ? 0.6f : 1), displayable(name, fonts().bold).c_str());
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.8f, ImVec2(b.x + 10 * s, p.y + 22 * s), col(look().c.muted), displayable(sub).c_str());
        if (on) dl->AddText(fonts().bold, ImGui::GetFontSize(), ImVec2(p.x + w - 18 * s, p.y + 12 * s), col(look().c.accent), "✓");
        if (t.secret) lockedIdx++;
    }
    ImGui::EndPopup();
}

void defaultsButton(AppState& app) {
    if (app.lab == 1) {
        if (ImGui::Button("Defaults")) ImGui::OpenPopup("Put Atom Lab back?");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Put this lab's settings back to how they started");
        ImGui::SetNextWindowSize(ImVec2(24 * ImGui::GetFontSize(), 0));
        if (ImGui::BeginPopupModal("Put Atom Lab back?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::TextWrapped("This puts every Atom Lab setting back to how it started: hydrogen-1, the Orbit view, level n = 1, "
                               "zoom, scale, quark style and the other choices, and starts a fresh atom (the decay history starts "
                               "again). Your units and More / Less info choices stay.");
            ImGui::Spacing();
            if (ImGui::Button("Yes, put them back")) { putBackAtomDefaults(app); ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }
        return;
    }
    if (ImGui::Button("Defaults")) ImGui::OpenPopup("Put Orbit Lab back?");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Put this lab's settings back to how they started");
    ImGui::SetNextWindowSize(ImVec2(24 * ImGui::GetFontSize(), 0));
    if (ImGui::BeginPopupModal("Put Orbit Lab back?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextWrapped("This puts every Orbit Lab setting back to how it started: 2D view, zoom, Time speed, sizes, trails, "
                           "sky and extras, and the Solar System back to today. What you selected is let go. Your units choice stays.");
        ImGui::Spacing();
        if (ImGui::Button("Yes, put them back")) { putBackDefaults(app); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }
}

}  // namespace ui
