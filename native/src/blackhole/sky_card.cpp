#include "blackhole/sky_card.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "blackhole/disc.h"
#include "blackhole/elliptic.h"
#include "blackhole/kerr.h"
#include "blackhole/kerr_closed.h"
#include "blackhole/looks.h"
#include "blackhole/low_priority.h"
#include "blackhole/spin.h"
#include "blackhole/spin_sky.h"
#include "render/gl_loader.h"

namespace blackhole {

namespace {
constexpr double kPi = 3.14159265358979323846;
double msSince(std::chrono::steady_clock::time_point t0) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); }
}  // namespace

void skyCamera(render::Renderer2D::LensSky& L, double a, double r, double th) {
    const KerrMetric m = kerrMetric(a, r, th);
    const double rp = horizon(a);
    const float cam[4] = {static_cast<float>(a), static_cast<float>(r), static_cast<float>(std::cos(th)), static_cast<float>(std::sin(th))};
    const float met[4] = {static_cast<float>(m.alpha), static_cast<float>(m.omega), static_cast<float>(m.varpi), static_cast<float>(std::sqrt(m.rho2))};
    const float met2[4] = {static_cast<float>(std::sqrt(m.Delta)), static_cast<float>(rp), static_cast<float>(a * a / rp), static_cast<float>(rp + 1e-3)};
    std::copy(cam, cam + 4, L.skyCam); std::copy(met, met + 4, L.skyMet); std::copy(met2, met2 + 4, L.skyMet2);
    L.spin = a;
}

render::Renderer2D::LensSky skyProbeLens(const SkyCase& c) {
    render::Renderer2D::LensSky L;
    L.cx = c.w / 2.0; L.cy = c.h / 2.0; L.kRad = c.K;
    L.bend = true; L.kerrSky = true; L.sky = false;
    L.kO[0] = 0; L.kO[1] = 1; L.kO[2] = -1; L.kO[3] = 0;                 // e_theta up, e_phi left
    L.rIn = c.rIn; L.rOut = c.rOut;
    skyCamera(L, c.a, c.r, c.th);
    double l0, dl;                                                         // 0.58.0: the light check's colour table (skyLightCompare)
    L.discCol = &cardColourTable(L.discC, l0, dl);
    L.colLog0 = l0; L.colDlog = dl; L.colVersion = 1;
    L.allInvTsun = 1 / sunTeff();
    return L;
}

const std::vector<float>& cardColourTable(int& n, double& log0, double& dlog) {
    static const int N = 512;
    static const double l0 = std::log10(kTmin), dl = (std::log10(kTmax) - l0) / (N - 1);
    static const std::vector<float> c = [] {
        std::vector<float> v(3 * N);
        for (int i = 0; i < N; i++) {
            const auto l = blackBodyLinear(std::pow(10.0, l0 + i * dl));
            for (int k = 0; k < 3; k++) v[3 * i + k] = static_cast<float>(l[k]);
        }
        return v;
    }();
    n = N; log0 = l0; dlog = dl;
    return c;
}

