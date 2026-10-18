// Molecule Lab (0.21 stage 1): the Realistic style's data and maths (see density.h).
#include "molecule/density.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <unordered_map>

namespace molecule::dens {

// ---- reading the file (format: tools/make_density_data.py) ----
namespace {
struct Reader {
    const unsigned char* p; std::size_t n, at = 0; bool ok = true;
    template <class T> T get() { T v{}; if (at + sizeof(T) > n) { ok = false; return v; } std::memcpy(&v, p + at, sizeof(T)); at += sizeof(T); return v; }
    std::string str(std::size_t len) { if (at + len > n) { ok = false; return {}; } std::string s(reinterpret_cast<const char*>(p + at), len); at += len; return s.c_str(); }
    const unsigned char* take(std::size_t len) { if (len > n || at + len > n) { ok = false; return nullptr; } const unsigned char* q = p + at; at += len; return q; }
};
bool sane(int nx, int ny, int nz) { return nx > 1 && ny > 1 && nz > 1 && nx < 2000 && ny < 2000 && nz < 2000 && static_cast<long long>(nx) * ny * nz < 50000000; }
void decode(const unsigned char* q, std::size_t cnt, double lnMin, double lnStep, std::vector<float>& out) {
    out.resize(cnt);
    for (std::size_t i = 0; i < cnt; i++) {
        std::uint16_t k;
        std::memcpy(&k, q + 2 * i, 2);
        out[i] = k == 0 ? 0.0f : static_cast<float>(std::exp(lnMin + (k - 1) * lnStep));
    }
}
}  // namespace

bool parse(const unsigned char* p, std::size_t n, Data& d, std::string& err) {
    Reader r{p, n};
    if (n < 8 || std::memcmp(p, "PLDENS1\0", 8) != 0) { err = "not a PLDENS1 file"; return false; }
    r.at = 8;
    const int na = r.get<std::int32_t>();
    if (!r.ok || na < 1 || na > 64) { err = "bad atom count"; return false; }
    d.nuclei.clear();
    for (int i = 0; i < na; i++) { Nucleus u{}; u.Z = r.get<std::int32_t>(); u.x = r.get<double>(); u.y = r.get<double>(); u.z = r.get<double>(); d.nuclei.push_back(u); }
    for (double& v : d.dip) v = r.get<double>();
    d.energy = r.get<double>();
    d.electrons = r.get<std::int32_t>();
    d.spin2S = r.get<std::int32_t>();
    d.basis = r.str(16); d.method = r.str(8);
    const double lnMin = r.get<double>(), lnStep = r.get<double>();
    Grid& g = d.rho;
    g.ox = r.get<double>(); g.oy = r.get<double>(); g.oz = r.get<double>(); g.h = r.get<double>();
    g.nx = r.get<std::int32_t>(); g.ny = r.get<std::int32_t>(); g.nz = r.get<std::int32_t>();
    if (!r.ok || !sane(g.nx, g.ny, g.nz) || !(g.h > 0) || !(lnStep > 0)) { err = "bad density grid"; return false; }
    const std::size_t cnt = static_cast<std::size_t>(g.nx) * g.ny * g.nz;
    const unsigned char* q = r.take(2 * cnt);
    if (!q) { err = "density grid cut short"; return false; }
    decode(q, cnt, lnMin, lnStep, g.v);
    d.r1 = r.get<double>(); d.r0 = r.get<double>();
    const int nf = r.get<std::int32_t>();
    if (!r.ok || nf < 0 || nf > na || !(d.r0 > d.r1) || !(d.r1 >= 0)) { err = "bad fine cubes"; return false; }
    d.fine.assign(static_cast<std::size_t>(nf), Fine{});
    for (Fine& f : d.fine) {
        f.atom = r.get<std::int32_t>(); f.h = r.get<double>(); f.m = r.get<std::int32_t>();
        if (!r.ok || f.atom < 0 || f.atom >= na || f.m < 1 || f.m > 200 || !(f.h > 0) || f.h * f.m < d.r0 * 0.999) { err = "bad fine cube"; return false; }
        const std::size_t c = static_cast<std::size_t>(2 * f.m + 1) * (2 * f.m + 1) * (2 * f.m + 1);
        const unsigned char* fq = r.take(2 * c);
        if (!fq) { err = "fine cube cut short"; return false; }
        decode(fq, c, lnMin, lnStep, f.v);
    }
    Grid& P = d.pot;
    P.ox = r.get<double>(); P.oy = r.get<double>(); P.oz = r.get<double>(); P.h = r.get<double>();
    P.nx = r.get<std::int32_t>(); P.ny = r.get<std::int32_t>(); P.nz = r.get<std::int32_t>();
    const double scale = r.get<double>();
    if (!r.ok || !sane(P.nx, P.ny, P.nz) || !(P.h > 0) || !(scale > 0)) { err = "bad potential grid"; return false; }
    const std::size_t pc = static_cast<std::size_t>(P.nx) * P.ny * P.nz;
    const unsigned char* pq = r.take(2 * pc);
    if (!pq) { err = "potential grid cut short"; return false; }
    P.v.resize(pc);
    for (std::size_t i = 0; i < pc; i++) {
        std::int16_t k;
        std::memcpy(&k, pq + 2 * i, 2);
        P.v[i] = k == -32768 ? std::numeric_limits<float>::quiet_NaN() : static_cast<float>(k * scale);
    }
    if (r.at != n) { err = "trailing bytes"; return false; }
    int e = 0;
    for (const Nucleus& u : d.nuclei) e += u.Z;
    if (e != d.electrons) { err = "not a neutral molecule"; return false; }
    return true;
}

const Data* get(const std::string& id) {
    static std::map<std::string, std::unique_ptr<Data>> cache;     // decoded once (a few MB for the largest; only for molecules shown in the Realistic style)
    auto it = cache.find(id);
    if (it != cache.end()) return it->second.get();
    std::size_t n = 0;
    const unsigned char* p = embedded(id, n);
    std::unique_ptr<Data> d;
    if (p) {
        d = std::make_unique<Data>();
        std::string err;
        d->id = id;
        if (!parse(p, n, *d, err)) d.reset();
    }
    return cache.emplace(id, std::move(d)).first->second.get();
}

const MeasuredDipole* measuredDipole(const std::string& id) {
    for (const MeasuredDipole& m : kMeasuredDipole) if (id == m.id) return &m;
    return nullptr;
}

double Grid::sample(double x, double y, double z) const {
    const double fx = (x - ox) / h, fy = (y - oy) / h, fz = (z - oz) / h;
    if (!(fx >= 0 && fy >= 0 && fz >= 0 && fx <= nx - 1 && fy <= ny - 1 && fz <= nz - 1)) return 0;
    const int i = std::min(nx - 2, static_cast<int>(fx)), j = std::min(ny - 2, static_cast<int>(fy)), k = std::min(nz - 2, static_cast<int>(fz));
    const double tx = fx - i, ty = fy - j, tz = fz - k;
    double s = 0;
    for (int c = 0; c < 8; c++) {
        const int a = c & 1, b = (c >> 1) & 1, e = c >> 2;
        s += at(i + a, j + b, k + e) * (a ? tx : 1 - tx) * (b ? ty : 1 - ty) * (e ? tz : 1 - tz);
    }
    return s;
}

// ---- integrals ----
double smoothStep(double t) {
    if (t <= 0) return 1;
    if (t >= 1) return 0;
    const double a = std::exp(-1 / (1 - t)), b = std::exp(-1 / t);   // f(1 - t) / (f(1 - t) + f(t)), f(x) = exp(-1/x)
    return a / (a + b);
}
double cubeWeight(double d, double r1, double r0) { return smoothStep((d - r1) / (r0 - r1)); }

namespace {
// sum over the stored data of rho f(x, y, z) dV (f: 1, x, y or z)
template <class F> double integrate(const Data& d, F f) {
    const Grid& g = d.rho;
    double s = 0;
    for (int k = 0; k < g.nz; k++)
        for (int j = 0; j < g.ny; j++)
            for (int i = 0; i < g.nx; i++) {
                const double x = g.x(i), y = g.y(j), z = g.z(k);
                double w = 1;
                for (const Fine& c : d.fine) {
                    const Nucleus& u = d.nuclei[static_cast<std::size_t>(c.atom)];
                    const double dd = std::sqrt((x - u.x) * (x - u.x) + (y - u.y) * (y - u.y) + (z - u.z) * (z - u.z));
                    if (dd < d.r0) w -= cubeWeight(dd, d.r1, d.r0);
                }
                s += g.at(i, j, k) * w * f(x, y, z);
            }
    s *= g.h * g.h * g.h;
    for (const Fine& c : d.fine) {
        const Nucleus& u = d.nuclei[static_cast<std::size_t>(c.atom)];
        const int m = c.m, w = 2 * m + 1;
        double t = 0;
        for (int k = -m; k <= m; k++)
            for (int j = -m; j <= m; j++)
                for (int i = -m; i <= m; i++) {
                    const double dx = i * c.h, dy = j * c.h, dz = k * c.h, dd = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (dd >= d.r0) continue;
                    t += c.v[(static_cast<std::size_t>(k + m) * w + (j + m)) * w + (i + m)] * cubeWeight(dd, d.r1, d.r0) * f(u.x + dx, u.y + dy, u.z + dz);
                }
        s += t * c.h * c.h * c.h;
    }
    return s;
}
}  // namespace

double electronCount(const Data& d) { return integrate(d, [](double, double, double) { return 1.0; }); }
void dipoleFromGrid(const Data& d, double mu[3]) {
    mu[0] = -integrate(d, [](double x, double, double) { return x; });
    mu[1] = -integrate(d, [](double, double y, double) { return y; });
    mu[2] = -integrate(d, [](double, double, double z) { return z; });
    for (const Nucleus& u : d.nuclei) { mu[0] += u.Z * u.x; mu[1] += u.Z * u.y; mu[2] += u.Z * u.z; }
}
double rhoAt(const Data& d, double x, double y, double z) { return d.rho.sample(x, y, z); }

// ---- the dots ----
void sampleDots(const Data& d, int n, std::uint64_t seed, std::vector<float>& out, std::vector<double>& cdf) {
    // the same split as the integrals: a grid point's cell carries rho (1 - sum w) h^3, a fine cube point's cell rho w hf^3, so a nucleus's core electrons land in the
    // fine cells round it (a small bright point) instead of filling one 0.25 bohr cube
    const Grid& g = d.rho;
    std::size_t total = g.v.size();
    for (const Fine& c : d.fine) total += c.v.size();
    cdf.resize(total);
    double s = 0;
    const double h3 = g.h * g.h * g.h;
    for (int k = 0, idx = 0; k < g.nz; k++)
        for (int j = 0; j < g.ny; j++)
            for (int i = 0; i < g.nx; i++, idx++) {
                double w = 1;
                const double x = g.x(i), y = g.y(j), z = g.z(k);
                for (const Fine& c : d.fine) {
                    const Nucleus& u = d.nuclei[static_cast<std::size_t>(c.atom)];
                    const double dd = std::sqrt((x - u.x) * (x - u.x) + (y - u.y) * (y - u.y) + (z - u.z) * (z - u.z));
                    if (dd < d.r0) w -= cubeWeight(dd, d.r1, d.r0);
                }
                s += g.v[static_cast<std::size_t>(idx)] * std::max(0.0, w) * h3;
                cdf[static_cast<std::size_t>(idx)] = s;
            }
    std::size_t at = g.v.size();
    for (const Fine& c : d.fine) {
        const int m = c.m, w = 2 * m + 1;
        const double hf3 = c.h * c.h * c.h;
        for (int k = -m; k <= m; k++)
            for (int j = -m; j <= m; j++)
                for (int i = -m; i <= m; i++, at++) {
                    const double dd = c.h * std::sqrt(double(i * i + j * j + k * k));
                    s += c.v[(static_cast<std::size_t>(k + m) * w + (j + m)) * w + (i + m)] * (dd < d.r0 ? cubeWeight(dd, d.r1, d.r0) : 0.0) * hf3;
                    cdf[at] = s;
                }
    }
    out.resize(static_cast<std::size_t>(std::max(0, n)) * 3);
    Rng rng(seed);
    for (int t = 0; t < n; t++) {
        const double u = rng.uniform() * s;
        std::size_t idx = std::min<std::size_t>(total - 1, static_cast<std::size_t>(std::upper_bound(cdf.begin(), cdf.end(), u) - cdf.begin()));
        double x, y, z, h;
        if (idx < g.v.size()) {
            const int i = static_cast<int>(idx % static_cast<std::size_t>(g.nx)), j = static_cast<int>((idx / static_cast<std::size_t>(g.nx)) % static_cast<std::size_t>(g.ny)),
                      k = static_cast<int>(idx / (static_cast<std::size_t>(g.nx) * g.ny));
            x = g.x(i); y = g.y(j); z = g.z(k); h = g.h;
        } else {
            idx -= g.v.size();
            std::size_t ci = 0;
            while (idx >= d.fine[ci].v.size()) idx -= d.fine[ci++].v.size();
            const Fine& c = d.fine[ci];
            const std::size_t w = static_cast<std::size_t>(2 * c.m + 1);
            const Nucleus& nu = d.nuclei[static_cast<std::size_t>(c.atom)];
            x = nu.x + c.h * (static_cast<int>(idx % w) - c.m); y = nu.y + c.h * (static_cast<int>((idx / w) % w) - c.m); z = nu.z + c.h * (static_cast<int>(idx / (w * w)) - c.m); h = c.h;
        }
        x += (rng.uniform() - 0.5) * h; y += (rng.uniform() - 0.5) * h; z += (rng.uniform() - 0.5) * h;
        out[3 * static_cast<std::size_t>(t)] = static_cast<float>(x * kA0Angstrom);
        out[3 * static_cast<std::size_t>(t) + 1] = static_cast<float>(y * kA0Angstrom);
        out[3 * static_cast<std::size_t>(t) + 2] = static_cast<float>(z * kA0Angstrom);
    }
}

// ---- the surface ----
void marchTets(const float* f, int nx, int ny, int nz, double ox, double oy, double oz, double h, double iso, Mesh& out) {
    out.p.clear(); out.n.clear(); out.tri.clear(); out.value.clear();
    if (nx < 2 || ny < 2 || nz < 2) return;
    static const int kTet[6][4] = {{0, 1, 3, 7}, {0, 1, 5, 7}, {0, 2, 3, 7}, {0, 2, 6, 7}, {0, 4, 5, 7}, {0, 4, 6, 7}};   // corner bits: x 1, y 2, z 4; each a path 0 -> 7 along the axes
    std::unordered_map<long long, int> edgeVertex;
    edgeVertex.reserve(1 << 16);
    const auto index = [&](int i, int j, int k) { return (static_cast<long long>(k) * ny + j) * nx + i; };
    int ci[8][3];
    double cv[8];
    long long cidx[8];
    const auto vertexOn = [&](int a, int b) {                    // the crossing on the edge between corners a and b (one inside, one out)
        const int lo = (a & b) == a ? a : b, hi = lo == a ? b : a;   // in the Kuhn split every edge goes from a corner to one with more bits set
        const long long key = cidx[lo] * 8 + (hi ^ lo);
        auto it = edgeVertex.find(key);
        if (it != edgeVertex.end()) return it->second;
        double t = (iso - cv[a]) / (cv[b] - cv[a]);
        t = std::clamp(t, 1e-4, 1 - 1e-4);                     // never exactly on a grid point: no zero-area triangles
        const int v = static_cast<int>(out.p.size() / 3);
        for (int c = 0; c < 3; c++) {
            const double o = c == 0 ? ox : c == 1 ? oy : oz;
            out.p.push_back(static_cast<float>(o + h * (ci[a][c] + t * (ci[b][c] - ci[a][c]))));
        }
        edgeVertex.emplace(key, v);
        return v;
    };
    const auto emit = [&](int v0, int v1, int v2, const double dir[3]) {   // oriented so the normal points along dir (from inside to outside)
        const float* a = &out.p[3 * static_cast<std::size_t>(v0)]; const float* b = &out.p[3 * static_cast<std::size_t>(v1)]; const float* c = &out.p[3 * static_cast<std::size_t>(v2)];
        const double ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2], wx = c[0] - a[0], wy = c[1] - a[1], wz = c[2] - a[2];
        const double nx_ = uy * wz - uz * wy, ny_ = uz * wx - ux * wz, nz_ = ux * wy - uy * wx;
        if (nx_ * dir[0] + ny_ * dir[1] + nz_ * dir[2] < 0) std::swap(v1, v2);
        out.tri.push_back(v0); out.tri.push_back(v1); out.tri.push_back(v2);
    };
    for (int k = 0; k + 1 < nz; k++)
        for (int j = 0; j + 1 < ny; j++)
            for (int i = 0; i + 1 < nx; i++) {
                int inside = 0;
                for (int c = 0; c < 8; c++) {
                    ci[c][0] = i + (c & 1); ci[c][1] = j + ((c >> 1) & 1); ci[c][2] = k + (c >> 2);
                    cidx[c] = index(ci[c][0], ci[c][1], ci[c][2]);
                    cv[c] = f[cidx[c]];
                    inside += cv[c] > iso;
                }
                if (inside == 0 || inside == 8) continue;
                for (const auto& T : kTet) {
                    int in[4], ou[4], ni = 0, no = 0;
                    for (int q = 0; q < 4; q++) (cv[T[q]] > iso ? in[ni++] : ou[no++]) = T[q];
                    if (ni == 0 || no == 0) continue;
                    double dir[3] = {0, 0, 0};                     // from the inside corners' middle to the outside corners' middle
                    for (int q = 0; q < no; q++) for (int c = 0; c < 3; c++) dir[c] += ci[ou[q]][c] / double(no);
                    for (int q = 0; q < ni; q++) for (int c = 0; c < 3; c++) dir[c] -= ci[in[q]][c] / double(ni);
                    if (ni == 1) emit(vertexOn(in[0], ou[0]), vertexOn(in[0], ou[1]), vertexOn(in[0], ou[2]), dir);
                    else if (ni == 3) emit(vertexOn(ou[0], in[0]), vertexOn(ou[0], in[1]), vertexOn(ou[0], in[2]), dir);
                    else {
                        const int q0 = vertexOn(in[0], ou[0]), q1 = vertexOn(in[0], ou[1]), q2 = vertexOn(in[1], ou[1]), q3 = vertexOn(in[1], ou[0]);
                        emit(q0, q1, q2, dir);
                        emit(q0, q2, q3, dir);
                    }
                }
            }
    out.n.assign(out.p.size(), 0.0f);                              // area-weighted face normals, summed at the vertices
    for (std::size_t t = 0; t < out.tri.size(); t += 3) {
        const float* a = &out.p[3 * static_cast<std::size_t>(out.tri[t])]; const float* b = &out.p[3 * static_cast<std::size_t>(out.tri[t + 1])]; const float* c = &out.p[3 * static_cast<std::size_t>(out.tri[t + 2])];
        const float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2], wx = c[0] - a[0], wy = c[1] - a[1], wz = c[2] - a[2];
        const float n3[3] = {uy * wz - uz * wy, uz * wx - ux * wz, ux * wy - uy * wx};
        for (int q = 0; q < 3; q++) for (int c3 = 0; c3 < 3; c3++) out.n[3 * static_cast<std::size_t>(out.tri[t + q]) + c3] += n3[c3];
    }
    for (std::size_t v = 0; v < out.n.size(); v += 3) {
        const float l = std::sqrt(out.n[v] * out.n[v] + out.n[v + 1] * out.n[v + 1] + out.n[v + 2] * out.n[v + 2]);
        if (l > 0) for (int c = 0; c < 3; c++) out.n[v + c] /= l;
    }
    out.value.assign(out.p.size() / 3, 0.0f);
}

