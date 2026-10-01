#include "prefs.h"

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <vector>

#include "app_data.h"

namespace app {

namespace {

std::string clean(const std::string& v) {           // one line: no line breaks inside a value
    std::string o;
    for (char c : v) if (c != '\n' && c != '\r') o += c;
    return o;
}

}  // namespace

bool validTheme(const std::string& id) { for (const ThemeDef& t : kThemes) if (id == t.id) return true; return false; }
bool validTextSize(const std::string& k) { for (const SizeOpt& s : kTextSizes) if (k == s.key) return true; return false; }
bool validFont(const std::string& k) { for (const FontOpt& f : kFonts) if (k == f.key) return true; return false; }
bool validMotion(const std::string& k) { return k == "auto" || k == "on" || k == "off"; }
double textSizeFactor(const std::string& k) { for (const SizeOpt& s : kTextSizes) if (k == s.key) return s.factor; return 1; }

bool achievementInApp(const std::string& id) { return id != "blackhole"; }   // Point of No Return belongs to the Black Hole Lab
int achievementCount() { int n = 0; for (const AchDef& a : kAchievements) if (achievementInApp(a.id)) n++; return n; }

std::string savePrefs(const Prefs& p) {
    std::ostringstream o;
    o << "# Physics Lab (native) settings. Written by the app; safe to delete.\n";
    o << "theme=" << clean(p.theme) << "\n" << "textSize=" << clean(p.textSize) << "\n" << "font=" << clean(p.font) << "\n"
      << "motion=" << clean(p.motion) << "\n" << "units=" << clean(p.units) << "\n" << "lang=" << clean(p.lang) << "\n";
    std::string un;
    for (const std::string& u : p.unlocked) un += (un.empty() ? "" : ",") + clean(u);
    o << "unlock=" << un << "\n";
    for (const auto& [id, t] : p.achievements) o << "ach." << clean(id) << "=" << t << "\n";
    if (p.atomMore) o << "atomMore=" << (*p.atomMore ? 1 : 0) << "\n";
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
        else if (k == "lab") { if (v == "Orbit Lab" || v == "Atom Lab") p.lab = v; }
        else if (k.rfind("orbit.", 0) == 0) p.orbit[k.substr(6)] = v;
        else if (k.rfind("atom.", 0) == 0) p.atom[k.substr(5)] = v;
    }
    // secret themes unlocked before achievements existed count too (the web's start-up check, line 5280)
    for (const std::string& id : p.unlocked) if (!p.achievements.count(id)) p.achievements[id] = 0;
    // a secret theme that is not unlocked cannot be in use (as the web's start-up check)
    if (!themeUsable(p, p.theme)) p.theme = "auto";
    return p;
}

Rgb hexRgb(const std::string& h) {
    std::string s = h.size() && h[0] == '#' ? h.substr(1) : h;
    if (s.size() == 3) s = std::string{s[0], s[0], s[1], s[1], s[2], s[2]};
    const unsigned long v = std::strtoul(s.c_str(), nullptr, 16);
    return {((v >> 16) & 255) / 255.f, ((v >> 8) & 255) / 255.f, (v & 255) / 255.f};
}

ThemeColors themeColors(const std::string& id0, bool systemLight) {
    const std::string id = id0 == "auto" || !validTheme(id0) ? (systemLight ? "light" : "dark") : id0;
    for (const ThemeDef& t : kThemes)
        if (id == t.id)
            return {hexRgb(t.bg), hexRgb(t.fg), hexRgb(t.muted), hexRgb(t.card), hexRgb(t.line), hexRgb(t.accent), hexRgb(t.onacc), t.light, id};
    return themeColors("dark", false);
}

bool themeUsable(const Prefs& p, const std::string& id) {
    for (const ThemeDef& t : kThemes)
        if (id == t.id) return !t.secret || p.unlocked.count(id);
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

}  // namespace app
