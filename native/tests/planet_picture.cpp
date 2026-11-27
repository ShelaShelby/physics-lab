// 0.77.0: the CPU picture of one planet or moon with a map, for the GPU-vs-CPU picture check (tests/check_orbit_ui.py planets).
//   planet_picture out.bin <the fields of one PLPLANET line> <the fields of the PLPLANETX line after it> (the app prints both with PL_LOG_PLANET=1)
// The C++ twin of kPlanetFS mode 1 (render/shaders.h) for a ball or ellipsoid with ring shadows, eclipses, Earth's clouds (the map's alpha) and air (earthAirLight;
// the shader reads airTransmit from a 48-row table, this twin computes it): render/ellipsoid.h for the shape,
// planetUV for the map, the map's mip levels made as the GPU makes them (each level the mean of 2 x 2 texels of the one above, in linear light) and read
// as the GPU's sampler does (bilinear in a level, linear between levels, longitude repeated, latitude clamped), the level from the pixel's footprint
// (the uv change to the next pixel right and down), albedoScaled, Lambert or McEwen's lunar law, then sRGB over the colour of space.
// 0.85.0: times the sunlight factor (PLPLANET's last field; orbit/true_light.h); the eclipses with the Sun's measured edge per channel (render/sun_limb.h).
//   planet_picture out.bin sun <the fields of a PLSUN line>: the CPU twin of kPlanetFS uMode 4, the Sun: Neckel & Labs 1994's edge per channel, the sunspots
//   (0.94.0: filled, umbra and penumbra), the granulation and NASA's photo (render/sun_surface.h, orbit/sun_photo.h).
// Writes "PLCPU w h x0 y0\n" and w * h RGBA bytes: A = 255 inside 0.95 of the outline (the smooth edge is left out), 0 elsewhere.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include "render/ellipsoid.h"
#include "render/planet_maps.h"
#include "render/shading.h"
#include "render/sun_limb.h"
#include "render/sun_surface.h"
#include "orbit/sun_photo.h"

namespace {
struct Level { int w = 0, h = 0; std::vector<float> rgb; };   // 4 floats a texel: linear r, g, b and the alpha (Earth's cloud cover)
std::vector<Level> mips(render::Map map, const render::MapImage& im) {
    std::vector<Level> L(1);
    L[0].w = im.w; L[0].h = im.h; L[0].rgb.resize(std::size_t(im.w) * im.h * 4);
    for (std::size_t i = 0; i < std::size_t(im.w) * im.h; i++)
        { for (int c = 0; c < 3; c++) L[0].rgb[4 * i + c] = static_cast<float>(render::mapDecode(map, im.rgba[4 * i + c])); L[0].rgb[4 * i + 3] = static_cast<float>(im.rgba[4 * i + 3] / 255.0); }
    while (L.back().w > 1 || L.back().h > 1) {
        const Level& a = L.back();
        Level b; b.w = std::max(1, a.w / 2); b.h = std::max(1, a.h / 2); b.rgb.resize(std::size_t(b.w) * b.h * 4);
        for (int y = 0; y < b.h; y++)
            for (int x = 0; x < b.w; x++)
                for (int c = 0; c < 4; c++) {
                    double s = 0;
                    for (int j = 0; j < 2; j++)
                        for (int i = 0; i < 2; i++) s += a.rgb[4 * (std::size_t(std::min(a.h - 1, 2 * y + j)) * a.w + std::min(a.w - 1, 2 * x + i)) + c];
                    b.rgb[4 * (std::size_t(y) * b.w + x) + c] = static_cast<float>(s / 4);
                }
        L.push_back(std::move(b));
    }
    return L;
}
void bilinear(const Level& l, double u, double v, double out[4]) {
    const double x = u * l.w - 0.5, y = v * l.h - 0.5;
    const int x0 = static_cast<int>(std::floor(x)), y0 = static_cast<int>(std::floor(y));
    const double fx = x - x0, fy = y - y0;
    for (int c = 0; c < 4; c++) out[c] = 0;
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 2; i++) {
            const int xx = ((x0 + i) % l.w + l.w) % l.w, yy = std::clamp(y0 + j, 0, l.h - 1);
            const double w = (i ? fx : 1 - fx) * (j ? fy : 1 - fy);
            for (int c = 0; c < 4; c++) out[c] += w * l.rgb[4 * (std::size_t(yy) * l.w + xx) + c];
        }
}
}  // namespace

