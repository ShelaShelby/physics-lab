#include "atom_panel.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "changelog.h"
#include "shot.h"
#include "fonts.h"
#include "imgui.h"
#include "app_pages.h"
#include "look.h"
#include "orbit/format.h"
#include "atom/atom_learn.h"
#include "atom_learn.h"
#include "orbit_panel.h"
#include "rich_text.h"
#include "settings.h"

namespace ui {

using namespace atom;

namespace {

// ---- small helpers ----
float S() { return ImGui::GetFontSize() / 16.0f; }    // points -> window pixels (the panel's text is 16 points)
float B(float pt) { return pt * ImGui::GetStyle().FontSizeBase / 16.0f; }   // a text size for PushFont (which scales it itself)
ImU32 U32(const render::Color& c, float a = -1) { return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, a < 0 ? c.a : a)); }
// CSS hsl(h s% l% / a)
ImU32 hsl(double h, double s, double l, double a) {
    s /= 100; l /= 100;
    const double c = (1 - std::fabs(2 * l - 1)) * s, hp = std::fmod(h, 360) / 60, x = c * (1 - std::fabs(std::fmod(hp, 2) - 1));
    double r = 0, g = 0, b = 0;
    if (hp < 1) { r = c; g = x; } else if (hp < 2) { r = x; g = c; } else if (hp < 3) { g = c; b = x; }
    else if (hp < 4) { g = x; b = c; } else if (hp < 5) { r = x; b = c; } else { r = c; b = x; }
    const double m = l - c / 2;
    return ImGui::ColorConvertFloat4ToU32(ImVec4(float(r + m), float(g + m), float(b + m), float(a)));
}
Theme panelTheme() { return theme(look().c); }    // the panel follows the theme picked with 🎨

void dashedRect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float th, float dash) {
    auto seg = [&](ImVec2 p, ImVec2 q) {
        const float len = std::hypot(q.x - p.x, q.y - p.y);
        for (float s = 0; s < len; s += 2 * dash) {
            const float e = std::min(len, s + dash);
            dl->AddLine(ImVec2(p.x + (q.x - p.x) * s / len, p.y + (q.y - p.y) * s / len), ImVec2(p.x + (q.x - p.x) * e / len, p.y + (q.y - p.y) * e / len), col, th);
        }
    };
    seg(a, ImVec2(b.x, a.y)); seg(ImVec2(b.x, a.y), b); seg(b, ImVec2(a.x, b.y)); seg(ImVec2(a.x, b.y), a);
}

