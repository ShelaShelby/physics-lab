// 0.13 stage 1: the pictures of the old and the new phase-colour path, drawn by the REAL renderer (src/render/renderer2d.cpp and its shaders) on Mesa's software OpenGL
// (llvmpipe, through EGL, no window and no graphics card), and compared pixel by pixel. Built only with -DPL_GL_TESTS=ON (needs libEGL and Mesa's llvmpipe).
//
// What is compared: the dots layer of the cloud (the dots of the benchmark's scenes 8, 9 and 10, built by the very functions the app uses: cloud_dots.h), drawn
// (a) the 0.12 way, every dot carrying its colour (GPU colours Off), and (b) the 0.13 way, every dot carrying its colour index and the vertex shader picking the colour
// (GPU colours On). Also: the 24 wheel colours and the two Sign colours as flat squares, and soft (Glow) dots, on a dark and a light page.
// What is NOT in these pictures: the axes, the legend ring, the outline and the text (they are drawn the same way in both paths).
// Not a speed test: llvmpipe is the CPU pretending to be a graphics card.
//
// Usage: gl_compare_tests [output folder]   (writes cpu_*.png, gpu_*.png, diff_*.png, side_*.png there). Exit 0 if every comparison is within its limit.
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <thread>
#include <chrono>
#include <string>
#include <vector>

#include "atom/atom_bench.h"
#include "atom/cloud_dots.h"
#include "atom/cloud_light.h"
#include "atom/outline.h"
#include "render/group_compute.h"
#include "render/outline_compute.h"
#include "render/renderer2d.h"

