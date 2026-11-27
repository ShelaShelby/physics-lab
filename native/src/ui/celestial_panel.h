// 0.92.0: the Orbit Lab's celestial mechanics in the Data panel: Lagrange points, the Kirkwood gaps, a Lambert transfer planner with its porkchop plot, an ephemeris table and an
// integrator drift check. The numbers are orbit/lagrange.h, orbit/lambert.h and orbit/ephem_table.h (tested by tests/test_celestial.cpp); this file draws them.
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "ui/orbit_panel.h"
#include "ui/shell_widgets.h"

namespace ui {

// the tools as Data panel "tables": id and title (ids start with "cm:")
const std::vector<std::pair<std::string, std::string>>& celestialTools();
bool isCelestialTool(const std::string& id);
// draws one tool in the panel (below the table chooser); keyboard focus in a text box is reported by celestialFieldActive
void celestialDraw(sh::Panel& p, AppState& app, const std::string& id);
bool celestialFieldActive();
// the panel is closed or another table is chosen: the picture's overlay (Lagrange points, a transfer orbit) goes
void celestialLeave(AppState& app);
// the benchmark's orbit.porkchop (and a test): Earth to Mars, the largest grid the controls reach; phase moves the dates as a user's drag does (any number, it wraps).
// Returns the cells solved this call (0 when nothing was remade).
long celestialBenchPorkchop(AppState& app, double phase);
// text for the benchmark's Path line: who solves the porkchop grid and why
std::string celestialPorkchopPathText(bool computeOk);
// a grid is being solved (the benchmark's response line waits for it)
bool celestialBusy();
// the benchmark keeps the transfer planner's choices and gives them back
void celestialBenchSave();
void celestialBenchRestore();
// the largest grid the controls reach (cells on a side)
int celestialPorkchopMaxSide();

}  // namespace ui
