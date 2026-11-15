// 0.15 stage 5m: the look of the New UI (M1, chosen by the owner on 2026-10-07; stages 5i/5j had three to choose from).
// 0.38: the owner's new look (chosen on 6 Nov 2026 from the clickable mock-up "mission look": sharp 4-6 px corners, flat surfaces, a 1 px edge, one calm accent for "chosen" and "on").
// It is now drawn in EVERY theme: Dark has its own palette (m1()), every other theme gets the same tokens with its own seven colours (forTheme()).
// A look is only a set of design tokens (colours, radii, padding, border, halo, shine, icon stroke): no layout, text size or physics is touched.
// No graphics here, so the contrast of every pair is tested (tests/test_look.inc). Colours are our own design choices (no external source); the contrast rule is the W3C's WCAG 2.x.
#pragma once

#include "app/prefs.h"

namespace app::lookopt {

struct Tokens {
    int id;  // 1 = M1
    const char* name;
    Rgb bg, card, fg, muted, accent, accentFill, onFill, line;
    float faceMix, hoverMix, pressMix;   // a control's face = card moved towards the text colour by faceMix; hover / press = the face moved towards the accent
    float panelAlpha;                    // opacity of panels and bars (1 = solid; below 1 = see-through, no blur: there is no blur path in the app)
    float edgeAlpha;                     // the 1 px border = the text colour at this opacity
    float shineAlpha;                    // the top-lit gradient on panels (0 = none)
    int haloAlpha;                       // the soft outer edge round a panel, black at this alpha 0..255 (0 = none); reaches at most 2 px (app::shell::kHaloMaxPx)
    int radiusPanel, radiusCtl;          // corner radii, points at scale 1
    int pad;                             // a panel's padding, points at scale 1
    float stroke;                        // icon stroke, times today's 1.75 u
    const char* font;                    // the font family key made by ui/fonts.cpp for this look ("m_inter"); text is drawn in it, today's font stays behind it as the fallback
};

const Tokens& m1();                      // the one look, with the Dark palette
const Tokens& forTheme(const ThemeColors& c);   // 0.38: the same tokens with the theme's own colours (Dark: m1()); cached per theme id
ThemeColors apply(const Tokens& t, ThemeColors dark);   // the Dark theme's colours with this look's tokens in place of the colours (other themes: unchanged, see applyFor)
ThemeColors applyFor(const ThemeColors& c);
bool outlineMode(const ThemeColors& c);              // 0.38.1: Monochrome and High contrast draw outlines and solid choices; named by theme, not guessed from a contrast              // 0.38: Dark -> apply(m1(), c); every other theme -> c as it is

}  // namespace app::lookopt
