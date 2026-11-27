// 0.57.0: the Atom Lab's two card programs (render/atom_card.h) made and checked step by step, each within its own guard (app/gpu_guard.h gpuSliceGuard,
// gpuJumpGuard), the pattern of the spinning hole's sky (blackhole_view.cpp skyReady): off at once when a past run died inside one of its steps with this
// program and driver; nothing of it before its kind of picture is on screen (the Slice view; a jump in the every-electron Glow cloud in Cloud 3D) for 3 frames; then a
// step a frame, each logged with its time and guarded by a marker: compile, link, a first small run (read back without waiting), the check against the
// processor (the same seeds and shares: atom/atom_card.h; it must pass, or the processor's path is kept and Settings, Performance says why), the first
// full use (until the card has finished it). A step that does not finish within 10 s (PL_ATOM_CARDLIMIT seconds) switches that program off, kept as "slow".
#pragma once

#include <string>

#include "atom/atom_sim.h"
#include "render/renderer2d.h"

namespace atom {

class AtomCardPaths {
public:
    // once per frame while the Atom Lab is drawn (any view): moves each program a step on when its picture is on screen
    void frame(render::Renderer2D& R, AtomLab& L);
    // the jump's groups for this frame from the card (Renderer2D::cloudGpu's grpBuf), or 0: the processor's path (not ready, switched off, Off)
    unsigned jumpGroupsBuffer(render::Renderer2D& R, AtomLab& L);
    // 1 the card (its check passed), 0 not decided yet, -1 the processor (why), for the Slice (which 0) and the jump (which 1)
    int state(int which) const { return p_[which & 1].state == 1 ? 1 : p_[which & 1].state == -1 ? -1 : 0; }
    // 0.65.0: the Slice's program is still starting where the lab uses it (its steps, or its first full use not yet done): another program's start-up waits,
    // so the Slice's dots come from the card at the same frame as without it (the Classic dots' program now starts in the Slice too)
    bool sliceStarting(const AtomLab& L) const { const Path& P = p_[0]; return P.state == 2 || (P.state == 1 && !P.firstDone && L.sliceCardKind()); }
    const std::string& why(int which) const { return p_[which & 1].why; }
    const std::string& checkText(int which) const { return p_[which & 1].check; }
    std::string pathText(int which, const AtomLab& L, bool computeOk) const;   // one line: who does it and why (Settings, the benchmark)
private:
    struct Path {
        int state = 0;               // 0 waiting for its picture, 2 its steps under way, 1 ready, -1 off
        int step = 0, frames = 0, gen = -1;
        double stepAt = 0;
        bool firstUse = false, firstDone = false;
        bool fenced = false;                             // 0.85.3: the first full use's fence is made once and kept until it signals
        int jobs[16] = {};                               // (0.91.0: 16; 0.93.0: the Slice's 7 cases, its Dirac program's 6 and the Stark cloud's 3)
        int nJobs = 0, doneJobs = 0;
        bool allOk = true;
        std::string why, check, worst;
    };
    Path p_[2];
    void off(int which, const std::string& why);
    void advance(int which, render::Renderer2D& R, AtomLab& L);
};

AtomCardPaths& atomCardPaths();

}  // namespace atom
