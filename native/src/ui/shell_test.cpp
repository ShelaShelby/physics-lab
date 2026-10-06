#include "ui/shell_test.h"
#include "atom/chart_draw.h"

#include <SDL3/SDL.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "imgui.h"
#include "atom/atom_view.h"
#include "ui/orbit_actions.h"
#include "ui/orbit_panel.h"

namespace ui {

namespace {

struct Rect4 { float x = 0, y = 0, w = 0, h = 0; };
bool g_collect = false;
std::set<std::string> g_panelItems;                       // the names of the Controls panel's controls
Rect4 g_scroll;                                          // the Controls panel's window (0 when it is not shown)
std::string g_reveal;
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

// The Atom Lab's state for the old-against-new comparison (check_atom_toolbar.py): what the toolbar's buttons change
std::string atomStateLine(const AppState& app) {
    const atom::AtomLab& L = app.atom;
    char b[640];
    std::snprintf(b, sizeof b, "view=%s layer=%s zoom=%.6g zoomTo=%d focus=%d drag=%s paused=%d yaw=%.6g pitch=%.6g oyaw=%.6g opitch=%.6g ctilt=%.6g czoom=%.6g Z=%d N=%d usualN=%d seed=%d big=%d",
                  L.p.view.c_str(), L.s.layer.c_str(), L.p.zoom, L.s.zoomTo ? 1 : 0, L.s.focus, L.s.dragMode.c_str(), app.paused ? 1 : 0, L.s.yaw, L.s.pitch, L.s.oyaw, L.s.opitch, L.s.chartView.tilt, L.s.chartView.zoom,
                  L.p.Z, L.p.N, atom::defaultN(L.p.Z), L.s.nucSeed, app.bigTable ? 1 : 0);
    return b;
}

// 0.15 stage 5d: every Atom Lab setting the Controls panel can change (tab-separated key=value; the values have spaces), for the old-against-new comparison (check_atom_panel.py)
std::string atomSettingsLine(const AppState& app) {
    const atom::AtomLab& L = app.atom;
    const atom::AtomParams& p = L.p;
    std::string t;
    const auto add = [&t](const char* k, const std::string& v) { t += std::string(k) + "=" + v + "\t"; };
    const auto num = [](double v) { char b[32]; std::snprintf(b, sizeof b, "%.6g", v); return std::string(b); };
    add("view", p.view); add("orb", p.orb); add("pAx", p.pAx); add("dAx", p.dAx); add("fAx", p.fAx); add("n", num(p.n)); add("speed", num(p.speed));
    add("cstyle", p.cstyle); add("cdens", p.cdens); add("super", p.super); add("orb2", p.orb2); add("pAx2", p.pAx2); add("dAx2", p.dAx2); add("fAx2", p.fAx2); add("mix", num(p.mix));
    add("cutAxis", p.cutAxis); add("cut", num(p.cut)); add("ostyle", p.ostyle); add("scale", p.scale); add("zoom", num(p.zoom)); add("spin", num(L.s.spin));
    add("qstyle", p.qstyle); add("detail", p.detail); add("style", p.style); add("tilt", num(L.s.chartView.tilt)); add("cheight", L.s.chartView.height == atom::ChartHeightMode::Binding ? "Binding energy per nucleon" : "Half-life");
    add("outline", p.outline); add("phase", p.phase); add("chain", num(L.s.showChain)); add("walkAuto", num(L.s.walkAuto)); add("walkSteps", num(static_cast<double>(L.s.walk.steps.size())));
    add("jfrom", p.jfrom); add("jto", p.jto); add("auto", num(L.s.autoPhotons)); add("jumping", num(L.s.jumping)); add("Z", num(p.Z)); add("N", num(p.N));
    // (stage 5e) what a photon click started (the words of the last photon: emitted / absorbed, and from which level) and what the picture drew for the marks (atom/atom_view.h)
    add("last", L.s.last); add("pending", L.s.photon ? L.s.photon->kind : L.s.photonAfter ? L.s.photonAfter->kind : std::string("none"));
    const atom::MarksDrawn& d = atom::marksDrawn();
    add("drawnOutline", d.outline); add("drawnOrb", num(d.outlineOrb)); add("drawnMass", num(d.outlineMass)); add("drawnReach", num(d.outlineReach)); add("drawnSegs", num(static_cast<double>(d.outlineSegs)));
    add("drawnLegend", num(atom::chartLegendDrawn())); add("drawnGroups", num(d.phaseGroups)); add("drawnWheel", num(d.wheel)); add("drawnChainNodes", num(d.chainNodes)); add("drawnChainEdges", num(d.chainEdges));
    return t;
}

}  // namespace

void noteItem(const char* label) {
    if (!scriptText() && !shellLogOn()) return;
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    g_items[keyOf(label)] = {a.x, a.y, b.x - a.x, b.y - a.y};
    if (g_collect) g_panelItems.insert(keyOf(label));
}
void collectPanelItems(bool on) { g_collect = on; }
void noteScrollArea(float x, float y, float w, float h) { g_scroll = {x, y, w, h}; }
bool takeReveal(std::string& name) { if (g_reveal.empty()) return false; name = g_reveal; g_reveal.clear(); return true; }
void shiftItem(const std::string& name, float dy) { const auto it = g_items.find(name); if (it != g_items.end()) it->second.y += dy; }
bool itemRect(const std::string& name, float out[4]) {
    const auto it = g_items.find(name);
    if (it == g_items.end()) return false;
    out[0] = it->second.x; out[1] = it->second.y; out[2] = it->second.w; out[3] = it->second.h;
    return true;
}
void noteRect(const char* name, float x, float y, float w, float h) {
    if (!scriptText() && !shellLogOn()) return;
    g_items[name] = {x, y, w, h};
    if (g_collect) g_panelItems.insert(name);             // (stage 5d: a part of a segmented button in the Controls panel is scrolled in like the other controls)
}

void runScript(AppState& app, int frame, SDL_Window* window) {
    static const std::vector<Cmd> cmds = parseScript();
    // a click is: the pointer arrives (so Dear ImGui knows it is over a control), the press on the next frame, the release on the one after
    struct Step { float x, y; int what; };
    static std::map<int, std::vector<Step>> pending;
    if (auto it = pending.find(frame); it != pending.end()) { for (const Step& st : it->second) pushMouse(window, st.x, st.y, st.what); pending.erase(it); }
    static std::vector<Cmd> later;                          // clicks held back until the Controls panel has scrolled the control into view
    std::vector<Cmd> all = cmds;
    all.insert(all.end(), later.begin(), later.end());
    for (const Cmd& c : all) {
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
                if (it != g_items.end() && verb == "click" && g_scroll.w > 0 && g_panelItems.count(name)) {       // in the Controls panel but scrolled out of view: scroll it in, click later
                    const Rect4& r0 = it->second;
                    const float cy = r0.y + r0.h / 2;
                    if (cy < g_scroll.y || cy > g_scroll.y + g_scroll.h) {
                        g_reveal = name;
                        later.push_back({frame + 1, "click=" + arg});
                        continue;
                    }
                }
                if (it != g_items.end()) {
                    const Rect4& r = it->second;
                    y = r.y + r.h / 2;
                    if (frac >= 0) { const float grab = ImGui::GetStyle().GrabMinSize, pad = 2.0f; x = r.x + pad + grab / 2 + float(frac) * (r.w - 2 * pad - grab); }   // a slider (Dear ImGui's and the new UI's): the grab's centre is where the value is
                    else x = r.x + r.w / 2;
                    found = true;
                } else std::printf("PLSCRIPT frame %d: no control called \"%s\"\n", frame, name.c_str());
            }
            if (found) { pushMouse(window, x, y, 0); pending[frame + 1].push_back({x, y, 1}); pending[frame + 2].push_back({x, y, 2}); }
        } else if (verb == "move") {                       // the pointer goes to a named control's middle (or to x,y) and stays there: hover
            float x = 0, y = 0;
            bool found = false;
            const auto it = g_items.find(arg);
            if (it != g_items.end()) { x = it->second.x + it->second.w / 2; y = it->second.y + it->second.h / 2; found = true; }
            else found = std::sscanf(arg.c_str(), "%f,%f", &x, &y) == 2;
            if (found) pushMouse(window, x, y, 0); else std::printf("PLSCRIPT frame %d: no control called \"%s\"\n", frame, arg.c_str());
        } else if (verb == "wheel") {                      // the mouse wheel turns by n notches (negative: down) at the pointer
            SDL_Event e;
            SDL_zero(e);
            e.type = SDL_EVENT_MOUSE_WHEEL; e.wheel.windowID = SDL_GetWindowID(window); e.wheel.y = static_cast<float>(std::atof(arg.c_str())); e.wheel.mouse_x = ImGui::GetIO().MousePos.x; e.wheel.mouse_y = ImGui::GetIO().MousePos.y;
            SDL_PushEvent(&e);
        } else if (verb == "drag") {                       // press at x0,y0, move to x1,y1 over a few frames, release (the picture's drag)
            float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
            if (std::sscanf(arg.c_str(), "%f,%f,%f,%f", &x0, &y0, &x1, &y1) == 4) {
                pushMouse(window, x0, y0, 0);
                pending[frame + 1].push_back({x0, y0, 1});
                for (int k = 1; k <= 4; k++) pending[frame + 1 + k].push_back({x0 + (x1 - x0) * k / 4, y0 + (y1 - y0) * k / 4, 0});
                pending[frame + 6].push_back({x1, y1, 2});
            }
        } else if (verb == "key") {
            std::string k = arg;
            SDL_Keymod mod = SDL_KMOD_NONE;
            if (k.rfind("ctrl+", 0) == 0) { mod = SDL_KMOD_LCTRL; k = k.substr(5); }
            SDL_Keycode key = SDLK_UNKNOWN; SDL_Scancode sc = SDL_SCANCODE_UNKNOWN;
            if (k == "escape") { key = SDLK_ESCAPE; sc = SDL_SCANCODE_ESCAPE; }
            else if (k == "space") { key = SDLK_SPACE; sc = SDL_SCANCODE_SPACE; }
            else if (k == "enter") { key = SDLK_RETURN; sc = SDL_SCANCODE_RETURN; }
            else if (k == "down") { key = SDLK_DOWN; sc = SDL_SCANCODE_DOWN; }
            else if (k == "up") { key = SDLK_UP; sc = SDL_SCANCODE_UP; }
            else if (k == "pagedown") { key = SDLK_PAGEDOWN; sc = SDL_SCANCODE_PAGEDOWN; }
            else if (k == "pageup") { key = SDLK_PAGEUP; sc = SDL_SCANCODE_PAGEUP; }
            else if (k == "home") { key = SDLK_HOME; sc = SDL_SCANCODE_HOME; }
            else if (k == "end") { key = SDLK_END; sc = SDL_SCANCODE_END; }
            else if (k == "left") { key = SDLK_LEFT; sc = SDL_SCANCODE_LEFT; }
            else if (k == "right") { key = SDLK_RIGHT; sc = SDL_SCANCODE_RIGHT; }
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
        } else if (verb == "astate") {
            std::printf("PLASTATE %s\n", atomSettingsLine(app).c_str());
            { const atom::MarksDrawn& d = atom::marksDrawn(); std::printf("PLATOMBOX info=%g,%g,%g,%g ring=%g,%g,%g,%g\n", d.info[0], d.info[1], d.info[2], d.info[3], d.ring[0], d.ring[1], d.ring[2], d.ring[3]); }   // (stage 5g: where the picture put its info text and phase ring, in points)
        } else if (verb == "state") {
            std::printf("PLSTATE %s\n", (app.lab == 1 ? atomStateLine(app) : orbitStateLine(app)).c_str());
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
namespace { std::map<std::string, std::array<float, 8>> g_halos; }
void auditHalo(const std::string& name, ImVec2 a, ImVec2 b, ImVec2 oa, ImVec2 ob) {
    if (!shellLogOn()) return;
    g_halos[name] = {a.x, a.y, b.x, b.y, oa.x, oa.y, ob.x, ob.y};
}
void auditPictureText(float px) { if (shellLogOn()) auditText("plabel", px, "a label of the picture"); }   // (one line per size)
void auditFrame() { if (!shellLogOn()) return; g_rects.clear(); g_notes.clear(); }
void auditNote(const std::string& line) {
    if (!shellLogOn()) return;
    const std::size_t sp = line.find(' ');
    std::string key = line.substr(0, sp);
    if (line.rfind("cardtitle", 0) == 0 || line.rfind("panelopen", 0) == 0) key += line.substr(sp);
    else if (line.rfind("panelneed", 0) == 0 && sp != std::string::npos) key += line.substr(sp, line.find(' ', sp + 1) - sp);      // one line per panel
    g_notes[key] = line;
}

void shellReport(int winW, int winH, float dpi, float textScale) {
    if (!shellLogOn()) return;
    std::printf("PLSHELL window w=%d h=%d dpi=%.7f textScale=%.7f\n", winW, winH, dpi, textScale);
    for (const auto& [name, r] : g_rects) std::printf("PLSHELL rect name=%s x=%.1f y=%.1f w=%.1f h=%.1f\n", name.c_str(), r.x, r.y, r.w, r.h);
    for (const auto& [k, line] : g_notes) std::printf("PLSHELL %s\n", line.c_str());
    for (const std::string& t : g_texts) std::printf("PLSHELL text %s\n", t.c_str());
    for (const auto& [name, v] : g_halos) std::printf("PLSHELL halo name=%s a=%.1f,%.1f,%.1f,%.1f o=%.1f,%.1f,%.1f,%.1f\n", name.c_str(), v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
}

}  // namespace ui#include "atom/atom_view.h"

