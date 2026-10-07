// 0.15 stage 5m: the look M1 of the New UI (Dark theme). Included by test_app.cpp (uses its checkTrue). (Stage 5i/5j tested M1..M3 and PL_LOOK; M2, M3 and fromEnv were removed on purpose, see REPORT.md.)
// Reference: the W3C's WCAG 2.x contrast formula (https://www.w3.org/TR/WCAG21/#dfn-contrast-ratio, #dfn-relative-luminance), WRITTEN OUT HERE (not the app's own function),
// checked against the W3C's own worked values first. Needs: body and card text 4.5:1 (SC 1.4.3), icons and the borders of controls 3:1 (SC 1.4.11).
#include "app/look_options.h"

namespace look_t {
struct C3 { double r, g, b; };
static C3 hex(const char* s) {                       // "#rrggbb"
    const auto v = [&](int i) { return std::strtol(std::string(s + 1 + i, 2).c_str(), nullptr, 16) / 255.0; };
    return {v(0), v(2), v(4)};
}
static C3 mix(C3 a, C3 b, double f) { return {a.r + (b.r - a.r) * f, a.g + (b.g - a.g) * f, a.b + (b.b - a.b) * f}; }
static double lin(double c) { return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4); }
static double lum(C3 c) { return 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b); }
static double ratio(C3 a, C3 b) { const double x = lum(a), y = lum(b); return (std::max(x, y) + 0.05) / (std::min(x, y) + 0.05); }
static C3 rg(const app::Rgb& c) { return {c.r, c.g, c.b}; }
}  // namespace look_t

