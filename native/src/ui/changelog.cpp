#include "changelog.h"

#include <algorithm>
#include <string>

#include "app/app_data.h"
#include "fonts.h"
#include "look.h"

#include "imgui.h"

namespace ui {

namespace {

struct Entry { const char* version; const char* title; const char* text; };

// The native app's own history. Newest first.
const Entry kNative[] = {
    {"0.5", "The web app's own features: settings, themes, header, history",
     "Settings as in the web version: text size (Small, Normal, Large, Extra large), font (Theme default, Serif, Easy to read, "
     "Rounded, Monospace; Atkinson Hyperlegible and Nunito are built in, under the SIL Open Font License), units, motion (Auto "
     "follows Windows' animation setting; Reduced starts the labs paused, makes camera moves jump, stops Cloud 3D spinning), "
     "Reset my progress with the web's two pop-ups, and Developer > Unlock everything. The 🎨 Theme menu has the web's eleven "
     "themes, including the secret Synthwave and Monochrome; the panels, cards, text and pictures all follow it. Everything is "
     "remembered between runs in the app-data folder: the look, units, unlocks and every setting of both labs. A header across "
     "the top has the web's buttons: 🎨 Theme, 🌐 language (English; Eesti keel is coming), 👤 About (the web's page with its "
     "sources), ⚙ Settings, 🕰 history (the web's history, then this list) and the lab's Update badge with its changelog. "
     "Under it, the 💡 Did you know? line with all the web's facts (tap for the next one; it waits while the mouse is on it) "
     "and its hidden 🎁 surprise, and the tabs: 📚 Learn lists every guide and timeline of both labs and opens them in their "
     "lab; 🏆 Achievements shows Neon Nights, Sharp Eyes, Half-Life and Galactic Center, locked until earned, with the "
     "web's pop-up when one is earned. Point of No Return is left out: it belongs to the Black Hole Lab, which is not in "
     "the native app. The \"monochrome\" word in the Atom Lab guide unlocks the Monochrome theme and Sharp Eyes."},
    {"0.4", "Atom Lab",
     "The second lab: pick Orbit Lab or Atom Lab at the top of the panel (each keeps its state while the other is shown). "
     "Atom Lab is the web version's, feature for feature: any nucleus from hydrogen to oganesson with one electron, in four "
     "pictures (Orbit, as Bohr's 1913 circles or with the real nucleus; Cloud; Cloud 3D; Slice), photons that make the electron "
     "jump and build a spectrum, a zoom from the whole atom down to the protons and neutrons and the quarks inside them "
     "(Cinematic, Accurate or Classic quarks), every known nuclide with its real half-life and decay odds, decays happening on "
     "their own with a decay history, the periodic table with element cards and facts, the light spectrum, and the "
     "Temperature card with its little box of atoms. The four Learn guides are here too: What am I even looking at?, Scale, "
     "Weight and the 22-step timeline, whose buttons set the lab up. "
     "The physics uses the web's constants and steps (SI units inside), and the tests compare it with the web's own code: "
     "the electron's orbit, the sampled cloud, photon jumps and decays step by step with the same random numbers, and every "
     "text word for word. Cloud dots, glows and gluon particles are drawn as instanced shapes, a few draw calls for thousands. "
     "Units follow Settings (°F, mph, miles where the web converts them). Defaults asks first; Reset starts a fresh atom; "
     "More / Less info is remembered. The Performance card is on the Atom Lab panel too, with the picture's draw calls."},
    {"0.3", "Orbit Lab complete: Learn guides, Settings with metric or imperial, watch a planet turn",
     "New ⚙ Settings: show distances, speeds and temperatures in metric (km, km/s, °C) or imperial (miles, mi/s, °F). "
     "Every line of every facts card follows it, including the km/h and °C inside the notes, and so does the scale bar; "
     "the choice is remembered. The lab still works in metres and seconds inside. The Defaults button now asks before it "
     "puts Orbit Lab's settings back to how they started. "
     "With Stars set to Off, the picture now follows Windows' light or dark theme, as the web version follows the page "
     "theme. The Journey's last caption is now the same as in the web version. "
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

// The web Orbit Lab's changelog (the native Orbit Lab follows web Orbit Lab 7.7.2).
const Entry kWeb[] = {
#include "web_orbit_changelog.inc"
};
// The web Atom Lab's history (the native Atom Lab follows web Atom Lab 7.7.3).
const Entry kWebAtom[] = {
#include "web_atom_changelog.inc"
};
const char* const kWebAtomNote =
#include "web_atom_changelog_note.inc"
    ;

}  // namespace

const char* appVersion() { return PL_VERSION; }

const NativeEntry* nativeHistory(int& count) {
    count = int(sizeof kNative / sizeof kNative[0]);
    static NativeEntry out[sizeof kNative / sizeof kNative[0]];
    for (int i = 0; i < count; i++) out[i] = {kNative[i].version, kNative[i].title, kNative[i].text};
    return out;
}

// The web's changelog pop-up for the lab on screen ("Orbit Lab · changelog"): each update's number, title and text
void labLogWindow(bool* open, int lab) {
    if (!*open) return;
    const app::LabInfo& L = app::kLabInfo[lab];
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const float s = ImGui::GetFontSize() / 16;
    ImGui::SetNextWindowSize(ImVec2(std::min(vp->WorkSize.x - 32, 620 * s), vp->WorkSize.y * 0.8f), ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    const std::string title = std::string(L.name) + app::kLogTitleTail + "###labLog";
    if (ImGui::Begin(title.c_str(), open, ImGuiWindowFlags_NoCollapse)) {
        const Entry* rows = lab == 1 ? kWebAtom : kWeb;
        const int n = lab == 1 ? int(sizeof kWebAtom / sizeof kWebAtom[0]) : int(sizeof kWeb / sizeof kWeb[0]);
        const ImU32 accent = col(look().c.accent), line = col(look().c.line);
        for (int i = 0; i < n; i++) {
            // .logRow: the number in a 52-point column (accent, bold), then the title and the text (muted, smaller)
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float x1 = p.x + 62 * s, w = ImGui::GetContentRegionAvail().x;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddText(fonts().bold, ImGui::GetFontSize(), ImVec2(p.x, p.y + 10 * s), accent, rows[i].version);
            ImGui::SetCursorScreenPos(ImVec2(x1, p.y + 10 * s));
            ImGui::PushTextWrapPos(p.x + w - ImGui::GetWindowPos().x);   // window-relative
            ImGui::PushFont(fonts().bold, 0);
            ImGui::TextUnformatted(displayable(rows[i].title, fonts().bold).c_str());
            ImGui::PopFont();
            ImGui::SetCursorScreenPos(ImVec2(x1, ImGui::GetCursorScreenPos().y + 2 * s));
            ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 13 / 16);
            ImGui::PushStyleColor(ImGuiCol_Text, vec(look().c.muted));
            ImGui::TextUnformatted(displayable(rows[i].text).c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::PopTextWrapPos();
            const float y = ImGui::GetCursorScreenPos().y + 10 * s;
            if (i + 1 < n) dl->AddLine(ImVec2(p.x, y), ImVec2(p.x + w, y), line);
            ImGui::SetCursorScreenPos(ImVec2(p.x, y));
            ImGui::Dummy(ImVec2(w, 0));
        }
        const char* note = lab == 1 ? kWebAtomNote : app::kOrbitLogNote;
        if (*note) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, vec(look().c.muted));
            ImGui::TextWrapped("%s", displayable(note).c_str());
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();
}

}  // namespace ui
