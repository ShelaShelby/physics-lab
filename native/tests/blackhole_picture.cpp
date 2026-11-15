// Black Hole Lab (0.54.3): the reference picture and the speed measurements (a tool run by hand; not a test, not part of the app).
//   blackhole_picture <out.png>      the non-spinning hole in front of our sky (Gaia map + catalogue stars), 1280 x 800, toward the galactic centre
//   blackhole_picture --measure      the plan's section 6 measurements that need no graphics card (printed)
//   blackhole_picture --spin <out.png>   0.55.2: the same disc round a hole spinning at 0.99 (spin_sky.h, the fine table in double; spinPicture below)
//   blackhole_picture --disc <out.png>   0.55.1: the feeding preset's gas disc (blackhole/lab.h), seen from 60 M, 10 degrees above the disc's plane, 1280 x 800:
//                                    the sky by lensPixel, the disc by discPixel (the table the card uses, in double), 2 x 2 rays per pixel, laid over each other in
//                                    linear light at the lab's automatic exposure (discLook); stars the disc covers are not drawn
// The picture: blackhole/schwarzschild.h LensView for every pixel (2 x 2 rays per pixel, averaged), the map sampled bilinearly at the far-sky direction
// (render/planet_maps.h skyUV, as the Orbit Lab's sky shader); the map drawn at full strength (the Orbit Lab's sky level 1) over black; the shadow black;
// each catalogue star's images (starImages) as discs by orbit/sky.h starDraw at the image's magnified magnitude. No colour or brightness shift of the
// hovering camera is drawn (its f_c/f' is printed).
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "app/shot.h"
#include "blackhole/lab.h"
#include "blackhole/spin.h"
#include "blackhole/spin_sky.h"
#include "blackhole/schwarzschild.h"
#include "orbit/sky.h"
#include "render/planet_maps.h"

using namespace blackhole;
static const double kPi = 3.14159265358979323846;

struct View { double ca, sa, cb, sb; };
// the yaw and pitch of orbit/sky.cpp skyScreen that put direction g at the middle of the picture (skyDirection at the middle is (-sb sa, -sb ca, -cb))
static View lookAt(const Vec3& g) {
    View v;
    v.cb = -g.z; v.sb = std::sqrt(std::max(0.0, 1 - v.cb * v.cb));
    v.sa = -g.x / v.sb; v.ca = -g.y / v.sb;
    return v;
}

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

static void fillMap(const LensView& V, int W, int H, int threads, std::vector<float>& out) {
    out.assign(static_cast<std::size_t>(W) * H * 3, 0.f);
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; t++)
        pool.emplace_back([&, t] {
            for (int y = t; y < H; y += threads)
                for (int x = 0; x < W; x++) {
                    Vec3 d;
                    const int k = V.pixel(x + 0.5, y + 0.5, d);
                    float* o = &out[(static_cast<std::size_t>(y) * W + x) * 3];
                    if (k == 0) { o[0] = static_cast<float>(d.x); o[1] = static_cast<float>(d.y); o[2] = static_cast<float>(d.z); }
                }
        });
    for (auto& p : pool) p.join();
}

