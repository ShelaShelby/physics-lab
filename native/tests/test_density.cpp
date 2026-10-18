// Molecule Lab, Realistic style (0.21 stage 1) tests, written before the code. Independent references only:
//  - the electron count of each molecule (the sum of the atomic numbers, typed again HERE from the job text),
//  - the MEASURED dipole moments from NIST CCCBDB (experimental, debye; reference codes as CCCBDB gives them; the zeros are by symmetry, NSRDS-NBS10), typed again here,
//  - the molecules' symmetry (mirror planes, inversion, axes),
//  - statistics (a chi-square test of the Monte Carlo dots), geometry (the volume of a sphere), topology (a closed surface).
// The computed numbers (the density, the potential, the calculation's own dipole) come from the embedded data (tools/make_density_data.py).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "molecule/density.h"
#include "molecule/molecule.h"

using namespace molecule;

static int g_failed = 0, g_checks = 0;
static void check(bool ok, const std::string& what) {
    g_checks++;
    if (!ok) { g_failed++; std::printf("FAIL: %s\n", what.c_str()); }
}
static std::string f(double v) { char b[64]; std::snprintf(b, sizeof b, "%.6g", v); return b; }
static const double kPi = 3.14159265358979323846;

struct Ref { const char* id; int electrons; double dipoleD; const char* src; };
static const Ref kRef[] = {
    {"H2", 2, 0, "symmetry (NSRDS-NBS10)"}, {"N2", 14, 0, "symmetry (NSRDS-NBS10)"}, {"O2", 16, 0, "symmetry (NSRDS-NBS10)"},
    {"HCl", 18, 1.093, "NISTDiatomic"}, {"CO2", 22, 0, "symmetry (NSRDS-NBS10)"}, {"H2O", 10, 1.857, "1991Sho/Ebe:5875-5882"},
    {"O3", 24, 0.530, "NSRDS-NBS10"}, {"NH3", 10, 1.476, "1970Shi:3572-3576"}, {"CH4", 10, 0, "symmetry (NSRDS-NBS10)"},
    {"HCN", 14, 2.980, "NSRDS-NBS10"}, {"C2H2", 14, 0, "symmetry (NSRDS-NBS10)"}, {"H2CO", 16, 2.332, "2003The/Cal:8910-8915"},
    {"C2H4", 16, 0, "symmetry (NSRDS-NBS10)"}, {"CH3OH", 18, 1.672, "1981Sas/Lee:228-230"}, {"C6H6", 42, 0, "symmetry (NSRDS-NBS10)"},
};

// ln rho between grid points: tricubic (Catmull-Rom) on ln rho, which is smooth away from the nuclei (a fair way to compare two points that are not grid points)
static double cr(double p0, double p1, double p2, double p3, double t) { return p1 + 0.5 * t * (p2 - p0 + t * (2 * p0 - 5 * p1 + 4 * p2 - p3 + t * (3 * (p1 - p2) + p3 - p0))); }
static double logSample(const dens::Grid& g, double x, double y, double z) {
    const double fx = (x - g.ox) / g.h, fy = (y - g.oy) / g.h, fz = (z - g.oz) / g.h;
    const int i = static_cast<int>(std::floor(fx)), j = static_cast<int>(std::floor(fy)), k = static_cast<int>(std::floor(fz));
    if (i < 1 || j < 1 || k < 1 || i + 2 >= g.nx || j + 2 >= g.ny || k + 2 >= g.nz) return -1e9;
    const auto L = [&](int a, int b, int c) { return std::log(std::max(1e-30f, g.at(a, b, c))); };
    double zz[4];
    for (int c = 0; c < 4; c++) {
        double yy[4];
        for (int b = 0; b < 4; b++) yy[b] = cr(L(i - 1, j - 1 + b, k - 1 + c), L(i, j - 1 + b, k - 1 + c), L(i + 1, j - 1 + b, k - 1 + c), L(i + 2, j - 1 + b, k - 1 + c), fx - i);
        zz[c] = cr(yy[0], yy[1], yy[2], yy[3], fy - j);
    }
    return cr(zz[0], zz[1], zz[2], zz[3], fz - k);
}
// the grid index of the lattice point at (x, y, z), or -1 when it is not (to 1e-6 h) a grid point
static int gridIndex(const dens::Grid& g, double v, double o, int n) {
    const double t = (v - o) / g.h;
    const double r = std::round(t);
    if (std::fabs(t - r) > 1e-6 || r < 0 || r >= n) return -1;
    return static_cast<int>(r);
}
// every grid point with rho > 1e-3 against its image under the map M (a symmetry of the molecule that maps the grid onto itself): equal within 1 %
template <class M> static void latticeSymmetry(const dens::Data& d, const char* what, M map) {
    const dens::Grid& g = d.rho;
    long n = 0, off = 0;
    double worst = 0;
    for (int k = 0; k < g.nz; k++)
        for (int j = 0; j < g.ny; j++)
            for (int i = 0; i < g.nx; i++) {
                const double v = g.at(i, j, k);
                if (v <= 1e-3) continue;
                double x = g.x(i), y = g.y(j), z = g.z(k);
                map(x, y, z);
                const int a = gridIndex(g, x, g.ox, g.nx), b = gridIndex(g, y, g.oy, g.ny), c = gridIndex(g, z, g.oz, g.nz);
                if (a < 0 || b < 0 || c < 0) { off++; continue; }
                worst = std::max(worst, std::fabs(g.at(a, b, c) - v) / v);
                n++;
            }
    std::printf("  symmetry %s %s: %ld grid points above 1e-3, worst difference %s %%\n", d.id.c_str(), what, n, f(100 * worst).c_str());
    check(off == 0 && n > 1000, d.id + " " + what + ": the image of every grid point is a grid point (" + std::to_string(off) + " are not, " + std::to_string(n) + " compared)");
    check(worst < 0.01, d.id + " " + what + ": density equal within 1 % (worst " + f(100 * worst) + " %)");
}

