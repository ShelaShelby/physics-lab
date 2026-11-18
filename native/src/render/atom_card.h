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

// 0.61.0: the Atom Lab's card programs as the lab sees them; AtomCardGl is OpenGL's (render/atom_card.cpp), Vulkan's is made by render/vk_card.cpp
// (makeVkAtomCard), AtomCardNone stands in where neither can be made (every program absent: the processor's paths)
class AtomCard : public ISliceCompute {
public:
    enum Program { kSlice = 0, kJump = 1 };
    ~AtomCard() override {}
    static const char* programName(int which);                   // "the Slice's dots shader", "the jump's groups shader"
    // compile and link one program: two steps told to steps (begin, end with the driver's log and the time); false: it failed (why)
    virtual bool build(int which, KerrSteps* steps, std::string& why) = 0;
    virtual bool built(int which) const = 0;
    virtual void reset() = 0;                                    // the programs and buffers deleted (the renderer's shutdown)

    // ---- the Slice ----
    bool ready() const override { return sliceReady_ && built(kSlice); }
    void setSliceReady(bool on) { sliceReady_ = on; if (!on) sliceUsed_ = false; }   // after the check passed (the app's guard steps)

    std::function<void()> beforeFirstUse;                        // called once, before the first batch the lab asks for after setSliceReady (the guard's step)

    // ---- the jump ----
    bool jumpReady() const { return jumpReady_ && built(kJump); }
    void setJumpReady(bool on) { jumpReady_ = on; }
    // every dot's fixed group (2 jn bytes: the old dots, then the new), sent once per jump (epoch); then each frame the groups of the moment are made on
    // the card into a buffer the cloud's programs read as their group attribute (jumpGroupsBuffer). False: nothing made (no program, a size of 0).
    virtual bool jumpGroups(const unsigned char* fixedGroups, long jn, std::uint64_t epoch, std::uint64_t t0, std::uint64_t t1) = 0;
    virtual GLuint jumpGroupsBuffer() const = 0;                 // (Vulkan: not 0 once made; the renderer reads its own buffer)
    // the same with the result read back later (the start-up check and the tests): a job number, polled without waiting
    virtual int beginJumpRead(const unsigned char* fixedGroups, long jn, std::uint64_t t0, std::uint64_t t1) = 0;
    virtual JobState pollJumpRead(int job, std::vector<unsigned char>& groups) = 0;

    // a fence after the last thing sent (the step "the first full use" ends when it has signalled); fenceState: 1 signalled, 0 not yet, -1 failed / none
    virtual void fence() = 0;
    virtual int fenceState() = 0;
protected:
    void firstBatch() { if (sliceReady_ && !sliceUsed_) { sliceUsed_ = true; if (beforeFirstUse) beforeFirstUse(); } }   // (the guard's marker before the first batch the lab asks for)
    bool sliceReady_ = false, jumpReady_ = false, sliceUsed_ = false;
};

class AtomCardGl : public AtomCard {
public:
    bool build(int which, KerrSteps* steps, std::string& why) override;
    bool built(int which) const override { return prog_[which & 1] != 0; }
    void reset() override;
    int beginSlice(const SliceDotsJob& j) override;
    JobState pollSlice(int job, std::vector<float>& res) override;
    void cancelSlice(int job) override { release(job); }
    bool jumpGroups(const unsigned char* fixedGroups, long jn, std::uint64_t epoch, std::uint64_t t0, std::uint64_t t1) override;
    GLuint jumpGroupsBuffer() const override { return jumpOut_; }
    int beginJumpRead(const unsigned char* fixedGroups, long jn, std::uint64_t t0, std::uint64_t t1) override;
    JobState pollJumpRead(int job, std::vector<unsigned char>& groups) override;
    void fence() override;
    int fenceState() override;
private:
    struct Job { int id = 0; GLsync sync = nullptr; GLuint buf = 0; long bytes = 0; };
    void release(int id);
    Job* find(int id) { for (Job& j : jobs_) if (j.id == id) return &j; return nullptr; }
    JobState poll(Job& j);
    void dispatchJump(GLuint fixedBuf, GLuint outBuf, long jn, std::uint64_t t0, std::uint64_t t1);
    GLuint prog_[2] = {0, 0};
    std::vector<Job> jobs_;
    int nextId_ = 1;
    GLuint jumpFixed_ = 0, jumpOut_ = 0;
    std::uint64_t jumpEpoch_ = 0;
    long jumpN_ = 0;
    GLsync fence_ = nullptr;
};

// no card program at all (a Vulkan start whose card programs could not be made): every call says no
class AtomCardNone : public AtomCard {
public:
    bool build(int, KerrSteps*, std::string& why) override { why = "the graphics card's programs are not available on this start"; return false; }
    bool built(int) const override { return false; }
    void reset() override { sliceReady_ = jumpReady_ = sliceUsed_ = false; }
    int beginSlice(const SliceDotsJob&) override { return 0; }
    JobState pollSlice(int, std::vector<float>&) override { return JobState::Failed; }
    void cancelSlice(int) override {}
    bool jumpGroups(const unsigned char*, long, std::uint64_t, std::uint64_t, std::uint64_t) override { return false; }
    GLuint jumpGroupsBuffer() const override { return 0; }
    int beginJumpRead(const unsigned char*, long, std::uint64_t, std::uint64_t) override { return 0; }
    JobState pollJumpRead(int, std::vector<unsigned char>&) override { return JobState::Failed; }
    void fence() override {}
    int fenceState() override { return -1; }
};

}  // namespace render
