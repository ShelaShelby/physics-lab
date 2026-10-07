// 0.15 stage 5a: what the Atom Lab's toolbar buttons DO (the new UI), in one place and without any drawing, so a unit test can check each button's state change against the old controls
// (INVENTORY.md section 2: the "View" row and "the bar under the picture"). Every button calls the same AtomLab function the old control called.
#pragma once

#include "atom/atom_sim.h"

namespace atom {

enum class Tool {
    Atom,       // View: Orbit, and the picture zoomed out to the whole atom (old: the View row's "Orbit" and the bar's "⌕ Atom")
    Nucleus,    // the nucleus fills the picture (old: the bar's "⌕ Nucleus"); the chart has no zoom, so from the chart it also goes to the Orbit view
    Nucleon,    // one nucleon fills the picture (old: the bar's "⌕ Nucleon"); the small choice inside the Nucleus button
    Cloud,      // View: Cloud (side view)
    Cloud3D,    // View: Cloud 3D
    Slice,      // View: Slice
    Chart,      // View: Chart of nuclides
    ResetView,  // old: the bar's "↺ Reset view" (turns the 3D views back); on the chart: "Reset tilt, zoom and position"
    Centre,     // old: the bar's "⌖ Centre" (shown only while the picture is zoomed or moved)
    DragMode,   // old: the bar's "✥ Drag: rotate / move"
    BuildAnother,  // old: the Nucleus section's "☢ Build another" (0.15 stage 5b: in the Nucleus panel of the toolbar)
    Reshuffle,     // old: "🎲 Reshuffle nucleons" (same)
};

void pressTool(AtomLab& L, Tool t);
bool toolOn(const AtomLab& L, Tool t);       // the button is lit: Atom / Nucleus by the level shown, Cloud / Cloud 3D / Slice each by its own view only (5l), Chart by the chart
bool toolShown(const AtomLab& L, Tool t);    // Centre and DragMode appear only when the old bar showed them; the others always

// ---- 0.15 stage 5b: the element chip and the dropdown's controls (each = the old control's setParam calls) ----
struct ChipText { std::string title, sub; };   // "U · Uranium-238" and "92 protons · 146 neutrons" (1 proton, 1 neutron, 0 neutrons)
ChipText elementChipText(int Z, int N);
void pickElement(AtomLab& L, int Z);           // a table cell: the element and its usual isotope
void stepProtons(AtomLab& L, int d);           // the Protons slider's - and + (the neutrons follow to the usual count)
void stepNeutrons(AtomLab& L, int d);          // the Neutrons slider's - and +
void usualNeutrons(AtomLab& L);                // the "usual" button
void pickFamous(AtomLab& L, int i);            // the famous-isotopes list; an index out of range does nothing

}  // namespace atom