void mutedWrapped(const std::string& t, float size = 0.75f) {
    ImGui::PushFont(nullptr, B(16 * size));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted(displayable(t).c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

// "Auto-fit (orbit fills the canvas)" -> {"Auto-fit", "orbit fills the canvas"}; "1s · sphere" -> {"1s", "sphere"}
std::pair<std::string, std::string> splitLabel(const std::string& s) {
    const std::size_t open = s.find(" (");
    if (open != std::string::npos && !s.empty() && s.back() == ')') return {s.substr(0, open), s.substr(open + 2, s.size() - open - 3)};
    const std::size_t dot = s.find(" · ");
    if (dot != std::string::npos) return {s.substr(0, dot), s.substr(dot + strlen(" · "))};
    return {s, ""};
}

// A select setting as the web draws it: a row of buttons (the one in use highlighted) and a note
// below explaining the chosen one.
void selectRow(AppState& app, const char* key, const char* label, const std::vector<std::pair<std::string, std::string>>& options, const std::string& cur) {
    ImGui::PushID(key);
    mutedWrapped(label, 13.0f / 16);
    const float avail = ImGui::GetContentRegionAvail().x, gap = 6 * S();
    float x = 0;
    std::string note;
    for (std::size_t i = 0; i < options.size(); i++) {
        const auto [text, sub] = splitLabel(options[i].second);
        const bool on = options[i].first == cur;
        if (on) note = sub;
        const std::string t = displayable(text);
        float w = options.size() == 2 ? (avail - gap) / 2 : ImGui::CalcTextSize(t.c_str()).x + 2 * ImGui::GetStyle().FramePadding.x + 8 * S();
        if (i > 0) { if (x + gap + w <= avail) { ImGui::SameLine(0, gap); x += gap; } else x = 0; }
        if (on) pushOn();
        if (ImGui::Button(t.c_str(), ImVec2(w, 0)) && !on) app.atom.setParam(key, options[i].first);
        if (on) popOn();
        x += w;
    }
    if (!note.empty()) mutedWrapped(note, 13.0f / 16);
    ImGui::Dummy(ImVec2(0, 2 * S()));
    ImGui::PopID();
}

// the choices of the Orbital and Orientation rows (used for the first orbital and for the second one of a superposition)
using Choices = std::vector<std::pair<std::string, std::string>>;
static const Choices SHELLS = {{"1s", "1s · sphere"}, {"2s", "2s · sphere with a shell"}, {"2p", "2p · dumbbell"}, {"3s", "3s · sphere with two shells"}, {"3p", "3p · dumbbell with a shell"},
                                          {"3d", "3d · four lobes, or two and a ring"}, {"4s", "4s · sphere with three shells"}, {"4p", "4p · dumbbell with two shells"}, {"4d", "4d · d shape with a shell"}, {"4f", "4f · six or eight lobes"}};
static const Choices P_AX = {{"x", "px · along x"}, {"y", "py · along y"}, {"z", "pz · along z (up)"},
                                                          {"m+1", "m = +1 · complex state: a doughnut round z; the phase turns once round the axis"}, {"m-1", "m = −1 · complex state: the same doughnut, the phase turns the other way"}};
static const Choices D_AX = {{"z2", "dz² · two lobes and a ring"}, {"xz", "dxz · four lobes"}, {"yz", "dyz · four lobes"}, {"xy", "dxy · four lobes, flat"}, {"x2y2", "dx²−y² · four lobes on the axes"},
                                                          {"m+1", "m = +1 · complex state: the phase turns once round the z axis"}, {"m-1", "m = −1 · complex state: once round the z axis, the other way"},
                                                          {"m+2", "m = +2 · complex state: the phase turns twice round the z axis"}, {"m-2", "m = −2 · complex state: twice round the z axis, the other way"}};
static const Choices F_AX = {{"z3", "fz³ · two lobes and two rings"}, {"xz2", "fxz² · six lobes"}, {"yz2", "fyz² · six lobes"}, {"xyz", "fxyz · eight lobes"}, {"zx2y2", "fz(x²−y²) · eight lobes"},
                                                          {"xx23y2", "fx(x²−3y²) · six lobes, flat"}, {"y3x2y2", "fy(3x²−y²) · six lobes, flat"},
                                                          {"m+1", "m = +1 · complex state: the phase turns once round the z axis"}, {"m-1", "m = −1 · complex state: once round the z axis, the other way"},
                                                          {"m+2", "m = +2 · complex state: the phase turns twice round the z axis"}, {"m-2", "m = −2 · complex state: twice round the z axis, the other way"},
                                                          {"m+3", "m = +3 · complex state: the phase turns three times round the z axis"}, {"m-3", "m = −3 · complex state: three times round the z axis, the other way"}};

// A number setting: its label with the value in bold, then the slider (optionally with − and +).
void sliderRow(AppState& app, const char* key, const std::string& label, const std::string& value, double v, double lo, double hi, double step, bool plusMinus, const char* what = nullptr) {
    ImGui::PushID(key);
    ImGui::TextUnformatted(displayable(label).c_str());
    ImGui::SameLine();
    ImGui::PushFont(fonts().bold, 0.0f);
    ImGui::TextUnformatted(displayable(value).c_str());
    ImGui::PopFont();
    const float bw = ImGui::GetFrameHeight();
    if (plusMinus) {
        if (ImGui::Button("−", ImVec2(bw, 0))) app.atom.setParam(key, v - step);
        if (ImGui::IsItemHovered() && what) ImGui::SetTooltip("one fewer %s", what);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - bw - ImGui::GetStyle().ItemSpacing.x);
    } else ImGui::SetNextItemWidth(-1);
    if (step >= 1) {
        int iv = int(std::lround(v));
        if (ImGui::SliderInt("##s", &iv, int(lo), int(hi), "")) app.atom.setParam(key, iv);
    } else {
        float fv = float(v);
        if (ImGui::SliderFloat("##s", &fv, float(lo), float(hi), "")) app.atom.setParam(key, std::round(double(fv) / step) * step);
    }
    if (plusMinus) {
        ImGui::SameLine();
        if (ImGui::Button("+", ImVec2(bw, 0))) app.atom.setParam(key, v + step);
        if (ImGui::IsItemHovered() && what) ImGui::SetTooltip("one more %s", what);
    }
    ImGui::PopID();
}

void pickElement(AppState& app, int Z) { app.atom.setParam("Z", Z); app.atom.setParam("N", defaultN(Z)); }

std::string cellTip(int Z) { return std::to_string(Z) + " · " + name(Z) + " (" + categoryName(Z) + ")\n" + isoLine(Z, defaultN(Z)); }

// The periodic table: 18 columns, 7 rows, a gap, and the lanthanide and actinide rows.
// big: the large table (symbol and number); returns the element clicked, or 0.
int periodicTable(int sel, float cellH, bool big) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float gap = (big ? 2 : 2) * S(), avail = ImGui::GetContentRegionAvail().x;
    const float cw = (avail - 17 * gap) / 18, ch = cellH, rowGap = (big ? 10 : 8) * S();
    const ImVec2 o = ImGui::GetCursorScreenPos();
    const Theme th = panelTheme();
    // rows 1–7, then a thin gap row, then the lanthanides (row 9) and actinides (row 10)
    auto cellPos = [&](int row, int col) {
        const float y = row <= 7 ? o.y + (row - 1) * (ch + gap) : o.y + 7 * (ch + gap) + rowGap + gap + (row - 9) * (ch + gap);
        return ImVec2(o.x + (col - 1) * (cw + gap), y);
    };
    int clicked = 0;
    ImFont* reg = fonts().regular;
    ImFont* bold = fonts().bold;
    for (int Z = 1; Z <= 118; Z++) {
        const auto [r, c] = ptPos(Z);
        const ImVec2 a = cellPos(r, c), b(a.x + cw, a.y + ch);
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(Z);
        if (ImGui::InvisibleButton("el", ImVec2(cw, ch))) clicked = Z;
        const bool hov = ImGui::IsItemHovered();
        if (hov) ImGui::SetTooltip("%s", displayable(cellTip(Z)).c_str());
        ImGui::PopID();
        const int hue = categoryHue(Z);
        const bool on = Z == sel, rad = !hasStable(Z);
        dl->AddRectFilled(a, b, hsl(hue, 70, 55, on ? 0.6 : hov ? 0.35 : 0.22), (big ? 4 : 3) * S());
        if (rad) dashedRect(dl, a, b, hsl(hue, 60, 45, 0.8), 1, 2.5f * S());
        else dl->AddRect(a, b, hsl(hue, 60, 50, 0.5), (big ? 4 : 3) * S());
        if (on) dl->AddRect(ImVec2(a.x + 1, a.y + 1), ImVec2(b.x - 1, b.y - 1), U32(th.accent), 2 * S(), 0, 2 * S());
        ImFont* f = on || big ? bold : reg;
        const float fs = (big ? 9.0f : 9.5f) * S() * (big && cw > 34 * S() ? 15.0f / 9.0f : 1.0f);
        const ImVec2 ts = f->CalcTextSizeA(fs, FLT_MAX, 0, symbol(Z));
        dl->AddText(f, fs, ImVec2(a.x + (cw - ts.x) / 2, a.y + (ch - ts.y) / 2), U32(th.fg), symbol(Z));
    }
    const int marks[4][2] = {{6, 3}, {7, 3}, {9, 2}, {10, 2}};
    const char* mt[4] = {"*", "**", "*", "**"};
    for (int i = 0; i < 4; i++) {
        const ImVec2 a = cellPos(marks[i][0], marks[i][1]);
        const ImVec2 ts = reg->CalcTextSizeA(11 * S(), FLT_MAX, 0, mt[i]);
        dl->AddText(reg, 11 * S(), ImVec2(a.x + (cw - ts.x) / 2, a.y + (ch - ts.y) / 2), U32(th.muted), mt[i]);
    }
    const ImVec2 end = cellPos(10, 1);
    ImGui::SetCursorScreenPos(ImVec2(o.x, end.y + ch + 6 * S()));
    ImGui::Dummy(ImVec2(avail, 0));
    return clicked;
}