void skyLightCompare(const std::vector<float>& P, int w, int h, SkyLight out[2]) {
    out[0] = out[1] = SkyLight{};
    if (P.size() < static_cast<std::size_t>(32) * w * h) return;
    int nC; double l0d, dld;
    const std::vector<float>& tab = cardColourTable(nC, l0d, dld);
    const double u = std::ldexp(1.0, -24), l0 = static_cast<float>(l0d), dl = static_cast<float>(dld), lg = static_cast<float>(0.43429448);
    const double ua = static_cast<float>(1 / sunTeff());
    for (int m = 0; m < 2; m++)
        for (int j = 0; j < h; j++)
            for (int i = 0; i < w; i++) {
                const float* p = &P[(static_cast<std::size_t>(h + j) * 4 * w + static_cast<std::size_t>(m * w + i)) * 4];
                const double T = p[3];
                SkyLight& o = out[m];
                o.px++;
                if (!(T > 0)) { o.bad++; o.worst = 1e9; continue; }
                const double lnT = std::log(T), ulpLn = std::ldexp(1.0, std::ilogb(lnT) - 23);
                double c = (lnT * lg - l0) / dl;
                if (c < 0) continue;                                       // (below the table: no light on either side; T here is 10^3 K or more)
                c = std::min(c, nC - 1 - 0.001);
                const int k = static_cast<int>(c);
                const double f = c - k;
                double ref[3], top = 0, D = 0;
                for (int q = 0; q < 3; q++) {
                    const double a = tab[3 * static_cast<std::size_t>(k) + q], b = tab[3 * static_cast<std::size_t>(k + 1) + q];
                    ref[q] = a + f * (b - a);
                    top = std::max(top, ref[q]);
                    D = std::max(D, std::fabs(b - a));
                }
                if (!(top > 0)) continue;
                D /= top;
                const double dc = (3 * ulpLn * lg + 4 * u * std::fabs(lnT * lg) + 2 * u * std::fabs(c * dl)) / dl;
                double lim = D * dc + 3 * u;
                if (m == 1) {
                    const double y = kLumR * ref[0] + kLumG * ref[1] + kLumB * ref[2], q = T * ua, k4 = q * q * q * q / y;
                    for (double& v : ref) v *= k4;
                    top *= k4;
                    lim = 2 * lim + 15 * u;
                }
                lim *= 2;
                double e = 0;
                for (int q = 0; q < 3; q++) e = std::max(e, std::fabs(p[q] - ref[q]) / top);
                o.worst = std::max(o.worst, e / lim);
                if (e > lim) o.bad++;
            }
}

// 0.57.2: the reference (traceClosed at every pixel's centre, the ends, each pixel's distance from the edge) apart from the comparison: it needs no probe, so
// the start-up check makes it on a thread of its own while the card draws (blackhole_view.cpp skyReady), and the frame only compares
SkyRef skyReference(const SkyCase& c) {
    SkyRef R;
    const auto t0 = std::chrono::steady_clock::now();
    const int w = c.w, h = c.h, n = w * h;
    const Camera cam{c.r, c.th, 0};
    const double cx = std::round(w / 2.0), cy = h - std::round(h / 2.0);
    std::vector<ClosedEnd>& ref = R.ref;
    std::vector<signed char>& kind = R.kind;
    ref.resize(static_cast<std::size_t>(n));
    kind.resize(static_cast<std::size_t>(n));
    const auto row = [&](int j) {
        for (int i = 0; i < w; i++) {
            const double ox = i + 0.5 - cx, oy = j + 0.5 - cy, d = std::hypot(ox, oy), th = std::min(d / c.K, kPi);
            const double chi = std::atan2(d > 0 ? -ox / d : 0, d > 0 ? oy / d : 1);
            const ClosedEnd e = traceClosed(c.a, cam, lookAt(th, chi), kClosedCross);
            ref[static_cast<std::size_t>(j * w + i)] = e;
            kind[static_cast<std::size_t>(j * w + i)] = static_cast<signed char>(e.kind == RayEnd::Sky ? 1 : e.kind == RayEnd::Horizon ? 2 : 0);
        }
    };
    if (n > 20000) parallelFor(h, row);                                    // (the full-size pictures of the tests)
    else for (int j = 0; j < h; j++) row(j);
    const auto edgeDist = [&](int i, int j) {
        const int k0 = kind[static_cast<std::size_t>(j * w + i)];
        double best = 12;
        for (int jj = std::max(0, j - 12); jj <= std::min(h - 1, j + 12); jj++)
            for (int ii = std::max(0, i - 12); ii <= std::min(w - 1, i + 12); ii++)
                if (kind[static_cast<std::size_t>(jj * w + ii)] != k0) best = std::min(best, std::hypot(ii - i, jj - j) - 0.5);
        return best;
    };
    std::vector<float>& edge = R.edge;
    edge.resize(static_cast<std::size_t>(n));
    const auto edgeRow = [&](int j) { for (int i = 0; i < w; i++) edge[static_cast<std::size_t>(j * w + i)] = static_cast<float>(edgeDist(i, j)); };
    if (n > 20000) parallelFor(h, edgeRow);
    else for (int j = 0; j < h; j++) edgeRow(j);
    R.ms = msSince(t0);
    return R;
}

SkyCompare skyCompare(const SkyCase& c, const std::vector<float>& P, double dirLimit, double rLimit) {
    if (P.size() < static_cast<std::size_t>(16) * c.w * c.h) return SkyCompare{};
    return skyCompare(c, skyReference(c), P, dirLimit, rLimit);
}

