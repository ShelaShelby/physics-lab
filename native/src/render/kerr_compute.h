// 0.55.6: the spinning hole's light paths on the graphics card (OpenGL 4.3 compute, double precision): the Black Hole Lab's table (blackhole/spin_sky.h SpinTable)
// filled by the card instead of the processor. This file only runs the shader (render/shaders.h kKerrTableCS) and moves numbers; what they mean and how they
// are checked is blackhole/spin_card.h. Nothing here waits for the card unless it says so: a pass is queued, done() looks whether it has finished.
#pragma once

#include <string>
#include <vector>

namespace render {

// 0.55.7: the steps of making the shader, named to the caller before they begin and after they end (the guard's marker and log, app/gpu_guard.h)
// 0.55.9: "shader " and a 64-bit FNV-1a hash of the light-path shader's text (render/shaders.h kKerrTableCS), in hex: what the guard keeps in place of the
// app version (app/gpu_guard.h), so that only a changed shader (or driver) tries the card's path again
std::string kerrShaderId();

class KerrSteps {
public:
    virtual ~KerrSteps() {}
    virtual void begin(const std::string& step) = 0;
    virtual void end(const std::string& step, const std::string& result) = 0;
};

class IKerrCompute {
public:
    virtual ~IKerrCompute() {}
    // Pass A: the shadow's edge at each azimuth of chi and the probe ray just outside it (10 numbers each), with the camera's numbers par
    virtual void passA(const std::vector<double>& par, const std::vector<double>& chi) = 0;
    // Pass B: the rows' edges at rowChi and every node's ray (2 sides x rows x nS columns, 10 numbers each), queued in pieces of at most kPiece rays
    // (a long single dispatch can trip the operating system's graphics timeout)
    static constexpr int kPiece = 8192;
    virtual void passB(const std::vector<double>& par, const std::vector<double>& rowChi, int nS) = 0;
    // 0.55.7: pass B in steps, each with its own fence (the first fill on a new PC, piece by piece, growing; passB is the three without fences between)
    virtual bool passBRows(const std::vector<double>& par, const std::vector<double>& rowChi, int nS) = 0;   // false: the last table is still mapped
    virtual void passBPiece(int first, int n) = 0;
    virtual void passBCopy() = 0;
    int nodes() const { return nodes_; }                                                 // pass B's rays (2 x rows x nS)
    // the last queued work has finished. wait: waits for it, at most kWaitLimitMs in all (0.55.7: a fence that never signals cannot hold the app);
    // false then too. queuedMs: how long ago the last work was queued (the callers' time limits)
    static constexpr double kWaitLimitMs = 10000;
    virtual bool done(bool wait) = 0;
    virtual double queuedMs() const = 0;
    // 0.55.7: the waiting calls below in two halves, so the start-up check runs over frames without waiting: queue, then (done) read
    virtual void queueRays(const std::vector<double>& par, const std::vector<double>& psiChi) = 0;
    virtual void readRays(std::vector<double>& out) = 0;
    virtual void queueEdges(const std::vector<double>& par, const std::vector<double>& chi) = 0;
    virtual void readEdges(std::vector<double>& out) = 0;
    virtual void queueMath(const std::vector<double>& par, const std::vector<double>& x) = 0;
    virtual void readMath(std::vector<double>& out) = 0;
    virtual void readA(std::vector<double>& edge, std::vector<double>& probe) = 0;    // after pass A
    // after pass B: the rows' edges, and the nodes' numbers in memory the processor reads directly; valid until release()
    virtual const double* mapB(std::vector<double>& rowEdge) = 0;
    virtual void release() = 0;
    virtual bool mapped() const = 0;
    // tests and the start-up check (these wait): rays at (psi, chi) pairs, 16 numbers each (10 as a node, then the end state's 5 and q); edges at azimuths
    virtual void rays(const std::vector<double>& par, const std::vector<double>& psiChi, std::vector<double>& out) = 0;
    virtual void edges(const std::vector<double>& par, const std::vector<double>& chi, std::vector<double>& out) = 0;
    // tests: the shader's own double functions at x (8 numbers each: sin, cos, exp(x), log(|x|), atan2(x, 1 - x), acos(x / (1 + |x|)), cbrt(x), |x|^-0.2)
    virtual void math(const std::vector<double>& par, const std::vector<double>& x, std::vector<double>& out) = 0;
protected:
    int nodes_ = 0;
};

// The OpenGL 4.3 implementation for the current context, or nullptr (a 3.3 context, a missing function, a shader the driver does not compile); why says
// which. The caller owns it. steps (0.55.7, may be null): told before and after the compile and the link, with the driver's info logs (also when it compiled).
// PL_BH_CARDHANG (tests): its fences never signal.
IKerrCompute* makeKerrCompute(const char** why, KerrSteps* steps = nullptr);

}  // namespace render
