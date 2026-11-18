#include <cstdio>
#include <filesystem>
#include <system_error>
#include <algorithm>
#include <vector>
#include "prefs.h"

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>

#include "app_data.h"
#include "theme_style.h"

namespace app {

namespace {

std::string clean(const std::string& v) {           // one line: no line breaks inside a value
    std::string o;
    for (char c : v) if (c != '\n' && c != '\r') o += c;
    return o;
}

}  // namespace

bool validTheme(const std::string& id) { return findTheme(id) != nullptr; }
bool validTextSize(const std::string& k) { for (const SizeOpt& s : kTextSizes) if (k == s.key) return true; return false; }
bool validFont(const std::string& k) { for (const FontOpt& f : kFonts) if (k == f.key) return true; return false; }
bool validMotion(const std::string& k) { return k == "auto" || k == "on" || k == "off"; }
double textSizeFactor(const std::string& k) { for (const SizeOpt& s : kTextSizes) if (k == s.key) return s.factor; return 1; }

std::string savePrefs(const Prefs& p) {
    std::ostringstream o;
    o << "# Physics Lab settings. Written by the app; safe to delete.\n";
    o << "theme=" << clean(p.theme) << "\n" << "textSize=" << clean(p.textSize) << "\n" << "font=" << clean(p.font) << "\n"
      << "motion=" << clean(p.motion) << "\n" << "units=" << clean(p.units) << "\n" << "lang=" << clean(p.lang) << "\n";
    if (!p.shellPanels.empty()) o << "shellpanels=" << clean(p.shellPanels) << "\n";
    if (p.fps == "on") o << "fps=on\n";                                                   // 0.22 stage 6: only written when on
    if (p.facts != "on") o << "facts=" << clean(p.facts) << "\n";                       // 0.15 stage 4: only written when it is off, so a file from stage 3 is unchanged
    if (!p.recent.empty()) o << "recent=" << clean(p.recent) << "\n";
    if (p.atomMore) o << "atomMore=" << (*p.atomMore ? 1 : 0) << "\n";
    if (p.window) o << "window=" << p.window->w << "," << p.window->h << "," << (p.window->hasPos ? std::to_string(p.window->x) : "-") << ","
                    << (p.window->hasPos ? std::to_string(p.window->y) : "-") << "," << (p.window->maximized ? 1 : 0) << "\n";
    o << "lab=" << clean(p.lab) << "\n";
    if (!p.molecule.empty()) o << "molecule=" << clean(p.molecule) << "\n";
    for (const auto& [k, v] : p.orbit) o << "orbit." << clean(k) << "=" << clean(v) << "\n";
    for (const auto& [k, v] : p.atom) o << "atom." << clean(k) << "=" << clean(v) << "\n";
    for (const auto& [k, v] : p.blackhole) o << "blackhole." << clean(k) << "=" << clean(v) << "\n";   // 0.55.0
    if (!p.gpuPaths.empty()) o << "gpupaths=" << clean(p.gpuPaths) << "\n";
    if (!p.gpuSky.empty()) o << "gpusky=" << clean(p.gpuSky) << "\n";                  // 0.56.0            // 0.55.7 (only written when the guard has something to keep)
    if (!p.gpuSlice.empty()) o << "gpuslice=" << clean(p.gpuSlice) << "\n";            // 0.57.0
    if (!p.gpuJump.empty()) o << "gpujump=" << clean(p.gpuJump) << "\n";               // 0.57.0
    if (!p.gpuMolCloud.empty()) o << "gpumolcloud=" << clean(p.gpuMolCloud) << "\n";   // 0.57.1
    if (p.graphics != "vulkan") o << "graphics=" << clean(p.graphics) << "\n";        // 0.59.0 (0.63.0: Vulkan the default, OpenGL written)
    if (!p.gpuVulkan.empty()) o << "gpuvulkan=" << clean(p.gpuVulkan) << "\n";       // 0.59.0
    for (const auto& [name, line] : p.gpuVk) if (!line.empty()) o << "gpuvk" << name << "=" << clean(line) << "\n";   // 0.61.0
    for (std::size_t i = 0; i < p.calcHistory.size() && i < static_cast<std::size_t>(kCalcHistoryMax); i++) o << "calc." << i << "=" << clean(p.calcHistory[i]) << "\n";   // 0.49
    return o.str();
}

static const char* const kLabNames[kLabCount] = {"Orbit Lab", "Atom Lab", "Molecule Lab", "Black Hole Lab"};
static const char* const kLabKeys[kLabCount] = {"orbit", "atom", "molecule", "blackhole"};
const char* labName(int index) { return index >= 0 && index < kLabCount ? kLabNames[index] : kLabNames[0]; }
const char* labKey(int index) { return index >= 0 && index < kLabCount ? kLabKeys[index] : kLabKeys[0]; }
int labIndex(const std::string& name) {
    for (int i = 0; i < kLabCount; i++) if (name == kLabNames[i]) return i;
    return 0;
}

Prefs loadPrefs(const std::string& text) {
    Prefs p;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const std::size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = clean(line.substr(eq + 1));   // (0.19: a stray CR inside a value is dropped here as when saving, so the file reads back the same)
        if (k == "theme") { if (themeArchived(v)) p.theme = "dark"; else if (validTheme(v)) p.theme = v; }   // 0.15 stage 5m: an archived theme opens as Dark, once, no message
        else if (k == "textSize") { if (validTextSize(v)) p.textSize = v; }
        else if (k == "font") { if (validFont(v)) p.font = v; }
        else if (k == "motion") { if (validMotion(v)) p.motion = v; }
        else if (k == "shellpanels") p.shellPanels = v;
        else if (k == "molecule") p.molecule = v;
        else if (k == "fps") { if (v == "on" || v == "off") p.fps = v; }
        else if (k == "facts") { if (v == "on" || v == "off") p.facts = v; }
        else if (k == "recent") p.recent = v;
        else if (k == "gpupaths") p.gpuPaths = v;
        else if (k == "gpusky") p.gpuSky = v;                                  // 0.56.0                              // 0.55.7 (read by app/gpu_guard.h, which checks it)
        else if (k == "gpuslice") p.gpuSlice = v;                              // 0.57.0
        else if (k == "gpujump") p.gpuJump = v;                                // 0.57.0
        else if (k == "gpumolcloud") p.gpuMolCloud = v;                        // 0.57.1
        else if (k == "graphics") { if (v == "opengl" || v == "vulkan") p.graphics = v; }   // 0.59.0
        else if (k == "gpuvulkan") p.gpuVulkan = v;                            // 0.59.0
        else if (k.rfind("gpuvk", 0) == 0 && k.size() > 5) p.gpuVk[k.substr(5)] = v;   // 0.61.0
        else if (k.rfind("calc.", 0) == 0) {                                   // 0.49: the calculator's history, calc.0 the newest; a bad number or an empty line is ignored
            const std::string n = k.substr(5);
            if (n.size() == 1 && n[0] >= '0' && n[0] < '0' + kCalcHistoryMax && !v.empty()) {
                const std::size_t i = static_cast<std::size_t>(n[0] - '0');
                if (p.calcHistory.size() <= i) p.calcHistory.resize(i + 1);
                p.calcHistory[i] = v;
            }
        }
        else if (k == "units") { if (v == "metric" || v == "imperial") p.units = v; }
        else if (k == "lang") { if (v == "en") p.lang = v; }
        // (0.52.0: the "unlock=" and "ach.*" lines of older files are left unread, as every unknown line: the achievements and secret themes were removed)
        else if (k == "atomMore") { if (v == "1" || v == "0") p.atomMore = v == "1"; }
        else if (k == "window") {                                                  // w,h,x,y,max  (x and y are "-" when the place is not known)
            std::vector<std::string> f;
            std::size_t a = 0;
            while (a <= v.size()) { std::size_t b = v.find(',', a); if (b == std::string::npos) b = v.size(); f.push_back(v.substr(a, b - a)); a = b + 1; }
            auto num = [](const std::string& s, long long lo, long long hi, long long& out) {
                char* end = nullptr;
                const long long n = std::strtoll(s.c_str(), &end, 10);
                if (s.empty() || !end || *end || n < lo || n > hi) return false;
                out = n; return true;
            };
            long long w = 0, h = 0, x = 0, y = 0, m = 0;
            if (f.size() == 5 && num(f[0], 1, 30000, w) && num(f[1], 1, 30000, h) && num(f[4], 0, 1, m) && ((f[2] == "-" && f[3] == "-") || (num(f[2], -100000, 100000, x) && num(f[3], -100000, 100000, y)))) {
                WindowState ws;
                ws.w = std::max<int>(kMinWindowW, int(w)); ws.h = std::max<int>(kMinWindowH, int(h));
                ws.hasPos = f[2] != "-"; ws.x = int(x); ws.y = int(y); ws.maximized = m == 1;
                p.window = ws;
            }
        } else if (k == "lab") { if (labIndex(v) > 0 || v == kLabNames[0]) p.lab = v; }   // 0.19: a removed lab (Defense Lab) or an unknown name keeps the default
        else if (k.rfind("orbit.", 0) == 0) p.orbit[k.substr(6)] = v;
        else if (k.rfind("atom.", 0) == 0) p.atom[k.substr(5)] = v;
        else if (k.rfind("blackhole.", 0) == 0) p.blackhole[k.substr(10)] = v;   // 0.55.0 (an older app ignores these lines and, not knowing the lab's name, opens the Orbit Lab)
    }
    p.calcHistory.erase(std::remove(p.calcHistory.begin(), p.calcHistory.end(), std::string()), p.calcHistory.end());   // 0.49: no gaps (calc.0 and calc.3 only)
    return p;
}