SkyCompare skyCompare(const SkyCase& c, const SkyRef& R, const std::vector<float>& P, double dirLimit, double rLimit) {
    SkyCompare m;
    const auto t0 = std::chrono::steady_clock::now();
    const int w = c.w, h = c.h, n = w * h;
    if (P.size() < static_cast<std::size_t>(16) * n || R.ref.size() != static_cast<std::size_t>(n)) return m;
    const std::vector<ClosedEnd>& ref = R.ref;
    const std::vector<signed char>& kind = R.kind;
    const std::vector<float>& edge = R.edge;
    const bool dump = std::getenv("PL_BH_SKYDUMP") != nullptr;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            const std::size_t q = static_cast<std::size_t>(j * w + i);
            const ClosedEnd& e = ref[q];
            const float* p1 = &P[(static_cast<std::size_t>(j) * 4 * w + i) * 4];               // (dir, xpx)
            const float* p2 = &P[(static_cast<std::size_t>(j) * 4 * w + w + i) * 4];           // (order, r, g, xpx)
            const float* p3 = &P[(static_cast<std::size_t>(j) * 4 * w + 2 * w + i) * 4];       // (kind, b, q, sub)
            m.px++;
            if (kind[q] == 0) { m.refLost++; continue; }
            const int gk = static_cast<int>(std::lround(p3[0]));
            if (gk == 0) {
                m.lost++;
                if (dump) std::printf("SKYLOST %s i=%d j=%d sub=%d refsub=%d b=%.6g q=%.6g gb=%.6g gq=%.6g edge=%.1f\n", c.name, i, j, static_cast<int>(p3[3]), e.radial, e.b, e.q, p3[1], p3[2], edge[q]);
                continue;
            }
            const double ed = edge[q];
            if (gk != kind[q]) {
                m.ends++;
                if (ed > 1.5) { m.endsFar++; m.endsFarPx = std::max(m.endsFarPx, ed);
                    if (dump) std::printf("SKYENDS %s i=%d j=%d kind=%d refkind=%d sub=%d refsub=%d b=%.6g q=%.6g edge=%.1f\n", c.name, i, j, gk, kind[q], static_cast<int>(p3[3]), e.radial, e.b, e.q, ed); }
                continue;
            }
            m.b = std::max(m.b, std::fabs(p3[1] - e.b) / std::max(1.0, std::fabs(e.b)));
            if (gk == 1) {
                m.sky++;
                const Vec3 g{p1[0], p1[1], p1[2]};
                const double ang = std::atan2(length(cross(g, e.dir)), dot(g, e.dir));
                if (ed >= 2) {
                    m.dir = std::max(m.dir, ang);
                    if (ang > dirLimit) { m.dirBad++; m.dirBadFar = std::max(m.dirBadFar, ed);
                        if (dump) std::printf("SKYDIRBAD %s i=%d j=%d ang=%.3g sub=%d refsub=%d b=%.6g q=%.6g edge=%.1f\n", c.name, i, j, ang, static_cast<int>(p3[3]), e.radial, e.b, e.q, ed); }
                } else m.dirNear = std::max(m.dirNear, ang);
            }
            // 0.56.2: every crossing of the disc's plane the reference makes (up to four) at a radius in [rIn, rOut] on either side: 1/u within rLimit
            if (ed >= 2) {
                const float* p4 = &P[(static_cast<std::size_t>(j) * 4 * w + 3 * w + i) * 4];   // (u0..u3)
                for (int k = 0; k < 4; k++) {
                    const bool rk = k < e.nCross && e.u[k] > 0, gIn = p4[k] > 0 && 1 / p4[k] >= c.rIn && 1 / p4[k] <= c.rOut;
                    const bool rIn = rk && 1 / e.u[k] >= c.rIn && 1 / e.u[k] <= c.rOut;
                    if (!rIn && !(gIn && (gk == 1 || k < e.nCross))) continue;
                    const double rr = rk ? 1 / e.u[k] : 0, rel = rk && p4[k] > 0 ? std::fabs(1 / p4[k] - rr) / rr : 1;
                    m.u = std::max(m.u, rel);
                    if (rel > rLimit) { m.uBad++;
                        if (dump) std::printf("SKYUBAD %s i=%d j=%d k=%d r=%.6g gr=%.6g sub=%d refsub=%d b=%.6g q=%.6g edge=%.1f\n", c.name, i, j, k, rr, p4[k] > 0 ? 1 / p4[k] : 0.0, static_cast<int>(p3[3]), e.radial, e.b, e.q, ed); }
                }
            }
            // the disc's first image in [rIn, rOut]
            int ro = -1;
            double rr = 0;
            for (int k = 0; k < e.nCross && k < 4; k++) { const double r = 1 / e.u[k]; if (e.u[k] > 0 && r >= c.rIn && r <= c.rOut) { ro = k; rr = r; break; } }
            const int go = static_cast<int>(std::lround(p2[0]));
            if (ro < 0 && go < 0) continue;
            m.discPx++;
            if (ro != go) {
                const double rel = ro >= 0 ? std::min(std::fabs(rr - c.rIn), std::fabs(rr - c.rOut)) / rr
                                           : std::min(std::fabs(p2[1] - c.rIn), std::fabs(p2[1] - c.rOut)) / std::max(1.0, static_cast<double>(p2[1]));
                m.discWrong++;
                m.discWrongFar = std::max(m.discWrongFar, ed);
                if (ed >= 2 && rel > rLimit) m.discWrongOut++;   // (a radius within rLimit of the disc's edge is within the radius's own limit)
                if (dump && ed >= 2 && rel > rLimit) std::printf("SKYDISCWRONG %s i=%d j=%d order=%d reforder=%d r=%.6g gr=%.6g b=%.6g q=%.6g edge=%.1f\n", c.name, i, j, go, ro, rr, static_cast<double>(p2[1]), e.b, e.q, ed);
                m.discWrongEdge = std::min(m.discWrongEdge, rel);
                continue;
            }
            const double dr = std::fabs(p2[1] - rr) / rr, dg = std::fabs(p2[2] - discShiftKerr(rr, e.b, c.a)) / discShiftKerr(rr, e.b, c.a);
            if (ed >= 2) { m.r = std::max(m.r, dr); m.g = std::max(m.g, dg); if (dr > rLimit || dg > rLimit) { m.rBad++;
                if (dump) std::printf("SKYRBAD %s i=%d j=%d dr=%.3g dg=%.3g r=%.6g b=%.6g q=%.6g edge=%.1f\n", c.name, i, j, dr, dg, rr, e.b, e.q, ed); } }
        }
    m.ms = R.ms + msSince(t0);
    return m;
}

