// A small batched 2D renderer (0.69.0: drawn by Vulkan only: render/renderer2d_vk.cpp, render/vk_draw2d.h).
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
#include <string>
#include <thread>
#include <vector>

#include "group_compute.h"
#include "ring_card.h"
#include "classic_card.h"
#include "nucdens_card.h"
#include "lines_card.h"
#include "surface_card.h"
#include "mesh.h"
#include "outline_compute.h"
#include "rock_compute.h"
#include "kerr_compute.h"
#include "kerr_sky.h"
#include "atom_card.h"
#include "molecule_card.h"
#include "porkchop_card.h"

namespace render {

class VkDraw2D;   // 0.60.0: render/vk_draw2d.h (Vulkan, step 2)
class VkCards;    // 0.61.0: render/vk_card.h (Vulkan, step 3)
class VkGlow;
class VkBlackHole;   // 0.62.0: render/vk_blackhole.h (Vulkan, step 4)

struct Color {
    float r = 1, g = 1, b = 1, a = 1;
    Color withAlpha(float alpha) const { return {r, g, b, alpha}; }
};
Color hexColor(const char* hex, float alpha = 1);   // "#rrggbb"
constexpr float kBlobPalSquare = 5, kBlobPalSoft = 6;   // Blob::kind of a square / soft dot with a colour picked by the shader (see Renderer2D::blobsPalette)
Color rgba(int r, int g, int b, float a = 1);

class Renderer2D {
public:
    // The card's compute path (0.13 stage 2; 0.61.0: Vulkan's, render/vk_card.h), or nullptr when this start has none (then the processor does that work).
    // Valid after useVulkanCards().
    IGroupCompute* compute() const { return compute_; }
    IOutlineCompute* outlineCompute() const { return outline_; }   // the same for the 90 % outline (0.13 stage 3)
    IRockCompute* rockCompute() const { return rocks_; }           // the same for the Orbit Lab's belt rocks (0.13 stage 4)
    void useRockCompute(IRockCompute* r) { delete rocks_; rocks_ = r; }   // tests: put a stand-in (or nullptr) in place of the card's object; the renderer owns it
    void disableCompute();                // forget the compute object: the CPU paths are used (PL_NO_COMPUTE; tests)
    // 0.55.6: the Black Hole Lab's light paths on the card (render/kerr_compute.h), made at the first call; nullptr when this PC cannot (kerrWhy() says why)
    IKerrCompute* kerrCompute(KerrSteps* steps = nullptr);   // 0.55.7: steps told of the compile and link (the guard: app/gpu_guard.h)
    const std::string& kerrWhy() const { return kerrWhy_; }
    // 0.56.0: the spinning hole per pixel (render/kerr_sky.h): the object that makes its shaders (the caller builds the stages, guarded); always there
    KerrSky& kerrSky() { return sky_; }
    PorkchopCard& porkchopCard();                                    // 0.92.0: the porkchop grid on the card (made at the first call)
    AtomCard& atomCard();                                            // 0.57.0: the Atom Lab's Slice dots and jump groups on the card (made at the first call)
    MoleculeCard& moleculeCard();                                    // 0.57.1: the Molecule Lab's Density cloud on the card (made at the first call)
    // 0.64.0: Saturn's rings per pixel on the card (nullptr without the card programs); ringsCard records one half of the rings
    // once its check passed (ready): false: not drawn (the caller draws the processor's triangles)
    RingCard* ringCard();
    bool ringsCard(const RingsDraw& d);
    // 0.64.0: the Classic Cloud 3D dots from the card (Vulkan only: nullptr otherwise); cloudClassic records them once its check passed (ready): false: not
    // drawn (the caller makes the processor's squares)
    ClassicCard* classicCard();
    bool cloudClassic(const ClassicDraw& d);
    // 0.67.0: the nucleus' Density look from the card (Vulkan only: nullptr otherwise); nucDensity records it once its check passed (ready): false: not drawn
    // (the caller makes the processor's squares). nucDensRuns: how many times its steps ran (they run only when something changed)
    NucDensCard* nucDensCard();
    bool nucDensity(const NucDensDraw& d);
    // 0.68.0: the Orbit Lab's trails, orbits and distance rings from the card (Vulkan only: nullptr otherwise); linesProgram makes the drawing program (the
    // guard's pipeline step, with the card's steps); lines records one set of runs once its check passed (ready): false: not drawn (the caller's LinePen)
    LinesCard* linesCard();
    bool linesProgram(std::string& why);
    bool lines(const LinesDraw& d);
    // 0.68.0: the Molecule Lab's Density surface from the card (Vulkan only: nullptr otherwise); surfaceProgram makes its drawing program; surface records it
    // once its check passed (ready): false: not drawn (the caller's triangles)
    SurfaceCard* surfaceCard();
    bool surfaceProgram(std::string& why);
    bool surface(const SurfaceDraw& d);
    long nucDensRuns() const;
    // 0.56.0: the last sky pass: its time on the card (timestamp pairs, read back without waiting: a few frames old) for the whole picture and at the
    // resolution drawn, the resolution's scale (1 = full), the passes drawn, the card's exposure (its brightest disc channel and Y, read back a frame or two
    // later; white the screen's white it gives, 0 none yet) and how many sample points it had
    struct SkyStats { double fullMs = -1, lastMs = -1, scale = 1; long draws = 0, lowDraws = 0; double white = 0, whiteY = 0, logAvg = 0; long whiteReads = 0; int expN = 0; double expMs = -1; long timed = 0; };
    const SkyStats& skyStats() const { return skyStats_; }
    // 0.56.0: one star image laid over the bent picture through its open-sky share (LensSky::kerrSky's fourth output): hidden where the disc or the shadow is
    void maskedDisc(double x, double y, double r, Color c);

