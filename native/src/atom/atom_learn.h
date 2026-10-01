// The Atom Lab's Learn guides as text (HTML pieces, the same the web builds), ported from the web's
// lrnLook, lrnScale, lrnMass and LRN_TL. tests/test_atom.cpp checks them word for word against the
// web's own functions run in Node. The interactive parts (tabs, chips, the element bar, the ▶ buttons)
// are drawn by the app (ui/atom_learn.cpp); in the HTML a row of them is marked [[chips]].
#pragma once

#include <string>
#include <utility>
#include <vector>

#include "atom_sim.h"

namespace atom {

struct LearnSection { std::string key, title, html; };

// "What am I even looking at?": the sections for the atom and view open now.
// live: the readout lines at the top of the picture, joined with <br>.
std::vector<LearnSection> lookSections(const AtomParams& p, const std::string& live);
extern const char* const kLookIntro;

// "Scale: how empty is an atom?": tab "scale" (obj = the chosen object, 0–4) or "ten".
std::string scaleHtml(const AtomParams& p, const std::string& tab, int obj);
extern const char* const kScaleObjects[5];       // the chips: pea, marble, tennis ball, football, beach ball

// "Weight: what does an atom weigh?": tab "parts", "miss", "count", "proton" or "ladder".
std::string massHtml(const AtomParams& p, const std::string& tab);

// The timeline: 22 steps; some have a ▶ button that sets the lab (settings in order).
struct TimelineStep {
    const char* y; const char* who; const char* t; const char* d; const char* svg;
    const char* lab;                               // the button's words, or ""
    std::vector<std::pair<std::string, std::string>> set;   // key, value (numbers as text)
};
const std::vector<TimelineStep>& timelineSteps();
extern const char* const kTimelineIntro;

// ---- the panel's texts (the web's buildUI), as the web writes them ----
std::string isotopeLine(int Z, int N);                // "☢ Iron-60: half-life ..." or "● Iron-56: stable"
std::string elemCardHtml(int Z);                      // the element card (web: elemCardHTML)
extern const char* const kTableNote;                  // "Click an element for its most common isotope ..."
std::string factsHtml(int Z);                         // the facts with the temperature bar (web: factsHTML)
std::string spectrumNote(int Z, int N);               // the note under the light spectrum
std::string tempStateHtml(int Z, double K);           // "<b>293 K</b> · 20 °C · 68 °F  →  Iron is <b>🧊 solid</b>"
std::string tempSpeedHtml(int Z, double K);           // "Typical atom speed here: ..." (follows metric/imperial)
extern const char* const kTempNote;
extern const char* const kTempDetailsHtml;            // "What is temperature, really?"
// the decay history list: (kind, text) with kind decNuc, decStep or decNone
std::vector<std::pair<std::string, std::string>> decayHistory(const std::vector<ChainStep>& chain, int Z, int N);

}  // namespace atom
