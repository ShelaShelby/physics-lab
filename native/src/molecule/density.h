// Molecule Lab (0.21 stage 1): the "Realistic" style's data and maths (no graphics; tests/test_density.cpp checks it).
//
// The data is COMPUTED, not measured: a Kohn-Sham DFT calculation for each of the 15 viewer molecules at the app's own (CCCBDB experimental) shape, made by
// tools/make_density_data.py (PySCF, B3LYP, aug-cc-pVTZ; data/molecules/density/README.txt) and embedded in the program (cmake/embed_density.cmake).
// Per molecule: the electron density on a cubic grid (point values, e/bohr^3), the same density on fine cubes round the non-hydrogen nuclei (only for the integrals:
// the density has a cusp at a nucleus, which a 0.25 bohr grid cannot integrate), the electrostatic potential on a coarser grid (atomic units) and the dipole moment
// from the calculation's density matrix. Units inside: bohr and atomic units; the picture works in angstrom (kA0Angstrom).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace molecule::dens {

constexpr double kA0Angstrom = 0.529177210544;    // Bohr radius in angstrom, CODATA 2022 (NIST, physics.nist.gov/cuu/Constants)
constexpr double kAuDebye = 2.5417464715;         // 1 e bohr in debye: CODATA 2022 e a0 c x 1e21 (1 D = 1e-21 / c C m)
constexpr double kHartreeKJmol = 2625.4996394792; // 1 hartree (CODATA 2022 E_h = 4.3597447222060e-18 J) x N_A (exact) in kJ/mol: 1 au of potential = 2625.50 kJ/mol per elementary charge

struct Grid {                                      // values at origin + h (i, j, k), index (k ny + j) nx + i
    double ox = 0, oy = 0, oz = 0, h = 1;
    int nx = 0, ny = 0, nz = 0;
    std::vector<float> v;
    double x(int i) const { return ox + h * i; }
    double y(int j) const { return oy + h * j; }
    double z(int k) const { return oz + h * k; }
    float at(int i, int j, int k) const { return v[(static_cast<std::size_t>(k) * ny + j) * nx + i]; }
    double sample(double x, double y, double z) const;   // trilinear; 0 outside
};
struct Fine { int atom = 0; double h = 0; int m = 0; std::vector<float> v; };   // (2m+1)^3 values at nucleus + h (i, j, k), i, j, k = -m..m
struct Nucleus { int Z; double x, y, z; };         // bohr
struct Data {
    std::string id, basis, method;
    std::vector<Nucleus> nuclei;
    double dip[3] = {0, 0, 0};                     // au (e bohr), from the density matrix (the calculation's exact dipole)
    double energy = 0;                             // hartree
    int electrons = 0, spin2S = 0;
    Grid rho;                                      // e/bohr^3
    double r1 = 0, r0 = 0;                         // the partition of unity round each fine cube's nucleus (bohr)
    std::vector<Fine> fine;
    Grid pot;                                      // au (hartree per elementary charge); NaN = not computed
};

// the MEASURED dipole moments the panel compares with (NIST CCCBDB, experimental, debye, with CCCBDB's reference codes; the zeros are by symmetry, NSRDS-NBS10)
struct MeasuredDipole { const char* id; double debye; const char* ref; };
constexpr MeasuredDipole kMeasuredDipole[15] = {
    {"H2", 0, "NSRDS-NBS10 (zero by symmetry)"}, {"N2", 0, "NSRDS-NBS10 (zero by symmetry)"}, {"O2", 0, "NSRDS-NBS10 (zero by symmetry)"}, {"HCl", 1.093, "NISTDiatomic"},
    {"HCN", 2.980, "NSRDS-NBS10"}, {"H2O", 1.857, "1991Sho/Ebe:5875-5882"}, {"CO2", 0, "NSRDS-NBS10 (zero by symmetry)"}, {"O3", 0.530, "NSRDS-NBS10"},
    {"NH3", 1.476, "1970Shi:3572-3576"}, {"CH4", 0, "NSRDS-NBS10 (zero by symmetry)"}, {"C2H2", 0, "NSRDS-NBS10 (zero by symmetry)"}, {"H2CO", 2.332, "2003The/Cal:8910-8915"},
    {"C2H4", 0, "NSRDS-NBS10 (zero by symmetry)"}, {"CH3OH", 1.672, "1981Sas/Lee:228-230"}, {"C6H6", 0, "NSRDS-NBS10 (zero by symmetry)"},
};
const MeasuredDipole* measuredDipole(const std::string& id);