std::string skyCompareText(const SkyCase& c, const SkyCompare& m) {
    char b[900];
    std::snprintf(b, sizeof b, "case=%s a=%g dist=%g th=%.5f px=%ld sky=%ld ends=%ld endsfar=%ld endsfarpx=%.1f lost=%ld reflost=%ld dir=%.2e dirnear=%.2e dirbad=%ld "
                  "dirbadfar=%.1f b=%.2e discpx=%ld discwrong=%ld discwrongfar=%.1f discwrongedge=%.2e discwrongout=%ld r=%.2e g=%.2e rbad=%ld u=%.2e ubad=%ld",
                  c.name, c.a, c.r, c.th, m.px, m.sky, m.ends, m.endsFar, m.endsFarPx, m.lost, m.refLost, m.dir, m.dirNear, m.dirBad, m.dirBadFar, m.b, m.discPx,
                  m.discWrong, m.discWrongFar, m.discWrongEdge, m.discWrongOut, m.r, m.g, m.rBad, m.u, m.uBad);
    return b;
}

const SkyCase& skyCheckCase(int k) {
    static const FilmChoices film;
    static const SkyCase cases[2] = {{"benchmark", 0.9, 20, 80 * kPi / 180, kSkyCheckW, kSkyCheckH, kSkyCheckH / (60 * kPi / 180), 2.32, 30},
                                     {"film", film.spin, film.camR, film.camTheta, kSkyCheckW, kSkyCheckH, kSkyCheckH / (30 * kPi / 180), film.rIn, film.rOut}};
    return cases[k == 0 ? 0 : 1];
}

