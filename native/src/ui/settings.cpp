#include "settings.h"
#include "ui/shell.h"
#include "ui/shell_test.h"

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
#include <vector>

#include "app/app_data.h"
#include "app/grid.h"
#include "fonts.h"
#include "html_view.h"
#include "imgui.h"
#include "app_pages.h"
#include "look.h"
#include "style_draw.h"
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

const char* const kNewUiAtomView = "Cloud 3D";   // the New UI's first-ever Atom Lab view (owner's decision, 6 Oct 2026); a view saved by an earlier run is kept

// Atom Lab back to how it starts: every setting at its default and a fresh atom (the web's load(lab, fresh));
// the More / Less info choice and the Temperature slider are kept, as on the web
void putBackAtomDefaults(AppState& app) {
    app.atom.p = atom::AtomParams::appDefaults();
    if (newUiOn(app)) app.atom.p.view = kNewUiAtomView;    // (stage 5g: the New UI's starting view)
    app.atom.restart();
    app.atomLearn = 0;
}

// Orbit Lab back to how it starts: today's Solar System, the starting view and every setting at its default
void putBackDefaults(AppState& app) {
    journeyStop(app);
    app.set = orbit::ViewSettings{};
    app.set.tspeed = units::kTspeedDefault;               // Defaults puts the Time speed, zoom and switches back itself (ViewSettings{} above; resetToToday keeps the user's)
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
               {"belts", v.belts ? "1" : "0"}, {"physics", orbit::planetPhysicsText(v.realPlanets)}, {"gr", v.relativity ? "1" : "0"}, {"wobble", v.exaggerateWobble ? "1" : "0"}, {"sky", kSky[int(v.sky)]}, {"cons", kCons[int(v.cons)]}, {"far", v.far ? "1" : "0"}};
    const atom::AtomParams& a = app.atom.p;
    p.atom = {{"scale", a.scale}, {"zoom", num(a.zoom)}, {"qstyle", a.qstyle}, {"detail", a.detail}, {"style", a.style}, {"Z", std::to_string(a.Z)},
              {"N", std::to_string(a.N)}, {"view", a.view}, {"ostyle", a.ostyle}, {"orb", a.orb}, {"pAx", a.pAx}, {"dAx", a.dAx}, {"fAx", a.fAx},
              {"cutAxis", a.cutAxis}, {"cstyle", a.cstyle}, {"cdens", a.cdens}, {"outline", a.outline}, {"phase", a.phase}, {"super", a.super}, {"orb2", a.orb2}, {"pAx2", a.pAx2}, {"dAx2", a.dAx2}, {"fAx2", a.fAx2}, {"mix", num(a.mix)}, {"cut", num(a.cut)}, {"n", std::to_string(a.n)}, {"speed", std::to_string(a.speed)}};
    if (a.dirac == "On") p.atom["dirac"] = a.dirac;           // saved only when ON: with the setting off the settings file is byte for byte what it was before 0.10.8
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
    readBool(p.orbit, "gr", v.relativity); readBool(p.orbit, "wobble", v.exaggerateWobble);
    if (p.orbit.count("physics")) { bool real = v.realPlanets; if (orbit::planetPhysicsFromText(p.orbit.at("physics"), real)) v.realPlanets = real; }   // a bad value keeps the default
    for (int i = 0; i < 3; i++) {
        if (p.orbit.count("sky") && p.orbit.at("sky") == kSky[i]) v.sky = orbit::SkyMode(i);
        if (p.orbit.count("cons") && p.orbit.at("cons") == kCons[i]) v.cons = orbit::ConsMode(i);
    }
    atom::AtomParams a = atom::AtomParams::appDefaults();
    for (const char* k : {"scale", "qstyle", "detail", "style", "view", "ostyle", "orb", "pAx", "dAx", "fAx", "cutAxis", "cstyle", "cdens", "outline", "dirac", "phase", "super", "orb2", "pAx2", "dAx2", "fAx2"}) {
        const auto it = p.atom.find(k);
        if (it == p.atom.end() || !atom::AtomLab::validChoice(k, it->second)) continue;
        std::string& f = std::string(k) == "scale" ? a.scale : std::string(k) == "qstyle" ? a.qstyle : std::string(k) == "detail" ? a.detail
                       : std::string(k) == "style" ? a.style : std::string(k) == "view" ? a.view : std::string(k) == "ostyle" ? a.ostyle
                       : std::string(k) == "orb" ? a.orb : std::string(k) == "pAx" ? a.pAx : std::string(k) == "dAx" ? a.dAx
                       : std::string(k) == "fAx" ? a.fAx : std::string(k) == "cstyle" ? a.cstyle : std::string(k) == "cdens" ? a.cdens : std::string(k) == "outline" ? a.outline : std::string(k) == "dirac" ? a.dirac : std::string(k) == "phase" ? a.phase
                       : std::string(k) == "super" ? a.super : std::string(k) == "orb2" ? a.orb2 : std::string(k) == "pAx2" ? a.pAx2 : std::string(k) == "dAx2" ? a.dAx2 : std::string(k) == "fAx2" ? a.fAx2 : a.cutAxis;
        f = it->second;
    }
    if (!p.atom.count("view") && newUiOn(app)) a.view = kNewUiAtomView;   // 0.15 stage 5g (owner's decision of 6 Oct 2026): the New UI's Atom Lab opens in Cloud 3D the first time (no saved view); the old app keeps Orbit
    if (readNum(p.atom, "zoom", atom::kZoomMin, 7, d)) a.zoom = d;
    if (readNum(p.atom, "Z", 1, 118, d)) { a.Z = int(d); a.N = atom::defaultN(a.Z); }
    if (readNum(p.atom, "N", atom::nRange(a.Z).first, atom::nRange(a.Z).second, d)) a.N = int(d);
    if (readNum(p.atom, "cut", -1, 1, d)) a.cut = d;
    if (readNum(p.atom, "mix", 0, 100, d)) a.mix = d;
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
    const bool clicked = ui::Button("##opt", ImVec2(width, h));
    noteItem(id);                                           // (the automatic checks click a tile by its name)
    if (on) popOn();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 fg = on ? col(look().w.onacc) : col(look().w.fg), mu = on ? col(look().w.onacc) : col(look().w.muted);
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
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + size.y + 2 * padY), col(look().w.line), corner(10 * s));
    dl->AddText(fonts().bold, ImGui::GetFontSize(), ImVec2(p.x + padX, p.y + padY), col(look().w.fg), t.c_str(), nullptr, w - 2 * padX);
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
    if (app.benchRunning) return;                             // the benchmark changes the labs on purpose and gives them back: not written
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

