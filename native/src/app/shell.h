// 0.15 stage 3: the logic of the new UI's shell (DESIGN.md): the type scale and the sizes that follow it, where the top bar, toolbar, readout and floating panels sit, the
// "centre shift" that keeps the selected body out from under a card, the glass panel's opacity and contrast, the keys (Esc, Ctrl+K, Space), the reading view's column and the
// search ranking. No graphics, so every function here is unit-tested (tests/test_shell.inc). ui/shell.cpp only draws what these say.
#pragma once

#include <string>
#include <vector>

#include "app/prefs.h"

namespace app::shell {

// ---- the type scale (DESIGN.md section 1) ----
// u = 1 px at Windows scale 100 % and Text size Normal. A size on screen = max(floor, round(u * Windows scale * Text size factor)).
enum class Type { Caption, Body, Title, Key };
struct TypeDef { const char* name; double u; int floorPx; };
const TypeDef& typeDef(Type t);
int textPx(Type t, double dpi, double tf);              // the size in pixels, floor applied
float fontBase(Type t, double dpi, double tf);          // what to give ImGui::PushFont so that base * Text size * Windows scale = textPx (the style multiplies both)
int unitPx(double u, double dpi, double tf);            // a length in u -> pixels, rounded, at least 1 (heights, gaps, radii)

// All the sizes of the shell for one Windows scale and Text size. Widths are in em of Body (1 em = body px), so they grow with the text.
struct Metrics {
    double dpi = 1, tf = 1;
    int caption = 13, body = 15, title = 17, key = 28;                        // text, px
    int h1 = 28, h2 = 20, h3 = 17;                                           // the reading view's headings, px (floors: 22, 16, 15)
    int control = 30, iconBtn = 30, toolBtn = 32, toolbarH = 40, topBar = 44, chipH = 38, check = 16, row = 28, caretW = 16;
    int s4 = 4, s8 = 8, s12 = 12, s16 = 16, s24 = 24;                         // the spacing scale
    int radiusPanel = 10, radiusCtl = 6;
    int em(double n) const;                                                   // n em of Body, px
};
Metrics metrics(double dpi, double tf);

// ---- rectangles ----
struct Pt { double x = 0, y = 0; };
struct Rect {
    double x = 0, y = 0, w = 0, h = 0;
    double right() const { return x + w; }
    double bottom() const { return y + h; }
    bool contains(Pt p) const { return p.x >= x && p.x <= right() && p.y >= y && p.y <= bottom(); }
    bool intersects(const Rect& o) const { return x < o.right() && o.x < right() && y < o.bottom() && o.y < bottom(); }
    Rect inflated(double d) const { return {x - d, y - d, w + 2 * d, h + 2 * d}; }
    double area() const { return w > 0 && h > 0 ? w * h : 0; }
};
bool within(const Rect& r, const Rect& bounds, double eps = 0.5);          // r lies inside bounds
Rect clampInto(Rect r, const Rect& bounds);                                // the same size (smaller if it must be) moved to lie inside bounds

// ---- the fixed pieces ----
// 0.15 stage 5c: `dock` is the Controls panel on the right (width 0: closed). freeW = the width of the window left of it: the toolbar is centred there, the readout and every panel that
// anchors to the right edge sit left of it, so nothing of the bars is ever under the dock.
struct Frame { Rect window, topBar, toolbar, readout, dock; double freeW = 0; };
Frame makeFrame(double winW, double winH, const Metrics& m, double toolbarW, double readoutW, double readoutH, double dockW = 0);
double dockWidth(const Metrics& m, double winW);                          // the Controls panel: 22 em of Body, at most 45 % of the window, never under 12 em
// How many toolbar items fit (the rest go behind a "..." button, never a second row). widths: each item's width incl. separators; the "..." button is `moreW` wide.
int toolbarVisible(const std::vector<double>& widths, double gap, double moreW, double winW, const Metrics& m);

struct TopBarWidths { double menu = 0, crumbFull = 0, crumbShort = 0, crumbIcon = 0, chip = 0, right = 0; };
struct TopBarPlan { int crumbMode = 0; Rect chip; bool fits = true; };     // crumbMode 0 full, 1 the last two, 2 icons only
TopBarPlan planTopBar(double winW, const Metrics& m, const TopBarWidths& w);

// ---- floating panels ----
enum class Anchor { TopLeft, TopRight, UnderChip, AboveToolbar, Centre };
// Where a panel of (w x h) sits. It never leaves the window: it is made smaller (scrolls inside) when it must be. anchorX: the centre x of the caret or chip it belongs to.
Rect placePanel(Anchor a, double w, double h, const Frame& f, const Metrics& m, double anchorX);
// A panel that opens over a card: if r overlaps `blocker` it moves sideways to sit beside it (left of it when there is room inside `window` with `margin` to spare, else right of it, else it
// stays). Only x changes. (Stage 4: the Time panel and the data sheet at 1000 px wide would otherwise cover half the subject card.)
Rect sideStep(Rect r, const Rect& blocker, const Rect& window, double gap, double margin);
double collapsedWidth(double titleW, const Metrics& m);                    // a collapsed panel: its title, the small caret and the padding

// ---- the centre shift (DESIGN.md section 6) ----
// The picture is full-window; cards and panels cover parts of it. When the selected body's circle falls under one of them (plus margin), `target` is where its centre should go:
// the middle of the largest free rectangle. False: nothing is in the way, nothing moves.
Rect largestFreeRect(const Rect& window, const std::vector<Rect>& occluders);
bool hiddenBy(Pt centre, double radius, const std::vector<Rect>& occluders, double margin);
bool centreShiftTarget(Pt centre, double radius, const Rect& window, const std::vector<Rect>& occluders, double margin, Pt& target);
double easeToward(double current, double target, double dt, double seconds, bool reduced);   // eased over about `seconds`; reduced motion: at once

// ---- glass panels (DESIGN.md section 3) ----
// The panel colour is the theme's card colour at this opacity over the picture; it is the least opacity at which the text, muted and accent colours pass WCAG 2.x 4.5:1
// over the WORST picture (white behind a dark panel, black behind a light one), found in 0.01 steps, plus 0.05, at most 0.97. Same method as docs/ui-0.15/tools/glass_alpha.py.
double glassOpacity(const ThemeColors& c);
Rgb worstBehind(const ThemeColors& c);
Rgb glassOver(const ThemeColors& c, const Rgb& behind);                    // what a panel shows over `behind`
// The text colour on an accent fill (a chosen segment, a primary button): the theme's own on-accent colour when it is 4.5:1 on the fill, else the page or the text colour, whichever
// reads better. (Dark and Black have white on #6c9cff = 2.68:1 in their own data.)
Rgb onAccent(const ThemeColors& c);
constexpr float kEdgeAlpha = 0.20f, kShineAlpha = 0.06f;                    // the 1 px edge and the top-lit shine are the text colour at these opacities

// ---- the readout's backing (stage 5c) ----
// The readout (Zoom, Scale, light travel time, fps) is Body size on a rounded dark pill: black at kReadoutAlpha. Its text is the theme's muted colour when that reads 4.5:1 on the pill
// over the WORST picture behind it (black behind, and white behind, where the pill is only a mid grey #666666), else the theme's text colour, else white. (Measured: no theme's muted
// colour passes over white at 0.6, so in practice it is the text colour or white; tests/test_shell.inc.)
constexpr float kReadoutAlpha = 0.6f;
Rgb readoutText(const ThemeColors& c);
double readoutContrast(const Rgb& text);                                  // the least contrast of `text` on the pill over black and over white behind
// ---- keys ----
enum class PanelId { None, Menu, LabMenu, Chip, Speed, Show, Pick, Scale, View, More, Facts, Data, Nucleus, Cloud };   // Facts: the full fact; Data: the subject card's "All values" (stage 4); Nucleus, Cloud: the Atom Lab toolbar's carets (stage 5a)
struct UiState { PanelId open = PanelId::None; bool search = false; bool reading = false; bool oldPanel = false; };
enum class Key { Escape, CtrlK, Space, Other };
struct KeyOutcome { bool consumed = false; bool togglePlay = false; };
// Esc closes the topmost thing: search, then the open dropdown, then the reading view, then the Old panel. Ctrl+K opens search. Space plays / pauses unless a text field has the keyboard.
KeyOutcome applyKey(UiState& s, Key k, bool textInputActive, bool labHasPlay);

// ---- the reading view ----
struct ReadingGeom { Rect column; Rect toc; bool rail = false; };
// A centred column about 34 em wide (about 70 characters), a contents rail on its left only when the window is wider than 60 em plus the column.
ReadingGeom readingGeometry(double winW, double winH, const Metrics& m);

// ---- search (Ctrl+K) ----
struct SearchItem { std::string label, group, id; };
// Up to maxResults indexes: labels that start with the query first, then labels with a word that starts with it, then labels that contain it; ties go to the recent choices
// (newest first: `recent` holds item ids), then keep the list's order. An empty query lists the recent choices (stage 4: "recent searches first").
std::vector<int> searchResults(const std::string& query, const std::vector<SearchItem>& items, int maxResults = 8, const std::vector<std::string>& recent = {});
std::vector<std::string> pushRecent(const std::vector<std::string>& recent, const std::string& id, std::size_t keep = 8);   // id to the front, no copies, at most `keep`
std::string packRecent(const std::vector<std::string>& recent);                                                           // "a;b;c" (the settings file's recent= line)
std::vector<std::string> unpackRecent(const std::string& packed);

// ---- the fact line (stage 4): one short line at the bottom left of the picture, fading to the next fact every ~20 s, still while the pointer is over it ----
constexpr double kFactInterval = 20.0, kFactFade = 0.5;                   // seconds on screen, and the length of each fade (in and out)
double factAlpha(double t, double interval, double fade);                 // how visible the text is t seconds after it appeared: 0 -> 1 over `fade`, held, 1 -> 0 over the last `fade`
struct FactClock { double t = 0; };
// One frame of the clock. True: the fade-out has ended, show the next fact (the clock restarts). Hovered: the clock stands still and the text comes back to full. Reduced motion: no
// automatic change at all (the arrows still step; as the old line did).
bool factTick(FactClock& c, double dt, bool hovered, bool reduced, double interval, double fade);
// Where the line sits: bottom left, level with the toolbar and to its left; if there is no room there (a narrow window) one row above the toolbar, left of the readout. textW: the width
// the fact would like (the arrows ‹ › are included in the rectangle). Never over the toolbar, the readout or the top bar, never outside the window.
Rect factLineRect(const Frame& f, const Metrics& m, double textW);
// Text drawn straight on the picture (the readout, the fact line) has a shadow round the letters: black round light text, white round dark text. Its opacity is kShadowAlpha at the
// letters; the text colour against it passes WCAG 4.5:1, so the text reads over black and over white behind (tested in all 14 themes).
constexpr float kShadowAlpha = 0.9f;
Rgb textShadow(const ThemeColors& c);
Rgb factTextColour(const ThemeColors& c);                                 // the muted colour if it passes 4.5:1 over both extremes behind its shadow, else the main text colour
double factContrast(const ThemeColors& c);                                // the least contrast of that colour over white and black behind the shadow

// ---- remembered panel states ----
// "card=c,legend=o": the collapsed (c) or open (o) state of each floating panel, kept in the settings file.
bool panelCollapsed(const std::string& saved, const std::string& name, bool dflt);
std::string withPanelState(const std::string& saved, const std::string& name, bool collapsed);

}  // namespace app::shell