bool writeSettingsFile(const std::string& path, const std::string& text) {
    // the whole text into a side file first; only a complete, closed file replaces the old one (a crash or a full disk never leaves a cut settings file)
    const std::string tmp = path + ".tmp";
    FILE* f = std::fopen(tmp.c_str(), "wb");
    if (!f) return false;
    const bool written = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    const bool closed = std::fclose(f) == 0;
    std::error_code ec;
    if (written && closed) std::filesystem::rename(tmp, path, ec);   // (replaces the old file, also on Windows: MoveFileEx with MOVEFILE_REPLACE_EXISTING)
    if (!written || !closed || ec) { std::remove(tmp.c_str()); return false; }
    return true;
}

WindowState fitWindow(WindowState s, const std::vector<ScreenRect>& screens) {
    s.w = std::max(s.w, kMinWindowW);
    s.h = std::max(s.h, kMinWindowH);
    int maxW = 0, maxH = 0;
    for (const ScreenRect& r : screens) { maxW = std::max(maxW, r.w); maxH = std::max(maxH, r.h); }
    if (maxW >= kMinWindowW && maxH >= kMinWindowH) { s.w = std::min(s.w, maxW); s.h = std::min(s.h, maxH); }   // never bigger than the biggest screen
    if (s.hasPos) {
        // the title bar (the top 40 units of the window) must show on some screen, at least 120 units wide
        bool seen = false;
        for (const ScreenRect& r : screens) {
            const int x0 = std::max(s.x, r.x), x1 = std::min(s.x + s.w, r.x + r.w), y0 = std::max(s.y, r.y), y1 = std::min(s.y + 40, r.y + r.h);
            if (x1 - x0 >= 120 && y1 - y0 >= 40) seen = true;
        }
        if (!seen) s.hasPos = false;                   // off every screen (a monitor was unplugged): centre it
    }
    return s;
}

