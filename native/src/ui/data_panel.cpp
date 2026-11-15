// The Data panel (0.51): see data_panel.h. The tables, the fits and the text in and out are analysis/* (tested); here: the drawing, the plot, the choices and the recording.
#include "ui/data_panel.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "imgui.h"

#include "analysis/fit.h"
#include "analysis/lab_tables.h"
#include "analysis/table.h"
#include "app/calc.h"
#include "app/shot.h"
#include "orbit/card_text.h"
#include "orbit/units.h"
#include "ui/calc_panel.h"
#include "ui/changelog.h"
#include "ui/look.h"
#include "ui/orbit_panel.h"
#include "ui/shell_test.h"
#include "ui/shell_widgets.h"
#include "ui/style_draw.h"

namespace ui {

using app::shell::Rect;
using app::shell::Type;
using namespace analysis;

namespace {

constexpr int kRecMax = 20000;           // samples a recording keeps (then it stops, and says so)
constexpr std::size_t kUserMax = 65536;  // the text box for the user's data

struct DataState {
    bool open = false, fieldActive = false;
    std::string tableId[3] = {"planets", "ionlines", "modes"};   // the table on show in each lab
    LabContext built;                    // the selection the table was made for
    std::string builtId;
    int builtLab = -1;
    Table table;
    bool have = false;
    std::string why;                     // why there is no table
    // the plot
    int xCol = -1, yCol = -1;
    Transform tx = Transform::None, ty = Transform::None;
    std::string group;                   // "" = every row
    int model = -1;                      // -1: no fit
    bool useSigma = true, showBand = true;
    std::vector<double> held;            // 0.51.1: one per parameter of the model: a number holds it there, NaN leaves it free
    bool logX = false, logY = false;     // 0.51.1: logarithmic plot axes (the drawing only)
    bool scaleErr = false;               // 0.51.1: a weighted fit's errors scaled by √(χ²/dof)
    bool showCorr = false;
    std::string expect;                  // the chosen worked analysis's textbook answer
    // what the plot shows, made when a choice changes
    bool dirty = true;
    std::vector<double> X, Y, SX, SY;
    std::vector<std::size_t> rowOf;
    int skipped = 0;
    bool weightedY = false, weightedX = false;
    std::string weightNote;
    FitResult fit;
    // which lists are open
    bool pickTable = false, pickX = false, pickY = false, pickModel = false, showSources = false, showTable = true, showPresets = true;
    // the user's data
    char user[kUserMax] = "# Type or paste your data: one row per line, columns separated by commas, tabs or spaces.\n"
                          "# An optional first line names the columns with units; a column named σ… is the uncertainty of the one before.\n"
                          "# The example (made-up numbers): a pendulum, T = 2π √(L/g). Fit a power law (n = 1/2), Send, then type 4 π^2 / fit_Apow^2 to m/s^2.\n"
                          "L (m), T (s), σT (s)\n0.20, 0.90, 0.02\n0.40, 1.27, 0.02\n0.60, 1.55, 0.03\n0.80, 1.79, 0.03\n1.00, 2.01, 0.03\n";
    int userGen = 0;
    Table userTable;
    bool userOk = false;
    std::string userWhy;
    std::vector<std::string> userNotes;
    bool userParsed = false;
    // the recording (Orbit Lab)
    bool rec = false, recReal = false;
    int recBody = -1;
    std::string recName, recStop;
    double recT0 = 0, recLast = 0, recStep = 0;
    std::vector<RecSample> rs;           // 0.51.1: with the osculating orbit (was t, r, v only)
    std::vector<double> rt;              // the simulation's time of each sample, s
    bool recGr = false;
    std::size_t recBuilt = 0;
    int recThinned = 0;                  // how many times the recording kept every other sample to go on (each doubles the step)
    std::string logged;
    std::string sendNote;
};
DataState D;

std::string fmtg(double v, int digits = 6) { char b[64]; std::snprintf(b, sizeof b, "%.*g", digits, v); return b; }

// v ± e with e to 2 significant digits and v to the same place; a common power of ten when the numbers are large or small: "(1.234 ± 0.012) × 10⁻⁵"
std::string sup(int e) {
    static const char* const d[10] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
    std::string s = e < 0 ? "⁻" : "";
    const std::string n = std::to_string(e < 0 ? -e : e);
    for (char c : n) s += d[c - '0'];
    return s;
}
std::string plusMinus(double v, double e) {
    if (!std::isfinite(v)) return "—";
    if (!(e > 0) || !std::isfinite(e)) return calc::formatNumber(v, 8);
    const double big = std::max(std::fabs(v), e);
    const int E = static_cast<int>(std::floor(std::log10(big)));
    const bool sci = E >= 5 || E <= -4;
    const double sc = sci ? std::pow(10.0, E) : 1;
    const double m = v / sc, me = e / sc;
    const int dec = std::max(0, std::min(12, 1 - static_cast<int>(std::floor(std::log10(me)))));
    char b[128];
    std::snprintf(b, sizeof b, "%.*f ± %.*f", dec, m, dec, me);
    return sci ? "(" + std::string(b) + ") × 10" + sup(E) : std::string(b);
}

LabContext contextOf(const AppState& app) {
    LabContext c;
    c.lab = app.lab;
    c.Z = app.atom.p.Z;
    c.N = app.atom.p.N;
    c.molecule = app.molecule.mol().id;
    c.T = app.molecule.liveK;              // 0.53: the rotation tables' temperature and HCl's isotope
    c.rotIso = app.molecule.rotIso;
    c.fieldT = app.atom.p.bfield;          // 0.54.0: the Zeeman tables of the line shown split
    c.zKind = app.atom.zeemanKind();
    c.zLine = app.atom.zeemanLine();
    return c;
}
bool sameContext(const LabContext& a, const LabContext& b) {
    if (a.lab != b.lab) return false;
    if (a.lab == 1) return a.Z == b.Z && a.N == b.N && a.fieldT == b.fieldT && a.zKind == b.zKind && a.zLine == b.zLine;
    if (a.lab == 2) return a.molecule == b.molecule && a.T == b.T && a.rotIso == b.rotIso;
    return true;
}
bool numericCol(int c) { return c >= 0 && c < static_cast<int>(D.table.cols.size()) && !D.table.cols[std::size_t(c)].isText(); }
int firstNumeric(int from) {
    for (int i = std::max(0, from); i < static_cast<int>(D.table.cols.size()); i++) if (numericCol(i)) return i;
    for (int i = 0; i < static_cast<int>(D.table.cols.size()); i++) if (numericCol(i)) return i;
    return -1;
}
void setModel(int m) {
    D.model = m;
    D.held.assign(m >= 0 ? modelInfo(static_cast<Model>(m)).params.size() : 0, NAN);
}
void applyPreset(const Preset& p) {
    D.xCol = p.x; D.yCol = p.y; D.tx = p.tx; D.ty = p.ty;
    setModel(p.fit ? static_cast<int>(p.model) : -1);
    if (p.hold >= 0 && std::size_t(p.hold) < D.held.size()) D.held[std::size_t(p.hold)] = p.holdValue;
    D.logX = p.logX; D.logY = p.logY;
    D.group = p.group;
    D.useSigma = p.useSigma;
    D.expect = p.label + ": " + p.expect;
    D.dirty = true;
}
// a new table: keep the chosen columns where the new table has columns of the same names, else take its first worked analysis
void takeTable(Table t) {
    const std::string xn = numericCol(D.xCol) ? D.table.cols[std::size_t(D.xCol)].name : "", yn = numericCol(D.yCol) ? D.table.cols[std::size_t(D.yCol)].name : "";
    const bool same = D.have && t.id == D.table.id;
    D.table = std::move(t);
    D.have = true;
    D.dirty = true;
    if (same && D.table.find(xn) >= 0 && D.table.find(yn) >= 0) { D.xCol = D.table.find(xn); D.yCol = D.table.find(yn); return; }
    D.group.clear();
    D.expect.clear();
    if (!D.table.presets.empty()) applyPreset(D.table.presets[0]);
    else { D.xCol = firstNumeric(0); D.yCol = firstNumeric(D.xCol + 1); D.tx = D.ty = Transform::None; setModel(-1); D.logX = D.logY = false; }
}
Table recordingTable() {
    return orbitRecording(D.recName.empty() ? "no body" : D.recName, D.rs, D.recReal, D.recGr);
}

// The table for this lab and selection (made again when the selection changes)
void refreshTable(const AppState& app) {
    const LabContext c = contextOf(app);
    std::string& id = D.tableId[std::clamp(app.lab, 0, 2)];
    if (id == "user") {
        if (!D.userParsed) { D.userOk = parseText(D.user, D.userTable, D.userWhy, D.userNotes); D.userParsed = true; if (D.userOk) takeTable(D.userTable); }
        if (D.builtId != "user" || D.builtLab != app.lab) { D.builtId = "user"; D.builtLab = app.lab; if (D.userOk) takeTable(D.userTable); else D.have = false; }
        D.why = D.userOk ? "" : D.userWhy;
        return;
    }
    if (id == "recording") {
        if (app.lab != 0) { id = "planets"; }
        else {
            static double lastBuild = -1;                 // while it records, the table and the fit are made again at most twice a second
            const double now = ImGui::GetTime();
            if (D.builtId != "recording" || D.builtLab != 0 || (D.recBuilt != D.rt.size() && (!D.rec || now - lastBuild > 0.5))) {
                takeTable(recordingTable()); D.builtId = "recording"; D.builtLab = 0; D.recBuilt = D.rt.size(); lastBuild = now;
            }
            D.why = D.rt.empty() ? "Nothing recorded yet: pick a planet, press Record and let time run." : "";
            return;
        }
    }
    if (D.builtId == id && D.builtLab == app.lab && sameContext(c, D.built)) return;
    Table t;
    if (!makeTable(id, c, t)) {                     // not for this selection (an atom with no quantum defects ...): the lab's first table
        const std::vector<TableRef> refs = labTables(c);
        if (!refs.empty() && makeTable(refs[0].id, c, t)) id = refs[0].id;
        else { D.have = false; D.why = "No table for this selection."; D.builtId = id; D.builtLab = app.lab; D.built = c; return; }
    }
    D.builtId = id; D.builtLab = app.lab; D.built = c;
    D.why.clear();
    takeTable(std::move(t));
}

// The points of the plot and the fit, when a choice changed
void rebuild() {
    D.dirty = false;
    D.X.clear(); D.Y.clear(); D.SX.clear(); D.SY.clear(); D.rowOf.clear();
    D.skipped = 0;
    D.fit = FitResult();
    D.weightNote.clear();
    if (!D.have || !numericCol(D.xCol) || !numericCol(D.yCol)) return;
    const Column& cx = D.table.cols[std::size_t(D.xCol)];
    const Column& cy = D.table.cols[std::size_t(D.yCol)];
    int ySig = 0;
    for (std::size_t r = 0; r < D.table.rows(); r++) {
        if (!D.group.empty() && D.table.group >= 0 && cellText(D.table, D.table.group, r) != D.group) continue;
        const double xv = r < cx.v.size() ? cx.v[r] : NAN, yv = r < cy.v.size() ? cy.v[r] : NAN;
        const double sx = r < cx.sigma.size() ? cx.sigma[r] : NAN, sy = r < cy.sigma.size() ? cy.sigma[r] : NAN;
        if (!std::isfinite(xv) || !std::isfinite(yv)) { D.skipped++; continue; }
        double X, SX, Y, SY;
        if (!applyTransform(D.tx, xv, sx, X, SX) || !applyTransform(D.ty, yv, sy, Y, SY)) { D.skipped++; continue; }
        D.X.push_back(X); D.Y.push_back(Y); D.SX.push_back(SX); D.SY.push_back(SY); D.rowOf.push_back(r);
        if (std::isfinite(SY) && SY > 0) ySig++;
    }
    const std::size_t n = D.X.size();
    D.weightedY = D.useSigma && n > 0 && ySig == static_cast<int>(n);
    D.weightedX = false;
    if (D.useSigma) for (double s : D.SX) D.weightedX = D.weightedX || (std::isfinite(s) && s > 0);
    if (D.useSigma && ySig > 0 && ySig < static_cast<int>(n)) D.weightNote = std::to_string(n - ySig) + " of the " + std::to_string(n) + " points have no y uncertainty: the fit counts every point the same.";
    if (D.weightedX && !D.weightedY) { D.weightedX = false; D.weightNote = "x has uncertainties but y has none: the fit counts every point the same (give y uncertainties to use x's too)."; }
    if (D.model < 0) return;
    std::vector<double> sy, sx;
    if (D.weightedY) sy = D.SY;
    if (D.weightedX) { sx = D.SX; for (double& s : sx) if (!std::isfinite(s)) s = 0; }
    if (D.held.size() != modelInfo(static_cast<Model>(D.model)).params.size()) D.held.assign(modelInfo(static_cast<Model>(D.model)).params.size(), NAN);
    D.fit = fitModel(static_cast<Model>(D.model), D.X, D.Y, sy, sx, D.held);
    if (D.scaleErr && D.fit.ok && D.fit.weighted) D.fit = scaledErrors(D.fit);
}

// ---- small pieces of the panel ----
bool fitRow(sh::Panel& p, const std::string& id, const std::string& label, bool selected = false) {
    std::string s = label;
    const float room = p.w - 2.0f * static_cast<float>(sh::F().m.s8);
    while (sh::width(Type::Body, false, s) > room && s.size() > 4) app::shell::dropLastChar(s);
    if (s != label) s += "…";
    const bool c = sh::pRow(p, id.c_str(), s, selected);
    noteItem(id.c_str());
    if (s != label && ImGui::IsItemHovered()) ui::SetTooltip("%s", label.c_str());
    return c;
}
std::string colLabel(int c, Transform t) {
    if (c < 0 || c >= static_cast<int>(D.table.cols.size())) return "—";
    const Column& col = D.table.cols[std::size_t(c)];
    return axisLabel(col.name, col.unit, t);
}
// a choice of column and transform for one axis
void axisChooser(sh::Panel& p, const char* which, int& col, Transform& t, bool& open) {
    const std::string id = std::string("data.") + which;
    if (fitRow(p, id, std::string(open ? "▾ " : "▸ ") + which + ": " + colLabel(col, t))) open = !open;
    if (!open) return;
    for (int i = 0; i < static_cast<int>(D.table.cols.size()); i++) {
        if (!numericCol(i)) continue;
        const Column& c = D.table.cols[std::size_t(i)];
        if (fitRow(p, id + "col" + std::to_string(i), "    " + c.name + (c.unit.empty() ? "" : " (" + c.unit + ")"), i == col)) { col = i; D.dirty = true; D.expect.clear(); }
    }
    std::vector<std::string> labels;
    for (int k = 0; k < static_cast<int>(Transform::Count); k++) labels.push_back(transformName(static_cast<Transform>(k)));
    int cur = static_cast<int>(t);
    if (sh::pSegmented(p, (id + "t").c_str(), labels, &cur)) { t = static_cast<Transform>(cur); D.dirty = true; D.expect.clear(); }
}

// nice tick steps: 1, 2, 5 × 10^k
double niceStep(double range, int want) {
    if (!(range > 0)) return 1;
    const double raw = range / std::max(1, want), mag = std::pow(10.0, std::floor(std::log10(raw))), f = raw / mag;
    return (f < 1.5 ? 1 : f < 3.5 ? 2 : f < 7.5 ? 5 : 10) * mag;
}
// the power of ten an axis's tick labels are shown in, when its numbers are big or small: 10^4 for 0 ... 120000 (the labels 0, 2, 4 ... 12, "× 10⁴" after the axis name)
int axisPower(double lo, double hi) {
    const double a = std::max(std::fabs(lo), std::fabs(hi));
    if (!(a > 0) || (a < 1e5 && a >= 1e-3)) return 0;
    return static_cast<int>(std::floor(std::log10(a)));
}
std::string tickText(double v, double step) {
    if (std::fabs(v) < step * 1e-9) v = 0;
    const int dec = std::max(0, -static_cast<int>(std::floor(std::log10(step) + 1e-9)));
    char b[32];
    std::snprintf(b, sizeof b, "%.*f", dec, v);
    return b;
}

// The plot: the points with their error bars, the fitted curve with its 1-sigma band; under it the residuals. Returns the height used.
// 0.51.1: either axis may be logarithmic (the drawing only: the points and the curve are put at log₁₀ of their values; the fit is of the values as they are).
void drawPlot(sh::Panel& p, const AppState& app) {
    const sh::Frame& F = sh::F();
    const auto& m = F.m;
    const float lh = sh::lineH(Type::Caption);
    const float h = static_cast<float>(m.em(15)), hr = static_cast<float>(m.em(5));
    const ImVec2 o = sh::here(p);
    ImDrawList* dl = p.dl;
    const std::size_t n = D.X.size();
    const float top = o.y + lh + static_cast<float>(m.s4);
    // the plotted coordinate of a value (log₁₀ on a log axis; NaN where a log axis cannot show it) and back
    const auto ux = [&](double v) { return D.logX ? (v > 0 ? std::log10(v) : NAN) : v; };
    const auto uy = [&](double v) { return D.logY ? (v > 0 ? std::log10(v) : NAN) : v; };
    const auto vx = [&](double u) { return D.logX ? std::pow(10.0, u) : u; };
    std::size_t shown = 0;
    for (std::size_t i = 0; i < n; i++) if (std::isfinite(ux(D.X[i])) && std::isfinite(uy(D.Y[i]))) shown++;
    if (shown == 0) {
        const std::string why = !D.have ? D.why : n == 0 ? "No points to plot (no values, or the transform is not defined for them)." : "No points on these axes: a log axis shows only values above 0.";
        sh::text(dl, Type::Body, false, ImVec2(o.x, top + h / 2), sh::cMuted(), why);
        p.y += lh + h + static_cast<float>(m.s8);
        return;
    }
    // the ranges (with the error bars and the fitted curve inside)
    double x0 = 1e300, x1 = -1e300, y0 = 1e300, y1 = -1e300;
    for (std::size_t i = 0; i < n; i++) {
        const double px0 = ux(D.X[i]), py0 = uy(D.Y[i]);
        if (!std::isfinite(px0) || !std::isfinite(py0)) continue;
        const double sx = std::isfinite(D.SX[i]) ? D.SX[i] : 0, sy = std::isfinite(D.SY[i]) ? D.SY[i] : 0;
        for (double v : {ux(D.X[i] - sx), px0, ux(D.X[i] + sx)}) if (std::isfinite(v)) { x0 = std::min(x0, v); x1 = std::max(x1, v); }
        for (double v : {uy(D.Y[i] - sy), py0, uy(D.Y[i] + sy)}) if (std::isfinite(v)) { y0 = std::min(y0, v); y1 = std::max(y1, v); }
    }
    const bool fitted = D.fit.ok && D.model >= 0;
    const Model mdl = fitted ? static_cast<Model>(D.model) : Model::Line;
    if (fitted) for (int k = 0; k <= 60; k++) { const double yy = uy(modelValue(mdl, D.fit.p.data(), vx(x0 + (x1 - x0) * k / 60.0))); if (std::isfinite(yy)) { y0 = std::min(y0, yy); y1 = std::max(y1, yy); } }
    if (!(x1 > x0)) { x0 -= D.logX ? 0.5 : 1; x1 += D.logX ? 0.5 : 1; }
    if (!(y1 > y0)) { y0 -= D.logY ? 0.5 : 1; y1 += D.logY ? 0.5 : 1; }
    const double px = 0.04 * (x1 - x0), py = 0.06 * (y1 - y0);
    x0 -= px; x1 += px; y0 -= py; y1 += py;
    // the ticks: a linear axis at 1, 2, 5 × 10^k (with a common power of ten); a log axis at whole decades, "10ⁿ" (or, under two decades, at nice steps of the log, the value written)
    struct Tick { double u; std::string s; };
    const auto ticks = [&](double a, double b, bool lg, int want, int& power) {
        std::vector<Tick> t;
        power = 0;
        if (lg && b - a >= 2) {
            const double st = std::max(1.0, std::ceil((b - a) / want));
            for (double u = std::ceil(a / st) * st; u <= b; u += st) t.push_back({u, "10" + sup(static_cast<int>(std::lround(u)))});
        } else if (lg) {
            const double st = niceStep(b - a, want);
            for (double u = std::ceil(a / st) * st; u <= b; u += st) t.push_back({u, calc::formatNumber(std::pow(10.0, u), 3)});
        } else {
            const double st = niceStep(b - a, want);
            power = axisPower(a, b);
            const double sc = std::pow(10.0, -power);
            for (double u = std::ceil(a / st) * st; u <= b; u += st) t.push_back({u, tickText(u * sc, st * sc)});
        }
        return t;
    };
    int xe = 0, ye = 0;
    const std::vector<Tick> yt = ticks(y0, y1, D.logY, 5, ye), xt = ticks(x0, x1, D.logX, 6, xe);
    // the residuals' scale (worked out here: its label sets the room at the left too, so it is never cut off)
    const bool showRes = fitted && D.fit.resid.size() == n;
    const bool norm = D.fit.weighted && D.weightedY;
    std::vector<double> rs(n, NAN);
    double ra = 0;
    std::string rLab;
    if (showRes) {
        for (std::size_t i = 0; i < n; i++) rs[i] = norm ? D.fit.resid[i] / D.SY[i] : D.fit.resid[i];
        for (double v : rs) if (std::isfinite(v)) ra = std::max(ra, std::fabs(v));
        if (!(ra > 0)) ra = 1;
        rLab = calc::formatNumber(ra, 2);
    }
    float tickW = 0;
    for (const Tick& t : yt) tickW = std::max(tickW, sh::width(Type::Caption, false, t.s));
    if (showRes) tickW = std::max(tickW, sh::width(Type::Caption, false, "−" + rLab));
    const float L = o.x + tickW + static_cast<float>(m.s8), R = o.x + p.w - static_cast<float>(m.s4), T = top, B = top + h;
    sh::text(dl, Type::Caption, true, o, sh::cMuted(), colLabel(D.yCol, D.ty) + (ye ? "   × 10" + sup(ye) : "") + (D.logY ? "   (log axis)" : ""));   // the y label above the plot, at its left
    const auto mx = [&](double u) { return L + static_cast<float>((u - x0) / (x1 - x0)) * (R - L); };
    const auto my = [&](double u) { return B - static_cast<float>((u - y0) / (y1 - y0)) * (B - T); };
    dl->AddRect(ImVec2(L, T), ImVec2(R, B), sh::cEdge());
    for (const Tick& t : yt) {
        const float yy = my(t.u);
        dl->AddLine(ImVec2(L, yy), ImVec2(R, yy), sh::cHair());
        sh::text(dl, Type::Caption, false, ImVec2(L - static_cast<float>(m.s4) - sh::width(Type::Caption, false, t.s), yy - lh / 2), sh::cMuted(), t.s);
    }
    for (const Tick& t : xt) {
        const float xx = mx(t.u);
        dl->AddLine(ImVec2(xx, T), ImVec2(xx, B), sh::cHair());
        sh::text(dl, Type::Caption, false, ImVec2(xx - sh::width(Type::Caption, false, t.s) / 2, B + static_cast<float>(m.s4)), sh::cMuted(), t.s);
    }
    dl->PushClipRect(ImVec2(L, T), ImVec2(R, B), true);
    // the band and the curve (sampled evenly along the plotted x)
    if (fitted) {
        const int K = 160;
        std::vector<ImVec2> up, lo, mid;
        for (int k = 0; k <= K; k++) {
            const double u = x0 + (x1 - x0) * k / K, xx = vx(u), yy = modelValue(mdl, D.fit.p.data(), xx);
            if (!std::isfinite(uy(yy))) continue;
            mid.push_back(ImVec2(mx(u), my(uy(yy))));
            const double b = modelBand(mdl, D.fit.p, D.fit.cov, xx);
            if (D.showBand && std::isfinite(b)) {
                const double a1 = uy(yy + b), a0 = uy(yy - b);
                up.push_back(ImVec2(mx(u), my(a1)));
                lo.push_back(ImVec2(mx(u), std::isfinite(a0) ? my(a0) : B + 1000));
            }
        }
        if (up.size() > 1 && up.size() == lo.size())
            for (std::size_t k = 0; k + 1 < up.size(); k++) {
                dl->AddQuadFilled(up[k], up[k + 1], lo[k + 1], lo[k], sh::cAccent(0.16f));
            }
        if (mid.size() > 1) dl->AddPolyline(mid.data(), static_cast<int>(mid.size()), sh::cText(0.85f), 0, 1.6f * static_cast<float>(F.dpi));
    }
    // the points and their error bars (on a log axis a bar that reaches 0 or below runs off the bottom)
    const float rad = 3.0f * static_cast<float>(F.dpi), cap = 3.0f * static_cast<float>(F.dpi);
    int hover = -1;
    float best = 9.0f * static_cast<float>(F.dpi);
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    const bool inPlot = sh::hoveredRect(ImVec2(L, T), ImVec2(R - L, B - T));
    for (std::size_t i = 0; i < n; i++) {
        const double pxv = ux(D.X[i]), pyv = uy(D.Y[i]);
        if (!std::isfinite(pxv) || !std::isfinite(pyv)) continue;
        const float xx = mx(pxv), yy = my(pyv);
        if (std::isfinite(D.SY[i]) && D.SY[i] > 0) {
            const double lo = uy(D.Y[i] - D.SY[i]);
            const float a = std::isfinite(lo) ? my(lo) : B + 1000, b = my(uy(D.Y[i] + D.SY[i]));
            if (a - b > 1.5f) { dl->AddLine(ImVec2(xx, a), ImVec2(xx, b), sh::cAccent(0.7f)); dl->AddLine(ImVec2(xx - cap, a), ImVec2(xx + cap, a), sh::cAccent(0.7f)); dl->AddLine(ImVec2(xx - cap, b), ImVec2(xx + cap, b), sh::cAccent(0.7f)); }
        }
        if (std::isfinite(D.SX[i]) && D.SX[i] > 0) {
            const double lo = ux(D.X[i] - D.SX[i]);
            const float a = std::isfinite(lo) ? mx(lo) : L - 1000, b = mx(ux(D.X[i] + D.SX[i]));
            if (b - a > 1.5f) dl->AddLine(ImVec2(a, yy), ImVec2(b, yy), sh::cAccent(0.7f));
        }
        dl->AddCircleFilled(ImVec2(xx, yy), rad, sh::cAccent(), 12);
        const float d = std::hypot(mouse.x - xx, mouse.y - yy);
        if (inPlot && d < best) { best = d; hover = static_cast<int>(i); }
    }
    dl->PopClipRect();
    if (hover >= 0) {
        const std::size_t r = D.rowOf[std::size_t(hover)];
        dl->AddCircle(ImVec2(mx(ux(D.X[std::size_t(hover)])), my(uy(D.Y[std::size_t(hover)]))), rad * 2, sh::cText(), 16, 1.5f);
        std::string tip;
        for (int c = 0; c < static_cast<int>(D.table.cols.size()); c++) {
            const Column& col = D.table.cols[std::size_t(c)];
            std::string v = cellText(D.table, c, r);
            if (v.empty()) continue;
            if (!col.isText() && r < col.sigma.size() && std::isfinite(col.sigma[r])) v = plusMinus(col.v[r], col.sigma[r]);
            tip += (tip.empty() ? "" : "\n") + col.name + " = " + v + (col.unit.empty() || col.isText() ? "" : " " + col.unit);
        }
        if (fitted) tip += "\nresidual = " + fmtg(D.fit.resid.size() > std::size_t(hover) ? D.fit.resid[std::size_t(hover)] : NAN);
        ui::SetTooltip("%s", tip.c_str());
    }
    // the x label, centred under the ticks
    const std::string xl = colLabel(D.xCol, D.tx) + (xe ? "   × 10" + sup(xe) : "") + (D.logX ? "   (log axis)" : "");
    sh::text(dl, Type::Caption, true, ImVec2((L + R) / 2 - sh::width(Type::Caption, true, xl) / 2, B + lh + static_cast<float>(m.s8)), sh::cMuted(), xl);
    float used = (B + 2 * lh + static_cast<float>(m.s12)) - o.y;
    noteRect("data.plot", L, T, R - L, B - T);
    if (shown < n) {
        sh::text(dl, Type::Caption, false, ImVec2(o.x, o.y + used), sh::cMuted(), std::to_string(n - shown) + " points at 0 or below are not on the log axis (they are still in the fit).");
        used += lh + static_cast<float>(m.s4);
    }
    // the residuals (always on a linear scale, symmetric about 0; the label at the top is the scale's half-height)
    if (showRes) {
        const double rb = ra * 1.15;
        const float t2 = o.y + used;
        const std::string lab = norm ? "residual / σ" : "residual" + (D.table.cols[std::size_t(D.yCol)].unit.empty() || D.ty == Transform::Ln || D.ty == Transform::Log10 ? "" : " (" + transformedUnit(D.table.cols[std::size_t(D.yCol)].unit, D.ty) + ")");
        sh::text(dl, Type::Caption, true, ImVec2(o.x, t2), sh::cMuted(), lab);
        const float T2 = t2 + lh + static_cast<float>(m.s4), B2 = T2 + hr;
        const auto ry = [&](double v) { return (T2 + B2) / 2 - static_cast<float>(v / rb) * (B2 - T2) / 2; };
        dl->AddRect(ImVec2(L, T2), ImVec2(R, B2), sh::cEdge());
        dl->AddLine(ImVec2(L, ry(0)), ImVec2(R, ry(0)), sh::cText(0.5f));
        for (const std::string& t : {rLab, std::string("0"), "−" + rLab}) {
            const double v = t == "0" ? 0 : t == rLab ? ra : -ra;
            sh::text(dl, Type::Caption, false, ImVec2(L - static_cast<float>(m.s4) - sh::width(Type::Caption, false, t), ry(v) - lh / 2), sh::cMuted(), t);
        }
        for (std::size_t i = 0; i < n; i++) if (std::isfinite(rs[i]) && std::isfinite(ux(D.X[i]))) {
            const float xx = mx(ux(D.X[i]));
            dl->AddLine(ImVec2(xx, ry(0)), ImVec2(xx, ry(rs[i])), sh::cAccent(0.5f));
            dl->AddCircleFilled(ImVec2(xx, ry(rs[i])), rad * 0.8f, sh::cAccent(), 10);
        }
        used = B2 + static_cast<float>(m.s8) - o.y;
    }
    (void)app;
    p.y += used;
}

// The fit's numbers, written out
std::vector<std::string> resultLines(bool forExport) {
    std::vector<std::string> out;
    if (D.model < 0 || !D.fit.ok) return out;
    const ModelInfo& mi = modelInfo(static_cast<Model>(D.model));
    const Column& cx = D.table.cols[std::size_t(D.xCol)];
    const Column& cy = D.table.cols[std::size_t(D.yCol)];
    const std::string xu = transformedUnit(cx.unit, D.tx), yu = transformedUnit(cy.unit, D.ty);
    out.push_back(std::string("Fit: ") + mi.name + ", " + mi.formula + "; x = " + colLabel(D.xCol, D.tx) + ", y = " + colLabel(D.yCol, D.ty) + (D.group.empty() ? "" : "; rows: " + D.group));
    for (std::size_t k = 0; k < mi.params.size() && k < D.fit.p.size(); k++) {
        const ParamInfo& pi = mi.params[k];
        const std::string u = paramUnit(yu, xu, pi.yPow, pi.xPow);
        out.push_back(std::string(pi.name) + " = " + (forExport ? numberText(D.fit.p[k], 12) + " ± " + numberText(D.fit.err[k], 3) : plusMinus(D.fit.p[k], D.fit.err[k])) + (u.empty() ? "" : " " + u) +
                      (std::strlen(pi.meaning) ? "   (" + std::string(pi.meaning) + ")" : ""));
    }
    {   // 0.51.1: the correlations of the free parameters (a held one has none)
        const std::vector<double> c = correlation(D.fit);
        const std::size_t np = D.fit.p.size();
        std::string cl;
        for (std::size_t i = 0; i < np && i < mi.params.size(); i++)
            for (std::size_t j = i + 1; j < np && j < mi.params.size(); j++)
                if (std::isfinite(c[i * np + j])) { char b[96]; std::snprintf(b, sizeof b, "ρ(%s, %s) = %+.4f", mi.params[i].name, mi.params[j].name, c[i * np + j]); cl += (cl.empty() ? "" : ",  ") + std::string(b); }
        if (!cl.empty()) out.push_back("Correlations: " + cl);
    }
    if (D.fit.weighted) {
        char b[256];
        std::snprintf(b, sizeof b, "χ² = %.4g for %d degrees of freedom, χ²/dof = %.3g; Q = %.3g (the chance of a χ² this big or bigger if the model and the uncertainties are right)",
                      D.fit.chi2, D.fit.dof, D.fit.redChi2, D.fit.q);
        out.push_back(b);
    } else {
        out.push_back("No uncertainties used: every point counts the same, and the errors come from the scatter, s = " + fmtg(D.fit.s, 4) + (yu.empty() ? "" : " " + yu) + " (" + std::to_string(D.fit.n) + " points, " +
                      std::to_string(D.fit.dof) + " degrees of freedom).");
    }
    return out;
}

void sendToCalculator() {
    if (D.model < 0 || !D.fit.ok) return;
    const ModelInfo& mi = modelInfo(static_cast<Model>(D.model));
    const std::string xu = transformedUnit(D.table.cols[std::size_t(D.xCol)].unit, D.tx), yu = transformedUnit(D.table.cols[std::size_t(D.yCol)].unit, D.ty);
    std::string names, why;
    for (std::size_t k = 0; k < mi.params.size(); k++) {
        const std::string u = paramUnit(yu, xu, mi.params[k].yPow, mi.params[k].xPow);
        const std::string name = std::string("fit_") + mi.params[k].ascii;
        const std::string val = numberText(D.fit.p[k], 15) + (u.empty() ? "" : " (" + u + ")");
        std::string w;
        if (calcDefine(name, val, w)) names += (names.empty() ? "" : ", ") + name; else why = w;
        if (std::isfinite(D.fit.err[k]) && calcDefine(name + "_err", numberText(D.fit.err[k], 15) + (u.empty() ? "" : " (" + u + ")"), w)) names += ", " + name + "_err";
    }
    D.sendNote = names.empty() ? "The calculator refused them: " + why : "In the calculator now: " + names + ".";
    calcInsert(names.empty() ? "" : std::string("fit_") + mi.params[0].ascii);
}

void saveCsv(AppState& app) {
    const std::string csv = toCsv(D.table, appVersion(), resultLines(true));
    std::vector<std::string> folders;
    if (const char* d = SDL_getenv("PL_SHOT_DIR")) folders.push_back(d);
    else {
        if (const char* f = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS)) folders.push_back(f);
        if (const char* f = SDL_GetUserFolder(SDL_FOLDER_DOWNLOADS)) folders.push_back(f);
    }
    SDL_Time now = 0;
    SDL_DateTime dt{};
    SDL_GetCurrentTime(&now);
    SDL_TimeToDateTime(now, &dt, true);
    char name[128];
    std::snprintf(name, sizeof name, "PhysicsLab-%s-%04d%02d%02d-%02d%02d%02d.csv", D.table.id.c_str(), dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
    std::string why = folders.empty() ? "no Documents or Downloads folder found" : "";
    for (const std::string& f : folders) {
        const std::string path = app::joinPath(f, name);
        if (SDL_SaveFile(path.c_str(), csv.data(), csv.size())) {
            app.shot.note = "Saved to " + path; app.shot.failed = false; app.shot.noteUntil = ImGui::GetTime() + 20;
            std::printf("Saved data: %s\n", path.c_str());
            return;
        }
        why = SDL_GetError();
    }
    app.shot.note = "The table could not be saved (" + why + ")."; app.shot.failed = true; app.shot.noteUntil = ImGui::GetTime() + 30;
}

// The table itself, scrolling; a tap on a number puts it (with its unit) into the calculator
void drawTable(sh::Panel& p) {
    const sh::Frame& F = sh::F();
    const auto& m = F.m;
    const std::size_t rows = D.table.rows();
    const int ncol = static_cast<int>(D.table.cols.size());
    if (ncol == 0 || ncol > 60) return;
    const float rowH = sh::lineH(Type::Caption) + 2.0f * static_cast<float>(F.dpi) * 2;
    const float h = std::min(static_cast<float>(rows + 1), 13.0f) * rowH + static_cast<float>(m.s8);
    ImGui::SetCursorScreenPos(sh::here(p));
    ImGui::PushFont(nullptr, app::shell::fontBase(Type::Caption, F.dpi, F.tf));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(sh::cText()));
    ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImGui::ColorConvertU32ToFloat4(sh::cFace()));
    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImGui::ColorConvertU32ToFloat4(sh::cFace(0.35f)));
    ImGui::PushStyleColor(ImGuiCol_TableBorderLight, ImGui::ColorConvertU32ToFloat4(sh::cHair()));
    ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, ImGui::ColorConvertU32ToFloat4(sh::cEdge()));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::ColorConvertU32ToFloat4(sh::cFaceHover()));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImGui::ColorConvertU32ToFloat4(sh::cFacePress()));
    const ImGuiTableFlags fl = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("##datatable", ncol, fl, ImVec2(p.w, h))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        for (const Column& c : D.table.cols) ImGui::TableSetupColumn((c.name + (c.unit.empty() || c.isText() ? "" : " (" + c.unit + ")")).c_str());
        ImGui::TableHeadersRow();
        ImGuiListClipper clip;
        clip.Begin(static_cast<int>(rows));
        while (clip.Step())
            for (int r = clip.DisplayStart; r < clip.DisplayEnd; r++) {
                ImGui::TableNextRow();
                const bool inGroup = D.group.empty() || D.table.group < 0 || cellText(D.table, D.table.group, std::size_t(r)) == D.group;
                for (int c = 0; c < ncol; c++) {
                    ImGui::TableSetColumnIndex(c);
                    const Column& col = D.table.cols[std::size_t(c)];
                    std::string s = cellText(D.table, c, std::size_t(r));
                    if (!col.isText() && std::size_t(r) < col.sigma.size() && std::isfinite(col.sigma[std::size_t(r)]) && std::isfinite(col.v[std::size_t(r)])) s = plusMinus(col.v[std::size_t(r)], col.sigma[std::size_t(r)]);
                    if (!inGroup) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(sh::cMuted(0.7f)));
                    ImGui::PushID(r * 64 + c);
                    if (col.isText() || s.empty()) ImGui::TextUnformatted(s.c_str());
                    else if (ImGui::Selectable(s.c_str(), false)) calcInsert("(" + numberText(col.v[std::size_t(r)], 15) + (col.unit.empty() ? "" : " " + col.unit) + ")");
                    else if (ImGui::IsItemHovered()) ui::SetTooltip("Tap to put %s%s%s into the calculator", s.c_str(), col.unit.empty() ? "" : " ", col.unit.c_str());
                    ImGui::PopID();
                    if (!inGroup) ImGui::PopStyleColor();
                }
            }
        ImGui::EndTable();
    }
    ImGui::PopStyleColor(7);
    ImGui::PopFont();
    noteItem("data.table");
    p.y += h + static_cast<float>(m.s8);
}

}  // namespace

