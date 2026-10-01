#include "changelog.h"

#include <string>

#include "imgui.h"

namespace ui {

namespace {

struct Entry { const char* version; const char* title; const char* text; };

// The native app's own history. Newest first.
const Entry kNative[] = {
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