// the element card (web: elemCardHTML): a tile, the name, chips and "Where you meet it"
void elementCard(int Z) {
    const Theme th = panelTheme();
    const int hue = categoryHue(Z);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float s = S(), pad = 10 * s, tile = 58 * s, width = ImGui::GetContentRegionAvail().x;
    const ImVec2 o = ImGui::GetCursorScreenPos();
    dl->ChannelsSplit(2);
    dl->ChannelsSetCurrent(1);
    // the tile
    const ImVec2 ta(o.x + pad, o.y + pad), tb(ta.x + tile, ta.y + tile);
    dl->AddRectFilled(ta, tb, hsl(hue, 70, 55, 0.24), 10 * s);
    dl->AddRect(ta, tb, hsl(hue, 60, 55, 1), 10 * s, 0, 1.5f * s);
    const std::string zt = std::to_string(Z);
    dl->AddText(fonts().regular, 12 * s, ImVec2(ta.x + 7 * s, ta.y + 5 * s), U32(th.muted), zt.c_str());
    const ImVec2 ss = fonts().bold->CalcTextSizeA(26 * s, FLT_MAX, 0, symbol(Z));
    dl->AddText(fonts().bold, 26 * s, ImVec2(ta.x + (tile - ss.x) / 2, ta.y + (tile - ss.y) / 2 + 5 * s), U32(th.fg), symbol(Z));
    // the body
    const float bx = tb.x + 12 * s, bwid = o.x + width - pad - bx;
    ImGui::SetCursorScreenPos(ImVec2(bx, o.y + pad));
    ImGui::BeginGroup();
    ImGui::PushFont(fonts().bold, B(17));
    ImGui::TextUnformatted(name(Z));
    ImGui::PopFont();
    // chips
    float x = 0;
    const float chipH = 18 * s;
    ImVec2 cp = ImGui::GetCursorScreenPos();
    cp.y += 3 * s;
    for (const std::string& c0 : elementChips(Z)) {
        const std::string c = displayable(c0);
        const ImVec2 t = fonts().regular->CalcTextSizeA(12 * s, FLT_MAX, 0, c.c_str());
        const float w = t.x + 16 * s;
        if (x > 0 && x + w > bwid) { x = 0; cp.y += chipH + 4 * s; }
        const ImVec2 a(cp.x + x, cp.y), b(a.x + w, a.y + chipH);
        dl->AddRectFilled(a, b, U32(th.card), chipH / 2);
        dl->AddRect(a, b, U32(th.line), chipH / 2);
        dl->AddText(fonts().regular, 12 * s, ImVec2(a.x + 8 * s, a.y + (chipH - t.y) / 2), U32(th.muted), c.c_str());
        x += w + 4 * s;
    }
    ImGui::SetCursorScreenPos(ImVec2(cp.x, cp.y + chipH + 6 * s));
    ImGui::PushTextWrapPos(o.x + width - pad);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    richText("<b>Where you meet it:</b> " + everyday(Z), false, 12.5f / 16, 0);
    ImGui::PopStyleVar();
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();
    const float bottom = std::max(tb.y, ImGui::GetItemRectMax().y) + pad;
    dl->ChannelsSetCurrent(0);
    dl->AddRectFilled(o, ImVec2(o.x + width, bottom), hsl(hue, 70, 55, 0.10), 12 * s);
    dl->AddRect(o, ImVec2(o.x + width, bottom), hsl(hue, 55, 55, 0.5), 12 * s);
    dl->ChannelsMerge();
    ImGui::SetCursorScreenPos(ImVec2(o.x, bottom + 8 * s));
    ImGui::Dummy(ImVec2(width, 0));
}

// the facts (web: factsHTML): lines, the solid/liquid/gas bar with room temperature and the slider's temperature, notes
void factsCard(int Z, double tempK) {
    const Theme th = panelTheme();
    const float s = S();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float width = ImGui::GetContentRegionAvail().x;
    ImVec2 p0 = ImGui::GetCursorScreenPos();
    dl->AddLine(p0, ImVec2(p0.x + width, p0.y), U32(th.line));
    ImGui::Dummy(ImVec2(0, 6 * s));
    richText(factsLinesHtml(Z), true, 13.0f / 16, 0.15f);
    const Temp& t = tempData(Z);
    if (std::isnan(t.melt) && std::isnan(t.boil)) return;
    const char f = t.flag;
    const double m = t.melt, b = t.boil;
    auto frac = [](double k) { return std::sqrt(std::min(k, kTMax) / kTMax); };
    const ImVec2 a = ImGui::GetCursorScreenPos();
    const float bh = 10 * s, by = a.y + 8 * s;
    dl->AddRectFilled(ImVec2(a.x, by), ImVec2(a.x + width, by + bh), IM_COL32(128, 128, 128, 51), 5 * s);
    const double sEnd = frac(f == 's' ? b : std::isnan(m) ? 0 : m);
    double lEnd = sEnd;
    if (!(f == 's' || std::isnan(m))) lEnd = sEnd + (frac(std::isnan(b) ? kTMax : b) - frac(m));
    dl->AddRectFilled(ImVec2(a.x, by), ImVec2(a.x + float(sEnd) * width, by + bh), IM_COL32(0x5a, 0xa0, 0xe6, 255), 5 * s, ImDrawFlags_RoundCornersLeft);
    if (lEnd > sEnd) dl->AddRectFilled(ImVec2(a.x + float(sEnd) * width, by), ImVec2(a.x + float(lEnd) * width, by + bh), IM_COL32(0x3f, 0xc0, 0xa8, 255));
    dl->AddRectFilled(ImVec2(a.x + float(lEnd) * width, by), ImVec2(a.x + width, by + bh), IM_COL32(0xe8, 0xa3, 0x5a, 255), 5 * s, ImDrawFlags_RoundCornersRight);
    const float rx = a.x + float(frac(kTRoom)) * width;               // room temperature
    dl->AddRectFilled(ImVec2(rx - 1 * s, by - 3 * s), ImVec2(rx + 1 * s, by + 13 * s), U32(th.fg, 0.75f));
    const float nx = a.x + float(std::sqrt(tempK / kTMax)) * width;   // the Temperature slider
    dl->AddTriangleFilled(ImVec2(nx - 5 * s, by - 6 * s), ImVec2(nx + 5 * s, by - 6 * s), ImVec2(nx, by + 1 * s), U32(th.accent));
    ImGui::Dummy(ImVec2(width, bh + 10 * s));
    ImGui::PushFont(nullptr, B(11));
    const float y = ImGui::GetCursorScreenPos().y;
    const std::string hi = jsNum(kTMax, 6) + " K";
    dl->AddText(ImVec2(a.x, y), U32(th.fg), "0 K");
    dl->AddText(ImVec2(a.x + (width - ImGui::CalcTextSize("room").x) / 2, y), U32(th.fg), "room");
    dl->AddText(ImVec2(a.x + width - ImGui::CalcTextSize(hi.c_str()).x, y), U32(th.fg), hi.c_str());
    ImGui::Dummy(ImVec2(width, 14 * s));
    ImGui::PopFont();
    const std::string notes = factsNotesHtml(Z);
    if (!notes.empty()) mutedWrapped(notes, 12.0f / 16);
    mutedWrapped(kFactsFootnote, 12.0f / 16);
    ImGui::Dummy(ImVec2(0, 4 * s));
}