    // 0.60.0 (Vulkan, step 2 of 5): the same recording, drawn by Vulkan at the end (render/renderer2d_vk.cpp, render/vk_draw2d.h). useVulkan before
    // anything is recorded (without it every card program is absent and each lab takes its processor path); loadMapsVulkan: the picture maps;
    // endVulkan: the frame's recording drawn into Vulkan's picture (fbW x fbH pixels, cleared to clear[]; forShot: Save image's own picture, one sample).
    // The frame's command buffer is the backend's (render/vk_backend.cpp). Vulkan is the only drawing system.
    void useVulkan(VkDraw2D* d);                       // (render/renderer2d_vk.cpp: renderer2d.cpp itself needs no Vulkan to link)
    bool vulkan() const { return vk_ != nullptr; }
    bool loadMapsVulkan(std::string& why);
    // 0.61.0 (Vulkan, step 3 of 5): the card programs on Vulkan (render/vk_card.h), made here, each under its own guard (app/gpu_guard.h: groups,
    // outline, rocks and the Glow dots at once; the Slice, the jump and the Density cloud by their labs' paths): compute(), outlineCompute(),
    // rockCompute(), cloudGpuOk(), atomCard() and moleculeCard() then give Vulkan's objects. why: a line for each program that could not be made.
    // null: every Vulkan object of the card programs deleted (before the Vulkan device goes)
    void useVulkanCards(VkCards* c, std::vector<std::string>* why = nullptr);
    // this start has the card's compute programs (Vulkan's) and they are not switched off (PL_NO_COMPUTE): the labs' card paths ask this
    bool computeShaders() const { return vk_ && vkc_ != nullptr && !noCompute_; }
    const std::string& rocksWhy() const { return rocksWhy_; }   // 0.61.0: why the rocks are not on the card ("" when they are)
    void endVulkan(int fbW, int fbH, const float clear[4], bool forShot);
    // 0.60.0 (tests: the coverage of the views): how many times each drawing call was made since the start, "rect=12 line=40 ..." (PL_LOG_DRAWKINDS)
    std::string drawKindsText() const;

    // Start a frame for a picture `width` × `height` points.
    void begin(double width, double height);
    // 0.67.0: instead of begin() and the drawing: the last frame's picture again (the Orbit Lab at rest). Its lists are drawn again as they are; on Vulkan the
    // window's kept picture is shown with nothing drawn when it is whole and of the same size (Save image and the screenshot draw it from the lists).
    void keepLast() { keep_ = true; }
    bool keeping() const { return keep_; }

    void rect(double x, double y, double w, double h, Color c);
    void line(double x0, double y0, double x1, double y1, double width, Color c);
    void triangle(double x0, double y0, Color c0, double x1, double y1, Color c1, double x2, double y2, Color c2);
    void disc(double x, double y, double r, Color c);                       // flat round dot, smooth edge
    void ring(double x, double y, double r, double width, Color c);         // circle outline
    void sphere(double x, double y, double r, Color c, double lightX, double lightY, double lightZ, double lightK = 1.0);  // a ball lit from the unit direction (right, down, towards you) on screen; 0.85.0: lightK the sunlight factor (PlanetLight::k)
    // 0.85.0 (R2, R3): the Sun as a ball (kPlanetFS uMode 4): its edge by Neckel & Labs 1994 (render/sun_limb.h), its disc centre the screen's white. K turns screen
    // directions into the Sun's own frame (IAU: rows its 0 meridian, 90 E, north pole), or nullptr (the screen frame). spots: n groups, 4 floats each (unit vector
    // in the Sun's frame, angular radius in radians), drawn as outlines; at most kMaxSunSpots.
    void sunBall(double x, double y, double r, const float* K, const float* spots, int nSpots);
    static constexpr int kMaxSunSpots = 64;
    void sunDisc(double x, double y, double r, Color c);                    // the Sun: brighter middle, darker edge
    void fadeDisc(double x, double y, double r, Color c);                   // a soft dot: full colour in the middle, fading to nothing at radius r

