// Everything the app remembers between runs, in one text file in the app-data folder
// (Windows: %APPDATA%\PhysicsLab\PhysicsLab\settings.txt): the look (theme, text size, font, motion),
// units and language, and every setting of the labs.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
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

constexpr int kCalcHistoryMax = 8;
struct Prefs {
    std::string theme = "auto";          // a THEMES id
    std::string textSize = "normal";     // small / normal / large / xl
    std::string font = "auto";           // auto / serif / easy / round / mono
    std::string motion = "auto";         // auto / on (reduced) / off (full)
    std::string shellPanels;             // 0.15 stage 3: the new UI's floating panels, collapsed or open ("card=c,legend=o")
    std::string fps = "off";             // 0.22 stage 6: the fps counter on the picture, on / off (off by default; Settings → Performance)
    std::string facts = "on";            // 0.15 stage 4: Settings → Facts, on / off: the fact line at the bottom left of the picture (new UI)
    std::string recent;                  // 0.15 stage 4: the search's recent choices, newest first, ids joined by ";"
    std::string units = "metric";        // metric / imperial
    std::string lang = "en";
    std::optional<bool> atomMore;        // the Atom Lab's More / Less info choice
    std::optional<WindowState> window;   // the window as it was last time (none: the default size, centred)
    std::string lab = "Orbit Lab";       // the lab on screen
    std::string molecule;                // 0.18: the Molecule Lab's settings text (molecule::saveSettings, versioned; written only when set)
    std::map<std::string, std::string> orbit, atom;   // each lab's settings, as text (checked when they are used)
    std::map<std::string, std::string> blackhole;     // 0.55.0: the Black Hole Lab's settings (blackhole/lab.h saveSettings; written only once the lab was opened)
    std::vector<std::string> calcHistory;  // 0.49: the calculator's last lines, newest first (at most kCalcHistoryMax)
};

std::string savePrefs(const Prefs& p);            // the file's text
Prefs loadPrefs(const std::string& text);         // unknown or broken lines are ignored; bad choices fall back to the defaults
bool writeSettingsFile(const std::string& path, const std::string& text);   // 0.19: true only when every byte reached the disk

// the labs and their indexes (AppState::lab); a removed or unknown lab name gives the default lab, 0
constexpr int kLabCount = 4;   // 0.19: Orbit 0, Atom 1, Molecule 2 (the Defense Lab, index 2 in 0.17 and 0.18, was removed); 0.55.0: Black Hole 3
const char* labName(int index);
int labIndex(const std::string& name);
const char* labKey(int index);  // 0.55.0: "orbit", "atom", "molecule", "blackhole": the prefix of the lab's names in the settings file and the panel states (the default lab's for a bad index)

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
    Rgb accentFill{0, 0, 0}, danger{0, 0, 0}, dangerFill{0, 0, 0}, onDanger{0, 0, 0}, soft{0, 0, 0}, neutral{0, 0, 0}, button{0, 0, 0};   // button: the face of a button (0.14 stage 2; the card colour unless the theme says otherwise)
    ThemeStyle style;
};
// The colours of a theme. Auto follows Windows' light or dark setting.
ThemeColors themeColors(const std::string& id, bool systemLight);
// a theme the app has (0.52.0: no theme is secret any more)
bool themeUsable(const Prefs& p, const std::string& id);

}  // namespace app