// ---- 0.15 stage 5l: the list of the Settings window's rows (settings.h). The window draws each heading from it, so search (built from the same list) cannot miss one. ----
const std::vector<SettingRow>& settingRows() {
    static const std::vector<SettingRow> rows = {
        {"text", app::kSetH[0], "Text size (Settings)", -1, false},
        {"font", app::kSetH[1], "Font", -1, false},
        {"units", app::kSetH[2], "Units (metric or imperial)", -1, false},
        {"motion", app::kSetH[3], "Motion (reduced motion)", -1, false},
        {"progress", app::kSetH[4], "Progress (start again)", -1, false},
        {"developer", app::kSetH[5], "Developer (unlock everything)", -1, false},
        {"newui", "🧪 New UI (preview)", "New UI (preview)", -1, false},
        {"facts", "💡 Facts", "Did you know facts", -1, true},
        {"perf", "Performance", "Performance", -1, true},
        {"vsync", "VSync (wait for the screen)", "VSync (wait for the screen)", -1, true},
        {"gpurocks", "GPU rocks (OpenGL 4.3)", "GPU rocks (OpenGL 4.3)", 0, true},
        {"gpuspin", "GPU superposition and jumps (OpenGL 4.3)", "GPU superposition and jumps (OpenGL 4.3)", 1, true},
        {"gpuoutline", "GPU outline (OpenGL 4.3)", "GPU outline (OpenGL 4.3)", 1, true},
        {"gpucolours", "GPU colours (Atom Lab cloud dots)", "GPU colours (Atom Lab cloud dots)", 1, true},
        {"frames", "Frame times", "Frame times (fps)", -1, true},
        {"benchmark", "Benchmark", "Benchmark (speed test)", -1, true},
    };
    return rows;
}
const SettingRow& settingRow(const char* id) {
    for (const SettingRow& r : settingRows()) if (std::string(r.id) == id) return r;
    return settingRows().front();
}
bool settingRowShown(const SettingRow& r, const AppState& app) { return (r.lab < 0 || r.lab == app.lab) && (!r.newUiOnly || newUiOn(app)); }

