// 0.21 stage 1: writes the 15 viewer molecules' coordinates (exactly what the app draws: molecule::get(i).atoms, angstrom) to <dir>/<id>.xyz,
// for tools/make_density_data.py. Run: molecule_xyz <dir>. %.10f keeps every digit that matters (the coordinates are doubles built from CCCBDB values).
#include <cstdio>
#include <string>

#include "molecule/molecule.h"

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: molecule_xyz <dir>\n"); return 2; }
    for (int i = 0; i < molecule::count(); i++) {
        const molecule::Molecule& m = molecule::get(i);
        const std::string path = std::string(argv[1]) + "/" + m.id + ".xyz";
        FILE* f = std::fopen(path.c_str(), "w");
        if (!f) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); return 1; }
        std::fprintf(f, "%d\n%s %s: the Physics Lab Molecule Lab viewer geometry (CCCBDB experimental, principal axes, centre of mass at 0), angstrom\n", static_cast<int>(m.atoms.size()), m.id.c_str(), m.name.c_str());
        for (const molecule::Atom& a : m.atoms) std::fprintf(f, "%-2s %16.10f %16.10f %16.10f\n", molecule::element(a.el).sym, a.x, a.y, a.z);
        std::fclose(f);
    }
    return 0;
}
