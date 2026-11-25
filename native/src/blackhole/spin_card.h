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

namespace app { class GpuGuard; }

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
// the rays' ends (sky or horizon) must be equal. Measured (REPORT 0.55.6): the processor's own rays move by up to 2.5e-12 rad, u by 5.5e-10 when psi moves
// by 1e-15 (the step's choices change: the floor two correct double-precision fills can differ by); llvmpipe's card 1.5e-13 rad, u 8.4e-10, edge 0;
// arithmetic of float precision (psi moved by 1e-7) moves them by 2.5e-5 rad and u by 1.4e-6. The limits sit between, far nearer the floor.
constexpr double kCardEdgeTol = 1e-12, kCardDirTol = 1e-9, kCardBTol = 1e-10, kCardUTol = 1e-7;
// 0.55.7: the check's first dispatch is the smallest that can be checked: the shader's own double functions at 8 values (one work group) against the C++
// library's. Measured (REPORT 0.55.6, 4,400 values): within 6e-16 (sin, cos absolute; the others relative); float arithmetic would be 1e-7. Limit between.
constexpr double kCardMathTol = 1e-13;
struct CardCheck {
    bool ok = false;
    int edges = 0, rays = 0, endsDiffer = 0;
    double edge = 0, dir = 0, b = 0, u = 0, math = 0, ms = 0;
    std::string why;                                       // empty when ok
};
CardCheck cardSelfCheck(render::IKerrCompute& K);         // waits (tests, PL_BH_COMPARE); the app uses CardStartup

// 0.55.7: the time a step of the card's path may take (a fence that never signals counts as failed after it): 10 s for a step of the start-up check, a pass
// or a piece of the first fill; 60 s for a later fill's whole pass B (queued at once). PL_BH_CARDLIMIT=<seconds> (tests) sets the first.
double cardStepLimitMs();
constexpr int kCardWholeFactor = 6;
// 0.85.3: a whole fill on the card slower than this keeps the light paths off (blackhole_view.cpp drawAsync). A choice, not a measured value: a quarter
// of a second is longer than the processor's coarse table takes for a first picture (27 ms on the build machine, REPORT 0.81.0).
constexpr double kCardFillSlowMs = 250.0;
// The first fill is sent in pieces, one a frame, so it also counts a frame per piece (22 pieces: 370 ms at 60 Hz). Its limit is larger.
constexpr double kCardFirstFillSlowMs = 1000.0;

// 0.55.7: the start-up check in steps, at most a step per frame (the frame never waits): each step named to the guard (its marker and log) before it begins
class CardStartup {
public:
    CardStartup(render::IKerrCompute& K, app::GpuGuard* guard, double limitMs);
    int pump(bool wait);                                   // 0 under way, 1 passed, -1 failed (check().why), -2 a step ran out of time (lateStep())
    const CardCheck& check() const { return c_; }
    const std::string& lateStep() const { return late_; }
private:
    render::IKerrCompute& K_;
    app::GpuGuard* g_;
    double limit_, t0_ = 0, tq_ = 0;
    int stage_ = 0;
    std::string wait_, late_;
    std::vector<double> chis_, ref_, pc_;
    CardCheck c_;
    void begin(const std::string& s);
    void end(const std::string& s, const std::string& r = std::string());
    void queued(const std::string& what);                  // a dispatch queued: its fence's wait begins
};

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
// 0.55.7: the first fill of a run goes step by step, each with its fence and time: pass A, the rows' edges, then pieces growing from 64 rays (x4 each) to
// kPiece, the copy, the mapping, the worker's read (with a guard: each step named to it; the last step's end = the whole path succeeded once)
class CardFill {
public:
    explicit CardFill(render::IKerrCompute& K, app::GpuGuard* guard = nullptr, double limitMs = 1e300) : K_(K), g_(guard), limit_(limitMs) {}
    bool idle() const { return stage_ == 0; }
    void start(const Settings& s, double W, double H, double cx, double cy, int nChi, int nS, int nEdge);
    // as far as the card has got: a finished table once (else null); a table handed out stays mapped until its data is used or dropped
    std::shared_ptr<CardTableData> pump();
    long fills() const { return fills_; }
    double lastMs() const { return lastMs_; }
    void clearLate() { late_.clear(); }
    const std::string& lateStep() const { return late_; }  // a step ran out of time (the fill was dropped; the caller switches the card's path off)
    static constexpr int kFirstPiece = 64;
private:
    render::IKerrCompute& K_;
    app::GpuGuard* g_;
    double limit_;
    std::string wait_, late_;
    bool stepwise_ = true, reading_ = false;               // (the first fill: step by step; reading_: the worker's read of the mapped nodes is a step)
    int next_ = 0, piece_ = kFirstPiece, pieces_ = 0;
    double tStep_ = 0;
    bool late(double limit);
    void say(const std::string& line);
    int stage_ = 0;                                        // 0 idle, 1 pass A queued, 2 pass B queued, 3 a finished table mapped, 4 (first fill) a part of pass B queued
    std::shared_ptr<CardTableData> run_, out_;
    std::vector<double> par_, edgeChi_;
    double t0_ = 0, lastMs_ = 0;
    long fills_ = 0;
};

}  // namespace blackhole
