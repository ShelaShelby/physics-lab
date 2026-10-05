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
#include <string>
#include <vector>

#include "atom/atom_bench.h"
#include "atom/cloud_dots.h"
#include "atom/cloud_light.h"
#include "render/renderer2d.h"

// ---- the GL functions, from EGL (the app gets them from SDL: render/gl_loader.cpp) ----
namespace plgl {
#define PL_GL_DEFINE(ret, name, args) PFN_##name name = nullptr;
PL_GL_FUNCTIONS(PL_GL_DEFINE)
#undef PL_GL_DEFINE
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
    const EGLint ctxAttr[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3, EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
    if (ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, surf, surf, ctx)) { std::printf("no OpenGL 3.3 core context\n"); return 2; }
    const char* missing = nullptr;
    if (!plgl::loadGL(&missing)) { std::printf("missing GL function %s\n", missing); return 2; }
    std::printf("OpenGL: %s | %s | %s\n", plgl::glGetString(GL_VENDOR), plgl::glGetString(GL_RENDERER), plgl::glGetString(GL_VERSION));
    render::Renderer2D R;
    if (!R.init()) { std::printf("the renderer did not start (shader error above)\n"); return 2; }
    swatches(R, dir);
    scenes(R, dir);
    glowDots(R, dir);
    std::printf("%d GL checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
