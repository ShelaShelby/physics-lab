// Molecule Lab stage 2 (0.20): the builder. A flat sketch (atoms and bonds, edited by the owner), its formula, mass and name, and an ESTIMATED 3D shape.
// No graphics in here (molecule_view.cpp draws the sketch, ui/shell_molecule.cpp has the panel); everything is plain data and pure functions, tested in tests/test_molbuild.cpp.
//
// Owner decisions (18 Oct 2026, DECISIONS.md): atoms H, C, N, O, Cl only; strict valences H 1, C 4, N 3, O 2, Cl 1 (a convention: formal charges, as in ozone, are not offered);
// a built molecule has no measured shape, so its 3D shape is ESTIMATED (ideal VSEPR angles + one CCCBDB bond length per bond type) and labelled "estimated, not measured";
// a name is shown only when the built graph matches one of 23 known molecules.
//
// Sources:
//   - Bond lengths: NIST CCCBDB, SRD 101, Release 22, experimental geometries, one reference molecule per bond type (the table in builder.cpp names it and its reference code).
//     Given to the job by the owner as text (fetched 18 Oct 2026); the cloud session cannot reach cccbdb.nist.gov. A bond type not in the table is refused, never guessed.
//   - Angles: VSEPR (R. J. Gillespie and R. S. Nyholm, Q. Rev. Chem. Soc. 11, 339 (1957)), the ideal angles only: 2 electron domains 180, 3 domains 120, 4 domains arccos(-1/3) = 109.4712.
//     A simplified model: lone pairs make real angles smaller (water: 104.48 measured, 109.47 here).
//   - Valence electrons (H 1, C 4, N 5, O 6, Cl 7): the main-group column of the periodic table.
#pragma once

#include <string>
#include <vector>

#include "molecule/molecule.h"