    // Many small squares of one colour, drawn with ONE instanced draw call (used for the belts'
    // thousands of rocks). xy holds the squares' centres, 2 floats each, in picture points.
    void squares(const float* xy, int count, double size, Color c);
    // The same squares, but their places are worked out on the GPU (0.13 stage 4 part A, render/rock_compute.h): sends the rocks if this set is not on the GPU yet, and in endVulkan() runs the compute
    // shader for the camera `f` and draws from the buffer it wrote (no CPU wait, no per-rock CPU work). Returns false, adding nothing, when the card cannot (the caller then uses squares()).
    bool rocksGpu(const RockSet& set, const RockFrame& f, double size, Color c);
    // 0.78.0: a comet's dust grains, the same way: the grains of `slot` (IRockCompute::grainSlot, already written) placed by kGrainsCS for `f` and drawn as squares.
    bool grainsGpu(int slot, int count, const RockFrame& f, double size, Color c);

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
    // 0.66.0: blobs of kind 10, glow stacks: each is up to kMaxGlow glows round its centre, added up in one quad (a nucleon's soft glows were a blob each,
    // every one a quad, at full zoom each over the whole picture). table: n x (radius / r, relative strength, 1 for kind 1's profile or 0 for kind 4's
    // Gaussian, 0); a blob's c0 is the colour and the Gaussians' strength, fx the profile's strength, fy how far its quad reaches (units of r, at most 1).
    static constexpr int kMaxGlow = 16;
    void glowStacks(const Blob* b, int count, bool additive, const float* table, int n);
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
    const std::vector<Blob>& pendingBlobs() const { return blobs_; }          // what endVulkan() would draw as blobs (for tests)
    std::size_t pendingSegments() const { return segments_.size(); }          // the draw calls so far (the open run of triangles is not counted until it is closed)
    // A retained mesh (0.13 stage 4 part B, render/mesh.h): uploaded once with meshCreate (needs Vulkan: -1 without it), drawn from the GPU buffer in every frame that asks for it with drawMesh, in the order
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
    // (base x exposure x 12,000 / dots), or a negative number for "draw nothing" (as glowBlobs returning false); it runs inside endVulkan().
    struct GlowNearDots { std::vector<int> idx; std::vector<float> xyz; };   // 0.90.0: the dots kept, in increasing number, and their places (x, y, z)
    struct CloudGpuDraw {
        const float* pts = nullptr; long cnt = 0, cap = 0;          // places (m, x y z), dots shown (0 .. cnt), the ring's size
        std::uint64_t epoch = 0; long k = 0;                        // which cloud (a new number when its dots were changed other than by being added) and dots recorded so far
        const unsigned char* grp = nullptr;                         // colour group of each dot (255: not shown), cnt of them
        unsigned grpBuf = 0;                                        // 0.57.0: or a buffer on the card that already holds them (a jump's groups made there: AtomCard::jumpGroups)
        std::uint64_t grpEpoch = 0;                                 // 0.65.0: the groups' epoch (atom/cloud_dots.h keepGroups): unchanged since the last frame: not sent again (Vulkan); 0: sent
        float cx = 0, cy = 0, pxm = 0, ext = 1, ca = 1, sa = 0, cb = 1, sb = 0, w = 0, h = 0;
        bool additive = true;
        PhasePalette pal;
        std::function<double(const std::vector<float>&)> exposure;
        // 0.90.0: the Glow core's dots found on the card (kGlowNearCS, atom/cloud_dots.h glowDotsNear's quick pass at nearPass): nearFound gets the
        // numbers of the dots it kept, in increasing order, with their places then, before exposure runs (null: the card could not: the caller's own
        // pass over the dots of this frame). With the grid of the frame before (Part C of 0.90.0) it is that frame's nearFound, with that frame's dots.
        bool near = false;
        float nearPass[9] = {1, 0, 1, 0, 0, 0, 0, 0, 0};             // GlowNearPass: ca, sa, cb, sb, k4, pxm, dx0, dy0, lim
        std::function<void(const GlowNearDots*)> nearFound;
    };
    bool cloudGpuOk() const { return vk_ && vkGlow_ != nullptr; }
    bool cloudNearGpuOk() const;                                    // 0.90.0: the Glow core's dots can be found on the card
    // 0.57.1: the Molecule Lab's Density cloud from the card (render/molecule_card.h): its dots made, summed into the exposure's grid and drawn there, in the
    // order of the other shapes; the same picture as glowBlobs + blobsPaletteTake up to float rounding. False (nothing added): the card's programs are not ready.
    bool molCloud(const MolCloudDraw& d);
    void cloudGpu(const CloudGpuDraw& d);
    long cloudBytesSent() const { return clBytes_; }                           // bytes of dot places sent to the GPU so far (for tests: a still cloud sends nothing)

