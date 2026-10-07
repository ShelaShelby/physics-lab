#include "app/look_options.h"


namespace app::lookopt {

namespace {
// stage 5j: the old looks A, B, C were removed (the owner disliked them; they are in git history). The new looks M1..M3 come in part 2.
}  // namespace

int fromEnv(const char* v) {
    if (!v || (v[0] != 'm' && v[0] != 'M') || v[1] < '1' || v[1] > '0' + kCount || v[2]) return 0;
    return v[1] - '0';
}

const Tokens* tokens(int) { return nullptr; }

ThemeColors apply(const Tokens& t, ThemeColors c) {
    c.bg = t.bg; c.card = t.card; c.fg = t.fg; c.muted = t.muted; c.accent = t.accent; c.accentFill = t.accentFill; c.onacc = t.onFill; c.line = t.line;
    c.button = t.card;
    return c;
}

}  // namespace app::lookopt
