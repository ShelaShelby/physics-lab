#include "ui/shot.h"
#include "ui/style_draw.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "app/app_data.h"
#include "app/shot.h"
#include "imgui.h"
#include "ui/fonts.h"
#include "ui/look.h"
#include "ui/shell_test.h"

namespace ui {

std::vector<std::string> shotFolders() {
    std::vector<std::string> out;
    if (const char* d = SDL_getenv("PL_SHOT_DIR")) { out.push_back(d); return out; }
    if (const char* p = SDL_GetUserFolder(SDL_FOLDER_PICTURES)) out.push_back(p);
    if (const char* p = SDL_GetUserFolder(SDL_FOLDER_DOWNLOADS)) out.push_back(p);
    return out;
}

namespace {

void report(AppState& app, bool ok, const std::string& note) {
    ShotUi& s = app.shot;
    s.failed = !ok;
    s.flash = ok ? app::kShotSaved : app::kShotFailed;
    s.flashUntil = ImGui::GetTime() + 1.8;
    s.note = note;
    s.noteUntil = ImGui::GetTime() + (ok ? 20.0 : 30.0);
}

}  // namespace

// the PNG into the first folder that takes it (Pictures, then Downloads); the message on app.shot
static void saveShot(AppState& app, int w, int h, const std::vector<uint8_t>& rgb) {
    const std::vector<uint8_t> png = app::encodePng(w, h, rgb.data());
    SDL_Time now = 0;
    SDL_DateTime dt{};
    SDL_GetCurrentTime(&now);
    SDL_TimeToDateTime(now, &dt, true);                // local time
    const std::string lab = app::labName(app.lab);
    const std::string name = app::shotFileName(lab, app.atom.p.Z, app.atom.p.N, {dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second});
    std::string tried, why;
    for (const std::string& folder : shotFolders()) {
        SDL_CreateDirectory(folder.c_str());           // a missing Pictures folder is made
        const std::string path = app::joinPath(folder, name);
        if (SDL_SaveFile(path.c_str(), png.data(), png.size())) {
            report(app, true, "Saved to " + path);
            std::printf("Saved image: %s (%d x %d)\n", path.c_str(), w, h);
            return;
        }
        why = SDL_GetError();
        tried += (tried.empty() ? "" : ", ") + folder;
    }
    if (tried.empty()) why = "no Pictures or Downloads folder found";
    report(app, false, "The picture could not be saved (" + why + (tried.empty() ? "" : "; tried " + tried) + ").");
    std::fprintf(stderr, "Save image failed: %s\n", why.c_str());
}

// 0.59.0: the lab's text (the background list, the readout included; no panels) drawn by Vulkan into an image the size of the picture area, on the
// card colour; 0.60.0: over the lab's shapes (renderer2d on Vulkan, one sample; Monochrome: grey)
void takeShotVulkan(AppState& app, render::VkBackend& vk, render::Renderer2D* picture, const ShotArea& a) {
    const int w = std::max(1, a.pictureFbW), h = std::max(1, a.pictureFbH);
    ImDrawData* src = ImGui::GetDrawData();
    ImDrawList* bgList = ImGui::GetBackgroundDrawList();
    bool has = false;
    for (ImDrawList* l : src->CmdLists) has = has || l == bgList;
    const float sx = static_cast<float>(a.fbW) / std::max(1, a.winW), sy = static_cast<float>(a.fbH) / std::max(1, a.winH);
    ImDrawData dd;
    dd.Valid = true;
    if (has) {
        dd.CmdLists.push_back(bgList);
        dd.CmdListsCount = 1;
        dd.TotalVtxCount = bgList->VtxBuffer.Size;
        dd.TotalIdxCount = bgList->IdxBuffer.Size;
    }
    dd.DisplayPos = ImVec2(0, a.headerH);
    dd.DisplaySize = ImVec2((w + 0.01f) / sx, (h + 0.01f) / sy);
    dd.FramebufferScale = ImVec2(sx, sy);
    dd.OwnerViewport = src->OwnerViewport;
    dd.Textures = src->Textures;
    const app::Rgb bg = look().c.card;
    const float clear[4] = {bg.r, bg.g, bg.b, 1.0f};
    std::vector<uint8_t> rgb;
    const bool ok = vk.renderToImage(has ? &dd : nullptr, w, h, clear, rgb, picture);
    dd.CmdLists.clear();
    if (!ok) { report(app, false, "The picture could not be saved (Vulkan could not draw it)."); std::fprintf(stderr, "Save image failed: Vulkan\n"); return; }
    saveShot(app, w, h, rgb);
}

}  // namespace ui
