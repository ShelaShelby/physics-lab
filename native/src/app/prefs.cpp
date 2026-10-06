#include <algorithm>
#include <vector>
#include "prefs.h"

#include <cstdlib>
#include <ctime>
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
bool validEffects(const std::string& k) { return k == "on" || k == "off"; }
bool validNewUi(const std::string& k) { return k == "on" || k == "off"; }
bool validMotion(const std::string& k) { return k == "auto" || k == "on" || k == "off"; }
double textSizeFactor(const std::string& k) { for (const SizeOpt& s : kTextSizes) if (k == s.key) return s.factor; return 1; }

bool achievementInApp(const std::string& id) { return id != "blackhole"; }   // Point of No Return belongs to the Black Hole Lab
int achievementCount() { int n = 0; for (const AchDef& a : kAchievements) if (achievementInApp(a.id)) n++; return n; }

std::string savePrefs(const Prefs& p) {
    std::ostringstream o;
    o << "# Physics Lab (native) settings. Written by the app; safe to delete.\n";
    o << "theme=" << clean(p.theme) << "\n" << "textSize=" << clean(p.textSize) << "\n" << "font=" << clean(p.font) << "\n"
      << "motion=" << clean(p.motion) << "\n" << "effects=" << clean(p.effects) << "\n" << "units=" << clean(p.units) << "\n" << "lang=" << clean(p.lang) << "\n";
    if (p.newUi != "off") o << "newui=" << clean(p.newUi) << "\n";                      // 0.15 stage 3: only written when it is on, so a file from before it is unchanged
    if (!p.shellPanels.empty()) o << "shellpanels=" << clean(p.shellPanels) << "\n";
    if (p.facts != "on") o << "facts=" << clean(p.facts) << "\n";                       // 0.15 stage 4: only written when it is off, so a file from stage 3 is unchanged
    if (!p.recent.empty()) o << "recent=" << clean(p.recent) << "\n";
    std::string un;
    for (const std::string& u : p.unlocked) un += (un.empty() ? "" : ",") + clean(u);
    o << "unlock=" << un << "\n";
    for (const auto& [id, t] : p.achievements) o << "ach." << clean(id) << "=" << t << "\n";
    if (p.atomMore) o << "atomMore=" << (*p.atomMore ? 1 : 0) << "\n";
    if (p.window) o << "window=" << p.window->w << "," << p.window->h << "," << (p.window->hasPos ? std::to_string(p.window->x) : "-") << ","
                    << (p.window->hasPos ? std::to_string(p.window->y) : "-") << "," << (p.window->maximized ? 1 : 0) << "\n";
    o << "lab=" << clean(p.lab) << "\n";
    for (const auto& [k, v] : p.orbit) o << "orbit." << clean(k) << "=" << clean(v) << "\n";
    for (const auto& [k, v] : p.atom) o << "atom." << clean(k) << "=" << clean(v) << "\n";
    return o.str();
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
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "theme") { if (validTheme(v)) p.theme = v; }
        else if (k == "textSize") { if (validTextSize(v)) p.textSize = v; }
        else if (k == "font") { if (validFont(v)) p.font = v; }
        else if (k == "motion") { if (validMotion(v)) p.motion = v; }
        else if (k == "effects") { if (validEffects(v)) p.effects = v; }
        else if (k == "newui") { if (validNewUi(v)) p.newUi = v; }
        else if (k == "shellpanels") p.shellPanels = v;
        else if (k == "facts") { if (v == "on" || v == "off") p.facts = v; }
        else if (k == "recent") p.recent = v;
        else if (k == "units") { if (v == "metric" || v == "imperial") p.units = v; }
        else if (k == "lang") { if (v == "en") p.lang = v; }                     // as the web: only English can be picked yet
        else if (k == "unlock") {
            std::size_t a = 0;
            while (a <= v.size()) {
                std::size_t b = v.find(',', a);
                if (b == std::string::npos) b = v.size();
                const std::string id = v.substr(a, b - a);
                if (id == "synthwave" || id == "monochrome") p.unlocked.insert(id);
                a = b + 1;
            }
        } else if (k.rfind("ach.", 0) == 0) {
            const std::string id = k.substr(4);
            bool known = false;
            for (const AchDef& d : kAchievements) if (id == d.id) known = true;
            char* end = nullptr;
            const long long t = std::strtoll(v.c_str(), &end, 10);
            if (known && end && *end == 0 && !v.empty()) p.achievements[id] = t;
        } else if (k == "atomMore") { if (v == "1" || v == "0") p.atomMore = v == "1"; }
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
        } else if (k == "lab") { if (v == "Orbit Lab" || v == "Atom Lab") p.lab = v; }
        else if (k.rfind("orbit.", 0) == 0) p.orbit[k.substr(6)] = v;
        else if (k.rfind("atom.", 0) == 0) p.atom[k.substr(5)] = v;
    }
    // secret themes unlocked before achievements existed count too (the web's start-up check, line 5280)
    for (const std::string& id : p.unlocked) if (!p.achievements.count(id)) p.achievements[id] = 0;
    // a secret theme that is not unlocked cannot be in use (as the web's start-up check)
    if (p.theme == "y2kice") p.theme = "dark";                        // 0.15 stage 5g: the Y2K Ice theme was removed; a settings file that names it gets Dark
    if (!themeUsable(p, p.theme)) p.theme = "auto";
    return p;
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
        if (s.accent.set) c.accent = s.accent.c;                                            // 0.14 stage 2: Light's readable accent (the web's is 4.24:1 on its page)
        // what used to be written into the drawing code: the same numbers as before unless the theme says otherwise
        c.accentFill = s.accentFill.set ? s.accentFill.c : c.accent;
        c.danger = s.danger.set ? s.danger.c : Rgb{0.90f, 0.35f, 0.30f};                  // error messages (Save image, the benchmark)
        c.dangerFill = s.dangerFill.set ? s.dangerFill.c : hexRgb("#d0453b");               // the red button of the reset pop-up
        c.onDanger = s.onDanger.set ? s.onDanger.c : Rgb{1, 1, 1};
        c.soft = s.soft.set ? s.soft.c : Rgb{120 / 255.f, 140 / 255.f, 200 / 255.f};        // the faint blue tint behind illustrations, bars and the Learn cards
        c.neutral = s.neutral.set ? s.neutral.c : Rgb{128 / 255.f, 128 / 255.f, 128 / 255.f};   // the grey of tracks, frames and boxes
        c.button = s.button.set ? s.button.c : c.card;                                      // the face of a button (0.14 stage 2: Aero's aqua jelly, Y2K's silver)
        return c;
    }
    return themeColors("dark", false);
}

