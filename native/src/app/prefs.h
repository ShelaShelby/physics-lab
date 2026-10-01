// Everything the app remembers between runs, in one text file in the app-data folder
// (Windows: %APPDATA%\PhysicsLab\PhysicsLab\settings.txt): the look (theme, text size, font, motion),
// units and language, the unlocked secret themes and achievements, and every setting of both labs.
// The web keeps the same things in the browser (plTheme, plTextSize, plFont, plMotion, plUnits,
// plLang, plUnlock, plAch, plAtomMore and the per-lab settings of physics-lab-v1).
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>

namespace app {

struct Prefs {
    std::string theme = "auto";          // a THEMES id
    std::string textSize = "normal";     // small / normal / large / xl
    std::string font = "auto";           // auto / serif / easy / round / mono
    std::string motion = "auto";         // auto / on (reduced) / off (full)
    std::string units = "metric";        // metric / imperial
    std::string lang = "en";
    std::set<std::string> unlocked;      // secret themes: synthwave, monochrome
    std::map<std::string, int64_t> achievements;   // id -> when it was earned (Unix ms; 0 = earlier, or unlocked for testing)
    std::optional<bool> atomMore;        // the Atom Lab's More / Less info choice
    std::string lab = "Orbit Lab";       // the lab on screen
    std::map<std::string, std::string> orbit, atom;   // each lab's settings, as text (checked when they are used)
};

std::string savePrefs(const Prefs& p);            // the file's text
Prefs loadPrefs(const std::string& text);         // unknown or broken lines are ignored; bad choices fall back to the defaults

// the choices the web allows for each setting
bool validTheme(const std::string& id);
bool validTextSize(const std::string& k);
bool validFont(const std::string& k);
bool validMotion(const std::string& k);
double textSizeFactor(const std::string& k);      // 0.9, 1, 1.15, 1.3

// ---- themes ----
struct Rgb { float r, g, b; };
Rgb hexRgb(const std::string& hex);               // "#1e1e21" or "#fff"
struct ThemeColors { Rgb bg, fg, muted, card, line, accent, onacc; bool light; std::string id; };
// The colours of a theme. Auto follows Windows' light or dark setting (the web's device setting).
ThemeColors themeColors(const std::string& id, bool systemLight);
// a secret theme can only be used once it is unlocked (as the web's pick())
bool themeUsable(const Prefs& p, const std::string& id);

// ---- achievements ----
int achievementCount();                           // the ones in the native app (the web's, without Point of No Return)
bool achievementInApp(const std::string& id);

// ---- progress (Settings → Progress and Developer) ----
int unlockedThemeCount(const Prefs& p);
int earnedAchievementCount(const Prefs& p);          // the ones in the native app
void resetProgress(Prefs& p);                        // lock them all again (the web's PLTheme.reset)
std::pair<int, int> unlockEverything(Prefs& p);      // {themes, achievements} newly unlocked

}  // namespace app
