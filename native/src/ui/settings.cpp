#include "settings.h"
#include "ui/shell.h"
#include "ui/shell_widgets.h"
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
#include <sstream>
#include <string>
#include <vector>

#include "app/app_data.h"
#include "app/gpu_guard.h"
#include "app/grid.h"
#include "fonts.h"
#include "html_view.h"
#include "reading.h"
#include "imgui.h"
#include "app_pages.h"
#include "look.h"
#include "style_draw.h"
#include "orbit/format.h"
#include "orbit/units.h"
#include "rich_text.h"
#include "ui/changelog.h"
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

}  // namespace

std::string settingsFolder() {
    const std::string p = settingsPath();
    const std::size_t k = p.find_last_of("/\\");
    return k == std::string::npos ? std::string() : p.substr(0, k + 1);
}

namespace {

std::string g_written;            // what the file holds now
double g_changedAt = -1;          // when the settings last changed (s), -1 = nothing waiting to be written

const char* const kNewUiAtomView = "Cloud 3D";   // the New UI's first-ever Atom Lab view (owner's decision, 6 Oct 2026); a view saved by an earlier run is kept

// Atom Lab back to how it starts: every setting at its default and a fresh atom;
void putBackAtomDefaults(AppState& app) {
    app.atom.p = atom::AtomParams::appDefaults();
    app.atom.p.view = kNewUiAtomView;    // (stage 5g: the New UI's starting view)
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
    if (app::gpuGuard().started()) p.gpuPaths = app::gpuGuard().saved();
    if (app::gpuSkyGuard().started()) p.gpuSky = app::gpuSkyGuard().saved();
    if (app::gpuSliceGuard().started()) p.gpuSlice = app::gpuSliceGuard().saved();   // 0.57.0
    if (app::gpuMolCloudGuard().started()) p.gpuMolCloud = app::gpuMolCloudGuard().saved();   // 0.57.1
    if (app::gpuJumpGuard().started()) p.gpuJump = app::gpuJumpGuard().saved();   // 0.56.0   // 0.55.7: what the light paths' guard keeps (before it starts: the file's line as read)
    const orbit::ViewSettings& v = app.set;
    p.orbit = {{"view3D", v.view3D ? "1" : "0"}, {"zoom", num(v.zoom)}, {"tspeed", num(v.tspeed)}, {"trueScale", v.trueScale ? "1" : "0"},
               {"fullTrails", v.fullTrails ? "1" : "0"}, {"moons", v.moons ? "1" : "0"}, {"dwarfs", v.dwarfs ? "1" : "0"}, {"comets", v.comets ? "1" : "0"},
               {"belts", v.belts ? "1" : "0"}, {"physics", orbit::planetPhysicsText(v.realPlanets)}, {"gr", v.relativity ? "1" : "0"}, {"gr29", "1"}, {"wobble", v.exaggerateWobble ? "1" : "0"}, {"sky", kSky[int(v.sky)]}, {"cons", kCons[int(v.cons)]}, {"far", v.far ? "1" : "0"}};
    if (!v.orbits) p.orbit["orbits"] = "0";                 // 0.23 stage 2: saved only when off (on is the default): older settings files stay byte for byte
    const atom::AtomParams& a = app.atom.p;
    p.atom = {{"scale", a.scale}, {"zoom", num(a.zoom)}, {"qstyle", a.qstyle}, {"detail", a.detail}, {"style", a.style}, {"Z", std::to_string(a.Z)},
              {"N", std::to_string(a.N)}, {"view", a.view}, {"ostyle", a.ostyle}, {"orb", a.orb}, {"pAx", a.pAx}, {"dAx", a.dAx}, {"fAx", a.fAx},
              {"cutAxis", a.cutAxis}, {"cstyle", a.cstyle}, {"cdens", a.cdens}, {"outline", a.outline}, {"phase", a.phase}, {"super", a.super}, {"orb2", a.orb2}, {"pAx2", a.pAx2}, {"dAx2", a.dAx2}, {"fAx2", a.fAx2}, {"mix", num(a.mix)}, {"cut", num(a.cut)}, {"n", std::to_string(a.n)}, {"speed", std::to_string(a.speed)}};
    if (a.dirac != "On") p.atom["dirac"] = a.dirac;           // saved only when not the default (0.49.3: On is the default; it was Off)
    if (a.elec != "All") p.atom["elec"] = a.elec;   // 0.46: All is the default (was One)
    if (a.mrel != "Relativistic") p.atom["mrel"] = a.mrel;    // 0.41: saved only when not the default             // 0.39: saved only when not the default (older settings files stay byte for byte)
    if (a.show != "Whole atom") p.atom["show"] = a.show;
    if (a.rscale != "True scale") p.atom["rscale"] = a.rscale;
    if (a.hrel != "Auto") p.atom["hrel"] = a.hrel;           // 0.49.3: saved only when not the default
    if (a.hj != "l + 1/2") p.atom["hj"] = a.hj;
    if (a.vdots != "Easier to see") p.atom["vdots"] = a.vdots;   // 0.54.1: saved only when not the default   // 0.49.3: True scale is the default (was Shells enlarged)
    p.atomMore = app.atom.moreStored;
    if (app.visited[molecule::kLabIndex] || !p.molecule.empty()) p.molecule = molecule::saveSettings(app.molecule.set);   // (only once the lab was opened: older settings files stay as they were)
    if (app.visited[blackhole::kLabIndex] || !p.blackhole.empty()) blackhole::saveSettings(app.blackhole, p.blackhole);    // 0.55.0 (the same rule)
    p.lab = app::labName(app.lab);
    p.units = fmt::imperial() ? "imperial" : "metric";
}

// a saved value, checked against what the control still allows
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
    readBool(p.orbit, "dwarfs", v.dwarfs); readBool(p.orbit, "comets", v.comets); readBool(p.orbit, "belts", v.belts); readBool(p.orbit, "far", v.far); readBool(p.orbit, "orbits", v.orbits);
    if (p.orbit.count("gr29")) readBool(p.orbit, "gr", v.relativity);   // 0.29: relativity is on by default; a "gr" saved before 0.29 (nearly always the old default, off) is not read
    readBool(p.orbit, "wobble", v.exaggerateWobble);
    if (p.orbit.count("physics")) { bool real = v.realPlanets; if (orbit::planetPhysicsFromText(p.orbit.at("physics"), real)) v.realPlanets = real; }   // a bad value keeps the default
    for (int i = 0; i < 3; i++) {
        if (p.orbit.count("sky") && p.orbit.at("sky") == kSky[i]) v.sky = orbit::SkyMode(i);
        if (p.orbit.count("cons") && p.orbit.at("cons") == kCons[i]) v.cons = orbit::ConsMode(i);
    }
    atom::AtomParams a = atom::AtomParams::appDefaults();
    for (const char* k : {"scale", "qstyle", "detail", "style", "view", "ostyle", "orb", "pAx", "dAx", "fAx", "cutAxis", "cstyle", "cdens", "outline", "dirac", "phase", "super", "orb2", "pAx2", "dAx2", "fAx2"}) {
        const auto it = p.atom.find(k);
        if (it == p.atom.end()) continue;
        const std::string val = atom::AtomLab::migrateChoice(k, it->second);   // 0.43: old style names
        if (!atom::AtomLab::validChoice(k, val)) continue;
        std::string& f = std::string(k) == "scale" ? a.scale : std::string(k) == "qstyle" ? a.qstyle : std::string(k) == "detail" ? a.detail
                       : std::string(k) == "style" ? a.style : std::string(k) == "view" ? a.view : std::string(k) == "ostyle" ? a.ostyle
                       : std::string(k) == "orb" ? a.orb : std::string(k) == "pAx" ? a.pAx : std::string(k) == "dAx" ? a.dAx
                       : std::string(k) == "fAx" ? a.fAx : std::string(k) == "cstyle" ? a.cstyle : std::string(k) == "cdens" ? a.cdens : std::string(k) == "outline" ? a.outline : std::string(k) == "dirac" ? a.dirac : std::string(k) == "phase" ? a.phase
                       : std::string(k) == "super" ? a.super : std::string(k) == "orb2" ? a.orb2 : std::string(k) == "pAx2" ? a.pAx2 : std::string(k) == "dAx2" ? a.dAx2 : std::string(k) == "fAx2" ? a.fAx2 : a.cutAxis;
        f = val;
    }
    for (const char* k : {"hrel", "hj"}) { const auto it = p.atom.find(k); if (it != p.atom.end() && atom::AtomLab::validChoice(k, it->second)) (std::string(k) == "hrel" ? a.hrel : a.hj) = it->second; }   // 0.49.3
    { const auto it = p.atom.find("vdots"); if (it != p.atom.end() && atom::AtomLab::validChoice("vdots", it->second)) a.vdots = it->second; }   // 0.54.1
    for (const char* k : {"elec", "show", "rscale", "mrel"}) {       // 0.39
        const auto it = p.atom.find(k);
        const std::string v = it != p.atom.end() ? atom::AtomLab::migrateChoice(k, it->second) : std::string();   // 0.46: a saved "One" loads as "Hydrogen-like"
        if (it != p.atom.end() && atom::AtomLab::validChoice(k, v)) (std::string(k) == "elec" ? a.elec : std::string(k) == "show" ? a.show : std::string(k) == "mrel" ? a.mrel : a.rscale) = v;
    }
    if (!p.atom.count("view")) a.view = kNewUiAtomView;   // 0.15 stage 5g (owner's decision of 6 Oct 2026): the New UI's Atom Lab opens in Cloud 3D the first time (no saved view); the old app keeps Orbit
    if (readNum(p.atom, "zoom", atom::kZoomMin, 7, d)) a.zoom = d;
    if (readNum(p.atom, "Z", 1, 118, d)) { a.Z = int(d); a.N = atom::defaultN(a.Z); }
    if (readNum(p.atom, "N", atom::nRange(a.Z).first, atom::nRange(a.Z).second, d)) a.N = int(d);
    if (readNum(p.atom, "cut", -1, 1, d)) a.cut = d;
    if (readNum(p.atom, "mix", 0, 100, d)) a.mix = d;
    if (readNum(p.atom, "n", 1, atom::AtomLab::kLineMaxN, d)) a.n = int(d);   // 0.50.3 (outside review): the panel's limit since 0.46 (was 4: a saved n = 6 came back as 4)
    if (readNum(p.atom, "speed", 1, 30, d)) a.speed = int(d);
    app.atom.p = a;
    app.atom.moreStored = p.atomMore;
    app.atom.restart();
    app.molecule.set = molecule::loadSettings(p.molecule);
    app.blackhole = blackhole::loadSettings(p.blackhole);   // 0.55.0
    app.lab = app::labIndex(p.lab);
    fmt::setImperial(p.units == "imperial");
}