namespace {
bool g_inSettings = false;                                  // the Settings window is being drawn
std::string g_goto;                                         // search asked for this row
int g_gotoFrames = 0;                                       // the page's lines are measured as they are drawn: the scroll is aimed at the row for a few frames
double g_hlStart = -100;                                    // when the highlight began (ImGui time)
struct Hl { float x0 = 0, y0 = 0, x1 = 0, y1 = 0, a = 0; } g_hl;   // the highlight to draw this frame (a == 0: none)
std::vector<std::string> g_drawnRows;                       // the rows drawn this frame (for the test)
constexpr double kHighlightSeconds = 1.6;
}  // namespace

void settingsGoto(AppState& app, const char* id) { app.showSettings = true; g_goto = id; g_gotoFrames = 8; g_hlStart = -100; }

void settingMark(const char* id) {
    if (!g_inSettings) return;
    g_drawnRows.push_back(id);
    const ImVec2 c = ImGui::GetCursorScreenPos();
    const float h = ImGui::GetFrameHeight() + 4;
    if (g_goto == id) {
        if (g_gotoFrames > 0) {
            ImGui::SetScrollHereY(0.12f);                   // the row's line near the top of the window
            if (--g_gotoFrames == 7) g_hlStart = ImGui::GetTime();
        }
    }
    const double age = ImGui::GetTime() - g_hlStart;
    if (g_goto == id && age >= 0 && age < kHighlightSeconds) {
        const float a = static_cast<float>(0.38 * (1.0 - age / kHighlightSeconds));
        const ImVec2 wp = ImGui::GetWindowPos();
        g_hl = {wp.x + 4, c.y - 2, wp.x + ImGui::GetWindowWidth() - 4, c.y - 2 + h, a};          // drawn over the row when the window ends (a button drawn after the mark would cover it)
        if (shellLogOn()) auditNote("settinghl " + std::string(id) + " alpha=" + std::to_string(a));
    }
    if (shellLogOn()) {
        auditRect(std::string("setting.") + id, c.x, c.y - 2, ImGui::GetWindowWidth() - 8, h);
        if (g_goto == id && g_gotoFrames == 0 && g_hlStart > -50) {
            g_gotoFrames = -1;
            const ImVec2 wp = ImGui::GetWindowPos();
            std::printf("PLSETROW %s y=%.1f h=%.1f winTop=%.1f winBottom=%.1f\n", id, c.y - 2, h, wp.y + ImGui::GetFrameHeight(), wp.y + ImGui::GetWindowHeight());
        }
    }
}

