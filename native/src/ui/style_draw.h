// The widgets and window helpers every page calls. Since 0.28 only the plain path is left (the 0.14 glass, gloss and page textures were removed: no
// theme had used them since 0.15 stage 5m); each helper is the Dear ImGui call the pages made before 0.14, plus noteItem for the automatic checks.
#pragma once

#include "app/theme_style.h"
#include "imgui.h"

namespace ui {

const app::ThemeStyle& style();       // the style of the theme in use

// ---- panels ----
// Call right after ImGui::Begin(...) of a window, before its contents: paints the page gradient, the glass, the shine line and (floating windows) the shadow.
// A docked window is a panel of the main window; a floating one is a pop-up or a window that can sit over anything. Plain themes: does nothing.
float panelDecor(app::PanelKind kind, bool hug = false);   // returns how far (px) its shadow / glow reaches outside the window; hug (the new UI only, 0.15 stage 5c.1): at most app::shell::kHaloMaxPx
// Floating windows (Settings, About, the periodic table) have rounded corners in some themes: push before ImGui::Begin, pop after it.
// Docked panels are see-through on purpose (the page gradient is painted under them): push before their ImGui::Begin, pop right after it. Every other window is solid.
void pushFloatStyle();
void popFloatStyle();
// A card drawn by hand (the facts line, a Learn card): the card colour with its border, or glass with a shine. Plain themes: card colour + border, as before.
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

}  // namespace ui
