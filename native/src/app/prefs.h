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
#include <vector>

#include "app/theme_types.h"

namespace app {

// Where the window was: its size and (if known) its place, in the window system's own units, and whether it was maximised.
// w, h, x, y describe the window as it is when NOT maximised, so un-maximising goes back to it.
struct WindowState {
    int w = 0, h = 0;
    int x = 0, y = 0;
    bool hasPos = false;
    bool maximized = false;
    bool operator==(const WindowState& o) const { return w == o.w && h == o.h && x == o.x && y == o.y && hasPos == o.hasPos && maximized == o.maximized; }
};
constexpr int kMinWindowW = 800, kMinWindowH = 500;      // the safe minimum: the header, the tabs and the panel all fit
struct ScreenRect { int x, y, w, h; };                    // a screen (monitor) in the window system's units
// What to open the window with: the saved state with its size at least the safe minimum (and at most the biggest screen),
// and its place dropped (hasPos = false: centre it) if its title bar is not on any screen now (a monitor was unplugged).
WindowState fitWindow(WindowState s, const std::vector<ScreenRect>& screens);

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
    std::optional<WindowState> window;   // the window as it was last time (none: the default size, centred)
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
Rgb hexRgb(const std::string& hex);               // "#1e1e21" or "#fff"
// The colours of a theme and (0.14) its style. The six extra colours are what used to be written into drawing code:
//   accentFill  the accent when it is a FILL behind onacc text (chosen buttons, the selected tab); the accent itself is for text and marks
//   danger      an error message's colour;  dangerFill / onDanger  the red button and its text
//   soft        the faint tint behind illustrations and bars;  neutral  the grey of tracks and frames
struct ThemeColors {
    Rgb bg, fg, muted, card, line, accent, onacc; bool light; std::string id;
    Rgb accentFill{0, 0, 0}, danger{0, 0, 0}, dangerFill{0, 0, 0}, onDanger{0, 0, 0}, soft{0, 0, 0}, neutral{0, 0, 0};
    ThemeStyle style;
};
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

// ---- achievements and secret themes ----
bool grantAchievement(Prefs& p, const std::string& id, int64_t nowMs);   // true if it is new (the web's PLAch.grant)
bool unlockTheme(Prefs& p, const std::string& id, int64_t nowMs);        // also earns its achievement; true if the theme is new
std::string achievementDate(int64_t ms);         // the local date as the web's en-GB toLocaleDateString: "5 Sept 2026"

}  // namespace app
