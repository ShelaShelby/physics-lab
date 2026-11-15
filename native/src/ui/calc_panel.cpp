// The calculator panel (0.49): see calc_panel.h. The maths and the formulas are app/calc.* and app/calc_lab.* (tested); here: the drawing, the labs' numbers and the history.
#include "ui/calc_panel.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "imgui.h"

#include "app/calc.h"
#include "app/calc_lab.h"
#include "atom/atom_core.h"
#include "molecule/vibration.h"
#include "orbit/card_text.h"
#include "orbit/units.h"
#include "ui/look.h"
#include "ui/orbit_panel.h"
#include "ui/shell_test.h"
#include "ui/shell_widgets.h"
#include "ui/style_draw.h"

namespace ui {

using app::shell::Rect;
using app::shell::Type;

namespace {

struct CalcState {
    bool open = false, focus = false, fieldActive = false;
    char buf[1024] = "";
    int gen = 0;                      // a new text box id whenever the line is set from outside (Dear ImGui keeps its own copy of an active box's text)
    bool haveAns = false;
    calc::Vars vars;            // 0.50.2: the named variables kept so far (x = 2 m), for this run of the app
    calc::Quantity ans;
    std::string recipeLine, recipeNote;   // the note of the formula put in, shown while the line is still that formula
    bool showConst = false, showHelp = false, showQm = false;
    std::string logged;
};
CalcState C;

// Puts s in the line (cut at 1023 bytes, never inside a UTF-8 character) and gives the box the keyboard.
void setLine(const std::string& s) {
    std::size_t n = std::min(s.size(), sizeof C.buf - 1);
    while (n > 0 && n < s.size() && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) n--;
    std::memcpy(C.buf, s.data(), n);
    C.buf[n] = 0;
    C.gen++;
    C.focus = true;
}
// Adds a name at the end of the line, with a space before it unless the line is empty or ends in a space or an opening bracket.
void append(const std::string& name) {
    std::string s = C.buf;
    if (!s.empty() && s.back() != ' ' && s.back() != '(') s += ' ';
    setLine(s + name);
}

void keep(AppState& app, const calc::Result& r) {   // Enter: the line goes into the history and its answer becomes ans
    if (!r.ok) return;
    C.ans = r.q;
    C.haveAns = true;
    if (!r.assigned.empty()) C.vars[r.assigned] = r.q;
    std::vector<std::string>& h = app.prefs.calcHistory;
    const std::string line = C.buf;
    h.erase(std::remove(h.begin(), h.end(), line), h.end());
    h.insert(h.begin(), line);
    if (h.size() > static_cast<std::size_t>(app::kCalcHistoryMax)) h.resize(app::kCalcHistoryMax);
}

// The selected thing's formulas, and what to call it
std::vector<calc::Recipe> labRecipes(const AppState& app, std::string& subject) {
    subject.clear();
    if (app.lab == 0) {
        if (app.view.sel.kind != orbit::Selection::Body) return {};
        const orbit::Body* b = orbit::findBody(app.sys, app.view.sel.bodyId);
        if (!b) return {};
        const orbit::OrbitShape o = orbit::orbitOf(*b);
        const double a = o.e < 1 && std::isfinite(o.a) && o.a > 0 ? o.a : 0;
        subject = b->name;
        return calc::orbitRecipes(b->name, o.r, o.v, b->radius, b->massEarths > 0 ? b->massEarths * units::kEarthMassKg : 0, a);
    }
    if (app.lab == 1) {
        const int Z = app.atom.p.Z, N = app.atom.p.N;
        if (Z < 1) return {};
        const atom::Binding bd = atom::massBinding(Z, N);
        subject = std::string(atom::name(Z)) + "-" + std::to_string(Z + N);
        return calc::atomRecipes(subject, Z, bd.real ? atom::nuclearMassMe(Z, N) * atom::kMeKg : 0, app.atom.p.n, bd.real ? bd.B : -1, Z + N);
    }
    const molecule::Molecule& mo = app.molecule.mol();
    subject = mo.name;
    const double nu = app.molecule.vibOn() ? molecule::vib::nuShown(mo.id, app.molecule.vibMode) : 0;
    return calc::moleculeRecipes(mo.name, molecule::massU(mo), nu);
}

// The keypad (0.49, the owner: "you have to type numbers from memory?"): each key adds its text at the end of the line, or ⌫ takes the last character off.
struct Key { const char* label; const char* add; int kind; };   // kind 0: added as it is (digits, signs), 1: a name, added with a space before it, 2: ⌫
const Key kKeys[] = {
    {"7", "7", 0}, {"8", "8", 0}, {"9", "9", 0}, {"÷", " / ", 0}, {"(", "(", 0},
    {"4", "4", 0}, {"5", "5", 0}, {"6", "6", 0}, {"×", " * ", 0}, {")", ")", 0},
    {"1", "1", 0}, {"2", "2", 0}, {"3", "3", 0}, {"−", " - ", 0}, {"^", "^", 0},
    {"0", "0", 0}, {".", ".", 0}, {"×10^", "*10^", 0}, {"+", " + ", 0}, {"⌫", "", 2},
    {"m", "m", 1}, {"km", "km", 1}, {"s", "s", 1}, {"hr", "hr", 1}, {"kg", "kg", 1},
    {"J", "J", 1}, {"eV", "eV", 1}, {"K", "K", 1}, {"nm", "nm", 1}, {"au", "au", 1},
    {"c", "c", 1}, {"G", "G", 1}, {"hP", "hP", 1}, {"e", "e", 1}, {"π", "π", 1},
    {"√", "sqrt(", 1}, {"deg", "deg", 1}, {"to", "to", 1}, {"ans", "ans", 1}, {"Clear", "", 3},
};
void backspace() {   // ⌫: the last name tapped goes as a whole ("hr", not "h"); otherwise one character (a digit, a sign, π); the spaces round it go with it
    std::string s = C.buf;
    const auto trimSpaces = [&] { while (!s.empty() && s.back() == ' ') s.pop_back(); };
    trimSpaces();
    if (s.empty()) { setLine(""); return; }
    const auto nameByte = [](unsigned char c) { return std::isalnum(c) || c == '_' || c >= 0x80; };
    std::size_t a = s.size();
    while (a > 0 && nameByte(static_cast<unsigned char>(s[a - 1]))) a--;
    bool letter = false;
    for (std::size_t k = a; k < s.size(); k++) letter = letter || !std::isdigit(static_cast<unsigned char>(s[k]));
    if (a < s.size() && letter && (std::isalpha(static_cast<unsigned char>(s[a])) || static_cast<unsigned char>(s[a]) >= 0x80)) s.erase(a);   // a name (it starts with a letter)
    else {
        std::size_t n = s.size() - 1;
        while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80) n--;   // a whole UTF-8 character (π, ²)
        s.erase(n);
    }
    trimSpaces();
    setLine(s);
}

// 0.54.2: a matrix answer as an aligned grid. The entries are written with as many digits as fit the panel's width (10, then fewer; then the caption size;
// then a complex entry on two lines, its imaginary part under its real part); if even that is too wide (four columns of numbers with long exponents), the
// answer is written as wrapped text instead, so nothing is ever cut off.
struct Grid {
    bool on = false, fits = false, split = false;
    Type type = Type::Body;
    int digits = 10, rows = 0, cols = 0;
    std::vector<std::string> top, bottom;        // each entry's text (bottom: the imaginary part, only when split)
    std::vector<float> colW;
    float lead = 0, pad = 0, gap = 0, unitW = 0, w = 0, lineH = 0;
    std::string unit;
};
Grid layoutGrid(const calc::Result& r, float room) {
    Grid g;
    if (!r.ok || r.rows == 0 || r.list) return g;
    const sh::Frame& F = sh::F();
    g.on = true; g.rows = r.rows; g.cols = r.cols; g.unit = r.unit;
    struct Try { Type t; int digits; bool split; };
    const Try tries[] = {{Type::Body, 10, false}, {Type::Body, 8, false}, {Type::Body, 6, false}, {Type::Body, 5, false}, {Type::Body, 4, false},
                         {Type::Caption, 4, false}, {Type::Caption, 3, false}, {Type::Caption, 3, true}};
    for (const Try& t : tries) {
        g.type = t.t; g.digits = t.digits; g.split = false;
        g.top.clear(); g.bottom.clear();
        g.colW.assign(static_cast<std::size_t>(r.cols), 0.0f);
        for (std::size_t k = 0; k < r.cells.size(); k++) {
            const double re = r.cells[k].real(), im = r.cells[k].imag();
            std::string a = calc::complexText(re, im, false, t.digits), b;
            if (t.split && re != 0 && im != 0) {
                a = calc::formatNumber(re, t.digits);
                b = std::string(im < 0 ? "− " : "+ ") + (std::fabs(im) == 1 ? "" : calc::formatNumber(std::fabs(im), t.digits)) + "i";
                g.split = true;
            }
            float& cw = g.colW[k % static_cast<std::size_t>(r.cols)];
            cw = std::max(cw, std::max(sh::width(t.t, true, a), b.empty() ? 0.0f : sh::width(t.t, true, b)));
            g.top.push_back(a); g.bottom.push_back(b);
        }
        g.lead = sh::width(Type::Body, true, "= ");
        g.pad = static_cast<float>(F.m.s8);
        g.gap = static_cast<float>(F.m.s12);
        g.unitW = g.unit.empty() ? 0.0f : static_cast<float>(F.m.s8) + sh::width(Type::Body, true, g.unit);
        g.lineH = sh::lineH(t.t);
        g.w = g.lead + 2 * g.pad + g.gap * static_cast<float>(r.cols - 1) + g.unitW;
        for (float cw : g.colW) g.w += cw;
        if (g.w <= room) { g.fits = true; break; }
    }
    return g;
}
void drawGrid(sh::Panel& p, const Grid& g) {
    const sh::Frame& F = sh::F();
    const ImVec2 o = sh::here(p);
    const float cellH = g.lineH * (g.split ? 2.0f : 1.0f), rowGap = static_cast<float>(F.m.s4), h = static_cast<float>(g.rows) * cellH + static_cast<float>(g.rows - 1) * rowGap;
    const float padY = static_cast<float>(F.m.s4), midY = o.y + padY + h / 2;
    sh::text(p.dl, Type::Body, true, ImVec2(o.x, midY - sh::lineH(Type::Body) / 2), sh::cText(), "=");
    float inner = 0;
    for (float cw : g.colW) inner += cw;
    inner += g.gap * static_cast<float>(g.cols - 1);
    const float x0 = o.x + g.lead, x1 = x0 + 2 * g.pad + inner, y0 = o.y, y1 = o.y + 2 * padY + h;
    const float th = std::max(1.0f, std::round(static_cast<float>(F.dpi) * 1.5f)), tick = static_cast<float>(F.m.s4);
    for (int side = 0; side < 2; side++) {                                     // the two square brackets
        const float x = side ? x1 : x0, dir = side ? -1.0f : 1.0f;
        p.dl->AddLine(ImVec2(x, y0), ImVec2(x, y1), sh::cText(), th);
        p.dl->AddLine(ImVec2(x, y0), ImVec2(x + dir * tick, y0), sh::cText(), th);
        p.dl->AddLine(ImVec2(x, y1), ImVec2(x + dir * tick, y1), sh::cText(), th);
    }
    float x = x0 + g.pad;
    for (int c = 0; c < g.cols; c++) {
        const float cw = g.colW[static_cast<std::size_t>(c)];
        for (int r = 0; r < g.rows; r++) {
            const std::size_t k = static_cast<std::size_t>(r * g.cols + c);
            const float y = o.y + padY + static_cast<float>(r) * (cellH + rowGap);
            const bool two = !g.bottom[k].empty();
            // right-aligned: the last digits of a column stand under each other
            sh::text(p.dl, g.type, true, ImVec2(x + cw - sh::width(g.type, true, g.top[k]), two ? y : y + (cellH - g.lineH) / 2), sh::cText(), g.top[k]);
            if (two) sh::text(p.dl, g.type, true, ImVec2(x + cw - sh::width(g.type, true, g.bottom[k]), y + g.lineH), sh::cText(), g.bottom[k]);
        }
        x += cw + g.gap;
    }
    if (!g.unit.empty()) sh::text(p.dl, Type::Body, true, ImVec2(x1 + static_cast<float>(F.m.s8), midY - sh::lineH(Type::Body) / 2), sh::cText(), g.unit);
    p.y += 2 * padY + h + static_cast<float>(F.m.s8);
}

// A row whose label is cut with "…" to fit
bool fitRow(sh::Panel& p, const std::string& id, const std::string& label) {
    std::string s = label;
    const float room = p.w - 2.0f * static_cast<float>(sh::F().m.s8);
    while (sh::width(Type::Body, false, s) > room && s.size() > 4) app::shell::dropLastChar(s);
    if (s != label) s += "…";
    const bool c = sh::pRow(p, id.c_str(), s);
    noteItem(id.c_str());
    return c;
}

}  // namespace