SkyCheck skyCheckOf(const std::vector<float>* probes[2], const SkyRef* refs) {
    SkyCheck c;
    const auto t0 = std::chrono::steady_clock::now();
    c.ok = true;
    double refMs = 0;
    for (int k = 0; k < 2; k++) {
        const SkyCase& cs = skyCheckCase(k);
        c.m[k] = refs ? skyCompare(cs, refs[k], *probes[k], kSkyDirLimit, kSkyRLimit) : skyCompare(cs, *probes[k], kSkyDirLimit, kSkyRLimit);
        if (refs) refMs += refs[k].ms;
        const SkyCompare& m = c.m[k];
        std::string bad;
        if (m.px != static_cast<long>(cs.w) * cs.h) bad = "no probe picture";
        else if (m.lost > 0) bad = std::to_string(m.lost) + " rays lost";
        else if (m.endsFar > 0) bad = std::to_string(m.endsFar) + " pixels end elsewhere away from the shadow's edge";
        else if (m.dirBad > 0) bad = "the far direction off by " + std::to_string(m.dir) + " rad";
        else if (m.b > 1e-4) bad = "b off by " + std::to_string(m.b);
        else if (m.rBad > 0) bad = "the disc's radius or shift off by " + std::to_string(std::max(m.r, m.g));
        SkyLight L[2];
        skyLightCompare(*probes[k], cs.w, cs.h, L);                        // 0.58.0: the light, both modes (summed over the two cases)
        for (int q = 0; q < 2; q++) { c.light[q].px += L[q].px; c.light[q].bad += L[q].bad; c.light[q].worst = std::max(c.light[q].worst, L[q].worst); }
        if (bad.empty() && (L[0].px != static_cast<long>(cs.w) * cs.h || L[1].px != static_cast<long>(cs.w) * cs.h)) bad = "no light probe";
        else if (bad.empty() && (L[0].bad > 0 || L[1].bad > 0))
            bad = "the disc's light off (" + std::to_string(L[0].bad) + " eye, " + std::to_string(L[1].bad) + " all light pixels over their limit)";
        if (!bad.empty() && c.ok) { c.ok = false; c.why = std::string("the start-up check on this graphics card failed (") + cs.name + ": " + bad + ")"; }
    }
    c.ms = msSince(t0) + refMs;
    return c;
}

// ---- tests ----
namespace {
double ellE(double s, double c, double kc2) {                          // E(phi | m), any phi in (-pi, pi] (as kerr_closed.cpp's nearR2)
    const double j = 1 - kc2, as = std::fabs(s), c2 = c * c, d2 = c2 + kc2 * s * s;
    double E = as * carlsonRF(c2, d2, 1) - j / 3 * as * as * as * carlsonRJ(c2, d2, 1, 1);
    if (c < 0) E = 2 * (carlsonRF(0, kc2, 1) - j / 3 * carlsonRJ(0, kc2, 1, 1)) - E;
    return s < 0 ? -E : E;
}
}  // namespace