namespace molecule::build {

constexpr int kMaxAtoms = 30;            // a design limit: the estimated shape is built (and timed) for up to 30 atoms
constexpr int kUndoSteps = 100;          // undo / redo depth (the owner asked for at least 50)
constexpr double kSketchBond = 1.3;      // a bond's length in the flat sketch (sketch units; a drawing choice, NOT a measured length)
constexpr double kSnapRadius = 1.9;      // dropped this close to another atom (sketch units): it snaps and bonds
constexpr double kAtomRadius = 0.36;     // the sketch disc's radius (sketch units; hit test)

int valence(int el);                     // H 1, C 4, N 3, O 2, Cl 1 (owner decision 3: a convention)
int valenceElectrons(int el);            // H 1, C 4, N 5, O 6, Cl 7
const char* orderSymbol(int order);      // "-", "=", "#"
std::string bondName(int e0, int e1, int order);   // "C=O", "N-H", "H-Cl", "O-Cl": as the job writes them (C, N, O, then H, then Cl)

// a bond type's length (angstrom) from the CCCBDB table, with the reference molecule and CCCBDB's reference code; nullptr: not in our sources (refused)
struct LengthRef { int e0, e1, order; double angstrom; const char* molecule; const char* ref; };
const LengthRef* bondLength(int e0, int e1, int order);
int lengthCount();
const LengthRef& lengthAt(int i);

// ---- the graph: atoms (element, flat sketch position) and bonds (orders 1-3) ----
struct SAtom { int el; double x, y; };
struct SBond { int a, b, order; };
struct Graph {
    std::vector<SAtom> atoms;
    std::vector<SBond> bonds;
    bool operator==(const Graph& o) const;
};
int bondOrderSum(const Graph& g, int atom);     // the valence it uses
int openValences(const Graph& g);               // summed over the atoms (never negative per atom)
int findBond(const Graph& g, int a, int b);     // the bond's index, -1 if none
int pieces(const Graph& g);                     // connected pieces (0 for an empty graph)
bool complete(const Graph& g);                  // at least one atom, no open valence, one piece
// why a bond of `order` between a and b (new, or the existing one changed to that order) is refused; "" = allowed. Valence first ("O already has 2 bonds"), then the table.
std::string refuseBond(const Graph& g, int a, int b, int order);

std::string hill(const Graph& g);               // Hill order: C, then H, then the rest alphabetically; with no C, all alphabetically
Molecule toMolecule(const Graph& g);            // the sketch as a flat Molecule (z = 0; for the mass and the formula)
double massU(const Graph& g);                   // most abundant isotopes (the NIST masses of molecule.cpp)
double massUncU(const Graph& g);

// ---- names: the 15 viewer molecules and 8 more; a name only for an exact match of elements, bonds and bond orders ----
struct Known { const char* id; const char* name; int viewer; };   // viewer: its index in the viewer list, or -1
int knownCount();                               // 23
const Known& known(int i);
const Graph& knownGraph(int i);                 // benzene as a Kekule ring (alternating single and double bonds)
int matchKnown(const Graph& g);                 // the index in known(), or -1 ("no name in our list")

// ---- the estimated shape ----
int lonePairs(const Graph& g, int atom);        // (valence electrons - bond order sum) / 2, rounded down, never below 0
int steric(const Graph& g, int atom);           // bonded neighbours + lone pairs
double idealAngle(int stericNumber);            // 180, 120, 109.4712206... (arccos(-1/3)); NaN below 2
constexpr double kTetrahedral = 109.47122063449069;
// the 3D shape: atoms placed by walking the graph with the ideal angles and staggered (anti) torsions, a ring first laid out flat, then relaxed by a spring model
// (bond lengths from the table, 1-3 distances from the ideal angles, a short-range repulsion between the other pairs). Deterministic: no random numbers.
// Fails (false, why) for an empty or disconnected graph, too many atoms, or a bond with no table length. Completeness is NOT required here (the panel asks for it).
bool estimateShape(const Graph& g, Molecule& out, std::string& why);

// ---- the sketch editor, with undo / redo ----
class Editor {
public:
    const Graph& graph() const { return g_; }
    int picked = -1;                            // the picked atom (-1: none)
    std::string message;                        // the last refusal or note ("" = none)
    unsigned revision() const { return rev_; }  // grows with every change of the graph
    int addAtom(int el, double x, double y);    // its index, or -1 (message says why)
    bool bond(int a, int b, int order);         // a new bond (refused: false, message)
    void beginDrag(int atom);                   // a drag of an atom starts: one undo step for the whole drag and its snap
    void moveAtom(int atom, double x, double y);
    int drop(int atom);                         // the drag ends: snaps to and bonds with the nearest unbonded atom in reach; the new bond's index or -1
    int cycleBond(int b);                       // 1 -> 2 -> 3 -> 1, skipping refused orders (message: why); the new order
    bool deleteAtom(int atom);
    void clear();
    bool undo();
    bool redo();
    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    void load(const Graph& g);                  // replaces everything (settings); no undo step
    int hitAtom(double x, double y) const;      // the atom under a sketch point (front-most = last drawn), -1
    int hitBond(double x, double y, double halfWidth) const;
private:
    void step();                                // remember the graph before a change
    Graph g_;
    std::vector<Graph> undo_, redo_;
    unsigned rev_ = 0;
    bool dragging_ = false;
    Graph dragStart_;
};

// ---- settings: one versioned line, "v1 mode=build view=sketch el=C atoms=C:0:0/H:1.3:0 bonds=0:1:1"; anything damaged falls back to the defaults (an empty sketch) ----
struct BuildSettings {
    bool build = false;                         // Viewer (false) or Build
    int view = 0;                               // 0 Sketch, 1 3D shape
    int element = C;                            // the element a click adds
    Graph graph;
};
std::string saveBuild(const BuildSettings& s);
BuildSettings loadBuild(const std::string& text);

}  // namespace molecule::build