// 0.94.0: the PLSUN line: x y r K[9] n, 6 a spot (unit vector, radius, umbra radius, penumbra factor), then granulation on, cells and grid spacings per radian, k, f, photo on,
// the photo's files ("<cache folder>/<day>": .png, .xml, .json; "-" without one). The twin of kPlanetFS uMode 4 (render/sun_surface.h, orbit/sun_photo.h).
// The photo is read bilinearly from its full-size level (the GPU uses its mip levels where the photo is shrunk: the check's photo is smooth there).
int sunPicture(const char* outPath, const std::vector<std::string>& a) {
    std::vector<double> f;
    for (const auto& s : a) f.push_back(std::atof(s.c_str()));
    if (f.size() < 13) { std::fprintf(stderr, "PLSUN: %zu fields\n", f.size()); return 2; }
    const int ns = static_cast<int>(f[12]);
    const std::size_t tail = 13 + 6 * std::size_t(ns);
    if (f.size() < tail + 7) { std::fprintf(stderr, "PLSUN: %zu fields for %d spots\n", f.size(), ns); return 2; }
    const double X = f[0], Y = f[1], R = f[2];
    float K[9]; for (int i = 0; i < 9; i++) K[i] = static_cast<float>(f[3 + i]);
    std::vector<render::SunSpotDraw> spots(static_cast<std::size_t>(ns));
    for (int k = 0; k < ns; k++) { const double* s = &f[13 + 6 * std::size_t(k)]; auto& d = spots[std::size_t(k)]; d.v[0] = s[0]; d.v[1] = s[1]; d.v[2] = s[2]; d.rho = s[3]; d.rhoU = s[4]; d.pen = s[5]; }
    const bool gran = f[tail] > 0.5;
    const double cellsPerRad = f[tail + 1], gridPerRad = f[tail + 2];
    render::GranTime gt; gt.k = static_cast<std::uint32_t>(std::strtoul(a[tail + 3].c_str(), nullptr, 10)); gt.f = f[tail + 4];
    const bool photoOn = f[tail + 5] > 0.5;
    orbit::SunPhoto photo;
    std::string base = a[tail + 6];   // the photo's files: the rest of the line (a folder may hold spaces)
    for (std::size_t i = tail + 7; i < a.size(); i++) base += " " + a[i];
    if (photoOn) {
        const auto rd = [](const std::string& p) { std::FILE* fp = std::fopen(p.c_str(), "rb"); std::string s; if (!fp) return s; char b[65536]; std::size_t n; while ((n = std::fread(b, 1, sizeof b, fp)) > 0) s.append(b, n); std::fclose(fp); return s; };
        const std::string png = rd(base + ".png");
        photo = orbit::loadSunPhoto(std::vector<std::uint8_t>(png.begin(), png.end()), rd(base + ".xml"), rd(base + ".json"));
        if (!photo.ok) { std::fprintf(stderr, "PLSUN: the photo %s cannot be read: %s\n", base.c_str(), photo.why.c_str()); return 2; }
    }
    const render::SunLimb& lim = render::sunLimb();
    const auto& U = render::umbraRgb();
    const auto& G = render::granContrastRgb();
    int x0 = static_cast<int>(std::floor(X - R)), y0 = static_cast<int>(std::floor(Y - R)), w = static_cast<int>(std::ceil(2 * R)) + 2, h = w;
    if (const char* clip = std::getenv("PL_PICTURE_CLIP")) {   // "x0 y0 x1 y1": only these pixels of the window (a disc far larger than the window, panels elsewhere)
        int cx0 = 0, cy0 = 0, cx1 = 0, cy1 = 0;
        if (std::sscanf(clip, "%d %d %d %d", &cx0, &cy0, &cx1, &cy1) == 4 && cx1 > cx0 && cy1 > cy0) {
            const int x1 = std::min(x0 + w, cx1), y1 = std::min(y0 + h, cy1);
            x0 = std::max(x0, cx0); y0 = std::max(y0, cy0); w = std::max(0, x1 - x0); h = std::max(0, y1 - y0);
        }
    }
    std::vector<unsigned char> out(std::size_t(w) * h * 4, 0);
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            const double px = x0 + i + 0.5, py = y0 + j + 0.5;
            const render::EllipsoidHit hit = render::ellipsoidHit(K, 1, 1, (px - X) / R, (py - Y) / R);
            if (!hit.hit || hit.d > 0.95) continue;
            const render::EllipsoidHit hx = render::ellipsoidHit(K, 1, 1, (px + 1 - X) / R, (py - Y) / R), hy = render::ellipsoidHit(K, 1, 1, (px - X) / R, (py + 1 - Y) / R);
            const auto dist = [&](const render::EllipsoidHit& e) { return std::sqrt(std::pow(e.b[0] - hit.b[0], 2) + std::pow(e.b[1] - hit.b[1], 2) + std::pow(e.b[2] - hit.b[2], 2)); };
            const double pw = std::max(dist(hx), dist(hy));
            double sf[3], cover;
            render::spotsFactor(hit.b, pw, spots.data(), ns, U, sf, cover);
            double rel = 1, wP = 0;
            if (photoOn) orbit::photoAt(photo, hit.b, rel, wP);
            double g = 0, A = 0;
            if (gran) {
                A = render::granFade(pw * cellsPerRad);
                if (A > 0 && wP < 1 && cover < 1) {
                    const double q[3] = {hit.b[0] * gridPerRad, hit.b[1] * gridPerRad, hit.b[2] * gridPerRad};
                    const double an = 1.5707963267948966 * gt.f;
                    g = std::cos(an) * (render::kGranMean - render::granRaw(q, gt.k)) / render::kGranRms + std::sin(an) * (render::kGranMean - render::granRaw(q, gt.k + 1u)) / render::kGranRms;
                }
            }
            const double mu = std::clamp(hit.n[2], 0.0, 1.0);
            unsigned char* o = &out[4 * (std::size_t(j) * w + i)];
            for (int c = 0; c < 3; c++) {
                const double drawn = (1 + A * g * (1 - cover) * G[std::size_t(c)]) * sf[c];
                const double v = lim.centre[c] * std::max(render::limbP(lim.poly[c], mu), 0.0) * (drawn + (rel - drawn) * wP);
                o[c] = static_cast<unsigned char>(std::lround(255 * render::srgbEncode(v)));
            }
            o[3] = 255;
        }
    std::FILE* fp = std::fopen(outPath, "wb");
    if (!fp) return 1;
    std::fprintf(fp, "PLCPU %d %d %d %d\n", w, h, x0, y0);
    std::fwrite(out.data(), 1, out.size(), fp);
    std::fclose(fp);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: planet_picture out.bin <PLPLANET fields> | planet_picture out.bin sun <PLSUN fields>\n"); return 2; }
    if (std::string(argv[2]) == "sun") {
        std::vector<std::string> g;
        for (int i = 3; i < argc; i++) g.push_back(argv[i]);
        return sunPicture(argv[1], g);
    }
    std::vector<double> f;
    for (int i = 2; i < argc; i++) f.push_back(std::atof(argv[i]));
    if (f.size() < 28) { std::fprintf(stderr, "a PLPLANET line has 27 fields and PLPLANETX at least 1, got %zu\n", f.size()); return 2; }
    // map x y r lx ly lz K[9] shapeY shapeZ lunar albedoK nring nocc air spaceR spaceG spaceB lightK
    const auto map = static_cast<render::Map>(static_cast<int>(f[0]));
    const double X = f[1], Y = f[2], R = f[3];
    float K[9]; for (int i = 0; i < 9; i++) K[i] = static_cast<float>(f[7 + i]);
    const double sy = f[16], sz = f[17], k = f[19];
    const bool lunar = f[18] != 0;
    const bool air = f[22] != 0, clouds = map == render::Map::Earth;
    const int nring = static_cast<int>(f[20]), nocc = static_cast<int>(f[21]);
    if (f.size() != std::size_t(28 + 3 * nring + 4 * nocc)) { std::fprintf(stderr, "PLPLANETX: %zu fields, want %d\n", f.size() - 27, 1 + 3 * nring + 4 * nocc); return 2; }
    const double lightK = f[26];
    const double sunAng = f[27];
    const double* ring = f.data() + 28;
    const double* occ = ring + 3 * nring;
    const double space[3] = {f[23], f[24], f[25]};
    double L[3] = {f[4], -f[5], f[6]};
    const double ll = std::sqrt(L[0] * L[0] + L[1] * L[1] + L[2] * L[2]);
    for (double& c : L) c /= ll;
    render::MapImage im;
    if (!render::decodeMap(map, im)) { std::fprintf(stderr, "no map %d\n", static_cast<int>(map)); return 1; }
    const auto lv = mips(map, im);
    const double lon0 = render::mapInfo(map).lonCentreDeg * 3.14159265358979323846 / 180;
    const float tw = static_cast<float>(im.w);
    const double uLod = std::max(0.0f, std::log2(tw / (3.14159f * std::max(2.0f * static_cast<float>(R), 1.0f))));   // renderer2d_vk.cpp's uLod
    const int x0 = static_cast<int>(std::floor(X - R)), y0 = static_cast<int>(std::floor(Y - R)), w = static_cast<int>(std::ceil(2 * R)) + 2, h = w;
    std::vector<unsigned char> out(std::size_t(w) * h * 4, 0);
    const auto uvAt = [&](double px, double py, double& u, double& v, render::EllipsoidHit& hit) {
        hit = render::ellipsoidHit(K, sy, sz, (px - X) / R, (py - Y) / R);
        u = (hit.lonRad - lon0) / (2 * 3.14159265358979323846) + 0.5;
        v = 0.5 - hit.latRad / 3.14159265358979323846;
    };
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++) {
            const double px = x0 + i + 0.5, py = y0 + j + 0.5;
            double u, v, ux, vx, uy, vy;
            render::EllipsoidHit hit, hx, hy;
            uvAt(px, py, u, v, hit);
            if (!hit.hit || hit.d > 0.95) continue;
            uvAt(px + 1, py, ux, vx, hx);
            uvAt(px, py + 1, uy, vy, hy);
            double dux = ux - u, duy = uy - u;
            dux -= std::floor(dux + 0.5); duy -= std::floor(duy + 0.5);
            const double lx = std::hypot(dux * im.w, (vx - v) * im.h), lyy = std::hypot(duy * im.w, (vy - v) * im.h);
            const double lod = std::clamp(std::max(uLod, std::log2(std::max(std::max(lx, lyy), 1e-6))), 0.0, double(lv.size() - 1));
            const int l0 = static_cast<int>(std::floor(lod)), l1 = std::min(l0 + 1, int(lv.size()) - 1);
            double c0[4], c1[4], c[4];
            bilinear(lv[std::size_t(l0)], u - std::floor(u), v, c0);
            bilinear(lv[std::size_t(l1)], u - std::floor(u), v, c1);
            for (int q = 0; q < 4; q++) c[q] = c0[q] + (lod - l0) * (c1[q] - c0[q]);
            render::albedoScaled(c, k);
            if (clouds) render::cloudsOver(c, c[3]);
            double lit = render::lambertLit(hit.n, L);
            if (lunar) {
                const double phi = std::acos(std::clamp(L[2], -1.0, 1.0)) * 180 / 3.14159265358979323846;
                lit = render::mcewenLunar(hit.n[0] * L[0] + hit.n[1] * L[1] + hit.n[2] * L[2], hit.n[2], phi);
            }
            if (nring > 0 && lit > 0) {   // the rings' shadow (kPlanetFS: the ray to the Sun through the ring plane, b.z = 0, exp(-tau / mu0))
                double Lp[3];
                for (int q = 0; q < 3; q++) Lp[q] = K[3 * q] * L[0] + K[3 * q + 1] * L[1] + K[3 * q + 2] * L[2];
                if (std::fabs(Lp[2]) > 1e-4) {
                    const double t = -hit.b[2] / Lp[2];
                    if (t > 0) {
                        const double rho = std::hypot(hit.b[0] + t * Lp[0], hit.b[1] + t * Lp[1]);
                        double tau = 0;
                        for (int q = 0; q < nring; q++) if (rho >= ring[3 * q] && rho <= ring[3 * q + 1]) tau += ring[3 * q + 2];
                        lit *= std::exp(-tau / std::fabs(Lp[2]));
                    }
                }
            }
            double sunF[3] = {1, 1, 1};
            if (nocc > 0 && hit.n[0] * L[0] + hit.n[1] * L[1] + hit.n[2] * L[2] > 0) {   // eclipses: the point in the screen frame, K^T b; 0.85.0: the measured edge, per channel
                double sp[3];
                for (int q = 0; q < 3; q++) sp[q] = K[q] * hit.b[0] + K[3 + q] * hit.b[1] + K[6 + q] * hit.b[2];
                for (int q = 0; q < nocc; q++) {
                    const double oc[3] = {occ[4 * q], occ[4 * q + 1], occ[4 * q + 2]};
                    double s3[3];
                    render::sunlightPastLimb(sp, oc, occ[4 * q + 3], L, sunAng, s3);
                    for (int c = 0; c < 3; c++) sunF[c] *= s3[c];
                }
            }
            unsigned char* o = &out[4 * (std::size_t(j) * w + i)];
            if (air) {   // kPlanetFS's air branch: earthAirLight, times the ring shadow's factor (lit / mu0) and the eclipse's
                const double mu0 = hit.n[0] * L[0] + hit.n[1] * L[1] + hit.n[2] * L[2], mu = hit.n[2];
                const double ringF = mu0 > 0 ? lit / mu0 : 0;
                for (int q = 0; q < 3; q++)
                    o[q] = static_cast<unsigned char>(std::lround(255 * render::srgbEncode(render::srgbDecode(space[q]) + render::earthAirLight(q, c[q], mu0, mu, -L[2]) * ringF * sunF[q] * lightK)));
            } else {
                for (int q = 0; q < 3; q++) o[q] = static_cast<unsigned char>(std::lround(255 * render::srgbEncode(render::srgbDecode(space[q]) + c[q] * std::max(lit, 0.0) * sunF[q] * lightK)));
            }
            o[3] = 255;
        }
    std::FILE* fp = std::fopen(argv[1], "wb");
    if (!fp) return 1;
    std::fprintf(fp, "PLCPU %d %d %d %d\n", w, h, x0, y0);
    std::fwrite(out.data(), 1, out.size(), fp);
    std::fclose(fp);
    return 0;
}
