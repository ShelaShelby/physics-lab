// The widgets and window helpers every page calls (Button, Checkbox, sliders, tooltips, combo boxes, floating windows, hand-drawn cards). Until 0.28
// this file also painted the richer theme styles of 0.14 (gradient page, glass panels, gloss, page textures and their cache); no theme has used them
// since the Frutiger Aero and Y2K themes were archived (0.15 stage 5m: every shipped theme is plain, tests/test_theme_style.inc checks it), so 0.28
// removed that code (git history has it). What is left is exactly the plain path: the same Dear ImGui calls as before.
#include "style_draw.h"
#include "app/look_options.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "fonts.h"
#include "imgui_internal.h"
#include "look.h"
#include "reading.h"
#include "shell.h"
#include "shell_test.h"

namespace ui {

using app::PanelKind;
using app::Rgb;
using app::WidgetState;

const app::ThemeStyle& style() { return look().c.style; }

float panelDecor(PanelKind kind, bool hug) {
    static const bool logWindows = std::getenv("PL_LOG_WINDOWS") != nullptr;   // for automatic checks: where every painted window is (tests/check_window_pictures.py)
    if (logWindows) {
        const ImGuiWindow* lw = ImGui::GetCurrentWindow();
        std::fprintf(stderr, "PLWIN %s %d %d %d %d %s\n", lw->Name, int(lw->Pos.x), int(lw->Pos.y), int(lw->Size.x), int(lw->Size.y), kind == PanelKind::Docked ? "docked" : "floating");
        if (lw->TitleBarHeight > 0) std::fprintf(stderr, "PLTITLEBAR %s %d %d %d %d %.3f\n", lw->Name, int(lw->Pos.x), int(lw->Pos.y), int(lw->Size.x), int(lw->TitleBarHeight), lw->TitleBarHeight);
    }
    (void)hug;
    return 0;
}

// the title bar of a window with a styled bar has its own text colour; and the title (not the content) may be in the theme's display font
// 0.15 stage 5k (New UI only; with it Off every floating window is exactly as before): a floating window is never taller than the room under the top bar, and on opening and after the app window
// is resized it is moved inside it (title bar reachable, bottom edge 8 px above the window's bottom edge). A taller window scrolls inside.
static const float kKeepMargin = 8.0f;
static float keepTop() { return std::max(0.0f, shellTopBarH()); }
static void keepInside() {
    if (!readingModeOn()) return;
    ImGuiWindow* w = ImGui::GetCurrentWindow();
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGuiStorage* st = ImGui::GetStateStorage();
    const ImGuiID kW = ImGui::GetID("##keepW"), kH = ImGui::GetID("##keepH"), kLeft = ImGui::GetID("##keepLeft");
    int left = st->GetInt(kLeft, 0);
    if (w->Appearing || st->GetFloat(kW, -1) != vp->Size.x || st->GetFloat(kH, -1) != vp->Size.y) left = 5;     // a few frames: an auto-sized window settles over two or three
    st->SetFloat(kW, vp->Size.x); st->SetFloat(kH, vp->Size.y);
    if (left > 0) {
        const float top = keepTop();
        const float nx = std::max(vp->Pos.x, std::min(w->Pos.x, vp->Pos.x + vp->Size.x - w->Size.x));
        const float ny = std::max(vp->Pos.y + top, std::min(w->Pos.y, vp->Pos.y + vp->Size.y - kKeepMargin - w->Size.y));
        if (nx != w->Pos.x || ny != w->Pos.y) ImGui::SetWindowPos(w, ImVec2(nx, ny), ImGuiCond_Always);
        left--;
    }
    st->SetInt(kLeft, left);
}

void pushFloatStyle() {
    if (readingModeOn()) {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(std::max(100.0f, vp->Size.x - 2 * kKeepMargin), std::max(100.0f, vp->Size.y - keepTop() - 2 * kKeepMargin)));
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, ui::corner(static_cast<float>(app::lookopt::m1().radiusPanel) * ImGui::GetFontSize() / 16));   // 5n: windows have the panels' radius (0.38: 6, square in Monochrome)
}
void popFloatStyle() {
    keepInside();                                   // (called right after Begin: the window is the current one)
    ImGui::PopStyleVar();
}

void cardRect(ImDrawList* dl, ImVec2 a, ImVec2 b, float rounding, const Rgb& border, float alpha, bool bordered) {
    const ImU32 line = ImGui::ColorConvertFloat4ToU32(ImVec4(border.r, border.g, border.b, alpha));
    const Rgb& c = look().c.card;
    dl->AddRectFilled(a, b, ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, alpha)), rounding);
    if (bordered) dl->AddRect(a, b, line, rounding);
}

void faceRect(ImDrawList* dl, ImVec2 a, ImVec2 b, const Rgb& base, float alpha, WidgetState, float rounding, ImDrawFlags flags) {
    dl->AddRectFilled(a, b, ImGui::ColorConvertFloat4ToU32(ImVec4(base.r, base.g, base.b, alpha)), rounding, flags);
}

bool Button(const char* label, const ImVec2& size_arg) { const bool r = ImGui::Button(label, size_arg); noteItem(label); return r; }   // (noteItem: the automatic checks find a control by its label; it does nothing otherwise)
bool Checkbox(const char* label, bool* v) { const bool r = ImGui::Checkbox(label, v); noteItem(label); return r; }
bool SliderFloat(const char* label, float* v, float lo, float hi, const char* format) {
    const bool r = ImGui::SliderFloat(label, v, lo, hi, format);
    noteItem(label);
    return r;
}
bool SliderInt(const char* label, int* v, int lo, int hi, const char* format) { return ImGui::SliderInt(label, v, lo, hi, format); }

bool BeginTooltip() {
    if (!ImGui::BeginTooltip()) return false;
    panelDecor(PanelKind::Floating);
    return true;
}
void SetTooltip(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if (BeginTooltip()) { ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26); ImGui::TextV(fmt, args); ImGui::PopTextWrapPos(); ImGui::EndTooltip(); }   // (0.25: wrapped: a long tooltip was one line that could run off the window)
    va_end(args);
}


bool BeginCombo(const char* label, const char* preview, ImGuiComboFlags flags) { const bool o = ImGui::BeginCombo(label, preview, flags); noteItem(label); return o; }
bool Combo(const char* label, int* current, const char* items_separated_by_zeros) {
    const char* p = items_separated_by_zeros;
    std::vector<const char*> items;
    while (*p) { items.push_back(p); p += std::strlen(p) + 1; }
    const char* preview = *current >= 0 && *current < int(items.size()) ? items[*current] : "";
    bool changed = false;
    if (BeginCombo(label, preview)) {
        for (int i = 0; i < int(items.size()); i++) {
            ImGui::PushID(i);
            const bool sel = i == *current;
            if (ImGui::Selectable(items[i], sel)) { *current = i; changed = true; }
            noteItem(items[i]);
            if (sel) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    return changed;
}

}  // namespace ui
