// The Data panel's celestial mechanics tools (0.92.0): see celestial_panel.h.
#include "ui/celestial_panel.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <future>
#include <memory>

#include "imgui.h"

#include "app/shell.h"
#include "orbit/belts.h"
#include "orbit/ephem_table.h"
#include "orbit/format.h"
#include "orbit/lagrange.h"
#include "orbit/lambert.h"
#include "orbit/solar_data.h"
#include "orbit/time_scale.h"
#include "orbit/units.h"
#include "ui/porkchop_runner.h"
#include "ui/shell_test.h"

namespace ui {

using app::shell::Type;

namespace {

struct CM {
    bool fieldActive = false;
    // Lagrange
    int lgSys = 0;
    bool lgPick = false;
    // transfer planner
    int trDep = 2, trArr = 3;
    float depOff = -90, depSpan = 300, arrOff = 120, arrSpan = 420, side = 96;     // days from the date shown; cells on a side
    int trShow = 0;                                                                // 0: C3 at departure, 1: v_inf at arrival
    int trSig = 0;                                                                 // changes whenever the grid must be made again
    bool have = false;
    orbit::lambert::PorkchopInput in;
    orbit::lambert::PorkchopGrid grid;
    int selI = -1, selJ = -1;
    double madeAtMs = 0;
    // ephemeris
    int ephPlanet = 4, ephModel = 0, ephStep = 1;
    float ephCount = 8;
    char ephDate[16] = "";
    std::vector<orbit::ephem::Row> ephRows;
    std::string ephKey, ephNote;
    std::future<std::vector<orbit::ephem::Row>> ephFut;
    std::string ephFutKey;
    // drift
    int drIntegrator = 0, drSpan = 1;
    bool drGr = false;
    std::future<orbit::ephem::DriftResult> drFut;
    std::atomic<long> drProgress{0};
    std::atomic<bool> drStop{false};
    long drTotal = 0;
    bool drHave = false;
    orbit::ephem::DriftResult drRes;
};
CM C;

using Clock = std::chrono::steady_clock;

const char* const kShort[8] = {"Mer", "Ven", "Ear", "Mar", "Jup", "Sat", "Ura", "Nep"};
std::string planetName(int i) { return orbit::kPlanets[i].name; }

std::string commas(double v) { return fmt::commas(v); }
std::string num(double v, int digits) { char b[48]; std::snprintf(b, sizeof b, "%.*g", digits, v); return b; }

// ImGui text box in the Caption look, as the Data panel's own
bool textField(sh::Panel& p, const char* label, const char* id, char* buf, std::size_t n) {
    const sh::Frame& F = sh::F();
    const float lh = static_cast<float>(F.m.control);
    sh::text(p.dl, Type::Caption, false, sh::here(p), sh::cMuted(), label);
    p.y += sh::lineH(Type::Caption) + static_cast<float>(F.m.s4);
    ImGui::SetCursorScreenPos(sh::here(p));
    ImGui::PushFont(nullptr, app::shell::fontBase(Type::Caption, F.dpi, F.tf));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(sh::cFace()));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(sh::cText()));
    ImGui::SetNextItemWidth(p.w);
    const bool changed = ImGui::InputText(id, buf, n, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
    if (ImGui::IsItemActive()) C.fieldActive = true;
    noteItem(id);
    ImGui::PopStyleColor(2);
    ImGui::PopFont();
    p.y += lh + static_cast<float>(F.m.s4);
    return changed || (ImGui::IsItemDeactivatedAfterEdit());
}

ImU32 ramp(double t) {           // the lab's colour ramp (orbit/lambert.h rampRGB, also what the card's grid is compared through)
    unsigned char c[3];
    orbit::lambert::rampRGB(t, c);
    return IM_COL32(c[0], c[1], c[2], 255);
}

// the checks read one line a tool (PL_LOG_SHELL), printed when it changes
void logLine(const std::string& line) {
    static std::string last;
    if (!shellLogOn() || line == last) return;
    last = line;
    std::printf("PLCELESTIAL %s\n", line.c_str());
    std::fflush(stdout);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
void bumpOverlay(AppState& app) { app.set.overlay.version++; }

void drawLagrange(sh::Panel& p, AppState& app) {
    const auto& S = orbit::lagrange::systems();
    C.lgSys = std::max(0, std::min(int(S.size()) - 1, C.lgSys));
    if (sh::pRow(p, "cm.lg.pick", std::string(C.lgPick ? "▾ " : "▸ ") + "Pair (heavy - light): " + S[std::size_t(C.lgSys)].name)) C.lgPick = !C.lgPick;
    noteItem("cm.lg.pick");
    if (C.lgPick)
        for (std::size_t i = 0; i < S.size(); i++)
            if (sh::pRow(p, ("cm.lg." + std::to_string(i)).c_str(), "    " + S[i].name, int(i) == C.lgSys)) { C.lgSys = int(i); C.lgPick = false; bumpOverlay(app); }
    const orbit::lagrange::System& s = S[std::size_t(C.lgSys)];
    orbit::Overlay& O = app.set.overlay;
    if (O.heavy != s.heavy || O.light != s.light || O.mu != s.mu) { O.heavy = s.heavy; O.light = s.light; O.mu = s.mu; bumpOverlay(app); }
    bool on = O.lagrange;
    if (sh::pCheck(p, "cm.lg.show", "Show the five points on the picture (2D and 3D)", &on)) { O.lagrange = on; bumpOverlay(app); }
    sh::pRule(p);
    const orbit::lagrange::Table T = orbit::lagrange::table(s);
    const double hillKm = orbit::lagrange::hillRadius(s.mu) * s.sepKm;
    sh::pStat(p, "Mass fraction μ = light / (heavy + light)", num(s.mu, 6));
    sh::pStat(p, "Separation a (the circular orbit's radius)", commas(s.sepKm) + " km");
    sh::pStat(p, "Hill radius of " + s.light + ", a (μ/3)^(1/3)", commas(hillKm) + " km");
    const char* nm[5] = {"L1 between the two", "L2 beyond the light one", "L3 beyond the heavy one", "L4 60° ahead of the light one", "L5 60° behind the light one"};
    sh::pHeading(p, "Where the five points are (distance from " + s.light + ")");
    for (int k = 0; k < 5; k++) {
        const std::string val = commas(T.fromLightKm[k]) + " km · " + num(T.inSep[k], 6) + " a";
        sh::pStat(p, nm[k], val);
    }
    {
        char b[400];
        std::snprintf(b, sizeof b, "lagrange pair=%s mu=%.9g sepkm=%.9g L1km=%.9g L2km=%.9g L3km=%.9g L4km=%.9g L5km=%.9g stable=%d overlay=%d", s.name.c_str(), s.mu, s.sepKm, T.fromLightKm[0], T.fromLightKm[1], T.fromLightKm[2], T.fromLightKm[3], T.fromLightKm[4],
                      orbit::lagrange::trianglePointsStable(s.mu) ? 1 : 0, O.lagrange ? 1 : 0);
        logLine(b);
    }
    sh::pHeading(p, "Stability of L4 and L5 (Routh's criterion)");
    const bool stable = orbit::lagrange::trianglePointsStable(s.mu);
    sh::pStat(p, "27 μ (1 − μ) < 1 ?", num(27 * s.mu * (1 - s.mu), 5) + (stable ? "  yes: stable" : "  no: unstable"));
    sh::pStat(p, "Critical μ", num(orbit::lagrange::routhCriticalMu(), 7));
    const orbit::lagrange::Libration L = orbit::lagrange::libration(s.mu);
    if (L.ok) sh::pStat(p, "Small orbits about L4, L5: periods", num(1 / L.fast, 5) + " and " + num(1 / L.slow, 5) + " of the pair's period");
    sh::pCaption(p, "L1, L2 and L3 are always unstable (saddles of the rotating frame's potential); a body there stays only with small pushes.");
    sh::pCaption(p, "Limits: the two bodies on circular orbits about their centre of mass, the third body with no mass, nothing else pulling and no radiation pressure. The Sun - planet pairs use the planet's whole system (Earth: Earth plus Moon) "
                    "and the planet's JPL semi-major axis; on the picture the points are drawn from the bodies where the picture draws them (a moon's orbit is drawn wider than it is, so its points are drawn wider too; the table has the real distances).");
    sh::pCaption(p, "Source: J. Tatum, Celestial Mechanics, University of Victoria, chapter 16 (equations 16.1.9 to 16.1.23 for the collinear points, 16.2.26 and 16.2.27 for the stability; see About, Where the facts come from). "
                    "GM values: JPL astrodynamic parameters (DE440) and JPL planetary satellite physical parameters.");
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawKirkwood(sh::Panel& p, AppState&) {
    const sh::Frame& F = sh::F();
    const auto& m = F.m;
    const double lo = 1.9, hi = 3.5;
    const int bins = 80;
    static const orbit::kirkwood::Histogram H = orbit::kirkwood::mainBelt(lo, hi, bins);
    const auto& R = orbit::kirkwood::resonances();
    sh::pStat(p, "Rocks in the group", commas(H.total) + " (the brightest numbered main-belt asteroids)");
    sh::pStat(p, "Jupiter's semi-major axis a_J", num(orbit::kirkwood::jupiterAU(), 6) + " AU (the lab's)");
    {
        std::string b = "kirkwood total=" + std::to_string(H.total) + " below=" + std::to_string(H.below) + " above=" + std::to_string(H.above) + " lo=" + num(lo, 6) + " w=" + num(H.w, 9) + " counts=";
        for (int c : H.count) b += std::to_string(c) + ",";
        b += " markers=";
        for (const auto& r : R) { char q[48]; std::snprintf(q, sizeof q, "%d:%d@%.9g,", r.p, r.q, r.aAU); b += q; }
        logLine(b);
    }
    sh::pHeading(p, "Number of rocks by semi-major axis (bins of " + num(H.w, 3) + " AU), with Jupiter's mean-motion resonances");
    const float h = static_cast<float>(m.em(13)), lh = sh::lineH(Type::Caption);
    const ImVec2 o = sh::here(p);
    int top = 1;
    for (int c : H.count) top = std::max(top, c);
    const float Lx = o.x + static_cast<float>(m.em(2.4)), Rx = o.x + p.w - static_cast<float>(m.s4), T = o.y + lh + static_cast<float>(m.s4), B = T + h;
    ImDrawList* dl = p.dl;
    dl->AddRect(ImVec2(Lx, T), ImVec2(Rx, B), sh::cEdge());
    const auto X = [&](double a) { return Lx + float((a - lo) / (hi - lo)) * (Rx - Lx); };
    const auto Y = [&](double c) { return B - float(c / (top * 1.1)) * (B - T); };
    for (int k = 0; k < 5; k++) {                                                    // count ticks
        const double c = top * k / 4.0;
        dl->AddLine(ImVec2(Lx, Y(c)), ImVec2(Rx, Y(c)), sh::cHair());
        sh::text(dl, Type::Caption, false, ImVec2(Lx - sh::width(Type::Caption, false, num(std::round(c), 4)) - static_cast<float>(m.s4), Y(c) - lh / 2), sh::cMuted(), num(std::round(c), 4));
    }
    for (double a = 2.0; a <= 3.4001; a += 0.2) {                                    // AU ticks
        char b[16]; std::snprintf(b, sizeof b, "%.1f", a);
        dl->AddLine(ImVec2(X(a), B), ImVec2(X(a), B + 3), sh::cEdge());
        sh::text(dl, Type::Caption, false, ImVec2(X(a) - sh::width(Type::Caption, false, b) / 2, B + static_cast<float>(m.s4)), sh::cMuted(), b);
    }
    for (int i = 0; i < bins; i++) {
        if (H.count[std::size_t(i)] == 0) continue;
        dl->AddRectFilled(ImVec2(X(lo + i * H.w) + 0.5f, Y(H.count[std::size_t(i)])), ImVec2(X(lo + (i + 1) * H.w) - 0.5f, B), sh::cAccent(0.75f));
    }
    for (std::size_t k = 0; k < R.size(); k++) {                                     // the resonances: a dashed line and its name, the names on two rows so they do not overlap
        const float x = X(R[k].aAU);
        for (float y = T; y < B; y += 8) dl->AddLine(ImVec2(x, y), ImVec2(x, std::min(B, y + 4)), sh::cText(0.8f), 1.2f);
        const std::string name = std::to_string(R[k].p) + ":" + std::to_string(R[k].q);
        sh::text(dl, Type::Caption, true, ImVec2(x - sh::width(Type::Caption, true, name) / 2, T - lh), sh::cText(), name);
    }
    sh::text(dl, Type::Caption, false, ImVec2(Lx, B + lh + static_cast<float>(m.s8)), sh::cMuted(), "semi-major axis a (AU)");
    p.y = (B + lh * 2 + static_cast<float>(m.s8) * 2) - p.origin.y;
    for (const auto& r : R) sh::pStat(p, std::to_string(r.p) + ":" + std::to_string(r.q) + " resonance (a = a_J (" + std::to_string(r.q) + "/" + std::to_string(r.p) + ")^(2/3))", num(r.aAU, 5) + " AU");
    sh::pCaption(p, "p:q: the rock goes round p times while Jupiter goes round q times. Kepler's third law (the period grows as a^(3/2)) puts that at a = a_J (q/p)^(2/3).");
    sh::pCaption(p, "The gaps come from the data: this is the count of the real rocks' osculating semi-major axes. The lab does not simulate how the gaps formed (Jupiter's pull over millions of years); "
                    "the rocks here are the brightest numbered asteroids, so faint ones are missing, and a few lie outside the plot (" + std::to_string(H.below + H.above) + " of " + std::to_string(H.total) + ").");
    sh::pCaption(p, "Source: NASA/JPL Small-Body Database (osculating elements at each record's epoch, see About, Where the facts come from); a_J: JPL approximate planetary elements.");
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
double nowMs() { return std::chrono::duration<double, std::milli>(Clock::now().time_since_epoch()).count(); }

orbit::lambert::PorkchopSetup setupOf(const AppState& app) {
    orbit::lambert::PorkchopSetup s;
    s.dep = C.trDep; s.arr = C.trArr;
    const double d0 = app.sys.julianDayTDB();
    const int n = std::max(8, std::min(celestialPorkchopMaxSide(), int(std::lround(C.side))));
    s.nDep = s.nArr = n;
    s.depJD0 = d0 + C.depOff;
    s.depStep = std::max(0.05, double(C.depSpan) / (n - 1));
    s.arrJD0 = s.depJD0 + C.arrOff;
    s.arrStep = std::max(0.05, double(C.arrSpan) / (n - 1));
    return s;
}

void selectCell(AppState& app, int i, int j) {
    C.selI = i; C.selJ = j;
    orbit::Overlay& O = app.set.overlay;
    O.transfer = false;
    if (i >= 0 && C.have) {
        const double* d = &C.in.dep[std::size_t(6) * std::size_t(i)];
        const double* a = &C.in.arr[std::size_t(6) * std::size_t(j)];
        const double tof = (C.in.s.arrJD0 + C.in.s.arrStep * j) - (C.in.s.depJD0 + C.in.s.depStep * i);
        const orbit::Vec3 r1{d[0], d[1], d[2]}, r2{a[0], a[1], a[2]};
        const orbit::lambert::Solution S = orbit::lambert::solve(r1, r2, tof / C.in.timeUnitDays, 1.0);
        if (S.ok) {
            const auto arc = orbit::lambert::transferArc(r1, S.v1, 1.0, tof / C.in.timeUnitDays, 160);
            O.arc.clear();
            for (const orbit::Vec3& q : arc) O.arc.push_back(q * units::kAU);
            O.depPos = r1 * units::kAU; O.arrPos = r2 * units::kAU;
            O.transfer = O.arc.size() > 1;
        }
    }
    bumpOverlay(app);
}

void remake(AppState& app) {
    C.in = orbit::lambert::porkchopInput(setupOf(app));
    porkchopRunner().start(app, C.in);
    C.madeAtMs = nowMs();
}

void drawTransfer(sh::Panel& p, AppState& app) {
    const sh::Frame& F = sh::F();
    const auto& m = F.m;
    sh::pHeading(p, "From");
    std::vector<std::string> names(kShort, kShort + 8);
    bool changed = false;
    changed |= sh::pSegmented(p, "cm.tr.dep", names, &C.trDep);
    sh::pHeading(p, "To");
    changed |= sh::pSegmented(p, "cm.tr.arr", names, &C.trArr);
    sh::pStat(p, planetName(C.trDep) + " to " + planetName(C.trArr), "on the lab's JPL-elements ephemeris (Simple)");
    const auto slider = [&](const char* id, const char* label, float* v, float lo, float hi, const std::string& value) {
        sh::pStat(p, label, value);
        changed |= sh::pSlider(p, id, v, lo, hi);
    };
    slider("cm.tr.depoff", "Departures start (days from the date shown)", &C.depOff, -800, 1500, num(std::round(C.depOff), 5) + " d");
    slider("cm.tr.depspan", "Departure window", &C.depSpan, 20, 1200, num(std::round(C.depSpan), 5) + " d");
    slider("cm.tr.arroff", "Arrivals start (days after the first departure)", &C.arrOff, 10, 1500, num(std::round(C.arrOff), 5) + " d");
    slider("cm.tr.arrspan", "Arrival window", &C.arrSpan, 20, 2000, num(std::round(C.arrSpan), 5) + " d");
    slider("cm.tr.side", "Grid: cells on a side", &C.side, 16, float(celestialPorkchopMaxSide()), num(std::round(C.side), 4) + " × " + num(std::round(C.side), 4) + " = " + commas(std::round(C.side) * std::round(C.side)) + " cells");
    sh::pSegmented(p, "cm.tr.show", {"C3 at departure", "v∞ at arrival"}, &C.trShow);
    if (changed || !C.have) { C.have = true; remake(app); C.selI = -1; selectCell(app, -1, -1); }
    orbit::lambert::PorkchopGrid g;
    if (porkchopRunner().poll(app, g)) { C.grid = std::move(g); }
    const auto& G = C.grid;
    if (porkchopRunner().busy() && G.c3.empty()) sh::pCaption(p, "Solving the grid…");
    if (G.nDep == 0) return;
    // range: the best cell to a few times it (the colours), NaN cells dark
    const std::vector<float>& V = C.trShow == 0 ? G.c3 : G.vinf;
    double vmin = 1e300;
    for (float v : V) if (std::isfinite(v) && v < vmin) vmin = v;
    const double vmax = vmin * 3.0;
    const float side = p.w - static_cast<float>(m.em(2.6));
    const float cellsOnScreen = float(std::min(G.nDep, 96));
    const ImVec2 o = sh::here(p);
    const float lh = sh::lineH(Type::Caption);
    const float Lx = o.x + static_cast<float>(m.em(2.6)), T = o.y + lh;
    ImDrawList* dl = p.dl;
    dl->AddRectFilled(ImVec2(Lx, T), ImVec2(Lx + side, T + side), IM_COL32(20, 20, 24, 255));
    const int step = std::max(1, int(std::ceil(G.nDep / cellsOnScreen)));
    const float cs = side / float(G.nDep);
    for (int i = 0; i < G.nDep; i += step)
        for (int j = 0; j < G.nArr; j += step) {
            const float v = V[std::size_t(i) * std::size_t(G.nArr) + std::size_t(j)];
            if (!std::isfinite(v)) continue;
            const float x = Lx + cs * i, y = T + side - cs * (j + step);       // departure to the right, arrival upwards
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + cs * step + 0.5f, y + cs * step + 0.5f), ramp((v - vmin) / (vmax - vmin)));
        }
    dl->AddRect(ImVec2(Lx, T), ImVec2(Lx + side, T + side), sh::cEdge());
    // the best cell and the selected one
    int bi = -1, bj = -1;
    for (int i = 0; i < G.nDep; i++) for (int j = 0; j < G.nArr; j++) { const float v = V[std::size_t(i) * std::size_t(G.nArr) + std::size_t(j)]; if (std::isfinite(v) && v <= vmin) { bi = i; bj = j; } }
    const auto centre = [&](int i, int j) { return ImVec2(Lx + cs * (i + 0.5f), T + side - cs * (j + 0.5f)); };
    if (bi >= 0) dl->AddCircle(centre(bi, bj), 4.5f, IM_COL32(255, 255, 255, 230), 12, 1.5f);
    if (C.selI >= 0) { const ImVec2 c = centre(C.selI, C.selJ); dl->AddLine(ImVec2(c.x - 8, c.y), ImVec2(c.x + 8, c.y), IM_COL32(255, 70, 70, 255), 1.6f); dl->AddLine(ImVec2(c.x, c.y - 8), ImVec2(c.x, c.y + 8), IM_COL32(255, 70, 70, 255), 1.6f); }
    // axes: dates at both ends
    const auto dateOf = [&](double jdTDB) { return orbit::ephem::jdUTToDate(jdTDB - timescale::deltaTSeconds(jdTDB) / 86400.0); };
    const orbit::lambert::PorkchopSetup& s = C.in.s;
    sh::text(dl, Type::Caption, false, ImVec2(Lx, o.y), sh::cMuted(), std::string("Arrival at ") + planetName(C.trArr) + " (up) against departure from " + planetName(C.trDep) + " (right)");
    sh::text(dl, Type::Caption, false, ImVec2(Lx, T + side + static_cast<float>(m.s4)), sh::cMuted(), dateOf(s.depJD0));
    const std::string de = dateOf(s.depJD0 + s.depStep * (s.nDep - 1));
    sh::text(dl, Type::Caption, false, ImVec2(Lx + side - sh::width(Type::Caption, false, de), T + side + static_cast<float>(m.s4)), sh::cMuted(), de);
    sh::text(dl, Type::Caption, false, ImVec2(o.x, T + side - lh), sh::cMuted(), "");
    {   // arrival dates on the left, written upright is not possible: first and last, small
        const std::string a0 = dateOf(s.arrJD0).substr(2), a1 = dateOf(s.arrJD0 + s.arrStep * (s.nArr - 1)).substr(2);
        sh::text(dl, Type::Caption, false, ImVec2(Lx - sh::width(Type::Caption, false, a1) - static_cast<float>(m.s4), T), sh::cMuted(), a1);
        sh::text(dl, Type::Caption, false, ImVec2(Lx - sh::width(Type::Caption, false, a0) - static_cast<float>(m.s4), T + side - lh), sh::cMuted(), a0);
    }
    const bool hover = sh::hoveredRect(ImVec2(Lx, T), ImVec2(side, side));
    int hi = -1, hj = -1;
    if (hover) {
        const ImVec2 mp = ImGui::GetIO().MousePos;
        hi = std::max(0, std::min(G.nDep - 1, int((mp.x - Lx) / cs)));
        hj = std::max(0, std::min(G.nArr - 1, int((T + side - mp.y) / cs)));
        if (ImGui::IsMouseClicked(0)) selectCell(app, hi, hj);
        if (ImGui::IsMouseDown(0)) { selectCell(app, hi, hj); }
    }
    p.y = (T + side + lh * 2 + static_cast<float>(m.s8)) - p.origin.y;
    noteItem("cm.tr.plot");
    sh::pCaption(p, (C.trShow == 0 ? "Colour: C3 at departure, " : "Colour: v∞ at arrival, ") + num(vmin, 4) + (C.trShow == 0 ? " km²/s²" : " km/s") + " (yellow is three times that); dark: no transfer (arrival not after departure). The white ring is the best cell. Click or drag in the plot to pick a cell.");
    // the picked cell
    const int ci = C.selI >= 0 ? C.selI : hi, cj = C.selI >= 0 ? C.selJ : hj;
    if (ci >= 0) {
        double c3 = 0, vi = 0, tof = 0;
        if (orbit::lambert::porkchopCell(C.in, ci, cj, &c3, &vi, &tof)) {
            sh::pHeading(p, C.selI >= 0 ? "The picked transfer" : "Under the pointer (click to pick)");
            sh::pStat(p, "Departure from " + planetName(C.trDep), dateOf(s.depJD0 + s.depStep * ci));
            sh::pStat(p, "Arrival at " + planetName(C.trArr), dateOf(s.arrJD0 + s.arrStep * cj));
            sh::pStat(p, "Time of flight", num(tof, 5) + " days");
            sh::pStat(p, "Departure C3 = v∞²", num(c3, 5) + " km²/s²  (v∞ " + num(std::sqrt(c3), 4) + " km/s)");
            sh::pStat(p, "Arrival v∞", num(vi, 5) + " km/s");
        } else sh::pCaption(p, "No transfer for this cell (arrival is not after departure, or the solver found none).");
    }
    {
        char b[300];
        std::snprintf(b, sizeof b, "transfer dep=%s arr=%s n=%d best_c3=%.6g best_i=%d best_j=%d show=%d gpu=%d cardstate=%d", planetName(C.trDep).c_str(), planetName(C.trArr).c_str(), G.nDep, C.trShow == 0 ? vmin : 0.0, bi, bj, C.trShow,
                      porkchopRunner().lastWasGpu() ? 1 : 0, porkchopRunner().state());
        logLine(b);
    }
    sh::pStat(p, "Last grid solved by", porkchopRunner().lastWasGpu() ? "the graphics card" : "the processor");
    sh::pCaption(p, porkchopRunner().pathText(app.computeOk));
    sh::pCaption(p, "Limits: Lambert's problem with zero revolutions (less than one turn about the Sun, in the planets' own direction), patched conics: only the Sun pulls during the transfer; each planet's gravity only sets the "
                    "departure C3 and the arrival v∞ (no capture or escape orbit, no flyby). Planets on the lab's own table of JPL approximate elements (Simple). C3 and v∞ are heliocentric velocity differences to the planet's own velocity.");
    sh::pCaption(p, "Source: D. Izzo, Revisiting Lambert's problem, Celestial Mechanics and Dynamical Astronomy 121 (2015) 1-15 (see About, Where the facts come from).");
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
double stepDays(int k) { static const double d[4] = {1, 7, 30, 365.25}; return d[std::max(0, std::min(3, k))]; }

void drawEphemeris(sh::Panel& p, AppState& app) {
    const sh::Frame& F = sh::F();
    if (C.ephDate[0] == 0) std::snprintf(C.ephDate, sizeof C.ephDate, "%s", orbit::ephem::jdUTToDate(app.sys.julianDay()).c_str());
    sh::pHeading(p, "Body");
    std::vector<std::string> names(kShort, kShort + 8);
    sh::pSegmented(p, "cm.eph.body", names, &C.ephPlanet);
    sh::pHeading(p, "Model");
    sh::pSegmented(p, "cm.eph.model", {"Simple", "Real, GR on", "Real, GR off"}, &C.ephModel);
    textField(p, "First date (YYYY-MM-DD, 12:00 UT)", "##cmephdate", C.ephDate, sizeof C.ephDate);
    sh::pHeading(p, "Step between dates");
    sh::pSegmented(p, "cm.eph.step", {"1 day", "7 days", "30 days", "1 year"}, &C.ephStep);
    sh::pStat(p, "Number of dates", num(std::round(C.ephCount), 3));
    sh::pSlider(p, "cm.eph.count", &C.ephCount, 1, 30);
    double jd0 = 0;
    const orbit::ephem::Model model = C.ephModel == 0 ? orbit::ephem::Model::Simple : C.ephModel == 1 ? orbit::ephem::Model::Real : orbit::ephem::Model::RealNoRelativity;
    if (!orbit::ephem::dateToJdUT(C.ephDate, jd0)) { sh::pText(p, Type::Body, false, sh::cDanger(), "That is not a date (write it as YYYY-MM-DD, for example 2026-11-27)."); return; }
    if (jd0 < 2378496.5 || jd0 > 2488069.5) { sh::pText(p, Type::Body, false, sh::cDanger(), "The lab's dates run from 1800-01-01 to 2100-12-31."); return; }
    const int n = std::max(1, std::min(30, int(std::lround(C.ephCount))));
    const std::string key = std::to_string(C.ephPlanet) + "/" + std::to_string(C.ephModel) + "/" + C.ephDate + "/" + std::to_string(C.ephStep) + "/" + std::to_string(n);
    if (key != C.ephKey && key != C.ephFutKey) {
        std::vector<double> jds;
        for (int k = 0; k < n; k++) jds.push_back(timescale::tdbFromUT(jd0 + stepDays(C.ephStep) * k));
        const int planet = C.ephPlanet;
        C.ephFutKey = key;
        C.ephFut = std::async(std::launch::async, [model, planet, jds] { orbit::ephem::Ephemeris E(model); return E.table(planet, jds); });
    }
    if (C.ephFut.valid() && C.ephFut.wait_for(std::chrono::seconds(0)) == std::future_status::ready) { C.ephRows = C.ephFut.get(); C.ephKey = C.ephFutKey; C.ephFutKey.clear(); }
    if (C.ephFut.valid()) sh::pCaption(p, "Working it out (the Real model walks the integrator from 2026-01-01)…");
    if (C.ephRows.empty()) return;
    {
        char b[400];
        std::snprintf(b, sizeof b, "ephem planet=%s model=%d rows=%zu first=%.6f dist0=%.9g lon0=%.6f lat0=%.6f distN=%.9g", planetName(C.ephPlanet).c_str(), C.ephModel, C.ephRows.size(), C.ephRows[0].jdTDB, C.ephRows[0].distAU, C.ephRows[0].lonDeg, C.ephRows[0].latDeg, C.ephRows.back().distAU);
        logLine(b);
    }
    // the rows
    const auto& m = F.m;
    const float lh = sh::lineH(Type::Caption);
    const ImVec2 o = sh::here(p);
    const float colx[5] = {0, 0.27f, 0.53f, 0.71f, 0.86f};
    const char* head[5] = {"Date (UT)", "Distance from the Sun", "Longitude", "Latitude", ""};
    for (int c = 0; c < 4; c++) sh::text(p.dl, Type::Caption, true, ImVec2(o.x + colx[c] * p.w, o.y), sh::cMuted(), head[c]);
    float y = o.y + lh + static_cast<float>(m.s4);
    for (std::size_t k = 0; k < C.ephRows.size(); k++) {
        const orbit::ephem::Row& r = C.ephRows[k];
        char b1[48], b2[24], b3[24];
        std::snprintf(b1, sizeof b1, "%.7f AU", r.distAU);
        std::snprintf(b2, sizeof b2, "%.4f°", r.lonDeg);
        std::snprintf(b3, sizeof b3, "%.4f°", r.latDeg);
        sh::text(p.dl, Type::Caption, false, ImVec2(o.x, y), sh::cText(), orbit::ephem::jdUTToDate(jd0 + stepDays(C.ephStep) * double(k)));
        sh::text(p.dl, Type::Caption, false, ImVec2(o.x + colx[1] * p.w, y), sh::cText(), b1);
        sh::text(p.dl, Type::Caption, false, ImVec2(o.x + colx[2] * p.w, y), sh::cText(), b2);
        sh::text(p.dl, Type::Caption, false, ImVec2(o.x + colx[3] * p.w, y), sh::cText(), b3);
        y += lh + static_cast<float>(m.s4);
    }
    p.y = (y - p.origin.y) + static_cast<float>(m.s4);
    if (sh::pButton(p, "cm.eph.csv", "Copy the table as CSV (with x, y, z in km)")) {
        std::string t = "date_UT,JD_TDB,x_km,y_km,z_km,distance_AU,longitude_deg,latitude_deg\n";
        for (std::size_t k = 0; k < C.ephRows.size(); k++) {
            const orbit::ephem::Row& r = C.ephRows[k];
            char b[256];
            std::snprintf(b, sizeof b, "%s,%.6f,%.3f,%.3f,%.3f,%.9f,%.6f,%.6f\n", orbit::ephem::jdUTToDate(jd0 + stepDays(C.ephStep) * double(k)).c_str(), r.jdTDB, r.pos.x / 1000, r.pos.y / 1000, r.pos.z / 1000, r.distAU, r.lonDeg, r.latDeg);
            t += b;
        }
        SDL_SetClipboardText(t.c_str());
    }
    noteItem("cm.eph.csv");
    sh::pStat(p, "Model", orbit::ephem::modelName(model));
    sh::pStat(p, std::string("Its worst error against JPL Horizons (1800-2100)"), commas(double(orbit::ephem::worstErrorKm(model, C.ephPlanet))) + " km");
    sh::pCaption(p, "Heliocentric places in the ecliptic and mean equinox of J2000. The date is 12:00 UT, turned into TDB with ΔT inside. Earth is the Earth-Moon barycentre (the body the model carries). "
                    "Simple: JPL's table of approximate elements (made for 1800 to 2050); Real: the Sun and the planets pull on each other, started from JPL Horizons (DE441) at 2026-01-01 12:00 TDB. "
                    "The worst errors are measured by the lab's own tests at the 301 Horizons dates (1 January of every year, 1800 to 2100); between those dates the error is not measured.");
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
void drawDrift(sh::Panel& p, AppState&) {
    sh::pHeading(p, "Integrator");
    sh::pSegmented(p, "cm.dr.int", {"Accurate step (the app's)", "Wisdom-Holman map"}, &C.drIntegrator);
    sh::pCheck(p, "cm.dr.gr", "Sun's relativity (1PN) on", &C.drGr);
    sh::pHeading(p, "Span (one-day steps from the lab's start, 2026-01-01)");
    sh::pSegmented(p, "cm.dr.span", {"10 years", "100 years", "1,000 years"}, &C.drSpan);
    const double years = C.drSpan == 0 ? 10 : C.drSpan == 1 ? 100 : 1000;
    const bool running = C.drFut.valid();
    if (running && C.drFut.wait_for(std::chrono::seconds(0)) == std::future_status::ready) { C.drRes = C.drFut.get(); C.drHave = true; }
    if (C.drFut.valid()) {
        const long done = C.drProgress.load();
        sh::pStat(p, "Running", commas(double(done)) + " of " + commas(double(C.drTotal)) + " steps");
        if (sh::pButton(p, "cm.dr.stop", "Stop")) C.drStop = true;
        noteItem("cm.dr.stop");
    } else {
        if (sh::pButton(p, "cm.dr.run", "Run the check", sh::Face::Primary)) {
            C.drStop = false; C.drProgress = 0;
            C.drTotal = long(years * 365.25);
            const auto integ = C.drIntegrator == 0 ? orbit::ephem::Integrator::Accurate : orbit::ephem::Integrator::WisdomHolman;
            const bool gr = C.drGr;
            C.drFut = std::async(std::launch::async, [integ, gr, years] { return orbit::ephem::driftCheck(integ, gr, years, &C.drProgress, &C.drStop); });
        }
        noteItem("cm.dr.run");
    }
    if (C.drHave) {
        const orbit::ephem::DriftResult& R = C.drRes;
        char b[300];
        std::snprintf(b, sizeof b, "drift integrator=%d gr=%d years=%.4f steps=%ld done=%d maxE=%.6e maxL=%.6e", int(R.integrator), R.relativity ? 1 : 0, R.years, R.steps, R.done ? 1 : 0, R.maxRelEnergy, R.maxRelAngular);
        logLine(b);
        sh::pHeading(p, std::string(R.done ? "Result: " : "Stopped early: ") + orbit::ephem::integratorName(R.integrator) + ", relativity " + (R.relativity ? "on" : "off") + ", " + num(R.years, 4) + " years");
        sh::pStat(p, "Total energy, largest |E/E₀ − 1|", num(R.maxRelEnergy, 4));
        sh::pStat(p, "Total energy, at the end E/E₀ − 1", num(R.endRelEnergy, 4));
        sh::pStat(p, "Angular momentum, largest |ΔL|/|L₀|", num(R.maxRelAngular, 4));
        sh::pStat(p, "Angular momentum, at the end", num(R.endRelAngular, 4));
        sh::pStat(p, "Steps", commas(double(R.steps)));
    }
    sh::pCaption(p, "The eight planets and the Sun as point masses from the lab's JPL start, a one-day step; energy, angular momentum and momentum are sampled every 50 steps. With relativity on the energy includes the 1PN potential energy. "
                    "The accurate step is the app's, without its Earth-Moon split (the Moon is moved by time, not by the planets, so energy alone is not conserved with the split). "
                    "The Wisdom-Holman map is the older step the lab's tests keep. The lab's own tests (tests/test_nbody.cpp) check the same numbers: energy 1e-11 (accurate step, 100 years) and 1e-8 (the map, 1,000 years), angular momentum 1e-12. "
                    "A small change is the integrator's own error; it says nothing about how well the model matches the real Solar System (that is the ephemeris table's errors).");
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------
const std::vector<std::pair<std::string, std::string>>& celestialTools() {
    static const std::vector<std::pair<std::string, std::string>> t = {
        {"cm:lagrange", "Lagrange points of a pair of bodies"},
        {"cm:kirkwood", "Kirkwood gaps in the asteroid belt"},
        {"cm:transfer", "Transfer planner (Lambert) and porkchop plot"},
        {"cm:ephem", "Ephemeris table of a planet"},
        {"cm:drift", "Integrator drift check (energy, angular momentum)"},
    };
    return t;
}
bool isCelestialTool(const std::string& id) { return id.rfind("cm:", 0) == 0; }
bool celestialFieldActive() { return C.fieldActive; }

void celestialDraw(sh::Panel& p, AppState& app, const std::string& id) {
    C.fieldActive = false;
    orbit::Overlay& O = app.set.overlay;
    if (id != "cm:lagrange" && O.lagrange) { O.lagrange = false; bumpOverlay(app); }
    if (id != "cm:transfer" && O.transfer) { O.transfer = false; bumpOverlay(app); }
    if (id == "cm:lagrange") drawLagrange(p, app);
    else if (id == "cm:kirkwood") drawKirkwood(p, app);
    else if (id == "cm:transfer") drawTransfer(p, app);
    else if (id == "cm:ephem") drawEphemeris(p, app);
    else if (id == "cm:drift") drawDrift(p, app);
}

void celestialLeave(AppState& app) {
    orbit::Overlay& O = app.set.overlay;
    if (O.lagrange || O.transfer) { O.lagrange = false; O.transfer = false; bumpOverlay(app); }
    C.drStop = true;
}

int celestialPorkchopMaxSide() { return 256; }
bool celestialBusy() { return porkchopRunner().busy(); }

namespace {
struct Kept { int dep, arr; float depOff, depSpan, arrOff, arrSpan, side; bool have; } g_kept{};
}
void celestialBenchSave() { g_kept = {C.trDep, C.trArr, C.depOff, C.depSpan, C.arrOff, C.arrSpan, C.side, true}; }
void celestialBenchRestore() {
    if (!g_kept.have) return;
    C.trDep = g_kept.dep; C.trArr = g_kept.arr; C.depOff = g_kept.depOff; C.depSpan = g_kept.depSpan; C.arrOff = g_kept.arrOff; C.arrSpan = g_kept.arrSpan; C.side = g_kept.side;
    C.have = false;                                                // the grid is made again from them when the panel next shows the planner
    g_kept.have = false;
}

std::string celestialPorkchopPathText(bool computeOk) {
    return porkchopRunner().pathText(computeOk) + "; the plot: the panel's draw list, which the graphics card draws (the grid's colours are made on the processor)";
}

long celestialBenchPorkchop(AppState& app, double phase) {
    // Earth to Mars, the largest grid; the departure and the arrival windows slide as a drag does (a triangle wave over 0 .. 1000 days)
    C.trDep = 2; C.trArr = 3;
    C.side = float(celestialPorkchopMaxSide());
    const double w = std::fabs(std::fmod(phase, 2.0));
    const double u = w < 1 ? w : 2 - w;
    C.depOff = float(-200 + 900 * u);
    C.depSpan = 300; C.arrOff = 120; C.arrSpan = 420;
    const auto sOld = C.in.s;
    const auto sNew = setupOf(app);
    const bool changed = !C.have || sOld.depJD0 != sNew.depJD0 || sOld.nDep != sNew.nDep || sOld.arrJD0 != sNew.arrJD0;
    if (!changed) return 0;
    C.have = true;
    remake(app);
    return long(sNew.nDep) * long(sNew.nArr);
}

}  // namespace ui
