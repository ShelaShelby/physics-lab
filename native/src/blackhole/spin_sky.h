// Black Hole Lab (0.55.2): the picture round a spinning hole (plan step 3, section 6 way (c)): a table filled by the ray integrator (geodesic.h, J15 (A.15)) in
// double precision, looked up per pixel by the graphics card. Physics only, no graphics. G = c = 1, lengths in M.
//
// THE CAMERA hovers as a FIDO (J15 (A.3)) at (rC, thC, phiC): at a = 0 the lab's hovering camera; at a > 0 nothing can stand still close in, and the FIDO is
// carried round the spin axis at omega (J15 (A.2)) by the dragged space. Its sky: the angle psi from the hole's direction (-e_r) and the azimuth chi round it
// from e_theta towards e_phi (spin.h lookAt). Turning the camera round the spin axis (phiC) turns its whole picture's far sky by the same angle and nothing
// else (the metric does not depend on phi): the table is made for phiC = 0 and is used for every phiC.
//
// THE TABLE (per spin a, distance rC, colatitude thC). The shadow's edge psi_e(chi) by the closed test (spin.h shadowEdge) at nEdge azimuths; then rays on a
// grid: rows chi_i = 2 pi i / nChi, columns at x = psi - psi_e(chi) on two sides (side 0: outside the shadow, x > 0; side 1: inside, x < 0), evenly in
// s = ln|x| + |x| / kSpinXc from |x| = kSpinXlo to the side's largest |x| (pi - psi_e outside, psi_e inside): close to the edge the rays wind round the hole
// and the far direction turns with ln|x| (GLb (42)), so that is where the columns crowd. Per ray: where it ends (the far direction, J15 App. A.1 step (vi), on the
// chart's axes at infinity; or the horizon), its b (J15 (A.12)), and the radii of its first kSpinCross crossings of the equator (the disc's plane), stored as
// u = 1/r. The first crossing a ray does not make is continued smoothly, so that u has no corner where that crossing appears and the cubic does not ring
// there (spinRay): escaping, from the straight line it leaves on (u = 1/(mu* - 10) < 0 where that line meets the plane behind it); falling in, from the
// slope of theta at the horizon (u between 1/r and 2/r, inside the horizon). The crossings after it: 0 (escaping) or 2/r (falling in). None of these
// continued values lies on the disc (r < 0, or r inside the horizon).
// The lookup (as the card does it): the edge by a periodic 4-point cubic in chi over the nEdge samples, x and s from it, then a 4 x 4 point cubic (Lagrange) in the
// row (periodic) and the column (clamped), the far direction normalised afterwards. The rays within kSpinXlo of the edge are drawn as the ray at kSpinXlo
// (the cut-off: tested to be far below a pixel).
// The disc: its first crossing with rIn <= r <= rOut; g = C75 (A9) from r and b (spin.h discShiftKerr). The gas orbits with the spin (a > 0) in the equator.
//
// THE STARS: their images by inverting the table (J15 App. A.7: no simple inverse with spin): every grid cell (two triangles) whose far directions contain the
// star's; the image's place first by the triangle's linear map, then where the table's own cubic sends exactly the star's direction (Newton), its magnification =
// solid angle on the camera's sky over solid angle on the far sky of that cubic's infinitesimal cell (Riazuelo 2019 (51); J15 App. A.3.1). Cells within kStarXmin
// of the edge are left out (their images are below a pixel and below any star's brightness cut).
#pragma once

#include <atomic>
#include <vector>

#include "blackhole/disc.h"
#include "blackhole/geodesic.h"