namespace {
void settingHead(const char* id) { settingMark(id); richHeading(displayable(settingRow(id).heading)); }
}  // namespace

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
    pushFloatStyle();
    const bool settingsShown = ImGui::Begin((std::string(app::kSettingsTitle) + "###settings").c_str(), &app.showSettings, ImGuiWindowFlags_NoCollapse);
    popFloatStyle();
    if (settingsShown) {
            panelDecor(app::PanelKind::Floating);
        g_inSettings = true; g_drawnRows.clear(); g_hl.a = 0;
        auditRect("win.settings", ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight());
        const float avail = ImGui::GetContentRegionAvail().x, gap = ImGui::GetStyle().ItemSpacing.x;
        auto tiles = [&](int n) { return (avail - gap * (n - 1)) / n; };
        // 🔠 Text size
        settingHead("text");
        for (int i = 0; i < 4; i++) {
            if (i) ImGui::SameLine();
            const app::SizeOpt& o = app::kTextSizes[i];
            if (optionTile(o.key, o.name, P.textSize == o.key, fonts().regular, float(o.factor * 17) * ImGui::GetFontSize() / 16 / look().textScale, tiles(4)))
                P.textSize = o.key;
        }
        richText(std::string("<p class=\"lrnP lrnMuted\">") + app::kSizeNote + "</p>");
        richText(std::string("<p class=\"lrnP\">") + app::kSizeSample + "</p>");
        // 🔤 Font
        settingHead("font");
        for (int i = 0; i < 5; i++) {
            if (i) ImGui::SameLine();
            const app::FontOpt& o = app::kFonts[i];
            const std::string fam = app::fontKeyFor(look().w.style, o.key, fontFamilyAvailable(app::fontFamilyKeyOf(look().w.style)));   // the sample in the family Auto would use under this theme
            if (optionTile(o.key, o.name, P.font == o.key, fontFamily(fam).regular, 17 * ImGui::GetFontSize() / 16, tiles(5))) P.font = o.key;
        }
        // 📏 Units
        settingHead("units");
        for (int i = 0; i < 2; i++) {
            if (i) ImGui::SameLine();
            const bool on = (i == 1) == fmt::imperial();
            if (optionTile(i ? "imperial" : "metric", app::kUnitOpts[i].name, on, nullptr, 0, tiles(2), app::kUnitOpts[i].sub)) fmt::setImperial(i == 1);
        }
        richText(std::string("<p class=\"lrnP lrnMuted\">") + app::kUnitsNote + "</p>");
        // 🌀 Motion
        settingHead("motion");
        const char* motions[3] = {"auto", "on", "off"};
        for (int i = 0; i < 3; i++) {
            if (i) ImGui::SameLine();
            if (optionTile(motions[i], app::kMotionOpts[i].name, P.motion == motions[i], nullptr, 0, tiles(3), app::kMotionOpts[i].sub)) setMotion(app, motions[i]);
        }
        richText(std::string("<p class=\"lrnP lrnMuted\">") + app::kMotionNote + "</p>");
        // 🔒 Progress
        settingHead("progress");
        if (optionTile("reset", app::kResetRow.name, false, nullptr, 0, avail, app::kResetRow.sub)) {
            g_resetThemes = app::unlockedThemeCount(P); g_resetAch = app::earnedAchievementCount(P); g_resetStage = 1;
            ImGui::OpenPopup("##reset");
        }
        if (g_resetDone) doneLine(app::kResetDoneLine);
        // 🛠 Developer
        settingHead("developer");
        if (optionTile("unlock", app::kUnlockRow.name, false, nullptr, 0, avail, app::kUnlockRow.sub)) {
            const auto [t, a] = app::unlockEverything(P);
            g_unlockMsg = app::kUnlockMsg[std::min(t, 2)][std::min(a, 5)];
        }
        if (!g_unlockMsg.empty()) doneLine(g_unlockMsg);
        // 🧪 New UI (preview) (0.15 stage 3): last, so the rest of this page is exactly as it was
        settingHead("newui");
        {
            const char* uiKeys[2] = {"off", "on"};
            const char* uiNames[2] = {"Off", "On"}, *uiSubs[2] = {"The layout you know", "The new layout, unfinished"};
            for (int i = 0; i < 2; i++) {
                if (i) ImGui::SameLine();
                if (optionTile((std::string("newui-") + uiKeys[i]).c_str(), uiNames[i], P.newUi == uiKeys[i], nullptr, 0, tiles(2), uiSubs[i])) P.newUi = uiKeys[i];
            }
            richText("<p class=\"lrnP lrnMuted\">A preview of the redesign (version 0.15): the picture fills the window, with a slim bar on top, a toolbar at the bottom and small floating panels. Only the Orbit Lab's date, speed, show, pick, scale, 2D / 3D and save controls are moved so far; everything else is in the Old panel button at the top right. Off is exactly the layout you know.</p>");
        if (newUiOn(app)) {   // 0.15 stage 4: what the new UI needs a home for. Facts: the fact line; Performance: what the old side column held (VSync, GPU rocks, frame times, Benchmark)
            settingHead("facts");
            const char* fKeys[2] = {"on", "off"};
            const char* fNames[2] = {"On", "Off"}, *fSubs[2] = {"A fact at the bottom left of the picture", "No fact line"};
            for (int i = 0; i < 2; i++) {
                if (i) ImGui::SameLine();
                if (optionTile((std::string("facts-") + fKeys[i]).c_str(), fNames[i], P.facts == fKeys[i], nullptr, 0, tiles(2), fSubs[i])) P.facts = fKeys[i];
            }
            richText("<p class=\"lrnP lrnMuted\">One short line of \"Did you know?\" at the bottom left of the picture, changing about every 20 seconds. It stops while the pointer is over it; a click opens the whole fact.</p>");
            performanceCard(app);
            auditNote("settingsnew facts-and-performance");
        }
        }
        g_inSettings = false;
        if (g_hl.a > 0) { ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(g_hl.x0, g_hl.y0), ImVec2(g_hl.x1, g_hl.y1), col(look().w.accent, g_hl.a), 4); g_hl.a = 0; }
        if (shellLogOn()) { std::string d; for (const std::string& r : g_drawnRows) d += (d.empty() ? "" : ",") + r; auditNote("settingrows " + d); }
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

