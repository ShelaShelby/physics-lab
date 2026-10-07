#include "app/look_options.h"

#include <cctype>

namespace app::lookopt {

namespace {
// A "Soft glass": a blue-tinted see-through card (88 %: the least that keeps 4.5:1 over a pure white picture, tested), big radii, a light hairline, one soft blue accent.
// B "Flat minimal": solid surfaces, no shine, no halo, a very faint hairline, radii 8, roomier padding; the selected fill is neutral light grey, the blue only for links and sliders.
// C "Instrument": near-black neutral, radii 3 / 2, thin icons, thin dividers, one cool teal accent, dense padding.
const Tokens kA{'a', "Soft glass", hexRgb("#10131c"), hexRgb("#1a2030"), hexRgb("#f1f4fa"), hexRgb("#b0bad0"), hexRgb("#8fb5ff"), hexRgb("#3d6fe0"), hexRgb("#ffffff"), hexRgb("#2c3550"),
                0.08f, 0.10f, 0.18f, 0.88f, 0.18f, 0.07f, 30, 14, 10, 16, 1.0f};
const Tokens kB{'b', "Flat minimal", hexRgb("#141416"), hexRgb("#1c1c1e"), hexRgb("#f2f2f3"), hexRgb("#aaaab2"), hexRgb("#7da6ff"), hexRgb("#d9d9de"), hexRgb("#141416"), hexRgb("#2a2a2e"),
                0.06f, 0.10f, 0.16f, 1.0f, 0.08f, 0.0f, 0, 8, 6, 20, 1.0f};
const Tokens kC{'c', "Instrument", hexRgb("#08090b"), hexRgb("#0f1114"), hexRgb("#d8dee4"), hexRgb("#a2adb8"), hexRgb("#2dd4cf"), hexRgb("#0e7c86"), hexRgb("#ffffff"), hexRgb("#232830"),
                0.08f, 0.10f, 0.16f, 0.97f, 0.16f, 0.0f, 0, 4, 3, 12, 0.7f};
}  // namespace

char fromEnv(const char* v) {
    if (!v || !v[0] || v[1]) return 0;
    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(v[0])));
    return c == 'a' || c == 'b' || c == 'c' ? c : 0;
}

const Tokens* tokens(char id) { return id == 'a' ? &kA : id == 'b' ? &kB : id == 'c' ? &kC : nullptr; }

ThemeColors apply(const Tokens& t, ThemeColors c) {
    c.bg = t.bg; c.card = t.card; c.fg = t.fg; c.muted = t.muted; c.accent = t.accent; c.accentFill = t.accentFill; c.onacc = t.onFill; c.line = t.line;
    c.button = t.card;
    return c;
}

}  // namespace app::lookopt