    // Monochrome theme: the whole picture in shades of grey
    void setGrayscale(bool on, Color bg) { gray_ = on; grayBg_ = bg; }   // bg: the page colour under the picture

    // What the last endVulkan() drew: draw calls, and shapes drawn by the instanced calls (squares and blobs)
    int lastDrawCalls() const { return lastCalls_; }
    int lastInstances() const { return lastInstances_; }
    long lastVertices() const { return lastVerts_; }                          // the triangle vertices endVulkan() uploaded (lines, rings, discs, text-free shapes)
    std::size_t pendingVertexBytes() const { return verts_.size() * sizeof(Vertex); }   // what endVulkan() would upload for the shapes added so far (a measuring aid for tests and tools)

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
    // 0.85.0 (R1): k, the sunlight factor: the sunlight at the body over that at the exposure's body, (d_exposure / d)^2 (orbit/true_light.h); 1 = even lighting.
    struct PlanetLight { float occ[16] = {}; int nOcc = 0; float sunAng = 0; bool air = false; float k = 1; };
    void planet(double x, double y, double r, Color base, double lightX, double lightY, double lightZ, const float K[9], Map map, const float* rings = nullptr, int nRings = 0,
                double shapeY = 1.0, double shapeZ = 1.0, bool lunar = false, const PlanetLight* light = nullptr);   // lunar: McEwen's lunar function (render/shading.h), not Lambert
    static constexpr int kMaxRingBands = 32;   // 0.51.2: ring bands that shadow a ball (was 8): Saturn's from Cassini's profile
    // 0.26: the Gaia all-sky map behind everything, over the whole W x H picture. kRad: points per radian from the
    // middle (the night sky's map, orbit/sky.cpp); M turns a direction in the view frame (right, up, depth) into
    // galactic coordinates (rows: towards l = 0, l = 90°, the north galactic pole); bright: 0..1.
    void skyMap(double W, double H, double kRad, const float M[9], double bright);
    // 0.75.0: the same photo through the Orbit Lab's 3D pinhole camera (orbit/camera.h): the pixel's direction is (x - ppx, -(y - ppy), -F) in the view frame
    void skyMapPinhole(double W, double H, double ppx, double ppy, double F, const float M[9], double bright);
    bool hasMap(Map m) const { return vkMap_[static_cast<int>(m)]; }
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
        bool debugGas = false;              // 0.72.0 (tests): the first crossing's phi, t and the moving gas's (factor, n) with no footprint (kLensFS uDebug 3)
        // 0.72.0: the moving gas (blackhole/gas.h; render/shaders.h kGasPart): gas on; gasT (unused, c, 1: the streaks on, the clump's A or 0), gasC (r_c, its
        // phase at the epoch, Omega_c, 1 / (2 R_S^2)), gasG (g_rr, g_phph + u_phi^2 at r_c, the spin, 0); 0.79.0, the streaks (gas.h StreakField; they replace
        // 0.72's patches): strkV (per level c00, cx, cy, cd), strkW (per level the covariance with the next), strkB (per clock bin f, K div 65536, K mod 65536, 0),
        // streak (the field's atlas, streakW x streakH; sent once); gasTau (the far time now since the epoch, M), gasCam (the camera's phi), gasE1, gasE2 (the sky
        // frame's x and y on the view frame: spin 0), timeR (the radii whose crossings get phi and t: the spinning hole), discTime (spin 0: the table's travel
        // times, as discU; disc.h DiscTable::tValues), gasVer (a new number with any change of them: a new picture)
        bool gas = false, gasNoFoot = false;   // 0.79.0: gasNoFoot (tests): the streaks read with no footprint (kLensFS uGasFoot 0)
        float gasT[4] = {0, 0, 0, 0}, gasC[4] = {0, 0, 0, 0}, gasG[4] = {0, 0, 0, 0};
        float strkV[32] = {}, strkW[8] = {}, strkB[68] = {};
        const std::vector<float>* streak = nullptr;
        int streakW = 0, streakH = 0;
        float gasTau = 0, gasCam = 0, gasE1[3] = {1, 0, 0}, gasE2[3] = {0, 1, 0}, timeR[2] = {0, 0};
        // 0.79.0: the disc's thickness at spin 0 (blackhole/disc.h HeightTable; kLensFS uHgt): its values, (ln r_0, d ln r, count, 1: drawn), a new number for a new table
        const std::vector<float>* hgt = nullptr;
        float hgtR[4] = {0, 1, 0, 0};
        std::uint64_t hgtVersion = 0;
        const std::vector<float>* discTime = nullptr;
        std::uint64_t gasVer = 0;
        const std::vector<float>* discU = nullptr;   // nT x (2 nS): rows side 0 then side 1
        const std::vector<float>* discP = nullptr;   // 2 nS
        const std::vector<float>* discCol = nullptr; // 3 nC
        int discS = 0, discT = 0, discC = 0;
        double discS0 = 0, discDs0 = 0, discDs1 = 0, discXc = 0.1;
        std::uint64_t discVersion = 0, colVersion = 0;
        float discAxis[3] = {0, 0, 1};      // the disc's axis on the view frame (right, up, towards the camera)
        double rO = 20, rIn = 6, rOut = 30, tScale = 0, white = 1, colLog0 = 0, colDlog = 1;
        float lp[4] = {0, 0, 0, 0};          // 0.58.0: the light check's pass (kKerrSkyFS uMode 11: corner x, y, width, pixels; set by skyProbe)
        double allInvTsun = 0;              // 0.58.0, All light (blackhole/lab.h allLightOn): 1 / T_sun; 0: the eye's light (the shaders' uAll)
        // 0.87.0 (blackhole/disc.h, THE DISC'S REAL LIGHT): the shaders' uDL (render/shaders.h kDiscLightPart): the colour correction's lowest temperature
        // (3e4 K; 1e30: off), unused, the gas's own temperature over tScale F^(1/4) (spin 0: 1 / the camera's own shift; else 1), unused
        float discLight[4] = {1e30f, 0, 1, 0};
        float limb[44] = {};                // 0.87.0 R10: blackhole/disc.h limbFine (uLimb, 11 x vec4: 41 nodes and 3 unused); every node 1 for off
        const std::vector<float>* retF = nullptr;   // 0.87.0 R11, spin 0 (kLensFS uRet): the returning flux against ln r (retN values from retLog0 in steps retDl)
        int retN = 0; double retLog0 = 0, retDl = 1; std::uint64_t retVersion = 0;
        // 0.55.2: the spinning hole (kKerrFS, blackhole/spin_sky.h SpinTable), used when kerr is true (then table, alphaSh and the disc's a = 0 table are not).
        // kA (dir, b) and kB (u0..u3): kS columns x 2 kChi rows; kEdge: kEdgeN edge samples; kTau: kEdgeN + 1 values of tau. kM: the chart's axes (camera at
        // phi = 0) to galactic, through the camera's phi and the view; kO: the screen directions (right, up) of e_theta and e_phi. discF: F^(1/4) against ln r
        // (discFN values from ln r = discFLog0 in steps discFDl) for the disc's temperature at this spin; discCol, white, tScale, rIn, rOut as above.
        bool kerr = false;
        float fido[2] = {1, 0};             // 0.84.0: the held camera's own shift for a ray's b, f = fido[0] - b fido[1] (kKerrFS, kKerrSkyFS uFido; (1, 0): none)
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
        // temperature and shift rule (the spinning hole's pass). (0.86.0: the Film look's own glow (0.55.3) and see-through disc (0.81.0) are gone, R6.)
        float discFix[4] = {0, 0, 0, 0};
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
        // 0.85.5: the tables the card's veil reads (blackhole/light.h glareTables: the near weights, the flat ratios, the eye's age and pigmentation); null:
        // the veil is made on the processor
        void (*glareTables)(int W, int H, double cx, double cy, double K, std::vector<float>& nearW, std::vector<float>& corr, double eye[2]) = nullptr;
        // 0.56.0: the spinning hole per pixel from the closed forms (render/shaders.h kKerrSkyFS) in place of kerr's table (kA, kB, kEdge, kTau unused; kM, kO,
        // spin, the disc's fields as for kerr). skyCam (a, r_c, cos theta_c, sin theta_c), skyMet (alpha, omega, varpi, rho), skyMet2 (Delta^(1/2), r+, r-, r+ + 1e-3):
        // the camera, made in double by the caller (blackhole/kerr.h kerrMetric). filmRaw: the Film's one colour before the screen's white (w > 0: Film).
        // whiteMode 0: the screen's white is `white`; 1: found by the card in the same frame (auto exposure): the brightest channel of the first disc image
        // at the points (expStep / 2 + i expStep, expStep / 2 + j expStep) of the picture in points (lab.h discLook's rule), reduced on the card, no read-back
        // in the frame's way. moving: the view is changing (the pass may be drawn at a lower resolution: frameBudgetMs, the rule in drawLens); slowMs > 0
        // (tests): the pass's time on the card taken as this. skyMode (tests): 1..4 the shader's probe outputs (uMode) into the float framebuffer given.
        // 0.56.2: lowRes: a lower resolution allowed while moving, and then only when the whole picture takes longer than sharpMs on the card (the owner's
        // decision, 17 Nov 2026: every pixel always, unless the card cannot give 60 sharp pictures a second).
        bool kerrSky = false;
        float skyCam[4] = {0, 20, 0, 1}, skyMet[4] = {1, 0, 1, 1}, skyMet2[4] = {1, 2, 0, 2};
        float filmRaw[4] = {0, 0, 0, 0};
        int whiteMode = 0;
        double expStep = 4;
        // 0.70.0 (blackhole/tone.h): tone 1: the Real look's camera curve (Reinhard et al. 2002 eq. (4)) on each pixel's luminance, its chromaticity kept, in
        // place of the screen's white; 0: the screen's white (the Film look, as before). The disc's light is scaled by 1 / white (k: L = k Y); toneLw: the
        // burn-out white in L. whiteMode 1 (the card's exposure, the same frame): k = toneKey 2^ev / the samples' log-average (delta = toneDelta x their
        // largest Y), toneLw = k x their largest Y; Film: white = their brightest channel / 2^ev. Where the card's samples hold no light, white and toneLw
        // (the processor's, or the last good) are used.
        int tone = 0;
        double ev = 0, toneLw = 0, toneKey = 0.18, toneDelta = 1.0 / 16384;
        bool moving = false;
        double frameBudgetMs = 8, slowMs = 0;
        bool lowRes = true;
        double sharpMs = 1000.0 / 60;
        int skyMode = 0;
    };
    // 0.55.4: the last glare made (tests and the benchmark): the block in pixels, the grid, the read-back and the veil's time (ms), the light spread and laid (white x sr)
    // 0.55.5: waitMs: from the picture to its glare (asynchronous: over the frames between; at once: readMs + veilMs); frameMs: the frame thread's own share
    struct GlareStats { int b = 0, nx = 0, ny = 0; double readMs = 0, veilMs = 0, spread = 0, laid = 0; long made = 0; double waitMs = 0, frameMs = 0; };
    const GlareStats& glareStats() const { return glareStats_; }
    void lensSky(const LensSky& s);
    // the kept bent picture still shows L (drawLens's rule: the view, the tables' versions, the looks; ksky: the sky per pixel would draw it now, skyDrawn:
    // it drew the kept one; width / lastPtW: the picture's width in points now and then). 0.62.0: shared with Vulkan's (render/vk_blackhole.cpp)
    static bool lensSameView(const LensSky& P, const LensSky& L, bool ksky, bool skyDrawn, double width, double lastPtW);
    // 0.56.0 (the start-up check and the tests): the sky pass's probe outputs (its uMode 1..4: far direction and x_px; order, r, g, x_px; end, b, q, case; u0..u3)
    // of a w x h picture of L (cx, cy, kRad in pixels), 16 floats per pixel, rows from the bottom. queue: copied into a pixel buffer behind a fence, read by
    // skyProbeRead once skyProbeDone (the frame never waits); else read at once into out (waits)
    void skyProbe(const LensSky& L, int w, int h, bool queue, std::vector<float>* out);
    bool skyProbeDone();
    // (the Black Hole Lab's test hooks, PL_BH_COMPARE): one picture's float outputs (LensSky::debug, debugDisc, skyMode) of
    // w x h pixels (w x h points), 4 floats a pixel, rows from the bottom, waited for; the sky shader's elliptic functions alone (render/kerr_sky.h's sets
    // of 12 floats in, 8 out, waited for); the exposure's read-back of the last picture, waited for (0: none). render/vk_blackhole.h has them.
    void lensFloat(const LensSky& L, int w, int h, std::vector<float>& out);
    void skyMath(const std::vector<float>& in, std::vector<float>& out);
    // 0.85.5: the eye's glare veil made on the card from the blocks src (VkBlackHole::veilTest), waited for; empty: not made (no Vulkan, or its programs failed)
    void glareVeilCard(int W, int H, double cx, double cy, double K, int b, const std::vector<float>& src, std::vector<float>& out,
                       void (*tables)(int, int, double, double, double, std::vector<float>&, std::vector<float>&, double[2]));
    double skyWhiteWait();
    // 0.85.3: the lens pass's pipeline (render/vk_blackhole.h lensState): 2 made, -1 failed, 0 or 1 not yet. 2 without Vulkan's Black Hole programs.
    int skyLensState() const;
    void skyProbeRead(std::vector<float>& out);
    long lensRedraws() const { return lensRedraws_; }
    bool glareBusy() const { return vk_ && vkGlareBusy_; }   // (0.62.0: Vulkan's mirrored each frame)   // 0.55.5: an asynchronous glare is under way   // how many times the bent sky was drawn anew (the rest of the frames reused it)

private:
    enum class Kind { Triangles, Squares, Planet, Blobs, LayerBegin, LayerEnd, RocksGpu, Mesh, CloudGpu, Lens, MolCloud, Rings, CloudClassic, NucDens, Lines, Surface };
    struct Segment { Kind kind; int first, count; float size; Color c; float K[9]; int map; int mode; float r; int nring; float ring[96]; float occ[16]; int nocc; float sunAng; bool air; float shape[2]; bool additive; PhasePalette pal; int slot; bool spheres = false; int glowN = 0; float glow[4 * kMaxGlow]; bool hdr = false; bool lunar = false; float albedoK = 1.0f; float lightK = 1.0f; int spotFirst = 0, spotN = 0; RockFrame rf; MeshProjection mp; bool grains = false; };   // draw calls, in the order they were added
    void closeTriangles();
    std::vector<Segment> segments_;
    std::vector<float> sunSpots_;             // 0.85.0: this frame's sunspot outlines (Segment::spotFirst, spotN)
    std::vector<float> inst_;                 // square centres for all square draws of this frame
    std::size_t triStart_ = 0;
    int texW_[static_cast<int>(Map::Count)] = {};    // the maps' widths (the sky's: the Black Hole Lab's lens)

