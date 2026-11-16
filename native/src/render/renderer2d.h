// A small batched 2D renderer on OpenGL 3.3 core.
//
// Coordinates are "picture points" with (0, 0) at the top-left and y going down,
// All shapes of a frame go into one vertex buffer
// and are drawn with one draw call, in the order they were added.
// Positions arrive as doubles (the orbit maths is in double precision) and are
// only turned into floats once they are on or near the screen.
#pragma once

#include "planet_maps.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <cstdint>
#include <thread>
#include <vector>

#include "gl_loader.h"
#include "group_compute.h"
#include "mesh.h"
#include "outline_compute.h"
#include "rock_compute.h"

namespace render {

struct Color {
    float r = 1, g = 1, b = 1, a = 1;
    Color withAlpha(float alpha) const { return {r, g, b, alpha}; }
};
Color hexColor(const char* hex, float alpha = 1);   // "#rrggbb"
constexpr float kBlobPalSquare = 5, kBlobPalSoft = 6;   // Blob::kind of a square / soft dot with a colour picked by the shader (see Renderer2D::blobsPalette)
Color rgba(int r, int g, int b, float a = 1);

class Renderer2D {
public:
    bool init();
    void shutdown();
    // The OpenGL 4.3 compute path (0.13 stage 2), or nullptr when this context has no OpenGL 4.3 (then the CPU does that work, as before). Valid after init().
    IGroupCompute* compute() const { return compute_; }
    IOutlineCompute* outlineCompute() const { return outline_; }   // the same for the 90 % outline (0.13 stage 3)
    IRockCompute* rockCompute() const { return rocks_; }           // the same for the Orbit Lab's belt rocks (0.13 stage 4)
    void useRockCompute(IRockCompute* r) { delete rocks_; rocks_ = r; }   // tests: put a stand-in (or nullptr) in place of the OpenGL object; the renderer owns it
    void disableCompute();                // forget the compute object: the CPU paths are used (PL_NO_COMPUTE; tests)

    // Start a frame for a picture `width` × `height` points.
    void begin(double width, double height);
    // Draw everything that was added. The framebuffer is fbWidth × fbHeight pixels.
    // fbY: the picture's bottom edge, pixels from the window's bottom. outFbo: draw into this framebuffer
    // (fbWidth × fbHeight, from its bottom-left corner) instead of the window: used by 📷 Save image.
    void end(int fbWidth, int fbHeight, int fbY = 0, GLuint outFbo = 0);

    void rect(double x, double y, double w, double h, Color c);
    void line(double x0, double y0, double x1, double y1, double width, Color c);
    void triangle(double x0, double y0, Color c0, double x1, double y1, Color c1, double x2, double y2, Color c2);
    void disc(double x, double y, double r, Color c);                       // flat round dot, smooth edge
    void ring(double x, double y, double r, double width, Color c);         // circle outline
    void sphere(double x, double y, double r, Color c, double lightX, double lightY, double lightZ);  // a ball lit from the unit direction (right, down, towards you) on screen
    void sunDisc(double x, double y, double r, Color c);                    // the Sun: brighter middle, darker edge
    void fadeDisc(double x, double y, double r, Color c);                   // a soft dot: full colour in the middle, fading to nothing at radius r

    // Many small squares of one colour, drawn with ONE instanced draw call (used for the belts'
    // thousands of rocks). xy holds the squares' centres, 2 floats each, in picture points.
    void squares(const float* xy, int count, double size, Color c);
    // The same squares, but their places are worked out on the GPU (0.13 stage 4 part A, render/rock_compute.h): sends the rocks if this set is not on the GPU yet, and in end() runs the compute
    // shader for the camera `f` and draws from the buffer it wrote (no CPU wait, no per-rock CPU work). Returns false, adding nothing, when there is no OpenGL 4.3 (the caller then uses squares()).
    bool rocksGpu(const RockSet& set, const RockFrame& f, double size, Color c);

