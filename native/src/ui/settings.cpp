#include "settings.h"

#include <SDL3/SDL.h>

#include <chrono>
#include <cstdio>
#include <optional>
#include <string>

#include "fonts.h"
#include "imgui.h"
#include "orbit/format.h"
#include "rich_text.h"
#include "ui/journey_dates.h"
#include "ui/orbit_panel.h"

namespace ui {

namespace {

// the settings file, in the user's own app-data folder (Windows: %APPDATA%\PhysicsLab\PhysicsLab\)
std::string settingsPath() {
    char* dir = SDL_GetPrefPath("PhysicsLab", "PhysicsLab");
    if (!dir) return "";
    std::string p = std::string(dir) + "settings.txt";
    SDL_free(dir);
    return p;
}

// the Atom Lab's ⓘ More / Less info choice, once made (the web keeps it too)
std::optional<bool> g_atomMore;

void saveFile() {
    const std::string p = settingsPath();
    if (p.empty()) return;
    if (FILE* f = std::fopen(p.c_str(), "w")) {
        std::fprintf(f, "units=%s\n", fmt::imperial() ? "imperial" : "metric");
        if (g_atomMore) std::fprintf(f, "atomMore=%d\n", *g_atomMore ? 1 : 0);
        std::fclose(f);
    }
}
void saveSettings() { saveFile(); }

// Atom Lab back to how it starts: every setting at its default and a fresh atom (the web's load(lab, fresh));
// the More / Less info choice and the Temperature slider are kept, as on the web
void putBackAtomDefaults(AppState& app) {
    app.atom.p = atom::AtomParams{};
    app.atom.restart();
    app.atomLearn = 0;
}

// Orbit Lab back to how it starts: today's Solar System, the starting view and every setting at its default
void putBackDefaults(AppState& app) {
    journeyStop(app);
    app.set = orbit::ViewSettings{};
    app.view = orbit::OrbitView{};
    app.dateNote.clear();
    app.learnJump.clear();
    resetToToday(app);
}

}  // namespace

void loadSettings(AppState& app) {
    const std::string p = settingsPath();
    if (p.empty()) return;
    if (FILE* f = std::fopen(p.c_str(), "r")) {
        char line[128];
        while (std::fgets(line, sizeof line, f)) {
            const std::string l = line;
            if (l.rfind("units=imperial", 0) == 0) fmt::setImperial(true);
            if (l.rfind("atomMore=", 0) == 0) g_atomMore = l[9] == '1';
        }
        std::fclose(f);
    }
    app.atom.moreStored = g_atomMore;
    app.atom.s.more = g_atomMore;
}

void saveAtomMore(AppState& app) {
    if (app.atom.moreStored == g_atomMore) return;
    g_atomMore = app.atom.moreStored;
    saveFile();
}

void settingsWindow(AppState& app) {
    if (!app.showSettings) return;
    ImGui::SetNextWindowSize(ImVec2(28 * ImGui::GetFontSize(), 0), ImGuiCond_Appearing);
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.4f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::Begin("Settings", &app.showSettings, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        richHeading(displayable("📏 Units"));
        const float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
        const auto option = [&](const char* name, const char* sub, bool on) {
            if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            const bool clicked = ImGui::Button((std::string(name) + "\n" + sub).c_str(), ImVec2(w, 0));
            if (on) ImGui::PopStyleColor();
            return clicked;
        };
        if (option("Metric", "km · m/s · °C", !fmt::imperial())) { fmt::setImperial(false); saveSettings(); }
        ImGui::SameLine();
        if (option("Imperial", "miles · mph · °F", fmt::imperial())) { fmt::setImperial(true); saveSettings(); }
        if (app.lab == 1)
            richText("<p class=\"lrnP lrnMuted\">Switches the temperatures, speeds and everyday lengths that the lab works out for you "
                     "(melting and boiling points, room temperature in the facts, the typical atom speed, the notes on where you meet "
                     "each element, and the sizes in the Scale guide). Science units such as Å, pm, fm, eV, K and kg stay as they are. "
                     "Inside, the lab always works in metres and seconds; only the words on screen change.</p>");
        else
            richText("<p class=\"lrnP lrnMuted\">Switches the distances, speeds and temperatures that the lab works out for you "
                     "(planet, moon and comet cards, the scale bar, and the live numbers in the seasons guide). "
                     "Science units such as AU, K and kg, and the written guides, stay as they are. "
                     "Inside, the lab always works in metres and seconds; only the words on screen change.</p>");
        ImGui::Spacing();
        richHeading(app.lab == 1 ? "Atom Lab" : "Orbit Lab");
        defaultsButton(app);
        ImGui::TextDisabled("Puts this lab's settings back to how they started.");
    }
    ImGui::End();
}

void defaultsButton(AppState& app) {
    if (app.lab == 1) {
        if (ImGui::Button("Defaults")) ImGui::OpenPopup("Put Atom Lab back?");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Put this lab's settings back to how they started");
        ImGui::SetNextWindowSize(ImVec2(24 * ImGui::GetFontSize(), 0));
        if (ImGui::BeginPopupModal("Put Atom Lab back?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::TextWrapped("This puts every Atom Lab setting back to how it started: hydrogen-1, the Orbit view, level n = 1, "
                               "zoom, scale, quark style and the other choices, and starts a fresh atom (the decay history starts "
                               "again). Your units and More / Less info choices stay.");
            ImGui::Spacing();
            if (ImGui::Button("Yes, put them back")) { putBackAtomDefaults(app); ImGui::CloseCurrentPopup(); }
            ImGui::SameLine();
            if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
            ImGui::SetItemDefaultFocus();
            ImGui::EndPopup();
        }
        return;
    }
    if (ImGui::Button("Defaults")) ImGui::OpenPopup("Put Orbit Lab back?");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Put this lab's settings back to how they started");
    ImGui::SetNextWindowSize(ImVec2(24 * ImGui::GetFontSize(), 0));
    if (ImGui::BeginPopupModal("Put Orbit Lab back?", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextWrapped("This puts every Orbit Lab setting back to how it started: 2D view, zoom, Time speed, sizes, trails, "
                           "sky and extras, and the Solar System back to today. What you selected is let go. Your units choice stays.");
        ImGui::Spacing();
        if (ImGui::Button("Yes, put them back")) { putBackDefaults(app); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }
}

}  // namespace ui