// the light-spectrum picture (web: drawSpectrum): every line on a log scale, then visible light to scale
void spectrumPicture(int Z, int N, const std::vector<SeenLine>& seen) {
    const Theme th = panelTheme();
    const float s = S(), W = ImGui::GetContentRegionAvail().x / s, H = 158;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 o = ImGui::GetCursorScreenPos();
    auto P = [&](double x, double y) { return ImVec2(o.x + float(x) * s, o.y + float(y) * s); };
    ImFont* F = fonts().regular;
    const float fs = 11 * s;
    auto text = [&](const std::string& t0, double x, double y, double align, const render::Color& c) {   // y = baseline
        const std::string t = displayable(t0);
        const ImVec2 ts = F->CalcTextSizeA(fs, FLT_MAX, 0, t.c_str());
        dl->AddText(F, fs, ImVec2(o.x + float(x) * s - ts.x * float(align), o.y + float(y) * s - F->GetFontBaked(fs)->Ascent), U32(c), t.c_str());
    };
    auto width = [&](const std::string& t) { return F->CalcTextSizeA(fs, FLT_MAX, 0, t.c_str()).x / s; };
    const auto L = specLines(Z, N);
    double lim1 = 0, top5 = 0;
    for (const SpecLine& l : L) { if (l.lo == 1 && l.hi == 0) lim1 = l.lam; if (l.lo == 5 && l.hi == 6) top5 = l.lam; }
    const double a = std::log(lim1 * 0.85), b = std::log(top5 * 1.15);
    auto X = [&](double lam) { return 8 + (std::log(lam) - a) / (b - a) * (W - 16); };
    auto lamCol = [](double lam, float al) { const Rgb c = lamColor(lam); return IM_COL32(c.r, c.g, c.b, int(al * 255)); };
    text("All lines (log scale): shorter wavelength, more energy ←", 8, 11, 0, th.muted);
    for (int x = 8; x < W - 8; x++) {
        const double lam = std::exp(a + (x - 8) / (W - 16) * (b - a));
        const bool vis = lam >= 380 && lam <= 750;
        dl->AddRectFilled(P(x, 18), P(x + 1, 48), vis ? lamCol(lam, 0.9f) : U32(th.muted, 0.12f));
    }
    for (const SpecLine& l : L) {
        const double x = X(l.lam);
        const bool vis = l.lam >= 380 && l.lam <= 750;
        if (l.hi == 0) {   // the series limit: dashed
            for (double y = 16; y < 50; y += 4) dl->AddLine(P(x, y), P(x, std::min(50.0, y + 2)), U32(th.fg), 1);
            continue;
        }
        dl->AddRectFilled(P(x - 0.5, 18), P(x + 0.7, 48), vis ? IM_COL32(255, 255, 255, int((l.hi - l.lo > 8 ? 0.35 : 0.9) * 255)) : U32(th.fg, l.hi - l.lo > 8 ? 0.35f : 0.9f));
    }
    double ends[2] = {-1e9, -1e9};                                         // series names: two rows, so neighbours do not collide
    for (int lo = 1; lo <= 5; lo++) {
        double fl = 0, el = 0;
        for (const SpecLine& l : L) { if (l.lo == lo && l.hi == lo + 1) fl = l.lam; if (l.lo == lo && l.hi == 0) el = l.lam; }
        const std::string t = Z == 1 ? kSeries[lo] : "→ n=" + std::to_string(lo);
        const double tw = width(t), xm = std::max(tw / 2 + 4, std::min(W - tw / 2 - 4, (X(fl) + X(el)) / 2));
        const int row = xm - tw / 2 > ends[0] + 6 ? 0 : 1;
        ends[row] = xm + tw / 2;
        text(t, xm, 62 + 12 * row, 0.5, th.fg);
    }
    for (const SeenLine& sl : seen) { const double x = X(sl.lam); dl->AddTriangleFilled(P(x - 4, 12), P(x + 4, 12), P(x, 18), U32(th.accent)); }   // ▼ lines your jumps made
    auto rg = [](double v) { return v < 1 ? jsNum(v * 1000, 3) + " pm" : v < 1000 ? jsNum(v, 3) + " nm" : jsNum(v / 1000, 3) + " µm"; };
    text(rg(std::exp(a)), 8, 88, 0, th.muted);
    text(rg(std::exp(b)), W - 8, 88, 1, th.muted);
    const double y2 = 110;
    auto VX = [&](double lam) { return 8 + (lam - 380) / (750 - 380) * (W - 16); };
    text("Visible light, 380 to 750 nm (to scale):", 8, y2 - 6, 0, th.muted);
    for (int x = 8; x < W - 8; x++) dl->AddRectFilled(P(x, y2), P(x + 1, y2 + 30), lamCol(380 + (x - 8) / (W - 16) * 370, 0.28f));
    std::vector<SpecLine> vl;
    for (const SpecLine& l : L) if (l.hi != 0 && l.lam >= 380 && l.lam <= 750) vl.push_back(l);
    for (const SpecLine& l : vl) { const double x = VX(l.lam); dl->AddRectFilled(P(x - 1.5, y2), P(x + 1.5, y2 + 30), lamCol(l.lam, 1)); }
    std::vector<SpecLine> sorted = vl;
    std::stable_sort(sorted.begin(), sorted.end(), [](const SpecLine& u, const SpecLine& v) { return v.lam < u.lam; });
    double lastX = -99;
    for (const SpecLine& l : sorted) {
        const double x = VX(l.lam);
        if (std::fabs(x - lastX) > 44) { text(fmt::fixed(l.lam, 1), std::max(20.0, std::min(double(W) - 20, x)), y2 + 44, 0.5, th.fg); lastX = x; }
    }
    if (vl.empty()) text("no lines fall in visible light", W / 2, y2 + 20, 0.5, th.muted);
    ImGui::Dummy(ImVec2(W * s, H * s));
}

