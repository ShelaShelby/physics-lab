// 0.57.1: the Molecule Lab's Density cloud on the card (render/molecule_card.h) made and checked step by step within its own guard (app/gpu_guard.h
// gpuMolCloudGuard), the pattern of the Atom Lab's card programs (atom/atom_card_paths.h): off at once when a past run died inside one of its steps with this
// program and driver; nothing of it before the Density style's cloud is on screen for 3 frames; then a step a frame, each logged with its time and guarded by
// a marker: compile, link, a first small run (read back without waiting), the check against the processor (molecule/mol_cloud.h: every dot within its
// derived float limit; it must pass, or the processor's path is kept and Settings, Performance says why), the first full use (until the card has drawn it).
// A step that does not finish within 10 s (PL_MOL_CARDLIMIT seconds) switches it off, kept as "slow".
#pragma once

#include <string>
#include <vector>

#include "molecule/mol_cloud.h"
#include "render/renderer2d.h"

namespace molecule {

// the camera rounded to float once, for the card
inline render::MolCloudCam cardCam(const CloudCam& c) {
    render::MolCloudCam k;
    for (int i = 0; i < 9; i++) k.R[i] = static_cast<float>(c.R[i]);
    k.panX = static_cast<float>(c.px); k.panY = static_cast<float>(c.py); k.cx = static_cast<float>(c.cx); k.cy = static_cast<float>(c.cy);
    k.s = static_cast<float>(c.s); k.ext = static_cast<float>(c.ext); k.w = static_cast<float>(c.w); k.h = static_cast<float>(c.h);
    return k;
}

class MolCardPaths {
public:
    // once per frame while the Density style's cloud is drawn: moves the program a step on (pts, n: the lab's own dots, also used by the check)
    void frame(render::Renderer2D& R, const float* pts, long n);
    // the card draws this frame's cloud (its check passed, the lab's switch on): the step "the first full use" begins at the first such frame
    bool use(render::Renderer2D& R, bool switchOn);
    bool fenceWanted() const { return state_ == 1 && firstUse_ && !firstDone_; }   // the first full use: a fence after the card's dots
    int state() const { return state_ == 1 ? 1 : state_ == -1 ? -1 : 0; }   // 1 the card, 0 not decided yet, -1 the processor (why)
    const std::string& why() const { return why_; }
    const std::string& checkText() const { return check_; }
    std::string pathText(bool switchOn, bool computeOk) const;               // one line: who does it and why (Settings, the benchmark)
private:
    int state_ = 0;                  // 0 waiting for its picture, 2 its steps under way, 1 ready, -1 off
    int step_ = 0, frames_ = 0, gen_ = -1;
    double stepAt_ = 0;
    bool firstUse_ = false, firstDone_ = false;
    std::vector<int> jobs_;
    std::vector<std::vector<float>> caseDots_;
    std::vector<int> caseCam_;
    int doneJobs_ = 0;
    bool allOk_ = true;
    std::string why_, check_, worst_;
    void off(const std::string& why);
    void advance(render::Renderer2D& R, const float* pts, long n);
};

MolCardPaths& molCardPaths();

}  // namespace molecule
