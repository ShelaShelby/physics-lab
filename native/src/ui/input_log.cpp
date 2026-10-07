#include "ui/input_log.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"
#include "ui/look.h"
#include "ui/orbit_panel.h"
#include "ui/reading.h"
#include "ui/shell.h"
#include "ui/shell_test.h"

namespace ui {

namespace {
struct Wheel { double t; float v, x, y; };
struct Key { double t; int key; bool down; };
std::deque<Wheel> g_wheels;
std::deque<Key> g_keys;
const char* winName(const ImGuiWindow* w) { return w ? w->Name : "(none)"; }
}  // namespace

void inputNoteWheel(float value, float x, float y) { g_wheels.push_back({ImGui::GetTime(), value, x, y}); if (g_wheels.size() > 20) g_wheels.pop_front(); }
void inputNoteKey(int sdlKey, bool down) { g_keys.push_back({ImGui::GetTime(), sdlKey, down}); if (g_keys.size() > 20) g_keys.pop_front(); }

std::string inputLogWrite(const AppState& app) {
    char* dir = SDL_GetPrefPath("PhysicsLab", "PhysicsLab");
    if (!dir) return "";
    const std::string path = std::string(dir) + "physics-lab-input.txt";
    SDL_free(dir);
    char b[512];
    std::string s = "Physics Lab input log (Ctrl+Shift+F12)\n";
    const std::time_t t = std::time(nullptr);
    char tb[64];
    std::strftime(tb, sizeof tb, "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    s += std::string("time: ") + tb + "\n";
    ImGuiContext& c = *GImGui;
    const ImGuiIO& io = ImGui::GetIO();
    std::snprintf(b, sizeof b, "app window: %.0f x %.0f, theme: %s, motion: %s, new UI: %s\n", io.DisplaySize.x, io.DisplaySize.y, app.prefs.theme.c_str(), app.prefs.motion.c_str(), newUiOn(app) ? "on" : "off");
    s += b;
    s += std::string("hovered window: ") + winName(c.HoveredWindow) + "\n";
    s += std::string("focused window: ") + winName(c.NavWindow) + "\n";
    std::snprintf(b, sizeof b, "WantCaptureMouse=%d WantCaptureKeyboard=%d WantTextInput=%d\n", io.WantCaptureMouse, io.WantCaptureKeyboard, io.WantTextInput);
    s += b;
    s += readingDiag();
    s += "last wheel events (value, pointer x,y, seconds ago):\n";
    for (const Wheel& w : g_wheels) { std::snprintf(b, sizeof b, "  wheel %.2f at %.0f,%.0f  %.1f s ago\n", w.v, w.x, w.y, ImGui::GetTime() - w.t); s += b; }
    if (g_wheels.empty()) s += "  (none received)\n";
    s += "last key events (key name, down/up, seconds ago):\n";
    for (const Key& k : g_keys) { std::snprintf(b, sizeof b, "  %s %s  %.1f s ago\n", SDL_GetKeyName(static_cast<SDL_Keycode>(k.key)), k.down ? "down" : "up", ImGui::GetTime() - k.t); s += b; }
    if (g_keys.empty()) s += "  (none received)\n";
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return "";
    std::fwrite(s.data(), 1, s.size(), f);
    std::fclose(f);
    return path;
}

}  // namespace ui