// the little particle box of the Temperature card (web: tempPicture): solid = locked and jiggling, liquid = flowing, gas = flying
struct TempBox {
    struct P { double sx, sy, x, y, vx, vy, a, b, w1, w2; };
    std::vector<P> P_;
    std::optional<std::string> prev;
    bool init = false;
    double t = 0;
    std::mt19937 rng{12345};
    double rand() { return std::uniform_real_distribution<double>(0, 1)(rng); }
    // returns the state shown
    std::optional<std::string> draw(int Z, double T, double dt) {
        const double W = 280, H = 120, R = 5.2;
        const int N = 32;
        if (!init) {
            const int cols = 8;
            const double gap = 15, x0 = (W - (cols - 1) * gap) / 2, y0 = H - 12 - 3 * gap;
            for (int i = 0; i < N; i++) { const double sx = x0 + (i % cols) * gap, sy = y0 + (i / cols) * gap; P_.push_back({sx, sy, sx, sy, 0, 0, rand() * 6.28, rand() * 6.28, 9 + rand() * 6, 9 + rand() * 6}); }
            init = true;
        }
        t += dt;
        const Temp& td = tempData(Z);
        const auto st = tempState(Z, T);
        const int hue = categoryHue(Z);
        const double m = td.melt, b = td.boil;
        const double hi = std::isnan(b) ? kTMax : b, m0 = std::isnan(m) ? 0 : m;
        const double frac = std::max(0.0, std::min(1.0, (T - m0) / std::max(1.0, hi - m0)));
        const double heat = std::sqrt(T / kTMax);
        if (st && *st == "liquid" && prev && *prev == "solid") for (P& q : P_) { q.vx = (rand() - .5) * 30; q.vy = (rand() - .5) * 30; }
        if (st && *st == "gas" && !(prev && *prev == "gas")) for (P& q : P_) { const double an = rand() * 6.28; q.vx = std::cos(an) * 60; q.vy = std::sin(an) * 60; }
        prev = st;
        for (P& q : P_) {
            if (!st || *st == "solid") {
                const double A = !st ? 0 : 0.35 + 2.9 * std::sqrt(T / std::max(1.0, td.flag == 's' ? b : m)), k = 1 - std::exp(-7 * dt);
                const double tx = q.sx + A * std::sin(t * q.w1 + q.a), ty = q.sy + A * std::sin(t * q.w2 + q.b);
                q.x += (tx - q.x) * k; q.y += (ty - q.y) * k; q.vx = q.vy = 0;
            } else {
                const bool liquid = *st == "liquid";
                const double kick = liquid ? 35 + 110 * frac : 0;
                if (liquid) {
                    q.vy += 300 * dt;
                    q.vx += (W / 2 - q.x) * 1.6 * dt + (rand() - .5) * kick * std::sqrt(dt) * 3;
                    q.vy += (rand() - .5) * kick * std::sqrt(dt) * 3;
                    const double d = std::exp(-2.2 * dt);
                    q.vx *= d; q.vy *= d;
                } else {
                    const double sp = std::hypot(q.vx, q.vy) > 0 ? std::hypot(q.vx, q.vy) : 1, tg = 55 + 110 * heat;
                    q.vx *= 1 + (tg / sp - 1) * std::min(1.0, 3 * dt); q.vy *= 1 + (tg / sp - 1) * std::min(1.0, 3 * dt);
                }
                q.x += q.vx * dt; q.y += q.vy * dt;
                if (q.x < R) { q.x = R; q.vx = std::fabs(q.vx); }
                if (q.x > W - R) { q.x = W - R; q.vx = -std::fabs(q.vx); }
                if (q.y < R) { q.y = R; q.vy = std::fabs(q.vy); }
                if (q.y > H - R) { q.y = H - R; q.vy = -std::fabs(q.vy) * (liquid ? 0.3 : 1); }
            }
        }
        if (st && *st == "liquid")                                         // atoms in a liquid touch but cannot overlap
            for (int it = 0; it < 3; it++)
                for (int i = 0; i < N; i++)
                    for (int j = i + 1; j < N; j++) {
                        P &a = P_[i], &c = P_[j];
                        const double dx = c.x - a.x, dy = c.y - a.y, d = std::hypot(dx, dy), md = 2 * R - 0.6;
                        if (d < md && d > 0.01) { const double push = (md - d) / 2 / d; a.x -= dx * push; a.y -= dy * push; c.x += dx * push; c.y += dy * push; }
                    }
        // draw, scaled to the panel's width
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float width = ImGui::GetContentRegionAvail().x, k = width / float(W);
        const ImVec2 o = ImGui::GetCursorScreenPos();
        dl->AddRectFilled(o, ImVec2(o.x + width, o.y + float(H) * k), IM_COL32(128, 128, 128, 26), 6 * S());
        dl->AddRect(ImVec2(o.x + 0.5f * k, o.y + 0.5f * k), ImVec2(o.x + width - 0.5f * k, o.y + float(H - 0.5) * k), IM_COL32(128, 128, 128, 128), 6 * S());
        for (const P& q : P_) {
            const ImVec2 c(o.x + float(q.x) * k, o.y + float(q.y) * k);
            dl->AddCircleFilled(c, float(R) * k, hsl(hue, 70, st && *st == "gas" ? 62 : 55, 1));
            dl->AddCircle(c, float(R) * k, hsl(hue, 60, 30, 0.7), 0, 1);
        }
        ImGui::Dummy(ImVec2(width, float(H) * k));
        return st;
    }
};
TempBox g_tempBox;

}  // namespace

// ---------------------------------------------------------------- the lab picker
void openLab(AppState& app, int lab) {
    if (app.lab == lab) return;
    app.lab = lab;
    if (!app.visited[lab] && look().reduced) app.paused = true;   // reduced motion: a lab starts still (the web's load())
    app.visited[lab] = true;
}

void labPicker(AppState& app) {
    const float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
    const char* names[2] = {"Orbit Lab", "Atom Lab"};
    for (int i = 0; i < 2; i++) {
        if (i) ImGui::SameLine();
        const bool on = app.lab == i;
        if (on) pushOn();
        if (ImGui::Button(names[i], ImVec2(w, 0))) openLab(app, i);
        if (on) popOn();
    }
}

