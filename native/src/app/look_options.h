// 0.15 stage 5m: the look of the New UI (M1, chosen by the owner on 2026-10-07; stages 5i/5j had three to choose from). Always on with New UI On and the Dark theme; no setting, no env var.
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

const Tokens& m1();                      // the one look
ThemeColors apply(const Tokens& t, ThemeColors dark);   // the Dark theme's colours with this look's tokens in place of the colours

}  // namespace app::lookopt
