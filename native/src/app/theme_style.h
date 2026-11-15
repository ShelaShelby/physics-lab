// 0.14 stage 1: the maths and data behind the richer theme styles (Frutiger Aero, the first user, was archived in 0.15 stage 5m; 0.28 removed the glass, gloss and page-texture maths no theme used). No graphics, so every function here is tested
// (tests/test_theme_style.inc). The drawing itself is in ui/style_draw.cpp and only ever calls these.
#pragma once

#include <functional>
#include <string>
#include <vector>

#include "app/app_data.h"
#include "app/prefs.h"

namespace app {

// ---- contrast: the W3C's WCAG 2.x formula ----
// https://www.w3.org/TR/WCAG21/#dfn-relative-luminance :  L = 0.2126 R + 0.7152 G + 0.0722 B,  with each of R, G, B (the sRGB value c, 0..1)
// made linear: c / 12.92 if c <= 0.04045, else ((c + 0.055) / 1.055)^2.4.
// https://www.w3.org/TR/WCAG21/#dfn-contrast-ratio :  (L1 + 0.05) / (L2 + 0.05), L1 the lighter colour, L2 the darker. Needs: body text 4.5 (1.4.3), large text and
// parts of the user interface 3 (1.4.3, 1.4.11).
double relativeLuminance(const Rgb& c);
double contrastRatio(const Rgb& a, const Rgb& b);

Rgb mixRgb(const Rgb& a, const Rgb& b, float f);                 // a + (b - a) f, in sRGB values, as the drawing does
Rgb over(const Rgb& top, float alpha, const Rgb& under);         // `top` at opacity `alpha` laid over `under`

// ---- panels ----
enum class PanelKind { Docked, Floating };
// 0.14 stage 1.1: what a window or pop-up that holds text really shows, whatever is behind it (`backdrop` is ignored on purpose: nothing may show through).
// Every theme is plain since 0.15 stage 5m (0.28 removed the glass code): a floating window is its bg colour, a pop-up its card colour.
Rgb floatingSurface(const ThemeColors& c, const Rgb& backdrop, float t, bool popup);
// true when the style does nothing new: the default border, hover / press tints and corner scale
bool isPlainStyle(const ThemeStyle& s);
float scaledRadius(const ThemeStyle& s, float r);                  // 0 for square themes, else r times the theme's corner scale

// ---- widgets ----
enum class WidgetState { Normal, Hover, Pressed };

// ---- fonts: a system font by file name, with a fallback ----
// The path of `file` in the first of `dirs` (each ends with a folder separator) where exists() says it is there; "" if none (or no name).
std::string resolveFontFile(const std::string& file, const std::vector<std::string>& dirs, const std::function<bool(const std::string&)>& exists);
std::string fontFamilyKeyOf(const ThemeStyle& s);                  // the family key of a theme's own system font: its regular file without ".ttf" ("segoeui"), or ""
// The family to use: Settings' choice if it is not Auto; else the theme's own font family key if one was found on this PC (themeFontFound); else the theme's
// default family (Paper: serif, Monochrome: mono); else "auto". A family that does not exist falls back to "auto" in ui/fonts.cpp as well.
std::string fontKeyFor(const ThemeStyle& s, const std::string& userFont, bool themeFontFound);

// ---- the list of themes in the app, in the order of the picker ----
// Order: as in kThemes (app_text.inc); Monochrome, once a secret theme, is an ordinary one since 0.52.0 and stays last.
// and the app had Frutiger Aero and Y2K Night; those three were archived (git history (archive removed in 0.22)), so allThemes() skips every id for which themeArchived is true.
bool themeArchived(const std::string& id);                          // aero, y2knight, synthwave (and y2kice, removed in 0.15 stage 5g): a settings file that names one opens as Dark
const std::vector<const ThemeDef*>& allThemes();
const ThemeDef* findTheme(const std::string& id);
const ThemeStyle& themeStyle(const std::string& id);               // the style of a theme (plain defaults for those that list none)

}  // namespace app
