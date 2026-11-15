// 0.15 stage 5k: the hidden input log. Ctrl+Shift+F12 writes `physics-lab-input.txt` into the folder where the app keeps its settings and shows a toast with the full path (no menu entry,
// no setting). It says what the mouse wheel, the keys and Dear ImGui's hover / focus were doing, so a "the page does not scroll" report on a real PC can be read.
#pragma once

#include <string>

namespace ui {

struct AppState;

void inputNoteWheel(float value, float x, float y);   // every mouse-wheel event the app receives (the last 20 are kept)
void inputNoteKey(int sdlKey, bool down);              // every key event (the last 20 are kept)
// Writes the file; returns its full path, or "" when it could not be written. `now` is a text for the time line.
std::string inputLogWrite(const AppState& app);

}  // namespace ui
