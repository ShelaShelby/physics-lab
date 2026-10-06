// 0.15 stage 3: the drawing helpers of the new UI (ui/shell.cpp): sizes from the type scale, the glass panel, text at the scale's sizes, and the controls (button, icon
// button, check box, segmented button, slider). Everything is drawn by hand with an invisible button for the input, so every size is exactly the scale's. Colours are only
// the theme's tokens (card, text, muted, accent, accent fill, on-accent, edge): app/shell.h.
#pragma once

#include <string>
#include <vector>

#include "app/shell.h"
#include "imgui.h"

namespace ui::sh {

using app::shell::Type;

// ---- this frame's sizes and colours ----
struct Frame {
    app::shell::Metrics m;
    double dpi = 1, tf = 1;                                // (doubles: Text size 1.3 as a float is 1.29999995, and 15 times it rounds to 19, not 20)
    const app::ThemeColors* c = nullptr;
    bool outline = false;        // Monochrome and High contrast: a full-strength 1 px outline on every control
    bool plain = true;           // the theme has no glass style of its own (Aero, Y2K draw theirs: ui::panelDecor)
    float panelAlpha = 0.95f;    // the glass opacity (app::shell::glassOpacity)
};
void beginFrame();               // once per frame, before any shell window: reads the look and the Windows scale
const Frame& F();

ImU32 cText(float a = 1);
ImU32 cMuted(float a = 1);
ImU32 cAccent(float a = 1);
ImU32 cFill(float a = 1);        // the accent fill
ImU32 cOnFill();                 // text on it (4.5:1 in every theme)
ImU32 cFace(float a = 1);        // a control's face on a panel
ImU32 cFaceHover();
ImU32 cFacePress();
ImU32 cEdge();                   // the 1 px edge of panels and (outline mode) controls
ImU32 cHair();                   // a divider between list rows
ImU32 cPanel();                  // the glass fill (the card colour at the glass opacity)
ImU32 cBackdrop(float a);        // the page colour (the reading view's dim)
ImU32 cFactText();               // the fact line's colour (app::shell::factTextColour: the muted colour when it reads over black and white behind its shadow, else the main text colour)
ImU32 cShadow(float a);          // the theme's text-shadow colour (app::shell::textShadow: black round light text, white round dark text) at opacity a

// ---- text ----
ImFont* font(bool bold);
float px(Type t);
float width(Type t, bool bold, const std::string& s);
float lineH(Type t);                                                          // the height of one line of text at this size
void text(ImDrawList* dl, Type t, bool bold, ImVec2 pos, ImU32 col, const std::string& s);   // one line, top left at pos
std::vector<std::string> wrapText(Type t, bool bold, const std::string& s, float maxW);
float paragraph(ImDrawList* dl, Type t, bool bold, ImVec2 pos, float maxW, ImU32 col, const std::string& s);   // wrapped; returns its height
void outlined(ImDrawList* dl, Type t, ImVec2 pos, ImU32 col, ImU32 outline, const std::string& s);          // the readout: a 1 u outline so it reads over anything
// The fact line: the text with a soft shadow round it (a 1 u ring at app::shell::kShadowAlpha, a 2 u ring fainter), everything times `fade` (0..1). Never changes the text's size.
void shadowed(ImDrawList* dl, Type t, ImVec2 pos, ImU32 col, float fade, const std::string& s);
void headingText(ImDrawList* dl, const char* level, ImVec2 pos, ImU32 col, const std::string& s);          // reading view headings: "h1", "h2", "h3"
float headingPx(const char* level);

// ---- icons (drawn by lines and shapes; no icon font) ----
enum class Icon { Menu, Search, Play, Pause, Clock, Eye, Target, Ruler, Camera, Swap, Book, Trophy, Bulb, Palette, Gear, Caret, CaretUp, Close, Calendar, Cube, Dots, Check, Info };
void drawIcon(ImDrawList* dl, Icon icon, ImVec2 centre, float size, ImU32 col);

// ---- panels ----
enum class Shape { Panel, Bar, Pill, Overlay, Clear };      // Overlay: no glass, no input (the readout). Clear: no glass but it takes input (the fact line)
struct Panel {
    ImDrawList* dl = nullptr;
    ImVec2 origin;               // where the content starts (inside the padding), screen pixels
    float w = 0;                 // the content's width
    float y = 0;                 // how far down the content has been laid out
    std::string id;
};
// Opens a window at r (window pixels). False: it is not shown. Always call endPanel when it returned true.
bool beginPanel(const char* id, const app::shell::Rect& r, Panel& p, Shape shape = Shape::Panel, bool scroll = true);
// Ends it and returns the height the content needs (with the padding), to size the panel next frame.
float endPanel(Panel& p);
float lastHeight(const std::string& id, float dflt);     // the height a panel needed last time it was drawn

// ---- controls: each draws at `pos` and returns true when clicked / changed. id: the name a PL_SCRIPT click= uses ----
enum class Face { Secondary, Primary, Quiet };
bool button(const char* id, ImVec2 pos, ImVec2 size, const std::string& label, Face face = Face::Secondary, bool selected = false);
bool iconButton(const char* id, ImVec2 pos, ImVec2 size, Icon icon, const char* tip, bool on = false, float caretW = 0);   // caretW > 0: a small caret at the right end (the button opens a panel)
bool checkbox(const char* id, ImVec2 pos, float w, const std::string& label, bool* v);
bool segmented(const char* id, ImVec2 pos, float w, const std::vector<std::string>& labels, int* cur);
bool slider(const char* id, ImVec2 pos, float w, float* v, float lo, float hi);
// A flat clickable row (a list entry): a dot colour (or none), a label, optionally selected. Returns true when clicked.
bool row(const char* id, ImVec2 pos, float w, const std::string& label, bool selected, const char* dotHex = nullptr);
bool hoveredRect(ImVec2 pos, ImVec2 size);                // the mouse is over this rectangle (and the window is the one under it)


// ---- flowing content: each call lays one thing out at the panel's current place and moves it down (units in the metrics: gaps are in the 4 8 12 16 24 scale) ----
ImVec2 here(const Panel& p);
void pGap(Panel& p, int px);
void pTitle(Panel& p, const std::string& s);                                   // a panel's title: Title size, bold
void pHeading(Panel& p, const std::string& s);                                 // a section heading: Caption, muted
void pCaption(Panel& p, const std::string& s, bool muted = true);              // a wrapped note: Caption
// A stat line: the label muted on the left, the value on the right (a value that does not fit goes under its label, wrapped)
void pStat(Panel& p, const std::string& label, const std::string& value);
void pBody(Panel& p, const std::string& s, bool muted = false);                // a wrapped paragraph or a value line: Body
bool pCheck(Panel& p, const char* id, const std::string& label, bool* v);
bool pSegmented(Panel& p, const char* id, const std::vector<std::string>& labels, int* cur);
bool pSlider(Panel& p, const char* id, float* v, float lo, float hi);
bool pButton(Panel& p, const char* id, const std::string& label, Face face = Face::Secondary, bool selected = false);   // as wide as the panel
bool pRow(Panel& p, const char* id, const std::string& label, bool selected = false, const char* dotHex = nullptr);
void pRule(Panel& p);                                                          // a hairline between list rows

}  // namespace ui::sh