static int measure() {
    const orbit::Sky& S = orbit::skyData();
    const Vec3 g = S.galCentre;
    const View vw = lookAt(g);
    const double rO = 27;
    const int hw = std::max(1u, std::thread::hardware_concurrency());
    std::printf("cores: %d\n", hw);
    for (int n : {256, 1024, 4096}) {
        const double t0 = now();
        for (int i = 0; i < 20; i++) { LensTable T(rO, n); (void)T; }
        std::printf("table of %d points (rO = %g M): %.3f ms\n", n, rO, (now() - t0) / 20 * 1e3);
    }
    LensTable T(rO);
    const int sizes[3][2] = {{1280, 800}, {1920, 1080}, {3840, 2160}};
    for (const auto& s : sizes) {
        LensView V(T, g, s[0], s[1], vw.ca, vw.sa, vw.cb, vw.sb);
        std::vector<float> m;
        double t0 = now(); fillMap(V, s[0], s[1], 1, m); const double t1 = now() - t0;
        t0 = now(); fillMap(V, s[0], s[1], hw, m); const double tn = now() - t0;
        std::printf("pixel -> sky map %dx%d: one core %.0f ms, %d cores %.0f ms\n", s[0], s[1], t1 * 1e3, hw, tn * 1e3);
    }
    // the table's error against the closed form, for several sizes; the pixel's angle at 3840 x 2160 is 1/K
    const double K4 = std::hypot(3840.0, 2160.0) / 2 / 1.75;
    for (double r : {2.5, 3.0, 27.0, 1000.0}) {
        for (int n : {64, 128, 256, 512, 1024, 2048}) {
            LensTable t(r, n);
            double worst = 0, worstPx = 0;
            for (int i = 0; i <= 20000; i++) {
                const double x = t.edge() * std::pow(1e-7, i / 20000.0), psi = t.edge() - x;
                double dl; t.lookup(psi, dl);
                const double e = std::fabs(dl - bendFromCameraEdge(r, x).dphi);
                worst = std::max(worst, e);
                if (x >= 0.5 / K4) worstPx = std::max(worstPx, e);
            }
            std::printf("table rO %g M, %d points: worst |dphi - exact| %.2e rad (x >= 1e-7 rad), %.2e rad on pixels at least half a 4K pixel from the edge (%.3f of a 4K pixel)\n",
                        r, n, worst, worstPx, worstPx * K4);
        }
    }
    // single against double precision, at the shadow's edge (pixels within 2 pixels of it, 3840 x 2160)
    {
        LensView V(T, g, 3840, 2160, vw.ca, vw.sa, vw.cb, vw.sb);
        double worst[3] = {0, 0, 0};                             // all; at least 0.01 of a pixel from the edge; at least 0.1
        int n = 0;
        for (int i = 0; i < 200000; i++) {
            const double x = (2.0 / K4) * (i + 0.5) / 200000, psi = T.edge() - x;
            double dd; float df;
            const bool a = T.lookup(psi, dd), b = T.lookupF(static_cast<float>(psi), df);
            if (!(a && b)) continue;
            const double e = std::fabs(dd - static_cast<double>(df));
            worst[0] = std::max(worst[0], e);
            if (x * K4 >= 0.01) worst[1] = std::max(worst[1], e);
            if (x * K4 >= 0.1) worst[2] = std::max(worst[2], e);
            n++;
        }
        std::printf("single against double, pixels within 2 4K pixels of the edge (%d samples; a 4K pixel is %.2e rad): worst dphi difference %.2e rad;"
                    " %.2e at least 0.01 pixel from the edge; %.2e at least 0.1 pixel\n", n, 1 / K4, worst[0], worst[1], worst[2]);
        const double edgeErr = std::fabs(static_cast<double>(static_cast<float>(T.edge())) - T.edge());
        std::printf("the edge itself, rounded to single precision, moves %.2e rad = %.2e of a 4K pixel\n", edgeErr, edgeErr * K4);
    }
    {
        std::vector<StarImage> im;
        const double t0 = now();
        for (const auto& st : S.stars) starImages(T, g, st.dir, st.mag, kImageMagLimit, im);
        std::printf("all %zu stars' images (rO = %g M): %.0f ms, %zu images\n", S.stars.size(), rO, (now() - t0) * 1e3, im.size());
    }
    return 0;
}

static double decS(double e) { return e <= 0.04045 ? e / 12.92 : std::pow((e + 0.055) / 1.055, 2.4); }   // IEC 61966-2-1
static double encS(double l) { l = std::clamp(l, 0.0, 1.0); return l <= 0.0031308 ? 12.92 * l : 1.055 * std::pow(l, 1 / 2.4) - 0.055; }