    // Many round or square "blobs" with their own colours, drawn with ONE instanced draw call (the
    // Atom Lab's cloud dots, glowing nucleons, gluon-field particles, quarks). Each blob is a disc of
    // radius r filled like a canvas radial gradient: colour c0 at the focal circle (centre fx, fy and
    // radius f0, in units of r from the blob's centre) through c1 at stop s1 to c2 at the edge.
    // additive: colours add up (the canvas "lighter" mode).
    struct Blob {
        float x, y, r;
        float kind;          // 0 gradient disc, 1 fuzzy nucleon profile (colour c0), 2 square of side 2r (c0), 3 flat disc (c0), 4 soft Gaussian dot (c0; fades to 0 at r),
                             // 5 and 6: a square (like 2) or a soft dot (like 4) whose colour the SHADER picks from the group index in fx and the palette of blobsPalette(); only c0.a is used
                             // 7 a ring (c0): hard-edged band of half width f0 round the circle of radius fx (both in units of r); 8 a line (c0) from -(fx, fy) to +(fx, fy) (units of r), half width f0, no caps (0.16 stage 5)
        float fx, fy, f0, s1;
        Color c0, c1, c2;
    };
    void blobs(const Blob* b, int count, bool additive = false);
    static float fuzzAt(float d);   // 0.46.5: the fuzzy nucleon's profile (kind 1 blobs) at d = distance / blob radius, 0..1 (1 at the centre, 0 beyond the edge), as the shader draws it
    // 0.40: kind 9, spheres with a true depth at every pixel: x, y, r as a blob; fx = the sphere's index in this call (0, 1, ...), fy = its centre's depth in points (larger = nearer),
    // f0 = the outline's width in points; c0 the outline, c1 the lit colour, c2 the shadow. A pixel of one sphere is dropped where another of the same call is nearer, so the
    // order does not matter for what hides what (back to front still helps the smooth edges). At most kMaxSpheres per call; more are drawn but do not hide each other.
    static constexpr int kMaxSpheres = 64;
    void spheres(const Blob* b, int count);
    // Blobs of kind 5 and 6 (the cloud dots, 0.13 stage 1): the colour is picked in the vertex shader from the group number in fx. Wheel: group 0..23 is the colour of the phase at the middle
    // of that bin (a port of atom::phaseRgb in atom/phase_wheel.h; `light` = the light-page colours); otherwise the group 0 is sign[0] and any other group sign[1].
    struct PhasePalette { bool wheel = false, light = false; Color sign[2]; };
    void blobsPalette(const Blob* b, int count, bool additive, const PhasePalette& pal);
    // The same, but TAKES the caller's list (0.13 stage 4 part C): when no blobs are waiting yet this frame the two vectors are swapped, so the 48,000 blobs (3.8 MB) are not copied; the caller gets the
    // renderer's old, emptied buffer back (with its capacity) and keeps it for the next frame. Otherwise they are appended. `b` is empty afterwards. The picture is the same as blobsPalette's.
    void blobsPaletteTake(std::vector<Blob>& b, bool additive, const PhasePalette& pal);
    const std::vector<Blob>& pendingBlobs() const { return blobs_; }          // what end() would draw as blobs (for tests)
    std::size_t pendingSegments() const { return segments_.size(); }          // the draw calls so far (the open run of triangles is not counted until it is closed)
    // A retained mesh (0.13 stage 4 part B, render/mesh.h): uploaded once with meshCreate (needs the OpenGL context), drawn from the GPU buffer in every frame that asks for it with drawMesh, in the order
    // of the other shapes of the frame. The projection P goes in as uniforms, so changing it costs nothing but the uniforms. meshDelete frees the buffer. Ids are small integers (>= 0).
    int meshCreate(const std::vector<MeshVertex>& v);
    void meshDelete(int id);
    void drawMesh(int id, int firstVertex, int vertexCount, const MeshProjection& P);
    int meshesAlive() const;                                                  // for tests
    long meshUploads() const { return meshUploads_; }                         // vertices sent to the GPU by meshCreate so far (for tests: nothing is sent while a mesh is just drawn)
    // Everything drawn between beginSoftLayer() and endSoftLayer() goes into an offscreen picture at
    // blurry things cost a fraction of the pixels, and out-of-focus things look soft).
    void beginSoftLayer(float scale);
    void endSoftLayer();
    // 0.40: everything drawn between beginCloudLayer() and endCloudLayer() adds up in a full-size 32-bit float picture, which is then put on the screen once:
    // additive (dark page: the light is added) or premultiplied over (light page). The electron cloud's soft dots are each fainter than an 8-bit picture can hold
    // (the exposure keeps the dense core below white), so drawn straight onto the screen the faint ones rounded to nothing, and more dots made it worse.
    void beginCloudLayer();
    void endCloudLayer(bool additive);
    // 0.46 (C4): the electron cloud's Glow dots in Cloud 3D, the game way: the dots' places stay in a GPU buffer (only dots made since the last frame are sent: the cloud replaces a
    // few hundred a frame), the vertex shader projects, sizes and lights each dot from the camera (kCloudVS: the CPU path's formulas), and the exposure's 8 x 8-point grid is summed
    // on the GPU and read back (kCloudGridVS), so no CPU pass over the dots is left but the colour groups. Draw it between beginCloudLayer and endCloudLayer. The picture is the same
    // as glowBlobs + blobsPalette (tests/check_cloud_gpu.py compares them pixel by pixel). `exposure` gets the grid's cells (every cell, empty ones too) and returns uLight
    // (base x exposure x 12,000 / dots), or a negative number for "draw nothing" (as glowBlobs returning false); it runs inside end().
    struct CloudGpuDraw {
        const float* pts = nullptr; long cnt = 0, cap = 0;          // places (m, x y z), dots shown (0 .. cnt), the ring's size
        std::uint64_t epoch = 0; long k = 0;                        // which cloud (a new number when its dots were changed other than by being added) and dots recorded so far
        const unsigned char* grp = nullptr;                         // colour group of each dot (255: not shown), cnt of them
        float cx = 0, cy = 0, pxm = 0, ext = 1, ca = 1, sa = 0, cb = 1, sb = 0, w = 0, h = 0;
        bool additive = true;
        PhasePalette pal;
        std::function<double(const std::vector<float>&)> exposure;
    };
    bool cloudGpuOk() const { return clProgram_ != 0 && grProgram_ != 0; }
    void cloudGpu(const CloudGpuDraw& d);
    long cloudBytesSent() const { return clBytes_; }                           // bytes of dot places sent to the GPU so far (for tests: a still cloud sends nothing)

