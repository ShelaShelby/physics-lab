// The Zeeman effect (zeeman.h): the field states of one LS term, the split lines and their strengths, and the words the app shows with them.
#include "atom/zeeman.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <tuple>
#include <utility>

#include "atom/alkali.h"
#include "atom/atom_core.h"
#include "atom/dirac.h"
#include "atom/hfs.h"
#include "atom/levels.h"
#include "atom/threej.h"

namespace atom::zeeman {
namespace {
#include "atom/lande_data.inc"

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
const char* const kL = "SPDFGHIKLMNOQRTUV";   // spectroscopic letters for L = 0, 1, 2 ... (J is skipped, as Condon & Shortley write them)

std::string fmt(const char* f, double v) { char b[64]; std::snprintf(b, sizeof b, f, v); return b; }
// a number with about 3 significant digits, plain or as a power of ten
std::string num3(double v) {
    const double a = std::fabs(v);
    if (a == 0) return "0";
    if (a >= 1e-3 && a < 1e5) return fmt(a >= 100 ? "%.0f" : a >= 10 ? "%.1f" : a >= 1 ? "%.2f" : a >= 0.1 ? "%.3f" : a >= 0.01 ? "%.4f" : "%.5f", v);
    const int e = static_cast<int>(std::floor(std::log10(a)));
    std::string sup;
    const std::string es = std::to_string(e);
    static const char* const d[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
    for (const char ch : es) sup += ch == '-' ? std::string("⁻") : std::string(d[ch - '0']);
    return fmt("%.2f", v / std::pow(10.0, e)) + " × 10" + sup;
}
std::string jText(int J2) { return J2 % 2 ? std::to_string(J2) + "/2" : std::to_string(J2 / 2); }
double cmPerEV() { return 1e7 / kHcEVnm; }   // 1 eV in cm^-1: 1 / (h c), h c = kHcEVnm eV nm (exact)
const char* const kSup[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
std::string multText(int S2) { const int m = S2 + 1; return m < 10 ? std::string(kSup[m]) : std::to_string(m); }

// <J M| S_z |J' M> inside the term (L, S): sum over M_S of the two Clebsch-Gordan coefficients times M_S (all arguments doubled)
double szElement(int L, int S2, int J2a, int J2b, int M2) {
    double s = 0;
    for (int MS2 = -S2; MS2 <= S2; MS2 += 2) {
        const int ML2 = M2 - MS2;
        if (std::abs(ML2) > 2 * L) continue;
        s += clebschGordan(2 * L, ML2, S2, MS2, J2a, M2) * clebschGordan(2 * L, ML2, S2, MS2, J2b, M2) * (MS2 / 2.0);
    }
    return s;
}

// eigenvalues and eigenvectors of a small real symmetric matrix by Jacobi rotations (Golub & Van Loan, "Matrix Computations", the cyclic Jacobi method);
// a is destroyed, v holds the eigenvectors as columns
void jacobi(std::vector<std::vector<double>>& a, std::vector<double>& w, std::vector<std::vector<double>>& v) {
    const std::size_t n = a.size();
    v.assign(n, std::vector<double>(n, 0));
    for (std::size_t i = 0; i < n; i++) v[i][i] = 1;
    for (int sweep = 0; sweep < 100; sweep++) {
        double off = 0, all = 0;
        for (std::size_t i = 0; i < n; i++) for (std::size_t j = 0; j < n; j++) { all += a[i][j] * a[i][j]; if (i != j) off += a[i][j] * a[i][j]; }
        if (off <= 1e-34 * all || off == 0) break;
        for (std::size_t p = 0; p < n; p++)
            for (std::size_t q = p + 1; q < n; q++) {
                if (a[p][q] == 0) continue;
                const double th = (a[q][q] - a[p][p]) / (2 * a[p][q]);
                const double t = (th >= 0 ? 1.0 : -1.0) / (std::fabs(th) + std::sqrt(th * th + 1));
                const double c = 1 / std::sqrt(t * t + 1), s = t * c;
                for (std::size_t k = 0; k < n; k++) {             // A <- A R (columns p, q)
                    const double akp = a[k][p], akq = a[k][q];
                    a[k][p] = c * akp - s * akq; a[k][q] = s * akp + c * akq;
                }
                for (std::size_t k = 0; k < n; k++) {             // A <- R^T A (rows p, q)
                    const double apk = a[p][k], aqk = a[q][k];
                    a[p][k] = c * apk - s * aqk; a[q][k] = s * apk + c * aqk;
                }
                for (std::size_t k = 0; k < n; k++) {
                    const double vkp = v[k][p], vkq = v[k][q];
                    v[k][p] = c * vkp - s * vkq; v[k][q] = s * vkp + c * vkq;
                }
            }
    }
    w.resize(n);
    for (std::size_t i = 0; i < n; i++) w[i] = a[i][i];
}

std::string orbName(int n, int l) { return std::to_string(n) + "spdfghik"[l]; }
std::string termName(int L, int S2, bool odd) { return multText(S2) + std::string(1, kL[L]) + (odd ? "°" : ""); }
// fills gLin / gMeasured of a term without a matrix: NIST's measured g where ASD lists one, else the LS value
void linear(Term& t, const std::vector<double>& measured) {
    t.mode = Mode::Linear;
    t.gLin.clear(); t.gMeasured.clear();
    for (std::size_t k = 0; k < t.J2.size(); k++) {
        const bool m = k < measured.size() && std::isfinite(measured[k]);
        t.gLin.push_back(m ? measured[k] : landeLS(t.L, t.S2, t.J2[k], gsCodata()));
        t.gMeasured.push_back(m);
    }
}
// the term's mode from its J levels: a matrix when every J from |L - S| to L + S is there and there are at least two; else linear (the reason in why)
void decide(Term& t, const std::vector<double>& measured) {
    std::vector<int> want;
    for (int J2 = std::abs(2 * t.L - t.S2); J2 <= 2 * t.L + t.S2; J2 += 2) want.push_back(J2);
    bool all = true;
    std::string miss;
    for (const int J2 : want) if (std::find(t.J2.begin(), t.J2.end(), J2) == t.J2.end()) { all = false; miss += (miss.empty() ? "" : ", ") + jText(J2); }
    if (want.size() == 1) { linear(t, measured); t.why = "The term " + t.name + " has one level only (J = " + jText(t.J2[0]) + "), so no other level mixes in: it splits in proportion to the field."; return; }
    if (!all) { linear(t, measured); t.why = "The lab does not have every level of " + t.name + " (J = " + miss + " missing), so its levels are not mixed: each splits with its own g (weak field only)."; return; }
    t.mode = Mode::Matrix;
    t.gLin.clear(); t.gMeasured.clear();
}
}  // namespace

double lorentzCmPerT() { return kMuBJT / (kPlanckJs * kC) / 100; }
double gsCodata() { return -kGe; }
double landeLS(int L, int S2, int J2, double gs) {
    if (J2 == 0) return 0;
    const double J = J2 / 2.0, S = S2 / 2.0;
    return 1 + (gs - 1) * (J * (J + 1) + S * (S + 1) - L * (L + 1.0)) / (2 * J * (J + 1));
}

bool parseLS(const std::string& t, int& L, int& S2) {
    std::size_t i = 0;
    while (i < t.size() && std::isdigit(static_cast<unsigned char>(t[i]))) i++;
    if (i == 0 || i >= t.size()) return false;
    const int mult = std::atoi(t.substr(0, i).c_str());
    const char* p = std::strchr(kL, t[i]);
    if (!p || mult < 1) return false;
    const std::string rest = t.substr(i + 1);
    if (!(rest.empty() || rest == "*")) return false;
    L = static_cast<int>(p - kL); S2 = mult - 1;
    return true;
}

double measuredGAlkali(int Z, int n, int l, int J2) {
    for (const LandeRow& r : kLandeRows) if (r.Z == Z && r.n == n && r.l == l && r.j2 == J2 && r.conf[0] == 0) return r.g;
    return kNaN;
}
double measuredGLevel(int Z, const std::string& conf, const std::string& term, int J2) {
    for (const LandeRow& r : kLandeRows) if (r.Z == Z && r.n == 0 && r.j2 == J2 && conf == r.conf && term == r.term) return r.g;
    return kNaN;
}
std::string measuredGRef(int Z, int n, int l, int J2, const std::string& conf, const std::string& term) {
    for (const LandeRow& r : kLandeRows)
        if (r.Z == Z && r.j2 == J2 && (conf.empty() ? (r.n == n && r.l == l && r.conf[0] == 0) : (r.n == 0 && conf == r.conf && term == r.term))) return r.ref;
    return "";
}

Term termOneElectron(int Z, int N, int n, int l) {
    Term t;
    t.L = l; t.S2 = 1;
    t.name = orbName(n, l) + " " + termName(l, 1, l % 2 == 1);
    for (int J2 = std::abs(2 * l - 1); J2 <= 2 * l + 1; J2 += 2) { t.J2.push_back(J2); t.cm.push_back(diracQedEV(Z, N, n, l, J2) * cmPerEV()); }
    decide(t, {});
    return t;
}
Term termAlkali(int Z, int n, int l) {
    Term t;
    t.L = l; t.S2 = 1;
    t.name = orbName(n, l) + " " + termName(l, 1, l % 2 == 1);
    std::vector<AlkLevel> lv = alkaliLevels(Z);
    std::sort(lv.begin(), lv.end(), [](const AlkLevel& a, const AlkLevel& b) { return a.j2 < b.j2; });
    std::vector<double> g;
    for (const AlkLevel& a : lv) if (a.n == n && a.l == l) { t.J2.push_back(a.j2); t.cm.push_back(a.cm); g.push_back(measuredGAlkali(Z, n, l, a.j2)); }
    if (t.J2.empty()) { t.mode = Mode::None; t.why = "The lab has no level of " + t.name + "."; return t; }
    decide(t, g);
    return t;
}
Term termLevel(int Z, int level) {
    Term t;
    const std::vector<JLevel>& lv = jumpLevels(Z);
    if (level < 0 || level >= static_cast<int>(lv.size())) { t.mode = Mode::None; t.why = "No such level."; return t; }
    const JLevel& me = lv[std::size_t(level)];
    t.name = confPretty(me.conf) + " " + termPretty(me.term);
    if (!parseLS(me.term, t.L, t.S2)) {
        t.mode = Mode::None;
        t.why = "NIST names this level's term " + termPretty(me.term) + ", which is not an LS term (L and S are not good quantum numbers here), so the lab computes no splitting for it.";
        t.J2 = {me.j2}; t.cm = {me.cm};
        return t;
    }
    std::vector<std::pair<int, double>> js;
    for (const JLevel& a : lv) if (a.conf == me.conf && a.term == me.term) js.push_back({a.j2, a.cm});
    std::sort(js.begin(), js.end());
    std::vector<double> g;
    for (const auto& [j2, cm] : js) { t.J2.push_back(j2); t.cm.push_back(cm); g.push_back(measuredGLevel(Z, me.conf, me.term, j2)); }
    decide(t, g);
    return t;
}
double gapCm(const Term& t) {
    if (t.mode != Mode::Matrix || t.J2.size() < 2) return kNaN;
    double g = INFINITY;
    for (std::size_t k = 0; k + 1 < t.J2.size(); k++) g = std::min(g, std::fabs(t.cm[k + 1] - t.cm[k]));
    return g;
}

std::vector<Sub> sublevels(const Term& t, double B, double gs) {
    std::vector<Sub> out;
    if (t.mode == Mode::None || t.J2.empty()) return out;
    const double b = B * lorentzCmPerT();
    const std::size_t nJ = t.J2.size();
    const int Jmax = *std::max_element(t.J2.begin(), t.J2.end());
    double mean = 0;                                             // the matrix is built about the term's mean energy, so the field part keeps its digits
    for (const double e : t.cm) mean += e / static_cast<double>(nJ);
    for (int M2 = -Jmax; M2 <= Jmax; M2 += 2) {
        std::vector<std::size_t> idx;
        for (std::size_t k = 0; k < nJ; k++) if (t.J2[k] >= std::abs(M2)) idx.push_back(k);
        if (idx.empty()) continue;
        const std::size_t n = idx.size();
        std::vector<std::vector<double>> H(n, std::vector<double>(n, 0)), V;
        for (std::size_t i = 0; i < n; i++)
            for (std::size_t j = 0; j < n; j++) {
                const int Ja = t.J2[idx[i]], Jb = t.J2[idx[j]];
                if (t.mode == Mode::Linear) {   // its own g: NIST's measured one, else the LS value with this g_s
                    const double g = t.gMeasured[idx[i]] ? t.gLin[idx[i]] : landeLS(t.L, t.S2, Ja, gs);
                    if (i == j) H[i][i] = t.cm[idx[i]] - mean + b * g * (M2 / 2.0);
                    continue;
                }
                H[i][j] = b * (gs - 1) * szElement(t.L, t.S2, Ja, Jb, M2);
                if (i == j) H[i][i] += t.cm[idx[i]] - mean + b * (M2 / 2.0);
            }
        std::vector<double> w;
        jacobi(H, w, V);
        std::vector<std::size_t> byE(n), byJ(idx);
        for (std::size_t i = 0; i < n; i++) byE[i] = i;
        std::sort(byE.begin(), byE.end(), [&](std::size_t a, std::size_t c) { return w[a] < w[c]; });
        std::sort(byJ.begin(), byJ.end(), [&](std::size_t a, std::size_t c) { return t.cm[a] < t.cm[c]; });   // equal M never cross: the k-th lowest state joins the k-th lowest level
        for (std::size_t r = 0; r < n; r++) {
            const std::size_t e = byE[r];
            Sub s;
            s.J2 = t.J2[byJ[r]]; s.M2 = M2; s.rel = w[e]; s.mean = mean; s.cm = w[e] + mean;
            s.c.assign(nJ, 0);
            double sgn = 0;                                              // a fixed phase: the amplitude on its own level positive
            for (std::size_t i = 0; i < n; i++) if (idx[i] == byJ[r]) sgn = V[i][e] < 0 ? -1 : 1;
            for (std::size_t i = 0; i < n; i++) s.c[idx[i]] = sgn * V[i][e];
            s.u.assign(std::size_t(t.S2 + 1), 0);
            for (int MS2 = -t.S2; MS2 <= t.S2; MS2 += 2) {
                const int ML2 = M2 - MS2;
                if (std::abs(ML2) > 2 * t.L) continue;
                double a = 0;
                for (std::size_t k = 0; k < nJ; k++) if (s.c[k] != 0) a += s.c[k] * clebschGordan(2 * t.L, ML2, t.S2, MS2, t.J2[k], M2);
                s.u[std::size_t((MS2 + t.S2) / 2)] = a;
            }
            out.push_back(s);
        }
    }
    std::sort(out.begin(), out.end(), [](const Sub& a, const Sub& c) { return a.J2 != c.J2 ? a.J2 < c.J2 : a.M2 < c.M2; });
    return out;
}

bool lsRoute(const Line& L) {
    // both levels in LS terms of the same S (a matrix or not: an unmixed level's LS element is the same 3j pattern), and the orbital element allowed (L 0 -> 0 is not)
    return L.up.mode != Mode::None && L.lo.mode != Mode::None && L.up.S2 == L.lo.S2 && std::abs(L.up.L - L.lo.L) <= 1 && L.up.L + L.lo.L > 0;
}
namespace {
double fieldFreeOf(const Term& t, int J2) { for (std::size_t k = 0; k < t.J2.size(); k++) if (t.J2[k] == J2) return t.cm[k]; return kNaN; }
std::size_t indexOf(const Term& t, int J2) { for (std::size_t k = 0; k < t.J2.size(); k++) if (t.J2[k] == J2) return k; return t.J2.size(); }
// the dipole amplitude <u| d_q |l>, q = M_u - M_l
double amplitude(const Line& L, const Sub& u, const Sub& l) {
    const int q2 = u.M2 - l.M2;
    if (std::abs(q2) > 2) return 0;
    if (lsRoute(L)) {
        double a = 0;
        const int S2 = L.up.S2;
        for (int MS2 = -S2; MS2 <= S2; MS2 += 2) {
            const std::size_t k = std::size_t((MS2 + S2) / 2);
            if (u.u[k] == 0 || l.u[k] == 0) continue;
            const int MLu = u.M2 - MS2, MLl = l.M2 - MS2;
            const int ph = (2 * L.up.L - MLu) / 2;
            a += u.u[k] * l.u[k] * ((ph % 2) != 0 ? -1.0 : 1.0) * threeJ(2 * L.up.L, 2, 2 * L.lo.L, -MLu, q2, MLl);
        }
        return a;
    }
    const std::size_t iu = indexOf(L.up, L.upJ2), il = indexOf(L.lo, L.loJ2);
    if (iu >= u.c.size() || il >= l.c.size()) return 0;
    const int ph = (L.upJ2 - u.M2) / 2;
    return u.c[iu] * l.c[il] * ((ph % 2) != 0 ? -1.0 : 1.0) * threeJ(L.upJ2, 2, L.loJ2, -u.M2, q2, l.M2);
}
}  // namespace
std::vector<Component> rawComponents(const Line& L, double B, double gs) {
    std::vector<Component> out;
    const std::vector<Sub> su = sublevels(L.up, B, gs), sl = sublevels(L.lo, B, gs);
    const double eu = fieldFreeOf(L.up, L.upJ2), el = fieldFreeOf(L.lo, L.loJ2);
    for (const Sub& u : su) {
        if (u.J2 != L.upJ2) continue;
        for (const Sub& l : sl) {
            if (l.J2 != L.loJ2 || std::abs(u.M2 - l.M2) > 2) continue;
            const double a = amplitude(L, u, l);
            Component c;
            c.M2u = u.M2; c.M2l = l.M2; c.q = (u.M2 - l.M2) / 2;
            c.shiftCm = (u.rel - (eu - u.mean)) - (l.rel - (el - l.mean));   // (each level's own shift, then their difference: no large energies subtracted)
            c.raw = a * a;
            out.push_back(c);
        }
    }
    return out;
}

double fieldFreeRaw(const Line& L) {
    double s = 0;
    for (const Component& c : rawComponents(L, 0, gsCodata())) s += c.raw;
    return s;
}
std::vector<Component> components(const Line& L, double B, double gs) {
    std::vector<Component> all = rawComponents(L, B, gs), out;
    double s0 = 0;
    for (const Component& c : rawComponents(L, 0, gs)) s0 += c.raw;
    if (!(s0 > 0)) return out;
    for (Component& c : all) { c.strength = c.raw / s0; if (c.strength > 1e-12) out.push_back(c); }
    std::stable_sort(out.begin(), out.end(), [](const Component& a, const Component& b) { return a.shiftCm < b.shiftCm; });
    return out;
}
double polWeight(int q, double th) { return q == 0 ? std::sin(th) * std::sin(th) : (1 + std::cos(th) * std::cos(th)) / 2; }
double seenStrength(const Component& c, bool along) { return c.strength * polWeight(c.q, along ? 0.0 : kPi / 2); }
double shiftNm(const Line& L, double shiftCm) { return 1e7 / (L.cm0 + shiftCm) - 1e7 / L.cm0; }

namespace {
std::vector<LineRef> makeLines(int Z, int N, int kind) {
    std::vector<LineRef> out;
    const auto lineName = [](const std::string& up, const std::string& lo, double nm) { return up + " → " + lo + "  " + fmt("%.3f", nm) + " nm"; };
    if (kind == 2) {
        const std::vector<JLine>& ln = jumpLines(Z);
        for (int k = 0; k < static_cast<int>(ln.size()); k++) {
            if (!jumpOffered(Z, k)) continue;
            const JLine& q = ln[std::size_t(k)];
            LineRef r;
            r.a = q.hi; r.b = q.lo;
            r.line.up = termLevel(Z, q.hi); r.line.lo = termLevel(Z, q.lo);
            r.line.upJ2 = jumpLevels(Z)[std::size_t(q.hi)].j2; r.line.loJ2 = jumpLevels(Z)[std::size_t(q.lo)].j2;
            r.line.nm0 = q.nm(); r.line.cm0 = 1e7 / q.nm();
            r.line.name = lineName(levelName(Z, q.hi), levelName(Z, q.lo), q.nm());
            out.push_back(r);
        }
    } else if (kind == 1) {
        const std::vector<AlkLine> ln = alkaliLines(Z);
        for (int k = 0; k < static_cast<int>(ln.size()); k++) {
            const AlkLine& q = ln[std::size_t(k)];
            if (!std::isfinite(q.nm())) continue;
            LineRef r;
            r.a = k;
            r.line.up = termAlkali(Z, q.nHi, q.lHi); r.line.lo = termAlkali(Z, q.nLo, q.lLo);
            r.line.upJ2 = q.j2Hi; r.line.loJ2 = q.j2Lo;
            r.line.nm0 = q.nm(); r.line.cm0 = 1e7 / q.nm();
            r.line.name = lineName(orbName(q.nHi, q.lHi) + " " + termName(q.lHi, 1, q.lHi % 2 == 1) + jText(q.j2Hi), orbName(q.nLo, q.lLo) + " " + termName(q.lLo, 1, q.lLo % 2 == 1) + jText(q.j2Lo), q.nm());
            out.push_back(r);
        }
    } else {
        for (int nu = 2; nu <= 5; nu++)
            for (int nl = 1; nl < nu; nl++)
                for (int lu = 0; lu < nu; lu++)
                    for (int ll = std::max(0, lu - 1); ll <= lu + 1 && ll < nl; ll++) {
                        if (std::abs(lu - ll) != 1) continue;
                        for (int ju = std::abs(2 * lu - 1); ju <= 2 * lu + 1; ju += 2)
                            for (int jl = std::abs(2 * ll - 1); jl <= 2 * ll + 1; jl += 2) {
                                if (!diracAllowed(ll, jl, lu, ju)) continue;
                                LineRef r;
                                r.a = nu; r.b = nl;
                                r.line.up = termOneElectron(Z, N, nu, lu); r.line.lo = termOneElectron(Z, N, nl, ll);
                                r.line.upJ2 = ju; r.line.loJ2 = jl;
                                r.line.cm0 = fieldFreeOf(r.line.up, ju) - fieldFreeOf(r.line.lo, jl);
                                r.line.nm0 = 1e7 / r.line.cm0;
                                r.line.name = lineName(orbName(nu, lu) + " " + termName(lu, 1, lu % 2 == 1) + jText(ju), orbName(nl, ll) + " " + termName(ll, 1, ll % 2 == 1) + jText(jl), r.line.nm0);
                                out.push_back(r);
                            }
                    }
    }
    return out;
}
}  // namespace
const std::vector<LineRef>& labLines(int Z, int N, int kind) {   // made once per atom, isotope and kind (the panel asks every frame)
    static std::mutex mu;
    static std::map<std::tuple<int, int, int>, std::vector<LineRef>> cache;
    const std::lock_guard<std::mutex> lock(mu);
    const auto key = std::make_tuple(Z, N, kind);
    auto it = cache.find(key);
    if (it == cache.end()) it = cache.emplace(key, makeLines(Z, N, kind)).first;
    return it->second;
}
int defaultLine(int Z, int N, int kind, int levelNow, int nNow, int lNow) {
    const std::vector<LineRef>& v = labLines(Z, N, kind);
    if (v.empty()) return -1;
    int best = 0;
    double bestW = -1;
    for (int i = 0; i < static_cast<int>(v.size()); i++) {
        const LineRef& r = v[std::size_t(i)];
        double w = -1;
        if (kind == 2) {
            if (r.a != levelNow && r.b != levelNow) continue;
            for (const int k : linesBetween(Z, r.a, r.b)) { const JLine& q = jumpLines(Z)[std::size_t(k)]; if (std::isfinite(q.rate())) w = std::max(w, (r.line.upJ2 + 1) * q.rate()); }
            if (w < 0) w = 0;
        } else if (kind == 1) {
            const AlkLine q = alkaliLines(Z)[std::size_t(r.a)];
            if (!((q.nHi == nNow && q.lHi == lNow) || (q.nLo == nNow && q.lLo == lNow))) continue;
            w = std::isfinite(q.aki) ? (q.j2Hi + 1) * q.aki : 0;
        } else {
            const bool touches = (r.a == nNow && (lNow < 0 || r.line.up.L == lNow)) || (r.b == nNow && (lNow < 0 || r.line.lo.L == lNow));
            if (!touches) continue;
            w = 1.0 / (r.a * 10 + r.b) + (r.line.upJ2 + 1) * 1e-3;   // the lowest line from or to the shell now (Lyman alpha from 2p), the higher J first
        }
        if (w > bestW) { bestW = w; best = i; }
    }
    return best;
}

Regime regime(const Line& L, double B, double& muBcm, double& gap) {
    muBcm = B * lorentzCmPerT();
    const double a = gapCm(L.up), b = gapCm(L.lo);
    gap = std::isfinite(a) && std::isfinite(b) ? std::min(a, b) : std::isfinite(a) ? a : b;
    if (!std::isfinite(gap)) return Regime::NoFine;
    const double x = muBcm / gap;
    return x < 0.1 ? Regime::Weak : x > 10 ? Regime::Strong : Regime::Between;
}
std::string regimeText(const Line& L, double B) {
    double mu = 0, gap = 0;
    const Regime r = regime(L, B, mu, gap);
    const std::string m = "μB B / hc = " + num3(mu) + " cm⁻¹";
    if (r == Regime::NoFine) return m + "; neither level has a fine-structure partner here, so each splits in proportion to the field at every strength (no Paschen-Back change).";
    const std::string g = "the smaller fine-structure gap of the two terms, " + num3(gap) + " cm⁻¹";
    if (r == Regime::Weak) return "Weak field (anomalous Zeeman effect, Landé g): " + m + " is well below " + g + ".";
    if (r == Regime::Strong) return "Strong field (Paschen-Back): " + m + " is far above " + g + "; L and S align with the field on their own.";
    return "Between the weak and the strong field: " + m + " is comparable to " + g + ", so the levels of equal M mix and bend.";
}
std::string usedText(const Line& L) {
    const auto one = [](const Term& t, int J2) {
        if (t.mode == Mode::None) return t.why;
        if (t.mode == Mode::Matrix) return t.name + ": all its J levels in one matrix (Landé g " + fmt("%.5f", landeLS(t.L, t.S2, J2, gsCodata())) + " in weak fields).";
        const std::size_t k = indexOf(t, J2);
        if (J2 == 0) return t.why + " J = 0: one state, not shifted.";
        const double g = k < t.gLin.size() ? t.gLin[k] : kNaN;
        return t.why + " g = " + fmt("%.5f", g) + (k < t.gMeasured.size() && t.gMeasured[k] ? " (measured, NIST ASD)." : " (LS value; NIST lists no measured g).");
    };
    std::string s = "Upper: " + one(L.up, L.upJ2) + " Lower: " + one(L.lo, L.loJ2);
    if (!lsRoute(L)) s += " Strengths: the 3j pattern of the line's own two levels.";
    return s;
}
double hyperfineFieldT(int Z, int N, int kind) {
    if (kind != 0) return kNaN;
    const double dE = hfs::splittingEV(Z, N, 1, 0, 1);
    if (!(dE > 0)) return kNaN;
    return dE / (kMuBJT / kEV);
}
std::string limitsText(int Z, int N, int kind) {
    std::string s = "Limits: one LS term at a time (no mixing with other terms; in heavy atoms such as mercury the terms are mixed, hence the gap between the LS g and the measured g); "
                    "no diamagnetic (B²) term; the nucleus's own magnetic moment left out; no hyperfine structure: ";
    const double Bh = hyperfineFieldT(Z, N, kind);
    if (std::isfinite(Bh)) {
        const double dE = hfs::splittingEV(Z, N, 1, 0, 1);
        s += "below about " + num3(Bh) + " T (where μB B equals the lab's ground-level hyperfine splitting, " + num3(dE / kPlanckEVs / 1e9) + " GHz) the real pattern is different.";
    } else
        s += "where μB B is below the hyperfine splitting of a level the real pattern is different (the lab has no hyperfine numbers for this atom's levels; for an isotope with nuclear spin 0 there is none).";
    return s;
}
std::string mText(int M2) {
    if (M2 == 0) return "0";
    const std::string a = M2 % 2 ? std::to_string(std::abs(M2)) + "/2" : std::to_string(std::abs(M2) / 2);
    return (M2 < 0 ? "−" : "+") + a;
}
std::string polName(int q) { return q == 0 ? "π" : q > 0 ? "σ+" : "σ−"; }

}  // namespace atom::zeeman
