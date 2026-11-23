// 0.68.0: the Molecule Lab's Density surface on the graphics card (Vulkan; render/shaders.h kSurfaceCS and kSurfaceVS). The surface's vertices (place,
// normal, potential) and triangles are sent once while the density stays (a vibration's new phase: once per phase); the card turns and colours them,
// keys the triangles by depth and sorts them far to near as the processor does (molecule/surface_draw.h), and draws them in that order. The processor's
// way stays the reference and the fallback. Made and checked by molecule/surface_check.cpp under its guard (app/gpu_guard.h
// gpuVkSurfaceGuard, log lines "[vulkan] [surface] ").
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "outline_compute.h"   // JobState

namespace render {

// the surface as the card is given it: n vertices (p: x, y, z; nrm: unit normal; value: the potential), t triangles (3 numbers each); rev: a new number for
// a new surface (the card keeps the last one it was sent)
struct SurfaceMesh {
    std::uint64_t rev = 0;
    const float* p = nullptr;
    const float* nrm = nullptr;
    const float* value = nullptr;
    const int* tri = nullptr;
    int nv = 0, nt = 0;
};
// the view (molecule_view.cpp's): R the camera's turn (rows), pan (px, py), the picture's centre (cx, cy) and scale s (points per bohr); colours by the
// potential (pot, over potRange) or plain; the front and back faces' opacities; the picture's size (points)
struct SurfaceView {
    float R[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    float px = 0, py = 0, cx = 0, cy = 0, s = 1, potRange = 1;
    int pot = 0;
    float plain[4] = {1, 1, 1, 1};
    float aFront = 0.6f, aBack = 0.15f;
};
struct SurfaceDraw { SurfaceMesh mesh; SurfaceView view; };

class SurfaceCard {
public:
    virtual ~SurfaceCard() = default;
    virtual bool build(std::string& why) = 0;            // the steps' pipeline (kSurfaceCS); the drawing program by Renderer2D::surfaceProgram
    virtual bool built() const = 0;
    virtual long runs() const { return 0; }              // (tests) how many times the frame's steps ran (only when the view or the surface changed)
    virtual long sends() const { return 0; }             // (tests) how many surfaces were sent to the card
    // the start-up check and the tests: the steps on d (its own mesh), read back (never waits): per vertex sx, sy, sz, 0, r, g, b, a; the order (triangles)
    virtual int beginCheck(const SurfaceDraw& d) = 0;
    virtual JobState pollCheck(int job, std::vector<float>& verts, std::vector<std::uint32_t>& order) = 0;
    virtual void cancel(int job) = 0;
    bool ready = false;                                  // its start-up check passed: Renderer2D::surface draws with it
};

}  // namespace render
