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
// 0.14 stage 2: texture and decoration painted ON the page, under the glass and never over text: soft clouds and glass bubbles (Aero), film grain, halftone dots,
// grid lines and a light streak (Y2K). Every number is the MOST that pattern can move a pixel of the page towards its colour (0 = the pattern is not drawn), so the
// contrast tests can use the extreme: the page moved by the sum of them (capped at 1) towards `tone`, and by `streak` towards `streakColour`.
struct PageFx {
    Rgb tone{1, 1, 1};              // clouds, bubbles, grain, halftone and grid push the page towards this colour
    float clouds = 0, bubbles = 0, grain = 0, halftone = 0, grid = 0;
    float streak = 0;               // a soft swoosh of streakColour across the page
    Rgb streakColour{1, 1, 1};
    float gridStep = 40;            // points between grid lines
    float halftoneStep = 7;         // points between halftone dots
};

struct PageStyle {
    int kind = 0;
    int stops = 0;                  // 0 for a flat page, else 2 or 3
    GradientStop stop[3];
    float cx = 0.5f, cy = 0, rx = 1, ry = 1;
    PageFx fx;
};

// 0.14 stage 2: the title bar of a floating window (Vista-style glossy blue; Y2K silver or dark). Unset = Dear ImGui's flat bar, as before.
struct TitleStyle {
    bool set = false;
    Rgb top{0, 0, 0}, bottom{0, 0, 0};   // the bar's colour at its top and bottom edge
    Rgb text{1, 1, 1};                   // the title's colour
    float shine = 0, shineHeight = 0.5f; // a white band over the upper part of the bar: its opacity, and how far down it reaches (fraction of the bar)
    Rgb glow{1, 1, 1};                   // a thin line along the bottom edge of the bar (no text there) ...
    float glowAlpha = 0;                 // ... at this opacity
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
    // 0.14 stage 2
    TitleStyle title;
    Rgb rim{1, 1, 1};               // a thin bright line along the inside of a floating window's edge, so it reads over the dark picture ...
    float rimAlpha = 0;
    Rgb glow{1, 1, 1};              // ... and a soft outer glow round it
    float glowAlpha = 0, glowBlur = 0;
    float reflectAlpha = 0, reflectHeight = 0;   // a faint mirrored shine under a floating window and under cards (the "glossy floor"); height = fraction of the panel's height
    float bracketAlpha = 0, bracketSize = 0;     // corner brackets on panels and windows: opacity, arm length in points (Y2K)
    float hudAlpha = 0;                          // thin HUD rules along a panel's edges
    Rgb hud{1, 1, 1};                            // their colour, and the colour of the tiny spec label (full opacity: it is text)
    std::string spec;                            // the tiny monospace label where there is room, e.g. "SYS": the app adds its version (empty = none)
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
    // 0.14 stage 2
    float rimDark = 0;              // a thin rim round the face in its lower colour this far towards black, and then this far towards white (rimLight; both 0 = Dear ImGui's border)
    float rimLight = 0;
    float hoverGlow = 0;            // a hovered face gets a soft light line just inside its rim, this opaque
    float glint = 0;                // a hovered face gets a short slanted streak of light across it, this opaque (Y2K)
    float brush = 0;                // fine horizontal lines on the face, this opaque (brushed metal)
    Rgb glow{1, 1, 1};              // a chosen button / tab (the accent fill) gets a soft glow of this colour round it ...
    float glowAlpha = 0;            // ... this opaque
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
    // 0.14 stage 2
    OptRgb accent;                  // a readability override of the accent colour of the theme list (Light: the web's #2f6fed is 4.24:1 on the page, under 4.5)
    OptRgb button;                  // the colour of a button's face (unset = the card colour, as before): Aero's aqua jelly, Y2K's silver
    std::string titleFont;          // the key of a bundled font for window titles and the app name only ("" = the normal font): Y2K's wide font
};

}  // namespace app