// ---- the GL functions, from EGL (the app gets them from SDL: render/gl_loader.cpp) ----
namespace plgl {
#define PL_GL_DEFINE(ret, name, args) PFN_##name name = nullptr;
PL_GL_FUNCTIONS(PL_GL_DEFINE)
PL_GL43_FUNCTIONS(PL_GL_DEFINE)
#undef PL_GL_DEFINE
bool loadGL43(const char** missing) {
#define PL_GL_LOAD(ret, name, args)                                                  \
    name = reinterpret_cast<PFN_##name>(eglGetProcAddress(#name));                    \
    if (!name) { if (missing) *missing = #name; return false; }
    PL_GL43_FUNCTIONS(PL_GL_LOAD)
#undef PL_GL_LOAD
    return true;
}
bool loadGL(const char** missing) {
#define PL_GL_LOAD(ret, name, args)                                                  \
    name = reinterpret_cast<PFN_##name>(eglGetProcAddress(#name));                    \
    if (!name) { if (missing) *missing = #name; return false; }
    PL_GL_FUNCTIONS(PL_GL_LOAD)
#undef PL_GL_LOAD
    return true;
}
}  // namespace plgl

using namespace atom;
using render::Color;
using Blob = render::Renderer2D::Blob;

static int g_checks = 0, g_failed = 0;
static void check(const std::string& what, bool ok) {
    g_checks++;
    if (!ok) { g_failed++; std::printf("FAIL: %s\n", what.c_str()); }
}

// ---- a PNG writer (stored deflate blocks: big, but no zlib needed) ----
static std::uint32_t crc32(const unsigned char* p, std::size_t n, std::uint32_t c = 0) {
    static std::uint32_t table[256];
    static bool done = false;
    if (!done) { for (std::uint32_t i = 0; i < 256; i++) { std::uint32_t v = i; for (int k = 0; k < 8; k++) v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1; table[i] = v; } done = true; }
    c = ~c;
    for (std::size_t i = 0; i < n; i++) c = table[(c ^ p[i]) & 255] ^ (c >> 8);
    return ~c;
}
static void put32(std::vector<unsigned char>& v, std::uint32_t x) { for (int i = 3; i >= 0; i--) v.push_back((unsigned char)(x >> (8 * i))); }
static void chunk(std::vector<unsigned char>& out, const char* type, const std::vector<unsigned char>& data) {
    put32(out, std::uint32_t(data.size()));
    std::vector<unsigned char> t(type, type + 4); t.insert(t.end(), data.begin(), data.end());
    out.insert(out.end(), t.begin(), t.end());
    put32(out, crc32(t.data(), t.size()));
}
static void writePng(const std::string& path, int w, int h, const std::vector<unsigned char>& rgba) {   // rgba: top row first
    std::vector<unsigned char> raw;
    for (int y = 0; y < h; y++) { raw.push_back(0); for (int x = 0; x < w; x++) for (int c = 0; c < 3; c++) raw.push_back(rgba[std::size_t(y) * w * 4 + std::size_t(x) * 4 + c]); }
    std::vector<unsigned char> z = {0x78, 0x01};
    std::uint32_t a = 1, b = 0;
    for (unsigned char ch : raw) { a = (a + ch) % 65521; b = (b + a) % 65521; }
    for (std::size_t i = 0; i < raw.size(); i += 65535) {
        const std::size_t n = std::min<std::size_t>(65535, raw.size() - i);
        z.push_back(i + n >= raw.size() ? 1 : 0); z.push_back(n & 255); z.push_back(n >> 8); z.push_back(~n & 255); z.push_back((~n >> 8) & 255);
        z.insert(z.end(), raw.begin() + long(i), raw.begin() + long(i + n));
    }
    put32(z, (b << 16) | a);
    std::vector<unsigned char> png = {0x89, 'P', 'N', 'G', 13, 10, 26, 10}, ihdr;
    put32(ihdr, w); put32(ihdr, h); ihdr.push_back(8); ihdr.push_back(2); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    chunk(png, "IHDR", ihdr); chunk(png, "IDAT", z); chunk(png, "IEND", {});
    if (FILE* f = std::fopen(path.c_str(), "wb")) { std::fwrite(png.data(), 1, png.size(), f); std::fclose(f); }
}

// ---- an offscreen picture ----
struct Picture {
    int w, h;
    std::vector<unsigned char> px;      // RGBA, top row first
};
static GLuint g_fbo = 0, g_tex = 0;
static int g_fw = 0, g_fh = 0;

static Picture draw1(render::Renderer2D& R, int w, int h, Color bg, const std::function<void()>& draw) {
    using namespace plgl;
    if (!g_fbo || g_fw != w || g_fh != h) {
        if (!g_fbo) { glGenFramebuffers(1, &g_fbo); glGenTextures(1, &g_tex); }
        g_fw = w; g_fh = h;
        glBindTexture(GL_TEXTURE_2D, g_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_tex, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    R.begin(w, h);
    R.rect(0, 0, w, h, bg);
    draw();
    glEnable(GL_BLEND);
    R.end(w, h, 0, g_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_fbo);
    Picture s{w, h, std::vector<unsigned char>(std::size_t(w) * h * 4)};
    std::vector<unsigned char> bottomUp(s.px.size());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, bottomUp.data());
    for (int y = 0; y < h; y++) std::memcpy(&s.px[std::size_t(y) * w * 4], &bottomUp[std::size_t(h - 1 - y) * w * 4], std::size_t(w) * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return s;
}

struct Diff { int maxLevel = 0; long differing = 0, total = 0; double meanAbs = 0; };
static Diff compare(const Picture& a, const Picture& b) {
    Diff d; double sum = 0;
    for (std::size_t i = 0; i < a.px.size(); i += 4) {
        int m = 0;
        for (int c = 0; c < 3; c++) m = std::max(m, std::abs(int(a.px[i + c]) - int(b.px[i + c])));
        d.maxLevel = std::max(d.maxLevel, m); d.differing += m > 0; sum += m; d.total++;
    }
    d.meanAbs = sum / double(d.total);
    return d;
}
static void save(const std::string& dir, const std::string& name, const Picture& cpu, const Picture& gpu) {
    if (dir.empty()) return;
    writePng(dir + "/cpu_" + name + ".png", cpu.w, cpu.h, cpu.px);
    writePng(dir + "/gpu_" + name + ".png", gpu.w, gpu.h, gpu.px);
    std::vector<unsigned char> df(cpu.px.size()), both(std::size_t(cpu.w) * 2 * cpu.h * 4);     // diff: 64 times the difference, so one level shows; side: CPU path left, GPU path right
    for (std::size_t i = 0; i < cpu.px.size(); i += 4) { for (int c = 0; c < 3; c++) df[i + c] = (unsigned char)std::min(255, 64 * std::abs(int(cpu.px[i + c]) - int(gpu.px[i + c]))); df[i + 3] = 255; }
    for (int y = 0; y < cpu.h; y++) {
        std::memcpy(&both[std::size_t(y) * cpu.w * 8], &cpu.px[std::size_t(y) * cpu.w * 4], std::size_t(cpu.w) * 4);
        std::memcpy(&both[std::size_t(y) * cpu.w * 8 + std::size_t(cpu.w) * 4], &gpu.px[std::size_t(y) * cpu.w * 4], std::size_t(cpu.w) * 4);
    }
    writePng(dir + "/diff_" + name + ".png", cpu.w, cpu.h, df);
    writePng(dir + "/side_" + name + ".png", cpu.w * 2, cpu.h, both);
}
static void report(const std::string& dir, const std::string& name, const Picture& cpu, const Picture& gpu, int limit) {
    const Diff d = compare(cpu, gpu);
    std::printf("%-46s largest channel difference %d of 255, %ld of %ld pixels differ (%.4f %%), mean %.5f levels\n", name.c_str(), d.maxLevel, d.differing, d.total, 100.0 * double(d.differing) / double(d.total), d.meanAbs);
    check(name + ": the two pictures differ by at most " + std::to_string(limit) + " level(s) in any channel", d.maxLevel <= limit);
    save(dir, name, cpu, gpu);
}

// ---- the pictures ----
static const Color kPlus = render::hexColor("#6c9cff"), kMinus = render::hexColor("#f08a3c");           // the dark theme's accent and the Sign − colour, as the app

static void drawSwatches(render::Renderer2D& R, const DotColours& dc) {                                      // each group's colour as a big flat square (dot kind, alpha 1)
    std::vector<Blob> B;
    for (int g = 0; g < dc.n; g++) B.push_back(squareDot(dc, 20 + 40.0 * (g % 12), 20 + 40.0 * (g / 12), 18, g, 1.0));
    R.blobsPalette(B.data(), int(B.size()), false, dc.palette());
}

static void swatches(render::Renderer2D& R, const std::string& dir) {
    for (bool light : {false, true})
        for (bool wheel : {true, false}) {
            const Color bg = light ? Color{1, 1, 1, 1} : render::hexColor("#1e1e21");
            const DotColours cpu = makeDotColours(false, wheel, light, kPlus, kMinus), gpu = makeDotColours(true, wheel, light, kPlus, kMinus);
            const Picture a = draw1(R, 500, 80, bg, [&] { drawSwatches(R, cpu); }), b = draw1(R, 500, 80, bg, [&] { drawSwatches(R, gpu); });
            // 8-bit rounding: the colour is the same number to about 1e-6 (float against double); where it falls on x.5 of a level the two can round apart by one level
            report(dir, std::string("swatches ") + (wheel ? "Wheel" : "Sign") + (light ? " light page" : " dark page"), a, b, 1);
        }
}

// the scene's cloud, the way atom_view builds it, in the benchmark's own camera
struct SceneCloud { AtomLab L; std::vector<unsigned char> grp; std::vector<float> PX, PY, PD; CloudCamera cam; double w = 1040, h = 713; long cnt = 0; };
static void setupScene(SceneCloud& S, int which, const char* fAx = nullptr) {
    benchSetup(S.L, which, S.w, S.h);
    if (fAx) { S.L.setParam("fAx", std::string(fAx)); S.L.resetView(); for (int i = 0; i < 40000 && S.L.s.k < S.L.cloudCap(); i++) S.L.step(1.0 / 240); }   // a complex 4f: the phase takes every value, so all 24 colours show
    S.cnt = std::min<long>(S.L.s.k, S.L.cloudCap());
    S.grp.resize(std::size_t(S.cnt));
    plainGroups(S.L.usesWheel(), S.L.s.ph.data(), S.L.s.sg.data(), S.cnt, S.grp.data());
    const double m = std::min(S.w, S.h), pxm = S.L.pxPerM(m, S.L.s.szCur) * std::pow(10, S.L.p.zoom), ext = kExt[S.L.sizeN()] / S.L.p.Z * kA0;
    S.cam = {S.w / 2, S.h / 2, pxm, std::cos(S.L.s.yaw), std::sin(S.L.s.yaw), std::cos(S.L.s.pitch), std::sin(S.L.s.pitch), ext};
    projectCloud(S.L.s.pts.data(), S.cnt, S.cam, S.w, S.h, S.PX, S.PY, S.PD);
}
static void drawScene3D(render::Renderer2D& R, SceneCloud& S, const DotColours& dc) {
    std::vector<Blob> B;
    buildClassic3D(S.PX.data(), S.PY.data(), S.PD.data(), S.grp.data(), S.cnt, dc, cloudDotWeight(S.L.cloudCap()), B);
    R.blobsPalette(B.data(), int(B.size()), false, dc.palette());
}
static void drawScene2D(render::Renderer2D& R, SceneCloud& S, const DotColours& dc) {
    std::vector<SideDot> side;
    sideDots(S.L.s.pts.data(), S.cnt, S.grp.data(), dc.n, S.cam.cx, S.cam.cy, S.cam.pxm, S.w, S.h, dc.gpu, side);
    std::vector<Blob> B;
    for (const SideDot& d : side) B.push_back(squareDot(dc, d.x, d.y, 1, d.g, 0.3 * cloudDotWeight(S.L.cloudCap())));
    R.blobsPalette(B.data(), int(B.size()), false, dc.palette());
}

static void scenes(render::Renderer2D& R, const std::string& dir) {
    struct Sc { int which; const char* name; bool d3; const char* fAx; };
    const Sc list[] = {{5, "scene 8 Cloud 3D Wheel", true, nullptr}, {6, "scene 9 Cloud 3D Sign", true, nullptr}, {7, "scene 10 Cloud 2D Wheel", false, nullptr},
                      {5, "complex 4f m+3 Cloud 3D Wheel", true, "m+3"}, {7, "complex 4f m+3 Cloud 2D Wheel", false, "m+3"}};
    for (const Sc& sc : list)
        for (bool light : {false, true}) {
            static SceneCloud S;                       // (big: kept)
            setupScene(S, sc.which, sc.fAx);
            const bool wheel = S.L.usesWheel();
            check(std::string(sc.name) + ": the scene has 48,000 dots", S.cnt == 48000);
            if (sc.fAx) { int u[kPhaseBins] = {}; for (unsigned char g : S.grp) u[std::min<int>(g, kPhaseBins - 1)]++; int k = 0; for (int x : u) k += x > 0; check(std::string(sc.name) + ": uses at least 20 of the 24 colours", k >= 20); }
            const Color bg = light ? Color{1, 1, 1, 1} : render::hexColor("#1e1e21");
            const DotColours cpu = makeDotColours(false, wheel, light, light ? render::hexColor("#2f6fed") : kPlus, kMinus), gpu = makeDotColours(true, wheel, light, light ? render::hexColor("#2f6fed") : kPlus, kMinus);
            const Picture a = draw1(R, 1040, 713, bg, [&] { if (sc.d3) drawScene3D(R, S, cpu); else drawScene2D(R, S, cpu); });
            const Picture b = draw1(R, 1040, 713, bg, [&] { if (sc.d3) drawScene3D(R, S, gpu); else drawScene2D(R, S, gpu); });
            report(dir, std::string(sc.name) + (light ? " light page" : " dark page"), a, b, 1);
            // the picture is not empty, and uses many colours (the wheel)
            long lit = 0; for (std::size_t i = 0; i < a.px.size(); i += 4) lit += a.px[i] != a.px[0] || a.px[i + 1] != a.px[1] || a.px[i + 2] != a.px[2];
            check(std::string(sc.name) + (light ? " light page" : " dark page") + ": the picture shows the cloud (more than 5,000 pixels differ from the page)", lit > 5000);
        }
}

static void glowDots(render::Renderer2D& R, const std::string& dir) {                                         // soft dots (Glow style): additive on a dark page, alpha-blended on a light one
    for (bool light : {false, true}) {
        std::vector<float> xy; std::vector<unsigned char> g;
        std::uint32_t s = 12345;
        auto rnd = [&] { s = s * 1664525u + 1013904223u; return double(s >> 8) / 16777216.0; };
        for (int i = 0; i < 6000; i++) { xy.push_back(float(60 + 380 * rnd())); xy.push_back(float(60 + 280 * rnd())); g.push_back((unsigned char)(rnd() * kPhaseBins)); }
        const Color bg = light ? Color{1, 1, 1, 1} : render::hexColor("#1e1e21");
        Picture shots[2];
        for (int gpu = 0; gpu < 2; gpu++) {
            const DotColours dc = makeDotColours(gpu == 1, true, light, kPlus, kMinus);
            shots[gpu] = draw1(R, 500, 400, bg, [&] {
                std::vector<Blob> B;
                for (std::size_t i = 0; i < g.size(); i++) B.push_back(softDot(dc, xy[2 * i], xy[2 * i + 1], 4.6, g[i], light ? 0.5 : 0.12));
                R.blobsPalette(B.data(), int(B.size()), !light, dc.palette());
            });
        }
        report(dir, std::string("Glow soft dots Wheel ") + (light ? "light page" : "dark page (additive)"), shots[0], shots[1], 1);
    }
}


// ================= 0.13 stage 2: the superposition and the jump on the GPU (compute shaders, OpenGL 4.3) =================
// The CPU functions (AtomLab::superGroup, AtomLab::jumpGroups) are the reference. Every dot's shown / hidden decision and its colour group come from the GPU in 32-bit floats; they may
// differ from the 64-bit CPU numbers ONLY for a dot that is within float rounding of a threshold (its thinning odds against its fixed random number, or a colour-bin edge). Each differing
// dot is checked against a bound worked out here from the dot's own numbers; any other difference is a failure.
static const double kEps = 5.9604645e-8;      // kEps = 2^-24, the rounding of one 32-bit float operation (relative)
static double splitUniform(std::uint64_t z) {                             // the lab's splitmix64 finish as a double in [0, 1) (atom_sim.cpp: jumpUniform and superShown)
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL; z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL; z ^= z >> 31;
    return double(z >> 11) * (1.0 / 9007199254740992.0);
}
static double binEdgeDistance(double arg) {                               // radians from a phase to the nearest edge between two of the 24 bins (the edges are at (k + 1/2) 2 pi / 24)
    const double w = 2 * kPi / kPhaseBins;
    double t = std::fmod(arg - 0.5 * w, w); if (t < 0) t += w;
    return std::min(t, w - t);
}
struct GroupDiff { long dots = 0, shownDiff = 0, binDiff = 0, bad = 0; double worstRatio = 0; };
static void addDiff(GroupDiff& d, const GroupDiff& o) { d.dots += o.dots; d.shownDiff += o.shownDiff; d.binDiff += o.binDiff; d.bad += o.bad; d.worstRatio = std::max(d.worstRatio, o.worstRatio); }

// one superposition frame: the GPU groups against lab.superGroup, every difference accounted for
static GroupDiff compareSuper(AtomLab& L, render::IGroupCompute& gc, double theta, const std::string& what) {
    GroupDiff d;
    const long cnt = std::min<long>(L.s.k, L.cloudCap());
    std::vector<unsigned char> gpu(static_cast<std::size_t>(cnt));
    const bool ok = L.superGroupsGpu(gc, cnt, theta, gpu.data());
    check(what + ": the GPU kernel ran", ok);
    if (!ok) return d;
    const Superposition su = L.superState();
    const double a = su.a, b = su.b;
    const bool overlap = overlapAB(su.idA, su.idB) != 0.0;
    d.dots = cnt;
    for (long i = 0; i < cnt; i++) {
        const int cpu = L.superGroup(i, theta), g = gpu[std::size_t(i)];
        if (cpu == g) continue;
        const std::complex<double> A(L.s.sa[std::size_t(4 * i)], L.s.sa[std::size_t(4 * i + 1)]), B(L.s.sa[std::size_t(4 * i + 2)], L.s.sa[std::size_t(4 * i + 3)]);
        const std::complex<double> c = a * A + b * B * std::exp(std::complex<double>(0, -theta));
        const double S = a * std::abs(A) + b * std::abs(B), cm = std::max(std::abs(c), 1e-300);
        if ((cpu == 255) != (g == 255)) {                                  // shown on one side only: the dot's odds are within rounding of its random number
            d.shownDiff++;
            double w, u;
            if (overlap) { w = std::norm(a * A + b * B); u = 0; }
            else { const double q = 0.5 * (std::norm(A) + std::norm(B)); w = std::norm(c) / q; u = splitUniform(std::uint64_t(i) * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL); }
            const double thr = overlap ? 0.0 : w / 2, tol = overlap ? 1e-30 : thr * 1.5e-6 * (S / cm + 1) + 3.1e-8;
            const double ratio = std::fabs(u - thr) / tol;
            d.worstRatio = std::max(d.worstRatio, ratio);
            if (!(ratio <= 1)) { d.bad++; if (d.bad < 4) std::printf("  dot %ld: CPU %d GPU %d, u %.9g, w/2 %.9g, tolerance %.3g\n", i, cpu, g, u, thr, tol); }
        } else {                                                           // shown in both, in different colour bins: the phase is within rounding of a bin edge
            d.binDiff++;
            const double ph = std::arg(c), tol = 4 * 3 * kEps * S / cm + 2e-6, ratio = binEdgeDistance(ph) / tol;
            d.worstRatio = std::max(d.worstRatio, ratio);
            if (!(ratio <= 1)) { d.bad++; if (d.bad < 4) std::printf("  dot %ld: CPU bin %d GPU bin %d, phase %.9g, distance to a bin edge %.3g, tolerance %.3g\n", i, cpu, g, ph, binEdgeDistance(ph), tol); }
        }
    }
    return d;
}

// one jump frame: the same for the two populations of dots of a cloud jump
static GroupDiff compareJump(AtomLab& L, render::IGroupCompute& gc, const std::string& what) {
    GroupDiff d;
    const long jn = L.jumpDots();
    std::vector<unsigned char> gpu(static_cast<std::size_t>(2 * jn)), cpu(static_cast<std::size_t>(2 * jn));
    L.jumpGroups(cpu.data());
    const bool ok = L.jumpGroupsGpu(gc, gpu.data());
    check(what + ": the GPU kernel ran", ok);
    if (!ok) return d;
    d.dots = 2 * jn;
    const double u = L.jumpU(), phi = L.jumpPhi();
    const JumpTerms T = jumpTerms(kPi / 2 * u, phi, u);
    const bool wheel = L.p.phase == "Wheel";
    for (long k = 0; k < 2 * jn; k++) {
        const int cg = cpu[std::size_t(k)], gg = gpu[std::size_t(k)];
        if (cg == gg) continue;
        const int pop = k >= jn ? 1 : 0; const long i = k - pop * jn;
        const std::vector<float>& arr = pop ? L.s.jB : L.s.jA;
        const std::complex<double> A(arr[std::size_t(4 * i)], arr[std::size_t(4 * i + 1)]), B(arr[std::size_t(4 * i + 2)], arr[std::size_t(4 * i + 3)]);
        if ((cg == 255) != (gg == 255)) {
            d.shownDiff++;
            double fA, fB; jumpFractions(A, B, T, fA, fB);
            const double f = pop ? fB : fA, uu = splitUniform((std::uint64_t(i) + (std::uint64_t(pop) << 40)) * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL);
            const double tol = 2e-5;       // the fractions are ratios of float sums: their error is a few 1e-7 (measured, printed below); 2e-5 also covers dots near a node of the wave
            const double ratio = std::fabs(uu - f) / tol;
            d.worstRatio = std::max(d.worstRatio, ratio);
            if (!(ratio <= 1)) { d.bad++; if (d.bad < 4) std::printf("  jump dot pop %d i %ld: CPU %d GPU %d, u %.9g, fraction %.9g\n", pop, i, cg, gg, uu, f); }
        } else {
            d.binDiff++;
            const std::complex<double> z = T.ep * (T.cth * A + T.sth * B * T.em);
            const double S = std::abs(T.cth * A) + std::abs(T.sth * B), cm = std::max(std::abs(z), 1e-300);
            const double ph = std::arg(z), tol = 4 * 3 * kEps * S / cm + 2e-6;
            const double dist = wheel ? binEdgeDistance(ph) : std::fabs(std::fabs(ph) - kPi / 2);      // Sign: the two colours meet where Re = 0, at phase +- pi/2
            const double ratio = dist / tol;
            d.worstRatio = std::max(d.worstRatio, ratio);
            if (!(ratio <= 1)) { d.bad++; if (d.bad < 4) std::printf("  jump dot pop %d i %ld: CPU bin %d GPU bin %d, phase %.9g, distance %.3g\n", pop, i, cg, gg, ph, dist); }
        }
    }
    return d;
}

// a Classic Cloud 3D picture from given colour groups (the pipeline of atom_view.cpp: projection, depth bands, draw order, one palette draw)
static Picture groupPicture(render::Renderer2D& R, AtomLab& L, const std::vector<unsigned char>& grp, bool jump, bool light = false) {
    const double w = 1040, h = 713;
    std::vector<float> all;
    const float* PTS = L.s.pts.data();
    long cnt = long(grp.size());
    if (jump) { const long jn = L.jumpDots(); all.assign(L.s.pts.begin(), L.s.pts.begin() + 3 * jn); all.insert(all.end(), L.s.jp.begin(), L.s.jp.begin() + 3 * jn); PTS = all.data(); }
    const double pxm = L.pxPerM(std::min(w, h), L.s.szCur) * std::pow(10, L.p.zoom), ext = kExt[L.sizeN()] / L.p.Z * kA0;
    std::vector<float> PX, PY, PD;
    projectCloud(PTS, cnt, CloudCamera{w / 2, h / 2, pxm, std::cos(L.s.yaw), std::sin(L.s.yaw), std::cos(L.s.pitch), std::sin(L.s.pitch), ext}, w, h, PX, PY, PD);
    const DotColours dc = makeDotColours(true, L.usesWheel(), light, kPlus, kMinus);
    std::vector<Blob> B;
    buildClassic3D(PX.data(), PY.data(), PD.data(), grp.data(), cnt, dc, cloudDotWeight(L.cloudCap()), B);
    return draw1(R, 1040, 713, light ? Color{1, 1, 1, 1} : render::hexColor("#1e1e21"), [&] { R.blobsPalette(B.data(), int(B.size()), false, dc.palette()); });
}

static Picture g_cpuSuper14;                                               // scene 14 at beat 0 drawn from CPU groups on the 4.3 context, for the fallback test

static void gpuSuperScenes(render::Renderer2D& R, const std::string& dir) {
    render::IGroupCompute* gc = R.compute();
    check("OpenGL 4.3 context: the renderer made a compute object", gc != nullptr && gc->ok());
    if (!gc) return;
    struct Sc { int which; const char* name; };
    const Sc list[] = {{10, "scene 13 2p z + 2p x"}, {11, "scene 14 2p x + 2p m+1 (overlap)"}, {12, "scene 15 2p z + 3d z2 (beats)"}};
    for (const Sc& sc : list) {
        AtomLab L;
        benchSetup(L, sc.which, 1040, 713);
        check(std::string(sc.name) + ": the scene's superposition is on and has the 48,000 dots", L.superOn() && L.s.k >= 48000);
        GroupDiff total;
        for (const double beat : {0.0, 1.0, 2.0, 0.37, 3.1}) {              // t = 0, a quarter beat, half a beat (one beat is 4 s of the lab's beat clock), and two other times
            L.s.beatT = beat;
            const double theta = L.beatTheta();
            addDiff(total, compareSuper(L, *gc, theta, std::string(sc.name) + " at beat clock " + std::to_string(beat).substr(0, 4)));
        }
        std::printf("%-34s %ld dot-frames: %ld shown/hidden differences, %ld colour-bin differences (each within %.3g of its tolerance, worst)\n", sc.name, total.dots, total.shownDiff, total.binDiff, total.worstRatio);
        check(std::string(sc.name) + ": every dot's GPU decision equals the CPU one, except dots within float rounding of a threshold (none outside the tolerance)", total.bad == 0);
        check(std::string(sc.name) + ": differing dots are rare (under 0.05 % of the dot-frames)", double(total.shownDiff + total.binDiff) < 5e-4 * double(total.dots));
        // pictures: CPU groups against GPU groups, at beat 0, a quarter beat and half a beat
        for (const double beat : {0.0, 1.0, 2.0}) {
            L.s.beatT = beat;
            const double theta = L.beatTheta();
            const long cnt = std::min<long>(L.s.k, L.cloudCap());
            std::vector<unsigned char> cpu(static_cast<std::size_t>(cnt)), gpu(static_cast<std::size_t>(cnt));
            for (long i = 0; i < cnt; i++) cpu[std::size_t(i)] = (unsigned char)L.superGroup(i, theta);
            L.superGroupsGpu(*gc, cnt, theta, gpu.data());
            const Picture a = groupPicture(R, L, cpu, false), b = groupPicture(R, L, gpu, false);
            report(dir, std::string(sc.name) + " beat " + (beat == 0 ? "0" : beat == 1 ? "quarter" : "half"), a, b, 40);
            if (sc.which == 11 && beat == 0) g_cpuSuper14 = a;
        }
    }
}


// Directed edge cases: dots PUT on a colour-bin edge and on a thinning threshold (the real scenes have almost none), so that the CPU and the GPU really do disagree for some, and every
// disagreement is within the stated tolerance. This also tests the incremental upload: the dots are changed in place and only their range is marked dirty, as walkSuper does.
static void gpuEdgeCases(render::Renderer2D& R) {
    render::IGroupCompute* gc = R.compute();
    if (!gc) return;
    {   // 1. colour-bin edges: B = 0, so the phase is arg(A); A at the phase of a bin edge plus a small step
        AtomLab L; benchSetup(L, 12, 1040, 713);                               // 2p z + 3d z2, it beats
        const double theta0 = L.beatTheta();
        std::vector<unsigned char> warm(static_cast<std::size_t>(std::min<long>(L.s.k, L.cloudCap())));
        L.superGroupsGpu(*gc, long(warm.size()), theta0, warm.data());                     // a first full upload, as a frame does
        const double steps[] = {-2e-6, -5e-7, -1e-7, -3e-8, 0.0, 3e-8, 1e-7, 5e-7, 2e-6};
        long n = 0;
        const double w = 2 * 3.14159265358979323846 / kPhaseBins;
        for (long i = 0; i < 4000 && n < 4000; i++) {
            const double u = splitUniform(std::uint64_t(i) * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL);
            if (u > 0.4) continue;                                             // B = 0: the dot is shown when u < a^2 * 2 / 2 = 0.5, so u < 0.4 is shown with room to spare
            const int k = int(n % kPhaseBins); const double phi = (k + 0.5) * w + steps[(n / kPhaseBins) % 9];
            L.s.sa[std::size_t(4 * i)] = float(std::cos(phi)); L.s.sa[std::size_t(4 * i + 1)] = float(std::sin(phi)); L.s.sa[std::size_t(4 * i + 2)] = 0.f; L.s.sa[std::size_t(4 * i + 3)] = 0.f;
            n++;
        }
        L.s.saDirtyLo = 0; L.s.saDirtyHi = 4000;                               // only these dots changed: the GPU must take just them
        const GroupDiff d = compareSuper(L, *gc, theta0, "edge dots on bin edges");
        std::printf("dots put on bin edges: %ld; differing: %ld shown/hidden, %ld colour bin (worst %.3g of its tolerance)\n", n, d.shownDiff, d.binDiff, d.worstRatio);
        check("edge dots: some dots on a bin edge really come out in a different bin on the GPU (the test is not vacuous)", d.binDiff > 0);
        check("edge dots: every one of them is within the tolerance of a bin edge", d.bad == 0);
        check("edge dots: the incremental upload took the changed dots (all other dots equal)", d.shownDiff + d.binDiff < n / 2);
    }
    {   // 2. thinning thresholds: A = 1, B = beta (real), theta = 0 (2p z + 2p x: one energy), so |c|^2 / q is a known function of beta; beta is chosen so that w / 2 = u + step for the dot's own u
        AtomLab L; benchSetup(L, 10, 1040, 713);
        const Superposition su = L.superState();
        const double a = su.a, b = su.b;
        const double steps[] = {-3e-6, -1e-6, -3e-7, -1e-7, 0.0, 1e-7, 3e-7, 1e-6, 3e-6};
        long n = 0;
        for (long i = 0; i < 20000 && n < 2000; i++) {
            const double u = splitUniform(std::uint64_t(i) * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL), target = u + steps[n % 9];
            if (target < 0.52 || target > 0.97) continue;                      // w / 2 = (a + b beta)^2 / (1 + beta^2) is between 1/2 (beta = 0 gives a^2 = 1/2... up to 1) for beta in (0, 1)
            double lo = 0, hi = 1;
            for (int it = 0; it < 80; it++) { const double m = 0.5 * (lo + hi), f = (a + b * m) * (a + b * m) / (1 + m * m); if (f < target) lo = m; else hi = m; }
            const double beta = 0.5 * (lo + hi);
            L.s.sa[std::size_t(4 * i)] = 1.f; L.s.sa[std::size_t(4 * i + 1)] = 0.f; L.s.sa[std::size_t(4 * i + 2)] = float(beta); L.s.sa[std::size_t(4 * i + 3)] = 0.f;
            n++;
        }
        L.s.saDirtyLo = 0; L.s.saDirtyHi = 20000;
        const GroupDiff d = compareSuper(L, *gc, L.beatTheta(), "edge dots on thinning thresholds");
        std::printf("dots put on a thinning threshold: %ld; differing: %ld shown/hidden, %ld colour bin (worst %.3g of its tolerance)\n", n, d.shownDiff, d.binDiff, d.worstRatio);
        check("edge dots: some dots on a thinning threshold really come out differently on the GPU (the test is not vacuous)", d.shownDiff > 0);
        check("edge dots: every one of them is within the tolerance of its threshold", d.bad == 0);
    }
    {   // 2b. the real thing: frames in which the lab keeps sampling new dots (walkSuper writes 64 amplitudes a frame and marks them), the GPU copy following by dirty ranges only
        AtomLab L; benchSetup(L, 12, 1040, 713);
        GroupDiff total;
        for (int frame = 0; frame < 40; frame++) {
            L.tick(1.0 / 240); L.step(1.0 / 240);
            addDiff(total, compareSuper(L, *gc, L.beatTheta(), "frame " + std::to_string(frame) + " of a running superposition"));
        }
        std::printf("40 running frames of scene 15: %ld dot-frames, %ld differences (worst %.3g of its tolerance)\n", total.dots, total.shownDiff + total.binDiff, total.worstRatio);
        check("running frames: the GPU copy kept up by dirty ranges (every frame equal to the CPU, except edge dots)", total.bad == 0 && total.shownDiff + total.binDiff < 5e-4 * double(total.dots));
    }
    {   // 3. a new epoch (the lab restarted) is a full upload: after a restart the GPU must not keep the old dots
        AtomLab L; benchSetup(L, 11, 1040, 713);
        std::vector<unsigned char> g(static_cast<std::size_t>(std::min<long>(L.s.k, L.cloudCap())));
        L.superGroupsGpu(*gc, long(g.size()), 0, g.data());
        const std::uint64_t e0 = L.s.saEpoch;
        L.restart();
        check("restart: the amplitude array gets a new epoch", L.s.saEpoch != e0);
        for (int i = 0; i < 40000 && L.s.k < L.cloudCap(); i++) L.step(1.0 / 240);
        const GroupDiff d = compareSuper(L, *gc, 0, "after a restart");
        check("restart: the GPU copy follows the new dots", d.bad == 0 && d.shownDiff + d.binDiff < 50);
    }
}

// ================= 0.13 stage 3: the 90 % outline on the GPU (compute shaders, OpenGL 4.3) =================
// The CPU functions (outlineGridCpu, outlineLevelCpu, outlinePlaneCpu, outlineFromGrid) are the reference. The GPU's density grid may differ from the CPU's only within float rounding; the 90 % level it
// finds must be the right level FOR ITS OWN GRID (the enclosed probability of the cells at or above it is 0.9 to the float sum's accuracy, and the next higher cell value encloses less), and must agree
// with the CPU's level to 1e-4 relative; the Slice's plane likewise. Jobs are polled the way the app does it (never blocking); here the test just keeps asking, with a short sleep.
static bool waitGrid(render::IOutlineCompute& oc, int job, std::vector<float>& g, float& lv) {
    for (int i = 0; i < 5000; i++) { const render::JobState st = oc.pollGrid(job, g, lv); if (st == render::JobState::Done) return true; if (st == render::JobState::Failed) return false; std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    return false;
}
static bool waitPlane(render::IOutlineCompute& oc, int job, std::vector<float>& v) {
    for (int i = 0; i < 5000; i++) { const render::JobState st = oc.pollPlane(job, v); if (st == render::JobState::Done) return true; if (st == render::JobState::Failed) return false; std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    return false;
}
// largest relative difference between two density grids, where the CPU cell is at least 1e-3 of the grid's maximum; and (for the rest) the largest absolute difference as a share of the maximum.
// The bounds: 32-bit floats carry 6e-8; the radial Laguerre polynomial and the angular polynomials are sums with signs, so near a node the result is smaller than its terms and the relative error
// is amplified by terms / result: measured up to 4.3e-5 relative (3d z2 plane), so the bound is 2e-4 for cells above 0.1 % of the maximum, and 2e-6 of the maximum for all the others.
struct ValDiff { double rel = 0, small = 0, gmax = 0; long over = 0; };
static ValDiff compareValues(const std::vector<float>& gpu, const std::vector<float>& cpu) {
    ValDiff d;
    for (float v : cpu) d.gmax = std::max(d.gmax, double(v));
    for (std::size_t i = 0; i < cpu.size(); i++) {
        const double c = cpu[i], g = gpu[i], diff = std::fabs(g - c);
        if (c >= 1e-3 * d.gmax) { d.rel = std::max(d.rel, diff / c); if (diff > 2e-4 * c) d.over++; }
        else { d.small = std::max(d.small, diff / d.gmax); if (diff > 2e-6 * d.gmax) d.over++; }
    }
    return d;
}


// A Classic Cloud 3D picture with the outline's wireframe (slice == nullptr), or a Slice picture with the cut's contour: the drawing of atom_view.cpp, line for line
static Picture outlinePicture(render::Renderer2D& R, AtomLab& L, const Outline& O, const std::vector<OutSeg2>* slice, bool light) {
    const double w = 1040, h = 713;
    const long cnt = std::min<long>(L.s.k, L.cloudCap());
    std::vector<unsigned char> grp(static_cast<std::size_t>(cnt));
    plainGroups(L.usesWheel(), L.s.ph.data(), L.s.sg.data(), cnt, grp.data());
    const double pxm = L.pxPerM(std::min(w, h), L.s.szCur) * std::pow(10, L.p.zoom), ext = kExt[L.sizeN()] / L.p.Z * kA0, cx = w / 2, cy = h / 2;
    const double ca = std::cos(L.s.yaw), sa = std::sin(L.s.yaw), cb = std::cos(L.s.pitch), sb = std::sin(L.s.pitch);
    const Color fg = light ? render::hexColor("#1c1c1a") : render::hexColor("#eeeeee"), accent = light ? render::hexColor("#2f6fed") : kPlus;
    const DotColours dc = makeDotColours(true, L.usesWheel(), light, accent, kMinus);
    const double olUnit = kA0 / L.p.Z * pxm;
    return draw1(R, 1040, 713, light ? Color{1, 1, 1, 1} : render::hexColor("#1e1e21"), [&] {
        std::vector<Blob> B;
        if (!slice) {
            std::vector<float> PX, PY, PD;
            projectCloud(L.s.pts.data(), cnt, CloudCamera{cx, cy, pxm, ca, sa, cb, sb, ext}, w, h, PX, PY, PD);
            buildClassic3D(PX.data(), PY.data(), PD.data(), grp.data(), cnt, dc, cloudDotWeight(L.cloudCap()), B);
            R.blobsPalette(B.data(), int(B.size()), false, dc.palette());
            const double um = kA0 / L.p.Z;
            const auto pr = [&](double x, double y, double z, double& X, double& Y, double& t) {
                x *= um; y *= um; z *= um;
                const double x1 = x * ca - y * sa, y1 = x * sa + y * ca, dep = y1 * cb - z * sb, up = y1 * sb + z * cb, f = 1 / (1 + dep / (4 * ext));
                X = cx + x1 * pxm * f; Y = cy - up * pxm * f; t = std::max(-1.0, std::min(1.0, dep / ext));
            };
            for (const OutSeg3& g : O.wire) {
                double X0, Y0, t0, X1, Y1, t1;
                pr(g.x0, g.y0, g.z0, X0, Y0, t0); pr(g.x1, g.y1, g.z1, X1, Y1, t1);
                Color c = fg; c.a = float(std::max(0.0, std::min(1.0, 0.42 - 0.22 * 0.5 * (t0 + t1 + 2) / 2)));
                R.line(X0, Y0, X1, Y1, 0.9, c);
            }
        } else {
            const double size = kExt[L.sizeN()] / L.p.Z * kA0, off = L.cutOffsetM(), th = 0.07 * size;
            for (long i = 0; i < cnt; i++) {
                const double dist = std::fabs(L.s.pts[std::size_t(3 * i + 1)] - off);
                if (dist > th || grp[std::size_t(i)] == kHiddenGroup) continue;
                const double X = cx + L.s.pts[std::size_t(3 * i)] * pxm, Y = cy - L.s.pts[std::size_t(3 * i + 2)] * pxm;
                if (X < -2 || X > w || Y < -2 || Y > h) continue;
                B.push_back(squareDot(dc, X - 1 + 1.25, Y - 1 + 1.25, 1.25, grp[std::size_t(i)], 0.6 * (1 - 0.6 * dist / th) * cloudDotWeight(L.cloudCap())));
            }
            R.blobsPalette(B.data(), int(B.size()), false, dc.palette());
            Color c = fg; c.a = 0.6f;
            for (const OutSeg2& g : *slice) R.line(cx + g.x0 * olUnit, cy - g.y0 * olUnit, cx + g.x1 * olUnit, cy - g.y1 * olUnit, 1.1, c);
        }
    });
}

static AtomLab* outlineLab(AtomLab& L, const char* shell, const char* axKey, const char* ax, const char* view) {
    seedRandom(20260101);
    L.p = AtomParams::appDefaults(); L.p.super = "Off"; L.p.outline = "On"; L.viewW = 1040; L.viewH = 713;
    L.setParam("Z", 92.0); L.setParam("N", 146.0); L.restart(); L.s.nucSeed = 1; L.s.spin = false;
    L.p.cstyle = "Classic"; L.p.cdens = "Normal";
    L.setParam("view", std::string(view)); L.setParam("orb", std::string(shell)); L.setParam(axKey, std::string(ax)); L.setParam("zoom", 0.0); L.resetView();
    L.p.outline = "On";
    for (int i = 0; i < 40000 && L.s.k < L.cloudCap(); i++) L.step(1.0 / 240);
    return &L;
}

static Picture g_cpuOutline2p;                                              // 2p z, Cloud 3D, the CPU outline, on the 4.3 context: for the fallback test

static void gpuOutlinePictures(render::Renderer2D& R, const std::string& dir) {
    render::IOutlineCompute* oc = R.outlineCompute();
    if (!oc) return;
    struct Sc { const char* shell; const char* axKey; const char* ax; const char* name; };
    const Sc list[] = {{"4f", "fAx", "z3", "4f z3"}, {"2p", "pAx", "z", "2p z"}, {"3d", "dAx", "z2", "3d z2"}};
    for (const Sc& sc : list)
        for (const char* view : {"Cloud 3D", "Slice"}) {
            AtomLab L;
            outlineLab(L, sc.shell, sc.axKey, sc.ax, view);
            const int fo = L.outlineStateId();
            const bool slice = std::string(view) == "Slice";
            clearOutlineCache(); setOutlineCompute(nullptr);
            const Outline cpuO = outlineOf(fo);
            const double off0 = L.p.cut * 0.5 * kExt[orbitals()[stateOrbital(fo)].n];
            const std::vector<OutSeg2> cpuSlice = slice ? outlineSlice(fo, false, off0) : std::vector<OutSeg2>();
            clearOutlineCache(); setOutlineCompute(oc);
            const Outline* g = nullptr;
            outlineReady(fo, true);
            for (int i = 0; i < 5000 && !g; i++) { g = outlineReady(fo, true); if (!g) std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
            check(std::string(sc.name) + " " + view + ": the GPU outline arrives", g != nullptr);
            if (!g) continue;
            std::vector<OutSeg2> gpuSlice;
            if (slice) {
                const int job = oc->beginPlane(outlineDensityOf(fo), 200, float(g->half), false, float(off0));
                std::vector<float> v; const bool ok = job > 0 && waitPlane(*oc, job, v);
                check(std::string(sc.name) + " Slice: the GPU plane arrives", ok);
                if (ok) gpuSlice = outlineSliceFromValues(*g, v);
            }
            for (const bool light : {false, true}) {
                const Picture a = outlinePicture(R, L, cpuO, slice ? &cpuSlice : nullptr, light), b = outlinePicture(R, L, *g, slice ? &gpuSlice : nullptr, light);
                const Diff d = compare(a, b);
                std::printf("outline %-8s %-8s %s: largest difference %d of 255, %ld of %ld pixels differ (%.4f %%)\n", sc.name, view, light ? "light" : "dark", d.maxLevel, d.differing, d.total, 100.0 * double(d.differing) / double(d.total));
                check(std::string("outline picture ") + sc.name + " " + view + (light ? " light" : " dark") + ": the CPU and GPU outlines draw the same picture (under 0.05 % of the pixels differ: lines move only where a cell is within rounding of the level)", double(d.differing) < 5e-4 * double(d.total));
                save(dir, std::string("outline ") + sc.name + " " + view + (light ? " light" : " dark"), a, b);
                if (!light && !slice && std::string(sc.name) == "2p z") g_cpuOutline2p = a;
            }
            clearOutlineCache(); setOutlineCompute(nullptr);
        }
}

static void gpuOutlineGrids(render::Renderer2D& R) {
    render::IOutlineCompute* oc = R.outlineCompute();
    check("OpenGL 4.3 context: the renderer made an outline compute object", oc != nullptr && oc->ok());
    if (!oc) return;
    const int orbs[] = {orbIndex("4f|z3"), orbIndex("2p|z"), orbIndex("3d|z2"), complexState(orbIndex("4f"), 3), orbIndex("3s"), orbIndex("1s"), orbIndex("2p|x")};
    for (const int orb : orbs) {
        const std::string tag = "outline " + stateText(orb) + ": ";
        const Orbital& o = orbitals()[stateOrbital(orb)];
        const double half = 1.15 * kExt[o.n], h = 2 * half / kOutlineGrid, dV = h * h * h, norm = orbitalNorm(orb), scale = dV / norm;
        const std::vector<float> cpuGrid = outlineGridCpu(orb);
        const float cpuLevel = outlineLevelCpu(cpuGrid, dV, norm);
        const int job = oc->beginGrid(outlineDensityOf(orb), kOutlineGrid, float(half), float(scale), float(kOutlineProb));
        check(tag + "the grid job starts", job > 0);
        std::vector<float> g; float lv = 0;
        const bool done = job > 0 && waitGrid(*oc, job, g, lv);
        check(tag + "the grid job finishes", done && g.size() == cpuGrid.size());
        if (!done || g.size() != cpuGrid.size()) continue;
        const ValDiff d = compareValues(g, cpuGrid);
        // the level, on the GPU's own grid: the cells at or above it hold 0.9 (to the float sum), the next higher cell value holds less
        double above = 0; float next = 3.4e38f;
        for (float v : g) { if (v >= lv) above += double(v) * scale; if (v > lv && v < next) next = v; }
        double aboveNext = 0; for (float v : g) if (v >= next) aboveNext += double(v) * scale;
        const double ratio = double(lv) / double(cpuLevel);
        std::printf("%-12s grid: worst relative difference %.2e (cells over the bound: %ld), level GPU/CPU %.8f, enclosed probability at the GPU level %.7f, at the next cell value %.7f\n", stateText(orb).c_str(), d.rel, d.over, ratio, above, aboveNext);
        check(tag + "the GPU density grid equals the CPU's within float rounding (2e-4 relative where the cell is above 0.1 % of the maximum, 2e-6 of the maximum elsewhere: near a node the polynomial cancels)", d.over == 0);
        check(tag + "the GPU's 90 % level encloses 0.9 on its own grid (>= 0.9 - 1e-6) and the next higher cell value encloses less (< 0.9 + 1e-6)", above >= 0.9 - 1e-6 && aboveNext < 0.9 + 1e-6);
        check(tag + "the GPU's 90 % level agrees with the CPU's to 1e-4 relative", std::fabs(ratio - 1) < 1e-4);
    }
}

static void gpuOutlinePipeline(render::Renderer2D& R) {
    render::IOutlineCompute* oc = R.outlineCompute();
    if (!oc) return;
    setOutlineCompute(oc);
    for (const int orb : {orbIndex("4f|z3"), orbIndex("2p|z"), orbIndex("3d|z2"), complexState(orbIndex("4f"), 3)}) {
        const std::string tag = "outline pipeline " + stateText(orb) + ": ";
        clearOutlineCache();
        const Outline cpu = outlineOf(orb);
        clearOutlineCache();
        const Outline* first = outlineReady(orb, true);
        check(tag + "the first request returns nothing (the GPU job runs, nothing waits)", first == nullptr);
        const Outline* got = nullptr;
        for (int i = 0; i < 5000 && !got; i++) { got = outlineReady(orb, true); if (!got) std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        check(tag + "the outline arrives", got != nullptr);
        if (!got) continue;
        const double dc = std::fabs(got->c / cpu.c - 1);
        std::printf("%-12s outline: level GPU/CPU %.8f, mass %.5f (CPU %.5f), reach %.4f (CPU %.4f), side %zu (CPU %zu), wire %zu (CPU %zu)\n", stateText(orb).c_str(), got->c / cpu.c, got->mass, cpu.mass, got->rmax, cpu.rmax, got->side.size(), cpu.side.size(), got->wire.size(), cpu.wire.size());
        check(tag + "level within 1e-4 of the CPU's, the same grid geometry, the enclosed probability 0.9 to the width of the cells at the level (0.01, as the CPU's own)", dc < 1e-4 && got->half == cpu.half && got->norm == cpu.norm && std::fabs(got->mass - 0.9) < 0.01 && std::fabs(got->mass - cpu.mass) < 5e-3);
        check(tag + "the reach and the line counts agree with the CPU's (the lines differ only where a cell is within rounding of the level)", std::fabs(got->rmax - cpu.rmax) <= 2 * cpu.half / kOutlineGrid &&
              std::abs(long(got->side.size()) - long(cpu.side.size())) <= 4 + long(cpu.side.size()) / 50 && std::abs(long(got->wire.size()) - long(cpu.wire.size())) <= 8 + long(cpu.wire.size()) / 50);
    }
    clearOutlineCache();
    setOutlineCompute(nullptr);
}

static void gpuOutlineSlice(render::Renderer2D& R) {
    render::IOutlineCompute* oc = R.outlineCompute();
    if (!oc) return;
    for (const int orb : {orbIndex("4f|z3"), orbIndex("2p|z"), orbIndex("3d|z2"), complexState(orbIndex("4f"), 3)}) {
        clearOutlineCache();
        const Outline& O = outlineOf(orb);
        const Orbital& o = orbitals()[stateOrbital(orb)];
        const std::string tag = "Slice " + stateText(orb) + ": ";
        ValDiff total; long segDiff = 0, planes = 0; bool allOk = true;
        for (const bool hz : {false, true})
            for (const double off : {-1.4, 0.0, 0.37, 1.9}) {
                const std::vector<float> cpu = outlinePlaneCpu(orb, O.half, hz, off, 200);
                const int job = oc->beginPlane(outlineDensityOf(orb), 200, float(O.half), hz, float(off));
                std::vector<float> gv;
                const bool ok = job > 0 && waitPlane(*oc, job, gv) && gv.size() == cpu.size();
                allOk = allOk && ok;
                if (!ok) continue;
                const ValDiff d = compareValues(gv, cpu);
                total.rel = std::max(total.rel, d.rel); total.small = std::max(total.small, d.small); total.over += d.over;
                const std::vector<OutSeg2> a = outlineSlice(orb, hz, off), b = outlineSliceFromValues(O, gv);
                segDiff = std::max<long>(segDiff, std::abs(long(a.size()) - long(b.size()))); planes++;
            }
        (void)o;
        std::printf("%-12s Slice: %ld planes, worst relative difference %.2e, cells over the tolerance %ld, largest difference in the number of contour segments %ld\n", stateText(orb).c_str(), planes, total.rel, total.over, segDiff);
        check(tag + "all eight plane jobs ran", allOk && planes == 8);
        check(tag + "the GPU plane equals the CPU's within float rounding (2e-4 relative above 0.1 % of the maximum, 2e-6 of the maximum elsewhere)", total.over == 0);
        check(tag + "the contour from the GPU's plane has the CPU's number of segments, to 2 (a cell within rounding of the level)", segDiff <= 2);
    }
    // the Slice contour object as the app drives it: one update a frame, the cut moving, the real GPU; each time it has caught up it equals the CPU's contour for that cut (within the 2 segments of a cell at rounding)
    {
        const int orb = orbIndex("4f|z3");
        clearOutlineCache(); setOutlineCompute(nullptr);
        const Outline O = outlineOf(orb);
        clearOutlineCache(); setOutlineCompute(oc);
        SliceContour sc;
        long worst = 0; bool caughtUp = true; int framesBehind = 0, maxBehind = 0;
        for (const double off : {0.0, 0.37, -0.8, 1.9}) {
            framesBehind = 0;
            for (int f = 0; f < 5000; f++) {
                sc.update(O, orb, false, off, true);
                if (sc.shows(orb, false) && !sc.jobInFlight() && std::abs(long(sc.segments().size()) - long(outlineSlice(orb, false, off).size())) <= 2) break;
                framesBehind++; std::this_thread::sleep_for(std::chrono::milliseconds(1));
                if (f == 4999) caughtUp = false;
            }
            maxBehind = std::max(maxBehind, framesBehind);
            worst = std::max<long>(worst, std::abs(long(sc.segments().size()) - long(outlineSlice(orb, false, off).size())));
        }
        std::printf("SliceContour on the GPU, four cuts: largest difference in segments %ld, updates needed to catch up at most %d (each one a frame of the app, with the app's frame time in between)\n", worst, maxBehind);
        check("Slice contour on the GPU: it catches up with every cut and equals the CPU's contour (to 2 segments)", caughtUp && worst <= 2);
        setOutlineCompute(nullptr); clearOutlineCache();
    }
    clearOutlineCache();
}

static void gpuJumpScenes(render::Renderer2D& R, const std::string& dir) {
    render::IGroupCompute* gc = R.compute();
    if (!gc) return;
    for (const int which : {8, 9}) {
        for (const char* phase : {"Sign", "Wheel"}) {
            AtomLab L;
            benchSetup(L, which, 1040, 713);
            L.p.phase = phase;
            const std::string name = std::string("scene ") + (which == 8 ? "11 jump held" : "12 jumps") + ", phase " + phase;
            for (int f = 0; f < 700 && !(L.s.trans && L.jumpDrawn()); f++) { benchFrame(L, which, f, f - 200); L.tick(1.0 / 240); L.step(1.0 / 240); }
            check(name + ": a jump is in flight and built", L.s.trans && L.jumpDrawn());
            if (!(L.s.trans && L.jumpDrawn())) continue;
            GroupDiff total;
            for (const double t : {0.0, 0.3, 0.6, 0.9, 1.15}) {              // the jump's clock: the start, and four times along it (it lasts 1.2 s)
                L.s.trans->t = t;
                addDiff(total, compareJump(L, *gc, name + " at jump time " + std::to_string(t).substr(0, 4)));
            }
            std::printf("%-34s %ld dot-frames: %ld shown/hidden differences, %ld colour differences (worst %.3g of its tolerance)\n", name.c_str(), total.dots, total.shownDiff, total.binDiff, total.worstRatio);
            check(name + ": every dot's GPU decision equals the CPU one, except dots within float rounding of a threshold", total.bad == 0);
            check(name + ": differing dots are rare (under 0.05 % of the dot-frames)", double(total.shownDiff + total.binDiff) < 5e-4 * double(total.dots));
            if (which == 8) {                                                // a picture of the jump in flight (the middle of it), CPU groups against GPU groups
                L.s.trans->t = 0.6;
                std::vector<unsigned char> cpu(static_cast<std::size_t>(2 * L.jumpDots())), gpu(cpu.size());
                L.jumpGroups(cpu.data()); L.jumpGroupsGpu(*gc, gpu.data());
                report(dir, std::string("jump in flight, phase ") + phase, groupPicture(R, L, cpu, true), groupPicture(R, L, gpu, true), 40);
            }
        }
    }
}

// the PC without OpenGL 4.3: a context that says 3.3 (Mesa's version override); the renderer must start, have no compute object, and the CPU path must be what draws, with the same picture
static void fallback33(EGLDisplay dpy, EGLConfig cfg, EGLSurface surf, const std::string& dir) {
    setenv("MESA_GL_VERSION_OVERRIDE", "3.3", 1); setenv("MESA_GLSL_VERSION_OVERRIDE", "330", 1);
    const EGLint ctxAttr[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3, EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
    if (ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, surf, surf, ctx)) { check("a 3.3 context can be made", false); return; }
    g_fbo = g_tex = 0; g_fw = g_fh = 0;                                     // the picture's framebuffer belonged to the other context
    const char* missing = nullptr;
    check("3.3 context: the 3.3 functions load", plgl::loadGL(&missing));
    GLint maj = 0, min = 0;
    plgl::glGetIntegerv(GL_MAJOR_VERSION, &maj); plgl::glGetIntegerv(GL_MINOR_VERSION, &min);
    std::printf("fallback context: OpenGL %d.%d (%s)\n", maj, min, plgl::glGetString(GL_VERSION));
    check("3.3 context: it really reports 3.3 (not 4.3 or more)", maj == 3 && min == 3);
    render::Renderer2D R33;
    check("3.3 context: the renderer starts (nothing crashes, no shader error)", R33.init());
    check("3.3 context: there is no compute object, so the CPU path is used", R33.compute() == nullptr);
    AtomLab L;
    benchSetup(L, 11, 1040, 713);
    L.s.beatT = 0;
    const long cnt = std::min<long>(L.s.k, L.cloudCap());
    std::vector<unsigned char> viaFill(static_cast<std::size_t>(cnt)), cpu(static_cast<std::size_t>(cnt));
    const GroupsBy used = fillGroups(L, R33.compute(), cnt, viaFill.data());
    const double theta = L.beatTheta();
    for (long i = 0; i < cnt; i++) cpu[std::size_t(i)] = (unsigned char)L.superGroup(i, theta);
    check("3.3 context: fillGroups used the CPU path", used == GroupsBy::Cpu);
    check("3.3 context: its groups are exactly the CPU reference's", viaFill == cpu);
    const Picture p33 = groupPicture(R33, L, viaFill, false);
    if (g_cpuSuper14.px.empty()) check("scene 14 picture from the 4.3 context was kept", false);
    else report(dir, "3.3 fallback scene 14 beat 0 against the 4.3 context's CPU-groups picture", g_cpuSuper14, p33, 0);
    // the switch: on a 4.3 context with gpuCompute off the CPU is used too (tested with a stand-in object that must not be called)
    struct Never : render::IGroupCompute { bool ok() const override { return true; } bool superGroups(const float*, long, std::uint64_t, long, long, const render::SuperGroupParams&, unsigned char*) override { called = true; return false; }
                                           bool jumpGroups(const float*, const float*, long, std::uint64_t, const render::JumpGroupParams&, unsigned char*) override { called = true; return false; } bool called = false; } never;
    L.gpuCompute = false;
    check("GPU superposition switch Off: the CPU path is used and the compute object is not called", fillGroups(L, &never, cnt, viaFill.data()) == GroupsBy::Cpu && !never.called && viaFill == cpu);
    L.gpuCompute = true;
    check("a compute object that fails (returns false) falls back to the CPU result", fillGroups(L, &never, cnt, viaFill.data()) == GroupsBy::Cpu && never.called && viaFill == cpu);
    // 0.13 stage 3: the outline on the 3.3 context: no compute object, the CPU path at once (a picture with an outline on the first frame), the same outline and the same picture
    check("3.3 context: there is no outline compute object either", R33.outlineCompute() == nullptr);
    setOutlineCompute(R33.outlineCompute());
    {
        AtomLab Lo;
        outlineLab(Lo, "2p", "pAx", "z", "Cloud 3D");
        const int fo = Lo.outlineStateId();
        clearOutlineCache();
        const Outline* o = outlineReady(fo, true);
        check("3.3 context: outlineReady returns the CPU outline at once (never blank)", o != nullptr);
        clearOutlineCache();
        const Outline ref = outlineOf(fo);
        check("3.3 context: it is the CPU's outline", o != nullptr && o->c == ref.c && o->mass == ref.mass && o->rmax == ref.rmax && o->wire.size() == ref.wire.size());
        if (o && !g_cpuOutline2p.px.empty()) report(dir, "3.3 fallback outline 2p z Cloud 3D against the 4.3 context's CPU-outline picture", g_cpuOutline2p, outlinePicture(R33, Lo, *o, nullptr, false), 0);
        clearOutlineCache();
    }
    setOutlineCompute(nullptr);
    R33.shutdown();
}

int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : "";
    setenv("EGL_PLATFORM", "surfaceless", 0);
    setenv("LIBGL_ALWAYS_SOFTWARE", "1", 0);
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLint maj = 0, min = 0;
    if (!eglInitialize(dpy, &maj, &min)) { std::printf("cannot start EGL (is Mesa's llvmpipe installed?)\n"); return 2; }
    eglBindAPI(EGL_OPENGL_API);
    const EGLint cfgAttr[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
    EGLConfig cfg; EGLint n = 0;
    if (!eglChooseConfig(dpy, cfgAttr, &cfg, 1, &n) || n < 1) { std::printf("no EGL config\n"); return 2; }
    const EGLint pbAttr[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pbAttr);
    const EGLint ctxAttr[] = {EGL_CONTEXT_MAJOR_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 3, EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};   // 0.13 stage 2: ask for 4.3
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
    if (ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, surf, surf, ctx)) { std::printf("no OpenGL 4.3 core context\n"); return 2; }
    const char* missing = nullptr;
    if (!plgl::loadGL(&missing)) { std::printf("missing GL function %s\n", missing); return 2; }
    std::printf("OpenGL: %s | %s | %s\n", plgl::glGetString(GL_VENDOR), plgl::glGetString(GL_RENDERER), plgl::glGetString(GL_VERSION));
    render::Renderer2D R;
    if (!R.init()) { std::printf("the renderer did not start (shader error above)\n"); return 2; }
    swatches(R, dir);
    scenes(R, dir);
    glowDots(R, dir);
    gpuSuperScenes(R, dir);
    gpuJumpScenes(R, dir);
    gpuEdgeCases(R);
    gpuOutlineGrids(R);
    gpuOutlinePipeline(R);
    gpuOutlineSlice(R);
    gpuOutlinePictures(R, dir);
    R.shutdown();
    fallback33(dpy, cfg, surf, dir);
    std::printf("%d GL checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