Rgb hexRgb(const std::string& h) {
    std::string s = h.size() && h[0] == '#' ? h.substr(1) : h;
    if (s.size() == 3) s = std::string{s[0], s[0], s[1], s[1], s[2], s[2]};
    const unsigned long v = std::strtoul(s.c_str(), nullptr, 16);
    return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f};
}

ThemeColors themeColors(const std::string& id0, bool systemLight) {
    const std::string id = id0 == "auto" || !validTheme(id0) ? (systemLight ? "light" : "dark") : id0;
    if (const ThemeDef* t = findTheme(id)) {
        ThemeColors c{hexRgb(t->bg), hexRgb(t->fg), hexRgb(t->muted), hexRgb(t->card), hexRgb(t->line), hexRgb(t->accent), hexRgb(t->onacc), t->light, id, {}, {}, {}, {}, {}, {}, {}, {}};
        c.style = themeStyle(id);
        const ThemeStyle& s = c.style;
        if (s.accent.set) c.accent = s.accent.c;                                            // 0.14 stage 2: Light's readable accent
        // what used to be written into the drawing code: the same numbers as before unless the theme says otherwise
        c.accentFill = s.accentFill.set ? s.accentFill.c : c.accent;
        c.danger = s.danger.set ? s.danger.c : Rgb{0.90f, 0.35f, 0.30f};                  // error messages (Save image, the benchmark)
        c.dangerFill = s.dangerFill.set ? s.dangerFill.c : hexRgb("#d0453b");               // the red button of the reset pop-up
        c.onDanger = s.onDanger.set ? s.onDanger.c : Rgb{1, 1, 1};
        c.soft = s.soft.set ? s.soft.c : Rgb{120 / 255.f, 140 / 255.f, 200 / 255.f};        // the faint blue tint behind illustrations, bars and the Learn cards
        c.neutral = s.neutral.set ? s.neutral.c : Rgb{128 / 255.f, 128 / 255.f, 128 / 255.f};   // the grey of tracks, frames and boxes
        c.button = c.card;                                                                  // the face of a button (no theme has its own since 0.15 stage 5m)
        return c;
    }
    return themeColors("dark", false);
}

bool themeUsable(const Prefs&, const std::string& id) { return findTheme(id) != nullptr; }

}  // namespace app