double enclosedVolume(const Mesh& m) {
    double v = 0;
    for (std::size_t t = 0; t < m.tri.size(); t += 3) {
        const float* a = &m.p[3 * static_cast<std::size_t>(m.tri[t])]; const float* b = &m.p[3 * static_cast<std::size_t>(m.tri[t + 1])]; const float* c = &m.p[3 * static_cast<std::size_t>(m.tri[t + 2])];
        v += (double(a[0]) * (double(b[1]) * c[2] - double(b[2]) * c[1]) - double(a[1]) * (double(b[0]) * c[2] - double(b[2]) * c[0]) + double(a[2]) * (double(b[0]) * c[1] - double(b[1]) * c[0])) / 6;
    }
    return v;
}

bool isClosed(const Mesh& m, std::string* why) {
    if (m.tri.empty()) { if (why) *why = "no triangles"; return false; }
    std::unordered_map<long long, int> edge;                     // directed edge -> uses; a closed, consistently oriented surface uses each directed edge once and its reverse once
    edge.reserve(m.tri.size() * 2);
    const long long nv = static_cast<long long>(m.p.size() / 3);
    for (std::size_t t = 0; t < m.tri.size(); t += 3)
        for (int q = 0; q < 3; q++) edge[m.tri[t + q] * nv + m.tri[t + (q + 1) % 3]]++;
    for (const auto& e : edge) {
        const long long a = e.first / nv, b = e.first % nv;
        auto r = edge.find(b * nv + a);
        if (e.second != 1 || r == edge.end() || r->second != 1) {
            if (why) *why = "edge " + std::to_string(a) + "-" + std::to_string(b) + " used " + std::to_string(e.second) + " times, reverse " + std::to_string(r == edge.end() ? 0 : r->second);
            return false;
        }
    }
    return true;
}

