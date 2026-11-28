// 0.25: "closest to reality" (the owner's decision of 5 Nov 2026): for each setting where one choice is clearly closer to the real thing, which one and why, in ONE table, so the
// mark on that choice (ui::sh::markReal) and the lab's "Most realistic" button can never disagree. Settings where no choice is more real (trails, labels, spin, outlines,
// colours, the app's look) are not in it. 0.96.0: the list is app/realistic.h, and every lab opens with it (the owner, 28 Nov 2026; ui/settings.cpp).
#pragma once

#include <initializer_list>
#include <string>

namespace ui { struct AppState; }

namespace ui::real {

struct Pick { const char* key; const char* value; std::string why; };
const Pick* find(const char* lab, const std::string& key);   // lab "orbit" / "atom" / "molecule" / "blackhole"; nullptr: this setting has no realistic choice
// marks part `part` of the next segmented control / check box when `key` has a pick and the pick's value is `value` (call once per part that could be the pick)
void mark(const char* lab, const std::string& key, const std::string& value, int part);

void markLabels(const char* lab, const std::string& key, const std::initializer_list<const char*>& labels);   // a segmented control whose labels are the values
void markCheck(const char* lab, const std::string& key);                                                         // a check box (the pick is "on" or "off")

void applyOrbit(AppState& app);      // every Orbit Lab pick at once
void applyAtom(AppState& app);
void applyMolecule(AppState& app);
void applyBlackHole(AppState& app);   // 0.55.0
void applyLab(AppState& app);         // 0.55.0: the lab on screen's
void applyAll(AppState& app);         // 0.96.0: all four labs (the one-time switch of 0.96.0, ui/settings.cpp)
const char* buttonCaption();         // the words under the button: what the dot means

}  // namespace ui::real
