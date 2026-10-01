// 📷 Save image, native side: the button (next to Defaults) and the saving of the picture area as a PNG.
#pragma once

#include "render/renderer2d.h"
#include "ui/orbit_panel.h"

namespace ui {

// The button, with the web's label and tooltip. sameLine: put it on the previous line if it fits.
void shotButton(AppState& app, bool sameLine);
// The line under the buttons for a few seconds after a save: where the file is, or why it was not saved.
void shotNote(AppState& app);

// What the picture area of the window is (pixels, and window coordinates), for takeShot.
struct ShotArea {
    int fbW, fbH;                  // the window's framebuffer
    int winW, winH;                // the window, in window coordinates
    int pictureFbW, pictureFbH;    // the picture area's size in pixels: the PNG has exactly this size
    float headerH;                 // the picture area's top edge, window coordinates
};

// Draws this frame's picture again into an offscreen image (the renderer's shapes, then the text the lab drew
// on the background list, the readout included, but no panels or windows), and writes it as a PNG into the
// Pictures folder (Downloads if that fails). Sets the message on app.shot. Call after the frame was drawn.
void takeShot(AppState& app, render::Renderer2D& renderer, const ShotArea& a);

// The folders to try, best first: the Pictures folder, then Downloads. PL_SHOT_DIR overrides them (for automatic checks).
std::vector<std::string> shotFolders();

}  // namespace ui