    IGroupCompute* compute_ = nullptr;        // owned; see compute()
    IOutlineCompute* outline_ = nullptr;      // owned; see outlineCompute()
    IRockCompute* rocks_ = nullptr;           // owned; see rockCompute()
    IKerrCompute* kerr_ = nullptr;            // owned; see kerrCompute()
    KerrSky sky_;                             // 0.56.0: the spinning hole per pixel (its stages made by Vulkan's side)
    PorkchopCard* porkCard_ = nullptr;       // 0.92.0: owned
    PorkchopCard* (*vkPorkCard_)(VkCards&) = nullptr;
    AtomCard* atomCard_ = nullptr;           // owned (a pointer: the classes that hold a renderer without its code need no AtomCard)
    MoleculeCard* molCard_ = nullptr;        // 0.57.1: owned, the same way
    std::vector<MolCloudDraw> molClouds_;    // this frame's (Segment::slot)
    bool maskValid_ = false;
    SkyStats skyStats_;
    bool kerrTried_ = false;
    std::string kerrWhy_;
    struct Mesh { int count = 0; bool used = false; int vk = -1; };   // vk: its buffer on Vulkan (VkDraw2D's id)
    std::vector<Mesh> meshes_;
    long meshUploads_ = 0;
    std::vector<Blob> blobs_;                 // all blobs of this frame
    std::vector<CloudGpuDraw> clouds_;              // 0.46: this frame's GPU clouds (Segment::slot)
    long clBytes_ = 0;
    float layerScale_ = 0.5f;
    int lastCalls_ = 0, lastInstances_ = 0;
    long lastVerts_ = 0;
    bool gray_ = false;
    bool keep_ = false;                            // 0.67.0: keepLast() this frame
    Color grayBg_{0, 0, 0, 1};

