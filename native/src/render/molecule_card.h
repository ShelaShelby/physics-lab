// 0.57.1: the Molecule Lab's Density cloud on the graphics card (single precision only; 0.61.0: on Vulkan, render/vk_card.cpp): each frame a compute shader (kMolDotsCS) turns
// every dot (kept in a buffer on the card per set of dots) into its place, size and depth weight; the exposure's grid is summed from those (kMolGridVS) and
// read back, and the soft dots are drawn from them (kMolDotVS with the blob fragment shader). Made step by step under its own guard (app/gpu_guard.h
// gpuMolCloudGuard; the caller, molecule/molecule_card_paths.cpp, names and logs the steps): compile, link, a first small run, the check against the
// processor (molecule/mol_cloud.h), the first full use. Nothing here waits for the card outside the frame's draw but the grid's read-back (as the Atom Lab's
// card cloud): a check's batch is started, a fence marks its end, and later frames ask whether it has signalled.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "kerr_compute.h"
#include "outline_compute.h"

namespace render {

std::string molCloudShaderId();                   // "molcloud <hash>": a 64-bit FNV-1a hash of the three programs' texts (a changed shader tries again by itself)

struct MolCloudCam {                              // the camera, rounded to float once (molecule/mol_cloud.h CloudCam is the processor's)
    float R[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    float panX = 0, panY = 0, cx = 0, cy = 0, s = 1, ext = 1, w = 1, h = 1;
};
struct MolCloudDraw {                             // one frame's cloud (Renderer2D::molCloud)
    const float* pts = nullptr; long cnt = 0;     // the dots (angstrom, x y z), kept by the caller until the frame's endVulkan()
    std::uint64_t epoch = 0;                      // which set of dots (a new number when they changed): sent to the card once per set
    MolCloudCam cam;
    bool additive = true;                         // dark page: the light adds up
    float colour[4] = {1, 1, 1, 1};
    std::function<double(const std::vector<float>&)> exposure;   // the grid's cells in, uLight out (negative: draw nothing); runs inside endVulkan()
    bool fenceAfter = false;                      // a fence after the dots unless one is waiting (the guard's step "the first full use" ends when it has signalled)
};

// 0.61.0: as the lab sees it; Vulkan's is made by render/vk_card.cpp (makeVkMoleculeCard), and MoleculeCardNone stands in where it cannot be made
class MoleculeCard {
public:
    virtual ~MoleculeCard() {}
    // compile and link the three programs: two steps told to steps (begin, end with the driver's logs and the time); false: it failed (why)
    virtual bool build(KerrSteps* steps, std::string& why) = 0;
    virtual bool built() const = 0;
    virtual void reset() = 0;                     // the programs and buffers deleted (the renderer's shutdown)
    bool ready() const { return ready_ && built(); }
    void setReady(bool on) { ready_ = on; }

    // the check: n dots projected with cam into 4 floats each (X, Y, r, wt), read back later without waiting
    virtual int beginProject(const float* pts, long n, const MolCloudCam& cam) = 0;
    virtual JobState pollProject(int job, std::vector<float>& out) = 0;

    long bytesSent() const { return bytes_; }     // dot places sent to the card so far (a still cloud sends nothing)
    virtual int fenceState() = 0;                 // 1 signalled, 0 not yet, -1 failed / none
protected:
    bool ready_ = false;
    long bytes_ = 0;
};

class MoleculeCardNone : public MoleculeCard {
public:
    bool build(KerrSteps*, std::string& why) override { why = "the graphics card's programs are not available on this start"; return false; }
    bool built() const override { return false; }
    void reset() override { ready_ = false; }
    int beginProject(const float*, long, const MolCloudCam&) override { return 0; }
    JobState pollProject(int, std::vector<float>&) override { return JobState::Failed; }
    int fenceState() override { return -1; }
};

}  // namespace render
