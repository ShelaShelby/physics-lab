#include "app/look_options.h"


namespace app::lookopt {

namespace {
// stage 5j: the old looks A, B, C were removed (the owner disliked them; they are in git history). Our own design choices (no external source), from the owner's brief:
// a modern dark app: very dark flat surfaces, a 1 px hairline (8 % of the text colour), no gradient / shine / halo, radii 10-12, thin 1.5 px icons, one solid selected pill, accent used sparingly.
// M1 Inter, light-grey selected pill.   M2 Inter, near-black, blue selected pill (#2563eb with white text).   M3 = M1's surfaces in IBM Plex Sans (to compare typefaces).
//          id name        bg           card         text         muted        accent       fill         on fill      line
const Tokens kM1{1, "M1 Inter flat", hexRgb("#101012"), hexRgb("#17171a"), hexRgb("#ececef"), hexRgb("#9a9aa3"), hexRgb("#8ab4ff"), hexRgb("#d9d9de"), hexRgb("#17171a"), hexRgb("#2a2a2f"),
                  0.05f, 0.06f, 0.12f, 1.0f, 0.08f, 0.0f, 0, 12, 8, 16, 0.95f, "m_inter"};
const Tokens kM2{2, "M2 Inter blue", hexRgb("#0b0b0c"), hexRgb("#111113"), hexRgb("#ededf0"), hexRgb("#9797a0"), hexRgb("#7aa8ff"), hexRgb("#2563eb"), hexRgb("#ffffff"), hexRgb("#26262b"),
                  0.05f, 0.06f, 0.12f, 1.0f, 0.08f, 0.0f, 0, 10, 8, 16, 0.95f, "m_inter"};
const Tokens kM3{3, "M3 Plex flat", hexRgb("#101012"), hexRgb("#17171a"), hexRgb("#ececef"), hexRgb("#9a9aa3"), hexRgb("#8ab4ff"), hexRgb("#d9d9de"), hexRgb("#17171a"), hexRgb("#2a2a2f"),
                  0.05f, 0.06f, 0.12f, 1.0f, 0.08f, 0.0f, 0, 12, 8, 16, 0.95f, "m_plex"};
}  // namespace

int fromEnv(const char* v) {
    if (!v || (v[0] != 'm' && v[0] != 'M') || v[1] < '1' || v[1] > '0' + kCount || v[2]) return 0;
    return v[1] - '0';
}

const Tokens* tokens(int id) { return id == 1 ? &kM1 : id == 2 ? &kM2 : id == 3 ? &kM3 : nullptr; }

ThemeColors apply(const Tokens& t, ThemeColors c) {
    c.bg = t.bg; c.card = t.card; c.fg = t.fg; c.muted = t.muted; c.accent = t.accent; c.accentFill = t.accentFill; c.onacc = t.onFill; c.line = t.line;
    c.button = t.card;
    return c;
}

}  // namespace app::lookopt