bool dataOpen() { return D.open; }
void dataToggle() { D.open = !D.open; }
void dataClose() { D.open = false; D.fieldActive = false; }
bool dataFieldActive() { return D.open && D.fieldActive; }

void dataTick(AppState& app) {
    if (!D.rec) return;
    const orbit::Body* b = orbit::findBody(app.sys, D.recBody);
    if (!b) { D.rec = false; D.recStop = "Stopped: the body is gone."; return; }
    const double t = app.sys.t;
    if (!D.rt.empty() && t < D.recLast) { D.rec = false; D.recStop = "Stopped: time went back (a date was set or time runs backwards)."; return; }
    if (!D.rt.empty() && t - D.recLast < D.recStep) return;
    if (D.rt.empty()) D.recT0 = t;
    if (app.sys.realPlanets() != D.recReal || app.sys.relativity() != D.recGr) { D.rec = false; D.recStop = "Stopped: the planet physics changed (record again to measure the new one)."; return; }
    D.rt.push_back(t); D.rs.push_back(recSample((t - D.recT0) / units::kDay, b->pos, b->vel));
    D.recLast = t;
    if (static_cast<int>(D.rt.size()) >= kRecMax) {              // 0.51.1: full: keep every other sample and sample half as often, so a long run (the perihelion over centuries) goes on
        std::size_t k = 0;
        for (std::size_t i = 0; i < D.rt.size(); i += 2, k++) { D.rt[k] = D.rt[i]; D.rs[k] = D.rs[i]; }
        D.rt.resize(k); D.rs.resize(k);
        D.recStep *= 2;
        D.recThinned++;
    }
}

