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
struct Frame { Rect window, topBar, toolbar, readout; };
Frame makeFrame(double winW, double winH, const Metrics& m, double toolbarW, double readoutW, double readoutH);
// How many toolbar items fit (the rest go behind a "..." button, never a second row). widths: each item's width incl. separators; the "..." button is `moreW` wide.
int toolbarVisible(const std::vector<double>& widths, double gap, double moreW, double winW, const Metrics& m);

struct TopBarWidths { double menu = 0, crumbFull = 0, crumbShort = 0, crumbIcon = 0, chip = 0, right = 0; };
struct TopBarPlan { int crumbMode = 0; Rect chip; bool fits = true; };     // crumbMode 0 full, 1 the last two, 2 icons only
TopBarPlan planTopBar(double winW, const Metrics& m, const TopBarWidths& w);

// ---- floating panels ----
enum class Anchor { TopLeft, TopRight, UnderChip, AboveToolbar, Centre };
// Where a panel of (w x h) sits. It never leaves the window: it is made smaller (scrolls inside) when it must be. anchorX: the centre x of the caret or chip it belongs to.
Rect placePanel(Anchor a, double w, double h, const Frame& f, const Metrics& m, double anchorX);
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

// ---- keys ----
enum class PanelId { None, Menu, LabMenu, Chip, Speed, Show, Pick, Scale, View, More, Facts };
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
// Up to maxResults indexes: labels that start with the query first, then labels with a word that starts with it, then labels that contain it; ties keep the list's order.
std::vector<int> searchResults(const std::string& query, const std::vector<SearchItem>& items, int maxResults = 8);

// ---- remembered panel states ----
// "card=c,legend=o": the collapsed (c) or open (o) state of each floating panel, kept in the settings file.
bool panelCollapsed(const std::string& saved, const std::string& name, bool dflt);
std::string withPanelState(const std::string& saved, const std::string& name, bool collapsed);

}  // namespace app::shell
