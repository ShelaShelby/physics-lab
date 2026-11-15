#include "look.h"
#include "app/look_options.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <cmath>
#include <cstdlib>
#include <vector>

#include "fonts.h"
#include "style_draw.h"

namespace ui {

static Look g_look;
static std::string g_applied;   // what was last applied to Dear ImGui (only redone when it changes)

const Look& look() { return g_look; }

bool systemReducedMotion() {
#ifdef _WIN32
    BOOL anim = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &anim, 0)) return !anim;
#endif
    return false;
}

ImVec4 vec(const app::Rgb& c, float a) { return ImVec4(c.r, c.g, c.b, a); }
ImU32 col(const app::Rgb& c, float a) { return ImGui::ColorConvertFloat4ToU32(vec(c, a)); }
app::Rgb mix(const app::Rgb& a, const app::Rgb& b, float f) { return {a.r + (b.r - a.r) * f, a.g + (b.g - a.g) * f, a.b + (b.b - a.b) * f}; }
// 0.38 (the owner: "i dont want round UI"): no corner anywhere in the interface is rounder than 6 px at 100 % Windows scale, whatever the drawing code asks for
float corner(float r) { return std::min(app::scaledRadius(g_look.c.style, r), 6.0f * ImGui::GetStyle().FontScaleDpi); }