// ---------------------------------------------------------------- the panel
void atomPanel(AppState& app, float width) {
    AtomLab& L = app.atom;
    const AtomParams& p = L.p;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x - width, vp->WorkPos.y + headerHeight()));   // under the header
    ImGui::SetNextWindowSize(ImVec2(width, vp->WorkSize.y - headerHeight()));
    ImGui::Begin("Atom controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    if (look().synthGlow) {                                   // Synthwave: the page's purple glow, seen through this panel
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        drawSynthGlow(ImGui::GetWindowDrawList(), vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y));
    }
    if (const char* sc = std::getenv("PL_SCROLL")) ImGui::SetScrollY(float(std::atof(sc)));   // for automatic checks
    labPicker(app);
    defaultsButton(app);
    ImGui::SameLine();
    if (ImGui::Button("Reset")) L.restart();                  // the web's Reset: a fresh atom, the same settings
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Restart this lab's simulation");
    shotButton(app, true);                                    // the web's 📷 Save image
    shotNote(app);
    if (wideButton(app.paused ? "▶ Play" : "❚❚ Pause")) app.paused = !app.paused;
    ImGui::TextDisabled("Space: play / pause");
    atomHelpButton(app);

    // ---- Nucleus ----
    ImGui::SeparatorText("Nucleus");
    {
        const int z = periodicTable(p.Z, 26 * S(), false);
        if (z) pickElement(app, z);
        if (ImGui::Button(displayable("⤢ Bigger table").c_str())) app.bigTable = true;
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open the periodic table large, with bigger buttons");
        mutedWrapped(isotopeLine(p.Z, p.N), 12.0f / 16);
        ImGui::Dummy(ImVec2(0, 4 * S()));
        elementCard(p.Z);
        mutedWrapped(kTableNote, 12.0f / 16);
        ImGui::Dummy(ImVec2(0, 4 * S()));
        factsCard(p.Z, app.tempK);
        if (L.showBuildAnother() && ImGui::Button(displayable("☢ Build another").c_str())) L.buildAnother();
        if (L.showBuildAnother() && L.showReshuffle()) ImGui::SameLine();
        if (L.showReshuffle() && ImGui::Button(displayable("🎲 Reshuffle nucleons").c_str())) L.reshuffle();
        sliderRow(app, "Z", "Protons (Z)", std::to_string(p.Z) + " · " + name(p.Z), p.Z, 1, 118, 1, true, "proton");
        const auto nr = nRange(p.Z);
        sliderRow(app, "N", "Neutrons", std::to_string(p.N), p.N, nr.first, nr.second, 1, true, "neutron");
        ImGui::TextUnformatted("Famous isotopes");
        int cur = -1;
        const auto& fam = famous();
        for (int i = 0; i < int(fam.size()); i++) if (fam[i].Z == p.Z && fam[i].N == p.N) cur = i;
        auto famName = [&](int i) { return std::string(name(fam[i].Z)) + "-" + std::to_string(fam[i].Z + fam[i].N) + " · " + fam[i].why; };
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##famous", cur < 0 ? "Pick one…" : famName(cur).c_str())) {
            for (int i = 0; i < int(fam.size()); i++)
                if (ImGui::Selectable(famName(i).c_str(), i == cur)) { L.setParam("Z", fam[i].Z); L.setParam("N", fam[i].N); }
            ImGui::EndCombo();
        }
    }

    // ---- Decay history ----
    ImGui::SeparatorText("Decay history");
    {
        const auto hist = decayHistory(L.chain, p.Z, p.N);
        if (hist.size() == 1) mutedWrapped(hist[0].second, 13.0f / 16);
        else {
            ImGui::BeginChild("##hist", ImVec2(-1, std::min(320 * S(), (hist.size() - 1) * ImGui::GetTextLineHeightWithSpacing() + 8)), ImGuiChildFlags_None);
            for (std::size_t i = 0; i + 1 < hist.size(); i++) {
                const std::string t = displayable(hist[i].second);
                if (hist[i].first == "decStep") { ImGui::Indent(10 * S()); ImGui::TextDisabled("%s", t.c_str()); ImGui::Unindent(10 * S()); }
                else { ImGui::PushFont(fonts().bold, 0.0f); ImGui::TextUnformatted(t.c_str()); ImGui::PopFont(); }
            }
            ImGui::EndChild();
            mutedWrapped(hist.back().second, 13.0f / 16);
        }
    }

    // ---- Learn ----
    atomLearnButtons(app);

    // ---- View & zoom ----
    ImGui::SeparatorText("View & zoom");
    if (L.showSpin() && ImGui::Button(displayable("⟳ Spin on/off").c_str())) L.toggleSpin();
    selectRow(app, "scale", "Scale", {{"Auto", "Auto-fit (orbit fills the canvas)"}, {"Fixed", "Fixed (same scale for every atom)"}}, p.scale);
    sliderRow(app, "zoom", "Zoom (log scale)", "×" + fmt::jsLocale(std::round(std::pow(10, p.zoom))), p.zoom, 0, 7, 0.01, false);
    if (p.zoom >= 4) {
        selectRow(app, "qstyle", "Quark style", {{"Cinematic", "Cinematic (glowing, colour charge)"}, {"Accurate", "Accurate (point quarks, colour charge)"}, {"Classic", "Classic (simple balls and tubes)"}}, p.qstyle);
        selectRow(app, "detail", "Nucleon detail", {{"Valence", "Valence quarks only"}, {"+ Gluons", "+ Gluons (sparks)"}, {"+ Sea", "+ Sea (virtual pairs)"}}, p.detail);
    }
    if (p.zoom >= 3) selectRow(app, "style", "Nucleon edge", {{"Fuzzy", "Fuzzy (real charge density)"}, {"Hard balls", "Hard balls (simple)"}}, p.style);

    // ---- Electron ----
    ImGui::SeparatorText("Electron");
    selectRow(app, "view", "View", {{"Orbit", "Orbit (electron on a circle)"}, {"Cloud", "Cloud (quantum, side view)"}, {"Cloud 3D", "Cloud 3D (quantum, rotatable)"}, {"Slice", "Slice (cross-section of the cloud)"}}, p.view);
    mutedWrapped("The energy E in the line over the picture is the one-electron, non-relativistic level, −Rydberg energy × Z²/n², with the nucleus's real mass (hydrogen: −13.60 eV). In the Orbit view it is worked out from the electron's position and speed and scaled to that level. "
                 "Fine structure, the Lamb shift and spin are not modelled, so real energies differ a little (more for heavy atoms).", 12.0f / 16);
    if (p.view == "Orbit") selectRow(app, "ostyle", "Orbit style", {{"Modern", "Modern (real nucleus, quarks when you zoom in)"}, {"Bohr 1913", "Bohr 1913 (simple balls, as Bohr drew it)"}}, p.ostyle);
    if (p.view != "Orbit") {
        selectRow(app, "orb", p.super == "On" ? "First orbital" : "Orbital", SHELLS, p.orb);
        const int l = orbitals()[orbIndex(p.orb)].l;
        if (l == 1) selectRow(app, "pAx", "Orientation", P_AX, p.pAx);
        if (l == 2) selectRow(app, "dAx", "Orientation", D_AX, p.dAx);
        if (l == 3) selectRow(app, "fAx", "Orientation", F_AX, p.fAx);
    }
    if (p.view != "Orbit") {
        selectRow(app, "cstyle", "Cloud style", {{"Classic", "Classic · small hard dots, as in the web version"}, {"Glow", "Glow · soft dots that add up: dense places glow, thin places fade"}}, p.cstyle);
        selectRow(app, "cdens", "Cloud density", {{"Normal", "Normal · 12,000 dots. More dots show the same probability more smoothly. They do not change the physics."},
                                                   {"High", "High · 24,000 dots. More dots show the same probability more smoothly. They do not change the physics."},
                                                   {"Ultra", "Ultra · 48,000 dots. More dots show the same probability more smoothly. They do not change the physics."}}, p.cdens);
    }
    if (p.view != "Orbit")
        selectRow(app, "outline", "90% outline", {{"Off", "Off · Shows where the electron is found 90% of the time (one-electron model)."},
                                                   {"On", "On · Thin line round the smallest region where the electron is found 90% of the time. One electron, hydrogen-like model; atoms with more electrons differ."}}, p.outline);
    if (p.view != "Orbit") {
        selectRow(app, "phase", "Phase colours", {{"Sign", "Sign · two colours: blue where the wave ψ is +, orange where it is −"},
                                                   {"Wheel", "Wheel · a colour for every phase of the wave, shown by the colour ring in the corner"}}, p.phase);
        mutedWrapped("What the colours mean: a wave rises and falls, and its phase says where in that cycle it is. The colour of a dot shows the phase of the wave there. It is not a place and not how likely the electron is there (that is how many dots there are). "
                     "The orientations m = +1, −1, … are complex waves whose phase turns round the z axis (angular momentum about z of m ħ); they are mixtures of the real orientations and always use the wheel. "
                     "One electron, non-relativistic, no fine structure: all l of one n have the same energy here, and so do all m.", 12.0f / 16);
    }
    if (p.view == "Orbit") {                                           // a Bohr orbit has no superposition: the control is there but disabled
        ImGui::BeginDisabled();
        selectRow(app, "super", "Superposition", {{"Off", "Off · one orbital"}, {"On", "On · two orbitals at once"}}, "Off");
        ImGui::EndDisabled();
        mutedWrapped("A Bohr orbit has no superposition; switch to a cloud view.", 12.0f / 16);
    } else {
        selectRow(app, "super", "Superposition", {{"Off", "Off · one orbital"},
                                                  {"On", "On · two orbitals at once: the cloud changes in time, at the beat frequency (quantum beats). Slowed down on screen."}}, p.super);
        if (p.super == "On") {
            selectRow(app, "orb2", "Second orbital", SHELLS, p.orb2);
            const int l2 = orbitals()[orbIndex(p.orb2)].l;
            if (l2 == 1) selectRow(app, "pAx2", "Second orientation", P_AX, p.pAx2);
            if (l2 == 2) selectRow(app, "dAx2", "Second orientation", D_AX, p.dAx2);
            if (l2 == 3) selectRow(app, "fAx2", "Second orientation", F_AX, p.fAx2);
            sliderRow(app, "mix", "Mix (share of the second orbital)", fmt::jsLocale(std::round(p.mix)) + " %", p.mix, 0, 100, 1, false);
            sliderRow(app, "speed", "Playback speed (beat speed)", std::to_string(p.speed), p.speed, 1, 30, 1, false);
            mutedWrapped("Two orbitals at once, ψ = a·ψ₁ + b·ψ₂ with their own energies. The density then changes in time at the beat frequency (E₂ − E₁)/h, like two notes beating. "
                         "Real beats are far too fast to see (hydrogen 1s + 2p: about 4×10⁻¹⁶ s), so the picture is slowed down; the line under the picture gives the real period and the slow-down. "
                         "1s + 2p: the cloud's centre sloshes back and forth (an oscillating dipole). 1s + 2s: the cloud breathes. 2s + 2p: the same n has the same energy here, so there are no beats (fine structure and the Lamb shift are not modelled). "
                         "Colours are the phase of the wave relative to the first orbital: a wave's overall phase means nothing, so the colours turn only once per beat. "
                         "Playback speed sets the beat speed (4 s per beat at 5); Space pauses. One electron, non-relativistic. No photon jumps and no 90% outline while a superposition is shown. "
                         "If the two states overlap (the same state twice, or a real orientation with an m state of the same shell, such as 2px and 2p m = +1), the mixture is renormalised: the percentages are then only the amplitudes of the mix, not exact shares, and the cloud is one fixed shape.", 12.0f / 16);
        }
    }
    if (p.view == "Slice") {
        selectRow(app, "cutAxis", "Cut direction", {{"Vertical", "Vertical (contains the z axis)"}, {"Horizontal", "Horizontal (across the z axis)"}}, p.cutAxis);
        sliderRow(app, "cut", "Cut position (0 = through the nucleus)", fmt::jsString(p.cut), p.cut, -1, 1, 0.01, false);
    }
    if (p.view == "Orbit") {
        sliderRow(app, "n", "Energy level (n)", std::to_string(p.n), p.n, 1, 4, 1, false);
        sliderRow(app, "speed", "Playback speed", std::to_string(p.speed), p.speed, 1, 30, 1, false);
    }

    // ---- Photons ----
    ImGui::SeparatorText("Photons");
    if (ImGui::Button("↓ Emit photon")) L.emit();
    ImGui::SameLine();
    if (ImGui::Button("↑ Absorb photon")) L.absorb();
    if (ImGui::Button(displayable(L.autoText()).c_str())) L.toggleAuto();
    mutedWrapped(p.view == "Orbit"
        ? "How a jump is drawn: the electron glides from one circle to the other over about 0.8 s, only to help the eye. Bohr's model has no path between circles: the electron is on one circle, and then on the other. "
          "A photon you emit appears when the glide ends; an absorbed photon arrives first, and then the glide starts. With Reduced motion on (Settings, Motion) the electron leaps at once."
        : "How a jump is drawn: the standard textbook picture of a transition, in which the electron's wave is a mixture of the old and the new state and slides from one shape to the other (1.2 s), "
          "sloshing at the slowed-down beat frequency on the way. It is not a calculation of how a photon is absorbed or emitted. A real jump is very different from the screen: the screen is slowed down, and the real time is not shown. "
          "The 90% outline fades out as the cloud leaves its orbital and back in after landing. A photon you emit appears when the jump ends; an absorbed photon arrives first, and then the jump starts. "
          "With Reduced motion on (Settings, Motion) the jump is over at once.", 12.0f / 16);

    // ---- Light spectrum ----
    ImGui::SeparatorText("Light spectrum");
    spectrumPicture(p.Z, p.N, L.s.lines);
    ImGui::PushTextWrapPos(0);
    richText(spectrumNote(p.Z, p.N), true, 12.0f / 16, 0);
    ImGui::PopTextWrapPos();

    // ---- Temperature ----
    ImGui::SeparatorText("Temperature");
    {
        auto v2T = [](double v) { return kTMax * (v / 1000) * (v / 1000); };
        auto T2v = [](double T) { return 1000 * std::sqrt(T / kTMax); };
        ImGui::TextUnformatted("Temperature");
        int tv = int(std::lround(T2v(app.tempK)));
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderInt("##temp", &tv, 0, 1000, "")) app.tempK = v2T(tv);
        const int Zs = p.Z;
        richText(tempStateHtml(Zs, app.tempK), true, 13.0f / 16, 0.2f);
        richText(tempSpeedHtml(Zs, app.tempK), true, 12.0f / 16, 0.2f);
        g_tempBox.draw(Zs, app.tempK, std::min(0.05, double(ImGui::GetIO().DeltaTime)));
        auto jump = [&](const char* txt, double T) { if (ImGui::Button(displayable(txt).c_str())) { if (!std::isnan(T)) app.tempK = std::max(0.0, std::min(kTMax, T)); } };
        const Temp& td = tempData(Zs);
        jump("❄ Cold", 3); ImGui::SameLine();
        jump("🏠 Room", kTRoom); ImGui::SameLine();
        jump("🔥 Just melted", td.flag == 's' ? td.boil - 1 : std::isnan(td.melt) ? td.boil - 0.5 : td.melt + 1);
        jump("💨 Just boiled", std::isnan(td.boil) ? NAN : td.boil + 1);
        ImGui::SetNextItemOpen(app.tempDetails);
        app.tempDetails = ImGui::TreeNodeEx(displayable("🌡 What is temperature, really?").c_str(), ImGuiTreeNodeFlags_NoTreePushOnOpen);
        if (app.tempDetails) {
            ImGui::PushTextWrapPos(0);
            richText(kTempDetailsHtml, true, 13.0f / 16, 0.4f);
            ImGui::PopTextWrapPos();
        }
        mutedWrapped(kTempNote, 12.0f / 16);
    }
    // ---- Performance ----
    performanceCard(app);
    ImGui::TextDisabled("Picture: %d draw calls, %d instanced shapes", app.drawCalls, app.drawInstances);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cloud dots, glows and gluon-field particles are drawn as instanced shapes,\na few draw calls for thousands of them.");

    // a slider or button held down: the decay clock waits (the web's PL_HOLD)
    L.hold = ImGui::IsAnyItemActive() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    ImGui::End();

    // ---- the large periodic table ----
    if (app.bigTable) {
        ImGui::SetNextWindowSize(ImVec2(std::min(760 * S(), vp->WorkSize.x * 0.95f), std::min(560 * S(), vp->WorkSize.y * 0.95f)), ImGuiCond_Appearing);
        ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x / 2, vp->WorkPos.y + vp->WorkSize.y / 2), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::Begin("Periodic table", &app.bigTable, ImGuiWindowFlags_NoCollapse)) {
            mutedWrapped("Tap an element to load its most common (or longest-lived) isotope. Dashed edges are elements with no stable isotope.", 13.0f / 16);
            // the category legend
            float x = 0;
            const float avail = ImGui::GetContentRegionAvail().x, h = 20 * S();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 o = ImGui::GetCursorScreenPos();
            for (int c = 0; c < 10; c++) {
                int z = 1;
                while (categoryIndex(z) != c) z++;
                const std::string t = categoryName(z);
                const float w = ImGui::CalcTextSize(t.c_str()).x * 0.75f + 18 * S();
                if (x > 0 && x + w > avail) { x = 0; o.y += h + 6 * S(); }
                const ImVec2 a(o.x + x, o.y), b(a.x + w, a.y + h);
                dl->AddRectFilled(a, b, hsl(categoryHue(z), 70, 55, 0.2), h / 2);
                dl->AddRect(a, b, hsl(categoryHue(z), 60, 50, 0.6), h / 2);
                dl->AddText(fonts().regular, 12 * S(), ImVec2(a.x + 9 * S(), a.y + 3 * S()), U32(panelTheme().fg), t.c_str());
                x += w + 6 * S();
            }
            ImGui::SetCursorScreenPos(ImVec2(o.x, o.y + h + 10 * S()));
            const float ch = std::max(30 * S(), std::min(50 * S(), (ImGui::GetContentRegionAvail().y - 30 * S()) / 9.5f));
            const int z = periodicTable(p.Z, ch, true);
            if (z) { pickElement(app, z); app.bigTable = false; }
        }
        ImGui::End();
    }
}