// a "setOpt" tile: a big Aa (in the given font and size) over a small name; returns true when clicked
// 0.38.1: a font tile's name split into lines that fit inside the tile (6 px in from each side): "Theme default" was wider than its tile in Paper's book font and touched both edges
std::vector<std::string> tileNameLines(const std::string& name, float width) {
    const float ns = ImGui::GetFontSize() * 0.8f, room = width - 2 * 6 * ImGui::GetFontSize() / 16;
    auto w = [&](const std::string& t) { return ImGui::GetFont()->CalcTextSizeA(ns, FLT_MAX, 0, t.c_str()).x; };
    std::vector<std::string> lines;
    std::string cur;
    std::size_t i = 0;
    while (i <= name.size()) {
        const std::size_t sp = std::min(name.find(' ', i), name.size());
        const std::string word = name.substr(i, sp - i);
        const std::string tryLine = cur.empty() ? word : cur + " " + word;
        if (!cur.empty() && w(tryLine) > room) { lines.push_back(cur); cur = word; } else cur = tryLine;
        i = sp + 1;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}
bool optionTile(const char* id, const std::string& name, bool on, ImFont* font, float aaSize, float width, const std::string& sub = "", int nameLines = 1) {
    ImGui::PushID(id);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float h = sub.empty() && font ? 62 * ImGui::GetFontSize() / 16 + (nameLines - 1) * ImGui::GetFontSize() * 0.8f * 1.1f : 46 * ImGui::GetFontSize() / 16;   // (0.38.1: room for a name on two lines)
    if (on) pushOn();
    const bool clicked = ui::Button("##opt", ImVec2(width, h));
    noteItem(id);                                           // (the automatic checks click a tile by its name)
    if (on) popOn();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 fg = on && onIsSolid() ? col(look().w.onacc) : col(look().w.fg), mu = on && onIsSolid() ? col(look().w.onacc) : col(look().w.muted);   // 0.38: on the tint, the usual colours
    if (font) {
        const ImVec2 a = font->CalcTextSizeA(aaSize, FLT_MAX, 0, "Aa");
        dl->AddText(font, aaSize, ImVec2(p.x + (width - a.x) / 2, p.y + 6 * ImGui::GetFontSize() / 16), fg, "Aa");
        const float ns = ImGui::GetFontSize() * 0.8f;
        const std::vector<std::string> lines = tileNameLines(displayable(name), width);   // 0.38.1: each line centred, the last one where the single line was
        for (std::size_t k = 0; k < lines.size(); k++) {
            const ImVec2 t = ImGui::GetFont()->CalcTextSizeA(ns, FLT_MAX, 0, lines[k].c_str());
            const ImVec2 at(p.x + (width - t.x) / 2, p.y + h - t.y - 6 - (lines.size() - 1 - k) * ns * 1.1f);
            dl->AddText(ImGui::GetFont(), ns, at, mu, lines[k].c_str());
            if (shellLogOn()) auditRect(std::string("tilename.") + id + "." + std::to_string(k), at.x, at.y, t.x, t.y);   // (for the automatic checks: the name inside its tile)
        }
        if (shellLogOn()) auditRect(std::string("tile.") + id, p.x, p.y, width, h);
    } else {
        const float fs = ImGui::GetFontSize();
        const ImVec2 t = ImGui::GetFont()->CalcTextSizeA(fs, FLT_MAX, 0, name.c_str());
        dl->AddText(fonts().bold, fs, ImVec2(p.x + 10, p.y + 6), fg, name.c_str());
        dl->AddText(ImGui::GetFont(), fs * 0.8f, ImVec2(p.x + 10, p.y + 8 + t.y), mu, sub.c_str());
    }
    ImGui::PopID();
    return clicked;
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

void saveSettingsWhenChanged(AppState& app, double nowS, bool now) {
    if (app.benchRunning) return;                             // the benchmark changes the labs on purpose and gives them back: not written
    labsToPrefs(app, app.prefs);
    const std::string text = app::savePrefs(app.prefs);
    if (text == g_written) { g_changedAt = -1; return; }
    if (g_changedAt < 0) g_changedAt = nowS;
    if (!now && nowS - g_changedAt < 0.25) return;
    const std::string path = settingsPath();
    if (path.empty()) return;
    if (app::writeSettingsFile(path, text)) { g_written = text; g_changedAt = -1; app::gpuGuard().settingsWritten(); app::gpuSkyGuard().settingsWritten(); app::gpuSliceGuard().settingsWritten(); app::gpuJumpGuard().settingsWritten(); app::gpuMolCloudGuard().settingsWritten(); }   // (0.55.7: a marker the file now keeps can go)
    else g_changedAt = nowS + 5;                              // 0.19: not written (a full disk, a read-only folder): the old file stays; try again in about 5 s, not every frame
}

// ---- 0.15 stage 5l: the list of the Settings window's rows (settings.h). The window draws each heading from it, so search (built from the same list) cannot miss one. ----
const std::vector<SettingRow>& settingRows() {
    static const std::vector<SettingRow> rows = {
        {"text", app::kSetH[0], "Text size (Settings)", -1},
        {"font", app::kSetH[1], "Font", -1},
        {"units", app::kSetH[2], "Units (metric or imperial)", -1},
        {"motion", app::kSetH[3], "Motion (reduced motion)", -1},
        {"facts", "💡 Facts", "Did you know facts", -1},
        {"perf", "Performance", "Performance", -1},
        {"vsync", "VSync (wait for the screen)", "VSync (wait for the screen)", -1},
        {"gpurocks", "GPU rocks (OpenGL 4.3)", "GPU rocks (OpenGL 4.3)", 0},
        {"gpuspin", "GPU superposition and jumps (OpenGL 4.3)", "GPU superposition and jumps (OpenGL 4.3)", 1},
        {"gpuoutline", "GPU outline (OpenGL 4.3)", "GPU outline (OpenGL 4.3)", 1},
        {"gpucolours", "GPU colours (Atom Lab cloud dots)", "GPU colours (Atom Lab cloud dots)", 1},
        {"gpuslice", "GPU Slice dots (OpenGL 4.3)", "GPU Slice dots (Atom Lab, OpenGL 4.3)", 1},     // 0.57.0
        {"gpujump", "GPU jump groups (OpenGL 4.3)", "GPU jump groups (Atom Lab, OpenGL 4.3)", 1},     // 0.57.0
        {"gpumolcloud", "GPU Density cloud (OpenGL 4.3)", "GPU Density cloud (Molecule Lab, OpenGL 4.3)", 2},   // 0.57.1
        {"gpupaths", "GPU light paths (OpenGL 4.3)", "GPU light paths (Black Hole Lab, OpenGL 4.3)", 3},   // 0.55.6 (blackhole::kLabIndex)
        {"gpusky", "GPU sky per pixel (spinning hole)", "GPU sky per pixel (Black Hole Lab)", 3},   // 0.56.0
        {"frames", "Frame times", "Frame times (fps)", -1},
        {"benchmark", "Benchmark", "Benchmark (speed test)", -1},
    };
    return rows;
}
const SettingRow& settingRow(const char* id) {
    for (const SettingRow& r : settingRows()) if (std::string(r.id) == id) return r;
    return settingRows().front();
}
bool settingRowShown(const SettingRow& r, const AppState& app) { return (r.lab < 0 || r.lab == app.lab); }

namespace {
bool g_inSettings = false;                                  // the Settings window is being drawn
std::string g_goto;                                         // search asked for this row
int g_gotoFrames = 0;                                       // the page's lines are measured as they are drawn: the scroll is aimed at the row for a few frames
double g_hlStart = -100;                                    // when the highlight began (ImGui time)
// 0.19: with PL_FIXED_DT (the automatic checks) the highlight's clock follows the frames, as the labs' clocks do; before, a slow, busy test machine could
// let the highlight run out on the wall clock before the picture was taken (ui5l_search failed twice in the parallel full run, never alone)
double hlNow() {
    static const double fixedDt = std::getenv("PL_FIXED_DT") ? std::atof(std::getenv("PL_FIXED_DT")) : 0.0;
    return fixedDt > 0 ? ImGui::GetFrameCount() * fixedDt : ImGui::GetTime();
}
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
            if (--g_gotoFrames == 7) g_hlStart = hlNow();
        }
    }
    const double age = hlNow() - g_hlStart;
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
// 0.38: the headings drop their leading emoji ("🔠 Text size" -> "Text size"): it was drawn as a tiny, misaligned glyph, and the new look has no emoji in the interface
std::string withoutLeadingEmoji(const std::string& h) {
    const std::size_t sp = h.find(' ');
    if (sp == std::string::npos || sp == 0) return h;
    for (std::size_t i = 0; i < sp; i++) if (static_cast<unsigned char>(h[i]) < 0x80) return h;   // the first word is plain text: keep it
    return h.substr(sp + 1);
}
void settingHead(const char* id) { settingMark(id); richHeading(displayable(withoutLeadingEmoji(settingRow(id).heading))); }
}  // namespace

