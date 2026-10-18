// Molecule Lab (0.18 stage 1): 15 small molecules in their MEASURED (experimental) equilibrium / average geometry, as fixed pictures. No graphics in here (molecule_view.cpp draws;
// ui/shell_molecule.cpp has the panel). Everything is a pure function or plain data, so the tests (tests/test_molecule.cpp) can check it without a window.
//
// Sources (every number below names one; the data tables are in molecule.cpp):
//   - Geometry (bond lengths in angstrom, angles in degrees) and rotational constants (cm^-1): NIST Computational Chemistry Comparison and Benchmark Database (CCCBDB),
//     NIST Standard Reference Database 101, Release 22, experimental geometries and experimental rotational constants pages, with the reference code CCCBDB gives for each value.
//     The values were read from CCCBDB by the owner on 18 Oct 2026 (the cloud session cannot reach cccbdb.nist.gov) and given to the job as text; see native/data/molecules/README.txt.
//   - Isotope masses: NIST "Atomic Weights and Isotopic Compositions" (physics.nist.gov/cgi-bin/Compositions/stand_alone.pl), pages saved in native/data/molecules/.
//   - Colours (CPK) and van der Waals radii: PubChem periodic table (as given to the job). Both are DRAWING CONVENTIONS, not measured shapes: a molecule has no hard edge.
//   - Bond orders (1, 2, 3, aromatic): the usual structural formula; a drawing convention.
#pragma once

#include <string>
#include <vector>

