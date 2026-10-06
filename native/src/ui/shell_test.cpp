#include "ui/shell_test.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "imgui.h"
#include "ui/orbit_actions.h"
#include "ui/orbit_panel.h"

namespace ui {

namespace {

struct Rect4 { float x = 0, y = 0, w = 0, h = 0; };
std::map<std::string, Rect4> g_items;                   // name -> where it was last frame (window pixels)
std::set<std::string> g_texts;                          // "kind px text" lines, each once
std::map<std::string, Rect4> g_rects;
std::map<std::string, std::string> g_notes;

const char* scriptText() { static const char* s = std::getenv("PL_SCRIPT"); return s; }

std::string keyOf(const char* label) {                  // "Moons###Moons", "##tspeed", "Real##x" -> the name a script uses
    const std::string l = label;
    const std::size_t t = l.find("###");
    if (t != std::string::npos) return l.substr(t + 3);
    if (l.rfind("##", 0) == 0) return l;
    const std::size_t h = l.find("##");
    return h == std::string::npos ? l : l.substr(0, h);
}

struct Cmd { int frame; std::string what; };
std::vector<Cmd> parseScript() {
    std::vector<Cmd> out;
    const char* s = scriptText();
    if (!s) return out;
    std::string all = s;
    std::size_t a = 0;
    while (a <= all.size()) {
        std::size_t b = all.find(';', a);
        if (b == std::string::npos) b = all.size();
        const std::string item = all.substr(a, b - a);
        a = b + 1;
        const std::size_t c = item.find(':');
        if (c == std::string::npos || c == 0) continue;
        out.push_back({std::atoi(item.substr(0, c).c_str()), item.substr(c + 1)});
    }
    return out;
}

void pushMouse(SDL_Window* w, float x, float y, int what) {   // 0 move, 1 down, 2 up
    SDL_Event e;
    SDL_zero(e);
    if (what == 0) { e.type = SDL_EVENT_MOUSE_MOTION; e.motion.windowID = SDL_GetWindowID(w); e.motion.x = x; e.motion.y = y; }
    else { e.type = what == 1 ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP; e.button.windowID = SDL_GetWindowID(w); e.button.x = x; e.button.y = y; e.button.button = SDL_BUTTON_LEFT; e.button.down = what == 1; e.button.clicks = 1; }
    SDL_PushEvent(&e);
}

void pushKey(SDL_Window* w, SDL_Keycode key, SDL_Scancode sc, bool down, SDL_Keymod mod) {
    SDL_Event e;
    SDL_zero(e);
    e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    e.key.windowID = SDL_GetWindowID(w); e.key.key = key; e.key.scancode = sc; e.key.mod = mod; e.key.down = down;
    SDL_PushEvent(&e);
}

}  // namespace

void noteItem(const char* label) {
    if (!scriptText() && !shellLogOn()) return;
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    g_items[keyOf(label)] = {a.x, a.y, b.x - a.x, b.y - a.y};
}
void noteRect(const char* name, float x, float y, float w, float h) {
    if (!scriptText() && !shellLogOn()) return;
    g_items[name] = {x, y, w, h};
}

void runScript(AppState& app, int frame, SDL_Window* window) {
    static const std::vector<Cmd> cmds = parseScript();
    // a click is: the pointer arrives (so Dear ImGui knows it is over a control), the press on the next frame, the release on the one after
    struct Step { float x, y; int what; };
    static std::map<int, std::vector<Step>> pending;
    if (auto it = pending.find(frame); it != pending.end()) { for (const Step& st : it->second) pushMouse(window, st.x, st.y, st.what); pending.erase(it); }
    for (const Cmd& c : cmds) {
        if (c.frame != frame) continue;
        const std::string& w = c.what;
        const std::size_t eq = w.find('=');
        const std::string verb = w.substr(0, eq), arg = eq == std::string::npos ? "" : w.substr(eq + 1);
        if (verb == "click" || verb == "clickxy") {
            float x = 0, y = 0;
            bool found = false;
            if (verb == "clickxy") found = std::sscanf(arg.c_str(), "%f,%f", &x, &y) == 2;
            else {
                std::string name = arg;
                double frac = -1;
                const std::size_t at = name.rfind('@');
                if (at != std::string::npos) { frac = std::atof(name.substr(at + 1).c_str()); name = name.substr(0, at); }
                const auto it = g_items.find(name);
                if (it != g_items.end()) {
                    const Rect4& r = it->second;
                    y = r.y + r.h / 2;
                    if (frac >= 0) { const float grab = ImGui::GetStyle().GrabMinSize, pad = 2.0f; x = r.x + pad + grab / 2 + float(frac) * (r.w - 2 * pad - grab); }   // a slider (Dear ImGui's and the new UI's): the grab's centre is where the value is
                    else x = r.x + r.w / 2;
                    found = true;
                } else std::printf("PLSCRIPT frame %d: no control called \"%s\"\n", frame, name.c_str());
            }
            if (found) { pushMouse(window, x, y, 0); pending[frame + 1].push_back({x, y, 1}); pending[frame + 2].push_back({x, y, 2}); }
        } else if (verb == "key") {
            std::string k = arg;
            SDL_Keymod mod = SDL_KMOD_NONE;
            if (k.rfind("ctrl+", 0) == 0) { mod = SDL_KMOD_LCTRL; k = k.substr(5); }
            SDL_Keycode key = SDLK_UNKNOWN; SDL_Scancode sc = SDL_SCANCODE_UNKNOWN;
            if (k == "escape") { key = SDLK_ESCAPE; sc = SDL_SCANCODE_ESCAPE; }
            else if (k == "space") { key = SDLK_SPACE; sc = SDL_SCANCODE_SPACE; }
            else if (k == "enter") { key = SDLK_RETURN; sc = SDL_SCANCODE_RETURN; }
            else if (k.size() == 1) { key = static_cast<SDL_Keycode>(k[0]); sc = static_cast<SDL_Scancode>(SDL_SCANCODE_A + (k[0] - 'a')); }
            if (mod) pushKey(window, SDLK_LCTRL, SDL_SCANCODE_LCTRL, true, mod);
            pushKey(window, key, sc, true, mod);
            pushKey(window, key, sc, false, mod);
            if (mod) pushKey(window, SDLK_LCTRL, SDL_SCANCODE_LCTRL, false, SDL_KMOD_NONE);
        } else if (verb == "text") {
            SDL_Event e;
            SDL_zero(e);
            e.type = SDL_EVENT_TEXT_INPUT; e.text.windowID = SDL_GetWindowID(window);
            static std::string keep; keep = arg;
            e.text.text = keep.c_str();
            SDL_PushEvent(&e);
        } else if (verb == "state") {
            std::printf("PLSTATE %s\n", orbitStateLine(app).c_str());
        }
    }
}

bool shellLogOn() { static const bool on = std::getenv("PL_LOG_SHELL") != nullptr; return on; }

void auditText(const char* kind, float px, const std::string& text) {
    if (!shellLogOn()) return;
    char b[64];
    std::snprintf(b, sizeof b, "kind=%s px=%.2f t=", kind, px);
    g_texts.insert(std::string(b) + text);
}
void auditRect(const std::string& name, float x, float y, float w, float h) {
    if (!shellLogOn()) return;
    g_rects[name] = {x, y, w, h};
}
void auditPictureText(float px) { if (shellLogOn()) auditText("plabel", px, "a label of the picture"); }   // (one line per size)
void auditFrame() { if (!shellLogOn()) return; g_rects.clear(); g_notes.clear(); }
void auditNote(const std::string& line) {
    if (!shellLogOn()) return;
    g_notes[line.substr(0, line.find(' '))+(line.rfind("cardtitle",0)==0||line.rfind("panelopen",0)==0 ? line.substr(line.find(' ')) : std::string())] = line;
}

void shellReport(int winW, int winH, float dpi, float textScale) {
    if (!shellLogOn()) return;
    std::printf("PLSHELL window w=%d h=%d dpi=%.7f textScale=%.7f\n", winW, winH, dpi, textScale);
    for (const auto& [name, r] : g_rects) std::printf("PLSHELL rect name=%s x=%.1f y=%.1f w=%.1f h=%.1f\n", name.c_str(), r.x, r.y, r.w, r.h);
    for (const auto& [k, line] : g_notes) std::printf("PLSHELL %s\n", line.c_str());
    for (const std::string& t : g_texts) std::printf("PLSHELL text %s\n", t.c_str());
}

}  // namespace ui
