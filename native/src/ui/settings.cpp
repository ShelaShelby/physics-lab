#include "settings.h"

#include <SDL3/SDL.h>

#include <chrono>
#include <cstdio>
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

void saveSettings() {
    const std::string p = settingsPath();
    if (p.empty()) return;
    if (FILE* f = std::fopen(p.c_str(), "w")) {
        std::fprintf(f, "units=%s\n", fmt::imperial() ? "imperial" : "metric");
        std::fclose(f);
    }
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

void loadSettings(AppState&) {
    const std::string p = settingsPath();
    if (p.empty()) return;
    if (FILE* f = std::fopen(p.c_str(), "r")) {
        char line[128];
        while (std::fgets(line, sizeof line, f))
            if (std::string(line).rfind("units=imperial", 0) == 0) fmt::setImperial(true);
        std::fclose(f);
    }
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
        richText("<p class=\"lrnP lrnMuted\">Switches the distances, speeds and temperatures that the lab works out for you "
                 "(planet, moon and comet cards, the scale bar, and the live numbers in the seasons guide). "
                 "Science units such as AU, K and kg, and the written guides, stay as they are. "
                 "Inside, the lab always works in metres and seconds; only the words on screen change.</p>");
        ImGui::Spacing();
        richHeading("Orbit Lab");
        defaultsButton(app);
        ImGui::TextDisabled("Puts this lab's settings back to how they started.");
    }
    ImGui::End();
}

void defaultsButton(AppState& app) {
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