void setMotion(AppState& app, const std::string& k) {
    const bool was = look().reduced;
    app.prefs.motion = k;
    const bool now = k == "on" || (k == "auto" && systemReducedMotion());
    if (now && !was) { app.paused = true; if (app.lab == 1) app.atom.s.spin = false; }
}

void settingsWindow(AppState& app) {
    if (!app.showSettings) return;
    app::Prefs& P = app.prefs;
    if (readingModeOn()) dimBehind("##settingsDim", &app.showSettings);   // 0.38.1: the picture dims behind Settings as behind About and the guides; a click on it closes Settings (before the SetNextWindow* calls below, which belong to Settings)
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(std::min(30 * ImGui::GetFontSize(), vp->WorkSize.x * 0.95f), std::min(40 * ImGui::GetFontSize(), vp->WorkSize.y * 0.92f)), ImGuiCond_Appearing);   // never bigger than the window (small window, big text)
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    pushFloatStyle();
    const bool settingsShown = ImGui::Begin((std::string(app::kSettingsTitle) + "###settings").c_str(), &app.showSettings, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    popFloatStyle();
    if (settingsShown) {
            panelDecor(app::PanelKind::Floating);
        {   // 0.22 stage 6: the same header as About and the guides (a large title and a close button), not a small title bar
            const sh::Frame& f = sh::F();
            const float btn = static_cast<float>(f.m.iconBtn);
            const ImVec2 o = ImGui::GetCursorScreenPos();
            const float w = ImGui::GetContentRegionAvail().x;
            sh::headingText(ImGui::GetWindowDrawList(), "h1", ImVec2(o.x, o.y + (btn - sh::headingPx("h1")) / 2), sh::cText(), displayable(app::kSettingsTitle));
            if (sh::iconButton("##settingsX", ImVec2(o.x + w - btn, o.y), ImVec2(btn, btn), sh::Icon::Close, "Close (Esc)")) app.showSettings = false;
            ImGui::SetCursorScreenPos(o);
            ImGui::Dummy(ImVec2(w, btn + static_cast<float>(f.m.s8)));
            if (ImGui::IsKeyPressed(ImGuiKey_Escape) && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) app.showSettings = false;
        }
        g_inSettings = true; g_drawnRows.clear(); g_hl.a = 0;
        auditRect("win.settings", ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight());
        const float avail = ImGui::GetContentRegionAvail().x, gap = ImGui::GetStyle().ItemSpacing.x;
        auto tiles = [&](int n) { return (avail - gap * (n - 1)) / n; };
        // 🔠 Text size
        settingHead("text");
        int sizeLines = 1;                                  // 0.38.1: as the font row below (a name on two lines gives the whole row the room)
        for (int i = 0; i < 4; i++) sizeLines = std::max<int>(sizeLines, static_cast<int>(tileNameLines(displayable(app::kTextSizes[i].name), tiles(4)).size()));
        for (int i = 0; i < 4; i++) {
            if (i) ImGui::SameLine();
            const app::SizeOpt& o = app::kTextSizes[i];
            if (optionTile(o.key, o.name, P.textSize == o.key, fonts().regular, float(o.factor * 17) * ImGui::GetFontSize() / 16 / look().textScale, tiles(4), "", sizeLines))
                P.textSize = o.key;
        }
        richText(std::string("<p class=\"lrnP lrnMuted\">") + app::kSizeNote + "</p>");
        richText(std::string("<p class=\"lrnP\">") + app::kSizeSample + "</p>");
        // 🔤 Font
        settingHead("font");
        int fontLines = 1;                                  // 0.38.1: when one name needs two lines, every tile of the row gets the room (one height for the row)
        for (int i = 0; i < 5; i++) fontLines = std::max<int>(fontLines, static_cast<int>(tileNameLines(displayable(app::kFonts[i].name), tiles(5)).size()));
        for (int i = 0; i < 5; i++) {
            if (i) ImGui::SameLine();
            const app::FontOpt& o = app::kFonts[i];
            const std::string fam = app::fontKeyFor(look().w.style, o.key, fontFamilyAvailable(app::fontFamilyKeyOf(look().w.style)));   // the sample in the family Auto would use under this theme
            if (optionTile(o.key, o.name, P.font == o.key, fontFamily(fam).regular, 17 * ImGui::GetFontSize() / 16, tiles(5), "", fontLines)) P.font = o.key;
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
        {   // 0.15 stage 4: what the new UI needs a home for. Facts: the fact line; Performance: what the old side column held (VSync, GPU rocks, frame times, Benchmark)
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
        g_inSettings = false;
        if (g_hl.a > 0) { ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(g_hl.x0, g_hl.y0), ImVec2(g_hl.x1, g_hl.y1), col(look().w.accent, g_hl.a), 4); g_hl.a = 0; }
        if (shellLogOn()) { std::string d; for (const std::string& r : g_drawnRows) d += (d.empty() ? "" : ",") + r; auditNote("settingrows " + d); }
    }
    ImGui::End();
}

// The picker's list (0.15 stage 3: also drawn by the new UI's palette button, right after it)
void themePicker(AppState& app, const ImVec2* hang) {
    static const bool testOpen = SDL_getenv("PL_OPEN") && std::string(SDL_getenv("PL_OPEN")) == "themes";   // for automatic checks: open the picker (after the other windows have appeared)
    static int frames = 0;
    if (testOpen && ++frames == 12) ImGui::OpenPopup("##themes");
    if (hang) ImGui::SetNextWindowPos(*hang, ImGuiCond_Always, ImVec2(1.0f, 0.0f));   // 5q: the new UI hangs the list under the top bar, its right edge under the palette button: it covers no top-bar icon
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
    float nameW = 0;                                                     // the widest name
    const std::vector<const app::ThemeDef*>& themes = app::allThemes();
    for (const app::ThemeDef* t : themes) nameW = std::max(nameW, bold->CalcTextSizeA(fs, FLT_MAX, 0, displayable(t->name, bold).c_str()).x);
    float cardW = fixed + nameW + 2 * s;
    cardW = std::min(cardW, (maxWidth - (kColumns - 1) * gap - 2 * ImGui::GetStyle().WindowPadding.x) / kColumns);
    const float textW = std::max(20 * s, cardW - fixed);
    std::vector<std::string> shown;                                      // each card's name, wrapped to the card
    std::size_t lines = 1;
    for (const app::ThemeDef* t : themes) {
        std::string n = wrapLabel(displayable(t->name, bold), textW);
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
        const bool on = app.prefs.theme == t.id;
        ImGui::SetCursorPos(ImVec2(o.x + cell.col * (cardW + gap), o.y + cell.row * (cardH + gap)));
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::PushID(t.id);
        if (ImGui::Selectable("##t", on, 0, ImVec2(cardW, cardH))) { app.prefs.theme = t.id; ImGui::CloseCurrentPopup(); }
        if (ImGui::IsItemHovered()) ui::SetTooltip("%s", displayable(t.sub).c_str());
        ImGui::PopID();
        // the colour dot: the theme's background with its accent; Auto is half light, half dark
        const ImVec2 c(p.x + pad + dot / 2, p.y + cardH / 2);
        if (*t.swatchBg) {
            dl->AddCircleFilled(c, dot / 2, col(app::hexRgb(t.swatchBg)));
            dl->AddCircleFilled(ImVec2(c.x + dot * 0.18f, c.y + dot * 0.18f), dot * 0.2f, col(app::hexRgb(t.swatchAccent)));
        } else {
            dl->PathArcTo(c, dot / 2, PI * 0.5f, PI * 1.5f, 24); dl->PathFillConvex(col(app::themeColors("light", true).bg));      // left half light (the Light theme's page)
            dl->PathArcTo(c, dot / 2, -PI * 0.5f, PI * 0.5f, 24); dl->PathFillConvex(col(app::themeColors("dark", false).bg));     // right half dark (the Dark theme's page)
        }
        dl->AddCircle(c, dot / 2, col(look().w.line), 0, 1.0f);
        const float ty = p.y + cardH / 2 - (std::count(shown[i].begin(), shown[i].end(), '\n') + 1) * fs * 1.2f / 2;
        dl->AddText(bold, fs, ImVec2(p.x + pad + dot + pad, ty), col(look().w.fg), shown[i].c_str());
        if (on) dl->AddText(bold, fs, ImVec2(p.x + cardW - pad - tick, p.y + cardH / 2 - fs / 2), col(look().w.accent), "✓");
    }
    ImGui::SetCursorPos(ImVec2(o.x, o.y + rows * (cardH + gap) - gap));    // the popup is exactly as tall and wide as the grid
    ImGui::Dummy(ImVec2(kColumns * cardW + (kColumns - 1) * gap, 0));
    if (shellLogOn()) { const ImVec2 wp = ImGui::GetWindowPos(); auditRect("popup.themes", wp.x, wp.y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight()); auditNote("popupscroll themes max=" + std::to_string(static_cast<int>(ImGui::GetScrollMaxY()))); }   // (5l: for the tests)
    ImGui::EndPopup();
}

// The two "Put ... back?" questions of Defaults (0.15 stage 3: split from the button so the new UI's menu can open them from its own window)
void defaultsPopups(AppState& app) {
    if (app.lab == blackhole::kLabIndex) {                  // 0.55.0
        ImGui::SetNextWindowSize(ImVec2(24 * ImGui::GetFontSize(), 0));
        if (ImGui::BeginPopupModal("Put Black Hole Lab back?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            panelDecor(app::PanelKind::Floating);
            char t[320];
            std::snprintf(t, sizeof t, "This puts every Black Hole Lab setting back to how it started: Sagittarius A*, %g GM/c\xC2\xB2 away, a %g\xC2\xB0 view towards the "
                          "centre of our galaxy, gravity bending light, our sky behind. Your units choice stays.", blackhole::kDistDefault, blackhole::kFovDefault);
            ImGui::TextWrapped("%s", t);
            ImGui::Spacing();
            if (ui::Button("Yes, put them back")) { app.blackhole = blackhole::settingsDefaults(); ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            if (ui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }
        return;
    }
    if (app.lab == 1) {
        ImGui::SetNextWindowSize(ImVec2(24 * ImGui::GetFontSize(), 0));
        if (ImGui::BeginPopupModal("Put Atom Lab back?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            panelDecor(app::PanelKind::Floating);
            ImGui::TextWrapped("This puts every Atom Lab setting back to how it started: hydrogen-1, the Cloud 3D view, the 1s orbital, level n = 1, "
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


// ---- 0.51: a setup as text, to keep or share (the settings file's own lines for the labs, then the date and the selection) ----
std::string setupText(const AppState& app) {
    app::Prefs p;
    labsToPrefs(app, p);
    p.molecule = molecule::saveSettings(app.molecule.set);
    blackhole::saveSettings(app.blackhole, p.blackhole);    // 0.55.0
    std::string o = std::string("# Physics Lab ") + appVersion() + " setup: in the app, Menu > Paste setup opens this same state (the lab, its settings, the date, the selected body).\n";
    o += "setup=1\n";
    std::istringstream in(app::savePrefs(p));
    std::string line;
    while (std::getline(in, line))
        if (line.rfind("lab=", 0) == 0 || line.rfind("molecule=", 0) == 0 || line.rfind("orbit.", 0) == 0 || line.rfind("atom.", 0) == 0 || line.rfind("blackhole.", 0) == 0) o += line + "\n";
    o += "date=" + std::to_string(app.sys.currentUnixMs()) + "\n";
    if (app.view.sel.kind == orbit::Selection::Body)
        for (const auto& b : app.sys.bodies) if (b.id == app.view.sel.bodyId) o += "select=" + b.name + "\n";
    return o;
}
bool applySetup(AppState& app, const std::string& text, std::string& why) {
    if (text.find("setup=1") == std::string::npos) { why = "The clipboard holds no Physics Lab setup (Menu > Copy setup makes one)."; return false; }
    const app::Prefs in = app::loadPrefs(text);
    app::Prefs p = app.prefs;
    p.orbit = in.orbit; p.atom = in.atom; p.molecule = in.molecule; p.blackhole = in.blackhole; p.lab = in.lab;
    prefsToLabs(app, p);
    labsToPrefs(app, app.prefs);
    std::istringstream is(text);
    std::string line;
    while (std::getline(is, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("date=", 0) == 0) {
            char* end = nullptr;
            const long long ms = std::strtoll(line.c_str() + 5, &end, 10);
            if (end && !*end) jumpToDate(app, ms);
        }
        if (line.rfind("select=", 0) == 0)
            for (const auto& b : app.sys.bodies) if (b.name == line.substr(7)) app.view.select({orbit::Selection::Body, b.id}, true);
    }
    return true;
}

}  // namespace ui