// 🎨 Theme: a grid of cards, 3 in a row (rows are added as themes are added): a colour dot, the name, ✓ on the one in use. The small description of a
// theme is a tooltip. The two secret themes are "???" with their hint until they are unlocked. The cards are as wide as the longest name needs; if three of
// them do not fit in the window, the cards get narrower and long names wrap onto a second line (a name is never cut off).
void themeButton(AppState& app) {
    if (pillButton(app::kHdrTheme.label, app::kHdrTheme.tip)) ImGui::OpenPopup("##themes");
    themePicker(app);
}

// The picker's list (0.15 stage 3: also drawn by the new UI's palette button, right after it)
void themePicker(AppState& app) {
    static const bool testOpen = SDL_getenv("PL_OPEN") && std::string(SDL_getenv("PL_OPEN")) == "themes";   // for automatic checks: open the picker (after the other windows have appeared)
    static int frames = 0;
    if (testOpen && ++frames == 12) ImGui::OpenPopup("##themes");
    if (!ImGui::BeginPopup("##themes")) return;
    panelDecor(app::PanelKind::Floating);
    constexpr int kColumns = 3;
    constexpr float PI = 3.14159265f;
    const float s = ImGui::GetFontSize() / 16, fs = ImGui::GetFontSize(), pad = 8 * s, gap = 6 * s, dot = 24 * s;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImFont* bold = fonts().bold;
    const float tick = bold->CalcTextSizeA(fs, FLT_MAX, 0, "✓").x;
    const float fixed = 2 * pad + dot + 2 * pad + tick;                  // everything on a card except the name
    const float maxWidth = vp->WorkSize.x - 24 * s;                      // the popup must fit in the window
    float nameW = 0;                                                     // the widest name (a secret theme shows "???" only while it is locked)
    const std::vector<const app::ThemeDef*>& themes = app::allThemes();   // the web's themes in the picker's order (the archived ones are skipped)
    for (const app::ThemeDef* t : themes) nameW = std::max(nameW, bold->CalcTextSizeA(fs, FLT_MAX, 0, displayable(app::themeShownName(*t, app.prefs.unlocked), bold).c_str()).x);
    float cardW = fixed + nameW + 2 * s;
    cardW = std::min(cardW, (maxWidth - (kColumns - 1) * gap - 2 * ImGui::GetStyle().WindowPadding.x) / kColumns);
    const float textW = std::max(20 * s, cardW - fixed);
    std::vector<std::string> shown;                                      // each card's name, wrapped to the card
    std::size_t lines = 1;
    for (const app::ThemeDef* t : themes) {
        std::string n = wrapLabel(displayable(app::themeShownName(*t, app.prefs.unlocked), bold), textW);
        lines = std::max<std::size_t>(lines, 1 + std::count(n.begin(), n.end(), '\n'));
        shown.push_back(n);
    }
    const float cardH = std::max(dot, lines * fs * 1.2f) + 2 * pad;
    const int count = int(themes.size()), rows = app::gridRows(count, kColumns);
    const ImVec2 o = ImGui::GetCursorPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int i = 0; i < count; i++) {
        const app::ThemeDef& t = *themes[i];
        const app::GridCell cell = app::gridCell(i, kColumns);
        const bool locked = t.secret && !app.prefs.unlocked.count(t.id), on = app.prefs.theme == t.id;
        ImGui::SetCursorPos(ImVec2(o.x + cell.col * (cardW + gap), o.y + cell.row * (cardH + gap)));
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::PushID(t.id);
        if (locked) ImGui::BeginDisabled();
        if (ImGui::Selectable("##t", on, 0, ImVec2(cardW, cardH)) && !locked) { app.prefs.theme = t.id; ImGui::CloseCurrentPopup(); }
        if (locked) ImGui::EndDisabled();
        const std::string sub = locked ? app::kThemeLocked[1] : t.sub;
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ui::SetTooltip("%s", displayable(sub).c_str());
        ImGui::PopID();
        // the colour dot: the theme's background with its accent; Auto is half light, half dark
        const ImVec2 c(p.x + pad + dot / 2, p.y + cardH / 2);
        if (locked) {
            dl->AddCircleFilled(c, dot / 2, col(look().w.line));
            const ImVec2 q = bold->CalcTextSizeA(fs, FLT_MAX, 0, "?");
            dl->AddText(bold, fs, ImVec2(c.x - q.x / 2, c.y - q.y / 2), col(look().w.muted), "?");
        } else if (*t.swatchBg) {
            dl->AddCircleFilled(c, dot / 2, col(app::hexRgb(t.swatchBg)));
            dl->AddCircleFilled(ImVec2(c.x + dot * 0.18f, c.y + dot * 0.18f), dot * 0.2f, col(app::hexRgb(t.swatchAccent)));
        } else {
            dl->PathArcTo(c, dot / 2, PI * 0.5f, PI * 1.5f, 24); dl->PathFillConvex(col(app::themeColors("light", true).bg));      // left half light (the Light theme's page)
            dl->PathArcTo(c, dot / 2, -PI * 0.5f, PI * 0.5f, 24); dl->PathFillConvex(col(app::themeColors("dark", false).bg));     // right half dark (the Dark theme's page)
        }
        dl->AddCircle(c, dot / 2, col(look().w.line), 0, 1.0f);
        const float ty = p.y + cardH / 2 - (std::count(shown[i].begin(), shown[i].end(), '\n') + 1) * fs * 1.2f / 2;
        dl->AddText(bold, fs, ImVec2(p.x + pad + dot + pad, ty), col(look().w.fg, locked ? 0.6f : 1), shown[i].c_str());
        if (on) dl->AddText(bold, fs, ImVec2(p.x + cardW - pad - tick, p.y + cardH / 2 - fs / 2), col(look().w.accent), "✓");
    }
    ImGui::SetCursorPos(ImVec2(o.x, o.y + rows * (cardH + gap) - gap));    // the popup is exactly as tall and wide as the grid
    ImGui::Dummy(ImVec2(kColumns * cardW + (kColumns - 1) * gap, 0));
    if (shellLogOn()) { const ImVec2 wp = ImGui::GetWindowPos(); auditRect("popup.themes", wp.x, wp.y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight()); auditNote("popupscroll themes max=" + std::to_string(static_cast<int>(ImGui::GetScrollMaxY()))); }   // (5l: for the tests)
    ImGui::EndPopup();
}

