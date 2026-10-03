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
    {"0.10.4", "The real Moon (Orbit Lab, Real planet physics)",
     "With Planet physics set to Real, Earth's Moon is now the real one: its direction, distance and phase on the date shown come from the lunar theory ELP2000-82B (a 1988 theory), checked against NASA/JPL "
     "Horizons (DE441): at the dates checked between 1800 and 2100 it is within 5.2 km (2.9 arcseconds as seen from the Earth) at worst. Select the Moon to see its phase (new moon, waxing crescent, first quarter and so on) "
     "and how much of it is lit. Earth is now drawn where it really is, a few thousand km off the Earth–Moon balance point, which is what moves smoothly round the Sun. The Moon is still drawn at the "
     "exaggerated distance used for moon orbits, so you can see it. The other moons, and Simple, are unchanged; outside the years 1800 to 2100 the Moon goes back to its simple ellipse. At very fast Time "
     "speeds the Moon's orbit is lit instead of the Moon (as for every moon) and Earth is held at the balance point so it does not flicker. Real is still an option, and Simple is still the default."},
    {"0.10.3", "Real planets start from JPL's data (Orbit Lab)",
     "With Planet physics set to Real, the planets now start from NASA/JPL Horizons' positions and velocities (the DE441 ephemeris) for 1 January 2026 12:00 TDB, instead of the lab's own simplified orbital "
     "elements. Measured against Horizons' tables for 1 January of every year from 1800 to 2100, the worst error of any planet's position is now under 30,000 km (Mercury) with relativity off, and under 8,000 km "
     "for the others; before, the worst errors were millions of km for the inner planets and Jupiter, and over a hundred million km for Saturn, Uranus and Neptune. The lab's calendar dates are treated as TDB (the time scale "
     "of the JPL data) with no conversion from UTC; the note under Planet physics says so, and gives the worst errors. The General relativity (1PN) tick box is now off by default: measured against JPL, the Sun-potential "
     "term gets Mercury's extra turn right (about 43 arcseconds per century) but the motion along the orbit slightly wrong, so it moves every planet's position away from JPL (Mercury, worst case 1800 to 2100: "
     "70,045 km with it, 29,231 km without). The box is still there, and a setting you saved earlier is kept (press Defaults once to put it off). Real is still an option, and Simple is still the default, "
     "until it has been tried on a PC. Still left out: moons, dwarf planets (Pluto is not in) and asteroids pulling on the planets. Simple is unchanged."},
    {"0.10.2", "Phase colours, quantum beats and smooth photon jumps (Atom Lab)",
     "The Atom Lab can now show the phase of the electron's wave. New orientations m = +1, −1 (and ±2, ±3 for d and f) are complex waves, and a new setting, \"Phase colours: Sign / Wheel\", "
     "colours each dot by the phase of the wave with a colour ring (Sign is the old two-colour look and is still the default). New: \"Superposition\", two orbitals at once, such as 1s + 2p. "
     "The cloud then changes in time at the beat frequency (E₂ − E₁)/h: 1s + 2p sloshes back and forth, 1s + 2s breathes, 2s + 2p does not change at all (same energy here; fine structure and the Lamb shift are not modelled). "
     "Real beats are far too fast to see, so the picture is slowed down and the line under it says by how much. Photon jumps are now smooth: in the cloud views the cloud slides from one shape to the other, "
     "and in the Orbit view the electron glides between the circles (only to help the eye: Bohr's model has no path between circles). An emitted photon appears when the jump ends, and an absorbed photon starts the jump when it arrives. "
     "This is the textbook picture of a transition as a mixture of two states, not a calculation of how a photon is absorbed. With Reduced motion on, jumps are instant."},
    {"0.10.1", "General relativity, the Sun's wobble, and two fixes",
     "With Planet physics set to Real there is a new tick box, \"General relativity (1PN)\" (on by default), that adds Einstein's first-order correction to the Sun's pull, so Mercury's "
     "orbit turns about 43 arcseconds per century more, as measured. It is an approximation with the Sun's field only: it leaves out relativity between the planets, the Sun's spin and "
     "shape, and higher orders. Also in Real: the picture is drawn round the true balance point (the barycentre) from the planets' motion, the Sun's own path round it is drawn as a "
     "trail, and \"Exaggerate the Sun's wobble (x50)\" (on by default) draws that path 50 times too big so you can see it (it is labelled exaggerated, not to scale). "
     "Fixed: the Milky Way band no longer stops in the middle of the sky or goes missing when you turn the 3D view (most of it had been dropped), and the two secret themes show "
     "their names once unlocked instead of \"???\"."},
    {"0.10.0", "Planet physics: Simple or Real",
     "A new setting in the Orbit Lab, \"Planet physics\", has two choices. Simple (the default) is what the lab always did: every planet moves alone round the Sun and the planets do "
     "not pull on each other. Real lets the Sun and the eight planets pull on each other with Newtonian gravity, moved by a Wisdom-Holman integrator (a method made for planets). "
     "It leaves out relativity and the pull of moons, dwarf planets and asteroids on the planets, and the picture stays centred on the Sun. The start values are the lab's own simplified "
     "orbital elements for 1 January 2026, not NASA's ephemeris, so far from that date the planets drift away from their true positions (Mercury by about 12 degrees in 1800). "
     "Dates and Time speed work in both. The benchmark always uses Simple."},
    {"0.9.9", "Theme grid, clearer history button, readable nucleus label",
     "The 🎨 Theme menu is now a grid of cards, three in a row, each with its colour dot, its name and a tick on the one in use; the short description of a theme shows when "
     "you hold the mouse over its card, and names are never cut off, even at Extra large text. In the Atom Lab the header button that used to say \"Update 7.7.3\" now says "
     "\"Web Atom Lab history 7.7.3\" (it shows the history of the web Atom Lab that this Atom Lab follows), and the window it opens has the same name. The nucleus label in the "
     "Atom Lab (for example \"Cadmium-114 · 48p, 66n\") has a thin edge in the page colour, so it can be read over the electron cloud on dark and light themes. "
     "Sources added to About: the papers behind the star masses and the textbooks for the hydrogen-like orbitals."},
    {"0.9.8", "A 90% outline for the electron cloud",
     "A new setting in the Atom Lab's Cloud, Cloud 3D and Slice views, \"90% outline\" (Off by default), draws a thin line round the smallest region where the electron is found "
     "90% of the time. It is not a sphere: for p, d and f orbitals it follows the real shape (dumbbells, rings, clovers), and an orbital with nodes can have several surfaces. "
     "In Slice it is the line where the cut plane crosses that surface, in Cloud it is the region's shadow seen from the side, and in Cloud 3D it is a faint wireframe that turns "
     "with the cloud. It uses the same model as the cloud: one electron, no relativity, hydrogen-like orbitals; atoms with more electrons have different shapes. "
     "The benchmark always has it off."},
    {"0.9.7", "Reset to today keeps your view choices",
     "\"Reset to today\" (and the Today buttons in the Journey card and the Learn window) now sets only the date and time back to today, and clears the selection and Follow. "
     "It no longer changes your zoom or the Moons, Dwarf planets, Comets and Belts switches (it already kept your Time speed). Defaults still puts everything back. "
     "There is also a new roadmap file, ROADMAP.md, in the project, and the Windows download page is set up to say what the version is and how to get past Windows' warning for an app that is not code-signed yet."},
    {"0.9.6", "Quarks readable on light themes",
     "In the Atom Lab's quark zoom, the dark patch that was drawn behind the glowing quarks is gone on the light themes (Light and Paper), where it looked like a dark "
     "smudge. There the quarks, their halos, the gluon field and the sea pairs are drawn in darker, more saturated colours that read on a light page, and the red, green "
     "and blue colour charges are still easy to tell apart. On the dark themes nothing changes: the quarks glow as before. Only the drawing changed, not the physics."},
    {"0.9.5", "A smooth Milky Way, and star masses",
     "Orbit Lab: the Milky Way band in the night sky is now one smooth band with soft edges instead of three overlapping stripes, so there are no joints or "
     "stray grey streaks when you zoom far out; it is in the same place on the sky as before, and with the constellation names on it has its own label, "
     "\"The Milky Way (the plane of our galaxy, simplified)\". The far-away stars now show their mass under the name, in solar masses, taken from published "
     "papers: Alpha Centauri, Sirius, Procyon (each star of the pair), Proxima Centauri, Fomalhaut, Arcturus and Sagittarius A*. Masses worked out "
     "from stellar models say \"from models\", and a very uncertain one (Proxima) shows its range. Stars whose mass could not be checked against a "
     "published source are left without one. A line near the bottom says the masses are published estimates and some are uncertain."},
    {"0.9.4", "A glowing, denser electron cloud",
     "In the Atom Lab's Cloud, Cloud 3D and Slice views the electron cloud is drawn as soft glowing dots instead of tiny hard squares: where the electron is most likely to be "
     "the light adds up and glows, and thin places fade. In Cloud 3D, size and brightness change smoothly with depth. The nodes, where the wave is zero, stay as clear gaps, "
     "and the blue and orange phase colours are kept (on light themes the dots are darker so they show on white). Two new settings, Cloud style (Classic is the old look, "
     "Glow is new) and Cloud density (Normal 12,000 dots, High 24,000, Ultra 48,000), are remembered and put back by Defaults. More dots show the same probability more "
     "smoothly; they do not change the physics: the dots are found in exactly the same way, and the total light of the cloud is the same at every density. The benchmark's "
     "Cloud 3D scene uses Classic and Normal, so its results stay comparable with older ones, and the stress scenes got a Glow, Ultra Cloud 3D scene."},
    {"0.9.3", "Four fixes: benchmark scenes, guide buttons, benchmark input lock, Time speed on Today",
     "1) The Atom Lab benchmark scenes no longer pick up your own Atom Lab settings: they start from the lab's defaults, so \"Bohr 1913\" can no "
     "longer hide the nucleons (earlier results for the nucleus, one-nucleon and biggest-nucleus scenes taken with Bohr 1913 on are not valid). "
     "The result now says which settings each Atom Lab scene used. "
     "2) The two \"What am I even looking at?\" buttons in the Atom Lab panel no longer clash, so the red \"conflicting ID\" message is gone. "
     "3) While the benchmark runs, nothing but Cancel and Esc works: not the panels, the buttons at the top, the keys, nor the picture. "
     "The result says so. The window title also follows the lab that is on screen. "
     "4) \"Reset to today\" and the other Today buttons keep your Time speed; only Defaults puts it back."},
    {"0.9.2", "Alpha label, drop-down text fits",
     "The Orbit Lab's drop-downs (Sizes, Trails, Stars in the background, Constellations) show only the short name in the closed box, so "
     "it is never cut off at big Text sizes; the explanation is under each choice in the open list. "
     "The app is now labelled alpha: features are still being added. Beta will follow when everything planned for 1.0 is in, then the plain release."},
    {"0.9.1", "Sliders that look like sliders",
     "The Time speed and Zoom sliders have a clearly visible track and a bigger, easier-to-grab thumb in every theme, and their values "
     "are written above them instead of under the thumb. The \"Drag rotates the view\" button is no longer cut off."},
    {"0.9", "Keeps running while the window is moved or resized, a checked GPU timer, stress scenes",
     "The simulation and the drawing no longer freeze while you drag the window by its title bar or an edge (Windows pauses the app's own "
     "loop then; the app now draws from the window system's own refresh signal instead). The picture and the panels follow the new size "
     "while you drag, and after you let go nothing jumps or catches up. The Performance card counts the frames drawn during a drag. "
     "The benchmark's GPU time now covers the whole frame's drawing, and the timer is checked with a known load before the test: if it "
     "cannot be trusted, the result says \"not reliable\" instead of a number. A checkbox \"Include stress scenes\" adds two much "
     "heavier scenes (the biggest nucleus, and eight times the belt rocks) after the four normal ones."},
    {"0.8", "Benchmark, history with the native updates on top",
     "A Benchmark button in the Performance card (both labs): it runs four fixed scenes (the heaviest Orbit Lab view, the uranium-238 "
     "nucleus, Cloud 3D, one nucleon) with VSync off at the window's size, 2 s of warm-up and 8 s counted each, and shows the average "
     "frame time and fps, the 1 % low, the worst frame, the CPU work and the GPU time (where the driver gives it). The result is "
     "copied to the clipboard and saved as a text file next to the saved pictures, with the graphics card, the OpenGL version and the "
     "window size on top. It gives all your settings back, and Esc stops it. The 🕰 history now lists the native updates first, then "
     "the web's own history with its note."},
    {"0.7", "fps counter in both labs, the window remembers itself, icon",
     "The frames-per-second counter is now in the Atom Lab too (it was only in the Orbit Lab): top right of the picture, in the "
     "web's small see-through rounded box, in every theme and text size, and it is drawn in one place for every lab. It is not in "
     "a saved picture. The window opens with the size and place it had last time, and maximised if it was; if the place is no "
     "longer on a screen it opens centred, and it never starts smaller than 800 x 500. F11 turns full screen on and off. "
     "PhysicsLab.exe has an icon (an atom) on the file, in the taskbar and in the window. If the window or OpenGL 3.3 cannot "
     "start, a message box now says what is wrong and suggests updating the graphics driver."},
    {"0.6", "Save image, timeline pictures, cleaner text, small windows",
     "📷 Save image, next to Defaults in both labs, saves exactly the picture you see (the readout included, in every theme, "
     "grey in Monochrome) as a PNG in your Pictures folder (Downloads if that fails), named with the lab and the date and "
     "time. The button says Saved ✓ and the path is shown under it; if saving fails it says so. The Orbit Lab's timeline "
     "now has the web's small step pictures, like the Atom Lab's. The words that name the Black Hole Lab, the Spacetime Lab "
     "and the slingshot game are taken out of About, the Units note, the Sagittarius A* card and the guides, and the dead "
     "\"Try the slingshot game\" button is gone. Small windows: long button labels wrap instead of being cut off, and the "
     "Settings window and the big periodic table never get bigger than the window."},
    {"0.5", "The web app's own features: settings, themes, header, history",
     "Settings as in the web version: text size (Small, Normal, Large, Extra large), font (Theme default, Serif, Easy to read, "
     "Rounded, Monospace; Atkinson Hyperlegible and Nunito are built in, under the SIL Open Font License), units, motion (Auto "
     "follows Windows' animation setting; Reduced starts the labs paused, makes camera moves jump, stops Cloud 3D spinning), "
     "Reset my progress with the web's two pop-ups, and Developer > Unlock everything. The 🎨 Theme menu has the web's eleven "
     "themes, including the secret Synthwave and Monochrome; the panels, cards, text and pictures all follow it. Everything is "
     "remembered between runs in the app-data folder: the look, units, unlocks and every setting of both labs. A header across "
     "the top has the web's buttons: 🎨 Theme, 🌐 language (English; Eesti keel is coming), 👤 About (the web's page with its "
     "sources), ⚙ Settings, 🕰 history (the web's history, and this list) and the lab's Update badge with its changelog. "
     "Under it, the 💡 Did you know? line with all the web's facts (tap for the next one; it waits while the mouse is on it) "
     "and its hidden 🎁 surprise, and the tabs: 📚 Learn lists every guide and timeline of both labs and opens them in their "
     "lab; 🏆 Achievements shows Neon Nights, Sharp Eyes, Half-Life and Galactic Center, locked until earned, with the "
     "web's pop-up when one is earned. Point of No Return is left out: it belongs to the Black Hole Lab, which is not in "
     "the native app. The \"monochrome\" word in the Atom Lab guide unlocks the Monochrome theme and Sharp Eyes. "
     "Also: the Sun has the web's soft glowing patches that turn with it, antiquark rings in Cinematic add up their light "
     "as on the web, and the Atom Lab readout lost faint grey marks it had in light themes."},
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

// The stage word shown after the version number (change this one word to move from alpha to beta, then "" for 1.0)
constexpr const char* kStage = "alpha";

const char* appVersion() { return PL_VERSION; }
const char* appStage() { return kStage; }
std::string appVersionLabel() { return *kStage ? std::string(PL_VERSION) + " " + kStage : std::string(PL_VERSION); }

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
    const std::string title = (lab == 1 ? std::string(app::kAtomHistoryName) + " " + L.update : std::string(L.name) + app::kLogTitleTail) + "###labLog";
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
