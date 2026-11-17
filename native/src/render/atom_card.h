// 0.57.0: the Atom Lab's two compute programs on the graphics card (OpenGL 4.3, single precision only): the Slice view's new dots (kSliceDotsCS) and the
// groups of a jump with every electron (kJumpShowCS). Each is made step by step under its own guard (app/gpu_guard.h gpuSliceGuard, gpuJumpGuard: the
// caller, atom/atom_card_paths.cpp, names and logs the steps): compile, link, a first small run, the check against the processor, the first full use.
// Nothing here waits for the card in a frame: a batch is started, a fence marks its end, and later frames ask whether it has signalled.
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "atom_card_types.h"
#include "gl_loader.h"
#include "kerr_compute.h"

namespace render {

// "slice <hash>" / "jump <hash>": a 64-bit FNV-1a hash of the program's text, what its guard keeps (a changed shader tries again by itself)
std::string sliceShaderId();
std::string jumpShaderId();

class AtomCard : public ISliceCompute {
public:
    enum Program { kSlice = 0, kJump = 1 };
    static const char* programName(int which);                   // "the Slice's dots shader", "the jump's groups shader"
    // compile and link one program: two steps told to steps (begin, end with the driver's log and the time); false: it failed (why)
    bool build(int which, KerrSteps* steps, std::string& why);
    bool built(int which) const { return prog_[which & 1] != 0; }
    void reset();                                                // the programs and buffers deleted (the renderer's shutdown)

    // ---- the Slice ----
    bool ready() const override { return sliceReady_ && prog_[kSlice] != 0; }
    void setSliceReady(bool on) { sliceReady_ = on; if (!on) sliceUsed_ = false; }   // after the check passed (the app's guard steps)

    std::function<void()> beforeFirstUse;                        // called once, before the first batch the lab asks for after setSliceReady (the guard's step)
    int beginSlice(const SliceDotsJob& j) override;
    JobState pollSlice(int job, std::vector<float>& res) override;
    void cancelSlice(int job) override { release(job); }

    // ---- the jump ----
    bool jumpReady() const { return jumpReady_ && prog_[kJump] != 0; }
    void setJumpReady(bool on) { jumpReady_ = on; }
    // every dot's fixed group (2 jn bytes: the old dots, then the new), sent once per jump (epoch); then each frame the groups of the moment are made on
    // the card into a buffer the cloud's programs read as their group attribute (jumpGroupsBuffer). False: nothing made (no program, a size of 0).
    bool jumpGroups(const unsigned char* fixedGroups, long jn, std::uint64_t epoch, std::uint64_t t0, std::uint64_t t1);
    GLuint jumpGroupsBuffer() const { return jumpOut_; }
    // the same with the result read back later (the start-up check and the tests): a job number, polled without waiting
    int beginJumpRead(const unsigned char* fixedGroups, long jn, std::uint64_t t0, std::uint64_t t1);
    JobState pollJumpRead(int job, std::vector<unsigned char>& groups);

    // a fence after the last thing sent (the step "the first full use" ends when it has signalled); fenceState: 1 signalled, 0 not yet, -1 failed / none
    void fence();
    int fenceState();
private:
    struct Job { int id = 0; GLsync sync = nullptr; GLuint buf = 0; long bytes = 0; };
    void release(int id);
    Job* find(int id) { for (Job& j : jobs_) if (j.id == id) return &j; return nullptr; }
    JobState poll(Job& j);
    void dispatchJump(GLuint fixedBuf, GLuint outBuf, long jn, std::uint64_t t0, std::uint64_t t1);
    GLuint prog_[2] = {0, 0};
    bool sliceReady_ = false, jumpReady_ = false, sliceUsed_ = false;
    std::vector<Job> jobs_;
    int nextId_ = 1;
    GLuint jumpFixed_ = 0, jumpOut_ = 0;
    std::uint64_t jumpEpoch_ = 0;
    long jumpN_ = 0;
    GLsync fence_ = nullptr;
};

}  // namespace render
