// Black Hole Lab (0.55.6): the spinning hole's table (spin_sky.h SpinTable) filled by the graphics card (render/kerr_compute.h, the shader kKerrTableCS in
// render/shaders.h): the same grid, the same rays and adaptive step (geodesic.cpp, ported line by line to GLSL in double precision), the same stored values.
// The processor's fill stays the reference, the fallback and a switch (Settings, Performance). The card's table is used only after a start-up check on the
// user's own card: a fixed set of edges and rays filled by the card against the processor's own, within the limits below (measured: REPORT 0.55.6).
//
// The steps of a fill (as the processor's): (A) the shadow's edge at nEdge azimuths (the closed test by bisection) and a probe ray just outside each;
// on the processor: the rows' azimuths from the probes (SpinTable::rowsOf, so the rows are placed by the processor's own arithmetic); (B) the rows' edges
// and every node's ray. A and B are queued on the card and looked at in later frames: the frame never waits for them.
#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "blackhole/lab.h"
#include "blackhole/spin_sky.h"
#include "render/kerr_compute.h"

namespace blackhole {

// the camera's numbers the shader takes (its Pa[]): made by the processor, so they are the processor's numbers to the bit
std::vector<double> cardParams(double a, double rC, double thC, double rtol = 1e-10);
// the whole table on the card, waiting for it (tests, the benchmark): null if the card returned nothing
std::shared_ptr<SpinTable> cardTableNow(render::IKerrCompute& K, double a, double rC, double thC, int nChi, int nS, int nEdge, double rtol = 1e-10);
// single rays on the card at (psi, chi) pairs: the stored values, the end (1 sky, 2 horizon, 0 lost), the steps, the end state and q (the conservation checks)
struct CardRay { SpinHit h; int end = 0, steps = 0; double y[5] = {0, 0, 0, 0, 0}; double q = 0; };
void cardRays(render::IKerrCompute& K, double a, double rC, double thC, const std::vector<double>& psiChi, std::vector<CardRay>& out, double rtol = 1e-10);
void cardEdges(render::IKerrCompute& K, double a, double rC, double thC, const std::vector<double>& chi, std::vector<double>& out);

// the start-up check's limits against the processor (spinRay, shadowEdge): the edge (rad), the far direction (rad), b and each u (relative to 1 + |value|);
// the rays' ends (sky or horizon) must be equal
constexpr double kCardEdgeTol = 1e-12, kCardDirTol = 1e-8, kCardBTol = 1e-10, kCardUTol = 1e-8;
struct CardCheck {
    bool ok = false;
    int edges = 0, rays = 0, endsDiffer = 0;
    double edge = 0, dir = 0, b = 0, u = 0, ms = 0;
    std::string why;                                       // empty when ok
};
CardCheck cardSelfCheck(render::IKerrCompute& K);

// a finished fill: the view it was started for, the table's parts; its nodes stay in the card's mapped memory until the worker has made the table (used)
struct CardTableData {
    Settings s;
    double W = 0, H = 0, cx = 0, cy = 0;
    int nChi = 0, nS = 0, nEdge = 0;
    SpinTable::Parts parts;
    std::shared_ptr<std::atomic<bool>> used = std::make_shared<std::atomic<bool>>(false);
    std::shared_ptr<const SpinTable> make() const { return std::make_shared<SpinTable>(s.spin, s.dist, s.pitch, nChi, nS, nEdge, parts); }
};

// the asynchronous fill, driven by the frame's thread (never waits)
class CardFill {
public:
    explicit CardFill(render::IKerrCompute& K) : K_(K) {}
    bool idle() const { return stage_ == 0; }
    void start(const Settings& s, double W, double H, double cx, double cy, int nChi, int nS, int nEdge);
    // as far as the card has got: a finished table once (else null); a table handed out stays mapped until its data is used or dropped
    std::shared_ptr<CardTableData> pump();
    long fills() const { return fills_; }
    double lastMs() const { return lastMs_; }
private:
    render::IKerrCompute& K_;
    int stage_ = 0;                                        // 0 idle, 1 pass A queued, 2 pass B queued, 3 a finished table mapped
    std::shared_ptr<CardTableData> run_, out_;
    std::vector<double> par_, edgeChi_;
    double t0_ = 0, lastMs_ = 0;
    long fills_ = 0;
};

}  // namespace blackhole