Rect dataPanelDraw(AppState& app, const app::shell::Frame& fr, const app::shell::Metrics& m, const std::vector<Rect>& cards) {
    if (!D.open || app.benchRunning) { D.fieldActive = false; return Rect{0, 0, 0, 0}; }
    const double w = std::min<double>(m.em(40), std::max<double>(m.em(24), fr.window.w * 0.55));
    Rect r = app::shell::placePanel(app::shell::Anchor::TopLeft, w, sh::lastHeight("##data", static_cast<float>(m.em(40))), fr, m, 0);
    for (const Rect& c : cards) r = app::shell::sideStep(r, c, fr.window, m.s12, m.s12);
    sh::Panel p;
    if (!sh::beginPanel("##data", r, p, sh::Shape::Panel, true, true)) return r;
    const sh::Frame& F = sh::F();
    D.fieldActive = false;
    refreshTable(app);
    if (D.dirty) rebuild();

    // ---- the title and the close button ----
    {
        const ImVec2 o = sh::here(p);
        const float ib = static_cast<float>(m.iconBtn);
        sh::text(p.dl, Type::Title, true, ImVec2(o.x, o.y + (ib - sh::lineH(Type::Title)) / 2), sh::cText(), "Data");
        if (sh::iconButton("data.close", ImVec2(o.x + p.w - ib, o.y), ImVec2(ib, ib), sh::Icon::Close, "Close (Esc)")) { dataClose(); sh::endPanel(p); return r; }
        noteItem("data.close");
        p.y += ib + static_cast<float>(m.s8);
    }
    // ---- which table ----
    const int lab = std::clamp(app.lab, 0, 2);
    std::string& id = D.tableId[lab];
    const std::string shownTitle = id == "user" ? "Your data (type or paste)" : D.have ? D.table.title : id;
    if (fitRow(p, "data.tables", std::string(D.pickTable ? "▾ " : "▸ ") + "Table: " + shownTitle)) D.pickTable = !D.pickTable;
    if (D.pickTable) {
        static LabContext refsFor;                         // the list is made once per selection (making every table to name it is not free)
        static std::vector<TableRef> cached;
        static bool haveRefs = false;
        if (!haveRefs || !sameContext(refsFor, contextOf(app))) { cached = labTables(contextOf(app)); refsFor = contextOf(app); haveRefs = true; }
        std::vector<TableRef> refs = cached;
        if (app.lab == 0) refs.push_back({"recording", "Recording of a body as time runs"});
        refs.push_back({"user", "Your data (type or paste)"});
        for (const TableRef& t : refs)
            if (fitRow(p, "data.table." + t.id, "    " + t.title, t.id == id)) { id = t.id; D.builtId.clear(); D.pickTable = false; refreshTable(app); rebuild(); }
    }
    // ---- the user's data ----
    if (id == "user") {
        ImGui::SetCursorScreenPos(sh::here(p));
        ImGui::PushFont(nullptr, app::shell::fontBase(Type::Caption, F.dpi, F.tf));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(sh::cFace()));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(sh::cText()));
        const float bh = static_cast<float>(m.em(9));
        const bool changed = ImGui::InputTextMultiline("##datauser", D.user, sizeof D.user, ImVec2(p.w, bh), ImGuiInputTextFlags_AllowTabInput);
        D.fieldActive = ImGui::IsItemActive();
        noteItem("data.user");
        ImGui::PopStyleColor(2);
        ImGui::PopFont();
        p.y += bh + static_cast<float>(m.s8);
        const float bw = (p.w - static_cast<float>(m.s8)) / 2;
        const ImVec2 o = sh::here(p);
        bool reread = changed;
        if (sh::button("data.paste", o, ImVec2(bw, static_cast<float>(m.control)), "Paste")) {
            if (char* c = SDL_GetClipboardText()) { std::snprintf(D.user, sizeof D.user, "%s", c); SDL_free(c); reread = true; }
        }
        noteItem("data.paste");
        if (sh::button("data.clearuser", ImVec2(o.x + bw + static_cast<float>(m.s8), o.y), ImVec2(bw, static_cast<float>(m.control)), "Clear")) { D.user[0] = 0; reread = true; }
        noteItem("data.clearuser");
        p.y += static_cast<float>(m.control + m.s8);
        if (reread) {
            D.userNotes.clear();
            D.userOk = parseText(D.user, D.userTable, D.userWhy, D.userNotes);
            D.userParsed = true;
            if (D.userOk) takeTable(D.userTable); else D.have = false;
            D.why = D.userOk ? "" : D.userWhy;
            rebuild();
        }
        for (const std::string& n : D.userNotes) sh::pCaption(p, n);
        if (!D.userOk) sh::pText(p, Type::Body, false, sh::cDanger(), D.userWhy);
    }
    // ---- the recording ----
    if (id == "recording") {
        const orbit::Body* sel = app.view.sel.kind == orbit::Selection::Body ? orbit::findBody(app.sys, app.view.sel.bodyId) : nullptr;
        const float bw = (p.w - static_cast<float>(m.s8)) / 2;
        const ImVec2 o = sh::here(p);
        if (D.rec) {
            if (sh::button("data.rec", o, ImVec2(bw, static_cast<float>(m.control)), "Stop", sh::Face::Primary)) D.rec = false;
        } else if (sel) {
            if (sh::button("data.rec", o, ImVec2(bw, static_cast<float>(m.control)), "Record " + sel->name, sh::Face::Primary)) {
                D.rt.clear(); D.rs.clear(); D.recStop.clear(); D.recThinned = 0;
                D.rec = true; D.recBody = sel->id; D.recName = sel->name; D.recReal = app.sys.realPlanets(); D.recGr = app.sys.relativity();
                const orbit::OrbitShape os = orbit::orbitOf(*sel);
                D.recStep = std::max(60.0, (os.bound ? os.periodYears * units::kYear : units::kYear) / 500);
                D.recLast = app.sys.t;
                if (app.paused) app.paused = false;
            }
        } else sh::disabledButton("data.rec", o, ImVec2(bw, static_cast<float>(m.control)), "Pick a body first");
        noteItem("data.rec");
        if (sh::button("data.recclear", ImVec2(o.x + bw + static_cast<float>(m.s8), o.y), ImVec2(bw, static_cast<float>(m.control)), "Clear")) { D.rt.clear(); D.rs.clear(); D.rec = false; D.recStop.clear(); D.recThinned = 0; }
        noteItem("data.recclear");
        p.y += static_cast<float>(m.control + m.s8);
        sh::pCaption(p, std::to_string(D.rt.size()) + " samples" + (D.rec ? " (recording " + D.recName + ": one sample every " + fmtg(D.recStep / units::kDay, 3) + " days of simulated time)" : "") +
                            (D.recThinned > 0 ? "; the recording filled up " + std::to_string(D.recThinned) + (D.recThinned == 1 ? " time" : " times") + " and kept every other sample to go on" : "") +
                            (D.recStop.empty() ? "" : ". " + D.recStop));
    }
    if (!D.have) {
        if (!D.why.empty() && id != "user") sh::pCaption(p, D.why);
        sh::endPanel(p);
        return r;
    }
    sh::pCaption(p, D.table.about);
    if (fitRow(p, "data.sources", std::string(D.showSources ? "▾ " : "▸ ") + "Sources")) D.showSources = !D.showSources;
    if (D.showSources) {
        for (const std::string& s : D.table.sources) sh::pCaption(p, s, false);
        for (const Column& c : D.table.cols) if (!c.source.empty()) sh::pCaption(p, c.name + ": " + c.source);
    }
    // ---- worked analyses ----
    if (!D.table.presets.empty()) {
        if (fitRow(p, "data.presets", std::string(D.showPresets ? "▾ " : "▸ ") + "Worked analyses")) D.showPresets = !D.showPresets;
        if (D.showPresets)
            for (std::size_t i = 0; i < D.table.presets.size(); i++)
                if (fitRow(p, "data.preset" + std::to_string(i), "    " + D.table.presets[i].label)) { applyPreset(D.table.presets[i]); rebuild(); }
    }
    // ---- the axes and the rows ----
    axisChooser(p, "x", D.xCol, D.tx, D.pickX);
    axisChooser(p, "y", D.yCol, D.ty, D.pickY);
    if (D.table.group >= 0) {
        const std::vector<std::string> gv = groupValues(D.table);
        std::vector<std::string> labels = {"All"};
        for (const std::string& g : gv) labels.push_back(g);
        int cur = 0;
        for (std::size_t i = 0; i < gv.size(); i++) if (gv[i] == D.group) cur = static_cast<int>(i) + 1;
        sh::pCaption(p, "Rows: " + D.table.cols[std::size_t(D.table.group)].name);
        if (labels.size() <= 8) { if (sh::pSegmented(p, "data.group", labels, &cur)) { D.group = cur == 0 ? "" : gv[std::size_t(cur - 1)]; rebuild(); } }
        else {
            static bool pickGroup = false;
            if (fitRow(p, "data.group", std::string(pickGroup ? "▾ " : "▸ ") + (D.group.empty() ? "All rows" : D.group))) pickGroup = !pickGroup;
            if (pickGroup) for (std::size_t i = 0; i < labels.size(); i++)
                if (fitRow(p, "data.group" + std::to_string(i), "    " + labels[i], static_cast<int>(i) == cur)) { D.group = i == 0 ? "" : gv[i - 1]; pickGroup = false; rebuild(); }
        }
    }
    {   // 0.51.1: logarithmic axes (the drawing only)
        const float half = (p.w - static_cast<float>(m.s8)) / 2;
        const float y0 = p.y;
        sh::Panel q = p; q.w = half;
        sh::pCheck(q, "data.logx", "Log x axis", &D.logX);
        noteItem("data.logx");
        sh::Panel q2 = p; q2.origin.x += half + static_cast<float>(m.s8); q2.w = half;
        sh::pCheck(q2, "data.logy", "Log y axis", &D.logY);
        noteItem("data.logy");
        p.y = std::max(q.y, q2.y);
        (void)y0;
    }
    if (D.dirty) rebuild();
    sh::pGap(p, m.s4);
    drawPlot(p, app);
    if (D.skipped > 0) sh::pCaption(p, std::to_string(D.skipped) + " rows left out (no value, or the transform is not defined there).");
    // ---- the fit ----
    {
        const std::string label = D.model < 0 ? "No fit" : std::string(modelInfo(static_cast<Model>(D.model)).name) + ": " + modelInfo(static_cast<Model>(D.model)).formula;
        if (fitRow(p, "data.model", std::string(D.pickModel ? "▾ " : "▸ ") + "Fit: " + label)) D.pickModel = !D.pickModel;
        if (D.pickModel) {
            if (fitRow(p, "data.model.none", "    No fit", D.model < 0)) { setModel(-1); D.pickModel = false; rebuild(); }
            for (int k = 0; k < static_cast<int>(Model::Count); k++) {
                const ModelInfo& mi = modelInfo(static_cast<Model>(k));
                if (fitRow(p, std::string("data.model.") + mi.id, std::string("    ") + mi.name + ":  " + mi.formula, k == D.model)) { if (k != D.model) setModel(k); D.pickModel = false; rebuild(); }
            }
        }
        bool hasSig = false;
        if (numericCol(D.xCol)) hasSig = hasSig || D.table.cols[std::size_t(D.xCol)].hasSigma();
        if (numericCol(D.yCol)) hasSig = hasSig || D.table.cols[std::size_t(D.yCol)].hasSigma();
        if (hasSig && D.model >= 0) { if (sh::pCheck(p, "data.sigma", "Use the uncertainties (weighted fit)", &D.useSigma)) rebuild(); noteItem("data.sigma"); }
        if (D.model >= 0 && D.fit.ok) { if (sh::pCheck(p, "data.band", "Show the 1σ band of the curve", &D.showBand)) {} noteItem("data.band"); }
        if (D.model >= 0 && D.fit.ok && D.fit.weighted && (D.fit.redChi2 > 0 || D.scaleErr)) {
            if (sh::pCheck(p, "data.scale", "Scale the errors by √(χ²/dof) (the σ only right relative to each other)", &D.scaleErr)) rebuild();
            noteItem("data.scale");
        }
        // 0.51.1: hold a parameter at a value (Kepler's n at 3/2): the others are fitted with it fixed
        if (D.model >= 0) {
            const ModelInfo& mi = modelInfo(static_cast<Model>(D.model));
            if (D.held.size() != mi.params.size()) D.held.assign(mi.params.size(), NAN);
            for (std::size_t k = 0; k < mi.params.size(); k++) {
                bool on = std::isfinite(D.held[k]);
                const std::string id = std::string("data.hold.") + mi.params[k].ascii;
                const float fw = static_cast<float>(m.em(9));
                const ImVec2 o = sh::here(p);
                sh::Panel q = p;                                   // the check box stops short of the value field, so a tap on the field does not untick it
                q.w = p.w - fw - static_cast<float>(m.s8);
                if (sh::pCheck(q, id.c_str(), std::string("Hold ") + mi.params[k].name + " at", &on)) {
                    D.held[k] = on ? (D.fit.ok && k < D.fit.p.size() ? D.fit.p[k] : 1.0) : NAN;
                    rebuild();
                }
                noteItem(id.c_str());
                p.y = q.y;
                if (on) {
                    ImGui::SetCursorScreenPos(ImVec2(o.x + p.w - fw, o.y));
                    ImGui::PushFont(nullptr, app::shell::fontBase(Type::Body, F.dpi, F.tf));
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(sh::cFace()));
                    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(sh::cText()));
                    ImGui::SetNextItemWidth(fw);
                    double v = D.held[k];
                    if (ImGui::InputDouble(("##" + id + "v").c_str(), &v, 0, 0, "%.10g", ImGuiInputTextFlags_EnterReturnsTrue) && std::isfinite(v)) { D.held[k] = v; rebuild(); }
                    D.fieldActive = D.fieldActive || ImGui::IsItemActive();
                    noteItem((id + "v").c_str());
                    ImGui::PopStyleColor(2);
                    ImGui::PopFont();
                }
            }
        }
        if (!D.weightNote.empty() && D.model >= 0) sh::pCaption(p, D.weightNote);
        if (D.model >= 0) {
            if (!D.fit.ok) sh::pText(p, Type::Body, false, sh::cDanger(), D.fit.error.empty() ? "No fit." : D.fit.error);
            else {
                const std::vector<std::string> lines = resultLines(false);
                for (std::size_t i = 1; i < lines.size(); i++) sh::pText(p, i + 1 < lines.size() ? Type::Body : Type::Caption, i + 1 < lines.size(), sh::cText(), lines[i]);
                const ModelInfo& mi = modelInfo(static_cast<Model>(D.model));
                if (std::strlen(mi.note)) sh::pCaption(p, mi.note);
                for (const std::string& n : D.fit.notes) sh::pCaption(p, n);
                if (D.tx == Transform::Ln || D.tx == Transform::Log10 || D.ty == Transform::Ln || D.ty == Transform::Log10)
                    sh::pCaption(p, "The fit is made on the plotted (transformed) values, with the uncertainties carried through the transform to first order.");
                const float bw = (p.w - static_cast<float>(m.s8)) / 2;
                const ImVec2 o = sh::here(p);
                if (sh::button("data.send", o, ImVec2(bw, static_cast<float>(m.control)), "Send to calculator")) sendToCalculator();
                noteItem("data.send");
                if (sh::button("data.copyfit", ImVec2(o.x + bw + static_cast<float>(m.s8), o.y), ImVec2(bw, static_cast<float>(m.control)), "Copy results")) {
                    std::string s;
                    for (const std::string& l : resultLines(true)) s += l + "\n";
                    SDL_SetClipboardText(s.c_str());
                }
                noteItem("data.copyfit");
                p.y += static_cast<float>(m.control + m.s8);
                if (!D.sendNote.empty()) sh::pCaption(p, D.sendNote);
            }
        }
        if (!D.expect.empty()) sh::pCaption(p, "Expected: " + D.expect, false);
    }
    // ---- the table and the export ----
    sh::pGap(p, m.s4);
    if (fitRow(p, "data.showtable", std::string(D.showTable ? "▾ " : "▸ ") + "The table (" + std::to_string(D.table.rows()) + " rows; tap a number to use it in the calculator)")) D.showTable = !D.showTable;
    if (D.showTable) drawTable(p);
    {
        const float bw = (p.w - static_cast<float>(m.s8)) / 2;
        const ImVec2 o = sh::here(p);
        if (sh::button("data.csv", o, ImVec2(bw, static_cast<float>(m.control)), "Save as CSV")) saveCsv(app);
        noteItem("data.csv");
        if (sh::button("data.copy", ImVec2(o.x + bw + static_cast<float>(m.s8), o.y), ImVec2(bw, static_cast<float>(m.control)), "Copy table")) SDL_SetClipboardText(toTsv(D.table).c_str());
        noteItem("data.copy");
        p.y += static_cast<float>(m.control + m.s8);
        sh::pCaption(p, "Save as CSV writes the table with its units, sources and the fit to the Documents folder; Copy table puts it on the clipboard, tab-separated, for a spreadsheet.");
    }
    if (shellLogOn()) {
        std::string line = "PLDATA table=" + D.table.id + " rows=" + std::to_string(D.table.rows()) + " points=" + std::to_string(D.X.size()) + " x=" + colLabel(D.xCol, D.tx) + " y=" + colLabel(D.yCol, D.ty) +
                           " model=" + (D.model < 0 ? "none" : modelInfo(static_cast<Model>(D.model)).id) + " ok=" + (D.fit.ok ? "1" : "0") + " weighted=" + (D.fit.weighted ? "1" : "0");
        for (std::size_t k = 0; k < D.fit.p.size() && D.fit.ok; k++) line += " p" + std::to_string(k) + "=" + numberText(D.fit.p[k], 10) + " e" + std::to_string(k) + "=" + numberText(D.fit.err[k], 4);
        for (std::size_t k = 0; k < D.held.size(); k++) if (std::isfinite(D.held[k])) line += " held" + std::to_string(k) + "=" + numberText(D.held[k], 10);
        line += std::string(" logx=") + (D.logX ? "1" : "0") + " logy=" + (D.logY ? "1" : "0") + " scaled=" + (D.scaleErr ? "1" : "0");
        line += " samples=" + std::to_string(D.rt.size()) + " send=" + D.sendNote;
        if (line != D.logged) { D.logged = line; std::printf("%s\n", line.c_str()); std::fflush(stdout); }
    }
    sh::endPanel(p);
    return r;
}

}  // namespace ui
