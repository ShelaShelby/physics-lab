// 0.54.2: small matrices in the calculator (up to 4x4, the Pauli matrices, Hermitian eigenvalues). Included by test_calc.cpp.
// Pure mathematics; independent references only, all written here:
//   - the Pauli matrices and their algebra (σa² = 1, σa σb = i σc in cyclic order), typed out below;
//   - the two-level closed form: [[Δ/2, Ω/2], [Ω*/2, −Δ/2]] has the eigenvalues ±√(Δ² + |Ω|²)/2;
//   - spin 1 from the ladder formula <m±1|J±|m> = √(j(j+1) − m(m±1)), Jx = (J+ + J−)/2, Jy = (J+ − J−)/2i (eigenvalues −1, 0, 1; J² = j(j+1) = 2);
//   - two spins ½: σ·σ = 2 S² − 3 has the eigenvalues −3 (singlet, S = 0) and 1 three times (triplet, S = 1);
//   - identities: Σ eig = tr, Π eig = det, eig(M + c) = eig(M) + c, det(AB) = det A det B, (AB)† = B†A†, kron's mixed-product rule;
//   - the 3x3 and 4x4 determinants by the cofactor expansion written out below (the app uses its own code);
//   - Hermitian matrices with KNOWN eigenvalues: a diagonal matrix turned by this file's own plane rotations (a unitary change keeps the eigenvalues).
#include <algorithm>
#include <complex>
#include <utility>

