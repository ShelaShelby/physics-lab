// 0.15 stage 3: support for the automatic checks of the new UI (nothing here does anything in normal use; every function returns at once unless a PL_ switch is set).
//   PL_SCRIPT="frame:command;frame:command"   presses things at given frames, through the real input path (SDL events):
//       click=<name>[@fraction]   click the control called <name> (its rect from the last frame; for a slider @0.37 clicks at that part of its length)
//       clickxy=x,y   click at a window position        key=escape | ctrl+k | space        text=abc   type letters
//       drag=x0,y0,x1,y1   a left-button drag over a few frames;  rdrag=...  the same with the right button (0.20)
//       state         print one line of the Orbit Lab's state (PLSTATE ...)
//   PL_LOG_SHELL=1   at the end print what the new UI drew: the text sizes, the rectangles of the panels and bars, the picture's selected body (PLSHELL ...)
#pragma once

#include <string>

#include "imgui.h"

struct SDL_Window;

namespace ui {

struct AppState;

void noteItem(const char* label);                       // call right after a widget: remembers its rectangle under its label (for PL_SCRIPT click=)
void noteRect(const char* name, float x, float y, float w, float h);   // the same for a rectangle drawn by hand (window pixels)
// The Controls panel scrolls. It tells the checks where its window is (noteScrollArea); a scripted click on a control of it that is scrolled out of view first asks the panel to scroll it in
// (takeReveal / itemRect), and the click follows 1 frame later. Nothing here does anything without PL_SCRIPT.
void noteScrollArea(float x, float y, float w, float h);
void notePopupArea(float x, float y, float w, float h);   // 5q: the open dropdown panel's window; a scripted click on a control of it that is scrolled out of view scrolls it in first
bool takeRevealPopup(std::string& name);
void collectPanelItems(bool on);                        // between on and off, the controls noted belong to the Controls panel (only those are scrolled in)
bool takeReveal(std::string& name);                     // a scripted click wants this control shown: true once
bool itemRect(const std::string& name, float out[4]);
void shiftItem(const std::string& name, float dy);     // the panel scrolled by -dy: the control's remembered place moves with it, so the click that follows next frame lands on it   // where a noted control was last frame
void runScript(AppState& app, int frame, SDL_Window* window);          // called once per frame by main, before the events are read

bool shellLogOn();                                      // PL_LOG_SHELL is set
void auditText(const char* kind, float px, const std::string& text);   // a text the new UI drew: its kind (caption / body / title / key / h1 / h2 / h3) and its size in pixels
void auditRect(const std::string& name, float x, float y, float w, float h);
void auditHalo(const std::string& name, ImVec2 a, ImVec2 b, ImVec2 oa, ImVec2 ob);   // 0.15 stage 5c.1: a widget / panel (a..b) and how far its outline, glow or hover paint reaches (oa..ob); kept over frames   // a rectangle of the new UI (last frame wins)
void auditPictureText(float px);                        // a label the Orbit Lab picture drew in the new UI (kind plabel; one line per size)
void auditFrame();                                      // a new frame starts: forget last frame's rectangles (the texts are kept: every text size ever drawn)
void auditNote(const std::string& line);                // a free line (replaces an earlier line with the same first word)
void shellReport(int winW, int winH, float dpi, float textScale);      // prints it all (at exit)

}  // namespace ui