bool themeUsable(const Prefs& p, const std::string& id) {
    if (const ThemeDef* t = findTheme(id)) return !t->secret || p.unlocked.count(id);
    return false;
}

int unlockedThemeCount(const app::Prefs& p) { return int(p.unlocked.count("synthwave") + p.unlocked.count("monochrome")); }
int earnedAchievementCount(const app::Prefs& p) {
    int n = 0;
    for (const auto& [id, t] : p.achievements) if (app::achievementInApp(id)) n++;
    return n;
}

// lock the secret themes and achievements again (the web's PLTheme.reset)
void resetProgress(app::Prefs& p) {
    p.unlocked.clear();
    p.achievements.clear();
    if (p.theme == "synthwave" || p.theme == "monochrome") p.theme = "auto";
}

// unlock every secret theme and achievement, silently; returns {themes, achievements} newly unlocked
std::pair<int, int> unlockEverything(app::Prefs& p) {
    int t = 0, a = 0;
    for (const app::AchDef& d : app::kAchievements)
        if (app::achievementInApp(d.id) && !p.achievements.count(d.id)) { p.achievements[d.id] = 0; a++; }
    for (const char* id : {"synthwave", "monochrome"})
        if (!p.unlocked.count(id)) { p.unlocked.insert(id); t++; }
    return {t, a};
}

bool grantAchievement(Prefs& p, const std::string& id, int64_t nowMs) {
    bool known = false;
    for (const AchDef& d : kAchievements) if (id == d.id) known = true;
    if (!known || p.achievements.count(id)) return false;
    p.achievements[id] = nowMs;
    return true;
}

bool unlockTheme(Prefs& p, const std::string& id, int64_t nowMs) {
    grantAchievement(p, id, nowMs);
    return p.unlocked.insert(id).second;
}

std::string achievementDate(int64_t ms) {
    // en-GB short months (as Chrome and Node write them: September is "Sept")
    static const char* const kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sept", "Oct", "Nov", "Dec"};
    const std::time_t t = static_cast<std::time_t>(ms / 1000);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return std::to_string(tm.tm_mday) + " " + kMonths[tm.tm_mon] + " " + std::to_string(tm.tm_year + 1900);
}

}  // namespace app
