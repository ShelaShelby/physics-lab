// 0.15 stage 5i/5j: alternative LOOKS of the new UI, for the owner to choose from (render only; PL_LOOK=m1|m2|m3, read once at start; unset = today's tokens; no setting, no UI).
// A look is only a set of design tokens (colours, radii, padding, border, halo, shine, icon stroke): no layout, text size or physics is touched. Dark theme only.
// No graphics here, so the contrast of every pair is tested (tests/test_look.inc). Colours are our own design choices (no external source); the contrast rule is the W3C's WCAG 2.x.
#pragma once

#include "app/prefs.h"

namespace app::lookopt {

struct Tokens {
    int id;  // 1..3 = M1..M3
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
};

constexpr int kCount = 0;                // number of looks (part 1: none yet)
int fromEnv(const char* value);          // 1..kCount for PL_LOOK=m1..m3 (either case), else 0
const Tokens* tokens(int id);           // nullptr for 0
ThemeColors apply(const Tokens& t, ThemeColors dark);   // the Dark theme's colours with this look's tokens in place of the colours

}  // namespace app::lookopt
