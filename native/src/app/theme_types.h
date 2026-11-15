// The plain data of a theme's STYLE (0.14 stage 1): everything beyond the 7 flat colours. No graphics, so it can be tested.
// Every field has a default that reproduces the look themes had before 0.14 (flat page, solid panels, flat widgets, the
// radii written in the drawing code), so a theme only lists what it changes. See theme_style.h for the functions.
#pragma once

#include <string>

namespace app {

struct Rgb { float r, g, b; };

struct OptRgb { bool set = false; Rgb c{0, 0, 0}; };   // a colour a theme may or may not have

// Panels and cards. 0.29: only the fields a theme still uses; the glass, gloss, title-bar and page-texture fields of the archived
// Frutiger Aero and Y2K themes were removed with their data (0.28 removed the code that drew them).
struct PanelStyle {
    float borderFgMix = 0.25f;      // the border colour is the theme's line colour mixed with its text colour by this much
    float borderWidth = 1;          // points
    float floatRounding = 0;        // the corner radius of floating windows (Settings, About, the periodic table), points; docked panels stay square
};

// Buttons, tabs, check boxes and slider grabs.
struct WidgetStyle {
    float hoverTint = 0.22f;        // a hovered button's colour is the card colour this far towards the accent (0.22 and 0.45 were fixed in ui/look.cpp before 0.14)
    float pressTint = 0.45f;        // a pressed one's, this far
};

struct ThemeStyle {
    PanelStyle panel;
    WidgetStyle widget;
    float roundScale = 1;           // every corner radius in the app is multiplied by this (Monochrome: square instead)
    bool square = false;            // Monochrome: no rounded corners
    bool grayscale = false;         // Monochrome: the pictures in shades of grey
    std::string font;               // the theme's own font family key when Settings → Font is Auto ("" = Auto): "serif", "mono" ...
    std::string fontRegular, fontBold, fontItalic;   // a system font by file name (looked for in the system's font folders; "" = none)
    OptRgb accentFill, danger, dangerFill, onDanger, soft, neutral;   // see ThemeColors: unset = the old value
    // 0.14 stage 2
    OptRgb accent;                  // a readability override of the accent colour of the theme list
};

}  // namespace app
