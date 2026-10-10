#include "app/look_options.h"


namespace app::lookopt {

namespace {
// 0.15 stage 5m: M1 is THE look of the New UI (the owner chose it on 2026-10-07; M2 and M3 and PL_LOOK were removed, they are in git history). Our own design choices (no external source),
// from the owner's brief: a modern dark app: very dark flat surfaces, a 1 px hairline (8 % of the text colour), no gradient / shine / halo, radii 10-12, thin 1.5 px icons, one solid selected
// pill (light grey, dark text), accent used sparingly. Font: Inter.
//          id name        bg           card         text         muted        accent       fill         on fill      line
const Tokens kM1{1, "M1 Inter flat", hexRgb("#101012"), hexRgb("#17171a"), hexRgb("#ececef"), hexRgb("#9a9aa3"), hexRgb("#8ab4ff"), hexRgb("#d9d9de"), hexRgb("#17171a"), hexRgb("#2a2a2f"),
                  0.05f, 0.06f, 0.08f, 1.0f, 0.08f, 0.0f, 0, 12, 8, 16, 0.95f, "m_inter"};
}  // namespace

const Tokens& m1() { return kM1; }

ThemeColors apply(const Tokens& t, ThemeColors c) {
    c.bg = t.bg; c.card = t.card; c.fg = t.fg; c.muted = t.muted; c.accent = t.accent; c.accentFill = t.accentFill; c.onacc = t.onFill; c.line = t.line;
    c.button = t.card;
    c.soft = t.muted;                    // 5n: the soft boxes of the reading pages (About, Learn) are a neutral grey, not the Dark theme's blue tint
    return c;
}

}  // namespace app::lookopt