static void testLookContrast() {
    using namespace look_t;
    // the formula itself against the W3C's worked values: black on white 21:1; #767676 on white 4.54:1; #777 on white 4.48:1
    checkTrue("WCAG black on white = 21", std::fabs(ratio(hex("#000000"), hex("#ffffff")) - 21.0) < 1e-9);
    checkTrue("WCAG #767676 on white = 4.54", std::fabs(ratio(hex("#767676"), hex("#ffffff")) - 4.54) < 5e-3);
    checkTrue("WCAG #777777 on white = 4.48 (fails)", std::fabs(ratio(hex("#777777"), hex("#ffffff")) - 4.478) < 2e-3);

    const app::ThemeColors dark = app::themeColors("dark", false);
    // today's Dark as a sanity row: the pairs that pass today must still read as passing in our formula (the ones that do not are reported in REPORT.md, not asserted)
    {
        const C3 card = rg(dark.card), fg = rg(dark.fg), mu = rg(dark.muted), ac = rg(dark.accent);
        checkTrue("sanity Dark: text on card >= 4.5", ratio(fg, card) >= 4.5);
        checkTrue("sanity Dark: muted on card >= 4.5", ratio(mu, card) >= 4.5);
        checkTrue("sanity Dark: accent on card >= 4.5", ratio(ac, card) >= 4.5);
        checkTrue("sanity Dark: text on its button face >= 4.5", ratio(fg, mix(card, fg, 0.10)) >= 4.5);
    }
    {
        const app::lookopt::Tokens* t = &app::lookopt::m1();
        const std::string n = std::string("look ") + t->name + ": ";
        const C3 card = rg(t->card), fg = rg(t->fg), mu = rg(t->muted), ac = rg(t->accent), fill = rg(t->accentFill), on = rg(t->onFill);
        const C3 face = mix(card, fg, t->faceMix), hover = mix(face, ac, t->hoverMix), press = mix(face, ac, t->pressMix);
        struct Pair { const char* what; C3 a, b; double need; };
        const Pair pairs[] = {
            {"text on card", fg, card, 4.5},        {"text on a button face", fg, face, 4.5}, {"text on a hovered face", fg, hover, 4.5}, {"text on a pressed face", fg, press, 4.5},
            {"muted text on card", mu, card, 4.5},  {"muted text on face", mu, face, 4.5},    {"muted text on hovered face", mu, hover, 4.5}, {"muted text on pressed face", mu, press, 4.5},
            {"accent text on card", ac, card, 4.5}, {"accent text on hovered face", ac, hover, 4.5},
            {"text on the accent fill", on, fill, 4.5},
            {"icon (muted, at rest) on card", mu, card, 3.0}, {"icon (muted) on face", mu, face, 3.0}, {"icon (text colour, hovered) on hovered face", fg, hover, 3.0}, {"icon (text colour) on card", fg, card, 3.0}, {"icon on face", fg, face, 3.0}, {"caret (muted) on card", mu, card, 3.0},
            {"check box border (muted) on face", mu, face, 3.0}, {"slider track (accent) on card", ac, card, 3.0}, {"divider (line) on card: visible, a hairline", rg(t->line), card, 1.2}, {"accent fill against card (selected tab)", fill, card, 1.0},
        };
        for (const Pair& p : pairs) checkTrue(n + p.what + " >= " + std::to_string(p.need).substr(0, 3) + ":1 (is " + std::to_string(ratio(p.a, p.b)).substr(0, 4) + ")", ratio(p.a, p.b) >= p.need);
        // translucent panels: the card at panelAlpha over the WORST picture behind (pure black or pure white), text still 4.5:1
        checkTrue(n + "panel opacity in 0.5..1", t->panelAlpha >= 0.5f && t->panelAlpha <= 1.0f);
        for (const C3 behind : {C3{0, 0, 0}, C3{1, 1, 1}}) {
            const C3 s = mix(behind, card, t->panelAlpha);
            checkTrue(n + "text on the glass over " + (behind.r > 0.5 ? "white" : "black") + " >= 4.5", ratio(fg, s) >= 4.5);
            checkTrue(n + "muted on the glass over " + (behind.r > 0.5 ? "white" : "black") + " >= 4.5", ratio(mu, s) >= 4.5);
            checkTrue(n + "accent on the glass over " + (behind.r > 0.5 ? "white" : "black") + " >= 4.5", ratio(ac, s) >= 4.5);
        }
        // sizes: the spec's radii and the Body floor are not touched by a look
        checkTrue(n + "radii and padding are sane", t->radiusPanel >= 3 && t->radiusPanel <= 16 && t->radiusCtl >= 2 && t->radiusCtl <= t->radiusPanel && t->pad >= 12 && t->pad <= 24);
        // the owner's brief for M1: flat (no gradient / shine / halo / see-through), radii 10-12 / 8, thin icons, an own font; the selected pill is light grey with dark text
        checkTrue(n + "flat: solid, no shine, no halo", t->panelAlpha == 1.0f && t->shineAlpha == 0.0f && t->haloAlpha == 0);
        checkTrue(n + "radii 10..12 (panels) and 8 (controls), thin icons (stroke 0.9..1.0 of 1.75 u = about 1.5 px)", t->radiusPanel >= 10 && t->radiusPanel <= 12 && t->radiusCtl == 8 && t->stroke >= 0.9f && t->stroke <= 1.0f);
        checkTrue(n + "a look font is named", t->font && std::string(t->font) == "m_inter");
        checkTrue(n + "the hairline border is about 8 % (6..10 %)", t->edgeAlpha >= 0.06f && t->edgeAlpha <= 0.10f);
        checkTrue(n + "very dark flat background and card (luminance < 0.02), card a little lighter than the background", lum(rg(t->bg)) < 0.02 && lum(card) < 0.02 && lum(card) > lum(rg(t->bg)));
        checkTrue(n + "selected pill is light grey (luminance > 0.5) with dark text (< 0.02)", lum(fill) > 0.5 && lum(on) < 0.02 && std::fabs(fill.r - fill.b) < 0.05);
        checkTrue(n + "the border is a hairline (<= 0.2) and the halo stays within the 2 px rule (alpha <= 40)", t->edgeAlpha > 0 && t->edgeAlpha <= 0.2f && t->haloAlpha >= 0 && t->haloAlpha <= 40);
    }
}