static int discPicture(const std::string& outPath) {
    const int W = 1280, H = 800;
    Settings s = settingsDefaults();
    applyPreset(s, kFeedingKey);
    setDistance(s, 60);                                                        // (outside the disc's outer edge, 30 M)
    s.pitch = 100 * kPi / 180;                                                 // 10 degrees from the disc's plane, on the side where the picture's up points away from it
                                                                               // (lab.h: its axis is the sky frame's z; the view's up is (sa cb, ca cb, -sb))
    const HoleView v = holeView(s, W, H, W / 2.0, H / 2.0);
    const LensTable T(s.dist);
    const DiscTable D(s.dist);
    const DiscLook L = discLook(D, s, v, W, H, 2);
    render::MapImage map;
    if (!render::decodeMap(render::Map::Sky, map)) { std::fprintf(stderr, "no sky map\n"); return 1; }
    const orbit::Sky& S = orbit::skyData();
    const Vec3 G0 = S.galCentre, G1 = cross(S.galPole, S.galCentre), G2 = S.galPole;
    const auto sample = [&](const Vec3& d, double* lin) {                     // the map, bilinear, decoded to linear light
        const double l = std::atan2(dot(d, G1), dot(d, G0)), b = std::asin(std::clamp(dot(d, G2), -1.0, 1.0));
        double u, w;
        render::skyUV(l, b, u, w);
        const double fx = u * map.w - 0.5, fy = std::clamp(w * map.h - 0.5, 0.0, map.h - 1.0);
        const int x0 = static_cast<int>(std::floor(fx)), y0 = static_cast<int>(std::floor(fy));
        const double ax = fx - x0, ay = fy - y0;
        for (int c = 0; c < 3; c++) {
            double q = 0;
            for (int j = 0; j < 2; j++)
                for (int i = 0; i < 2; i++) {
                    const int xx = ((x0 + i) % map.w + map.w) % map.w, yy = std::min(y0 + j, map.h - 1);
                    q += (i ? ax : 1 - ax) * (j ? ay : 1 - ay) * map.rgba[(static_cast<std::size_t>(yy) * map.w + xx) * 4 + c];
                }
            lin[c] += decS(q / 255);
        }
    };
    std::vector<double> img(static_cast<std::size_t>(W) * H * 3, 0.0);
    std::vector<unsigned char> onDisc(static_cast<std::size_t>(W) * H, 0);
    int discRays = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            double lin[3] = {0, 0, 0};
            int hits = 0;
            for (int k = 0; k < 4; k++) {
                const double X = x + 0.25 + 0.5 * (k & 1), Y = y + 0.25 + 0.5 * (k >> 1);
                const DiscPixel q = discPixel(D, s, v, X, Y);
                if (q.hit.order >= 0) {
                    const double m = std::max({q.rgb[0], q.rgb[1], q.rgb[2]}) / L.white;
                    for (int c = 0; c < 3; c++) lin[c] += q.rgb[c] / L.white / std::max(1.0, m);
                    hits++; discRays++;
                    continue;
                }
                Vec3 d;
                if (lensPixel(T, v, X, Y, d) == 0) sample(d, lin);
            }
            onDisc[static_cast<std::size_t>(y) * W + x] = hits > 0;
            for (int c = 0; c < 3; c++) img[(static_cast<std::size_t>(y) * W + x) * 3 + c] = lin[c] / 4;
        }
    std::vector<StarImage> im;
    int drawn = 0, hidden = 0;
    for (const auto& st : S.stars) {
        im.clear();
        starImagesFast(T, viewMiddle(v), st.dir, st.mag, kImageMagLimit, im);
        const auto& col = S.colours[st.bucket];
        for (const auto& m : im) {
            double X, Y;
            if (!viewScreen(v, m.dir, X, Y) || X < -10 || X > W + 10 || Y < -10 || Y > H + 10) continue;
            if (discPixel(D, s, v, X, Y, false).hit.order >= 0) { hidden++; continue; }
            const orbit::StarDraw sd = orbit::starDraw(m.mag);
            drawn++;
            for (int yy = static_cast<int>(Y - sd.radius - 2); yy <= static_cast<int>(Y + sd.radius + 2); yy++)
                for (int xx = static_cast<int>(X - sd.radius - 2); xx <= static_cast<int>(X + sd.radius + 2); xx++) {
                    if (xx < 0 || yy < 0 || xx >= W || yy >= H || onDisc[static_cast<std::size_t>(yy) * W + xx]) continue;
                    const double cov = std::clamp(sd.radius + 0.5 - std::hypot(xx + 0.5 - X, yy + 0.5 - Y), 0.0, 1.0) * sd.alpha;
                    double* p = &img[(static_cast<std::size_t>(yy) * W + xx) * 3];
                    for (int c = 0; c < 3; c++) p[c] = p[c] * (1 - cov) + decS(col[c] / 255.0) * cov;
                }
        }
    }
    std::vector<uint8_t> rgb(static_cast<std::size_t>(W) * H * 3);
    for (std::size_t i = 0; i < rgb.size(); i++) rgb[i] = static_cast<uint8_t>(std::lround(encS(img[i]) * 255));
    const std::vector<uint8_t> png = app::encodePng(W, H, rgb.data());
    FILE* fo = std::fopen(outPath.c_str(), "wb");
    if (!fo) return 1;
    std::fwrite(png.data(), 1, png.size(), fo);
    std::fclose(fo);
    const Readout r = readout(s);
    std::printf("wrote %s (%zu bytes): %s, %.4g Suns, L = %.3g L_Edd, disc %g to %g M; camera at %g M, 10 degrees from the disc's plane; hottest ring %.0f K at %.4f M;\n"
                "brightest part %.4g x the Sun's surface (visible), screen white %.4g x; %d disc rays of %d; %d star images drawn, %d hidden by the disc; shadow %.4f deg\n",
                outPath.c_str(), png.size(), kFeedingName, s.massSun, s.feed, r.rIn, s.outer, s.dist, r.tHot, r.rHot, L.brightY, L.white, discRays, 4 * W * H, drawn, hidden,
                shadowAngle(s.dist) * 180 / kPi);
    return 0;
}

