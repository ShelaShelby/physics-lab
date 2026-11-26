#include "molecule/surface_check.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "app/gpu_guard.h"
#include "molecule/molecule.h"
#include "molecule/vibration.h"

namespace molecule {

namespace {
void turn(double yaw, double pitch, double R[9]) {   // rows: the picture's right, up and towards the viewer
    const double cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
    const double r[9] = {cy, 0, sy, sy * sp, cp, -cy * sp, -sy * cp, sp, cy * cp};
    for (int i = 0; i < 9; i++) R[i] = r[i];
}
double radiusOf(const dens::Mesh& m) {
    double r = 0;
    for (std::size_t v = 0; v + 2 < m.p.size(); v += 3) r = std::max(r, std::sqrt(double(m.p[v]) * m.p[v] + double(m.p[v + 1]) * m.p[v + 1] + double(m.p[v + 2]) * m.p[v + 2]));
    return r;
}
const render::Color kPlain{0.80f, 0.84f, 0.90f, 1};
}  // namespace

std::vector<SurfaceCase> surfaceCheckCases() {
    std::vector<SurfaceCase> v;
    const double iso = kRealIso[2];
    const auto add = [&](const char* name, const char* id, int mode, double phase, double yaw, double pitch) {
        const dens::Data* d = dens::get(id);
        if (!d) return;
        SurfaceCase c;
        c.name = name;
        dens::Mesh still;
        dens::densitySurface(*d, iso, 2, still);
        c.potRange = std::max(1e-6, dens::potentialRange(still));
        if (mode >= 0) {
            const dens::VibDensityMode* vm = dens::vibDensity(id, mode);
            if (!vm) return;
            dens::Data at;
            dens::blendVib(*d, *vm, phase, at, false);
            dens::densitySurface(at, iso, 2, c.mesh);
        } else c.mesh = still;
        if (c.mesh.tri.empty()) return;
        turn(yaw, pitch, c.R);
        c.px = 0.3; c.py = -0.2;
        c.radius = radiusOf(c.mesh);
        v.push_back(std::move(c));
    };
    add("benzene (C6H6)", "C6H6", -1, 0, 0.6, 0.45);
    add("water (H2O)", "H2O", -1, 0, -0.9, 0.3);
    int mode = 0;
    while (mode < int(vib::modes("H2O").size()) && !dens::vibDensity("H2O", mode)) mode++;
    add("water vibrating (its first mode with a density, at 0.7 of the swing)", "H2O", mode, 0.7, 0.4, -0.5);
    return v;
}

render::SurfaceView surfaceView(const double R[9], double px, double py, double cx, double cy, double s, bool pot, double potRange, render::Color plain, float aFront, float aBack) {
    render::SurfaceView v;
    for (int i = 0; i < 9; i++) v.R[i] = static_cast<float>(R[i]);
    v.px = static_cast<float>(px); v.py = static_cast<float>(py); v.cx = static_cast<float>(cx); v.cy = static_cast<float>(cy); v.s = static_cast<float>(s);
    v.pot = pot ? 1 : 0; v.potRange = static_cast<float>(potRange);
    v.plain[0] = plain.r; v.plain[1] = plain.g; v.plain[2] = plain.b; v.plain[3] = plain.a;
    v.aFront = aFront; v.aBack = aBack;
    return v;
}

render::SurfaceDraw surfaceDrawOf(const SurfaceCase& c) {
    render::SurfaceDraw d;
    d.mesh.rev = 0; d.mesh.p = c.mesh.p.data(); d.mesh.nrm = c.mesh.n.data(); d.mesh.value = c.mesh.value.data(); d.mesh.tri = c.mesh.tri.data();
    d.mesh.nv = static_cast<int>(c.mesh.p.size() / 3); d.mesh.nt = static_cast<int>(c.mesh.tri.size() / 3);
    d.view = surfaceView(c.R, c.px, c.py, c.cx, c.cy, c.s, true, c.potRange, kPlain, 0.6f, 0.15f);
    return d;
}

SurfaceCompare compareSurface(const SurfaceCase& c, const std::vector<float>& verts, const std::vector<std::uint32_t>& order) {
    SurfaceCompare k;
    const std::size_t nv = c.mesh.p.size() / 3, nt = c.mesh.tri.size() / 3;
    if (verts.size() != 8 * nv || order.size() != nt || nt == 0) return k;
    SurfaceCpu o;
    surfacePlace(c.mesh, c.R, c.px, c.py, c.cx, c.cy, c.s, o);
    surfaceColours(c.mesh, c.R, true, c.potRange, kPlain, o);
    k.posLimit = 2e-6 * (c.s * (c.radius + std::hypot(c.px, c.py)) + std::fabs(c.cx) + std::fabs(c.cy));
    for (std::size_t v = 0; v < nv; v++) {
        const float* q = &verts[8 * v];
        const double e = std::max(std::fabs(double(q[0]) - o.sx[v]), std::fabs(double(q[1]) - o.sy[v]));
        k.posWorst = std::max(k.posWorst, e);
        if (!(e <= k.posLimit)) k.posOver++;
        const double ec = std::max({std::fabs(double(q[4]) - o.vc[v].r), std::fabs(double(q[5]) - o.vc[v].g), std::fabs(double(q[6]) - o.vc[v].b), std::fabs(double(q[7]) - o.vc[v].a)});
        k.colourWorst = std::max(k.colourWorst, ec);
        if (!(ec <= 1e-4)) k.colourOver++;
        k.vertices++;
    }
    k.dropLimit = 2 * 6e-6 * c.radius;
    std::vector<char> seen(nt, 0);
    double prev = -1e300;
    for (std::size_t i = 0; i < nt; i++) {
        const std::uint32_t t = order[i];
        if (t >= nt || seen[t]) { k.orderBad++; continue; }
        seen[t] = 1;
        const double z = o.tz[t];
        if (prev > z) { k.dropWorst = std::max(k.dropWorst, prev - z); if (prev - z > k.dropLimit) k.orderBad++; }
        prev = std::max(prev, z);
        k.triangles++;
    }
    return k;
}

std::string surfaceCompareText(const SurfaceCompare& k) {
    char b[260];
    std::snprintf(b, sizeof b, "%ld vertices, %ld triangles: %ld places over %.1e point (the largest %.1e), %ld colours over 1e-4 (the largest %.1e), %ld out of order (the largest fall %.1e bohr, limit %.1e)",
                  k.vertices, k.triangles, k.posOver, k.posLimit, k.posWorst, k.colourOver, k.colourWorst, k.orderBad, k.dropWorst, k.dropLimit);
    return b;
}

// ---- 0.90.0: the surface made on the card ----
render::SurfaceMake surfaceMakeOf(const dens::Data& d, const dens::VibDensityMode* vm, double s, double iso, std::uint64_t key) {
    render::SurfaceMake m;
    m.key = key;
    s = std::clamp(s, -1.0, 1.0);
    m.rho[0] = d.rho.v.data(); m.pot[0] = d.pot.v.data();
    m.rho[1] = vm ? vm->plus.rho.data() : m.rho[0]; m.rho[2] = vm ? vm->minus.rho.data() : m.rho[0];
    m.pot[1] = vm ? vm->plus.pot.data() : m.pot[0]; m.pot[2] = vm ? vm->minus.pot.data() : m.pot[0];
    m.n[0] = d.rho.nx; m.n[1] = d.rho.ny; m.n[2] = d.rho.nz;
    m.pn[0] = d.pot.nx; m.pn[1] = d.pot.ny; m.pn[2] = d.pot.nz;
    m.o[0] = static_cast<float>(d.rho.ox); m.o[1] = static_cast<float>(d.rho.oy); m.o[2] = static_cast<float>(d.rho.oz); m.h = static_cast<float>(d.rho.h);
    m.po[0] = static_cast<float>(d.pot.ox); m.po[1] = static_cast<float>(d.pot.oy); m.po[2] = static_cast<float>(d.pot.oz); m.ph = static_cast<float>(d.pot.h);
    m.stride = 2;
    m.iso = static_cast<float>(std::log(iso));
    m.a = vm ? static_cast<float>(s / 2) : 0.0f;          // (as blendVib: a = s / 2, b = s^2 / 2)
    m.b = vm ? static_cast<float>(s * s / 2) : 0.0f;
    return m;
}

std::vector<MadeCase> surfaceMadeCases(bool full) {
    std::vector<MadeCase> v;
    const double iso = kRealIso[2];
    const auto add = [&](const std::string& name, const char* id, bool vib, double phase) {
        const dens::Data* d = dens::get(id);
        if (!d) return;
        const dens::VibDensityMode* vm = nullptr;
        int mode = -1;
        if (vib) {
            for (int k = 0; k < int(vib::modes(id).size()) && !vm; k++) if ((vm = dens::vibDensity(id, k)) != nullptr) mode = k;
            if (!vm) return;
        }
        MadeCase c;
        c.name = name; c.id = id; c.mode = mode; c.phase = phase;
        if (vm) dens::blendVib(*d, *vm, phase, c.at, false); else c.at = *d;
        dens::densitySurface(c.at, iso, 2, c.mesh, &c.keys);
        if (c.mesh.tri.empty()) return;
        c.make = surfaceMakeOf(*d, vm, phase, iso, 1000 + v.size());
        v.push_back(std::move(c));
    };
    if (full) {
        for (const double s : {-1.0, -0.35, 0.4, 1.0}) { char b[96]; std::snprintf(b, sizeof b, "water (H2O) vibrating at %.2f of its swing", s); add(b, "H2O", true, s); }
        for (const double s : {-1.0, -0.35, 0.4, 1.0}) { char b[120]; std::snprintf(b, sizeof b, "carbon dioxide (CO2), still (no vibrating density; phase %.2f)", s); add(b, "CO2", false, s); }
        for (const double s : {-1.0, -0.35, 0.4, 1.0}) { char b[120]; std::snprintf(b, sizeof b, "nitrogen (N2), still (no vibrating density; phase %.2f)", s); add(b, "N2", false, s); }
    } else {
        add("water (H2O) vibrating at -1 of its swing", "H2O", true, -1.0);
        add("water (H2O) vibrating at 0.7 of its swing", "H2O", true, 0.7);
    }
    return v;
}

namespace {
double ulpOf(double x) { const double a = std::fabs(x); return a > 0 ? std::ldexp(1.0, std::ilogb(a) - 23) : std::ldexp(1.0, -149); }
}  // namespace

MadeCompare compareMade(const MadeCase& c, const render::SurfaceMade& g) {
    MadeCompare k;
    const double u = std::ldexp(1.0, -24);
    const dens::Grid& R = c.at.rho;
    const int s = c.make.stride, nx = (R.nx - 1) / s + 1, ny = (R.ny - 1) / s + 1, nz = (R.nz - 1) / s + 1;
    const std::size_t nc = std::size_t(nx) * ny * nz;
    const double isoD = std::log(kRealIso[2]), eIso = std::fabs(double(c.make.iso) - isoD);
    const double a = c.make.a, b = c.make.b;
    std::vector<float> f(nc);
    std::vector<double> e(nc);
    for (int kk = 0; kk < nz; kk++)
        for (int j = 0; j < ny; j++)
            for (int i = 0; i < nx; i++) {
                const std::size_t full = (std::size_t(kk) * s * R.ny + std::size_t(j) * s) * R.nx + std::size_t(i) * s, q = (std::size_t(kk) * ny + j) * nx + i;
                const double f0 = c.make.rho[0][full], fp = c.make.rho[1][full], fm = c.make.rho[2][full];
                const double M = std::fabs(f0) + std::fabs(a) * (std::fabs(fp) + std::fabs(fm)) + std::fabs(b) * (std::fabs(fp) + std::fabs(fm) + 2 * std::fabs(f0));
                const double rho = R.v[full];
                f[q] = std::log(std::max(R.v[full], 1e-30f));
                // the card's density lies within eRho of the processor's; both are then raised to 1e-30 (dens::densitySurface), so ln rho lies between these two
                const double eRho = 8 * u * M + u * rho, arg = std::max(rho, 1e-30);
                const double lnLo = std::log(std::max(rho - eRho, 1e-30)), lnHi = std::log(std::max(rho + eRho, 1e-30));
                const double eLn = std::max(double(f[q]) - lnLo, lnHi - double(f[q])) + (arg >= 0.5 && arg <= 2 ? std::ldexp(1.0, -21) : 3 * ulpOf(f[q])) + ulpOf(f[q]);
                e[q] = eLn;
                if (std::fabs(double(f[q]) - isoD) <= eLn + eIso) k.nearCorners++;
            }
    k.vertices = long(c.mesh.p.size() / 3); k.triangles = long(c.mesh.tri.size() / 3);
    k.cardVertices = g.nv; k.cardTriangles = g.nt;
    if (g.verts.size() != 8 * std::size_t(g.nv) || g.keys.size() != std::size_t(g.nv) || g.tri.size() != 3 * std::size_t(g.nt)) { k.missing = k.triangles; return k; }
    // the card's vertex of each key (the card numbers its vertices by key, in increasing order)
    std::vector<std::pair<long long, long>> ck(std::size_t(g.nv));
    for (long v = 0; v < g.nv; v++) ck[std::size_t(v)] = {g.keys[std::size_t(v)], v};
    std::sort(ck.begin(), ck.end());
    const auto cardOf = [&](long long key) -> long {
        auto it = std::lower_bound(ck.begin(), ck.end(), std::make_pair(key, -1L));
        return it != ck.end() && it->first == key ? it->second : -1;
    };
    const double H = R.h * s, A0 = dens::kA0Angstrom;
    const double org[3] = {R.ox, R.oy, R.oz};
    for (long v = 0; v < k.vertices; v++) {
        const long long key = c.keys[std::size_t(v)];
        const long gv = cardOf(key);
        if (gv < 0) { k.posOver++; continue; }
        const long long pnt = key / 8;
        const int dir = int(key % 8);
        const int ci[3] = {int(pnt % nx), int((pnt / nx) % ny), int(pnt / (static_cast<long long>(nx) * ny))};
        const int hi[3] = {ci[0] + (dir & 1), ci[1] + ((dir >> 1) & 1), ci[2] + (dir >> 2)};
        const std::size_t qlo = std::size_t(pnt), qhi = (std::size_t(hi[2]) * ny + hi[1]) * nx + hi[0];
        const double clo = f[qlo], chi = f[qhi], D = std::fabs(chi - clo);
        const double t = std::clamp((isoD - clo) / (chi - clo), 1e-4, 1 - 1e-4);
        const double dt = ((1 - t) * e[qlo] + t * e[qhi] + eIso) / D + 3 * u * t;
        for (int ax = 0; ax < 3; ax++) {
            const double x = c.mesh.p[3 * std::size_t(v) + ax] / A0;                                      // bohr
            const double step = ((dir >> ax) & 1) ? 1.0 : 0.0;
            const double bohr = H * dt * step + 4 * u * (std::fabs(org[ax]) + H * (ci[ax] + 1)) + u * std::fabs(org[ax]) + u * std::fabs(x);
            const double lim = A0 * bohr + 3 * u * std::fabs(x) * A0;
            const double diff = std::fabs(double(g.verts[8 * std::size_t(gv) + std::size_t(ax)]) - c.mesh.p[3 * std::size_t(v) + ax]);
            k.posWorst = std::max(k.posWorst, diff);
            k.limitWorst = std::max(k.limitWorst, lim); k.limitLeast = std::min(k.limitLeast, lim);
            if (!(diff <= lim)) { k.posOver++; break; }
        }
    }
    // the triangles as cycles of three keys, each started at its smallest key (the way round kept)
    using T3 = std::array<long long, 3>;
    const auto norm = [](T3 t) { while (!(t[0] <= t[1] && t[0] <= t[2])) t = {t[1], t[2], t[0]}; return t; };
    std::vector<T3> cpu(std::size_t(k.triangles)), card(std::size_t(g.nt));
    for (long t = 0; t < k.triangles; t++) cpu[std::size_t(t)] = norm({c.keys[std::size_t(c.mesh.tri[3 * t])], c.keys[std::size_t(c.mesh.tri[3 * t + 1])], c.keys[std::size_t(c.mesh.tri[3 * t + 2])]});
    for (long t = 0; t < g.nt; t++) {
        const std::uint32_t* q = &g.tri[3 * std::size_t(t)];
        const bool in = q[0] < std::uint32_t(g.nv) && q[1] < std::uint32_t(g.nv) && q[2] < std::uint32_t(g.nv);
        card[std::size_t(t)] = in ? norm({g.keys[q[0]], g.keys[q[1]], g.keys[q[2]]}) : T3{-1, -1, -1};
    }
    // the same order too: the card makes them cube by cube, tetrahedron by tetrahedron, as the processor does (so the depth sort's ties fall alike)
    if (cpu.size() == card.size()) for (std::size_t t = 0; t < cpu.size(); t++) k.outOfOrder += cpu[t] != card[t] ? 1 : 0;
    std::sort(cpu.begin(), cpu.end()); std::sort(card.begin(), card.end());
    std::vector<T3> d;
    std::set_difference(cpu.begin(), cpu.end(), card.begin(), card.end(), std::back_inserter(d)); k.missing = long(d.size()); d.clear();
    std::set_difference(card.begin(), card.end(), cpu.begin(), cpu.end(), std::back_inserter(d)); k.extra = long(d.size());
    return k;
}

std::string madeCompareText(const MadeCompare& k) {
    char b[360];
    std::snprintf(b, sizeof b, "%ld vertices and %ld triangles (the card: %ld and %ld): %ld triangles missing, %ld extra, %ld out of order, %ld places over their limits (the largest difference %.1e angstrom, limits %.1e to %.1e), %ld corners within rounding of the isovalue",
                  k.vertices, k.triangles, k.cardVertices, k.cardTriangles, k.missing, k.extra, k.outOfOrder, k.posOver, k.posWorst, k.limitLeast > 1e299 ? 0.0 : k.limitLeast, k.limitWorst, k.nearCorners);
    return b;
}

namespace {
double nowS() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct Path {
    int state = 0;                 // 0 waiting for the surface on screen, 2 its steps under way, 1 ready, -1 off
    int step = 0, frames = 0, gen = -1, usedFrames = 0;
    double stepAt = 0;
    bool firstUse = false, firstDone = false;
    int jobs[3] = {0, 0, 0};
    std::vector<float> verts[3];
    std::vector<std::uint32_t> order[3];
    std::string why, check;
    int make = 0;                  // 0.90.0: the surface made on the card: 1 its check passed, -1 off (makeWhy), 0 not decided
    int makeJobs[2] = {0, 0};
    render::SurfaceMade made[2];
    std::string makeWhy;
    bool asked = false;            // (0.68.0: asked this frame)
};
Path g_p;
const char* kPipe = "the surface's pipelines";
const char* kCheck = "the surface's check";
const char* kFirst = "the surface's first full use";

void off(const std::string& why) {
    g_p.state = -1;
    g_p.why = why;
    std::printf("PLSURFACECARD state=-1 why=%s\n", why.c_str());
    std::fflush(stdout);
}

void advance(render::Renderer2D& R) {
    app::GpuGuard& G = app::gpuVkSurfaceGuard();
    render::SurfaceCard* C = R.surfaceCard();
    if (!C) { off("no card programs on this start"); return; }
    const double now = nowS();
    if (g_p.step == 0) {
        G.begin(kPipe);
        std::string why;
        const double t0 = nowS();
        if (!C->build(why) || !R.surfaceProgram(why)) { G.end(kPipe, "FAILED: " + why); G.finished(false, "its pipelines could not be made: " + why); off("its pipelines could not be made: " + why); return; }
        std::string mw;
        if (!C->buildMake(mw)) { g_p.make = -1; g_p.makeWhy = "its pipeline could not be made: " + mw; }   // (0.90.0: the vibrating surface then made by the processor)
        char b[200];
        std::snprintf(b, sizeof b, "kSurfaceCS and kSurfaceVS as SPIR-V%s, made after %.0f ms", g_p.make < 0 ? " (kSurfaceMakeCS could not be made)" : ", kSurfaceMakeCS too", 1000 * (nowS() - t0));
        G.end(kPipe, b);
        g_p.step = 1;
        return;
    }
    static std::vector<SurfaceCase> cases;
    if (cases.empty()) cases = surfaceCheckCases();
    static std::vector<MadeCase> madeCases;                // 0.90.0
    if (madeCases.empty() && g_p.make == 0) madeCases = surfaceMadeCases(false);
    if (cases.size() != 3) { G.begin(kCheck); G.end(kCheck, "its cases could not be made"); G.finished(false, "the surface's check cases could not be made"); off("the surface's check cases could not be made (the density files)"); return; }
    if (g_p.step == 1) {
        G.begin(kCheck);
        for (int i = 0; i < 3; i++) {
            g_p.jobs[i] = C->beginCheck(surfaceDrawOf(cases[std::size_t(i)]));
            if (!g_p.jobs[i]) { for (int k = 0; k < i; k++) C->cancel(g_p.jobs[k]); G.end(kCheck, "could not be started"); G.finished(false, "the surface's check could not be started"); off("the surface's check could not be started"); return; }
        }
        if (g_p.make == 0) {                               // 0.90.0: the made surface's cases in the same batch
            if (madeCases.size() != 2) { g_p.make = -1; g_p.makeWhy = "its check cases could not be made (the density files)"; }
            for (int i = 0; i < 2 && g_p.make == 0; i++)
                if (!(g_p.makeJobs[i] = C->beginMake(madeCases[std::size_t(i)].make))) { g_p.make = -1; g_p.makeWhy = "its check could not be started"; }
        }
        g_p.stepAt = now; g_p.step = 2;
        return;
    }
    if (g_p.step == 2) {
        bool pending = false;
        for (int i = 0; i < 3; i++) {
            if (!g_p.jobs[i]) continue;
            const render::JobState st = C->pollCheck(g_p.jobs[i], g_p.verts[i], g_p.order[i]);
            if (st == render::JobState::Pending) { pending = true; continue; }
            g_p.jobs[i] = 0;
            if (st == render::JobState::Failed) { g_p.verts[i].clear(); g_p.order[i].clear(); }
        }
        for (int i = 0; i < 2; i++) {                      // 0.90.0
            if (!g_p.makeJobs[i]) continue;
            const render::JobState st = C->pollMake(g_p.makeJobs[i], g_p.made[i]);
            if (st == render::JobState::Pending) { pending = true; continue; }
            g_p.makeJobs[i] = 0;
            if (st == render::JobState::Failed) g_p.made[i] = render::SurfaceMade();
        }
        if (pending) {
            if (now - g_p.stepAt > 10) { for (int& j : g_p.jobs) if (j) { C->cancel(j); j = 0; } for (int& j : g_p.makeJobs) j = 0; G.timedOut(kCheck); off("a step of the surface on the card did not finish in time (the check)"); }
            return;
        }
        bool ok = true;
        std::string text;
        for (int i = 0; i < 3; i++) {
            const SurfaceCase& c = cases[std::size_t(i)];
            const SurfaceCompare k = compareSurface(c, g_p.verts[i], g_p.order[i]);
            ok = ok && k.ok();
            const std::string t = c.name + ": " + surfaceCompareText(k);
            text += (i ? "; " : "") + t;
            std::printf("PLSURFACECASE %d %s\n", i, t.c_str());
        }
        for (int i = 0; i < 3; i++) { g_p.verts[i].clear(); g_p.verts[i].shrink_to_fit(); g_p.order[i].clear(); g_p.order[i].shrink_to_fit(); }
        if (g_p.make == 0) {                               // 0.90.0: the made surface (its failure leaves the rest of the surface on the card)
            bool mk = true;
            for (int i = 0; i < 2; i++) {
                const MadeCase& c = madeCases[std::size_t(i)];
                const MadeCompare k = compareMade(c, g_p.made[i]);
                mk = mk && k.ok();
                const std::string t = "made on the card, " + c.name + ": " + madeCompareText(k);
                text += "; " + t;
                std::printf("PLSURFACECASE made%d %s\n", i, t.c_str());
                g_p.made[i] = render::SurfaceMade();
            }
            static const bool forcedMake = std::getenv("PL_SURFACEMAKE_FAIL") != nullptr;   // (tests: as if the made surface's check had failed)
            if (forcedMake) mk = false;
            g_p.make = mk ? 1 : -1;
            if (!mk) g_p.makeWhy = forcedMake ? "the check was made to fail (test switch PL_SURFACEMAKE_FAIL)" : "its start-up check against the processor failed";
        }
        static const bool forced = std::getenv("PL_SURFACE_CARDFAIL") != nullptr;   // (tests: as if the check had failed)
        if (forced) ok = false;
        char t[96];
        std::snprintf(t, sizeof t, "%s after %.0f ms: ", ok ? "passed" : "FAILED", 1000 * (now - g_p.stepAt));
        G.end(kCheck, t + text);
        g_p.check = text;
        if (!ok) { const std::string why = forced ? "the check was made to fail (test switch PL_SURFACE_CARDFAIL)" : "its start-up check against the processor failed (" + text + ")"; G.finished(false, why); off(why); return; }
        g_p.state = 1;
        G.rest();
        std::printf("PLSURFACECARD state=1 check=%s\n", text.c_str());
        std::fflush(stdout);
    }
}
}  // namespace

bool surfaceOnCard(render::Renderer2D& R) {
    g_p.asked = true;
    if (!R.vulkan()) return false;                   // (no Vulkan renderer, as in the unit tests: the CPU's triangles)
    static const bool never = std::getenv("PL_NO_SURFACE_CARD") != nullptr;   // (tests: the processor's triangles on Vulkan, to compare)
    if (never) return false;
    app::GpuGuard& G = app::gpuVkSurfaceGuard();
    if (g_p.gen != G.generation()) {                 // the first frame, and "Try again" (Settings, Performance)
        g_p.gen = G.generation();
        if (g_p.state == -1) { g_p = Path(); g_p.gen = G.generation(); }
    }
    if (g_p.state == 0) {
        if (!R.computeShaders()) off("this Vulkan start has no compute programs on the card (or they are switched off)");
        else if (G.started() && !G.allowed()) off(G.offText().empty() ? std::string("its guard switched it off") : G.offText());
        else if (++g_p.frames >= 3 && G.hold()) { g_p.state = 2; g_p.step = 0; }
    }
    if (g_p.state == 2) advance(R);
    else if (g_p.state == 1 && g_p.firstUse && !g_p.firstDone && ++g_p.usedFrames >= 4) {   // (3 frames drawn by the card, their batches long done)
        char b[96];
        std::snprintf(b, sizeof b, "drawn in %d frames after %.0f ms", g_p.usedFrames - 1, 1000 * (nowS() - g_p.stepAt));
        G.end(kFirst, b);
        G.finished(true);
        g_p.firstDone = true;
    }
    render::SurfaceCard* C = R.surfaceCard();
    if (C) C->ready = g_p.state == 1 && (g_p.firstUse || G.hold());
    if (g_p.state != 1 || !C || !C->ready) return false;
    if (!g_p.firstUse) { G.begin(kFirst); g_p.firstUse = true; g_p.stepAt = nowS(); g_p.usedFrames = 0; }
    return true;
}

int surfaceCardState() { return g_p.state == 1 ? 1 : g_p.state == -1 ? -1 : 0; }

bool surfaceMakeOnCard(render::Renderer2D& R) {   // 0.90.0 (after surfaceOnCard said yes this frame)
    static const bool never = std::getenv("PL_NO_SURFACE_MAKE") != nullptr;   // (tests: the processor's vibrating surface, drawn by the card, to compare)
    render::SurfaceCard* C = R.surfaceCard();
    if (C) C->makeReady = !never && g_p.state == 1 && g_p.make == 1 && C->ready && C->madeBuilt();
    return C && C->makeReady;
}

std::string surfaceCardPathText(bool vulkan) {
    if (!vulkan) return "the surface: the processor (no Vulkan renderer)";
    if (std::getenv("PL_NO_SURFACE_CARD")) return "the surface: the processor (PL_NO_SURFACE_CARD)";
    if (g_p.state == 1) return "the surface: the graphics card (Vulkan; turned, coloured, sorted and drawn there, sent once for each new surface; " +
                               std::string(g_p.make == 1 ? "a vibrating surface made there for each new phase" : "a vibrating surface made by the processor, because " + (g_p.makeWhy.empty() ? std::string("its check has not finished") : g_p.makeWhy)) +
                               "; its start-up check passed: " + g_p.check + ")";
    if (g_p.state == -1) return "the surface: the processor, because " + g_p.why;
    return "the surface: the graphics card once its start-up check has passed (made when the surface is first shown)";
}


// 0.68.0: once a frame (main.cpp): a frame that did not ask (its lab not shown) still runs the check's steps, and a first full use left unfinished is
// ended and begun again when it is shown, so its guard never keeps the turn while another lab's card programs wait for it
void surfaceFrameEnd(render::Renderer2D& R) {
    const bool asked = g_p.asked;
    g_p.asked = false;
    if (asked || !R.vulkan()) return;
    if (g_p.state == 2) { advance(R); return; }
    if (g_p.state == 1 && g_p.firstUse && !g_p.firstDone) {
        app::GpuGuard& G = app::gpuVkSurfaceGuard();
        G.end(kFirst, "not finished: its lab was left before it was drawn 3 times (begun again when it is shown)");
        G.rest();
        g_p.firstUse = false; g_p.usedFrames = 0;
        if (auto* C = R.surfaceCard()) C->ready = false;
    }
}

}  // namespace molecule
