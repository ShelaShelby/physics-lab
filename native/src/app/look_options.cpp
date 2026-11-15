#include "app/look_options.h"

#include <map>
#include <string>


namespace app::lookopt {

namespace {
// 0.15 stage 5m: M1 was THE look of the New UI (the owner chose it on 2026-10-07). 0.38 (the owner, 6 Nov 2026, from the clickable mock-up): the new look keeps M1's flat
// surfaces and its Inter font and changes the rest: corners 6 (panels) and 4 (controls), a 1 px edge of 10 % of the text colour, no shine, no halo; a chosen option is a soft
// accent tint with a thin accent edge (ui/shell_widgets.cpp), a ticked box and a slider's filled part are the accent fill. Our own design choices (no external source); every
// colour pair is tested against WCAG 2.x in tests/test_look.inc. The accent fill #3a6fd4 is dark enough for white text (4.8:1).
//          id name             bg           card         text         muted        accent       fill         on fill      line
const Tokens kM1{1, "0.38 lab look", hexRgb("#0b0c0e"), hexRgb("#16171a"), hexRgb("#ecedef"), hexRgb("#a3a6ae"), hexRgb("#6b9ff7"), hexRgb("#3a6fd4"), hexRgb("#ffffff"), hexRgb("#2c2e33"),
                  0.05f, 0.08f, 0.10f, 1.0f, 0.10f, 0.0f, 0, 6, 4, 16, 0.95f, "m_inter"};
}  // namespace

const Tokens& m1() { return kM1; }

const Tokens& forTheme(const ThemeColors& c) {
    if (c.id == "dark") return kM1;
    static std::map<std::string, Tokens> cache;            // a map: the references stay valid
    auto it = cache.find(c.id);
    if (it == cache.end()) {
        Tokens t = kM1;                                    // the same sizes, mixes and font; the theme's own colours
        t.id = 2; t.name = "0.38 lab look (theme colours)";
        t.bg = c.bg; t.card = c.card; t.fg = c.fg; t.muted = c.muted; t.accent = c.accent; t.accentFill = c.accentFill; t.onFill = c.onacc; t.line = c.line;
        it = cache.emplace(c.id, t).first;
    }
    return it->second;
}

// 0.38.1: until now "outline mode" was guessed from the colours (square, or line against page >= 7:1), which would quietly switch any new high-contrast theme; it is named instead
bool outlineMode(const ThemeColors& c) { return c.style.square || c.id == "contrast"; }

ThemeColors applyFor(const ThemeColors& c) { return c.id == "dark" ? apply(kM1, c) : c; }

ThemeColors apply(const Tokens& t, ThemeColors c) {
    c.bg = t.bg; c.card = t.card; c.fg = t.fg; c.muted = t.muted; c.accent = t.accent; c.accentFill = t.accentFill; c.onacc = t.onFill; c.line = t.line;
    c.button = t.card;
    c.soft = t.muted;                    // 5n: the soft boxes of the reading pages (About, Learn) are a neutral grey, not the Dark theme's blue tint
    return c;
}

}  // namespace app::lookopt