namespace blackhole {

constexpr double kSpinXlo = 1e-8, kSpinXc = 0.3;
constexpr int kSpinCross = 4;
// the lab's two table sizes (rows nChi, columns nS per side, edge samples): the coarse one at once while a setting changes, the fine one when the view rests
// (the fine one: the far direction within 2e-3 rad of the integrator from 1e-3 of the shadow's radius outward, 1e-2 rad nearer, up to spin kSpinTop)
constexpr int kSpinChiFine = 192, kSpinSFine = 384, kSpinEdge = 2048, kSpinChiCoarse = 64, kSpinSCoarse = 72, kSpinEdgeCoarse = 256;
constexpr double kSpinTop = 0.99;   // the highest spin the tests pass at (tests/test_blackhole.cpp testSpinTable; REPORT 0.55.2): the Spin control's top

struct SpinHit {
    bool sky = false;            // from the far sky (else the horizon)
    Vec3 dir;                    // sky: the far direction (chart axes at infinity, camera at phi = 0)
    double b = 0;
    double u[kSpinCross] = {};   // 1 / r at the crossings of the equator (see above)
    bool fallback = false;       // 0.55.9: followed by mino.h because the closed forms gave no answer (spinRayClosed; counted by the table)
    // 0.56.4 (moving gas, PLAN_MOVINGGAS section 7): at the crossings the ray makes (the first nCross of u), the azimuth phi and the time the light took from
    // there to the camera (far clock, M), from the closed forms (kerr_closed.h); timed is false where they are not known (rays followed by an integrator, or
    // made on the card). 0.57.2: only where asked (SpinFill::timed, spinRayClosed's timed); the lab does not ask yet (the moving gas will)
    int nCross = 0;
    bool timed = false;
    double phi[kSpinCross] = {}, t[kSpinCross] = {};
    // 0.95.0 (the disc's surface, kerr_closed.h "THE DISC'S SURFACE"): dr/dPsi at each crossing (0 where it is not made, or the ray is not of polar type A)
    // and u+^(1/2); interpolated by the lookup like u. Rays followed by mino.h (the closed forms' fallback) have none: their disc stays at the plane.
    double sl[kSpinCross] = {};
    double cn = 0;
};
// 0.95.0: a crossing's slope from the integrator's state there: dr/dPsi = (dr/dtau) / (-u- a^2)^(1/2), dr/dtau = -Delta p_r (J15 (A.15) with Mino time
// d tau = -d zeta / rho^2, mino.h), and u+^(1/2) (kerr_closed.h Polar); both 0 for a ray that is not of type A (eta <= 0)
void crossingSlope(double a, double b, double q, double r, double pr, double& sl, double& cn);

// 0.55.8: how a table's rays are followed: their tolerance (spinRay's rtol), the form (mino.h: the Mino-time form; else geodesic.h's KerrBL) and the
// columns' nearest |x| to the edge (rays nearer are drawn as the ray there)
struct SpinFill {
    double rtol = 1e-10;
    bool mino = false;
    double xlo = kSpinXlo;
    double rtolFar = 0;          // > rtol: a node's tolerance is rtol up to |x| / psi_e = 1e-4, then grows in proportion to |x| / psi_e up to rtolFar (the far
                                 // direction's error from the integrator grows as 1/|x| near the edge, GLb (42); far from it the table's interpolation is the larger)
    bool closed = false;         // 0.55.9: every ray from the closed forms (kerr_closed.h; no tolerance); the rest of the fields then only say how a ray the closed
                                 // forms cannot answer is followed (mino.h at rtol)
    double discRows = 0;         // 0.55.9: > 0: this share of the rows (against 1 for the far direction's and 1 even) also where the disc's image changes fast (SpinTable::rowsOf; REPORT 0.55.9: seen nearly edge on, the moving table's
                                 // 32 rows missed the disc's thin near side, so the picture changed shape when the view came to rest)
    double xc = kSpinXc;         // 0.56.0: the columns' law s = ln|x| + |x| / xc (smaller: more columns far from the edge, where the disc's near side
                                 // crosses in front of the hole; REPORT 0.56.0)
    bool timed = false;          // 0.57.2: the closed forms' rays also give phi and t at their crossings (SpinHit; about five times a ray's cost: only where
                                 // a reader needs them)
};
// 0.55.8, the lab's two (blackhole_view.cpp; REPORT 0.55.8; SpinFill{} is 0.55.7's: KerrBL at 1e-10 for every ray): the fine one, Mino's form at 0.55.7's
// tolerance up to 1e-2 of psi_e, looser beyond (tests/test_blackhole.cpp testSpinTable keeps its limits); the one made while the view moves, Mino's form at the
// coarse table's 0.55.7 tolerance near the edge, looser far from it (testMovingTable: no worse than 0.55.7's coarse table)
// 0.55.9: both from the closed forms (kerr_closed.h); kSpinFineFillMino and kSpinMovingMino are 0.55.8's, kept as the switch for comparing
constexpr SpinFill kSpinFineFillMino{1e-10, true, kSpinXlo, 1e-8};
constexpr SpinFill kSpinMovingMino{1e-8, true, kSpinXlo, 1e-5};
constexpr SpinFill kSpinFineFill{1e-10, true, kSpinXlo, 1e-8, true};
// (0.56.0 tried more columns far from the edge for the moving table, SpinFill::xc 0.03 or 0.06, 96 columns, |x| from 1e-5: the disc's change at the stop
// shrank, but the far direction near the edge got 3 to 60 times worse than 0.55.7's coarse table, which testMovingTable forbids; kept as 0.55.9: REPORT 0.56.0)
constexpr SpinFill kSpinMoving{1e-8, true, kSpinXlo, 1e-5, true, 0.3};

class SpinTable {
public:
    // cancel (may be null): the fill stops early when it becomes true (ok() is then false). threads 0: all cores.
    SpinTable(double a, double rC, double thC, int nChi, int nS, int nEdge, const std::atomic<bool>* cancel = nullptr, int threads = 0, const SpinFill& fill = {});
    // 0.55.6: a table whose rays were followed elsewhere (the graphics card, blackhole/spin_card.h) on this table's grid: the edge samples, the rows' azimuths
    // and edges, and per node (side, row, column) 10 numbers: the far direction (3), b, u (4), the end (1 sky, 2 horizon, 0 lost), the steps (spinRay's values)
    struct Parts { std::vector<double> edge, tau, rowChi, rowEdge; const double* nodes = nullptr; double ms = 0; };
    SpinTable(double a, double rC, double thC, int nChi, int nS, int nEdge, const Parts& parts);
    // 0.55.6: the rows from the edge samples and the probe rays' far directions (one per sample, at x = 1e-4 psi_e): tau and the rows' azimuths (the step
    // the processor's fill takes between its probes and its rows' edges)
    // 0.55.9: discW (one per edge sample, or null): a further share of the rows (discShare against 1 + 1) where the disc's image changes fast (SpinFill::discRows)
    static void rowsOf(const std::vector<double>& edge, const std::vector<Vec3>& probe, int nChi, std::vector<double>& tau, std::vector<double>& rowChi,
                       const std::vector<double>* discW = nullptr, double discShare = 0);
    bool ok() const { return ok_; }
    double spin() const { return a_; }
    double rC() const { return rC_; }
    double thC() const { return thC_; }
    int nChi() const { return nChi_; }
    int nS() const { return nS_; }
    int nEdge() const { return nEdge_; }
    double fillMs() const { return ms_; }
    bool filledByCard() const { return card_; }       // 0.55.6: made from the graphics card's rays (the Parts constructor)
    long rays() const { return rays_; }
    long mismatches() const { return mism_; }        // rays whose end disagrees with their side of the closed edge (expected 0)
    long lost() const { return lost_; }
    long fallbacks() const { return fall_; }          // 0.55.9: rays the closed forms could not answer (followed by mino.h instead)
    double edgeSample(int i) const { return edge_[static_cast<std::size_t>(i)]; }
    double edge(double chi) const;                   // the cubic through the samples
    // the rows are not evenly spaced in chi: evenly in tau, which grows with chi and with the change of the far direction close to the edge (a probe ray at
    // x = 1e-4 psi_e per edge sample), smoothed: near the edge a ray makes ln(1/x) / gamma half-orbits (GLb (42)) and at high spin gamma and the orbit's
    // turn round the axis change fast along part of the edge, so the far direction there changes fast from row to row and those parts get more rows. tau(chi) in [0, 1) by straight lines
    // between the nEdge + 1 samples (texTau for the card)
    double tauOf(double chi) const;
    double rowChi(int i) const { return rowChi_[static_cast<std::size_t>(i)]; }
    double rowEdge(int i) const { return rowEdge_[static_cast<std::size_t>(i)]; }
    double sLo() const;
    double xlo() const { return xlo_; }              // 0.55.8 (SpinFill)
    double sHi(int side, double psiE) const;         // the side's largest s for an edge at psiE
    static double sOf(double ax, double xc = kSpinXc);       // 0.56.0: xc per table (SpinFill::xc)
    static double absXOf(double s, double xc = kSpinXc);     // the inverse (Newton)
    double xc() const { return xc_; }
    // the node (side, row i, column j)
    const SpinHit& node(int side, int i, int j) const { return v_[index(side, i, j)]; }
    double nodePsi(int side, int i, int j) const;
    // the card's data: rows side 0 (nChi of them) then side 1, nS columns each; texA (dir x, y, z, b), texB (u0..u3); the edge samples
    // 0.95.0: texC (the slopes dr/dPsi of the first two crossings, u+^(1/2), 0): the disc's surface (kKerrFS)
    const std::vector<float>& texA() const { return fa_; }
    const std::vector<float>& texB() const { return fb_; }
    const std::vector<float>& texC() const { return fc_; }
    const std::vector<float>& texEdge() const { return fe_; }
    const std::vector<float>& texTau() const { return ft_; }
    // the lookup the card does, in double
    SpinHit lookup(double psi, double chi) const;
private:
    double a_, rC_, thC_, xlo_ = kSpinXlo, xc_ = kSpinXc;
    int nChi_, nS_, nEdge_;
    bool ok_ = true, card_ = false;
    double ms_ = 0;
    long rays_ = 0, mism_ = 0, lost_ = 0, fall_ = 0;
    std::vector<double> edge_, tau_, rowChi_, rowEdge_;
    std::vector<SpinHit> v_;
    std::vector<float> fa_, fb_, fc_, fe_, ft_;
    std::size_t index(int side, int i, int j) const { return (static_cast<std::size_t>(side) * nChi_ + i) * nS_ + j; }
    void finish(const std::atomic<long>& mism, const std::atomic<long>& lost);
};

// one ray of the camera (the reference, rtol as given): the same numbers as a node
SpinHit spinRay(double a, const Camera& cam, double psi, double chi, double rtol = 1e-11);
// 0.55.8: the same from the Mino-time form (mino.h): the moving table's rays
SpinHit spinRayMino(double a, const Camera& cam, double psi, double chi, double rtol);
// 0.55.9: the same from the closed forms (kerr_closed.h; the crossings continued as spinRayMino does); a ray they cannot answer is followed by spinRayMino at
// rtol (its fallback flag set)
SpinHit spinRayClosed(double a, const Camera& cam, double psi, double chi, double rtol, bool timed = false);

// the disc through a hit: the first crossing with rIn <= r <= rOut (order, r, g); order -1 for none. 0.95.0: with a thickness table (ht), the first
// kSurfaceOrders crossings meet the surface |z| = h(R) instead (disc.h surfaceStep on the line through the crossing, its slope sl and u+^(1/2) cn: the
// table has no path beyond the crossing, so no exact steps), r = R there, z its height. The card's table pass (kKerrFS) does the same in single precision.
struct SpinDisc { int order = -1; double r = 0, g = 0, z = 0; };
SpinDisc spinDisc(const SpinHit& h, double a, double rIn, double rOut, const HeightTable* ht = nullptr);

// the images of a far direction (unit, chart axes, camera at phi = 0) through the table: (psi, chi, magnification), magnification = solid angle on the camera's
// sky over solid angle on the far sky of the cell's triangle. Cells within kStarXmin of the edge are not used.
constexpr double kStarXmin = 1e-6;
struct SpinImage { double psi = 0, chi = 0, mag = 0; int side = 0; };
class SpinStars {
public:
    explicit SpinStars(const SpinTable& t, int threads = 0);   // the triangles binned on the far sky (0.55.6: threads 0: all cores)
    static constexpr int kReference = -1;            // threads = kReference: 0.55.5's build on one thread, kept as the tests' reference    // the images of dir: found by the triangles, then put where the table's own cubic sends exactly dir (Newton, 2 steps) and the magnification from that
    // cubic's slopes (the solid-angle ratio of the infinitesimal cell: sin psi dpsi dchi over |d dir/dpsi x d dir/dchi| dpsi dchi)
    int images(const Vec3& dir, std::vector<SpinImage>& out) const;
    double ms() const { return ms_; }
private:
    struct Tri { float psi[3], chi[3]; Vec3 d[3]; double omegaCam; };
    const SpinTable& T_;
    std::vector<Tri> tri_;
    int nb_ = 0;                                      // bins: nb_ x 2 nb_ in (cos theta, phi)
    // 0.55.6: the bins in one array (binStart_[b] .. binStart_[b + 1] - 1 in binIds_: the triangles of bin b in the order of their numbers, as the lists of
    // 0.55.5 had them); built on all cores, the same triangles, numbers and order as the one-thread build
    std::vector<int> binStart_, binIds_;
    double ms_ = 0;
    int binOf(const Vec3& d) const;
    void buildReference();
};

}  // namespace blackhole