namespace mt {
using cd = std::complex<double>;
struct TM {
    int r = 0, c = 0;
    std::vector<cd> a;
    bool ok = false;
    std::string err, unit, number;
    cd& at(int i, int j) { return a[static_cast<std::size_t>(i * c + j)]; }
    const cd& at(int i, int j) const { return a[static_cast<std::size_t>(i * c + j)]; }
};
static TM make(int r, int c, std::vector<cd> a) { TM m; m.r = r; m.c = c; m.a = std::move(a); m.ok = true; return m; }
static TM ident(int n) { TM m = make(n, n, std::vector<cd>(static_cast<std::size_t>(n * n))); for (int i = 0; i < n; i++) m.at(i, i) = 1; return m; }
static TM tmul(const TM& a, const TM& b) {
    TM m = make(a.r, b.c, std::vector<cd>(static_cast<std::size_t>(a.r * b.c)));
    for (int i = 0; i < a.r; i++) for (int j = 0; j < b.c; j++) { cd s = 0; for (int k = 0; k < a.c; k++) s += a.at(i, k) * b.at(k, j); m.at(i, j) = s; }
    return m;
}
static TM tdag(const TM& a) { TM m = make(a.c, a.r, std::vector<cd>(a.a.size())); for (int i = 0; i < a.r; i++) for (int j = 0; j < a.c; j++) m.at(j, i) = std::conj(a.at(i, j)); return m; }
static TM tadd(const TM& a, const TM& b, cd fb = 1) { TM m = a; for (std::size_t k = 0; k < m.a.size(); k++) m.a[k] += fb * b.a[k]; return m; }
static TM tscale(const TM& a, cd f) { TM m = a; for (cd& z : m.a) z *= f; return m; }
static TM tkron(const TM& a, const TM& b) {
    TM m = make(a.r * b.r, a.c * b.c, std::vector<cd>(a.a.size() * b.a.size()));
    for (int i = 0; i < a.r; i++) for (int j = 0; j < a.c; j++) for (int k = 0; k < b.r; k++) for (int l = 0; l < b.c; l++) m.at(i * b.r + k, j * b.c + l) = a.at(i, j) * b.at(k, l);
    return m;
}
static cd ttr(const TM& a) { cd s = 0; for (int i = 0; i < a.r; i++) s += a.at(i, i); return s; }
// the cofactor expansions, written out
static cd det2(cd a, cd b, cd c, cd d) { return a * d - b * c; }
static cd det3(const TM& m) {
    return m.at(0, 0) * det2(m.at(1, 1), m.at(1, 2), m.at(2, 1), m.at(2, 2)) - m.at(0, 1) * det2(m.at(1, 0), m.at(1, 2), m.at(2, 0), m.at(2, 2)) +
           m.at(0, 2) * det2(m.at(1, 0), m.at(1, 1), m.at(2, 0), m.at(2, 1));
}
static TM minor4(const TM& m, int col) {       // the 4x4 without its first row and the column col
    TM s = make(3, 3, std::vector<cd>(9));
    for (int i = 1; i < 4; i++) { int jj = 0; for (int j = 0; j < 4; j++) if (j != col) s.at(i - 1, jj++) = m.at(i, j); }
    return s;
}
static cd det4(const TM& m) { return m.at(0, 0) * det3(minor4(m, 0)) - m.at(0, 1) * det3(minor4(m, 1)) + m.at(0, 2) * det3(minor4(m, 2)) - m.at(0, 3) * det3(minor4(m, 3)); }
static cd tdet(const TM& m) { return m.r == 1 ? m.at(0, 0) : m.r == 2 ? det2(m.at(0, 0), m.at(0, 1), m.at(1, 0), m.at(1, 1)) : m.r == 3 ? det3(m) : det4(m); }
static double tmax(const TM& m) { double x = 0; for (const cd& z : m.a) x = std::max(x, std::abs(z)); return x; }

// a number as text that reads back as the same double
static std::string n17(double v) { char b[48]; std::snprintf(b, sizeof b, "%.17g", v); return v < 0 ? "(" + std::string(b) + ")" : std::string(b); }
static std::string c17(cd z) { return z.imag() == 0 ? n17(z.real()) : "(" + n17(z.real()) + " + " + n17(z.imag()) + "*i)"; }
static std::string lit(const TM& m) {
    std::string s = "[";
    for (int i = 0; i < m.r; i++) { s += i ? ", [" : "["; for (int j = 0; j < m.c; j++) s += (j ? ", " : "") + c17(m.at(i, j)); s += "]"; }
    return s + "]";
}
// what the calculator answers: a matrix (its SI values), or a number as 1x1 with scalar set
static TM get(const std::string& in, const calc::Vars* vars = nullptr, const calc::Quantity* ans = nullptr) {
    const calc::Result r = calc::evaluate(in, ans, vars);
    TM m;
    m.ok = r.ok; m.err = r.error; m.unit = r.unit; m.number = r.number;
    if (!r.ok) return m;
    if (r.q.mat) { m.r = r.q.mat->rows; m.c = r.q.mat->cols; for (int k = 0; k < m.r * m.c; k++) m.a.push_back(r.q.mat->a[static_cast<std::size_t>(k)]); }
    else { m.r = m.c = 1; m.a = {cd(r.q.v, r.q.im)}; }
    return m;
}
static double gap(const TM& a, const TM& b) {     // the largest entry difference; 1e300 when the shapes differ or one failed
    if (!a.ok || !b.ok || a.r != b.r || a.c != b.c) return 1e300;
    double x = 0;
    for (std::size_t k = 0; k < a.a.size(); k++) x = std::max(x, std::abs(a.a[k] - b.a[k]));
    return x;
}
static void same(const std::string& in, const TM& want, double tol = 0) {
    const TM g = get(in);
    const double d = gap(g, want);
    check("matrix: \"" + in + "\"" + (g.ok ? " (got " + g.number + ", off by " + calc::formatNumber(d, 3) + ")" : " (error: " + g.err + ")"), d <= tol * std::max(1.0, tmax(want)));
}
static void refused(const std::string& in, const std::string& part) {
    const calc::Result r = calc::evaluate(in);
    check("matrix: \"" + in + "\" is refused with \"" + part + "\"" + (r.ok ? " (but it gave " + r.number + ")" : " (said: " + r.error + ")"),
          !r.ok && !r.empty && r.error.find(part) != std::string::npos);
}
static TM col(std::vector<double> v) { std::vector<cd> a(v.begin(), v.end()); return make(static_cast<int>(a.size()), 1, a); }
static unsigned g_seed = 20261114u;
static double rnd() { g_seed = g_seed * 1664525u + 1013904223u; return (double(g_seed >> 8) / 16777216.0) * 2 - 1; }   // -1 .. 1
// J† M J for the plane rotation J in (p, q): J_pp = J_qq = cos t, J_pq = sin t e^(i f), J_qp = −sin t e^(−i f) (unitary)
static TM turned(const TM& m, int p, int q, double t, double f) {
    TM J = ident(m.r);
    J.at(p, p) = J.at(q, q) = std::cos(t);
    J.at(p, q) = std::sin(t) * std::exp(cd(0, f));
    J.at(q, p) = -std::sin(t) * std::exp(cd(0, -f));
    return tmul(tdag(J), tmul(m, J));
}
static TM hermitianWith(const std::vector<double>& eigs) {
    const int n = static_cast<int>(eigs.size());
    TM m = make(n, n, std::vector<cd>(static_cast<std::size_t>(n * n)));
    for (int i = 0; i < n; i++) m.at(i, i) = eigs[static_cast<std::size_t>(i)];
    for (int round = 0; round < 3; round++) for (int p = 0; p < n; p++) for (int q = p + 1; q < n; q++) m = turned(m, p, q, 3.0 * rnd(), 3.0 * rnd());
    return tscale(tadd(m, tdag(m)), 0.5);       // exactly Hermitian (the rounding of the rotations taken out)
}

static double g_worstEig = 0;                    // the largest eigenvalue error found, relative to the largest entry
static void eigIs(const std::string& what, const std::string& in, std::vector<double> want, double tol, double scale = 0) {
    const TM g = get(in);
    std::sort(want.begin(), want.end());
    double s = scale;
    for (double w : want) s = std::max(s, std::fabs(w));
    if (s == 0) s = 1;
    double d = 1e300;
    if (g.ok && g.r == static_cast<int>(want.size()) && g.c == 1) {
        d = 0;
        for (std::size_t k = 0; k < want.size(); k++) d = std::max(d, std::abs(g.a[k] - want[k]) / s);
        g_worstEig = std::max(g_worstEig, d);
        for (std::size_t k = 0; k + 1 < want.size(); k++) if (g.a[k].real() > g.a[k + 1].real() || g.a[k].imag() != 0) d = 1e300;   // real, smallest first
    }
    check("eig: " + what + (g.ok ? " (got " + g.number + ", off by " + calc::formatNumber(d, 3) + " of the largest)" : " (error: " + g.err + ")"), d <= tol);
}

static void run() {
    using calc::evaluate;
    using calc::Result;
    const cd I(0, 1);
    const TM sx = make(2, 2, {0, 1, 1, 0}), sy = make(2, 2, {0, -I, I, 0}), sz = make(2, 2, {1, 0, 0, -1}), one = ident(2);
    const double eV = calc::findSymbol("eV")->q.v;

    // ---- typing a matrix: sizes 1x1 to 4x4, rows and columns
    for (int r = 1; r <= 4; r++) for (int c = 1; c <= 4; c++) {
        TM m = make(r, c, std::vector<cd>(static_cast<std::size_t>(r * c)));
        for (int k = 0; k < r * c; k++) m.a[static_cast<std::size_t>(k)] = k + 1;
        const Result res = evaluate(lit(m));
        check("typing: a " + std::to_string(r) + "x" + std::to_string(c) + " matrix is read" + (res.ok ? "" : " (error: " + res.error + ")"),
              res.ok && res.rows == r && res.cols == c && static_cast<int>(res.cells.size()) == r * c && gap(get(lit(m)), m) == 0);
    }
    same("[1, 2, 3]", make(1, 3, {1, 2, 3}));                                       // one pair of brackets: a row
    same("[[1 + 2i, sqrt(4)], [2^3, -i]]", make(2, 2, {cd(1, 2), 2, 8, -I}));       // entries are expressions
    refused("[[1,2,3,4,5],[1,2,3,4,5],[1,2,3,4,5],[1,2,3,4,5],[1,2,3,4,5]]", "at most 4");
    refused("[[1, 2, 3, 4, 5]]", "at most 4");
    refused("[[1], [2], [3], [4], [5]]", "at most 4");
    refused("[[1, 2], [3]]", "same number");
    refused("[[1, 2], 3]", "its own brackets");
    refused("[[1, 2], [3, 4]", "]");
    refused("[]", "Empty");
    refused("[[1, 2], [3, 4]]]", "]");
    refused("[[[1, 2]], [[3, 4]]]", "can't hold");
    refused("1, 2", "comma");
    {
        const Result r = evaluate("sigmax * sigmay");
        check("shown: σx σy is the grid [[i, 0], [0, -i]] (" + r.number + ")", r.ok && r.rows == 2 && r.cols == 2 && !r.list && r.number == "[[i, 0], [0, -i]]" && r.unit.empty() &&
              r.cells.size() == 4 && r.cells[0] == I && r.cells[1] == 0.0 && r.cells[2] == 0.0 && r.cells[3] == -I);
        const Result e = evaluate("eig(sigmaz)");
        check("shown: eig(σz) is the list -1, 1 (" + e.number + ")", e.ok && e.list && e.rows == 2 && e.cols == 1 && e.number == "-1, 1");
        const Result h = evaluate("[[1, 2], [3, 4]] eV");
        check("shown: a matrix's read-as line and its one unit (" + h.readAs + ", " + h.unit + ")", h.ok && h.readAs == "[[1, 2], [3, 4]] eV" && h.unit == "J" && h.kindWord == "energy");
    }

    // ---- units: one for the whole matrix
    same("[[1, 2], [3, 4]] eV", make(2, 2, {1 * eV, 2 * eV, 3 * eV, 4 * eV}), 1e-15);
    same("[[1 eV, 500 meV], [2 keV, 4 eV]]", make(2, 2, {eV, 500 * (1e-3 * eV), 2 * (1e3 * eV), 4 * eV}), 1e-15);
    {
        const Result r = evaluate("[[1 eV, 500 meV]] to eV");
        check("units: eV and meV entries mix, shown in eV as 1 and 0.5 (" + r.number + " " + r.unit + ")",
              r.ok && r.unit == "eV" && r.cells.size() == 2 && rel(r.cells[0].real(), 1, 1e-15) && rel(r.cells[1].real(), 0.5, 1e-15) && r.number == "[[1, 0.5]]");
        const Result k = evaluate("[[1, 2], [3, 4]] km to m");
        check("units: a whole matrix converts (km to m)", k.ok && k.cells.size() == 4 && k.cells[3].real() == 4000 && k.unit == "m");
    }
    refused("[[1 eV, 2 m]]", "outside");
    refused("[[1 eV, 2]]", "outside");
    refused("[[1, 2], [3, 4]] eV + [[1, 2], [3, 4]] m", "different kinds");
    refused("[[1, 2], [3, 4]] eV to cm^-1", "same kind");
    refused("[[1, 2], [3, 4]] to [[1, 2], [3, 4]]", "After \"to\"");
    refused("[[1, 2], [3, 4]] to sigmax", "matrix");
    refused("5 to sigmax", "matrix");

    // ---- the Pauli algebra
    const char* const typed[3] = {"sigmax", "sigmay", "sigmaz"};
    const char* const greek[3] = {"σx", "σy", "σz"};
    const TM pauli[3] = {sx, sy, sz};
    for (int a = 0; a < 3; a++) {
        const std::string t = typed[a], g = greek[a];
        same(t, pauli[a]);
        same(g, pauli[a]);
        same(t + " * " + t, one);
        same(g + "^2", one);
        same(g + "²", one);
        same("dagger(" + t + ")", pauli[a]);
        same("tr(" + t + ")", make(1, 1, {0}));
        same("det(" + t + ")", make(1, 1, {-1}));
        eigIs(t + " has the eigenvalues -1 and 1", "eig(" + t + ")", {-1, 1}, 0);
        const int b = (a + 1) % 3, c = (a + 2) % 3;
        same(t + " * " + typed[b], tscale(pauli[c], I));                               // cyclic: σa σb = i σc
        same(std::string(typed[b]) + " * " + t, tscale(pauli[c], -I));                 // reversed: −i σc
        same(g + " " + greek[b], tscale(pauli[c], I));                                 // written next to each other
        same(t + " * " + typed[b] + " + " + typed[b] + " * " + t, make(2, 2, {0, 0, 0, 0}));   // they anticommute
    }
    same("sigmax * sigmay * sigmaz", tscale(one, I));
    same("conj(sigmay)", tscale(sy, -1));
    same("transpose(sigmay)", tscale(sy, -1));
    same("sigmax(sigmay)", tscale(sz, I));                                             // a bracket next to a matrix multiplies, like a number

    // ---- operations
    const TM A = make(2, 2, {1, 2, 3, 4}), B = make(2, 2, {cd(0, 1), 5, -2, cd(1, -1)});
    const std::string sA = "[[1, 2], [3, 4]]", sB = "[[i, 5], [-2, 1 - i]]";
    same(sA + " + " + sB, tadd(A, B));
    same(sA + " - " + sB, tadd(A, B, -1));
    same(sA + " * " + sB, tmul(A, B));
    same(sB + " * " + sA, tmul(B, A));
    same("-" + sB, tscale(B, -1));
    same("2 " + sA, tscale(A, 2));
    same("(1 + i) * " + sA, tscale(A, cd(1, 1)));
    same(sA + " * 3", tscale(A, 3));
    same(sA + " / 4", tscale(A, 0.25));
    same(sB + " / (2i)", tscale(B, 1.0 / cd(0, 2)), 1e-15);
    same(sA + "^0", one);
    same(sA + "^1", A);
    same(sA + "^3", tmul(A, tmul(A, A)));
    same(sB + "^4", tmul(tmul(B, B), tmul(B, B)), 1e-15);
    same("identity(3)", ident(3));
    same("identity(1)", ident(1));
    same(sA + " * [[5], [6]]", tmul(A, col({5, 6})));                                  // M v
    same("[[5, 6]] * " + sA, tmul(make(1, 2, {5, 6}), A));                             // a row times M
    same("[[1, 2, 3]] * [[4], [5], [6]]", make(1, 1, {32}));                           // row times column: 1x1
    same("[[4], [5], [6]] * [[1, 2, 3]]", tmul(col({4, 5, 6}), make(1, 3, {1, 2, 3})));
    same("transpose([[1, 2, 3], [4, 5, 6]])", make(3, 2, {1, 4, 2, 5, 3, 6}));
    same("transpose(transpose(" + sB + "))", B);
    same("dagger(" + sB + ")", tdag(B));
    same("dagger(dagger(" + sB + "))", B);
    same("dagger(" + sA + " * " + sB + ")", tmul(tdag(B), tdag(A)));                   // (AB)† = B†A†, here against this file's product
    same("dagger(" + sA + " * " + sB + ") - dagger(" + sB + ") * dagger(" + sA + ")", make(2, 2, {0, 0, 0, 0}));
    same("tr(" + sB + ")", make(1, 1, {cd(1, 0)}));
    same("det(" + sB + ")", make(1, 1, {det2(B.at(0, 0), B.at(0, 1), B.at(1, 0), B.at(1, 1))}));
    same("det([[7]])", make(1, 1, {7}));
    same("det(" + sA + " * " + sB + ") - det(" + sA + ") * det(" + sB + ")", make(1, 1, {0}), 1e-14);
    same("kron(" + sA + ", " + sB + ")", tkron(A, B));
    same("kron([[1, 2]], [[3], [4]])", tkron(make(1, 2, {1, 2}), col({3, 4})));
    same("kron(sigmax, sigmaz)", tkron(sx, sz));
    same("kron(sigmay, sigmay)", tkron(sy, sy));
    // the mixed-product rule: kron(A, B) kron(C, D) = kron(A C, B D)
    same("kron(" + sA + ", " + sB + ") * kron(sigmay, " + sA + ") - kron(" + sA + " * sigmay, " + sB + " * " + sA + ")", make(4, 4, std::vector<cd>(16)), 1e-14);
    same("kron(" + sA + ", " + sB + ") * kron(sigmay, " + sA + ")", tkron(tmul(A, sy), tmul(B, A)), 1e-14);
    {   // units through the operations: eV × eV, det of a 2x2 in eV is eV², tr stays eV
        const Result d = evaluate("det([[1, 2], [3, 4]] eV)"), t = evaluate("tr([[1, 2], [3, 4]] eV) to eV"), p = evaluate("[[1, 2], [3, 4]] eV * [[1], [1]] / (2 eV)");
        check("units: det of a 2x2 in eV is -2 eV²", d.ok && rel(d.q.v, -2 * eV * eV, 1e-14) && d.q.dim == calc::dimOf(2, 4, -4));
        check("units: tr stays an energy (5 eV)", t.ok && rel(t.inUnit, 5, 1e-14) && t.unit == "eV");
        check("units: (M eV) v / (2 eV) is a plain column 1.5, 3.5", p.ok && p.unit.empty() && p.cells.size() == 2 && rel(p.cells[0].real(), 1.5, 1e-14) && rel(p.cells[1].real(), 3.5, 1e-14));
    }

    // ---- determinants against the cofactor expansion written out above
    const TM D3 = make(3, 3, {2, -1, 0.5, cd(1, 1), 3, -2, 4, cd(0, -2), 1.5});
    const TM D4 = make(4, 4, {1, 2, -1, 0.5, cd(0, 1), 3, 2, -2, 4, -1.5, cd(1, -1), 1, 0.25, 2, -3, cd(2, 0.5)});
    const TM R3 = make(3, 3, {2, 0, 1, 1, 3, 2, 1, 1, 1}), R4 = make(4, 4, {1, 2, 3, 4, 5, 6, 7, 8, 2, 6, 4, 8, 3, 1, 1, 2});
    same("det(" + lit(D3) + ")", make(1, 1, {det3(D3)}), 1e-15);
    same("det(" + lit(D4) + ")", make(1, 1, {det4(D4)}), 1e-15);
    same("det(" + lit(R3) + ")", make(1, 1, {det3(R3)}));          // whole numbers: exact (2·(3−2) − 0 + 1·(1−3) = 0)
    same("det(" + lit(R4) + ")", make(1, 1, {det4(R4)}));
    same("det(" + lit(D4) + " * " + lit(R4) + ") - det(" + lit(D4) + ") * det(" + lit(R4) + ")", make(1, 1, {0}), 1e-12 * std::abs(det4(D4) * det4(R4)));
    same("det(identity(4))", make(1, 1, {1}));
    same("tr(" + lit(D4) + ")", make(1, 1, {ttr(D4)}), 1e-15);
    same("transpose(transpose(" + lit(D4) + "))", D4);
    same("dagger(" + lit(D3) + " * " + lit(R3) + ")", tmul(tdag(R3), tdag(D3)), 1e-15);

    // ---- the two-level system: ±√(Δ² + Ω²)/2, in eV
    const double dl[] = {0, 1, 0, 0.3, -2, 5, 1e-3, 7}, om[] = {0, 0, 1, 0.4, 1.5, 12, 1, 1e-4};
    for (int k = 0; k < 8; k++) {
        const double e = std::sqrt(dl[k] * dl[k] + om[k] * om[k]) / 2 * eV;
        const std::string d = n17(dl[k]), o = n17(om[k]);
        eigIs("two-level Δ = " + d + ", Ω = " + o + " eV", "eig([[" + d + "/2, " + o + "/2], [" + o + "/2, -" + d + "/2]] eV)", {-e, e}, 4e-16);
        for (double phi : {0.7, 2.0, -1.1, 3.14159}) {            // a complex coupling Ω e^(iφ) with its conjugate: the same eigenvalues for every φ
            const std::string p = n17(phi);
            eigIs("two-level Δ = " + d + ", Ω = " + o + " e^(i " + p + ") eV",
                  "eig([[" + d + "/2, " + o + "/2 * exp(i * " + p + ")], [" + o + "/2 * exp(-i * " + p + "), -" + d + "/2]] eV)", {-e, e}, 4e-16);
        }
    }
    {
        const Result r = evaluate("eig([[0.3/2, 0.4/2], [0.4/2, -0.3/2]] eV) to eV");
        check("two-level: shown in eV as -0.25, 0.25 (" + r.number + " " + r.unit + ")", r.ok && r.list && r.unit == "eV" && r.number == "-0.25, 0.25" && r.kindWord == "energy");
    }

    // ---- spin 1 from the ladder formula, basis m = 1, 0, -1
    {
        const double j = 1;
        TM Jp = make(3, 3, std::vector<cd>(9)), Jz = Jp;
        for (int k = 0; k < 3; k++) {
            const double m = 1 - k;
            Jz.at(k, k) = m;
            if (k > 0) Jp.at(k - 1, k) = std::sqrt(j * (j + 1) - m * (m + 1));        // <m+1|J+|m>
        }
        const TM Jm = tdag(Jp), Jx = tscale(tadd(Jp, Jm), 0.5), Jy = tscale(tadd(Jp, Jm, -1), 1.0 / cd(0, 2));
        const std::string x = lit(Jx), y = lit(Jy), z = lit(Jz);
        eigIs("spin 1: Jx has -1, 0, 1", "eig(" + x + ")", {-1, 0, 1}, 1e-15);
        eigIs("spin 1: Jy has -1, 0, 1", "eig(" + y + ")", {-1, 0, 1}, 1e-15);
        eigIs("spin 1: Jz has -1, 0, 1", "eig(" + z + ")", {-1, 0, 1}, 0);
        same(x + "^2 + " + y + "^2 + " + z + "^2", tscale(ident(3), 2), 1e-15);        // J² = j(j+1) = 2
        same(x + " * " + y + " - " + y + " * " + x, tscale(Jz, I), 1e-15);             // [Jx, Jy] = i Jz
        same("dagger(" + y + ")", Jy);
    }

    // ---- two spins: σ·σ has -3 (singlet) and 1, 1, 1 (triplet)
    eigIs("two spins: σ·σ has -3, 1, 1, 1", "eig(kron(sigmax, sigmax) + kron(sigmay, sigmay) + kron(sigmaz, sigmaz))", {-3, 1, 1, 1}, 1e-15);
    eigIs("two spins, in eV: (J/4) σ·σ with J = 2 eV has -1.5 eV and 0.5 eV three times",
          "eig(2 eV / 4 * (kron(σx, σx) + kron(σy, σy) + kron(σz, σz)))", {-1.5 * eV, 0.5 * eV, 0.5 * eV, 0.5 * eV}, 1e-15);

    // ---- eigenvalues: diagonal, degenerate, shifted
    eigIs("a diagonal matrix: its diagonal, sorted", "eig([[3, 0, 0, 0], [0, -1, 0, 0], [0, 0, 2.5, 0], [0, 0, 0, -7]])", {-7, -1, 2.5, 3}, 0);
    eigIs("the 4x4 identity: 1 four times", "eig(identity(4))", {1, 1, 1, 1}, 0);
    eigIs("1x1", "eig([[-2.5]])", {-2.5}, 0);
    eigIs("[[2, 1], [1, 2]]: 1 and 3", "eig([[2, 1], [1, 2]])", {1, 3}, 0);
    eigIs("[[0, -i], [i, 0]] + 5 identity(2): 4 and 6", "eig(sigmay + 5 identity(2))", {4, 6}, 0);
    eigIs("ones(3): 0, 0, 3", "eig([[1, 1, 1], [1, 1, 1], [1, 1, 1]])", {0, 0, 3}, 1e-15);
    eigIs("ones(4): 0, 0, 0, 4", "eig([[1, 1, 1, 1], [1, 1, 1, 1], [1, 1, 1, 1], [1, 1, 1, 1]])", {0, 0, 0, 4}, 1e-15);
    eigIs("the zero matrix", "eig([[0, 0, 0], [0, 0, 0], [0, 0, 0]])", {0, 0, 0}, 0);

    // ---- Hermitian matrices with known eigenvalues (a diagonal turned by this file's rotations), 2x2 to 4x4, seeded; and the identities on each
    {
        int count = 0, good = 0, idGood = 0;
        double worst = 0, worstId = 0;
        for (int trial = 0; trial < 60; trial++) {
            const int n = 2 + trial % 3;
            std::vector<double> eigs;
            for (int k = 0; k < n; k++) eigs.push_back(std::nearbyint(rnd() * 5000) / 1000.0);
            if (trial % 5 == 1) eigs[1] = eigs[0];                                    // a repeated pair
            if (trial % 10 == 7) for (double& e : eigs) e *= 1e-9;                    // small ones (a scale test)
            if (trial % 10 == 8) eigs[0] = 1e6 * eigs[0];                             // one far larger than the rest
            const TM H = hermitianWith(eigs);
            const std::string s = lit(H);
            std::sort(eigs.begin(), eigs.end());
            const TM g = get("eig(" + s + ")");
            count++;
            double scale = 0, d = 1e300;
            for (double e : eigs) scale = std::max(scale, std::fabs(e));
            if (g.ok && g.r == n && g.c == 1) {
                d = 0;
                for (int k = 0; k < n; k++) d = std::max(d, std::abs(g.a[static_cast<std::size_t>(k)] - eigs[static_cast<std::size_t>(k)]) / scale);
            }
            worst = std::max(worst, d);
            if (d <= 2e-14) good++; else std::printf("  eig off by %.3g of the largest (n = %d, trial %d): %s\n", d, n, trial, g.ok ? g.number.c_str() : g.err.c_str());
            // the identities, all through the calculator: Σ eig = tr, Π eig = det, eig(M + c) = eig(M) + c
            double sum = 0, prod = 1, id = 1e300;
            if (g.ok) { for (const cd& z : g.a) { sum += z.real(); prod *= z.real(); } }
            const TM tr = get("tr(" + s + ")"), det = get("det(" + s + ")"), sh = get("eig(" + s + " + 2.5 identity(" + std::to_string(n) + "))");
            if (g.ok && tr.ok && det.ok && sh.ok && sh.r == n) {
                id = std::abs(sum - tr.a[0]) / scale;
                id = std::max(id, std::abs(prod - det.a[0]) / std::pow(scale, n));
                id = std::max(id, std::abs(det.a[0] - tdet(H)) / std::pow(scale, n));
                for (int k = 0; k < n; k++) id = std::max(id, std::abs(sh.a[static_cast<std::size_t>(k)] - g.a[static_cast<std::size_t>(k)] - 2.5) / std::max(scale, 2.5));
            }
            worstId = std::max(worstId, id);
            if (id <= 2e-13) idGood++; else std::printf("  identities off by %.3g (n = %d, trial %d)\n", id, n, trial);
        }
        std::printf("matrices: 60 Hermitian matrices with known eigenvalues: worst error %.3g of the largest eigenvalue; identities worst %.3g; fixed cases worst %.3g\n", worst, worstId, g_worstEig);
        check("eig: 60 seeded Hermitian 2x2, 3x3, 4x4 with known eigenvalues (some repeated, tiny, or one far larger): within 2e-14 of the largest", good == count);
        check("eig: on the same 60, the sum is tr, the product is det (also the cofactor det here) and eig(M + 2.5) = eig(M) + 2.5, within 2e-13", idGood == count);
    }

    // ---- refusals
    refused("[[1, 2], [3, 4]] * [[1, 2, 3]]", "do not fit");
    refused("[[1, 2]] [[3, 4]]", "do not fit");
    refused("[[1, 2], [3, 4]] + [[1, 2]]", "same size");
    refused("[[1, 2], [3, 4]] - identity(3)", "same size");
    refused("[[1, 2], [3, 4]] + 1", "identity");
    refused("2 - sigmax", "identity");
    refused("eig([[1, 2], [3, 4]])", "Hermitian");
    refused("eig([[1, 2], [3, 4]])", "differs");
    refused("eig([[1, i], [i, 1]])", "Hermitian");
    refused("eig([[i, 0], [0, 1]])", "Hermitian");
    refused("eig([[1, 2, 3], [4, 5, 6]])", "square");
    refused("det([[1, 2, 3], [4, 5, 6]])", "square");
    refused("tr([[1, 2, 3], [4, 5, 6]])", "square");
    refused("[[1, 2, 3], [4, 5, 6]]^2", "square");
    refused("[[1, 2], [3, 4]]^-1", "inverse");
    refused("1 / [[1, 2], [3, 4]]", "inverse");
    refused("sigmax / sigmay", "inverse");
    refused("[[1, 2], [3, 4]]^0.5", "whole");
    refused("[[1, 2], [3, 4]]^i", "whole");
    refused("2^[[1, 2], [3, 4]]", "power");
    for (const char* f : {"sqrt", "sin", "exp", "ln", "abs", "cbrt", "re", "arg", "fact", "gamma"}) refused(std::string(f) + "([[1, 2], [3, 4]])", "not supported");
    refused("besselj(0, sigmax)", "not supported");
    refused("sigmax!", "not supported");
    refused("sum(sigmax, k, 1, 3)", "not supported");
    refused("solve(sigmax - x, x, 0, 1)", "matri");
    refused("kron(identity(3), sigmax)", "at most 4");
    refused("kron([[1, 2, 3]], [[1, 2]])", "at most 4");
    refused("identity(5)", "1 to 4");
    refused("identity(0)", "1 to 4");
    refused("identity(1.5)", "1 to 4");
    refused("det(5)", "matrix");
    refused("eig(2)", "matrix");
    refused("transpose(3 m)", "matrix");
    refused("kron(2, sigmax)", "matrix");
    refused("kron(sigmax)", "kron(A, B)");
    refused("[[1, 2], [3, 4]]^[[1]]", "power");
    refused("sin([[1]])", "not supported");

    // ---- the names: sigma and σ alone stay the Stefan-Boltzmann constant; near misses are helped
    check("names: sigma is still the Stefan-Boltzmann constant", evaluate("sigma").ok && !evaluate("sigma").q.mat && evaluate("σ").ok && evaluate("σ").q.v == evaluate("sigma").q.v);
    refused("sigma_x", "sigmax");
    refused("sigmaX", "sigmax");
    refused("pauli_z", "sigmaz");
    refused("pauli", "sigmax");
    refused("sigma x", "sigmax");
    refused("σ y", "sigmay");
    refused("sigmax = 2", "Pauli");
    refused("σz = 2", "Pauli");
    refused("eig = 2", "already");

    // ---- variables and ans
    {
        const Result h = evaluate("H = [[1, 2], [2, 1]] eV");
        check("variables: H = [[1, 2], [2, 1]] eV names a matrix", h.ok && h.assigned == "H" && h.q.mat && h.q.mat->rows == 2);
        calc::Vars vars;
        vars["H"] = h.q;
        const Result e = evaluate("eig(H) to eV", nullptr, &vars);                       // [[a, b], [b, a]] has a − b and a + b
        check("variables: eig(H) to eV = -1, 3 (" + e.number + ")", e.ok && e.number == "-1, 3" && e.unit == "eV");
        check("variables: the used line shows the matrix", e.ok && !e.used.empty() && e.used[0].find("H = your variable = [[") == 0);
        const Result a = evaluate("ans * ans - 2 eV * ans to eV^2", &h.q);               // H² − 2 H = 3 for this H (its eigenvalues −1, 3 solve x² − 2x = 3)
        check("ans: holds a matrix answer (ans * ans - 2 eV * ans = 3 eV² identity) (" + a.number + ")", a.ok && a.number == "[[3, 0], [0, 3]]");
        const Result v = evaluate("H [[1], [1]] to eV", nullptr, &vars);
        check("variables: H v (" + v.number + ")", v.ok && v.number == "[[3], [3]]");
        const Result l = evaluate("ans", &e.q);
        check("ans: eig's list stays a list", l.ok && l.list && l.rows == 2);
    }

    // ---- ħ and h: the powers are carried for the whole matrix, as for a number
    {
        const Result w = evaluate("[[0, 1], [1, 0]] eV / ħ"), f = evaluate("[[0, 1], [1, 0]] eV / hP"), e = evaluate("eig([[0, 1], [1, 0]] eV / ħ) to Hz");
        check("ħ: a matrix of energies / ħ is angular: the note says so", w.ok && has(w.notes, "angular") && has(w.notes, "2π") && w.q.hbarPow == -1);
        check("ħ: divided by hP it is cyclic: no note", f.ok && f.notes.empty());
        check("ħ: eig of it, to Hz: the note too", e.ok && e.list && has(e.notes, "angular"));
        const Result m = evaluate("[[1 eV / ħ, 1 Hz]]");
        check("ħ: entries with different powers of ħ: unknown, no note", m.ok && m.notes.empty());
    }

    // ---- fuzz: random lines with brackets never crash; every answer is finite and shown, every refusal explained
    {
        const char* parts[] = {"[", "]", "[[", "]]", ",", ", ", "1", "2", "0", "-", "+", "*", "/", "^", "(", ")", " ", "i", "eV", "m", "sigmax", "σy", "sigmaz", "eig(", "det(", "tr(",
                               "kron(", "dagger(", "transpose(", "conj(", "identity(2)", "identity(", "to", " to eV", "²", "3.5", "ans", "x", "=", "sqrt(", "exp(", "hbar", "1e308", "!",
                               "[[1, 2], [3, 4]]", "[[1, i], [-i, 1]]", "\xc3"};
        const int np = sizeof(parts) / sizeof(parts[0]);
        unsigned s = 4242;
        int okCount = 0, errCount = 0, matCount = 0;
        bool fine = true;
        const calc::Quantity a = evaluate("[[1, 2], [2, 1]] eV").q;
        for (int i = 0; i < 150000 && fine; i++) {
            std::string l;
            s = s * 1664525u + 1013904223u;
            const int len = 1 + int(s >> 28);
            for (int k = 0; k < len; k++) { s = s * 1664525u + 1013904223u; l += parts[(s >> 16) % np]; }
            const Result r = calc::evaluate(l, (i % 3) ? &a : nullptr);
            if (r.empty) continue;
            if (r.ok) {
                okCount++;
                bool finite = std::isfinite(r.q.v) && std::isfinite(r.q.im) && !r.number.empty() && !r.readAs.empty();
                if (r.q.mat) {
                    matCount++;
                    finite = finite && r.rows >= 1 && r.rows <= 4 && r.cols >= 1 && r.cols <= 4 && static_cast<int>(r.cells.size()) == r.rows * r.cols;
                    for (const cd& z : r.cells) finite = finite && std::isfinite(z.real()) && std::isfinite(z.imag());
                    for (int k = 0; k < r.q.mat->rows * r.q.mat->cols; k++) finite = finite && std::isfinite(r.q.mat->a[static_cast<std::size_t>(k)].real()) && std::isfinite(r.q.mat->a[static_cast<std::size_t>(k)].imag());
                } else finite = finite && r.rows == 0 && r.cells.empty();
                if (!finite) { fine = false; std::printf("  matrix fuzz ok but bad: [%s]\n", l.c_str()); }
                const Result again = calc::evaluate(l, (i % 3) ? &a : nullptr);
                if (!again.ok || again.number != r.number) { fine = false; std::printf("  matrix fuzz: not the same twice [%s]\n", l.c_str()); }
            } else {
                errCount++;
                if (r.error.empty() || r.errPos < -1 || r.errPos > int(l.size()) || r.errPos + r.errLen > int(l.size())) {
                    fine = false; std::printf("  matrix fuzz error bad: [%s] %s %d %d\n", l.c_str(), r.error.c_str(), r.errPos, r.errLen);
                }
            }
        }
        std::printf("matrix fuzz: %d answers (%d matrices), %d refusals\n", okCount, matCount, errCount);
        check("fuzz: 150,000 random lines with brackets, matrices and their functions: no crash, every answer finite and shown, every refusal explained, same answer twice",
              fine && okCount > 1000 && matCount > 300 && errCount > 1000);
    }
}
}  // namespace mt
