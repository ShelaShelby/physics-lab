// 0.57.1: the Molecule Lab's Density cloud on the graphics card (OpenGL 4.3 compute, single precision only): each frame a compute shader (kMolDotsCS) turns
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

#include "gl_loader.h"
#include "kerr_compute.h"
#include "outline_compute.h"

namespace render {

std::string molCloudShaderId();                   // "molcloud <hash>": a 64-bit FNV-1a hash of the three programs' texts (a changed shader tries again by itself)

struct MolCloudCam {                              // the camera, rounded to float once (molecule/mol_cloud.h CloudCam is the processor's)
    float R[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    float panX = 0, panY = 0, cx = 0, cy = 0, s = 1, ext = 1, w = 1, h = 1;
};
struct MolCloudDraw {                             // one frame's cloud (Renderer2D::molCloud)
    const float* pts = nullptr; long cnt = 0;     // the dots (angstrom, x y z), kept by the caller until the frame's end()
    std::uint64_t epoch = 0;                      // which set of dots (a new number when they changed): sent to the card once per set
    MolCloudCam cam;
    bool additive = true;                         // dark page: the light adds up
    float colour[4] = {1, 1, 1, 1};
    std::function<double(const std::vector<float>&)> exposure;   // the grid's cells in, uLight out (negative: draw nothing); runs inside end()
    bool fenceAfter = false;                      // a fence after the dots unless one is waiting (the guard's step "the first full use" ends when it has signalled)
};

class MoleculeCard {
public:
    // compile and link the three programs: two steps told to steps (begin, end with the driver's logs and the time); false: it failed (why)
    bool build(KerrSteps* steps, std::string& why);
    bool built() const { return cs_ != 0 && dot_ != 0 && grid_ != 0; }
    void reset();                                 // the programs and buffers deleted (the renderer's shutdown)
    bool ready() const { return ready_ && built(); }
    void setReady(bool on) { ready_ = on; }

    // the check: n dots projected with cam into 4 floats each (X, Y, r, wt), read back later without waiting
    int beginProject(const float* pts, long n, const MolCloudCam& cam);
    JobState pollProject(int job, std::vector<float>& out);

    // the frame's cloud, inside Renderer2D::end(): the dots sent if their set is new on the card, made, the grid summed and read back, the dots drawn.
    // fuzz: the blob shader's table; cells: a work buffer. Leaves the blending to the caller.
    void drawCloud(const MolCloudDraw& d, double width, double height, const float* fuzz, std::vector<float>& cells);
    long bytesSent() const { return bytes_; }     // dot places sent to the card so far (a still cloud sends nothing)
    int fenceState();                             // 1 signalled, 0 not yet, -1 failed / none

private:
    struct Job { int id = 0; GLsync sync = nullptr; GLuint pts = 0, out = 0; long n = 0; };
    void release(int id);
    void dispatch(GLuint ptsBuf, GLuint outBuf, long n, const MolCloudCam& c);
    GLuint cs_ = 0, dot_ = 0, grid_ = 0;
    bool ready_ = false;
    std::vector<Job> jobs_;
    int nextId_ = 1;
    // the dots on the card: a few sets kept (a vibration's moments: the cloud of each is sent once), the least recently used one replaced
    struct Set { std::uint64_t epoch = 0; GLuint buf = 0; long n = 0; unsigned used = 0; };
    std::vector<Set> sets_;
    unsigned useClock_ = 0;
    GLuint out_ = 0; long outN_ = 0;
    GLuint dotVao_ = 0, gridVao_ = 0, corner_ = 0, gridFbo_ = 0, gridTex_ = 0;
    int gridW_ = 0, gridH_ = 0;
    long bytes_ = 0;
    GLsync fence_ = nullptr;
    GLint uR0_ = -1, uR1_ = -1, uR2_ = -1, uScr_ = -1, uView_ = -1, uN_ = -1, dSize_ = -1, dLight_ = -1, dColour_ = -1, dFuzz_ = -1, dSphN_ = -1, gGrid_ = -1;
};

}  // namespace render