// 0.55.2: the feeding preset round a spinning hole (spin 0.99, the Spin control's top), seen from 60 M, 10 degrees above the disc's plane, 1280 x 800: the fine
// spin table (spin_sky.h) in double at 2 x 2 rays per pixel, the disc by its first crossing (C75 (A9) shift, Page & Thorne's flux at this spin), the sky through
// the table's far direction, the stars by inverting the table (SpinStars); the lab's automatic exposure (the brightest disc channel at full brightness)
static int spinPicture(const std::string& outPath) {
    const int W = 1280, H = 800;
    Settings s = settingsDefaults();
    applyPreset(s, kFeedingKey);
    setDistance(s, 60);
    setSpin(s, kSpinTop);
    s.pitch = 100 * kPi / 180;
    const HoleView v = holeView(s, W, H, W / 2.0, H / 2.0);
    const SpinTable T(s.spin, s.dist, s.pitch, kSpinChiFine, kSpinSFine, kSpinEdge);
    const DiscGeom g = discGeom(s);
    const DiscPhys P = discPhys(s);
    const double ph = std::atan2(v.ca * v.sb, v.sa * v.sb), cph = std::cos(ph), sph = std::sin(ph), th = s.pitch;
    const Vec3 er{std::sin(th) * cph, std::sin(th) * sph, std::cos(th)}, et{std::cos(th) * cph, std::cos(th) * sph, -std::sin(th)}, ep{-sph, cph, 0};
    const auto at = [&](double X, double Y, SpinHit& h, SpinDisc& d) {
        const double dx = X - v.cx, dy = v.cy - Y, dd = std::hypot(dx, dy), t = dd / v.K;
        if (!(t <= kPi)) return false;
        double chi = std::atan2(-dx / dd, dy / dd);
        if (chi < 0) chi += 2 * kPi;
        h = T.lookup(t, chi);
        d = spinDisc(h, s.spin, g.rIn, g.rOut);
        return true;
    };
    double white = 0;
    for (int y = 1; y < H; y += 2)
        for (int x = 1; x < W; x += 2) {
            SpinHit h; SpinDisc d;
            if (!at(x, y, h, d) || d.order < 0) continue;
            const auto c = blackBodyLinearFast(d.g * P.temperature(d.r));
            white = std::max({white, c[0], c[1], c[2]});
        }
    if (!(white > 0)) white = 1;
    render::MapImage map;
    if (!render::decodeMap(render::Map::Sky, map)) { std::fprintf(stderr, "no sky map\n"); return 1; }
    const orbit::Sky& S = orbit::skyData();
    const Vec3 G0 = S.galCentre, G1 = cross(S.galPole, S.galCentre), G2 = S.galPole;
    const auto sample = [&](const Vec3& d, double* lin) {
        const double l = std::atan2(dot(d, G1), dot(d, G0)), b = std::asin(std::clamp(dot(d, G2), -1.0, 1.0));
        double u, w;
        render::skyUV(l, b, u, w);
        const double fx = u * map.w - 0.5, fy = std::clamp(w * map.h - 0.5, 0.0, map.h - 1.0);
        const int x0 = static_cast<int>(std::floor(fx)), y0 = static_cast<int>(std::floor(fy));
        const double ax = fx - x0, ay = fy - y0;
        for (int c = 0; c < 3; c++) {
            double q = 0;
            for (int j = 0; j < 2; j++)
                for (int i = 0; i < 2; i++) {
                    const int xx = ((x0 + i) % map.w + map.w) % map.w, yy = std::min(y0 + j, map.h - 1);
                    q += (i ? ax : 1 - ax) * (j ? ay : 1 - ay) * map.rgba[(static_cast<std::size_t>(yy) * map.w + xx) * 4 + c];
                }
            lin[c] += decS(q / 255);
        }
    };
    std::vector<double> img(static_cast<std::size_t>(W) * H * 3, 0.0);
    std::vector<unsigned char> onDisc(static_cast<std::size_t>(W) * H, 0);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            double lin[3] = {0, 0, 0};
            int hits = 0;
            for (int k = 0; k < 4; k++) {
                const double X = x + 0.25 + 0.5 * (k & 1), Y = y + 0.25 + 0.5 * (k >> 1);
                SpinHit h; SpinDisc d;
                if (!at(X, Y, h, d)) continue;
                if (d.order >= 0) {
                    const auto c = blackBodyLinearFast(d.g * P.temperature(d.r));
                    const double m = std::max({c[0], c[1], c[2]}) / white;
                    for (int q = 0; q < 3; q++) lin[q] += c[q] / white / std::max(1.0, m);
                    hits++;
                    continue;
                }
                if (h.sky) sample(Vec3{cph * h.dir.x - sph * h.dir.y, sph * h.dir.x + cph * h.dir.y, h.dir.z}, lin);
            }
            onDisc[static_cast<std::size_t>(y) * W + x] = hits > 0;
            for (int c = 0; c < 3; c++) img[(static_cast<std::size_t>(y) * W + x) * 3 + c] = lin[c] / 4;
        }
    const SpinStars St(T);
    std::vector<SpinImage> im;
    int drawn = 0, hidden = 0;
    for (const auto& st : S.stars) {
        im.clear();
        St.images(Vec3{cph * st.dir.x + sph * st.dir.y, -sph * st.dir.x + cph * st.dir.y, st.dir.z}, im);
        const auto& col = S.colours[st.bucket];
        for (const auto& m : im) {
            if (!(m.mag > 0)) continue;
            const double mag = st.mag - 2.5 * std::log10(m.mag);
            if (mag > kImageMagLimit) continue;
            const Vec3 N = lookAt(m.psi, m.chi), dir = er * N.x + et * N.y + ep * N.z;
            double X, Y;
            if (!viewScreen(v, dir, X, Y) || X < -10 || X > W + 10 || Y < -10 || Y > H + 10) continue;
            SpinHit h; SpinDisc d;
            if (at(X, Y, h, d) && d.order >= 0) { hidden++; continue; }
            const orbit::StarDraw sd = orbit::starDraw(mag);
            drawn++;
            for (int yy = static_cast<int>(Y - sd.radius - 2); yy <= static_cast<int>(Y + sd.radius + 2); yy++)
                for (int xx = static_cast<int>(X - sd.radius - 2); xx <= static_cast<int>(X + sd.radius + 2); xx++) {
                    if (xx < 0 || yy < 0 || xx >= W || yy >= H || onDisc[static_cast<std::size_t>(yy) * W + xx]) continue;
                    const double cov = std::clamp(sd.radius + 0.5 - std::hypot(xx + 0.5 - X, yy + 0.5 - Y), 0.0, 1.0) * sd.alpha;
                    double* p = &img[(static_cast<std::size_t>(yy) * W + xx) * 3];
                    for (int c = 0; c < 3; c++) p[c] = p[c] * (1 - cov) + decS(col[c] / 255.0) * cov;
                }
        }
    }
    std::vector<uint8_t> rgb(static_cast<std::size_t>(W) * H * 3);
    for (std::size_t i = 0; i < rgb.size(); i++) rgb[i] = static_cast<uint8_t>(std::lround(encS(img[i]) * 255));
    const std::vector<uint8_t> png = app::encodePng(W, H, rgb.data());
    FILE* fo = std::fopen(outPath.c_str(), "wb");
    if (!fo) return 1;
    std::fwrite(png.data(), 1, png.size(), fo);
    std::fclose(fo);
    const Readout r = readout(s);
    std::printf("wrote %s (%zu bytes): %s, spin %g, %.4g Suns, L = %.3g L_Edd, disc %.6g to %g M; camera at %g M, 10 degrees from the disc's plane; horizon %.6g M; hottest ring %.0f K at %.4f M;\n"
                "efficiency %.4f; table %ld rays in %.0f ms; screen white %.4g x the Sun's surface; %d star images drawn, %d hidden by the disc; shadow %.4f x %.4f deg; dragged round once every %.4g GM/c^3\n",
                outPath.c_str(), png.size(), kFeedingName, s.spin, s.massSun, s.feed, r.rIn, s.outer, s.dist, r.horizonM, r.tHot, r.rHot, r.efficiency, T.rays(), T.fillMs(), white, drawn, hidden,
                r.shadowWideDeg, r.shadowHighDeg, 2 * kPi / r.omega);
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--measure") == 0) return measure();
    if (argc > 2 && std::strcmp(argv[1], "--disc") == 0) return discPicture(argv[2]);
    if (argc > 2 && std::strcmp(argv[1], "--spin") == 0) return spinPicture(argv[2]);
    const std::string outPath = argc > 1 ? argv[1] : "blackhole_reference.png";
    const int W = 1280, H = 800;
    const orbit::Sky& S = orbit::skyData();
    const Vec3 g = S.galCentre;
    const View vw = lookAt(g);
    const double rO = 27, K = std::hypot(double(W), double(H)) / 2 / 1.75;
    LensTable T(rO);
    LensView V(T, g, W, H, vw.ca, vw.sa, vw.cb, vw.sb);
    render::MapImage map;
    if (!render::decodeMap(render::Map::Sky, map)) { std::fprintf(stderr, "no sky map\n"); return 1; }
    const Vec3 G0 = S.galCentre, G1 = cross(S.galPole, S.galCentre), G2 = S.galPole;   // galactic axes (orbit/sky.cpp skyViewToGalactic)
    const auto sample = [&](const Vec3& d, double* rgb) {
        const double l = std::atan2(dot(d, G1), dot(d, G0)), b = std::asin(std::clamp(dot(d, G2), -1.0, 1.0));
        double u, v;
        render::skyUV(l, b, u, v);
        const double fx = u * map.w - 0.5, fy = std::clamp(v * map.h - 0.5, 0.0, map.h - 1.0);
        const int x0 = static_cast<int>(std::floor(fx)), y0 = static_cast<int>(std::floor(fy));
        const double ax = fx - x0, ay = fy - y0;
        for (int c = 0; c < 3; c++) {
            double s = 0;
            for (int j = 0; j < 2; j++)
                for (int i = 0; i < 2; i++) {
                    const int xx = ((x0 + i) % map.w + map.w) % map.w, yy = std::min(y0 + j, map.h - 1);
                    s += (i ? ax : 1 - ax) * (j ? ay : 1 - ay) * map.rgba[(static_cast<std::size_t>(yy) * map.w + xx) * 4 + c];
                }
            rgb[c] += s;
        }
    };
    std::vector<double> img(static_cast<std::size_t>(W) * H * 3, 0.0);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            double rgb[3] = {0, 0, 0};
            for (int k = 0; k < 4; k++) {
                Vec3 d;
                if (V.pixel(x + 0.25 + 0.5 * (k & 1), y + 0.25 + 0.5 * (k >> 1), d) == 0) sample(d, rgb);
            }
            for (int c = 0; c < 3; c++) img[(static_cast<std::size_t>(y) * W + x) * 3 + c] = rgb[c] / 4;
        }
    // the stars' images, drawn by the Orbit Lab's rule, over the map
    std::vector<StarImage> im;
    int drawn = 0, extra = 0;
    for (const auto& st : S.stars) {
        im.clear();
        starImages(T, g, st.dir, st.mag, kImageMagLimit, im);
        const auto& col = S.colours[st.bucket];
        for (const auto& m : im) {
            double X, Y;
            if (!orbit::skyScreen(m.dir, W, H, vw.ca, vw.sa, vw.cb, vw.sb, X, Y) || X < -10 || X > W + 10 || Y < -10 || Y > H + 10) continue;
            const orbit::StarDraw sd = orbit::starDraw(m.mag);
            drawn++;
            if (m.order > 0) extra++;
            for (int yy = static_cast<int>(Y - sd.radius - 2); yy <= static_cast<int>(Y + sd.radius + 2); yy++)
                for (int xx = static_cast<int>(X - sd.radius - 2); xx <= static_cast<int>(X + sd.radius + 2); xx++) {
                    if (xx < 0 || yy < 0 || xx >= W || yy >= H) continue;
                    const double cov = std::clamp(sd.radius + 0.5 - std::hypot(xx + 0.5 - X, yy + 0.5 - Y), 0.0, 1.0) * sd.alpha;
                    double* p = &img[(static_cast<std::size_t>(yy) * W + xx) * 3];
                    for (int c = 0; c < 3; c++) p[c] = p[c] * (1 - cov) + col[c] * cov;
                }
        }
    }
    std::vector<uint8_t> rgb(static_cast<std::size_t>(W) * H * 3);
    for (std::size_t i = 0; i < rgb.size(); i++) rgb[i] = static_cast<uint8_t>(std::clamp(std::lround(img[i]), 0L, 255L));
    const std::vector<uint8_t> png = app::encodePng(W, H, rgb.data());
    FILE* fo = std::fopen(outPath.c_str(), "wb");
    if (!fo) return 1;
    std::fwrite(png.data(), 1, png.size(), fo);
    std::fclose(fo);
    std::printf("wrote %s (%zu bytes): camera hovering at %g M, looking at the galactic centre; shadow radius %.4f deg = %.1f px; field: %.1f deg from the middle to a corner;\n"
                "%d star images drawn (%d of them secondary or further); hovering camera's f_c/f' %.6f (not drawn)\n",
                outPath.c_str(), png.size(), rO, shadowAngle(rO) * 180 / kPi, shadowAngle(rO) * K, 1.75 * 180 / kPi, drawn, extra, V.shift());
    return 0;
}
