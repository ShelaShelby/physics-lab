#include "atom/hf.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>

namespace atom::hf {

namespace {
// 0.39's published nonrelativistic orbitals (Koga et al.) and NIST's table, as tools/make_hf_data.py wrote them
struct HfSto { int n; double zeta, c; };
struct HfOrbitalRec { int n, l, occ; double eps; int first, count; };
struct HfAtomRec { int Z, charge; const char* conf; const char* term; double energy; int first, count; };
struct NistIeRec { int Z, charge; const char *sp, *shells, *level, *prefix, *ie, *unc, *refs; };
#include "atom/hf_data.inc"

double factorial(int k) { double f = 1; for (int i = 2; i <= k; i++) f *= i; return f; }

// ---- data/dhf/dhf.bin (tools/dhf/make_dhf_data.py): little endian.
// "PLDHF001", u32 records; each: u16 Z, u16 A, i16 2J, u8 relativistic, u8 parts, f64 energy, three strings (u8 length + bytes: shells, level, source), then per part:
// u8 n, u8 l, u8 2j, u8 nb, f64 occ, f64 eps, f64 amin, f64 ratio, f64 dP[nb], f64 dQ[nb].
struct Reader {
    const unsigned char* p; std::size_t n, i = 0; bool ok = true;
    template <class T> T get() { T v{}; if (i + sizeof(T) > n) { ok = false; return v; } std::memcpy(&v, p + i, sizeof(T)); i += sizeof(T); return v; }
    std::string str() { const std::size_t k = get<std::uint8_t>(); if (i + k > n) { ok = false; return {}; } std::string s(reinterpret_cast<const char*>(p + i), k); i += k; return s; }
};

std::vector<Atom>& atoms() {
    static std::vector<Atom> v = [] {
        std::vector<Atom> out;
        std::size_t n = 0;
        const unsigned char* p = embeddedDhf(n);
        if (!p || n < 12 || std::memcmp(p, "PLDHF001", 8) != 0) return out;
        Reader rd{p, n}; rd.i = 8;
        const std::uint32_t count = rd.get<std::uint32_t>();
        for (std::uint32_t k = 0; k < count && rd.ok; k++) {
            Atom a;
            a.Z = rd.get<std::uint16_t>(); a.A = rd.get<std::uint16_t>(); a.J2 = rd.get<std::int16_t>();
            a.relativistic = rd.get<std::uint8_t>() != 0;
            const int np = rd.get<std::uint8_t>();
            a.energy = rd.get<double>();
            a.shells = rd.str(); a.level = rd.str(); a.source = rd.str();
            for (int q = 0; q < np && rd.ok; q++) {
                Part pt;
                const int nn = rd.get<std::uint8_t>(), l = rd.get<std::uint8_t>();
                pt.j2 = rd.get<std::uint8_t>();
                const int nb = rd.get<std::uint8_t>();
                pt.occ = rd.get<double>(); pt.eps = rd.get<double>(); pt.amin = rd.get<double>(); pt.ratio = rd.get<double>();
                pt.dP.resize(std::size_t(nb)); pt.dQ.resize(std::size_t(nb));
                for (double& x : pt.dP) x = rd.get<double>();
                for (double& x : pt.dQ) x = rd.get<double>();
                Subshell* s = nullptr;
                for (Subshell& t : a.sub) if (t.n == nn && t.l == l) s = &t;
                if (!s) { a.sub.push_back(Subshell{}); s = &a.sub.back(); s->n = nn; s->l = l; }
                s->parts.push_back(pt);
            }
            for (Subshell& s : a.sub) {
                double o = 0, e = 0;
                for (const Part& pt : s.parts) { o += pt.occ; e += pt.occ * pt.eps; }
                s.occ = int(std::lround(o)); s.eps = o > 0 ? e / o : 0;
            }
            std::sort(a.sub.begin(), a.sub.end(), [](const Subshell& x, const Subshell& y) { return x.n != y.n ? x.n < y.n : x.l < y.l; });
            if (rd.ok) out.push_back(a);
        }
        return out;
    }();
    return v;
}

// P/r and Q/r of one part (finite at r = 0)
double tabValueIn(const std::vector<float>& f, double r0, double h, int l, double r);
void evalPart(const Part& pt, int l, double r, double& pr, double& qr) {
    if (!pt.tabP.empty()) {                                // 0.47.2: tabulated (the same interpolation as tools/grasp/multipoles.py app_eval, whose check is in data/jumps/grasp/<X>/levels.tsv)
        pr = tabValueIn(pt.tabP, pt.tabR0, pt.tabH, l, r); qr = tabValueIn(pt.tabQ, pt.tabR0, pt.tabH, l, r);
        return;
    }
    const int kappa = pt.j2 == 2 * l + 1 ? -(l + 1) : l;
    const double r2 = r * r, rl = std::pow(r, l), lk = l + 1 + kappa;
    pr = 0; qr = 0;
    double a = pt.amin;
    for (std::size_t i = 0; i < pt.dP.size(); i++, a *= pt.ratio) {
        const double e = std::exp(-a * r2);
        if (e == 0) continue;
        pr += pt.dP[i] * rl * e;
        if (pt.dQ[i] != 0) qr += pt.dQ[i] * ((lk != 0 ? lk * std::pow(r, l - 1) : 0.0) - 2 * a * rl * r) * e;
    }
}

// Tables of one subshell on r = e^t, t from ln 1e-7 to ln 250 (4,001 points): D, its distribution function, and the same for the plane density D / r
struct Prep {
    std::vector<double> gx, gc, gd, pc, pd;
    double total = 1, ptotal = 1, mean = 0;
    int dom = 0;                                           // the part whose large component gives the sign
};
std::map<const Subshell*, std::unique_ptr<Prep>>& prepCache() { static std::map<const Subshell*, std::unique_ptr<Prep>> m; return m; }

double rawD(const Subshell& s, double r) {                // occupation-weighted r^2 (P^2 + Q^2) / r^2 * r^2
    double d = 0;
    for (const Part& pt : s.parts) {
        if (!(pt.occ > 0)) continue;
        double pr, qr; evalPart(pt, s.l, r, pr, qr);
        d += pt.occ / electronsIn(s) * (pr * pr + qr * qr);
    }
    return d * r * r;
}

// cumulative integral of f over the grid (uniform in t, integrand f r dt), trapezoid with the Euler-Maclaurin end correction per interval (4th order)
std::vector<double> cumulate(const std::vector<double>& t, const std::vector<double>& g) {
    const std::size_t N = t.size();
    std::vector<double> c(N, 0.0), d(N, 0.0);
    const double h = t[1] - t[0];
    for (std::size_t i = 0; i < N; i++) d[i] = (i == 0 ? g[1] - g[0] : i == N - 1 ? g[N - 1] - g[N - 2] : 0.5 * (g[i + 1] - g[i - 1])) / h;
    for (std::size_t i = 1; i < N; i++) c[i] = c[i - 1] + 0.5 * h * (g[i - 1] + g[i]) - h * h / 12 * (d[i] - d[i - 1]);
    return c;
}

const Prep& prep(const Subshell& s) {
    auto& m = prepCache();
    auto it = m.find(&s);
    if (it != m.end()) return *it->second;
    auto p = std::make_unique<Prep>();
    const int N = 4001;
    const double t0 = std::log(1e-7), t1 = std::log(250.0);
    std::vector<double> t(N), g(N), gp(N), gm(N);
    p->gx.resize(N); p->gd.resize(N); p->pd.resize(N);
    for (int i = 0; i < N; i++) {
        t[std::size_t(i)] = t0 + (t1 - t0) * i / (N - 1);
        const double r = std::exp(t[std::size_t(i)]), d = rawD(s, r);
        p->gx[std::size_t(i)] = r; p->gd[std::size_t(i)] = d; p->pd[std::size_t(i)] = d / r;
        g[std::size_t(i)] = d * r; gp[std::size_t(i)] = d; gm[std::size_t(i)] = d * r * r;   // integrands in t: D dr = D r dt, (D/r) dr = D dt, r D dr = D r^2 dt
    }
    std::vector<double> c = cumulate(t, g), cp = cumulate(t, gp), cm = cumulate(t, gm);
    p->total = c.back(); p->ptotal = cp.back(); p->mean = cm.back() / p->total;
    p->gc.resize(N); p->pc.resize(N);
    for (int i = 0; i < N; i++) {
        const std::size_t k = std::size_t(i);
        p->gc[k] = std::min(1.0, std::max(0.0, c[k] / p->total)); p->gd[k] /= p->total;
        p->pc[k] = std::min(1.0, std::max(0.0, cp[k] / p->ptotal)); p->pd[k] /= p->ptotal;
    }
    p->gc.front() = 0; p->gc.back() = 1; p->pc.front() = 0; p->pc.back() = 1;
    double best = -1;
    for (std::size_t q = 0; q < s.parts.size(); q++) if (s.parts[q].occ > best + 1e-12) { best = s.parts[q].occ; p->dom = int(q); }
    const Prep& ref = *p;
    m.emplace(&s, std::move(p));
    return ref;
}

double interpCdf(const std::vector<double>& gx, const std::vector<double>& gc, const std::vector<double>& gd, double r) {   // cubic Hermite in r
    if (r <= gx.front()) return 0;
    if (r >= gx.back()) return 1;
    const std::size_t hi = std::size_t(std::upper_bound(gx.begin(), gx.end(), r) - gx.begin()), lo = hi - 1;
    const double h = gx[hi] - gx[lo], u = (r - gx[lo]) / h, u2 = u * u, u3 = u2 * u;
    return (2 * u3 - 3 * u2 + 1) * gc[lo] + (u3 - 2 * u2 + u) * gd[lo] * h + (-2 * u3 + 3 * u2) * gc[hi] + (u3 - u2) * gd[hi] * h;
}
}  // namespace

double tabValue(const std::vector<float>& f, double r0, double h, int l, double r) {
    const int n = int(f.size());
    if (n < 4 || !(r > 0)) return 0;
    const double t = std::log(r / r0) / h;
    if (t < 0) return f[0] * std::pow(r / r0, l + 1);
    if (t >= n - 1) return 0;
    const int j0 = std::min(std::max(int(std::floor(t)) - 1, 0), n - 4);
    const double x = t - j0, y0 = f[std::size_t(j0)], y1 = f[std::size_t(j0 + 1)], y2 = f[std::size_t(j0 + 2)], y3 = f[std::size_t(j0 + 3)];
    return -y0 * (x - 1) * (x - 2) * (x - 3) / 6 + y1 * x * (x - 2) * (x - 3) / 2 - y2 * x * (x - 1) * (x - 3) / 2 + y3 * x * (x - 1) * (x - 2) / 6;
}
namespace { double tabValueIn(const std::vector<float>& f, double r0, double h, int l, double r) { return tabValue(f, r0, h, l, r); } }

const unsigned char* embeddedDhf(std::size_t& size);       // (cmake/embed_dhf.cmake)

int Atom::electrons() const { int n = 0; for (const Subshell& s : sub) n += s.occ; return n; }
double electronsIn(const Subshell& s) { return s.focc >= 0 ? s.focc : double(s.occ); }

const Atom* find(int Z, int charge, bool relativistic) {
    if (charge != 0 || Z < 2) return nullptr;
    for (const Atom& a : atoms()) if (a.Z == Z && a.relativistic == relativistic) return &a;
    return nullptr;
}
bool hasNeutral(int Z) { return find(Z, 0) != nullptr; }

double radialDensity(const Subshell& s, double r) { return r <= 0 ? 0 : std::max(0.0, rawD(s, r) / prep(s).total); }
double radialR(const Subshell& s, double r) {
    const Prep& p = prep(s);
    double d = 0, sign = 1;
    for (std::size_t q = 0; q < s.parts.size(); q++) {
        const Part& pt = s.parts[q];
        double pr, qr; evalPart(pt, s.l, r, pr, qr);
        if (int(q) == p.dom) sign = pr >= 0 ? 1 : -1;
        if (pt.occ > 0) d += pt.occ / s.occ * (pr * pr + qr * qr);
    }
    return sign * std::sqrt(d / p.total);
}
double meanRadius(const Subshell& s) { return prep(s).mean; }
double radialCdf(const Subshell& s, double r) { const Prep& p = prep(s); return interpCdf(p.gx, p.gc, p.gd, r); }
double totalRadial(const Atom& a, double r) { double d = 0; for (const Subshell& s : a.sub) if (electronsIn(s) > 0) d += electronsIn(s) * radialDensity(s, r); return d; }
double partMeanRadius(const Part& pt, int l) {
    const int N = 4001;
    const double t0 = std::log(1e-7), t1 = std::log(250.0);
    std::vector<double> t(N), g(N), gm(N);
    for (int i = 0; i < N; i++) {
        t[std::size_t(i)] = t0 + (t1 - t0) * i / (N - 1);
        const double r = std::exp(t[std::size_t(i)]);
        double pr, qr; evalPart(pt, l, r, pr, qr);
        const double d = (pr * pr + qr * qr) * r * r;
        g[std::size_t(i)] = d * r; gm[std::size_t(i)] = d * r * r;
    }
    return cumulate(t, gm).back() / cumulate(t, g).back();
}

// The inverse from the table of the distribution function and of its derivative (the density): the cubic Hermite polynomial on the bracketing interval, solved by Newton's
// method kept inside the interval (0.39.1).
static double invertTable(const std::vector<double>& gx, const std::vector<double>& gc, const std::vector<double>& gd, double u) {
    const std::size_t hi = std::min(gx.size() - 1, std::max<std::size_t>(1, std::size_t(std::upper_bound(gc.begin(), gc.end(), u) - gc.begin())));
    const std::size_t lo = hi - 1;
    const double x0 = gx[lo], x1 = gx[hi], h = x1 - x0, c0 = gc[lo], c1 = gc[hi], d0 = gd[lo] * h, d1 = gd[hi] * h;
    if (!(c1 > c0)) return x0;
    const auto H = [&](double t) { const double t2 = t * t, t3 = t2 * t;
        return (2 * t3 - 3 * t2 + 1) * c0 + (t3 - 2 * t2 + t) * d0 + (-2 * t3 + 3 * t2) * c1 + (t3 - t2) * d1; };
    const auto dH = [&](double t) { const double t2 = t * t; return (6 * t2 - 6 * t) * c0 + (3 * t2 - 4 * t + 1) * d0 + (-6 * t2 + 6 * t) * c1 + (3 * t2 - 2 * t) * d1; };
    double a = 0, b = 1, t = std::min(1.0, std::max(0.0, (u - c0) / (c1 - c0)));
    for (int it = 0; it < 40; it++) {
        const double f = H(t) - u, d = dH(t);
        if (f > 0) b = t; else a = t;
        double nt = d > 0 ? t - f / d : 0.5 * (a + b);
        if (!(nt > a && nt < b)) nt = 0.5 * (a + b);
        if (std::fabs(nt - t) < 1e-14) { t = nt; break; }
        t = nt;
    }
    return x0 + t * h;
}
double sampleRadius(const Subshell& s, orbit::Mulberry32& rng) { const Prep& p = prep(s); return invertTable(p.gx, p.gc, p.gd, rng.next()); }

double planeTail(const Subshell& s, double off) {        // integral from |off| to infinity of D / r
    const Prep& p = prep(s);
    return (1 - interpCdf(p.gx, p.pc, p.pd, std::fabs(off))) * p.ptotal / p.total;
}
Plane planeOf(const Atom& a, double off) {
    Plane pl;
    pl.off = off;
    for (const Subshell& sh : a.sub) {
        const Prep& p = prep(sh);
        const double e = electronsIn(sh);
        const double start = e > 0 ? interpCdf(p.gx, p.pc, p.pd, std::fabs(off)) : 1;
        pl.mass.push_back(e > 0 ? e * 0.5 * (1 - start) * p.ptotal / p.total : 0);
        pl.start.push_back(std::min(1.0, std::max(0.0, start)));
        pl.total += pl.mass.back();
    }
    return pl;
}
double samplePlaneRadius(const Subshell& s, double start, orbit::Mulberry32& rng) {
    const Prep& p = prep(s);
    return invertTable(p.gx, p.pc, p.pd, start + rng.next() * (1 - start));
}
bool samplePlane(const Atom& a, const Plane& pl, int one, orbit::Mulberry32& rng, double& radius, int& shellOut) {
    int i = one;
    if (i < 0) {
        if (!(pl.total > 0)) return false;
        double pick = rng.next() * pl.total;
        for (std::size_t j = 0; j < a.sub.size(); j++) {
            if (!(pl.mass[j] > 0)) continue;
            i = int(j);
            if (pick < pl.mass[j]) break;
            pick -= pl.mass[j];
        }
        if (i < 0) return false;
    } else if (!(pl.mass[std::size_t(i)] > 0)) return false;
    shellOut = i;
    radius = std::max(std::fabs(pl.off), samplePlaneRadius(a.sub[std::size_t(i)], pl.start[std::size_t(i)], rng));
    return true;
}

void sampleAtom(const Atom& a, orbit::Mulberry32& r, double& x, double& y, double& z, int& shellOut) {
    double ne = 0;
    for (const Subshell& s : a.sub) ne += std::max(0.0, electronsIn(s));
    double pick = r.next() * ne;
    shellOut = -1;
    for (std::size_t i = 0; i < a.sub.size(); i++) {
        const double e = electronsIn(a.sub[i]);
        if (!(e > 0)) continue;
        shellOut = int(i);
        if (pick < e) break;
        pick -= e;
    }
    const double rad = sampleRadius(a.sub[std::size_t(shellOut)], r);
    const double cz = 2 * r.next() - 1, ph = 2 * 3.14159265358979323846 * r.next(), sz = std::sqrt(std::max(0.0, 1 - cz * cz));
    x = rad * sz * std::cos(ph); y = rad * sz * std::sin(ph); z = rad * cz;
}

int subshellIndex(const Atom& a, int n, int l) {
    for (std::size_t i = 0; i < a.sub.size(); i++) if (a.sub[i].n == n && a.sub[i].l == l) return int(i);
    return -1;
}

std::string configText(const Atom& a) {
    std::string out;
    for (const Subshell& s : a.sub) if (s.occ) out += (out.empty() ? "" : " ") + std::to_string(s.n) + "spdfg"[s.l] + std::to_string(s.occ);
    return out;
}
std::string jText(const Subshell& s) {
    std::string out;
    for (const Part& p : s.parts) {
        char b[48];
        std::snprintf(b, sizeof b, "%d%c%d/2 %.2f", s.n, "spdfg"[s.l], p.j2, p.occ);
        out += (out.empty() ? "" : " · ") + std::string(b);
    }
    return out;
}
const char* shellLetter(int n) { static const char* const k[8] = {"", "K", "L", "M", "N", "O", "P", "Q"}; return n >= 1 && n <= 7 ? k[n] : "?"; }

double kogaMeanRadius(int Z, int n, int l) {
    for (const HfAtomRec& a : kHfAtom) {
        if (a.Z != Z || a.charge != 0) continue;
        for (int i = a.first; i < a.first + a.count; i++) {
            const HfOrbitalRec& o = kHfOrb[i];
            if (o.n != n || o.l != l) continue;
            double tot = 0, m = 0;
            for (int p = o.first; p < o.first + o.count; p++)
                for (int q = o.first; q < o.first + o.count; q++) {
                    const HfSto &u = kHfSto[p], &v = kHfSto[q];
                    const double nu = std::pow(2 * u.zeta, u.n + 0.5) / std::sqrt(factorial(2 * u.n)), nv = std::pow(2 * v.zeta, v.n + 0.5) / std::sqrt(factorial(2 * v.n));
                    const int k = u.n + v.n; const double z = u.zeta + v.zeta, w = u.c * v.c * nu * nv;
                    tot += w * factorial(k) / std::pow(z, k + 1);
                    m += w * factorial(k + 1) / std::pow(z, k + 2);
                }
            return m / tot;
        }
    }
    return -1;
}

const NistIe* nistIe(int Z, int charge) {
    static std::vector<NistIe> v = [] {
        std::vector<NistIe> out;
        for (const NistIeRec& r : kNistIe) out.push_back({r.Z, r.charge, r.sp, r.shells, r.level, r.prefix, r.ie, r.unc, r.refs});
        return out;
    }();
    for (const NistIe& n : v) if (n.Z == Z && n.charge == charge) return &n;
    return nullptr;
}
std::string nistKind(const NistIe& n) { return n.prefix == "[" ? "semi-empirical" : n.prefix == "(" ? "theoretical" : "measured"; }
std::string levelText(const std::string& s) {
    if (s.empty() || s[0] == '(') return s;                    // jj-coupled labels (heavy atoms) as NIST writes them
    static const char* const sup[10] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
    static const char* const sub[10] = {"₀", "₁", "₂", "₃", "₄", "₅", "₆", "₇", "₈", "₉"};
    std::string out;
    std::size_t i = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) out += sup[s[i++] - '0'];
    while (i < s.size() && std::isalpha(static_cast<unsigned char>(s[i]))) out += s[i++];
    if (i < s.size() && s[i] == '*') { out += "°"; i++; }
    for (; i < s.size(); i++) {
        const char c = s[i];
        if (c == '<' || c == '>') continue;
        if (std::isdigit(static_cast<unsigned char>(c))) out += sub[c - '0'];
        else if (c == '/') out += "⁄";
        else out += c;
    }
    return out;
}
double levelJ(const std::string& s) {
    // the J is the last number: "<9/2>", "<4>", "1S0", "(1/2,1/2)0", "0"
    std::size_t e = s.size();
    while (e > 0 && (s[e - 1] == '>' || s[e - 1] == ' ')) e--;
    std::size_t b = e;
    while (b > 0 && (std::isdigit(static_cast<unsigned char>(s[b - 1])) || s[b - 1] == '/')) b--;
    if (b == e) return -1;
    const std::string v = s.substr(b, e - b);
    const std::size_t sl = v.find('/');
    if (sl == std::string::npos) return std::atof(v.c_str());
    return std::atof(v.substr(0, sl).c_str()) / std::atof(v.substr(sl + 1).c_str());
}
std::string shellsText(const std::string& s) { std::string o = s; std::replace(o.begin(), o.end(), '.', ' '); return o; }

