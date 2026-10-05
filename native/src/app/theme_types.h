// The plain data of a theme's STYLE (0.14 stage 1): everything beyond the 7 flat colours. No graphics, so it can be tested.
// Every field has a default that reproduces the look themes had before 0.14 (flat page, solid panels, flat widgets, the
// radii written in the drawing code), so a theme only lists what it changes. See theme_style.h for the functions.
#pragma once

#include <string>

namespace app {

struct Rgb { float r, g, b; };

struct OptRgb { bool set = false; Rgb c{0, 0, 0}; };   // a colour a theme may or may not have

struct GradientStop { Rgb c{0, 0, 0}; float pos = 0; };  // pos 0..1, rising

// The page behind everything. kind 0: flat (the theme's bg colour). kind 1: a vertical gradient from the top of the window to its
// bottom. kind 2: a radial gradient (ellipse centred at cx, cy, as fractions of the window, radii rx, ry as fractions of its width and height).
struct PageStyle {
    int kind = 0;
    int stops = 0;                  // 0 for a flat page, else 2 or 3
    GradientStop stop[3];
    float cx = 0.5f, cy = 0, rx = 1, ry = 1;
};

// Panels and cards: Dear ImGui windows (the header, the side panels, the Learn pages), floating windows (Settings, pop-ups, toasts).
struct PanelStyle {
    float fillAlpha = 1;            // docked panels: 1 = solid (as before), less = glass that shows the page behind it
    float floatAlpha = 1;           // floating windows and pop-ups
    Rgb fill{1, 1, 1};              // the glass colour (only used when fillAlpha or floatAlpha is below 1)
    float shineAlpha = 0;           // a white band at the top of a panel: its strength at the very top, fading to 0 ...
    float shineHeight = 0;          // ... at this fraction of the panel's height
    float shineLineAlpha = 0;       // a thin bright line along the inside of the top edge
    float borderFgMix = 0.25f;      // the border colour is the theme's line colour mixed with its text colour by this much
    float borderWidth = 1;          // points
    float floatRounding = 0;        // the corner radius of floating windows (Settings, About, the periodic table), points; docked panels stay square
    float shadowDx = 0, shadowDy = 0, shadowBlur = 0, shadowAlpha = 0;   // a soft shadow under floating windows (points); alpha 0 = none
    Rgb shadow{0, 0, 0};
};

// Buttons, tabs, check boxes and slider grabs. All fractions 0..1; all 0 = flat, as before.
struct WidgetStyle {
    float hi = 0;                   // the top of the face is this far from its colour towards white
    float split = 0;                // where the gloss turns over, as a fraction of the button's height (0 = no gloss)
    float loAmount = 0;             // the lower half is this far from its colour towards (colour x lo)
    Rgb lo{1, 1, 1};
    float shineLineAlpha = 0;       // a thin bright line along the inside of the top edge
    float pressDark = 0;            // a pressed button's top is this much darker (towards black)
    float hoverTint = 0.22f;        // a hovered button's colour is the card colour this far towards the accent (0.22 and 0.45 were fixed in ui/look.cpp before 0.14)
    float pressTint = 0.45f;        // a pressed one's, this far
};

struct ThemeStyle {
    PageStyle page;
    PanelStyle panel;
    WidgetStyle widget;
    float roundScale = 1;           // every corner radius in the app is multiplied by this (Monochrome: square instead)
    bool square = false;            // Monochrome: no rounded corners
    bool grayscale = false;         // Monochrome: the pictures in shades of grey
    OptRgb glowTop;                 // Synthwave: the purple glow behind the top of the page (the colour at its middle)
    OptRgb titleGlow;               // Synthwave: the neon glow round the title
    std::string font;               // the theme's own font family key when Settings → Font is Auto ("" = Auto): "serif", "mono" ...
    std::string fontRegular, fontBold, fontItalic;   // a system font by file name (looked for in the system's font folders; "" = none)
    OptRgb accentFill, danger, dangerFill, onDanger, soft, neutral;   // see ThemeColors: unset = the old value
};

}  // namespace app