// The two "Put ... back?" questions of Defaults (0.15 stage 3: split from the button so the new UI's menu can open them from its own window)
void defaultsPopups(AppState& app) {
    if (app.lab == 1) {
        ImGui::SetNextWindowSize(ImVec2(24 * ImGui::GetFontSize(), 0));
        if (ImGui::BeginPopupModal("Put Atom Lab back?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            panelDecor(app::PanelKind::Floating);
            ImGui::TextWrapped("This puts every Atom Lab setting back to how it started: hydrogen-1, the Orbit view, level n = 1, "
                               "zoom, scale, quark style and the other choices, and starts a fresh atom (the decay history starts "
                               "again). Your units and More / Less info choices stay.");
            ImGui::Spacing();
            if (ui::Button("Yes, put them back")) { putBackAtomDefaults(app); ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            if (ui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }
        return;
    }
    ImGui::SetNextWindowSize(ImVec2(24 * ImGui::GetFontSize(), 0));
    if (ImGui::BeginPopupModal("Put Orbit Lab back?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        panelDecor(app::PanelKind::Floating);
        ImGui::TextWrapped("This puts every Orbit Lab setting back to how it started: 2D view, zoom, Time speed, sizes, trails, "
                           "sky and extras, and the Solar System back to today. What you selected is let go. Your units choice stays.");
        ImGui::Spacing();
        if (ui::Button("Yes, put them back")) { putBackDefaults(app); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }
}

void defaultsButton(AppState& app) {
    if (ui::Button("Defaults")) ImGui::OpenPopup(app.lab == 1 ? "Put Atom Lab back?" : "Put Orbit Lab back?");
    if (ImGui::IsItemHovered()) ui::SetTooltip("Put this lab's settings back to how they started");
    defaultsPopups(app);
}

}  // namespace ui
