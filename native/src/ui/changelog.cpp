#include "changelog.h"

#include <string>

#include "imgui.h"

namespace ui {

namespace {

struct Entry { const char* version; const char* title; const char* text; };

// The native app's own history. Newest first.
const Entry kNative[] = {
    {"0.3", "Orbit Lab complete: Learn guides, watch a planet turn",
     "New Learn card with the three guides of the web version, word for word: What am I even looking at? (one page "
     "with a row of headings to jump to, following your 2D/3D and Sizes choice and the live date), Why do we have "
     "seasons? (with the lab's own Earth: day length and noon Sun height from pole to pole, and each planet's lean), "
     "and the Solar System timeline, whose ▶ buttons take the lab there. "
     "New button in the View card: pick a planet and press Slow time to watch it turn; time slows down so one turn "
     "takes about 24 seconds, and the view follows and zooms in on it."},
    {"0.2", "3D view, night sky, belts, spinning planets, Journey and Jump to a date",
     "The planets now really spin, with the real IAU axes and rates: Earth shows its real coastlines, the others a grid of "
     "latitude and longitude, each with a dashed axis and N; Saturn's rings tilt with its axis. The cards say how long a turn "
     "takes and how far the axis leans, the Sun's card how it spins, and Earth's card which season it is. "
     "Journey and dates card: the Journey zooms out from Earth to the centre of our galaxy, and when it ends (or you touch "
     "anything) all your settings and your view come back. Jump to a date puts everything where it really was or will be "
     "from 1800 to 2100 (Greenwich time); a date outside that range, or one that does not exist, now gets a note instead of "
     "a silent jump. "
     "New 3D view (View card): drag to turn it, right-drag or Shift-drag (or the Drag button) to move it, Spin to turn it "
     "slowly. Nearer things look bigger, and dashed lines drop from each planet to the flat plane of the orbits. "
     "Behind everything is the real night sky: the 2,874 brightest stars at their true directions and colours, the band "
     "of the Milky Way, and (Sky card) constellation lines and names. Zoom far out to find the far-away things at their "
     "real distance: the nearest stars, the Pleiades, the Orion Nebula, the galactic centre and the two Voyagers, all "
     "clickable and in Go to. "
     "The asteroid belt (with its Kirkwood gaps), Jupiter's Trojans, the Kuiper belt and the Oort cloud are here, "
     "with the same 6,000 stand-in rocks as the web version (same random seed, same orbits; the tests check they sit in "
     "the same places). Each belt is drawn with a single draw call, and the rocks are only moved when time moves. "
     "Click a belt's name or its Go to tile for its facts; a Belts and Oort cloud switch is under Solar System extras. "
     "A new Performance card shows how long each frame takes (the last one, the average and the worst of the last "
     "240 frames, with a small graph) and how much of it is the app's own work. A VSync switch turns off waiting for "
     "the screen, so you can see the real headroom instead of the monitor's refresh rate. Each Windows build is now "
     "also published on one fixed GitHub Release page, tagged \"latest\"."},
    {"0.1", "First native version: Orbit Lab (pilot)",
     "A native Windows version of Physics Lab, written in C++ with SDL3, OpenGL 3.3 and Dear ImGui. "
     "It has Orbit Lab only: the real Solar System where everything is today (the eight planets, the five dwarf planets, "
     "fourteen moons and six famous comets), with the same physics, numbers and method as the web version (leapfrog steps "
     "around the Sun, NASA/JPL orbital elements). Play and pause, Time speed, zoom with the mouse wheel or the slider, drag "
     "to move the view, click anything for its facts, Go to tiles, the habitable zone, distance rings, trails, the scale bar "
     "and the barycentre. Not here yet: the 3D view, the night sky and far-away things, the belts and Oort cloud, spinning "
     "planets, Journey, Jump to a date, Learn and the other labs."},
};

// The web Orbit Lab's history, for reference (the native version started from web Orbit Lab 7.7.2).
const Entry kWeb[] = {
#include "web_orbit_changelog.inc"
};

}  // namespace

const char* appVersion() { return PL_VERSION; }

void changelogWindow(bool* open) {
    if (!*open) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x * 0.5f, vp->WorkSize.y * 0.75f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::Begin("What's new in Physics Lab (native)", open, ImGuiWindowFlags_NoCollapse)) {
        for (const Entry& e : kNative) {
            ImGui::SeparatorText((std::string("Update ") + e.version + " · " + e.title).c_str());
            ImGui::TextWrapped("%s", e.text);
        }
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("Web version history (Orbit Lab, for reference)")) {
            for (const Entry& e : kWeb) {
                ImGui::SeparatorText((std::string(e.version) + " · " + e.title).c_str());
                ImGui::TextWrapped("%s", e.text);
            }
        }
    }
    ImGui::End();
}

}  // namespace ui
