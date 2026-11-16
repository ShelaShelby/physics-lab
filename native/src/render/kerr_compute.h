// 0.55.6: the spinning hole's light paths on the graphics card (OpenGL 4.3 compute, double precision): the Black Hole Lab's table (blackhole/spin_sky.h SpinTable)
// filled by the card instead of the processor. This file only runs the shader (render/shaders.h kKerrTableCS) and moves numbers; what they mean and how they
// are checked is blackhole/spin_card.h. Nothing here waits for the card unless it says so: a pass is queued, done() looks whether it has finished.
#pragma once

#include <vector>

namespace render {

class IKerrCompute {
public:
    virtual ~IKerrCompute() {}
    // Pass A: the shadow's edge at each azimuth of chi and the probe ray just outside it (10 numbers each), with the camera's numbers par
    virtual void passA(const std::vector<double>& par, const std::vector<double>& chi) = 0;
    // Pass B: the rows' edges at rowChi and every node's ray (2 sides x rows x nS columns, 10 numbers each), queued in pieces of at most kPiece rays
    // (a long single dispatch can trip the operating system's graphics timeout)
    static constexpr int kPiece = 8192;
    virtual void passB(const std::vector<double>& par, const std::vector<double>& rowChi, int nS) = 0;
    virtual bool done(bool wait) = 0;                                                   // the last pass has finished (wait: waits for it)
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
};

// The OpenGL 4.3 implementation for the current context, or nullptr (a 3.3 context, a missing function, a shader the driver does not compile); why says
// which. The caller owns it.
IKerrCompute* makeKerrCompute(const char** why);

}  // namespace render