namespace molecule {

constexpr int kLabIndex = 2;               // AppState::lab (0.19: Orbit 0, Atom 1, Molecule 2; app::labName)

enum El { H = 0, C, N, O, Cl, kElCount };
struct Element {
    const char* sym;
    const char* name;
    int Z, A;                    // the isotope used: the most abundant one (NIST page: "Isotopic Composition")
    double massU, massUncU;      // its relative atomic mass and standard uncertainty (NIST page: "Relative Atomic Mass")
    double abundance;            // its isotopic composition (NIST page), a fraction
    double vdwPm;                // van der Waals radius, pm (PubChem periodic table): the space-filling ball
    unsigned rgb;                // CPK colour (PubChem periodic table), 0xRRGGBB
};
const Element& element(int el);
// Ball-and-stick ball radius in angstrom: ONLY a drawing scale (a quarter of the van der Waals radius), not a measured size.
double ballRadiusA(int el);
constexpr double kBallScale = 0.25;
constexpr double kStickRadiusA = 0.09;     // drawing scale: a stick's half width, angstrom

// A value the source lists. kind 'r' a bond length (elements a-b), 'a' an angle a-b-c (b in the middle), 'd' a dihedral a-b-c-d for the atoms i0..i3.
// Lengths and angles are given by ELEMENTS, as CCCBDB lists them (rCH, aHCH): every bond / angle in the molecule with those elements has that value.
struct Value {
    char kind;
    int e0, e1, e2;              // elements (r: e0-e1; a: e0-e1-e2)
    double value;                // angstrom or degrees; NaN until computed for a derived value
    const char* ref;             // the CCCBDB reference code
    const char* note;            // what kind of value CCCBDB calls it ("re", "rm structure"), or ""
    int decimals;                // digits after the point as listed
    bool symmetry;               // fixed by the other listed values plus the symmetry (the listed number is that value ROUNDED to `decimals`)
    bool derived;                // NOT in CCCBDB: worked out by this app from an assumption (CH3OH's H-C-O angle); labelled "derived" in the app
    int i0 = -1, i1 = -1, i2 = -1, i3 = -1;   // dihedral: the atoms
};
struct Atom { int el; double x, y, z; };
struct Bond { int a, b, order; };          // order 1, 2, 3 or 4 = aromatic (drawing convention)
struct Molecule {
    std::string id;              // the settings key ("H2O")
    std::string formula;         // as usually written (HCl, CH3OH), for the list
    std::string hill;            // Hill order (C first, then H, then the rest alphabetically; with no C all alphabetically)
    std::string name;
    std::vector<Atom> atoms;     // angstrom, centre of mass at the origin, principal axes along x, y, z (smallest moment along x)
    std::vector<Bond> bonds;
    std::vector<Value> values;
    double rotA = 0, rotB = 0, rotC = 0;   // CCCBDB experimental rotational constants, cm^-1 (linear: only B; 0 = not listed)
    const char* rotRef = "";
    bool linear = false, planar = false;
    std::string shapeNote;       // how the 3D shape was fixed (the symmetry assumed)
    bool derivedAngle = false;   // has a derived value (shown "H-C-O angle derived")
};

int count();                               // 15
const Molecule& get(int i);                // built once (geometry from the listed values), then fixed
int findId(const std::string& id);         // -1 if unknown

// ---- maths (also used by the tests) ----
double dist(const Atom& a, const Atom& b);
double angleDeg(const Atom& a, const Atom& centre, const Atom& c);
double dihedralDeg(const Atom& a, const Atom& b, const Atom& c, const Atom& d);
double massU(const Molecule& m);           // the sum of the atoms' isotope masses (u): the most abundant isotopologue
double massUncU(const Molecule& m);        // its uncertainty (the same isotope's uncertainty added linearly, different isotopes in quadrature)
// principal moments of inertia, u angstrom^2, ascending (Jacobi on the 3 x 3 tensor about the centre of mass)
void principalMoments(const Molecule& m, double I[3]);
// B = h / (8 pi^2 c I), cm^-1 (CODATA 2022 h, c and u, src/atom/atom_core.h); I in u angstrom^2; 0 for I ~ 0
double rotConstCm(double I);
// the listed value that a bond (a, b) / an angle (a, centre, c) of this molecule has, or nullptr
const Value* lengthValue(const Molecule& m, int a, int b);
const Value* angleValue(const Molecule& m, int a, int centre, int c);
// the atom's label: "O1", "H2" (element symbol and its number among the atoms of that element in the molecule, counted from 1)
std::string atomLabel(const Molecule& m, int i);

// ---- camera: orthographic. view = R (p) + pan; screen x = cx + view.x * s, y = cy - view.y * s; depth = view.z (bigger = nearer) ----
struct Camera {
    double R[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    double zoom = 1;             // 1 = the molecule fits the view
    double panX = 0, panY = 0;   // angstrom, in view space
};
constexpr double kZoomMin = 0.3, kZoomMax = 8;
Camera defaultCamera();
void rotate(Camera& c, double dxPts, double dyPts);   // a drag: about the screen's y axis by dx, its x axis by dy (0.01 rad a point)
void spin(Camera& c, double radians);                 // about the screen's y axis
void zoomBy(Camera& c, double factor);
void pan(Camera& c, double dxPts, double dyPts, double scale);   // scale: points per angstrom
void orthonormalize(double R[9]);
// points per angstrom so the molecule (with space-filling balls) fits a view of w x h points at zoom 1
double fitScale(const Molecule& m, double w, double h);

struct Proj { double x, y, depth, r; };              // screen point, depth (angstrom) and the drawn ball radius (points)
enum class Style { BallStick = 0, SpaceFill = 1 };
// the atoms on screen; view centre (cx, cy), s points per angstrom (fitScale * zoom)
void project(const Molecule& m, const Camera& c, Style st, double cx, double cy, double s, std::vector<Proj>& out);

// ---- picking: the front-most atom or bond under (x, y) ----
struct Pick { int atom = -1, bond = -1; };
Pick pick(const Molecule& m, const Camera& c, Style st, double cx, double cy, double s, double x, double y);

// ---- settings: "v1 mol=H2O style=ball labels=1 spin=0 zoom=1 pan=0,0 rot=r0,...,r8"; anything bad falls back to the default, never a crash ----
struct Settings {
    int mol = 5;                 // H2O
    Style style = Style::BallStick;
    bool labels = true, spin = false;
    Camera cam = defaultCamera();
};
constexpr int kSettingsVersion = 1;
std::string saveSettings(const Settings& s);
Settings loadSettings(const std::string& text);

}  // namespace molecule