    // Monochrome theme: the whole picture in shades of grey
    void setGrayscale(bool on, Color bg) { gray_ = on; grayBg_ = bg; }   // bg: the page colour under the picture

    // What the last end() drew: draw calls, and shapes drawn by the instanced calls (squares and blobs)
    int lastDrawCalls() const { return lastCalls_; }
    int lastInstances() const { return lastInstances_; }
    long lastVertices() const { return lastVerts_; }                          // the triangle vertices end() uploaded (lines, rings, discs, text-free shapes)
    std::size_t pendingVertexBytes() const { return verts_.size() * sizeof(Vertex); }   // what end() would upload for the shapes added so far (a measuring aid for tests and tools)

    // A turning planet. K turns screen directions into the planet's own frame (rows: its 0° meridian,
    // 90° east, north pole; columns: screen right, up, towards you). map: its picture map (planet_maps.h), or
    // Map::None for a plain ball in the colour given (0.26: the grid is gone).
    // rings (0.26): up to 8 bands (inner, outer radius in planet radii, optical depth) that shadow the ball, or nullptr.
    // shapeY, shapeZ (0.27): the body's radii along its 90° E axis and its pole over its equatorial radius r along the 0° meridian
    // (render/ellipsoid.h; 1, 1 for a ball). The disc is then the ellipsoid's outline, drawn exactly (orthographic, as the ball).
    // 0.50: the colour of empty space behind the picture (sRGB): a lit ball's zero light is drawn in it, so a night side matches the sky (render/shading.h). Black by default.
    void setSpace(Color c) { space_ = c; }
    // 0.51.2: what dims the sunlight on a ball beyond its own night side: up to 4 round bodies that can eclipse the Sun there (centre in the screen frame,
    // right, up, towards you, and radius, all in units of this body's equatorial radius; the real places, not the drawn ones) with the Sun's angular
    // radius at the body (radians); air: Earth's air (render/shading.h earthAirLight).
    struct PlanetLight { float occ[16] = {}; int nOcc = 0; float sunAng = 0; bool air = false; };
    void planet(double x, double y, double r, Color base, double lightX, double lightY, double lightZ, const float K[9], Map map, const float* rings = nullptr, int nRings = 0,
                double shapeY = 1.0, double shapeZ = 1.0, bool lunar = false, const PlanetLight* light = nullptr);   // lunar: McEwen's lunar function (render/shading.h), not Lambert
    static constexpr int kMaxRingBands = 32;   // 0.51.2: ring bands that shadow a ball (was 8): Saturn's from Cassini's profile
    // 0.26: the Gaia all-sky map behind everything, over the whole W x H picture. kRad: points per radian from the
    // middle (the night sky's map, orbit/sky.cpp); M turns a direction in the view frame (right, up, depth) into
    // galactic coordinates (rows: towards l = 0, l = 90°, the north galactic pole); bright: 0..1.
    void skyMap(double W, double H, double kRad, const float M[9], double bright);
    bool hasMap(Map m) const { return tex_[static_cast<int>(m)] != 0; }
    // 0.55.0, the Black Hole Lab: the Gaia map seen past a non-spinning black hole at (cx, cy) points, kRad points per radian (the night sky's projection), every
    // pixel's ray bent by the lens table (its values of dphi at s = ln x, x = the angle in radians from the shadow's edge alphaSh: blackhole/schwarzschild.h
    // LensTable), the shadow black; bend false: the plain map by skyMap's own rule. The picture is drawn into a kept texture only when one of these
    // changed (or the picture's size); otherwise the kept picture is drawn again. M and bright as skyMap. The middle is put on a whole pixel.
    struct LensSky {
        double cx = 0, cy = 0, kRad = 1;
        float M[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
        double bright = 1;
        bool bend = true;
        double alphaSh = 0;                 // the shadow's angular radius, rad
        const std::vector<double>* table = nullptr;
        double s0 = 0, ds = 0;
        std::uint64_t tableVersion = 0;     // a new number for a new table (it is sent to the GPU again)
        bool debug = false;                 // tests: write the far direction (view frame: right, up, depth) and x in pixels as floats, not colours (into a float framebuffer)
        // 0.55.1: the gas disc in front of the sky (blackhole/disc.h DiscTable: u = 1/r against s and the swept angle, the ray's end Phi per row; the colour of a
        // black body against log10 T in units of the Sun's surface). Drawn only with bend. debugDisc (tests): the first crossing's order, r and g, and x in pixels.
        bool sky = true;                    // false: nothing behind (black), the disc alone
        bool disc = false, debugDisc = false;
        const std::vector<float>* discU = nullptr;   // nT x (2 nS): rows side 0 then side 1
        const std::vector<float>* discP = nullptr;   // 2 nS
        const std::vector<float>* discCol = nullptr; // 3 nC
        int discS = 0, discT = 0, discC = 0;
        double discS0 = 0, discDs0 = 0, discDs1 = 0, discXc = 0.1;
        std::uint64_t discVersion = 0, colVersion = 0;
        float discAxis[3] = {0, 0, 1};      // the disc's axis on the view frame (right, up, towards the camera)
        double rO = 20, rIn = 6, rOut = 30, tScale = 0, white = 1, colLog0 = 0, colDlog = 1;
        // 0.55.2: the spinning hole (kKerrFS, blackhole/spin_sky.h SpinTable), used when kerr is true (then table, alphaSh and the disc's a = 0 table are not).
        // kA (dir, b) and kB (u0..u3): kS columns x 2 kChi rows; kEdge: kEdgeN edge samples; kTau: kEdgeN + 1 values of tau. kM: the chart's axes (camera at
        // phi = 0) to galactic, through the camera's phi and the view; kO: the screen directions (right, up) of e_theta and e_phi. discF: F^(1/4) against ln r
        // (discFN values from ln r = discFLog0 in steps discFDl) for the disc's temperature at this spin; discCol, white, tScale, rIn, rOut as above.
        bool kerr = false;
        const std::vector<float>* kA = nullptr;
        const std::vector<float>* kB = nullptr;
        const std::vector<float>* kEdge = nullptr;
        const std::vector<float>* kTau = nullptr;
        int kChi = 0, kS = 0, kEdgeN = 0;
        double kSLo = 0, kXc = 0.3, kXlo = 1e-8, spin = 0;
        float kM[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
        float kO[4] = {0, 1, 1, 0};
        std::uint64_t kVersion = 0;
        const std::vector<float>* discF = nullptr;
        int discFN = 0;
        double discFLog0 = 0, discFDl = 1;
        std::uint64_t discFVersion = 0;
        // 0.55.3 (blackhole/looks.h): discFix[3] > 0: the disc is one colour, discFix[0..2] (linear, already over the screen's white), in place of the
        // temperature and shift rule (the spinning hole's pass). glow[0] > 0: the app's own glow over the finished bent picture (glow[0] the strength, glow[1]
        // the threshold, glow[2] the blur's sigma over the picture's height; blocks of 4 x 4 pixels), made when the picture is drawn anew and kept with it.
        float discFix[4] = {0, 0, 0, 0};
        float glow[3] = {0, 0, 0};
        // 0.55.4 (blackhole/light.h): glareKeep > 0: the eye's glare over the finished bent picture: the disc's light (before its colour is scaled into the screen)
        // averaged over blocks of glareBlock(...) pixels, read back, glareVeil(...) makes the veil on the processor; the picture is then the disc's light x glareKeep
        // (its colour scaled into the screen when brighter), the sky behind it, and the veil (bilinear between the blocks' centres). Made when the picture is drawn
        // anew and kept with it. W, H, cx, cy (window pixels, y up) and K (pixels per radian) are the picture's.
        float glareKeep = 0;
        int (*glareBlock)(int W, int H, double cx, double cy, double K) = nullptr;
        void (*glareVeil)(int W, int H, double cx, double cy, double K, const std::vector<float>& src, std::vector<float>& veil, double out[3]) = nullptr;
        // 0.55.5: glareAsync: the frame never waits for the glare: the blocks are read back without waiting for the graphics card (a pixel buffer and a fence,
        // looked at in later frames), the veil is made on a thread of its own (glareThread is called on it first: below normal priority), and until it is done
        // the same picture is shown without the glare. A newer picture drops an older glare. false: all of it at once in this frame (automatic checks).
        bool glareAsync = false;
        void (*glareThread)() = nullptr;
    };
    // 0.55.4: the last glare made (tests and the benchmark): the block in pixels, the grid, the read-back and the veil's time (ms), the light spread and laid (white x sr)
    // 0.55.5: waitMs: from the picture to its glare (asynchronous: over the frames between; at once: readMs + veilMs); frameMs: the frame thread's own share
    struct GlareStats { int b = 0, nx = 0, ny = 0; double readMs = 0, veilMs = 0, spread = 0, laid = 0; long made = 0; double waitMs = 0, frameMs = 0; };
    const GlareStats& glareStats() const { return glareStats_; }
    void lensSky(const LensSky& s);
    long lensRedraws() const { return lensRedraws_; }
    bool glareBusy() const { return glareStage_ != 0 || (glareWant_.gen != 0 && !glareValid_); }   // 0.55.5: an asynchronous glare is under way   // how many times the bent sky was drawn anew (the rest of the frames reused it)

private:
    enum class Kind { Triangles, Squares, Planet, Blobs, LayerBegin, LayerEnd, RocksGpu, Mesh, CloudGpu, Lens };
    struct Segment { Kind kind; int first, count; float size; Color c; float K[9]; int map; int mode; float r; int nring; float ring[96]; float occ[16]; int nocc; float sunAng; bool air; float shape[2]; bool additive; PhasePalette pal; int slot; bool spheres = false; bool hdr = false; bool lunar = false; RockFrame rf; MeshProjection mp; };   // draw calls, in the order they were added
    void closeTriangles();
    std::vector<Segment> segments_;
    std::vector<float> inst_;                 // square centres for all square draws of this frame
    std::size_t triStart_ = 0;
    GLuint sqProgram_ = 0, sqVao_ = 0, sqCornerVbo_ = 0, sqInstVbo_ = 0;
    GLint sqSize_ = -1, sqSide_ = -1, sqColor_ = -1;
    GLuint plProgram_ = 0, tex_[static_cast<int>(Map::Count)] = {};
    int texW_[static_cast<int>(Map::Count)] = {};
    GLint plSize_ = -1, plK_ = -1, plMode_ = -1, plLod_ = -1, plTex_ = -1, plLon0_ = -1, plRingN_ = -1, plRingB_ = -1, plShape_ = -1;
    GLint plOccN_ = -1, plOcc_ = -1, plSunAng_ = -1, plAir_ = -1, plAirT_ = -1, plAirK_ = -1;   // 0.51.2: eclipses and Earth's air

    IGroupCompute* compute_ = nullptr;        // owned; see compute()
    IOutlineCompute* outline_ = nullptr;      // owned; see outlineCompute()
    IRockCompute* rocks_ = nullptr;           // owned; see rockCompute()
    struct Mesh { GLuint vbo = 0, vao = 0; int count = 0; bool used = false; };
    std::vector<Mesh> meshes_;
    GLuint meshProgram_ = 0;
    GLint mSize_ = -1, mRot_ = -1, mCen_ = -1, mFit_ = -1;
    long meshUploads_ = 0;
    std::vector<Blob> blobs_;                 // all blobs of this frame
    GLuint blProgram_ = 0, blVao_ = 0, blInstVbo_ = 0;
    GLint blSph_ = -1, blSphN_ = -1;
    GLint blSize_ = -1, blFuzz_ = -1, blPalWheel_ = -1, blPalLight_ = -1, blPalPlus_ = -1, blPalMinus_ = -1;
    GLuint layerFbo_ = 0, layerTex_ = 0, cmpProgram_ = 0, cmpVao_ = 0;
    GLint cmpTex_ = -1;
    int layerW_ = 0, layerH_ = 0;
    std::vector<CloudGpuDraw> clouds_;              // 0.46: this frame's GPU clouds (Segment::slot)
    GLuint clProgram_ = 0, clVao_ = 0, clPosVbo_ = 0, clGrpVbo_ = 0, grProgram_ = 0, grVao_ = 0, grFbo_ = 0, grTex_ = 0;
    GLint clSize_ = -1, clCam0_ = -1, clCam1_ = -1, clView_ = -1, clLight_ = -1, clPalWheel_ = -1, clPalLight_ = -1, clPalPlus_ = -1, clPalMinus_ = -1, clFuzz_ = -1, clSphN_ = -1;
    GLint grCam0_ = -1, grCam1_ = -1, grView_ = -1, grGrid_ = -1;
    std::uint64_t clEpoch_ = 0; long clK_ = -1, clCap_ = 0; long clBytes_ = 0;
    int grW_ = 0, grH_ = 0;
    std::vector<float> grCells_;
    GLuint hdrFbo_ = 0, hdrTex_ = 0;                // 0.40: the cloud's float picture
    int hdrW_ = 0, hdrH_ = 0;
    float layerScale_ = 0.5f;
    int lastCalls_ = 0, lastInstances_ = 0;
    long lastVerts_ = 0;
    bool gray_ = false;
    Color grayBg_{0, 0, 0, 1};
    GLuint grayFbo_ = 0, grayTex_ = 0, grayProgram_ = 0;
    GLint grayTexLoc_ = -1;
    int grayW_ = 0, grayH_ = 0;

    // 0.55.0: the bent sky (lensSky)
    std::vector<LensSky> lens_;                     // this frame's (Segment::slot)
    GLuint lensProgram_ = 0, lensFbo_ = 0, lensTex_ = 0, lensTableTex_ = 0, discUTex_ = 0, discPTex_ = 0, discCTex_ = 0;
    GLuint kerrProgram_ = 0, kATex_ = 0, kBTex_ = 0, kETex_ = 0, kTTex_ = 0, kFTex_ = 0;   // 0.55.2
    GLint krTex_ = -1, krA_ = -1, krB_ = -1, krE_ = -1, krT_ = -1, krDC_ = -1, krDF_ = -1, krKs_ = -1, krCentre_ = -1, krKpx_ = -1, krKN_ = -1, krKS_ = -1, krKO_ = -1,
          krDebug_ = -1, krBright_ = -1, krSky_ = -1, krDisc_ = -1, krDR_ = -1, krDT_ = -1, krDN_ = -1, krDFix_ = -1;
    GLuint glowDown_ = 0, glowBlur_ = 0, glowAdd_ = 0, glowFbo_ = 0, glowTex_[2] = {0, 0};   // 0.55.3: the Film look's glow (LensSky::glow)
    int glowW_ = 0, glowH_ = 0;
    bool glowValid_ = false;
    void makeGlow(const LensSky& L, int fbW, int fbH);
    GLuint bhLayer_[2] = {0, 0};                   // 0.55.4: the disc's light and the sky behind it (float), lensFbo_'s attachments 1 and 2 while a glow or the glare is made
    int layerTexW_ = 0, layerTexH_ = 0;
    GLuint glareDown_ = 0, glareAdd_ = 0, glareFbo_ = 0, glareSrcTex_ = 0, glareVeilTex_ = 0;
    int glareNx_ = 0, glareNy_ = 0, glareB_ = 1;
    bool glareValid_ = false;
    GlareStats glareStats_;
    void makeGlare(const LensSky& L, int fbW, int fbH);
    // 0.55.5: the asynchronous glare (LensSky::glareAsync): the newest picture's request (gen 0: none), the stage (0 idle, 4 the picture, 1 its blocks, 3 read back under way,
    // 2 the veil on its thread), the job under way, the pixel buffer and fence, the thread and its buffers
    struct GlareJob { int fbW = 0, fbH = 0, b = 1, nx = 0, ny = 0; double cx = 0, cy = 0, K = 0; std::uint64_t gen = 0;
                      void (*veil)(int, int, double, double, double, const std::vector<float>&, std::vector<float>&, double*) = nullptr; void (*lower)() = nullptr; };
    void glareDownPass(int fbW, int fbH, int b, int nx, int ny);
    void glareUpload(int nx, int ny, const std::vector<float>& veil);
    void glareWant(const LensSky& L, int fbW, int fbH);
    void glarePump();
    void glareStopThread();
    GlareJob glareWant_, glareJob_;
    std::uint64_t glareGen_ = 0;
    int glareStage_ = 0;
    GLuint glarePbo_ = 0;
    GLsync glareFence_ = nullptr;
    std::thread glareThread_;
    std::atomic<bool> glareThreadDone_{false};
    std::vector<float> glareSrcBuf_, glareVeilBuf_;
    double glareOut_[3] = {0, 0, 0};
    std::chrono::steady_clock::time_point glareT0_;
    std::uint64_t kVer_ = ~std::uint64_t(0), kFVer_ = ~std::uint64_t(0);
    GLint lnDisc_ = -1, lnDU_ = -1, lnDP_ = -1, lnDC_ = -1, lnDN_ = -1, lnDS_ = -1, lnDA_ = -1, lnDR_ = -1, lnDT_ = -1, lnSky_ = -1;
    std::uint64_t discVer_ = ~std::uint64_t(0), colVer_ = ~std::uint64_t(0);
    GLint lnTex_ = -1, lnTable_ = -1, lnK_ = -1, lnCentre_ = -1, lnKpx_ = -1, lnEdge_ = -1, lnT_ = -1, lnN_ = -1, lnBend_ = -1, lnDebug_ = -1, lnBright_ = -1, lnLod_ = -1;
    int lensW_ = 0, lensH_ = 0, lensN_ = 0;
    std::uint64_t lensTableVer_ = ~std::uint64_t(0);
    LensSky lensLast_{};
    double lensPtW_ = 0;
    bool lensValid_ = false;
    long lensRedraws_ = 0;
    void drawLens(const LensSky& L, int fbW, int fbH, GLuint target, int vx, int vy);

    struct Vertex { float x, y, u, v, r, g, b, a, mode, p1, p2, p3; };
    void quad(double cx, double cy, double half, Color c, float mode, float p1, float p2, float uvScale, float p3 = 0);
    void push(double x, double y, float u, float v, Color c, float mode, float p1 = 0, float p2 = 0, float p3 = 0);

    std::vector<Vertex> verts_;
    double width_ = 1, height_ = 1;
    GLuint program_ = 0, vao_ = 0, vbo_ = 0;
    GLint uSize_ = -1, uSpace_ = -1, plSpace_ = -1, plLunar_ = -1;
    Color space_{0, 0, 0, 1};
};

}  // namespace render