void skyCompareHook(render::Renderer2D& R) {
    render::KerrSky& K = R.kerrSky();
    struct NoSteps : render::KerrSteps { void begin(const std::string&) override {} void end(const std::string&, const std::string&) override {} } steps;
    std::string why;
    for (int st = 0; st < render::KerrSky::kStages; st++)
        if (!K.build(st, &steps, why)) { std::printf("PLBHSKYNONE why=%s\n", why.c_str()); return; }
    std::printf("PLBHSKYBUILD tinyms=%.1f mathms=%.1f wholems=%.1f id=%s\n", K.buildMs(0), K.buildMs(1), K.buildMs(2), render::kerrSkyShaderId().c_str());
    // 0.56.2 (PL_BH_SKYTIME="w,h"): the pass's time at w x h, waited for (median of 5), uMode 1 (the ray and its far direction) and 2 (also the disc's first
    // image and its shift), at the benchmark's view (0.9, 20 M, 80 deg, 90 deg across the height) and the Film's (30 deg); with PL_BH_SKYSTOP the parts
    if (const char* tm = std::getenv("PL_BH_SKYTIME")) {
        int w = 2560, h = 1369;
        std::sscanf(tm, "%d,%d", &w, &h);
        const FilmChoices film;
        const SkyCase cases[2] = {{"benchmark", 0.9, 20, 80 * kPi / 180, w, h, (h / 2.0) / (90 * kPi / 360), 2.32, 30},
                                  {"film", film.spin, film.camR, film.camTheta, w, h, (h / 2.0) / (30 * kPi / 360), film.rIn, film.rOut}};
        for (const SkyCase& c : cases)
            for (int mode : {1, 2}) {
                double t[5];
                for (double& x : t) x = R.skyTimePass(skyProbeLens(c), w, h, mode);
                std::sort(t, t + 5);
                std::printf("PLBHSKYTIME case=%s mode=%d stop=%s w=%d h=%d ms=%.1f\n", c.name, mode, std::getenv("PL_BH_SKYSTOP") ? std::getenv("PL_BH_SKYSTOP") : "-", w, h, t[2]);
            }
        std::fflush(stdout);
        return;
    }
    // 0.56.2 (PL_BH_SKYFULL="w,h,fov[,deg...]"): the Film's hole at a whole window's size, every pixel against traceClosed; the cameras at the given angles
    // from the spin axis (default: the paper's 86.56 and 88, 90, 92 degrees); only this part runs
    if (const char* full = std::getenv("PL_BH_SKYFULL")) {
        const FilmChoices film;
        double v[16] = {2560, 1369, 30};
        int nv = 0;
        for (const char* p = full; *p && nv < 16; ) {
            char* e = nullptr;
            v[nv++] = std::strtod(p, &e);
            if (e == p) break;
            p = *e == ',' ? e + 1 : e;
        }
        std::vector<double> ths;
        for (int k = 3; k < nv; k++) ths.push_back(v[k]);
        if (ths.empty()) ths = {film.camThetaDeg, 88, 90, 92};
        const int w = static_cast<int>(v[0]), h = static_cast<int>(v[1]);
        for (double deg : ths) {
            char name[32];
            std::snprintf(name, sizeof name, "film%.2f", deg);
            const SkyCase c{name, film.spin, film.camR, deg * kPi / 180, w, h, (h / 2.0) / (v[2] * kPi / 360), film.rIn, film.rOut};
            std::vector<float> probe;
            R.skyProbe(skyProbeLens(c), c.w, c.h, false, &probe);
            const SkyCompare m = skyCompare(c, probe, kSkyDirLimit, kSkyRLimit);
            std::printf("PLBHSKYFULL fov=%g %s\n", v[2], skyCompareText(c, m).c_str());
            std::fflush(stdout);
        }
        return;
    }
    // 1. the elliptic functions: R_F, R_J, R_C on arguments over 12 decades, am(u | m) for m from -4 to 1 - 1e-6, F, Pi (n < 1, and n > 1 on both sides of the
    //    pole), E for amplitudes over (-pi, pi]. Errors relative to the value (am: absolute, over 1 + |u|); Pi's error relative to max(|Pi|, |F|) (near the
    //    pole Pi's own size is the log's, which the antiderivative carries exactly; its single-precision rounding is that of the terms)
    {
        std::vector<float> in;
        std::vector<double> ref;
        const double xs[] = {0, 1e-9, 1e-5, 0.02, 0.3, 1, 2.5, 40, 1e3};
        for (double x : xs) for (double y : {1e-6, 0.4, 1.0, 7.0}) for (double z : {0.0, 1.0, 3.0}) for (double p : {1e-3, 0.5, 1.0, 9.0}) {
            if (x + z == 0) continue;
            in.insert(in.end(), {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z), static_cast<float>(p), 0, 1, 0, 1, 1, 0, 0, 0});
            ref.insert(ref.end(), {carlsonRF(x, y, z), carlsonRJ(x, y, z, p), carlsonRC(x, y), 0, 0, 0, 0, 0});
        }
        const int nRC = static_cast<int>(ref.size() / 8);
        for (double kc2 : {1e-6, 1e-3, 0.2, 0.7, 0.999, 1.0, 1.5, 5.0})
            for (int iu = 0; iu <= 40; iu++) {
                const double u = -12 + 24.0 * iu / 40;
                for (int ip = 0; ip < 12; ip++) {
                    const double ph = -kPi + 2 * kPi * (ip + 0.37) / 12, s = std::sin(ph), c = std::cos(ph);
                    for (double n : {-3.0, 0.4, 0.97, 1.6, 4.0}) {
                        if (iu % 8 != 0) { if (ip > 0 || n != 0.4) continue; }   // (am on the fine u grid; the integrals on the coarse one)
                        in.insert(in.end(), {1, 1, 1, 1, static_cast<float>(n), static_cast<float>(1 - n), static_cast<float>(s), static_cast<float>(c),
                                             static_cast<float>(kc2), static_cast<float>(u), 0, 0});
                        ref.insert(ref.end(), {0, 0, 0, kc2 <= 1 && kc2 >= 1 ? u : jacobiAm(u, kc2), ellFang(s, c, kc2), ellPiang(n, s, c, kc2), ellE(s, c, kc2), ellKc(kc2)});
                    }
                }
            }
        std::vector<float> out;
        R.skyMath(in, out);
        double e[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        const char* names[8] = {"rf", "rj", "rc", "am", "f", "pi", "e", "k"};
        const int n = static_cast<int>(ref.size() / 8);
        for (int i = 0; i < n; i++)
            for (int k = 0; k < 8; k++) {
                if ((i < nRC) != (k < 3)) continue;
                const double r = ref[static_cast<std::size_t>(8 * i + k)], g = out[static_cast<std::size_t>(8 * i + k)];
                if (!std::isfinite(r)) continue;
                double d;
                if (k == 3) d = std::fabs(g - r) / (1 + std::fabs(in[static_cast<std::size_t>(12 * i + 9)]));
                else if (k == 5) d = std::fabs(g - r) / std::max({std::fabs(r), std::fabs(ref[static_cast<std::size_t>(8 * i + 4)]), 1e-30});
                else d = std::fabs(g - r) / std::max(std::fabs(r), 1e-30);
                e[k] = std::max(e[k], std::isfinite(d) ? d : 1.0);
                if (std::getenv("PL_BH_SKYMATHDUMP") && d > 1e-5) std::printf("SKYMATHBAD k=%d kc2=%g u=%g n=%g s=%g c=%g ref=%.9g got=%.9g\n", k, in[12 * i + 8], in[12 * i + 9], in[12 * i + 4], in[12 * i + 6], in[12 * i + 7], r, g);
            }
        std::printf("PLBHSKYMATH n=%d", n);
        for (int k = 0; k < 8; k++) std::printf(" %s=%.2e", names[k], e[k]);
        std::printf("\n");
    }
    // 2. pictures per pixel against traceClosed in double: the benchmark's case, the Film's, spin 0.99 close in, a camera in the disc's plane
    {
        const FilmChoices film;
        const SkyCase cases[] = {{"benchmark", 0.9, 20, 80 * kPi / 180, 320, 200, 200 / (90 * kPi / 180), 2.32, 30},
                                 {"film", film.spin, film.camR, film.camTheta, 320, 200, 200 / (30 * kPi / 180), film.rIn, film.rOut},
                                 {"top", kSpinTop, 6, 1.4, 320, 200, 200 / (120 * kPi / 180), 1.46, 30},
                                 {"plane", 0.9, 20, kPi / 2, 320, 200, 200 / (60 * kPi / 180), 2.32, 30}};
        for (const SkyCase& c : cases) {
            std::vector<float> probe;
            R.skyProbe(skyProbeLens(c), c.w, c.h, false, &probe);
            const SkyCompare m = skyCompare(c, probe, kSkyDirLimit, kSkyRLimit);
            std::printf("PLBHSKYPIC %s\n", skyCompareText(c, m).c_str());
        }
    }
    {   std::vector<float> a, b;
        R.skyProbe(skyProbeLens(skyCheckCase(0)), kSkyCheckW, kSkyCheckH, false, &a);
        R.skyProbe(skyProbeLens(skyCheckCase(1)), kSkyCheckW, kSkyCheckH, false, &b);
        const std::vector<float>* p[2] = {&a, &b};
        const SkyCheck c = skyCheckOf(p);
        std::printf("PLBHSKYCHECK ok=%d ms=%.1f why=%s\n", c.ok ? 1 : 0, c.ms, c.ok ? "-" : c.why.c_str());
        for (int k = 0; k < 2; k++) std::printf("PLBHSKYCHECKCASE %s\n", skyCompareText(skyCheckCase(k), c.m[k]).c_str());
        std::printf("PLBHSKYCHECKLIGHT eyepx=%ld eyebad=%ld eyeworst=%.3f allpx=%ld allbad=%ld allworst=%.3f\n", c.light[0].px, c.light[0].bad, c.light[0].worst,
                    c.light[1].px, c.light[1].bad, c.light[1].worst);
    }
    std::fflush(stdout);
}

}  // namespace blackhole