    // 0.55.0: the bent sky (lensSky)
    std::vector<LensSky> lens_;                     // this frame's (Segment::slot)
    GlareStats glareStats_;
    long lensRedraws_ = 0;

    struct Vertex { float x, y, u, v, r, g, b, a, mode, p1, p2, p3; };
    void quad(double cx, double cy, double half, Color c, float mode, float p1, float p2, float uvScale, float p3 = 0);
    void push(double x, double y, float u, float v, Color c, float mode, float p1 = 0, float p2 = 0, float p3 = 0);

    std::vector<Vertex> verts_;
    double width_ = 1, height_ = 1;
    Color space_{0, 0, 0, 1};
    VkDraw2D* vk_ = nullptr;                        // 0.60.0: Vulkan draws the picture (not owned)
    VkCards* vkc_ = nullptr;                        // 0.61.0: the card programs on Vulkan (not owned; their objects below are)
    VkGlow* vkGlow_ = nullptr;
    VkBlackHole* vkbh_ = nullptr;                   // 0.62.0 (step 4): the Black Hole Lab's programs on Vulkan (owned); its Vulkan halves of the calls below
    void (*vkProbe_)(Renderer2D&, const LensSky&, int, int, bool, std::vector<float>*) = nullptr;
    bool (*vkProbeDone_)(Renderer2D&) = nullptr;
    void (*vkProbeRead_)(Renderer2D&, std::vector<float>&) = nullptr;
    IKerrCompute* (*vkKerr_)(Renderer2D&, KerrSteps*) = nullptr;
    bool vkGlareBusy_ = false;
    AtomCard* (*vkAtomCard_)(VkCards&) = nullptr;
    MoleculeCard* (*vkMolCard_)(VkCards&) = nullptr;
    RingCard* (*vkRingCard_)(VkCards&) = nullptr;   // 0.64.0
    RingCard* ringCard_ = nullptr;                  // 0.64.0: owned
    ClassicCard* (*vkClassicCard_)(VkCards&) = nullptr;
    ClassicCard* classicCard_ = nullptr;            // 0.64.0: owned
    std::vector<ClassicDraw> classicDraws_;         // this frame's (Segment::slot)
    std::vector<char> classicOk_;                   // recorded this frame (0.67.0: and the nucleus' Density look's)
    NucDensCard* (*vkNucDensCard_)(VkCards&) = nullptr;   // 0.67.0
    NucDensCard* nucDensCard_ = nullptr;            // 0.67.0: owned
    std::vector<NucDensDraw> nucDraws_;             // this frame's (Segment::slot)
    LinesCard* (*vkLinesCard_)(VkCards&) = nullptr;   // 0.68.0
    bool (*vkAddProgram_)(VkDraw2D*, int, std::string&) = nullptr;   // 0.68.0: VkDraw2D::addProgram (render/renderer2d_vk.cpp)
    LinesCard* linesCard_ = nullptr;                // 0.68.0: owned
    std::vector<LinesDraw> linesDraws_;             // this frame's (Segment::slot: also the card's buffers of that draw)
    SurfaceCard* (*vkSurfaceCard_)(VkCards&) = nullptr;   // 0.68.0
    SurfaceCard* surfaceCard_ = nullptr;            // 0.68.0: owned
    std::vector<SurfaceDraw> surfaceDraws_;         // this frame's (Segment::slot)
    std::vector<RingsDraw> ringDraws_;              // this frame's (Segment::slot)
    void (*vkDropCards_)(Renderer2D&) = nullptr;
    bool noCompute_ = false;
    std::string rocksWhy_;
    std::vector<int> vkGrid_;                       // 0.61.0: this frame's grids (by segment), their lights
    std::vector<double> vkLight_;
    // 0.90.0: the grids of the frame before (the Glow and Density clouds, by kind and number in the frame): this frame's exposure is worked out from them,
    // so the frame does not wait for its own grid (renderer2d_vk.cpp vkCardsFrame)
    struct PrevGrid { int kind = 0, nth = 0, index = -1, gw = 0, gh = 0, nearBank = -1; std::uint64_t ticket = 0; long frame = -1; CloudGpuDraw draw; };
    std::vector<PrevGrid> prevGrids_;
    long cardsFrame_ = 0;
public:
    struct GridStats { long late = 0, waited = 0, own = 0; double waitMs = 0; };   // frames' grids: from the frame before, waited for (that frame's batch not done yet), the frame's own (waited)
    const GridStats& gridStats() const { return gridStats_; }
private:
    GridStats gridStats_;
    std::vector<char> vkRocks_;
    bool vkCardsFrame(bool forShot, int fbW, int fbH);
    bool vkMap_[static_cast<int>(Map::Count)] = {};
    enum DrawKind { kdRect, kdLine, kdTriangle, kdDisc, kdRing, kdSphere, kdSunDisc, kdFadeDisc, kdMaskedDisc, kdSquares, kdBlobs, kdSpheres, kdMesh, kdSoftLayer,
                    kdCloudLayer, kdPlanet, kdPlanetMap, kdPlanetRings, kdPlanetEclipse, kdPlanetAir, kdSkyMap, kdGray, kdBlobKind0, kdKindCount = kdBlobKind0 + 11 };   // (0.66.0: blob kinds 0 .. 10)
    mutable long kinds_[kdKindCount] = {};
    int (*vkMeshCreate_)(VkDraw2D*, const void*, std::size_t) = nullptr;   // 0.60.0: set by useVulkan (render/renderer2d_vk.cpp)
    void (*vkMeshDelete_)(VkDraw2D*, int) = nullptr;
    void kindOf(const Blob& b) const { const int k = static_cast<int>(b.kind + 0.5f); if (k >= 0 && k < 11) kinds_[kdBlobKind0 + k]++; }
};

}  // namespace render