void densitySurface(const Data& d, double iso, int stride, Mesh& out) {
    const Grid& g = d.rho;
    stride = std::max(1, stride);
    const int nx = (g.nx - 1) / stride + 1, ny = (g.ny - 1) / stride + 1, nz = (g.nz - 1) / stride + 1;
    static std::vector<float> f;
    f.resize(static_cast<std::size_t>(nx) * ny * nz);
    for (int k = 0; k < nz; k++)
        for (int j = 0; j < ny; j++)
            for (int i = 0; i < nx; i++) f[(static_cast<std::size_t>(k) * ny + j) * nx + i] = std::log(std::max(g.at(i * stride, j * stride, k * stride), 1e-30f));   // ln rho: closer to linear between grid points
    marchTets(f.data(), nx, ny, nz, g.ox, g.oy, g.oz, g.h * stride, std::log(iso), out);
    const double e = g.h;                                        // smooth normals: minus the gradient of ln rho (central differences on the full grid), not the faces' (grid-aligned) normals
    for (std::size_t v = 0; v < out.p.size(); v += 3) {
        const double x = out.p[v], y = out.p[v + 1], z = out.p[v + 2];
        out.value[v / 3] = static_cast<float>(d.pot.sample(x, y, z));   // the potential at the vertex (bohr), au
        const auto L = [&](double a, double b, double c) { return std::log(std::max(1e-30, g.sample(a, b, c))); };
        const double gx = L(x + e, y, z) - L(x - e, y, z), gy = L(x, y + e, z) - L(x, y - e, z), gz = L(x, y, z + e) - L(x, y, z - e), gl = std::sqrt(gx * gx + gy * gy + gz * gz);
        if (gl > 0) { out.n[v] = static_cast<float>(-gx / gl); out.n[v + 1] = static_cast<float>(-gy / gl); out.n[v + 2] = static_cast<float>(-gz / gl); }
        for (int c = 0; c < 3; c++) out.p[v + c] = static_cast<float>(out.p[v + c] * kA0Angstrom);
    }
}

double potentialRange(const Mesh& m) {
    double r = 0;
    for (float v : m.value) if (std::isfinite(v)) r = std::max(r, std::fabs(double(v)));
    return r;
}

}  // namespace molecule::dens