bool calcOpen() { return C.open; }
void calcToggle() { C.open = !C.open; if (C.open) C.focus = true; }
void calcClose() { C.open = false; C.fieldActive = false; }
bool calcFieldActive() { return C.open && C.fieldActive; }
void calcInsert(const std::string& text) { C.open = true; append(text); }
bool calcDefine(const std::string& name, const std::string& value, std::string& why) {
    const calc::Result r = calc::evaluate(name + " = " + value, C.haveAns ? &C.ans : nullptr, &C.vars);
    if (!r.ok || r.assigned.empty()) { why = r.ok ? "not a variable" : r.error; return false; }
    C.vars[r.assigned] = r.q;
    return true;
}

Rect calcPanel(AppState& app, const app::shell::Frame& fr, const app::shell::Metrics& m, const std::vector<Rect>& cards) {
    if (!C.open || app.benchRunning) { C.fieldActive = false; return Rect{0, 0, 0, 0}; }
    Rect r = app::shell::placePanel(app::shell::Anchor::TopRight, m.em(26), sh::lastHeight("##calc", static_cast<float>(m.em(20))), fr, m, 0);
    for (const Rect& c : cards) r = app::shell::sideStep(r, c, fr.window, m.s12, m.s12);
    sh::Panel p;
    if (!sh::beginPanel("##calc", r, p, sh::Shape::Panel, true, true)) return r;
    const sh::Frame& F = sh::F();

    // ---- the title and the close button ----
    {
        const ImVec2 o = sh::here(p);
        const float ib = static_cast<float>(m.iconBtn);
        sh::text(p.dl, Type::Title, true, ImVec2(o.x, o.y + (ib - sh::lineH(Type::Title)) / 2), sh::cText(), "Calculator");
        if (sh::iconButton("calc.close", ImVec2(o.x + p.w - ib, o.y), ImVec2(ib, ib), sh::Icon::Close, "Close (Esc)")) { calcClose(); sh::endPanel(p); return r; }
        noteItem("calc.close");
        p.y += ib + static_cast<float>(m.s8);
    }
    // ---- the line ----
    ImGui::SetCursorScreenPos(sh::here(p));
    ImGui::PushFont(nullptr, app::shell::fontBase(Type::Body, F.dpi, F.tf));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(static_cast<float>(m.s8), (m.control - m.body) / 2.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ui::corner(static_cast<float>(m.radiusCtl)));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(sh::cFace()));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(sh::cText()));
    ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImGui::ColorConvertU32ToFloat4(sh::cMuted()));
    ImGui::SetNextItemWidth(p.w);
    if (C.focus) { ImGui::SetKeyboardFocusHere(); C.focus = false; }
    const std::string fieldId = "##calcfield" + std::to_string(C.gen);
    const bool enter = ImGui::InputTextWithHint(fieldId.c_str(), "Type a sum, like 384400 km / c", C.buf, sizeof C.buf, ImGuiInputTextFlags_EnterReturnsTrue);
    C.fieldActive = ImGui::IsItemActive();
    noteItem("calc.field");
    if (enter) C.focus = true;                         // the box keeps the keyboard after Enter
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
    ImGui::PopFont();
    p.y += static_cast<float>(m.control + m.s8);

    // ---- the keypad: 5 keys a row; the first 4 rows numbers and signs, then units, then constants ----
    {
        const float gap = static_cast<float>(m.s4), kw = (p.w - 4 * gap) / 5, kh = static_cast<float>(m.control);
        const int count = static_cast<int>(sizeof kKeys / sizeof kKeys[0]);
        for (int i = 0; i < count; i++) {
            const Key& k = kKeys[i];
            const int row = i / 5, col = i % 5;
            const float extra = row >= 4 ? static_cast<float>(m.s8) : 0;           // a little space between the number pad and the names
            const ImVec2 at(p.origin.x + col * (kw + gap), p.origin.y + p.y + row * (kh + gap) + extra);
            const std::string id = std::string("calc.key.") + k.label;
            if (sh::button(id.c_str(), at, ImVec2(kw, kh), k.label, k.kind == 0 && !(k.label[0] >= '0' && k.label[0] <= '9') && k.label[0] != '.' ? sh::Face::Quiet : sh::Face::Secondary)) {
                if (k.kind == 0) setLine(std::string(C.buf) + k.add);
                else if (k.kind == 1) append(k.add);
                else if (k.kind == 2) backspace();
                else setLine("");
            }
            noteItem(id.c_str());
        }
        const int rows = (count + 4) / 5;
        p.y += rows * (kh + gap) + static_cast<float>(m.s8) + static_cast<float>(m.s8);
    }
    // ---- the answer, worked out as you type ----
    const calc::Result res = calc::evaluate(C.buf, C.haveAns ? &C.ans : nullptr, &C.vars);
    if (enter) keep(app, res);
    const Grid grid = layoutGrid(res, p.w);                                // 0.54.2: a matrix answer's grid
    if (shellLogOn()) {
        std::string notesText;                                          // 0.50.1: the notes too (the UI check finds the ω note)
        for (const std::string& n : res.notes) notesText += (notesText.empty() ? "" : " / ") + n;
        const std::string line = std::string("PLCALC ok=") + (res.ok ? "1" : "0") + " number=" + res.number + " unit=" + res.unit + " ans=" + (C.haveAns ? calc::exactNumber(C.ans.v) : "-") +
                                 " hist=" + std::to_string(app.prefs.calcHistory.size()) + " lab=" + std::to_string(app.lab) +
                                 (grid.on ? " grid=" + std::to_string(grid.rows) + "x" + std::to_string(grid.cols) + " gfit=" + (grid.fits ? "1" : "0") + " gdigits=" + std::to_string(grid.digits) +
                                            " gsplit=" + (grid.split ? "1" : "0") + " gw=" + std::to_string(static_cast<int>(std::ceil(grid.w))) + " room=" + std::to_string(static_cast<int>(p.w)) : "") +
                                 (res.ok && res.list ? " list=" + std::to_string(res.rows) : "") + " err=" + res.error + " notes=" + notesText + " |" + C.buf;
        if (line != C.logged) { C.logged = line; std::printf("%s\n", line.c_str()); std::fflush(stdout); }
    }
    if (res.empty) {
        sh::pCaption(p, "A calculator that keeps track of units: 100 km / 2 hr gives a speed, and 5 km + 3 s is refused. Tap an example to try it.");
        int i = 0;
        for (const calc::Recipe& ex : calc::generalRecipes()) if (fitRow(p, "calc.example" + std::to_string(i++), ex.line + "   (" + ex.label + ")")) { setLine(ex.line); C.recipeLine = ex.line; C.recipeNote = ex.note; }
        if (fitRow(p, "calc.qmexamples", std::string(C.showQm ? "▾ " : "▸ ") + "QM examples")) C.showQm = !C.showQm;   // 0.50.1 (folded: the lab's formulas stay in view)
        i = 0;
        if (C.showQm)
            for (const calc::Recipe& ex : calc::qmRecipes()) if (fitRow(p, "calc.qm" + std::to_string(i++), ex.line + "   (" + ex.label + ")")) { setLine(ex.line); C.recipeLine = ex.line; C.recipeNote = ex.note; }
    } else if (res.ok) {
        if (grid.on && grid.fits) {                                         // 0.54.2: a matrix: the grid, its unit once
            drawGrid(p, grid);
            if (grid.digits < 10) sh::pCaption(p, "Entries shown to " + std::to_string(grid.digits) + " digits to fit here; ans and the next lines keep all of them.");
        } else if (grid.on) {
            sh::pText(p, Type::Body, true, sh::cText(), "= " + calc::matrixText(res.cells, res.rows, res.cols, false, 4) + (res.unit.empty() ? "" : " " + res.unit));
            sh::pCaption(p, "Too wide for a grid here: written row after row, to 4 digits; ans and the next lines keep all of them.");
        } else if (res.list) {                                              // eig's answer: a short list, on one line when it fits, else one value a line
            const std::string one = "= " + res.number + (res.unit.empty() ? "" : " " + res.unit);
            if (sh::width(Type::Title, true, one) <= p.w) sh::pText(p, Type::Title, true, sh::cText(), one);
            else if (sh::width(Type::Body, true, one) <= p.w) sh::pText(p, Type::Body, true, sh::cText(), one);
            else
                for (std::size_t k = 0; k < res.cells.size(); k++)
                    sh::pText(p, Type::Body, true, sh::cText(), std::string(k ? "   " : "= ") + calc::formatNumber(res.cells[k].real()) + (res.unit.empty() ? "" : " " + res.unit));
            sh::pCaption(p, std::to_string(res.rows) + (res.rows == 1 ? " eigenvalue" : " eigenvalues, smallest first"));
        } else
        sh::pText(p, Type::Title, true, sh::cText(), "= " + res.number + (res.unit.empty() ? "" : " " + res.unit));
        if (!res.kindWord.empty()) {
            const bool vowel = std::string("aeiou").find(res.kindWord[0]) != std::string::npos;
            sh::pCaption(p, std::string(vowel ? "an " : "a ") + res.kindWord);
        }
        for (const std::string& n : res.notes) sh::pCaption(p, n, false);
        if (!res.also.empty()) {
            std::string a;
            for (const std::string& x : res.also) a += (a.empty() ? "" : "    ") + x;
            sh::pCaption(p, a);
        }
        sh::pCaption(p, "Read as: " + res.readAs);
        if (C.recipeLine == C.buf && !C.recipeNote.empty()) sh::pCaption(p, C.recipeNote);
        for (const std::string& u : res.used) {                       // (a base unit's line, "m = metre", says nothing new: left out)
            bool base = false;
            for (const char* b : {"m = metre", "s = second", "kg = kilogram", "A = ampere", "K = kelvin", "mol = mole", "g = gram"}) base = base || u == b;
            if (!base) sh::pCaption(p, u);
        }
        const float bw = (p.w - static_cast<float>(m.s8)) / 2;
        const ImVec2 o = sh::here(p);
        if (sh::button("calc.keep", o, ImVec2(bw, static_cast<float>(m.control)), "Keep (Enter)")) { keep(app, res); C.focus = true; }
        noteItem("calc.keep");
        if (sh::button("calc.clear", ImVec2(o.x + bw + static_cast<float>(m.s8), o.y), ImVec2(bw, static_cast<float>(m.control)), "Clear")) setLine("");
        noteItem("calc.clear");
        p.y += static_cast<float>(m.control + m.s8);
        if (C.haveAns) sh::pCaption(p, "ans is the last answer you kept: " + calc::valueText(C.ans));
        if (!C.vars.empty()) {                                  // 0.50.2: the variables kept (x = 2 m)
            std::string v;
            for (const auto& [name, q] : C.vars) v += (v.empty() ? "" : ", ") + name + " = " + calc::valueText(q);
            sh::pCaption(p, "Your variables: " + v);
        }
    } else {
        sh::pText(p, Type::Body, false, sh::cDanger(), res.error);
        if (res.errPos >= 0 && res.errLen > 0 && res.errPos + res.errLen <= static_cast<int>(std::strlen(C.buf)))
            sh::pCaption(p, "The part: \"" + std::string(C.buf + res.errPos, static_cast<std::size_t>(res.errLen)) + "\"");
        const float bw = p.w;
        if (sh::button("calc.clear", sh::here(p), ImVec2(bw, static_cast<float>(m.control)), "Clear")) setLine("");
        noteItem("calc.clear");
        p.y += static_cast<float>(m.control + m.s8);
    }

    // ---- the selected thing's formulas ----
    std::string subject;
    const std::vector<calc::Recipe> rec = labRecipes(app, subject);
    sh::pGap(p, m.s4);
    if (!rec.empty()) {
        sh::pHeading(p, "Formulas for " + subject + " (its numbers now)");
        for (std::size_t i = 0; i < rec.size(); i++)
            if (fitRow(p, "calc.recipe" + std::to_string(i), rec[i].label)) { setLine(rec[i].line); C.recipeLine = rec[i].line; C.recipeNote = rec[i].note; }
    } else {
        sh::pHeading(p, "Formulas");
        sh::pCaption(p, app.lab == 0 ? "Pick a planet, dwarf planet or comet: its formulas appear here, with its numbers." : "Pick something in the lab: its formulas appear here.");
    }
    // ---- the history ----
    if (!app.prefs.calcHistory.empty()) {
        sh::pGap(p, m.s4);
        sh::pHeading(p, "History (tap to use again)");
        const std::vector<std::string> hist = app.prefs.calcHistory;    // (a copy: a tap changes nothing here, but keep() may run later this frame)
        for (std::size_t i = 0; i < hist.size(); i++) if (fitRow(p, "calc.hist" + std::to_string(i), hist[i])) setLine(hist[i]);
    }
    // ---- the constants and units ----
    sh::pGap(p, m.s4);
    if (fitRow(p, "calc.consts", std::string(C.showConst ? "▾ " : "▸ ") + "Constants and units (tap to add)")) C.showConst = !C.showConst;
    if (C.showConst) {
        int i = 0;
        for (const calc::Symbol& s : calc::symbols()) {
            if (!s.listed) continue;
            const std::string u = calc::unitText(s.q.dim);
            if (fitRow(p, "calc.sym" + std::to_string(i++), s.name + "   " + s.meaning)) append(s.name);
            if (ImGui::IsItemHovered() && !s.source.empty()) ui::SetTooltip("%s", (s.name + " = " + (s.published.empty() ? calc::formatNumber(s.q.v) : s.published) + (u.empty() ? "" : " " + u) + "\n" + s.source).c_str());
        }
        {   // 0.54.2: the Pauli matrices (not in the table of constants: they are matrices, pure mathematics)
            const char* const pauli[3][2] = {{"σx", "Pauli matrix [[0, 1], [1, 0]]  (also typed sigmax)"}, {"σy", "Pauli matrix [[0, -i], [i, 0]]  (also typed sigmay)"},
                                             {"σz", "Pauli matrix [[1, 0], [0, -1]]  (also typed sigmaz)"}};
            for (int k = 0; k < 3; k++) if (fitRow(p, "calc.pauli" + std::to_string(k), std::string(pauli[k][0]) + "   " + pauli[k][1])) append(pauli[k][0]);
        }
        std::string f;
        for (const std::string& n : calc::functionNames()) f += (f.empty() ? "" : " ") + n;
        sh::pCaption(p, "Functions: " + f + ". Write them with brackets: sqrt(2); several arguments with commas: besselj(0, 2.4).");
    }
    // ---- how to write things ----
    if (fitRow(p, "calc.help", std::string(C.showHelp ? "▾ " : "▸ ") + "How to write things")) C.showHelp = !C.showHelp;
    if (C.showHelp) {
        sh::pCaption(p, "A number next to a unit belongs to it: 100 km / 2 hr is (100 km) / (2 hr). Powers: m^2, m² or s^-1. Numbers: 1.5e3 = 1500, no commas.", false);
        sh::pCaption(p, "\"to\" shows the answer in a unit: 5 km to mi, hP c / 500 nm to eV. ans is the last answer you kept (Enter).", false);
        sh::pCaption(p, "Hours are hr (h could also be Planck's constant, which is hP). Temperatures in kelvin (K). Angles in radians, or write deg: sin(30 deg).", false);
        sh::pCaption(p, "When a line could be read two ways, like 4/3 π r^3, the calculator asks for brackets instead of guessing: (4/3) π r^3.", false);
        sh::pCaption(p, "Commas only separate a function's arguments, inside its brackets: cg(1, 1, 0.5, -0.5, 0.5, 0.5). Anywhere else they are refused (no 384,400).", false);
        sh::pCaption(p, "1/(4 π ε0) e^2 / r is read as (1/(4 π ε0)) × e^2 / r, the textbook way of writing Coulomb's constant; other 1/(...) x lines ask for brackets.", false);
        sh::pCaption(p, "After \"to\", energy, wavenumber, frequency and temperature convert into each other, and mass into energy: 1 eV to cm^-1, 1 eV to K, 1 u to MeV "
                        "(E = hcν̃, E = hf, E = kT, E = mc²; a note names the factor). In Hz a frequency is cyclic (E = hf), in rad/s angular (E = ħω): 1 eV to rad/s, "
                        "1 GHz to rad/s (×2π). A photon's vacuum wavelength: 1 eV to nm, 500 nm to eV (E = hc/λ).", false);
        sh::pCaption(p, "ħ and ω: a frequency from a line whose ħ is not divided by an h (E/ħ, μB B/ħ) is angular, ω in rad/s: a note gives f = ω/2π. "
                        "E/h, or ħ/h × anything, is cyclic: no note. With no \"to\" the note says first that a rate (a decay rate) has no 2π.", false);
        sh::pCaption(p, "Complex numbers: i is the imaginary unit (3 + 4i); re, im, conj, abs, arg, exp(i π). sqrt of a negative number is imaginary only in a line "
                        "that uses i. Units belong to the whole number: (2 + 3i) m.", false);
        sh::pCaption(p, "Matrices, up to 4x4: [[1, 2], [3, 4]], each row in its own brackets ([[1], [2]] is a column). One unit for the whole matrix: [[0, 1], [1, 0]] eV. "
                        "+ − *, / by a number, whole powers, transpose, dagger (the conjugate transpose), conj, det, tr, identity(n), kron(A, B), eig(M). The Pauli matrices "
                        "are sigmax, sigmay, sigmaz (or σx, σy, σz; sigma alone is the Stefan-Boltzmann constant). Examples: sigmax * sigmay gives i σz; "
                        "eig([[0.3/2, 0.4/2], [0.4/2, -0.3/2]] eV) to eV gives −0.25 and 0.25 (a two-level system, ±√(Δ² + Ω²)/2); kron(sigmax, sigmaz) is 4x4.", false);
        sh::pCaption(p, "Matrix limits: at most 4x4. eig gives the eigenvalues of Hermitian matrices only (equal to their conjugate transpose), smallest first, and only "
                        "the values: no eigenvectors, no inverse, no matrix exponential (sqrt, exp, sin ... of a matrix are refused). Ordinary 64-bit arithmetic: trust "
                        "an eigenvalue to about 13 digits of the largest one (the grid shows up to 10). No uncertainties.", false);
        sh::pCaption(p, "Variables: x = 2 m names a value (Enter keeps it); use x in the next lines. Names of units, constants and functions can't be used.", false);
        sh::pCaption(p, "Too small: a value below 2.2×10⁻³⁰⁸ is shown as 0, with a note, and + or sum() go on with 0 (1 + exp(-800) = 1). Too big (over 10³⁰⁸) is refused.", false);
        sh::pCaption(p, "On plain numbers: fact(n) or n!, gamma, lngamma, erf, erfc, besselj(n, x), sphbessel(l, x), legendre(l, x), assoclegendre(l, m, x) (with the "
                        "Condon-Shortley phase (-1)^m), laguerre(n, x), assoclaguerre(n, k, x) (L_n^(k), no n! factor), hermite(n, x) (physicists'), binom(n, k), "
                        "cg(j1, m1, j2, m2, J, M) and wigner3j(j1, j2, j3, m1, m2, m3) (write 1/2 or 0.5; exact sums: about 14 digits for every j up to 200). "
                        "Where the standard value is 0 the answer is 0 with a note: |m| > l or > j, k > n, M ≠ m1 + m2, the triangle rule.", false);
        sh::pCaption(p, "sum(1/k^2, k, 1, 100) adds over a whole-number index; solve(x^2 - 2, x, 0, 2) finds where an expression is 0 between a and b (it must change sign "
                        "there). The index or unknown must not be a unit's name (J is the joule: use j).", false);
        sh::pCaption(p, "Every constant shows its value and source when it is used. The calculator uses the computer's 64-bit numbers: about 16 digits; it shows 10.");
    }
    sh::endPanel(p);
    return r;
}

}  // namespace ui
