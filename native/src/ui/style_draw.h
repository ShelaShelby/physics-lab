// 0.14 stage 1: drawing the richer theme styles (a gradient page, glass panels with a shine and a soft shadow, glossy buttons / tabs / check boxes /
// sliders). One helper per kind of thing; each reads only look().c.style (data) and does NOTHING when the theme is plain, so the themes that existed
// before 0.14 are drawn by exactly the same Dear ImGui calls as before. The colours come from app/theme_style.* (tested); this file only paints them.
#pragma once

#include "app/theme_style.h"
#include "imgui.h"

namespace ui {

const app::ThemeStyle& style();       // the style of the theme in use
bool glossy();                        // the buttons, tabs, check boxes and sliders have a gloss
bool glassy();                        // the panels have a gradient page, glass, a shine or a shadow behind them

// ---- panels ----
// Call right after ImGui::Begin(...) of a window, before its contents: paints the page gradient, the glass, the shine line and (floating windows) the shadow.
// A docked window is a panel of the main window; a floating one is a pop-up or a window that can sit over anything. Plain themes: does nothing.
float panelDecor(app::PanelKind kind, bool hug = false);   // returns how far (px) its shadow / glow reaches outside the window; hug (the new UI only, 0.15 stage 5c.1): at most app::shell::kHaloMaxPx
// Floating windows (Settings, About, the periodic table) have rounded corners in some themes: push before ImGui::Begin, pop after it.
// Docked panels are see-through on purpose (the page gradient is painted under them): push before their ImGui::Begin, pop right after it. Every other window is solid.
void pushDockedBg();
void popDockedBg();
void pushFloatStyle();
void popFloatStyle();
// A card drawn by hand (the facts line, an achievement): the card colour with its border, or glass with a shine. Plain themes: card colour + border, as before.
void cardRect(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, const app::Rgb& border, float alpha = 1, bool bordered = true);

// ---- widget faces ----
// A button-like face from a to b. Plain themes: one flat rounded rectangle of `base`. Glossy ones: the two-tone gloss for `state`.
void faceRect(ImDrawList* dl, ImVec2 a, ImVec2 b, const app::Rgb& base, float alpha, app::WidgetState state, float rounding, ImDrawFlags flags = 0);

// ---- widgets: Dear ImGui's own when the theme is plain ----
bool Button(const char* label, const ImVec2& size = ImVec2(0, 0));
bool Checkbox(const char* label, bool* v);
bool SliderFloat(const char* label, float* v, float lo, float hi, const char* format = "%.3f");
bool SliderInt(const char* label, int* v, int lo, int hi, const char* format = "%d");

// ---- 0.14 stage 2: tooltips, combo boxes and their lists: Dear ImGui's own when the theme is plain; else a glossy face and a frosted, rimmed list ----
// Use these instead of ImGui::BeginTooltip / SetTooltip / BeginCombo / Combo (tests/check_windows_decorated.py checks it). End with ImGui::EndTooltip / EndCombo as before.
bool BeginTooltip();
void SetTooltip(const char* fmt, ...) IM_FMTARGS(1);
bool BeginCombo(const char* label, const char* preview, ImGuiComboFlags flags = 0);
bool Combo(const char* label, int* current, const char* items_separated_by_zeros);

void logTitleRect(const char* what, ImVec2 a, ImVec2 b);   // PL_LOG_WINDOWS only: writes where a title text is, for tests/check_y2k_pictures.py

// ---- the page behind everything ----
// Paints the page gradient into a..b (the window rectangle) as it is at that place on the main window. Used by panelDecor; exposed for the test pictures.
void pageGradient(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, ImDrawFlags flags);
// 0.15 stage 2: draws the piece a..b of the cached page (gradient + textures of the whole window, made once). False: not used (plain theme, effects off, first frame, PL_PAGE_CACHE=off).
bool pageCacheDraw(ImDrawList* dl, ImVec2 a, ImVec2 b);

}  // namespace ui