// ---------------------------------------------------------------- the bar under the picture
float atomBottomBar(AppState& app, float x, float y, float width, bool draw) {
    AtomLab& L = app.atom;
    const float pad = 8 * S();
    const std::string hint = displayable(L.hint());
    const float wrapW = width - 2 * pad;
    const float hintH = ImGui::CalcTextSize(hint.c_str(), nullptr, false, wrapW).y * (13.0f / 16) * 1.12f;
    const float barH = pad + ImGui::GetFrameHeight() + 6 * S() + hintH + pad;
    if (!draw) return barH;
    ImGui::SetNextWindowPos(ImVec2(x, y));
    ImGui::SetNextWindowSize(ImVec2(width, barH));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(pad, pad));
    ImGui::Begin("Atom view bar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);
    ImGui::PopStyleVar();
    float used = 0;
    auto button = [&](const std::string& t, bool show) {
        if (!show) return false;
        const std::string d = displayable(t);
        const float w = ImGui::CalcTextSize(d.c_str()).x + 2 * ImGui::GetStyle().FramePadding.x;
        if (used > 0) { if (used + 6 * S() + w < width - 2 * pad) ImGui::SameLine(0, 6 * S()); else used = 0; }
        used += (used > 0 ? 6 * S() : 0) + w;
        return ImGui::Button(d.c_str());
    };
    if (button("⌕ Atom", true)) L.zoomAtom();
    if (button("⌕ Nucleus", true)) L.zoomNucleus();
    if (button("⌕ Nucleon", true)) L.zoomNucleon();
    if (button("⌖ Centre", L.showCentre())) L.centre();
    if (button(L.dragText(), L.showDrag())) L.toggleDrag();
    if (button("↺ Reset view", L.showResetView())) L.resetView();
    ImGui::PushFont(nullptr, B(13));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted(hint.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::End();
    return barH;
}

}  // namespace ui
