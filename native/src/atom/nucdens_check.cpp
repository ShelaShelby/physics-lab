#include "atom/nucdens_check.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "app/gpu_guard.h"

namespace atom {

render::NucDensDraw nucDensDraw(const NucleusChargeDensity& d, const double e1[3], const double e2[3], const double e3[3], int G, double cellFm, std::vector<float>& table,
                                std::uint64_t version, double x0, double y0, double cellPts, const float col[4]) {
    render::NucDensDraw r;
    // the same numbers projectChargeDensity works out first (atom_core.cpp)
    const double n1 = d.n[0] * e1[0] + d.n[1] * e1[1] + d.n[2] * e1[2], n2 = d.n[0] * e2[0] + d.n[1] * e2[1] + d.n[2] * e2[2], n3 = d.n[0] * e3[0] + d.n[1] * e3[1] + d.n[2] * e3[2];
    const double half = 0.5 * G * cellFm;
    r.radial = d.radial ? 1 : 0; r.kk = d.kk != 0 ? 1 : 0;
    r.n1 = float(n1); r.n2 = float(n2); r.n3 = float(n3);
    r.G = G; r.halfFm = float(half); r.cellFm = float(cellFm);
    r.tableVersion = version;
    const bool fill = table.empty();                                 // (the caller keeps the table while the model, and its version, stay)
    if (d.radial) {
        r.nT = d.nR; r.hR = float(d.hR);
        if (fill)
            for (const std::vector<double>* t : {&d.I0, &d.K0, &d.K2, &d.I2})
                for (int i = 0; i < d.nR; i++) table.push_back(i < int(t->size()) ? float((*t)[std::size_t(i)]) : 0.f);
    } else {
        const int qn = 64;
        const double nperp = std::sqrt(n1 * n1 + n2 * n2), sx = nperp > 1e-9 ? n1 / nperp : 1, sy = nperp > 1e-9 ? n2 / nperp : 0;
        const double reach = half * std::sqrt(2.0), hq = reach / std::max(2, qn - 1);
        const double dl = (d.kind == 4 ? d.a : d.kind == 2 ? 0.5 * d.hoA : 1.0) * std::min(d.fx, d.fz);
        r.qn = qn; r.nT = d.nT; r.nperp = float(nperp); r.sx = float(sx); r.sy = float(sy); r.hq = float(hq); r.dl = float(dl);
        r.rE2 = float(d.rCut * d.rCut); r.invT = float(1 / d.hT); r.invU = float(1 / d.hU);
        if (fill) table = d.T;
    }
    r.table = table.data(); r.tableN = table.size();
    r.x0 = float(x0); r.y0 = float(y0); r.cellPts = float(cellPts);
    for (int i = 0; i < 4; i++) r.colour[i] = col[i];
    return r;
}

std::vector<NucDensCase> nucDensCheckCases() {
    std::vector<NucDensCase> v;
    const auto add = [&](const char* name, int Z, int N, int spin, double yaw, double pitch) {
        NucDensCase c;
        c.name = name;
        // the nucleus' axis tilted out of the picture's plane, and the view as atom_view.cpp's Orbit view (mode 0): x = (ca, -sa, 0), y = (sa sb, ca sb, cb), sight (sa cb, ca cb, -sb)
        c.d = nucleusChargeDensity(Z, N, 0.36, 0.48, 0.8, spin);
        const double ca = std::cos(yaw), sa = std::sin(yaw), cb = std::cos(pitch), sb = std::sin(pitch);
        const double e1[3] = {ca, -sa, 0}, e2[3] = {sa * sb, ca * sb, cb}, e3[3] = {sa * cb, ca * cb, -sb};
        for (int i = 0; i < 3; i++) { c.e1[i] = e1[i]; c.e2[i] = e2[i]; c.e3[i] = e3[i]; }
        c.G = 128;
        c.cellFm = std::max(c.d.ext997, 0.5) * 1.05 * 2 / c.G;
        v.push_back(c);
    };
    add("U-238 (deformed), seen at a slant", 92, 146, 0, 0.7, 0.4);
    add("Pb-208 (round)", 82, 126, 0, 0.3, 1.1);
    add("the deuteron, M = 0 (its L = 2 part)", 1, 1, 1, 0.5, 0.9);
    return v;
}

NucDensCompare compareNucDens(const std::vector<float>& cpu, const std::vector<float>& card) {
    NucDensCompare c;
    if (cpu.empty() || card.size() != cpu.size()) return c;
    for (float v : cpu) c.peak = std::max(c.peak, double(v));
    for (std::size_t i = 0; i < cpu.size(); i++) {
        const double d = std::fabs(double(card[i]) - double(cpu[i])) / std::max(c.peak, 1e-300);
        c.cells++;
        c.worst = std::max(c.worst, d);
        if (!(d <= kNucDensLimit)) c.over++;
    }
    return c;
}

namespace {
double nowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct Path {
    int state = 0;                 // 0 waiting for the Density look on screen, 2 its steps under way, 1 ready, -1 off
    int step = 0, frames = 0, gen = -1, usedFrames = 0;
    double stepAt = 0;
    bool firstUse = false, firstDone = false, reachedLogged = false, holdLogged = false;
    int jobs[3] = {0, 0, 0};
    std::vector<float> res[3];
    std::vector<float> tables[3];
    std::string why, check;
};
Path g_p;
const char* kPipe = "the nucleus density's pipeline";
const char* kCheck = "the nucleus density's check";
const char* kFirst = "the nucleus density's first full use";

void off(const std::string& why) {
    g_p.state = -1;
    g_p.why = why;
    std::printf("PLNUCDENSCARD state=-1 why=%s\n", why.c_str());
    std::fflush(stdout);
    app::gpuVkNucDensGuard().log("the card path is off: " + why);   // 0.68.1: gpu-paths-log.txt names the reason too (0.68.0 wrote it to stdout only: the owner's log was silent)
}

void advance(render::Renderer2D& R) {
    app::GpuGuard& G = app::gpuVkNucDensGuard();
    render::NucDensCard* C = R.nucDensCard();
    if (!C) { off("no card programs on this start"); return; }
    const double now = nowS();
    if (g_p.step == 0) {
        G.begin(kPipe);
        std::string why;
        const double t0 = nowS();
        if (!C->build(why)) { G.end(kPipe, "FAILED: " + why); G.finished(false, "its pipeline could not be made: " + why); off("its pipeline could not be made: " + why); return; }
        char b[120];
        std::snprintf(b, sizeof b, "kNucDensCS as SPIR-V, made after %.0f ms", 1000 * (nowS() - t0));
        G.end(kPipe, b);
        g_p.step = 1;
        return;
    }
    const std::vector<NucDensCase> cases = nucDensCheckCases();
    if (g_p.step == 1) {
        G.begin(kCheck);
        const float col[4] = {1, 1, 1, 1};
        for (int i = 0; i < 3; i++) {
            const NucDensCase& c = cases[std::size_t(i)];
            g_p.jobs[i] = C->beginCheck(nucDensDraw(c.d, c.e1, c.e2, c.e3, c.G, c.cellFm, g_p.tables[i], std::uint64_t(i + 1), 0, 0, 1, col));
            if (!g_p.jobs[i]) { for (int k = 0; k < i; k++) C->cancel(g_p.jobs[k]); G.end(kCheck, "could not be started"); G.finished(false, "the nucleus density's check could not be started"); off("the nucleus density's check could not be started"); return; }
        }
        g_p.stepAt = now; g_p.step = 2;
        return;
    }
    if (g_p.step == 2) {
        bool pending = false;
        for (int i = 0; i < 3; i++) {
            if (!g_p.jobs[i]) continue;
            const render::JobState st = C->pollCheck(g_p.jobs[i], g_p.res[i]);
            if (st == render::JobState::Pending) { pending = true; continue; }
            g_p.jobs[i] = 0;
            if (st == render::JobState::Failed) g_p.res[i].clear();
        }
        if (pending) {
            if (now - g_p.stepAt > 10) { for (int& j : g_p.jobs) if (j) { C->cancel(j); j = 0; } G.timedOut(kCheck); off("a step of the nucleus density on the card did not finish in time (the check)"); }
            return;
        }
        bool ok = true;
        std::string text;
        for (int i = 0; i < 3; i++) {
            const NucDensCase& c = cases[std::size_t(i)];
            std::vector<float> cpu;
            projectChargeDensity(c.d, c.e1, c.e2, c.e3, c.G, c.cellFm, cpu);
            const NucDensCompare k = compareNucDens(cpu, g_p.res[i]);
            ok = ok && k.ok();
            char b[200];
            std::snprintf(b, sizeof b, "%s%s: %ld of %ld cells over %.0e of the brightest, the largest %.1e", i ? "; " : "", c.name, k.over, k.cells, kNucDensLimit, k.worst);
            text += b;
            std::printf("PLNUCDENSCASE %d %s\n", i, b + (i ? 2 : 0));
        }
        for (std::vector<float>& r : g_p.res) { r.clear(); r.shrink_to_fit(); }
        for (std::vector<float>& t : g_p.tables) { t.clear(); t.shrink_to_fit(); }
        static const bool forced = std::getenv("PL_NUCDENS_CARDFAIL") != nullptr;   // (tests: as if the check had failed)
        if (forced) ok = false;
        char t[96];
        std::snprintf(t, sizeof t, "%s after %.0f ms: ", ok ? "passed" : "FAILED", 1000 * (now - g_p.stepAt));
        G.end(kCheck, t + text);
        g_p.check = text;
        if (!ok) { const std::string why = forced ? "the check was made to fail (test switch PL_NUCDENS_CARDFAIL)" : "its start-up check against the processor failed (" + text + ")"; G.finished(false, why); off(why); return; }
        g_p.state = 1;
        G.rest();
        std::printf("PLNUCDENSCARD state=1 check=%s\n", text.c_str());
        std::fflush(stdout);
    }
}
}  // namespace

bool drawNucDensOnCard(render::Renderer2D& R, const render::NucDensDraw& d) {
    if (!R.vulkan()) return false;                                   // (OpenGL: the processor's squares; no card program for them there)
    app::GpuGuard& G = app::gpuVkNucDensGuard();
    if (g_p.gen != G.generation()) {                                 // the first frame, and "Try again" (Settings, Performance)
        g_p.gen = G.generation();
        if (g_p.state == -1) { g_p = Path(); g_p.gen = G.generation(); }
    }
    if (g_p.state == 0) {
        if (g_p.frames == 0 && !g_p.reachedLogged) { g_p.reachedLogged = true; G.log("the Density look reached the card path (it waits 3 frames, then for its guard's turn)"); }
        if (!R.computeShaders()) off("this Vulkan start has no compute programs on the card (or they are switched off)");
        else if (G.started() && !G.allowed()) off(G.offText().empty() ? std::string("its guard switched it off") : G.offText());
        else if (++g_p.frames >= 3) {
            if (G.hold()) { g_p.state = 2; g_p.step = 0; }
            else if (!g_p.holdLogged) { g_p.holdLogged = true; G.log("hold() said no: its helper is not finished, or another guard's step is open (the turn)"); }
        }
    }
    if (g_p.state == 2) advance(R);
    else if (g_p.state == 1 && g_p.firstUse && !g_p.firstDone && ++g_p.usedFrames >= 4) {   // (3 frames drawn by the card, their batches long done)
        char b[96];
        std::snprintf(b, sizeof b, "drawn in %d frames after %.0f ms", g_p.usedFrames - 1, 1000 * (nowS() - g_p.stepAt));
        G.end(kFirst, b);
        G.finished(true);
        g_p.firstDone = true;
    }
    render::NucDensCard* C = R.nucDensCard();
    if (C) C->ready = g_p.state == 1 && (g_p.firstUse || G.hold());
    if (g_p.state != 1 || !C || !C->ready) return false;
    if (!g_p.firstUse) { G.begin(kFirst); g_p.firstUse = true; g_p.stepAt = nowS(); g_p.usedFrames = 0; }
    return R.nucDensity(d);
}

int nucDensCardState() { return g_p.state == 1 ? 1 : g_p.state == -1 ? -1 : 0; }

std::string nucDensCardPathText(bool vulkan) {
    if (!vulkan) return "the nucleus' Density look: the processor (no card program for it on OpenGL)";
    if (g_p.state == 1) return "the nucleus' Density look: the graphics card (Vulkan; its start-up check passed: " + g_p.check + ")";
    if (g_p.state == -1) return "the nucleus' Density look: the processor, because " + g_p.why;
    return "the nucleus' Density look: the graphics card once its start-up check has passed (made when the Density look is first shown)";
}

}  // namespace atom
