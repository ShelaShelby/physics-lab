// 0.68.0: the Orbit Lab's trails, orbits and distance rings on the graphics card (Vulkan; render/shaders.h kLinesCS and kLinesVS). The points stay on
// the card (a trail as a ring that grows at its head and loses its tail; an orbit's 257 corners and a distance ring's 97, sent again only when they
// changed), each as a high and a low float of the processor's double; the card projects the pieces about a reference point near the view, makes the
// long pieces near the picture into curves as the processor does (orbit/orbit_lines.h), clips and widens them, and draws them with kFragmentShader.
// The processor's LinePen stays the reference and the fallback. Made and checked by orbit/lines_check.cpp under its guard
// (app/gpu_guard.h gpuVkLinesGuard, log lines "[vulkan] [lines] ").
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "outline_compute.h"   // JobState

namespace render {

// one polyline (a run of pieces) as the card reads it (std430, 144 bytes: kLinesHeadCSPart's struct Run)
struct LinesRun {
    std::uint32_t slot = 0, cap = 1, start = 0, n = 0;   // its points: slot + (start + i) % cap, i < n
    std::int32_t from = 0, to = -1, kind = 0, first = 0; // pieces from .. to (piece i joins point i - 1 to point i); 0 straight, 1 a trail, 2 an orbit; its first piece among all
    std::int32_t circle = 0, pad0 = 0, pad1 = 0, pad2 = 0;   // an orbit: the slot of the unit circle's corners
    float colour[4] = {1, 1, 1, 1};
    float style[4] = {1, 0, 0, 0};                       // the width (points)
    float tailHi[4] = {0, 0, 0, 0}, tailLo[4] = {0, 0, 0, 0};   // a trail: point n, the body itself (m; high and low floats)
    float pa[4] = {0, 0, 0, 0}, qb[4] = {0, 0, 0, 0};   // an orbit: P a and Q b (m), qb[3] the step of E between corners
};
static_assert(sizeof(LinesRun) == 144, "kLinesHeadCSPart's struct Run is 144 bytes");

// the view, as the card's steps read it (orbit/orbit_lines.cpp linesCamera): C the reference point (m, high and low floats), the turn (cos and sin of the
// yaw and the pitch), the scale (points per metre), 0.75.0: the pinhole camera (orbit/camera.h): its distance d from the look-at plane (0: 2D), C's depth
// from that plane, the near plane; 2D: C's place on the picture (SX, SY); 3D: the principal point and C's offset from the camera's axis (AX, AY, m);
// the picture's size and the focal length (points)
struct LinesCamera {
    float cHi[3] = {0, 0, 0}, cLo[3] = {0, 0, 0};
    float rot[4] = {1, 0, 1, 0};
    float proj[4] = {1, 0, 0, 0};                        // px, d (0: 2D), gC, the near plane (m)
    float scr[4] = {0, 0, 0, 0};                         // 2D: SX, SY, 0, 0; 3D: the principal point, AX, AY
    float view[4] = {1, 1, 20, 1};                       // W, H, the clipping margin (20 points), the focal length (points)
};

struct LinesDraw {
    LinesCamera cam;
    std::vector<LinesRun> runs;                          // in drawing order; run.first set by add()
    int pieces = 0;
    void add(const LinesRun& r) { if (r.to < r.from) return; runs.push_back(r); runs.back().first = pieces; pieces += r.to - r.from + 1; }
    // the start-up check's own points (beginCheck sends them; the runs' slots count from 0 in this list), xyz doubles
    std::vector<double> checkPts;
    int checkRoom = 0;                                   // room for the check's step points
};

class LinesCard {
public:
    virtual ~LinesCard() = default;
    virtual bool build(std::string& why) = 0;            // the steps' pipeline (kLinesCS); the drawing program is made by the renderer (Renderer2D::linesProgram)
    virtual bool built() const = 0;
    // ---- the points kept on the card (the frame's; sent with the next frame's batch). false: no room (the caller draws on the processor) ----
    // a trail under key: its n points by at(i, xyz); the ring is updated by what was added at its head and lost at its tail, else sent whole
    virtual bool ring(std::uint64_t key, int n, const std::function<void(int, double*)>& at, LinesRun& r) = 0;
    // a list under key (an orbit's corners, a ring): n points, xyz; sent again only when they changed
    virtual bool list(std::uint64_t key, const double* xyz, int n, LinesRun& r) = 0;
    virtual std::uint32_t circleSlot() = 0;              // the unit circle's 257 corners (cos E, sin E, 0), E = 2 pi i / 256 (sent once)
    virtual void frameDone() = 0;                        // the end of a frame that used the store: keys unused for 120 frames are freed
    // ---- the start-up check and the tests: steps 0, 1 and 2 on d's own points, read back (never waits) ----
    virtual int beginCheck(const LinesDraw& d) = 0;
    // per piece its two ends on the picture (4 floats) and the offsets (pieces + 1), then every step's point (2 floats)
    virtual JobState pollCheck(int job, std::vector<float>& ends, std::vector<std::uint32_t>& offsets, std::vector<float>& points) = 0;
    virtual void cancel(int job) = 0;
    virtual long uploadedPoints() const { return 0; }    // (tests and the report) points sent to the card since the start
    bool ready = false;                                  // its start-up check passed: Renderer2D::lines draws with it
};

}  // namespace render
