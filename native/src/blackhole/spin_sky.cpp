#include "blackhole/spin_sky.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <thread>

#include "blackhole/kerr.h"
#include "blackhole/low_priority.h"
#include "blackhole/spin.h"

namespace blackhole {

namespace {
constexpr double kPi = 3.14159265358979323846;
// Lagrange 4-point weights at u in [0, 1] between the 2nd and 3rd points (the same as LensTable's and the card's)
void cubicW(double u, double w[4]) {
    w[0] = -u * (u - 1) * (u - 2) / 6; w[1] = (u + 1) * (u - 1) * (u - 2) / 2; w[2] = -(u + 1) * u * (u - 2) / 2; w[3] = (u + 1) * u * (u - 1) / 6;
}
// clamped stencil over n points at the real index q: the first index and the weights
int stencil(double q, int n, double w[4]) {
    q = std::clamp(q, 0.0, static_cast<double>(n - 1));
    const int i = std::clamp(static_cast<int>(q), 1, n - 3);
    cubicW(q - i, w);
    return i - 1;
}
double triSolid(const Vec3& a, const Vec3& b, const Vec3& c) {   // the solid angle of a spherical triangle (Van Oosterom & Strackee)
    return 2 * std::atan2(std::fabs(dot(a, cross(b, c))), 1 + dot(a, b) + dot(b, c) + dot(c, a));
}
}  // namespace

double SpinTable::sOf(double ax) { return std::log(ax) + ax / kSpinXc; }
double SpinTable::absXOf(double s) {
    double x = std::min(std::exp(s), kSpinXc * std::max(s, 1.0));      // a start on the right side of the root
    for (int it = 0; it < 100; it++) {
        const double f = std::log(x) + x / kSpinXc - s, d = 1 / x + 1 / kSpinXc, nx = x - f / d;
        x = nx > 0 ? nx : x / 2;
        if (std::fabs(f) < 1e-15 * std::max(1.0, std::fabs(s))) break;
    }
    return x;
}
double SpinTable::sLo() const { return sOf(kSpinXlo); }
double SpinTable::sHi(int side, double psiE) const { return sOf(side == 0 ? kPi - psiE : psiE); }

double SpinTable::edge(double chi) const {
    double q = chi / (2 * kPi) * nEdge_;
    q -= std::floor(q / nEdge_) * nEdge_;
    const int i = static_cast<int>(q);
    double w[4];
    cubicW(q - i, w);
    double e = 0;
    for (int k = 0; k < 4; k++) e += w[k] * edge_[static_cast<std::size_t>(((i - 1 + k) % nEdge_ + nEdge_) % nEdge_)];
    return e;
}

double SpinTable::tauOf(double chi) const {
    double q = chi / (2 * kPi) * nEdge_;
    const double turns = std::floor(q / nEdge_);
    q -= turns * nEdge_;
    const int i = std::min(static_cast<int>(q), nEdge_ - 1);
    return turns + tau_[static_cast<std::size_t>(i)] + (q - i) * (tau_[static_cast<std::size_t>(i) + 1] - tau_[static_cast<std::size_t>(i)]);
}

double SpinTable::nodePsi(int side, int i, int j) const {
    const double pe = rowEdge_[static_cast<std::size_t>(i)];
    const double s = sLo() + (sHi(side, pe) - sLo()) * j / (nS_ - 1);
    const double ax = absXOf(s);
    return side == 0 ? pe + ax : pe - ax;
}

SpinHit spinRay(double a, const Camera& cam, double psi, double chi, double rtol) {
    const KerrBL ch(a);
    TraceOptions o;
    o.rtol = rtol; o.atol = rtol * 0.1;
    std::vector<double> cr;
    o.cross = &cr; o.maxCross = kSpinCross;
    const Vec3 look = lookAt(psi, chi);
    Ray ray;
    SpinHit h;
    if (!ch.launch(cam, look, ray)) return h;
    h.b = ray.c[0];
    const RayEnd e = trace(ch, cam, look, o);
    h.sky = e.kind == RayEnd::Sky;
    h.dir = e.dir;
    const int n = static_cast<int>(cr.size() / 5);
    for (int k = 0; k < kSpinCross && k < n; k++) h.u[k] = 1 / cr[static_cast<std::size_t>(k) * 5];
    if (n < kSpinCross) {
        // the first crossing it does not make, continued smoothly (spin_sky.h): u is then the same analytic function on both sides of where it appears
        const double re = e.y[0], th = e.y[1];
        double uv = 0, rest = 0;
        if (h.sky) {
            // the straight line it leaves on, p + mu d (p the part of the end point P across d; the end is at mu_end = P . d): it meets the plane at
            // mu* = -p_z / d_z. mu* > mu_end: a real crossing beyond the integration's end, u = 1/mu*. Else not made: u = -1 / (mu*^2 + 100)^(1/2), the same
            // function to first order where mu* -> -infinity (where the crossing appears), smooth, negative and bounded by -0.1 everywhere else
            const Vec3 P{re * std::sin(th) * std::cos(e.y[2]), re * std::sin(th) * std::sin(e.y[2]), re * std::cos(th)}, d = e.dir;
            const double muEnd = dot(P, d);
            const Vec3 p = P - d * muEnd;
            const double mu = d.z != 0 ? -p.z / d.z : -1e300;
            uv = mu > muEnd ? 1 / mu : -1 / std::sqrt(mu * mu + 100);
            rest = 0;
        } else if (e.kind == RayEnd::Horizon) {
            // going in at the horizon: the plane is delta = (pi/2 - theta) / (dtheta/dr) away in r. delta < 0: further in, not reached:
            // u = (1 - tanh(delta / r)) / r: 1 / (r + delta) to first order (the real crossing's value just outside), between 1/r and 2/r (inside the horizon's
            // 1/r+: never on the disc). delta >= 0: the plane lies behind (the crossing already made): the next is far off, 2/r, the value of the ones after it.
            const double dthdr = e.dy[1] / e.dy[0], delta = dthdr != 0 ? (kPi / 2 - th) / dthdr : -1e300;
            uv = delta < 0 ? (1 - std::tanh(std::max(delta, -1e3) / re)) / re : 2 / re;
            rest = 2 / re;
        }
        h.u[n] = uv;
        for (int k = n + 1; k < kSpinCross; k++) h.u[k] = rest;
    }
    if (e.kind == RayEnd::Lost) h.dir = Vec3{0, 0, 0};
    return h;
}

SpinTable::SpinTable(double a, double rC, double thC, int nChi, int nS, int nEdge, const std::atomic<bool>* cancel, int threads, double rtol)
    : a_(a), rC_(rC), thC_(thC), nChi_(nChi), nS_(nS), nEdge_(nEdge) {
    const auto t0 = std::chrono::steady_clock::now();
    const Camera cam{rC, thC, 0};
    const int nth = threads > 0 ? threads : std::clamp(static_cast<int>(std::thread::hardware_concurrency()), 1, 16);
    const auto parallel = [&](int n, const std::function<void(int)>& job) {   // (a plain std::function: generic lambdas nested in lambdas crashed one compiler)
        std::atomic<int> next{0};
        const auto worker = [&] { for (int i = next++; i < n; i = next++) { if (cancel && cancel->load()) return; job(i); } };
        std::vector<std::thread> pool;
        const bool low = thisThreadLow();                                     // 0.55.5 (low_priority.h)
        for (int w = 1; w < nth; w++) pool.emplace_back([&worker, low] { if (low) lowerThisThread(); worker(); });
        worker();
        for (std::thread& t : pool) t.join();
    };
    edge_.assign(static_cast<std::size_t>(nEdge_), 0);
    parallel(nEdge_, [&](int i) { edge_[static_cast<std::size_t>(i)] = shadowEdge(a, cam, 2 * kPi * i / nEdge_); });
    if (cancel && cancel->load()) { ok_ = false; return; }
    // tau: rows equidistribute the change of the far direction from azimuth to azimuth close to the edge (a probe ray at x = 1e-4 psi_e at each edge sample:
    // the angle between neighbours), smoothed over +-8 samples, plus an even share (half the rows) (rowsOf)
    {
        std::vector<Vec3> probe(static_cast<std::size_t>(nEdge_));
        parallel(nEdge_, [&](int i) {
            const double pe = edge_[static_cast<std::size_t>(i)];
            probe[static_cast<std::size_t>(i)] = spinRay(a, cam, pe + 1e-4 * pe, 2 * kPi * i / nEdge_, rtol).dir;
        });
        if (cancel && cancel->load()) { ok_ = false; return; }
        rowsOf(edge_, probe, nChi, tau_, rowChi_);
        rowEdge_.assign(static_cast<std::size_t>(nChi), 0);
        parallel(nChi, [&](int i) { rowEdge_[static_cast<std::size_t>(i)] = shadowEdge(a, cam, rowChi_[static_cast<std::size_t>(i)]); });
        if (cancel && cancel->load()) { ok_ = false; return; }
    }
    v_.assign(static_cast<std::size_t>(2) * nChi * nS, SpinHit{});
    std::atomic<long> mism{0}, lost{0};
    parallel(2 * nChi, [&](int row) {
        const int side = row / nChi, i = row % nChi;
        const double chi = rowChi_[static_cast<std::size_t>(i)];
        for (int j = 0; j < nS; j++) {
            SpinHit h = spinRay(a, cam, nodePsi(side, i, j), chi, rtol);
            if (h.sky != (side == 0)) { mism++; if (length(h.dir) == 0 && h.sky == false && side == 0) lost++; }
            v_[index(side, i, j)] = h;
        }
    });
    if (cancel && cancel->load()) { ok_ = false; return; }
    finish(mism, lost);
    ms_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

void SpinTable::rowsOf(const std::vector<double>& edge, const std::vector<Vec3>& probe, int nChi, std::vector<double>& tau, std::vector<double>& rowChi) {
    const int nEdge = static_cast<int>(edge.size());
    std::vector<double> rt(static_cast<std::size_t>(nEdge)), d(static_cast<std::size_t>(nEdge));
    for (int i = 0; i < nEdge; i++) {
        const Vec3 &p0 = probe[static_cast<std::size_t>(i)], &p1 = probe[static_cast<std::size_t>((i + 1) % nEdge)];
        rt[static_cast<std::size_t>(i)] = std::atan2(length(cross(p0, p1)), dot(p0, p1));
    }
    double V = 0;
    for (int i = 0; i < nEdge; i++) {
        double m = 0;
        for (int k = -8; k <= 8; k++) m += rt[static_cast<std::size_t>(((i + k) % nEdge + nEdge) % nEdge)];
        d[static_cast<std::size_t>(i)] = m / 17;
        V += d[static_cast<std::size_t>(i)];
    }
    tau.assign(static_cast<std::size_t>(nEdge) + 1, 0);
    for (int i = 0; i < nEdge; i++)
        tau[static_cast<std::size_t>(i) + 1] = tau[static_cast<std::size_t>(i)] + (V > 1e-9 ? d[static_cast<std::size_t>(i)] / V : 0) + 1.0 / nEdge;
    const double tot = tau.back();
    for (double& x : tau) x /= tot;
    rowChi.assign(static_cast<std::size_t>(nChi), 0);
    int k = 0;
    for (int i = 0; i < nChi; i++) {
        const double t = static_cast<double>(i) / nChi;
        while (k + 1 < nEdge && tau[static_cast<std::size_t>(k) + 1] <= t) k++;
        const double f = (t - tau[static_cast<std::size_t>(k)]) / (tau[static_cast<std::size_t>(k) + 1] - tau[static_cast<std::size_t>(k)]);
        rowChi[static_cast<std::size_t>(i)] = 2 * kPi * (k + f) / nEdge;
    }
}

SpinTable::SpinTable(double a, double rC, double thC, int nChi, int nS, int nEdge, const Parts& p)
    : a_(a), rC_(rC), thC_(thC), nChi_(nChi), nS_(nS), nEdge_(nEdge), card_(true), ms_(p.ms) {
    edge_ = p.edge; tau_ = p.tau; rowChi_ = p.rowChi; rowEdge_ = p.rowEdge;
    v_.assign(static_cast<std::size_t>(2) * nChi * nS, SpinHit{});
    std::atomic<long> mism{0}, lost{0};
    for (std::size_t q = 0; q < v_.size(); q++) {
        const double* n = p.nodes + 10 * q;
        SpinHit& h = v_[q];
        h.dir = Vec3{n[0], n[1], n[2]};
        h.b = n[3];
        for (int k = 0; k < kSpinCross; k++) h.u[k] = n[4 + k];
        h.sky = n[8] == 1;
        const int side = static_cast<int>(q / (static_cast<std::size_t>(nChi) * nS));
        if (h.sky != (side == 0)) { mism++; if (length(h.dir) == 0 && !h.sky && side == 0) lost++; }
    }
    finish(mism, lost);
}

void SpinTable::finish(const std::atomic<long>& mism, const std::atomic<long>& lost) {
    mism_ = mism; lost_ = lost;
    rays_ = 2L * nChi_ * nS_;
    fa_.resize(v_.size() * 4); fb_.resize(v_.size() * 4);
    for (std::size_t q = 0; q < v_.size(); q++) {
        const SpinHit& h = v_[q];
        fa_[4 * q] = static_cast<float>(h.dir.x); fa_[4 * q + 1] = static_cast<float>(h.dir.y); fa_[4 * q + 2] = static_cast<float>(h.dir.z); fa_[4 * q + 3] = static_cast<float>(h.b);
        for (int k = 0; k < 4; k++) fb_[4 * q + k] = static_cast<float>(h.u[k]);
    }
    fe_.assign(edge_.begin(), edge_.end());
    ft_.assign(tau_.begin(), tau_.end());
}

SpinHit SpinTable::lookup(double psi, double chi) const {
    const double pe = edge(chi), x = psi - pe;
    const int side = x > 0 ? 0 : 1;
    const double ax = std::max(std::fabs(x), kSpinXlo);
    const double t = (sOf(ax) - sLo()) / (sHi(side, pe) - sLo()) * (nS_ - 1);
    double wc[4];
    const int j0 = stencil(t, nS_, wc);
    double q = tauOf(chi) * nChi_;
    q -= std::floor(q / nChi_) * nChi_;
    const int i = std::min(static_cast<int>(q), nChi_ - 1);
    double wr[4];
    cubicW(q - i, wr);
    SpinHit out;
    out.sky = side == 0;
    Vec3 d{0, 0, 0};
    for (int r = 0; r < 4; r++) {
        const int row = ((i - 1 + r) % nChi_ + nChi_) % nChi_;
        for (int c = 0; c < 4; c++) {
            const SpinHit& h = node(side, row, j0 + c);
            const double w = wr[r] * wc[c];
            d = d + h.dir * w;
            out.b += w * h.b;
            for (int k = 0; k < kSpinCross; k++) out.u[k] += w * h.u[k];
        }
    }
    const double L = length(d);
    out.dir = L > 0 ? d * (1 / L) : d;
    return out;
}

SpinDisc spinDisc(const SpinHit& h, double a, double rIn, double rOut) {
    SpinDisc d;
    for (int k = 0; k < kSpinCross; k++) {
        if (!(h.u[k] > 0)) continue;
        const double r = 1 / h.u[k];
        if (r >= rIn && r <= rOut) { d.order = k; d.r = r; d.g = discShiftKerr(r, h.b, a); return d; }
    }
    return d;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// the stars
int SpinStars::binOf(const Vec3& d) const {
    const double th = std::acos(std::clamp(d.z, -1.0, 1.0));
    double ph = std::atan2(d.y, d.x);
    if (ph < 0) ph += 2 * kPi;
    const int bi = std::min(static_cast<int>(th / kPi * nb_), nb_ - 1), bj = std::min(static_cast<int>(ph / (2 * kPi) * 2 * nb_), 2 * nb_ - 1);
    return bi * 2 * nb_ + bj;
}

SpinStars::SpinStars(const SpinTable& t, int threads) : T_(t) {
    const auto t0 = std::chrono::steady_clock::now();
    nb_ = 90;
    if (threads == kReference) { buildReference(); ms_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); return; }
    const int nChi = t.nChi(), nS = t.nS(), nBins = nb_ * 2 * nb_;
    // 0.55.6: each node's |x| (and so its psi) once, not again for every cell that has it as a corner; the rows on all cores
    // and its direction on the camera's sky (from psi and chi rounded to floats, as the triangles keep them)
    std::vector<double> ax(static_cast<std::size_t>(nChi) * nS);
    std::vector<Vec3> cam(static_cast<std::size_t>(nChi) * nS);
    parallelFor(nChi, [&](int i) {
        const double pe = t.rowEdge(i);
        const float chF = static_cast<float>(t.rowChi(i));
        for (int j = 0; j < nS; j++) {
            const std::size_t q = static_cast<std::size_t>(i) * nS + j;
            ax[q] = SpinTable::absXOf(t.sLo() + (t.sHi(0, pe) - t.sLo()) * j / (nS - 1));
            cam[q] = lookAt(static_cast<float>(pe + ax[q]), chF);
        }
    }, threads);
    // the first column whose |x| reaches kStarXmin (the same for every row: s is spaced from the common sLo)
    const auto colX = [&](int i, int j) { return ax[static_cast<std::size_t>(i) * nS + j]; };
    // the triangles of each row (cells (i, j) to (i + 1, j + 1)), with where each lies on the far sky; numbered afterwards in the order of the rows
    struct Cover { int b0 = 0, b1 = 0, j0 = 0, j1 = 0; bool all = false; };
    std::vector<std::vector<Tri>> rowTri(static_cast<std::size_t>(nChi));
    std::vector<std::vector<Cover>> rowCov(static_cast<std::size_t>(nChi));
    parallelFor(nChi, [&](int i) {
        const int i1 = (i + 1) % nChi;
        std::vector<Tri>& RT = rowTri[static_cast<std::size_t>(i)];
        std::vector<Cover>& RC = rowCov[static_cast<std::size_t>(i)];
        RT.reserve(2 * static_cast<std::size_t>(nS)); RC.reserve(2 * static_cast<std::size_t>(nS));
        for (int j = 0; j + 1 < nS; j++) {
            if (colX(i, j) < kStarXmin || colX(i1, j) < kStarXmin) continue;
            const int ci[4] = {i, i1, i, i1}, cj[4] = {j, j, j + 1, j + 1};
            Vec3 d[4], c[4];
            float ps[4], ch[4];
            bool good = true;
            for (int k = 0; k < 4; k++) {
                const SpinHit& h = t.node(0, ci[k], cj[k]);
                if (!h.sky || length(h.dir) == 0) good = false;
                d[k] = h.dir;
                ps[k] = static_cast<float>(t.rowEdge(ci[k]) + colX(ci[k], cj[k]));   // (nodePsi on side 0: the edge plus |x|)
                ch[k] = static_cast<float>(t.rowChi(ci[k]));
                c[k] = cam[static_cast<std::size_t>(ci[k]) * nS + cj[k]];
            }
            if (!good) continue;
            if (i1 == 0) { ch[1] = static_cast<float>(ch[1] + 2 * kPi); ch[3] = static_cast<float>(ch[3] + 2 * kPi); }   // (the last row's neighbour: one whole turn on)
            const int tris[2][3] = {{0, 1, 2}, {1, 3, 2}};
            for (const auto& tr : tris) {
                Tri T;
                for (int k = 0; k < 3; k++) { T.psi[k] = ps[tr[k]]; T.chi[k] = ch[tr[k]]; T.d[k] = d[tr[k]]; }
                T.omegaCam = triSolid(c[tr[0]], c[tr[1]], c[tr[2]]);
                Vec3 m = T.d[0] + T.d[1] + T.d[2];
                const double Lm = length(m);
                if (!(Lm > 1e-9)) continue;
                m = m * (1 / Lm);
                double R = 0;
                for (int k = 0; k < 3; k++) R = std::max(R, std::acos(std::clamp(dot(m, T.d[k]), -1.0, 1.0)));
                if (R > 0.5) continue;                                       // a cell stretched over more than about 30 degrees of the far sky: not a straight map
                const double th = std::acos(std::clamp(m.z, -1.0, 1.0)), lo = std::max(0.0, th - R), hi = std::min(kPi, th + R);
                Cover cv;
                cv.b0 = std::min(static_cast<int>(lo / kPi * nb_), nb_ - 1); cv.b1 = std::min(static_cast<int>(hi / kPi * nb_), nb_ - 1);
                double ph = std::atan2(m.y, m.x);
                if (ph < 0) ph += 2 * kPi;
                const bool pole = th - R <= 0 || th + R >= kPi;
                const double dph = pole ? kPi : std::asin(std::min(1.0, std::sin(R) / std::sin(th))) + 1e-9;
                cv.all = pole || dph >= kPi;
                if (!cv.all) { cv.j0 = static_cast<int>(std::floor((ph - dph) / (2 * kPi) * 2 * nb_)); cv.j1 = static_cast<int>(std::floor((ph + dph) / (2 * kPi) * 2 * nb_)); }
                RT.push_back(T);
                RC.push_back(cv);
            }
        }
    }, threads);
    std::vector<std::size_t> rowAt(static_cast<std::size_t>(nChi) + 1, 0);
    for (int i = 0; i < nChi; i++) rowAt[static_cast<std::size_t>(i) + 1] = rowAt[static_cast<std::size_t>(i)] + rowTri[static_cast<std::size_t>(i)].size();
    tri_.resize(rowAt.back());
    std::vector<Cover> cov(rowAt.back());
    parallelFor(nChi, [&](int i) {
        std::copy(rowTri[static_cast<std::size_t>(i)].begin(), rowTri[static_cast<std::size_t>(i)].end(), tri_.begin() + static_cast<std::ptrdiff_t>(rowAt[static_cast<std::size_t>(i)]));
        std::copy(rowCov[static_cast<std::size_t>(i)].begin(), rowCov[static_cast<std::size_t>(i)].end(), cov.begin() + static_cast<std::ptrdiff_t>(rowAt[static_cast<std::size_t>(i)]));
    }, threads);
    // the bins: each band of bins (one theta row) counted and filled by one thread, going through the triangles in the order of their numbers, so a bin
    // lists them as the one-thread build did (a triangle wider than the whole turn in phi is listed in a bin as often as that build listed it)
    const int nTri = static_cast<int>(tri_.size());
    std::vector<int> count(static_cast<std::size_t>(nBins) + 1, 0);
    // the triangles of each band, in id order (a counting sort)
    std::vector<int> bandStart(static_cast<std::size_t>(nb_) + 1, 0);
    for (const Cover& cv : cov) for (int bi = cv.b0; bi <= cv.b1; bi++) bandStart[static_cast<std::size_t>(bi) + 1]++;
    for (int bi = 0; bi < nb_; bi++) bandStart[static_cast<std::size_t>(bi) + 1] += bandStart[static_cast<std::size_t>(bi)];
    std::vector<int> bandIds(static_cast<std::size_t>(bandStart.back()));
    {   std::vector<int> at(bandStart.begin(), bandStart.end() - 1);
        for (int id = 0; id < nTri; id++) {
            const Cover& cv = cov[static_cast<std::size_t>(id)];
            for (int bi = cv.b0; bi <= cv.b1; bi++) bandIds[static_cast<std::size_t>(at[static_cast<std::size_t>(bi)]++)] = id;
        } }
    const auto visit = [&](int bi, const std::function<void(int, int)>& put) {   // every (bin, id) of band bi, in id order
        for (int q = bandStart[static_cast<std::size_t>(bi)]; q < bandStart[static_cast<std::size_t>(bi) + 1]; q++) {
            const int id = bandIds[static_cast<std::size_t>(q)];
            const Cover& cv = cov[static_cast<std::size_t>(id)];
            if (cv.all) { for (int bj = 0; bj < 2 * nb_; bj++) put(bi * 2 * nb_ + bj, id); continue; }
            for (int bj = cv.j0; bj <= cv.j1; bj++) put(bi * 2 * nb_ + ((bj % (2 * nb_)) + 2 * nb_) % (2 * nb_), id);
        }
    };
    parallelFor(nb_, [&](int bi) { visit(bi, [&](int b, int) { count[static_cast<std::size_t>(b) + 1]++; }); }, threads);
    binStart_.assign(static_cast<std::size_t>(nBins) + 1, 0);
    for (int b = 0; b < nBins; b++) binStart_[static_cast<std::size_t>(b) + 1] = binStart_[static_cast<std::size_t>(b)] + count[static_cast<std::size_t>(b) + 1];
    binIds_.assign(static_cast<std::size_t>(binStart_.back()), 0);
    parallelFor(nb_, [&](int bi) {
        std::vector<int> at(binStart_.begin() + bi * 2 * nb_, binStart_.begin() + (bi + 1) * 2 * nb_);
        visit(bi, [&](int b, int id) { binIds_[static_cast<std::size_t>(at[static_cast<std::size_t>(b - bi * 2 * nb_)]++)] = id; });
    }, threads);
    ms_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

// 0.55.5's build, on one thread (the tests' reference for the build above): lists per bin, packed into the same arrays at the end
void SpinStars::buildReference() {
    const SpinTable& t = T_;
    std::vector<std::vector<int>> bins(static_cast<std::size_t>(nb_) * 2 * nb_);
    const int nChi = t.nChi(), nS = t.nS();
    const auto colX = [&](int i, int j) { const double pe = t.rowEdge(i); return SpinTable::absXOf(t.sLo() + (t.sHi(0, pe) - t.sLo()) * j / (nS - 1)); };
    for (int i = 0; i < nChi; i++) {
        const int i1 = (i + 1) % nChi;
        for (int j = 0; j + 1 < nS; j++) {
            if (colX(i, j) < kStarXmin || colX(i1, j) < kStarXmin) continue;
            const int ci[4] = {i, i1, i, i1}, cj[4] = {j, j, j + 1, j + 1};
            Vec3 d[4], c[4];
            float ps[4], ch[4];
            bool good = true;
            for (int k = 0; k < 4; k++) {
                const SpinHit& h = t.node(0, ci[k], cj[k]);
                if (!h.sky || length(h.dir) == 0) good = false;
                d[k] = h.dir;
                ps[k] = static_cast<float>(t.nodePsi(0, ci[k], cj[k]));
                ch[k] = static_cast<float>(t.rowChi(ci[k]));
                c[k] = lookAt(ps[k], ch[k]);
            }
            if (!good) continue;
            if (i1 == 0) { ch[1] = static_cast<float>(ch[1] + 2 * kPi); ch[3] = static_cast<float>(ch[3] + 2 * kPi); }
            const int tris[2][3] = {{0, 1, 2}, {1, 3, 2}};
            for (const auto& tr : tris) {
                Tri T;
                for (int k = 0; k < 3; k++) { T.psi[k] = ps[tr[k]]; T.chi[k] = ch[tr[k]]; T.d[k] = d[tr[k]]; }
                T.omegaCam = triSolid(c[tr[0]], c[tr[1]], c[tr[2]]);
                Vec3 m = T.d[0] + T.d[1] + T.d[2];
                const double Lm = length(m);
                if (!(Lm > 1e-9)) continue;
                m = m * (1 / Lm);
                double R = 0;
                for (int k = 0; k < 3; k++) R = std::max(R, std::acos(std::clamp(dot(m, T.d[k]), -1.0, 1.0)));
                if (R > 0.5) continue;
                const int id = static_cast<int>(tri_.size());
                tri_.push_back(T);
                const double th = std::acos(std::clamp(m.z, -1.0, 1.0)), lo = std::max(0.0, th - R), hi = std::min(kPi, th + R);
                const int b0 = std::min(static_cast<int>(lo / kPi * nb_), nb_ - 1), b1 = std::min(static_cast<int>(hi / kPi * nb_), nb_ - 1);
                double ph = std::atan2(m.y, m.x);
                if (ph < 0) ph += 2 * kPi;
                const bool pole = th - R <= 0 || th + R >= kPi;
                const double dph = pole ? kPi : std::asin(std::min(1.0, std::sin(R) / std::sin(th))) + 1e-9;
                for (int bi = b0; bi <= b1; bi++) {
                    if (pole || dph >= kPi) { for (int bj = 0; bj < 2 * nb_; bj++) bins[static_cast<std::size_t>(bi) * 2 * nb_ + bj].push_back(id); continue; }
                    const int j0 = static_cast<int>(std::floor((ph - dph) / (2 * kPi) * 2 * nb_)), j1 = static_cast<int>(std::floor((ph + dph) / (2 * kPi) * 2 * nb_));
                    for (int bj = j0; bj <= j1; bj++) bins[static_cast<std::size_t>(bi) * 2 * nb_ + ((bj % (2 * nb_)) + 2 * nb_) % (2 * nb_)].push_back(id);
                }
            }
        }
    }
    binStart_.assign(bins.size() + 1, 0);
    for (std::size_t b = 0; b < bins.size(); b++) binStart_[b + 1] = binStart_[b] + static_cast<int>(bins[b].size());
    binIds_.clear();
    for (const std::vector<int>& b : bins) binIds_.insert(binIds_.end(), b.begin(), b.end());
}

int SpinStars::images(const Vec3& s, std::vector<SpinImage>& out) const {
    int n = 0;
    const int bin = binOf(s);
    for (int q = binStart_[static_cast<std::size_t>(bin)]; q < binStart_[static_cast<std::size_t>(bin) + 1]; q++) {
        const Tri& T = tri_[static_cast<std::size_t>(binIds_[static_cast<std::size_t>(q)])];
        const double l0 = dot(s, cross(T.d[1], T.d[2])), l1 = dot(s, cross(T.d[2], T.d[0])), l2 = dot(s, cross(T.d[0], T.d[1]));
        if (!((l0 >= 0 && l1 >= 0 && l2 >= 0) || (l0 <= 0 && l1 <= 0 && l2 <= 0))) continue;
        if (dot(s, T.d[0] + T.d[1] + T.d[2]) <= 0) continue;
        const double sum = l0 + l1 + l2;
        if (sum == 0) continue;
        const double w0 = l0 / sum, w1 = l1 / sum, w2 = l2 / sum;
        SpinImage im;
        double psi = w0 * T.psi[0] + w1 * T.psi[1] + w2 * T.psi[2], chi = w0 * T.chi[0] + w1 * T.chi[1] + w2 * T.chi[2];
        // Newton on the table's cubic: the residual across s on two unit vectors e1, e2 perpendicular to s
        Vec3 e1 = std::fabs(s.z) < 0.9 ? cross(s, Vec3{0, 0, 1}) : cross(s, Vec3{1, 0, 0});
        e1 = e1 * (1 / length(e1));
        const Vec3 e2 = cross(s, e1);
        double mag = 0;
        for (int it = 0; it < 3; it++) {                                   // (two Newton steps from the triangle's linear map: well inside the cubic's own error)
            const double h = 1e-7;
            const Vec3 d0 = T_.lookup(psi, chi).dir, dp = T_.lookup(psi + h, chi).dir, dc = T_.lookup(psi, chi + h).dir;
            const Vec3 gp = (dp - d0) * (1 / h), gc = (dc - d0) * (1 / h);
            mag = std::sin(psi) / length(cross(gp, gc));
            if (it == 2) break;
            const double r1 = dot(d0, e1), r2 = dot(d0, e2), a11 = dot(gp, e1), a12 = dot(gc, e1), a21 = dot(gp, e2), a22 = dot(gc, e2), det = a11 * a22 - a12 * a21;
            if (det == 0) break;
            psi -= (a22 * r1 - a12 * r2) / det;
            chi -= (-a21 * r1 + a11 * r2) / det;
        }
        const double psiT = w0 * T.psi[0] + w1 * T.psi[1] + w2 * T.psi[2], chiT = w0 * T.chi[0] + w1 * T.chi[1] + w2 * T.chi[2];
        const double cell = std::fabs(T.psi[1] - T.psi[0]) + std::fabs(T.psi[2] - T.psi[0]) + std::fabs(T.chi[1] - T.chi[0]) + std::fabs(T.chi[2] - T.chi[0]);
        if (std::isfinite(psi) && std::isfinite(chi) && std::isfinite(mag) && std::fabs(psi - psiT) + std::fabs(chi - chiT) <= 2 * cell && psi > 0 && psi < kPi) {
            im.psi = psi; im.chi = chi; im.mag = mag;
        } else {                                                            // (the triangle's own linear map, if Newton left it)
            const double omegaFar = triSolid(T.d[0], T.d[1], T.d[2]);
            im.psi = psiT; im.chi = chiT; im.mag = omegaFar > 0 ? T.omegaCam / omegaFar : 0;
        }
        out.push_back(im);
        n++;
    }
    return n;
}

}  // namespace blackhole