int main() {
    // ---- 0. every molecule has data, made at the app's own shape ----
    std::map<std::string, const dens::Data*> D;
    for (const Ref& r : kRef) {
        const dens::Data* d = dens::get(r.id);
        check(d != nullptr, std::string(r.id) + ": embedded density data present and readable");
        if (!d) continue;
        D[r.id] = d;
        const int mi = findId(r.id);
        check(mi >= 0, std::string(r.id) + " is a viewer molecule");
        if (mi < 0) continue;
        const Molecule& m = get(mi);
        bool same = m.atoms.size() == d->nuclei.size();
        for (std::size_t i = 0; same && i < m.atoms.size(); i++)
            same = d->nuclei[i].Z == (m.atoms[i].el == H ? 1 : m.atoms[i].el == C ? 6 : m.atoms[i].el == N ? 7 : m.atoms[i].el == O ? 8 : 17);
        double worst = 0;                                  // 0.21: the viewer draws the molecule in exactly the frame the density was computed in (on every compiler)
        for (std::size_t i = 0; same && i < m.atoms.size(); i++) {
            const dens::Nucleus& u = d->nuclei[i];
            worst = std::max({worst, std::fabs(u.x * dens::kA0Angstrom - m.atoms[i].x), std::fabs(u.y * dens::kA0Angstrom - m.atoms[i].y), std::fabs(u.z * dens::kA0Angstrom - m.atoms[i].z)});
        }
        check(same && worst < 1e-8 && m.storedFrame, std::string(r.id) + ": the calculation's nuclei are the viewer's atoms (worst " + f(worst) + " A)");
        check(d->electrons == r.electrons, std::string(r.id) + ": " + std::to_string(r.electrons) + " electrons in the calculation (file says " + std::to_string(d->electrons) + ")");
    }

    // ---- 1. electron count from the stored grid, within 0.5 % ----
    std::printf("Electron count from the stored grid (and the dipole):\n");
    for (const Ref& r : kRef) {
        if (!D.count(r.id)) continue;
        const dens::Data& d = *D[r.id];
        const double n = dens::electronCount(d), e = (n - r.electrons) / r.electrons;
        std::printf("  %-6s N = %.4f (expected %d, %+.3f %%)\n", r.id, n, r.electrons, 100 * e);
        check(std::fabs(e) < 0.005, std::string(r.id) + ": stored grid integrates to " + f(n) + " electrons, expected " + std::to_string(r.electrons) + " within 0.5 %");
    }

    // ---- 2. dipoles: the calculation vs the measurement (0.25 D; zeros 0.02 D), and the stored grid vs the calculation (0.05 D) ----
    for (const Ref& r : kRef) {
        if (!D.count(r.id)) continue;
        const dens::Data& d = *D[r.id];
        const double mu = std::sqrt(d.dip[0] * d.dip[0] + d.dip[1] * d.dip[1] + d.dip[2] * d.dip[2]) * dens::kAuDebye;
        double g[3];
        dens::dipoleFromGrid(d, g);
        const double dg = std::sqrt((g[0] - d.dip[0]) * (g[0] - d.dip[0]) + (g[1] - d.dip[1]) * (g[1] - d.dip[1]) + (g[2] - d.dip[2]) * (g[2] - d.dip[2])) * dens::kAuDebye;
        const double tol = r.dipoleD == 0 ? 0.02 : 0.25;
        std::printf("  %-6s dipole computed %.4f D (%.4f, %.4f, %.4f), measured %.3f D (%s), difference %+.4f D; from the stored grid %.4f D off\n", r.id, mu,
                    d.dip[0] * dens::kAuDebye, d.dip[1] * dens::kAuDebye, d.dip[2] * dens::kAuDebye, r.dipoleD, r.src, mu - r.dipoleD, dg);
        check(std::fabs(mu - r.dipoleD) <= tol, std::string(r.id) + ": computed dipole " + f(mu) + " D vs measured " + f(r.dipoleD) + " D within " + f(tol) + " D");
        check(dg <= 0.05, std::string(r.id) + ": dipole from the stored grid within 0.05 D of the calculation's (" + f(dg) + " D)");
    }

    // ---- 3. symmetry of the stored density ----
    if (D.count("H2O")) {                                  // planar, the H atoms at +-x, the molecule in the xy plane: mirror planes x = 0 and z = 0
        latticeSymmetry(*D["H2O"], "mirror x", [](double& x, double&, double&) { x = -x; });
        latticeSymmetry(*D["H2O"], "mirror z", [](double&, double&, double& z) { z = -z; });
    }
    for (const char* id : {"CO2", "N2"})
        if (D.count(id)) {                                 // linear along x: inversion, and the axis (a quarter turn about x maps the grid onto itself)
            latticeSymmetry(*D[id], "inversion", [](double& x, double& y, double& z) { x = -x; y = -y; z = -z; });
            latticeSymmetry(*D[id], "quarter turn about the axis", [](double&, double& y, double& z) { const double t = y; y = -z; z = t; });
        }
    if (D.count("CH4")) {                                  // the H atoms at (+-a, +-a, +-a) with an even number of minus signs: S4 about z and C3 about (1, 1, 1)
        latticeSymmetry(*D["CH4"], "S4 about z", [](double& x, double& y, double& z) { const double t = x; x = y; y = -t; z = -z; });
        latticeSymmetry(*D["CH4"], "C3 about (1,1,1)", [](double& x, double& y, double& z) { const double t = x; x = y; y = z; z = t; });
    }
    if (D.count("C6H6")) {                                 // a regular hexagon in the xy plane: z -> -z and a half turn about z on the grid; a sixth of a turn about z (not a grid map: compared by interpolating ln rho)
        const dens::Data& d = *D["C6H6"];
        latticeSymmetry(d, "mirror z", [](double&, double&, double& z) { z = -z; });
        latticeSymmetry(d, "half turn about z", [](double& x, double& y, double&) { x = -x; y = -y; });
        double worst = 0;
        long n = 0;
        for (double r = 0.3; r < 9; r += 0.37)
            for (double phi = 0; phi < 2 * kPi; phi += 0.0731)
                for (double z : {0.0, 0.6, 1.3, 2.2}) {
                    const double x = r * std::cos(phi), y = r * std::sin(phi);
                    const double a = logSample(d.rho, x, y, z);
                    if (a < std::log(1e-3)) continue;
                    bool nearNucleus = false;                          // within 1 bohr of a nucleus ln rho bends too sharply (the cusp, the core shell) for any interpolation between 0.25 bohr points
                    for (const dens::Nucleus& u : d.nuclei) nearNucleus = nearNucleus || std::hypot(x - u.x, y - u.y, z - u.z) < 1.0;   // (there the grid maps above check it)
                    if (nearNucleus) continue;
                    const double c = std::cos(kPi / 3), s = std::sin(kPi / 3);
                    const double b = logSample(d.rho, c * x - s * y, s * x + c * y, z);
                    worst = std::max(worst, std::fabs(std::exp(b - a) - 1));
                    n++;
                }
        std::printf("  symmetry C6H6 sixth of a turn: %ld points above 1e-3 (1 bohr or more from a nucleus, tricubic on ln rho), worst difference %s %%\n", n, f(100 * worst).c_str());
        check(n > 1000 && worst < 0.01, "C6H6 six-fold: density equal within 1 % (worst " + f(100 * worst) + " %, " + std::to_string(n) + " points)");
    }

    // ---- 4. the Monte Carlo dots ----
    if (D.count("H2O")) {
        const dens::Data& d = *D["H2O"];
        std::vector<float> a, b, c;
        std::vector<double> work;
        dens::sampleDots(d, 60000, dens::kDotSeed, a, work);
        dens::sampleDots(d, 60000, dens::kDotSeed, b, work);
        dens::sampleDots(d, 60000, dens::kDotSeed + 1, c, work);
        check(a.size() == 180000 && a == b, "dots: the same seed gives the same 60,000 dots");
        check(a != c, "dots: another seed gives other dots");
        // chi-square: dots per block of 4 x 4 x 4 grid cells against the stored density's weights: a grid point's cell carries rho (1 - sum w) h^3, a fine-cube cell rho w hf^3
        // (its volume shared between the blocks it overlaps). The weights are worked out here with this file's own loops.
        const dens::Grid& g = d.rho;
        const int B = 4, bx = (g.nx + B - 1) / B, by = (g.ny + B - 1) / B, bz = (g.nz + B - 1) / B;
        std::vector<double> expect(static_cast<std::size_t>(bx) * by * bz, 0.0), seen(expect.size(), 0.0);
        double tot = 0;
        for (int k = 0; k < g.nz; k++) for (int j = 0; j < g.ny; j++) for (int i = 0; i < g.nx; i++) {
            double w = 1;
            for (const dens::Fine& c : d.fine) {
                const dens::Nucleus& u = d.nuclei[static_cast<std::size_t>(c.atom)];
                const double r = std::hypot(g.x(i) - u.x, g.y(j) - u.y, g.z(k) - u.z);
                if (r < d.r0) w -= dens::cubeWeight(r, d.r1, d.r0);
            }
            const double e = g.at(i, j, k) * std::max(0.0, w) * g.h * g.h * g.h;
            expect[(static_cast<std::size_t>(k / B) * by + j / B) * bx + i / B] += e;
            tot += e;
        }
        const auto blockOf = [&](double v, double o) { return static_cast<int>(std::floor(((v - o) / g.h + 0.5) / B)); };   // blocks end half a step past their last grid point
        for (const dens::Fine& c : d.fine) {
            const dens::Nucleus& u = d.nuclei[static_cast<std::size_t>(c.atom)];
            for (int k = -c.m; k <= c.m; k++) for (int j = -c.m; j <= c.m; j++) for (int i = -c.m; i <= c.m; i++) {
                const double r = c.h * std::sqrt(double(i * i + j * j + k * k));
                if (r >= d.r0) continue;
                const int w = 2 * c.m + 1;
                const double e = c.v[(static_cast<std::size_t>(k + c.m) * w + (j + c.m)) * w + (i + c.m)] * dens::cubeWeight(r, d.r1, d.r0) * c.h * c.h * c.h;
                tot += e;
                const double ctr[3] = {u.x + c.h * i, u.y + c.h * j, u.z + c.h * k}, org[3] = {g.ox, g.oy, g.oz};
                int lo[3]; double fr[3];                      // per axis: the lower block and the share of the cell in it
                for (int a = 0; a < 3; a++) {
                    lo[a] = blockOf(ctr[a] - c.h / 2, org[a]);
                    const int hi = blockOf(ctr[a] + c.h / 2, org[a]);
                    const double edge = org[a] + (B * hi - 0.5) * g.h;
                    fr[a] = hi == lo[a] ? 1.0 : (edge - (ctr[a] - c.h / 2)) / c.h;
                }
                for (int q = 0; q < 8; q++) {
                    double f2 = 1;
                    int b3[3];
                    for (int a = 0; a < 3; a++) { const int up = (q >> a) & 1; b3[a] = lo[a] + up; f2 *= up ? 1 - fr[a] : fr[a]; }
                    if (f2 <= 0) continue;
                    expect[(static_cast<std::size_t>(b3[2]) * by + b3[1]) * bx + b3[0]] += e * f2;
                }
            }
        }
        long outside = 0;
        for (std::size_t t = 0; t < a.size(); t += 3) {
            const double x = a[t] / dens::kA0Angstrom, y = a[t + 1] / dens::kA0Angstrom, z = a[t + 2] / dens::kA0Angstrom;
            const int i = blockOf(x, g.ox), j = blockOf(y, g.oy), k = blockOf(z, g.oz);
            if (i < 0 || j < 0 || k < 0 || i >= bx || j >= by || k >= bz) { outside++; continue; }
            seen[(static_cast<std::size_t>(k) * by + j) * bx + i]++;
        }
        double chi2 = 0, restE = 0, restS = 0;
        int bins = 0;
        for (std::size_t q = 0; q < expect.size(); q++) {
            const double e = expect[q] / tot * 60000;
            if (e < 5) { restE += e; restS += seen[q]; continue; }   // the thin blocks pooled into one bin
            chi2 += (seen[q] - e) * (seen[q] - e) / e;
            bins++;
        }
        if (restE >= 5) { chi2 += (restS - restE) * (restS - restE) / restE; bins++; }
        const double dof = bins - 1, z = (chi2 - dof) / std::sqrt(2 * dof);
        std::printf("  dots chi-square: %.1f for %.0f degrees of freedom (z = %.2f), %ld outside the grid\n", chi2, dof, z, outside);
        check(outside == 0, "dots: all inside the grid");
        check(dof > 50 && std::fabs(z) < 3.5, "dots: chi-square of the counts per block against the grid density passes (z = " + f(z) + ", |z| < 3.5)");
    }

    // ---- 5. the surface: a sphere of known volume, and every molecule's surface closed ----
    {
        const int n = 61;
        const double h = 0.1, o = -3;
        std::vector<float> fld(static_cast<std::size_t>(n) * n * n);
        for (int k = 0; k < n; k++) for (int j = 0; j < n; j++) for (int i = 0; i < n; i++) {
            const double x = o + h * i, y = o + h * j, z = o + h * k;
            fld[(static_cast<std::size_t>(k) * n + j) * n + i] = static_cast<float>(std::exp(-std::sqrt(x * x + y * y + z * z)));   // a density-like field; the surface exp(-r) = exp(-2) is the sphere r = 2
        }
        dens::Mesh m;
        dens::marchTets(fld.data(), n, n, n, o, o, o, h, std::exp(-2.0), m);
        const double v = dens::enclosedVolume(m), want = 4.0 / 3 * kPi * 8;
        std::string why;
        const bool closed = dens::isClosed(m, &why);
        std::printf("  sphere r = 2: %d triangles, volume %.4f (exact %.4f, %+.3f %%), closed %d %s\n", m.triangles(), v, want, 100 * (v / want - 1), closed, why.c_str());
        check(closed, "marching: the sphere is a closed surface " + why);
        check(std::fabs(v / want - 1) < 0.01, "marching: the sphere's volume within 1 % (" + f(v) + " vs " + f(want) + ")");
        check(m.n.size() == m.p.size(), "marching: one normal per vertex");
        bool outward = true;                               // the normals point out of the sphere
        for (std::size_t q = 0; q < m.p.size(); q += 3) outward = outward && m.p[q] * m.n[q] + m.p[q + 1] * m.n[q + 1] + m.p[q + 2] * m.n[q + 2] > 0;
        check(outward, "marching: the normals point outward");
    }
    for (const Ref& r : kRef) {
        if (!D.count(r.id)) continue;
        for (double iso : {0.001, 0.0016, 0.002, 0.01}) {
            dens::Mesh m;
            dens::densitySurface(*D[r.id], iso, 2, m);
            std::string why;
            const bool closed = dens::isClosed(m, &why);
            if (iso == 0.002) std::printf("  %-6s surface 0.002: %d triangles, volume %.2f A^3, potential range +-%.4f au\n", r.id, m.triangles(), dens::enclosedVolume(m), dens::potentialRange(m));
            check(closed, std::string(r.id) + ": the " + f(iso) + " surface is closed " + why);
            check(m.triangles() < 30000, std::string(r.id) + ": the " + f(iso) + " surface has fewer than 30,000 triangles (" + std::to_string(m.triangles()) + ")");
            bool finite = true;
            for (float v : m.value) finite = finite && std::isfinite(v);
            check(finite, std::string(r.id) + ": the potential is known at every vertex of the " + f(iso) + " surface");
        }
    }

    // ---- 6 (numbers behind the pictures): the potential on the 0.002 surface. Water: negative on the O side, positive on the H side; CO2: negative at both O ----
    if (D.count("H2O")) {
        const dens::Data& d = *D["H2O"];
        dens::Mesh m;
        dens::densitySurface(d, 0.002, 2, m);
        const dens::Nucleus& o = d.nuclei[0];
        double nearO = 0, nearH = 0;
        for (std::size_t q = 0; q < m.value.size(); q++) {
            const double x = m.p[3 * q] / dens::kA0Angstrom, y = m.p[3 * q + 1] / dens::kA0Angstrom, z = m.p[3 * q + 2] / dens::kA0Angstrom;
            double dO = std::hypot(x - o.x, y - o.y, z - o.z), dH = 1e9;
            for (std::size_t a = 1; a < d.nuclei.size(); a++) dH = std::min(dH, std::hypot(x - d.nuclei[a].x, y - d.nuclei[a].y, z - d.nuclei[a].z));
            if (y < o.y - 1.0) nearO = std::min(nearO, double(m.value[q]));    // beyond the O, away from the H atoms (the H atoms are at +y)
            if (dH < dO - 1.5) nearH = std::max(nearH, double(m.value[q]));
        }
        std::printf("  water 0.002 surface: most negative on the O side %.4f au, most positive on the H side %.4f au\n", nearO, nearH);
        check(nearO < -0.02 && nearH > 0.02, "water: negative potential on the O side, positive on the H side");
    }
    if (D.count("CO2")) {
        const dens::Data& d = *D["CO2"];
        dens::Mesh m;
        dens::densitySurface(d, 0.002, 2, m);
        double minus = 0, plus = 0;
        for (std::size_t q = 0; q < m.value.size(); q++) {
            const double x = m.p[3 * q] / dens::kA0Angstrom;
            if (x < -3) minus = std::min(minus, double(m.value[q]));           // the O atoms are at x = -+2.196 bohr: the caps beyond them
            if (x > 3) plus = std::min(plus, double(m.value[q]));
        }
        std::printf("  CO2 0.002 surface: most negative beyond each O: %.4f and %.4f au\n", minus, plus);
        check(minus < -0.01 && plus < -0.01, "CO2: negative potential at both oxygen ends");
    }

    // ---- damaged files are refused, never a crash ----
    {
        std::size_t n = 0;
        const unsigned char* p = dens::embedded("H2O", n);
        check(p && n > 1000, "H2O: the embedded file is there");
        if (p) {
            std::vector<unsigned char> bad(p, p + n);
            dens::Data d;
            std::string err;
            check(dens::parse(bad.data(), bad.size(), d, err), "H2O: the embedded file parses (" + err + ")");
            check(!dens::parse(bad.data(), bad.size() - 1, d, err), "a file cut short is refused");
            bad[0] = 'X';
            check(!dens::parse(bad.data(), bad.size(), d, err), "a file with a wrong magic is refused");
            std::vector<unsigned char> b2(p, p + n);
            const std::int32_t na = 1000;
            std::memcpy(b2.data() + 8, &na, 4);
            check(!dens::parse(b2.data(), b2.size(), d, err), "a file with a silly atom count is refused");
            dens::Rng rng(7);
            for (int t = 0; t < 300; t++) {                     // random damage: never a crash
                std::vector<unsigned char> b3(p, p + n);
                for (int q = 0; q < 8; q++) b3[rng.next() % std::min<std::size_t>(n, 400)] = static_cast<unsigned char>(rng.next());
                dens::parse(b3.data(), static_cast<std::size_t>(rng.next() % (n + 1)), d, err);
            }
            check(true, "300 damaged files: no crash");
        }
        check(dens::get("XYZ") == nullptr, "an unknown molecule has no data");
    }

    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