namespace {
// NIST's ground shells as occupations: "[Ar].3d6.4s2" -> {3d: 6, 4s: 2, + argon's}; a core in brackets is NIST's own row for that element
void addShells(const std::string& shells, std::map<std::string, int>& occ, int depth) {
    std::size_t i = 0;
    while (i < shells.size()) {
        std::size_t j = shells.find('.', i);
        if (j == std::string::npos) j = shells.size();
        const std::string part = shells.substr(i, j - i);
        i = j + 1;
        if (part.size() > 2 && part.front() == '[' && part.back() == ']') {
            const std::string sym = part.substr(1, part.size() - 2);
            for (int Z = 1; Z <= 118 && depth < 8; Z++) {   // ([Xe] -> [Kr] -> [Ar] -> [Ne] -> [He]: five levels)
                const NistIe* n = nistIe(Z, 0);
                if (n && n->sp == sym + " I") { addShells(n->shells, occ, depth + 1); break; }
            }
            continue;
        }
        std::size_t k = 0;
        while (k < part.size() && std::isdigit(static_cast<unsigned char>(part[k]))) k++;
        if (k == 0 || k >= part.size()) continue;
        const std::string nl = part.substr(0, k + 1), num = part.substr(k + 1);
        occ[nl] += num.empty() ? 1 : std::atoi(num.c_str());
    }
}
}  // namespace

bool configMatchesNist(const Atom& a, const NistIe& n) {
    std::map<std::string, int> want, have;
    addShells(n.shells, want, 0);
    for (const Subshell& s : a.sub) if (s.occ) have[std::to_string(s.n) + (s.l >= 0 && s.l < 7 ? "spdfghi"[s.l] : '?')] += s.occ;   // 0.50.3: l = 4 read the literal's NUL
    return want == have;
}

}  // namespace atom::hf
