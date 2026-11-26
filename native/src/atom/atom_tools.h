// 0.15 stage 5a: what the Atom Lab's toolbar buttons DO (the new UI), in one place and without any drawing, so a unit test can check each button's state change against the old controls
// (INVENTORY.md section 2: the "View" row and "the bar under the picture"). Every button calls the same AtomLab function the old control called.
#pragma once

#include <string>
#include <vector>

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
    ZoomAtom,      // 0.22 stage 6: Controls › View › Zoom to: Atom (the whole atom; the picture stays the same)
};

void pressTool(AtomLab& L, Tool t);

// 0.86.0 (PLAN_REVIEW.md R5, the owner's decision of 25 Nov 2026): the made-up learning pictures are kept, labelled kMadeUpLabel next to their choice and in their
// caption, and are never the realistic choice. They are: the Orbit picture (its electrons as balls gliding on circles: atom_sim.h gAng, glideOrbit), the Simple
// quark style (three balls joined by flux tubes) and the Nucleons look's colours and glow. key: "picture" (the view), "qstyle", "style".
constexpr const char* kMadeUpLabel = "Made up, for learning";
bool madeUpChoice(const std::string& key, const std::string& value);
std::string madeUpCaption(const std::string& key);           // starts with kMadeUpLabel; empty for a key with no made-up choice
const char* madeUpValue(const std::string& key);             // the made-up value of that key ("Orbit", "Simple", "Nucleons"); nullptr: none
std::vector<std::string> madeUpOn(const AtomLab& L);         // the made-up choices the lab has now, as "key=value"
// The Atom Lab's realistic choices (ui/realest.cpp's dots and its Most realistic button read the values from here, its reasons stay there): Cloud 3D, then
// each setting below. tests/test_atom.cpp checks that none is made up and that no element or zoom brings one back.
const char* realisticValue(const std::string& key);          // nullptr: no realistic choice for that key
void applyRealistic(AtomLab& L);
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