// 0.38 (the owner's new look): a CHOSEN option (a Settings tile, a guide's view chip) is the soft accent tint with a thin accent edge and the text colour, as in the Controls panel;
// in the outline themes (Monochrome, High contrast) it stays the solid accent fill with its on-fill text. pushPrimary() is the solid fill of a primary action button.
static bool outlineTheme() { return app::lookopt::outlineMode(g_look.c); }
void pushOn() {
    const bool solid = outlineTheme();
    const app::Rgb face = mix(g_look.w.card, g_look.w.fg, app::lookopt::m1().faceMix), tint = mix(face, g_look.w.accent, 0.18f);
    ImGui::PushStyleColor(ImGuiCol_Button, vec(solid ? g_look.w.accentFill : tint));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, vec(solid ? g_look.w.accentFill : mix(tint, g_look.w.fg, 0.06f)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, vec(solid ? g_look.w.accentFill : mix(tint, g_look.w.fg, 0.10f)));
    ImGui::PushStyleColor(ImGuiCol_Text, vec(solid ? g_look.w.onacc : g_look.w.fg));
    ImGui::PushStyleColor(ImGuiCol_Border, vec(solid ? g_look.w.accentFill : g_look.w.accent, solid ? 1.0f : 0.85f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
}
void popOn() { ImGui::PopStyleColor(5); ImGui::PopStyleVar(); }
bool onIsSolid() { return outlineTheme(); }
void pushPrimary() {
    ImGui::PushStyleColor(ImGuiCol_Button, vec(g_look.w.accentFill));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, vec(mix(g_look.w.accentFill, g_look.w.fg, 0.12f)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, vec(g_look.w.accentFill));
    ImGui::PushStyleColor(ImGuiCol_Text, vec(g_look.w.onacc));
}
void popPrimary() { ImGui::PopStyleColor(4); }


void invalidateLook() { g_applied.clear(); }   // 0.15 stage 2: the Dear ImGui style was rebuilt (a new display scale): the next updateLook puts the theme back

void updateLook(const app::Prefs& p, bool systemLight, bool systemReduced) {
    Look L;
    L.c = app::themeColors(app::themeUsable(p, p.theme) ? p.theme : "auto", systemLight);
    L.textScale = float(app::textSizeFactor(p.textSize));
    L.reduced = p.motion == "on" || (p.motion == "auto" && systemReduced);
    const app::ThemeStyle& ts = L.c.style;                     // 0.14: what used to be tested as `id == "monochrome"` and so on is data of the theme
    L.square = ts.square;
    L.grayscale = ts.grayscale;
    // the font: the one picked in Settings; with Theme default, the theme's own (Paper's book font, Monochrome's monospace, Aero's Segoe UI when the PC has it)
    L.fontKey = app::fontKeyFor(ts, p.font, fontFamilyAvailable(app::fontFamilyKeyOf(ts)));
    {   // 0.15 stage 5m: the New UI's look M1 (Dark, font Auto): its own font (Inter); if it did not load, nothing changes
        const app::lookopt::Tokens* lt = &app::lookopt::m1();
        if (lt && L.fontKey == "auto" && fontFamilyAvailable(lt->font)) L.fontKey = lt->font;   // 0.38: Inter in every theme with Font Auto (Paper keeps its book font, Monochrome its monospace)
        L.m1 = lt != nullptr;                            // 0.15 stage 5n: the windows follow the look too (colours: Look::w); 0.38: in every theme
        L.w = app::lookopt::applyFor(L.c);
    }
    g_look = L;
    useFontFamily(L.fontKey);
    const std::string key = L.c.id + (L.m1 ? "|m1|" : "|")  + std::to_string(L.textScale) + "|" + std::to_string(ImGui::GetStyle().FontScaleDpi);   // 0.15 stage 2: the display scale can change while the app runs
    if (key == g_applied) return;
    g_applied = key;
    ImGuiStyle& st = ImGui::GetStyle();
    st.FontScaleMain = L.textScale;
    const app::lookopt::Tokens& t1 = app::lookopt::m1();
    const float r = L.square ? 0 : 6 * ts.roundScale * st.FontScaleDpi;   // 0.38: 6 px panels and pop-ups, 4 px controls, 3 px grabs (the owner's sharp look)
    st.FrameRounding = r * 4 / 6; st.GrabRounding = r * 3 / 6; st.PopupRounding = r; st.ChildRounding = r; st.TabRounding = r * 4 / 6;
    st.ScrollbarRounding = r / 2; st.WindowRounding = L.square ? 0 : t1.radiusPanel * st.FontScaleDpi;
    st.FrameBorderSize = ts.panel.borderWidth;
    st.GrabMinSize = 16.0f * st.FontScaleDpi * L.textScale;   // a thumb that is easy to see and grab
    const app::ThemeColors& c = L.w;
    ImVec4* k = st.Colors;
    const app::Rgb tint = c.fg;                                 // 5n: a hovered / pressed face is moved towards the text colour, never the accent (0.38: every theme)
    const float hov = t1.hoverMix, prs = t1.pressMix;
    k[ImGuiCol_Text] = vec(c.fg);
    k[ImGuiCol_TextDisabled] = vec(c.muted);
    k[ImGuiCol_WindowBg] = vec(c.bg);                         // every window is solid
    k[ImGuiCol_ChildBg] = vec(c.bg, 0);
    k[ImGuiCol_PopupBg] = vec(app::floatingSurface(c, c.bg, 0.5f, true));   // a pop-up list is one solid colour (a glass theme: the glass over the page's middle)
    k[ImGuiCol_Border] = vec(mix(c.line, c.fg, ts.panel.borderFgMix));
    k[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    k[ImGuiCol_FrameBg] = vec(mix(c.card, c.fg, 0.16f));
    k[ImGuiCol_FrameBgHovered] = vec(mix(mix(c.card, c.fg, 0.16f), tint, hov));
    k[ImGuiCol_FrameBgActive] = vec(mix(mix(c.card, c.fg, 0.16f), tint, prs));
    k[ImGuiCol_TitleBg] = vec(c.card);
    k[ImGuiCol_TitleBgActive] = vec(c.card);
    k[ImGuiCol_TitleBgCollapsed] = vec(c.card);
    k[ImGuiCol_MenuBarBg] = vec(c.card);
    k[ImGuiCol_ScrollbarBg] = vec(c.bg);
    k[ImGuiCol_ScrollbarGrab] = vec(c.line);
    k[ImGuiCol_ScrollbarGrabHovered] = vec(mix(c.line, c.fg, 0.2f));
    k[ImGuiCol_ScrollbarGrabActive] = vec(c.accent);
    k[ImGuiCol_CheckMark] = vec(c.accent);                   // 0.38: the accent marks "on"
    k[ImGuiCol_SliderGrab] = vec(mix(c.accent, c.fg, 0.1f));
    k[ImGuiCol_SliderGrabActive] = vec(mix(c.accent, c.fg, 0.2f));
    k[ImGuiCol_Button] = vec(c.button);                       // the card colour unless the theme has its own face colour (Aero's aqua, Y2K's silver)
    k[ImGuiCol_ButtonHovered] = vec(mix(mix(c.button, c.fg, t1.faceMix), tint, hov));
    k[ImGuiCol_ButtonActive] = vec(mix(mix(c.button, c.fg, t1.faceMix), tint, prs));
    k[ImGuiCol_Button] = vec(mix(c.button, c.fg, t1.faceMix));   // 5n: a button's face as in the shell's controls
    k[ImGuiCol_Header] = vec(mix(c.card, c.accentFill, 0.10f));
    k[ImGuiCol_HeaderHovered] = vec(mix(c.card, c.accentFill, 0.14f));
    k[ImGuiCol_HeaderActive] = vec(mix(c.card, c.accentFill, 0.20f));
    k[ImGuiCol_Separator] = vec(c.line);
    k[ImGuiCol_SeparatorHovered] = vec(c.accent);
    k[ImGuiCol_SeparatorActive] = vec(c.accent);
    k[ImGuiCol_ResizeGrip] = vec(c.line, 0.5f);
    k[ImGuiCol_ResizeGripHovered] = vec(c.accent, 0.6f);
    k[ImGuiCol_ResizeGripActive] = vec(c.accent);
    k[ImGuiCol_Tab] = vec(c.card);
    k[ImGuiCol_TabHovered] = vec(mix(c.card, tint, 0.14f));
    k[ImGuiCol_TabSelected] = vec(c.accentFill);
    k[ImGuiCol_PlotLines] = vec(c.accent);
    k[ImGuiCol_PlotLinesHovered] = vec(c.fg);
    k[ImGuiCol_PlotHistogram] = vec(c.accent);
    k[ImGuiCol_TableBorderStrong] = vec(c.line);
    k[ImGuiCol_TableBorderLight] = vec(c.line);
    k[ImGuiCol_TextSelectedBg] = vec(c.accent, 0.35f);
    k[ImGuiCol_NavCursor] = vec(c.accent);
    k[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.45f);
    {                                                          // 5n: the windows are the panels' card colour (flat), tables and headers are quiet greys
        k[ImGuiCol_WindowBg] = vec(c.card);
        k[ImGuiCol_PopupBg] = vec(c.card);
        k[ImGuiCol_ScrollbarBg] = vec(c.card);
        k[ImGuiCol_TableHeaderBg] = vec(mix(c.card, c.fg, 0.08f));
        k[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
        k[ImGuiCol_TableRowBgAlt] = vec(mix(c.card, c.fg, 0.035f));
        k[ImGuiCol_Border] = vec(mix(c.card, c.fg, t1.edgeAlpha));   // the 1 px hairline of the panels (the text colour at 8 %)
    }
}

}  // namespace ui
