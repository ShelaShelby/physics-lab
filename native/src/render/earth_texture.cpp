#include "earth_texture.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

namespace render {

#include "earth_land.inc"

namespace {

struct RGB { float r, g, b; };
RGB hex(const char* h) { const unsigned long v = std::strtoul(h + 1, nullptr, 16); return {float((v >> 16) & 255), float((v >> 8) & 255), float(v & 255)}; }
RGB mix(RGB a, RGB b, float f) { return {a.r + (b.r - a.r) * f, a.g + (b.g - a.g) * f, a.b + (b.b - a.b) * f}; }
double hsh(double n) { const double v = std::sin(n * 127.1 + 311.7) * 43758.5453; return v - std::floor(v); }   // the web's repeatable "random" number

}  // namespace

std::vector<uint8_t> makeEarthTexture() {
    const int W = kEarthTexW, H = kEarthTexH, S = 2;          // land is drawn at 2× and averaged, for smooth coasts
    const auto X = [&](double lon) { return (lon + 180) / 360 * W; };
    const auto Y = [&](double lat) { return (90 - lat) / 180 * H; };

    // 1) land polygons
    std::vector<std::vector<std::pair<double, double>>> polys;
    {
        std::string all = kEarthLand;
        std::size_t start = 0;
        while (start < all.size()) {
            std::size_t end = all.find(';', start);
            if (end == std::string::npos) end = all.size();
            std::vector<long> v;
            const char* p = all.c_str() + start;
            const char* stop = all.c_str() + end;
            while (p < stop) { char* q; v.push_back(std::strtol(p, &q, 10)); p = q; if (*p == ',') ++p; }
            std::vector<std::pair<double, double>> poly;
            long x = v.size() > 1 ? v[0] : 0, y = v.size() > 1 ? v[1] : 0;
            if (v.size() > 1) poly.push_back({X(x / 2.0) * S, Y(y / 2.0) * S});
            for (std::size_t i = 2; i + 1 < v.size(); i += 2) { x += v[i]; y += v[i + 1]; poly.push_back({X(x / 2.0) * S, Y(y / 2.0) * S}); }
            // a polygon that crosses the ±180° line (Chukotka, Fiji) jumps across the whole map: unwrap it,
            // and the columns are wrapped round when it is filled
            for (std::size_t i = 1; i < poly.size(); i++) {
                while (poly[i].first - poly[i - 1].first > W * S / 2.0) poly[i].first -= W * S;
                while (poly[i].first - poly[i - 1].first < -W * S / 2.0) poly[i].first += W * S;
            }
            if (poly.size() > 2) polys.push_back(std::move(poly));
            start = end + 1;
        }
    }
    // 2) land coverage (0–1 per pixel): scanline fill of each polygon at 2×, then averaged
    std::vector<uint8_t> big(static_cast<std::size_t>(W * S) * H * S, 0);
    std::vector<double> xs;
    for (const auto& poly : polys) {
        double y0 = 1e9, y1 = -1e9;
        for (auto& p : poly) { y0 = std::min(y0, p.second); y1 = std::max(y1, p.second); }
        for (int row = std::max(0, int(std::floor(y0))); row <= std::min(H * S - 1, int(std::ceil(y1))); row++) {
            const double yc = row + 0.5;
            xs.clear();
            for (std::size_t i = 0; i < poly.size(); i++) {
                const auto& a = poly[i];
                const auto& b = poly[(i + 1) % poly.size()];
                if ((a.second <= yc) != (b.second <= yc)) xs.push_back(a.first + (yc - a.second) / (b.second - a.second) * (b.first - a.first));
            }
            std::sort(xs.begin(), xs.end());
            for (std::size_t k = 0; k + 1 < xs.size(); k += 2)   // even–odd: inside between pairs of crossings
                for (int col = int(std::ceil(xs[k] - 0.5)); col <= int(std::floor(xs[k + 1] - 0.5)); col++)
                    big[static_cast<std::size_t>(row) * W * S + ((col % (W * S)) + W * S) % (W * S)] ^= 1;
        }
    }
    std::vector<float> land(static_cast<std::size_t>(W) * H);
    for (int j = 0; j < H; j++)
        for (int i = 0; i < W; i++) {
            int n = 0;
            for (int a = 0; a < S; a++) for (int b = 0; b < S; b++) n += big[static_cast<std::size_t>(j * S + a) * W * S + i * S + b];
            land[static_cast<std::size_t>(j) * W + i] = float(n) / (S * S);
        }
    // 3) a pale band of shallow water along the coasts (the web strokes the coast 5 px wide under the land)
    std::vector<float> coast(land.size(), 0);
    for (int j = 0; j < H; j++)
        for (int i = 0; i < W; i++) {
            float m = 0;
            for (int dj = -3; dj <= 3; dj++)
                for (int di = -3; di <= 3; di++) {
                    const double d = std::hypot(di, dj);
                    if (d > 3) continue;
                    const int jj = std::clamp(j + dj, 0, H - 1), ii = (i + di + W) % W;
                    m = std::max(m, land[static_cast<std::size_t>(jj) * W + ii] * float(std::clamp(3.0 - d, 0.0, 1.0)));
                }
            coast[static_cast<std::size_t>(j) * W + i] = m;
        }
    // 4) paint
    std::vector<RGB> img(land.size());
    const RGB o0 = hex("#1a4a80"), o1 = hex("#1d63a8"), o2 = hex("#2170b8");
    for (int j = 0; j < H; j++) {
        const double t = (j + 0.5) / H;                       // ocean: darker towards the poles
        const RGB oc = t < 0.3 ? mix(o0, o1, float(t / 0.3)) : t < 0.5 ? mix(o1, o2, float((t - 0.3) / 0.2)) : t < 0.7 ? mix(o2, o1, float((t - 0.5) / 0.2)) : mix(o1, o0, float((t - 0.7) / 0.3));
        for (int i = 0; i < W; i++) {
            const std::size_t k = static_cast<std::size_t>(j) * W + i;
            RGB c = mix(oc, RGB{120, 190, 235}, 0.45f * coast[k]);
            img[k] = mix(c, hex("#4f8a3c"), land[k]);
        }
    }
    // soft elliptical patches (radial gradients from alpha a in the middle to 0 at the edge)
    const auto blob = [&](double lon, double lat, double rx, double ry, RGB col, double a, bool onLand) {
        const double cx = X(lon), cy = Y(lat), sx = rx / 360 * W, sy = ry / 180 * H;
        for (int j = std::max(0, int(cy - sy)); j <= std::min(H - 1, int(cy + sy)); j++)
            for (int i = int(cx - sx); i <= int(cx + sx); i++) {
                if (i < 0 || i >= W) continue;
                const double d = std::hypot((i + 0.5 - cx) / sx, (j + 0.5 - cy) / sy);
                if (d >= 1) continue;
                const std::size_t k = static_cast<std::size_t>(j) * W + i;
                const float f = float(a * (1 - d)) * (onLand ? land[k] : 1.0f);
                img[k] = mix(img[k], col, f);
            }
    };
    struct B { double lon, lat, rx, ry; int r, g, b; double a; };
    const B patches[] = {   // forests, grassland, deserts, tundra and ice (the web's list)
        {-60, -4, 16, 11, 38, 105, 45, .95}, {22, 1, 12, 8, 38, 105, 45, .95}, {112, 0, 12, 7, 38, 105, 45, .9}, {-85, 12, 8, 5, 44, 112, 48, .8}, {80, 22, 9, 6, 70, 120, 50, .7},
        {-80, 36, 12, 6, 60, 115, 50, .7}, {105, 30, 10, 6, 60, 115, 50, .7}, {10, 47, 12, 5, 62, 112, 52, .7}, {-100, 60, 30, 9, 48, 88, 58, .85}, {90, 60, 60, 9, 48, 88, 58, .85},
        {-55, -30, 8, 8, 85, 135, 55, .8}, {30, -28, 8, 6, 120, 140, 60, .7}, {5, 23, 26, 8, 206, 178, 110, .95}, {45, 24, 12, 7, 210, 182, 116, .95}, {134, -25, 16, 9, 200, 150, 95, .95},
        {100, 42, 14, 5, 198, 180, 130, .9}, {22, -23, 8, 5, 200, 170, 110, .85}, {-112, 34, 8, 5, 205, 175, 120, .85}, {-69, -22, 3, 9, 200, 170, 120, .8}, {68, 28, 6, 4, 205, 175, 115, .8},
        {-100, 74, 40, 8, 200, 205, 200, .9}, {90, 74, 70, 5, 200, 205, 200, .8}, {-40, 72, 20, 12, 250, 252, 255, 1}, {0, -88, 200, 22, 250, 252, 255, 1}};
    for (const B& p : patches) blob(p.lon, p.lat, p.rx, p.ry, RGB{float(p.r), float(p.g), float(p.b)}, p.a, true);
    for (std::size_t k = 0; k < img.size(); k++) {            // a thin dark line along the coasts
        const float edge = 1 - std::fabs(2 * land[k] - 1);
        img[k] = mix(img[k], RGB{30, 60, 30}, 0.35f * edge);
    }
    for (int i = 0; i < 420; i++) {                           // clouds: thickest along the tropics and the storm belts
        const double band = hsh(i * 5 + 300);
        const double lat = band < .3 ? (hsh(i * 5 + 301) - .5) * 16 : band < .65 ? 42 + (hsh(i * 5 + 301) - .5) * 22 : -42 + (hsh(i * 5 + 301) - .5) * 22;
        blob(hsh(i * 5 + 302) * 360 - 180, lat, 5 + 14 * hsh(i * 5 + 303), 1.2 + 2.6 * hsh(i * 5 + 304), RGB{255, 255, 255}, .16 + .2 * hsh(i * 5 + 305), false);
    }
    std::vector<uint8_t> out(img.size() * 4);
    for (std::size_t k = 0; k < img.size(); k++) {
        out[k * 4] = uint8_t(std::clamp(img[k].r, 0.f, 255.f));
        out[k * 4 + 1] = uint8_t(std::clamp(img[k].g, 0.f, 255.f));
        out[k * 4 + 2] = uint8_t(std::clamp(img[k].b, 0.f, 255.f));
        out[k * 4 + 3] = 255;
    }
    return out;
}

}  // namespace render