bool parse(const unsigned char* p, std::size_t n, Data& out, std::string& err);
const unsigned char* embedded(const std::string& id, std::size_t& size);   // the embedded file (nullptr if none)
const Data* get(const std::string& id);            // decoded once, on first use (nullptr if missing or damaged)

// the integration weight of a fine cube: 1 for d <= r1, 0 for d >= r0, a C-infinity step between (the same function the tool used)
double smoothStep(double t);                       // 1 at t <= 0, 0 at t >= 1
double cubeWeight(double d, double r1, double r0);
// integral of rho f over all space from the STORED data: sum_grid f rho (1 - sum_A w_A) h^3 + sum_A sum_cube f rho w_A hf^3
double electronCount(const Data& d);
void dipoleFromGrid(const Data& d, double mu[3]);  // au: sum Z_A R_A - integral r rho (a neutral molecule: independent of the origin)
double rhoAt(const Data& d, double x, double y, double z);   // bohr in, trilinear on the grid

// ---- the cloud: Monte Carlo dots drawn from the stored density, deterministic. Each grid point's cell carries rho (1 - sum_A w_A) h^3 and each fine-cube point's cell rho w_A hf^3
// (the split of the integrals above, so the dots add up to the electron count); a dot is put uniformly at random in its cell ----
struct Rng {                                       // splitmix64: the same numbers on every compiler (std:: distributions are not)
    std::uint64_t s;
    explicit Rng(std::uint64_t seed) : s(seed) {}
    std::uint64_t next() { std::uint64_t z = (s += 0x9E3779B97F4A7C15ull); z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return z ^ (z >> 31); }
    double uniform() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }   // [0, 1)
};
constexpr std::uint64_t kDotSeed = 20261018;
// n dots, angstrom, xyz each, into `out` (resized); `cdf` is a work buffer kept by the caller
void sampleDots(const Data& d, int n, std::uint64_t seed, std::vector<float>& out, std::vector<double>& cdf);

// ---- the surface: marching tetrahedra (marching cubes with each grid cube cut into the 6 tetrahedra round its main diagonal, the Kuhn split: neighbouring cubes cut a
// shared face the same way, so the surface has no cracks) on a scalar field; inside = value > iso. Vertices are shared (one per crossed grid edge) ----
struct Mesh {
    std::vector<float> p;                          // xyz per vertex
    std::vector<float> n;                          // unit normal per vertex (outward: towards lower values), area-weighted from the faces (densitySurface: from the density's gradient)
    std::vector<int> tri;                          // 3 per triangle, counter-clockwise seen from outside
    std::vector<float> value;                      // per vertex: what the caller puts there (the potential)
    int triangles() const { return static_cast<int>(tri.size() / 3); }
};
// field: nx ny nz values at o + h (i, j, k), index (k ny + j) nx + i
void marchTets(const float* field, int nx, int ny, int nz, double ox, double oy, double oz, double h, double iso, Mesh& out);
double enclosedVolume(const Mesh& m);              // divergence theorem; > 0 for an outward-facing closed surface
bool isClosed(const Mesh& m, std::string* why = nullptr);   // every edge in exactly two triangles, once each way

// the density surface of a molecule, angstrom, coloured by the potential (au) at each vertex. The field is ln rho on every `stride`-th grid point (stride 2: 0.5 bohr)
void densitySurface(const Data& d, double iso, int stride, Mesh& out);
double potentialRange(const Mesh& m);              // max |V| over the vertices: the symmetric colour scale is -range..+range

}  // namespace molecule::dens
