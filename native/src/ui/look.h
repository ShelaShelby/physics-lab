// The app's look, from Settings and the theme: colours for the panels and the pictures, text size,
// font family, square corners (Monochrome) and reduced motion. Applied to Dear ImGui every frame.
#pragma once

#include <string>

#include "app/prefs.h"
#include "imgui.h"

namespace ui {

struct Look {
    app::ThemeColors c;          // the theme's colours (Auto resolved to Light or Dark)
    float textScale = 1;         // Settings → Text size
    bool effects = true;         // Settings → Theme effects (0.14 stage 3): off = the style has no textures, glints, reflection, brackets or HUD (app::withoutEffects)
    bool reduced = false;        // Settings → Motion (Auto: Windows' "Animation effects" off)
    bool square = false;         // Monochrome: no rounded corners
    bool grayscale = false;      // Monochrome: the pictures in shades of grey
    bool synthGlow = false;      // Synthwave: a purple glow at the top of the window
    std::string fontKey = "auto";   // the font family in use (Paper's book font, Monochrome's monospace, or the chosen one)
};
const Look& look();

// Works out the look and applies it to Dear ImGui (colours, corners, font, text size). Call before NewFrame.
void updateLook(const app::Prefs& p, bool systemLight, bool systemReduced);
void invalidateLook();     // forget what was applied to Dear ImGui, so the next updateLook applies it all again
bool systemReducedMotion();          // Windows' "Show animations in Windows" is off

ImU32 col(const app::Rgb& c, float a = 1);
ImVec4 vec(const app::Rgb& c, float a = 1);
app::Rgb mix(const app::Rgb& a, const app::Rgb& b, float f);
float corner(float r);               // a corner radius (0 with Monochrome)
void pushOn();                       // a chosen option button: accent background, accent-text colour
void popOn();
// The web's #gift pop-up page (centred card, big icon, heading, paragraphs, a row of buttons), from its HTML.
// if (beginGift("##id")) { std::string clicked = giftPage(html); ...; ImGui::EndPopup(); }  giftPage returns the clicked button's id.
bool beginGift(const char* id);
std::string giftPage(const std::string& html);
void drawSynthGlow(ImDrawList* dl, ImVec2 a, ImVec2 b);   // Synthwave's glow behind a window's top

}  // namespace ui
