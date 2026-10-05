// 0.14 stage 1: the maths and data behind the richer theme styles and Frutiger Aero. No graphics, so every function here is tested
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

// ---- the page ----
Rgb pageGradientAt(const PageStyle& p, float t);                  // the colour at t (0 = top, 1 = bottom; for a radial page, 0 = centre, 1 = edge); the ends are held
Rgb pageColourAt(const PageStyle& p, const Rgb& flatBg, float t); // a flat page is flatBg everywhere

// ---- panels ----
enum class PanelKind { Docked, Floating };
// The colour a panel really shows, where `under` is what is behind it (for a docked panel the page at that height). A plain theme: Docked is its
// bg colour, Floating its card colour, whatever is behind (they are solid). A glass panel: its glass colour at its alpha over `under`.
Rgb panelSurface(const ThemeColors& c, PanelKind kind, const Rgb& under);
// 0.14 stage 1.1: what a window or pop-up that holds text really shows, whatever is behind it (`backdrop` is ignored on purpose: nothing may show through).
// A plain theme: a floating window is its bg colour, a pop-up its card colour. A glass theme: the window paints the page (opaque) and the glass over it, so the
// stack is solid; a pop-up list is one solid colour (the glass over the page's middle colour). `t` is the place on the page, 0 top .. 1 bottom.
Rgb floatingSurface(const ThemeColors& c, const Rgb& backdrop, float t, bool popup);
Rgb dockedSurface(const ThemeColors& c, float t);                  // a docked panel: its glass over the page at height t
float floatingOpacity(const ThemeStyle& s);                         // how solid a floating window is as drawn (1 = hides everything behind it)
float popupOpacity(const ThemeStyle& s);                            // the same for a pop-up list
// true when the style does nothing new: flat page, solid panels without shine or shadow, flat widgets, scale 1
bool isPlainStyle(const ThemeStyle& s);
bool isPlainWidgetStyle(const WidgetStyle& w);                     // no gloss, no tints other than the old 0.22 / 0.45
bool isPlainPanelStyle(const PanelStyle& p, const PageStyle& page);  // flat page, solid panels, no shine, no shadow
float scaledRadius(const ThemeStyle& s, float r);                  // 0 for square themes, else r times the theme's corner scale

// ---- widgets: the four colours of a glossy face, top to bottom (the face splits into an upper and a lower half at WidgetStyle::split) ----
enum class WidgetState { Normal, Hover, Pressed };
struct Gloss { Rgb top0, top1, bot0, bot1; };                      // top edge, just above the split, just below it, bottom edge
Gloss glossColours(const WidgetStyle& w, const Rgb& base, WidgetState state);

// ---- fonts: a system font by file name, with a fallback ----
// The path of `file` in the first of `dirs` (each ends with a folder separator) where exists() says it is there; "" if none (or no name).
std::string resolveFontFile(const std::string& file, const std::vector<std::string>& dirs, const std::function<bool(const std::string&)>& exists);
std::string fontFamilyKeyOf(const ThemeStyle& s);                  // the family key of a theme's own system font: its regular file without ".ttf" ("segoeui"), or ""
// The family to use: Settings' choice if it is not Auto; else the theme's own font family key if one was found on this PC (themeFontFound); else the theme's
// default family (Paper: serif, Monochrome: mono); else "auto". A family that does not exist falls back to "auto" in ui/fonts.cpp as well.
std::string fontKeyFor(const ThemeStyle& s, const std::string& userFont, bool themeFontFound);

// ---- 0.14 stage 2: Aero's real look and the Y2K themes ----
// The colour of a title bar at y (0 top .. 1 bottom): the gradient from top to bottom, with the white shine band over its upper part (what ui/style_draw.cpp paints).
Rgb titleBarAt(const TitleStyle& t, float y);
float pageFxTotal(const PageFx& fx);                                // clouds + bubbles + grain + halftone + grid, capped at 1: the most the page moves towards `tone`
// The extremes the page's textures can make of a page colour: the page itself, moved fully towards `tone`, towards the streak colour, and both. A text colour that
// reads on all four reads on the page whatever the textures do (they are drawn under the glass and never over text).
std::vector<Rgb> pageFxExtremes(const PageFx& fx, const Rgb& page);
bool hasStage2Decor(const ThemeStyle& s);                           // any stage 2 field set: textures, title bar, rim, glow, reflection, brackets, HUD, spec label, rim / glint / brush / glow of widgets, a button colour, a title font
// The bundled font for titles: the theme's, if it is built into the app (`available`); else "" (the titles use the normal font).
std::string titleFontKeyFor(const ThemeStyle& s, bool available);

// ---- the list of themes in the app: the web's 11 plus the native ones, in the order of the picker ----
// Order: the web's themes that are not secret, then the native themes (Frutiger Aero), then the secret ones (so the "???" cards stay at the end).
const std::vector<const ThemeDef*>& allThemes();
const ThemeDef* findTheme(const std::string& id);
const ThemeStyle& themeStyle(const std::string& id);               // the style of a theme (plain defaults for those that list none)

}  // namespace app
