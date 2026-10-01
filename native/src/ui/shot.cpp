#include "ui/shot.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "app/app_data.h"
#include "app/shot.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "render/gl_loader.h"
#include "ui/fonts.h"
#include "ui/look.h"

namespace ui {

void shotButton(AppState& app, bool sameLine) {
    ShotUi& s = app.shot;
    const bool flashing = ImGui::GetTime() < s.flashUntil;
    const std::string label = flashing ? displayable(s.flash) : displayable(app::kShotLabel);
    const ImGuiStyle& st = ImGui::GetStyle();
    if (sameLine) {                                    // beside the previous button if there is room, else on the next line
        const float w = ImGui::CalcTextSize(displayable(app::kShotLabel).c_str()).x + 2 * st.FramePadding.x;
        const float right = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
        if (ImGui::GetItemRectMax().x + st.ItemSpacing.x + w <= right) ImGui::SameLine();
    }
    if (ImGui::Button((label + "###shot").c_str())) s.requested = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", app::kShotTitle);
}

void shotNote(AppState& app) {
    ShotUi& s = app.shot;
    if (s.note.empty() || ImGui::GetTime() >= s.noteUntil) return;
    const Look& L = look();
    ImGui::PushStyleColor(ImGuiCol_Text, s.failed ? ImVec4(0.90f, 0.35f, 0.30f, 1) : vec(L.c.muted));
    ImGui::TextWrapped("%s", s.note.c_str());
    ImGui::PopStyleColor();
}

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
    s.flashUntil = ImGui::GetTime() + 1.8;             // the web's flash lasts 1.8 s
    s.note = note;
    s.noteUntil = ImGui::GetTime() + (ok ? 20.0 : 30.0);
}

}  // namespace

void takeShot(AppState& app, render::Renderer2D& renderer, const ShotArea& a) {
    using namespace plgl;
    const int w = std::max(1, a.pictureFbW), h = std::max(1, a.pictureFbH);
    // an offscreen image the size of the picture area
    GLuint fbo = 0, tex = 0;
    glGenFramebuffers(1, &fbo);
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glViewport(0, 0, w, h);
    const app::Rgb bg = look().c.card;                 // the web: the canvas on the lab's background colour (both labs paint over all of it)
    glClearColor(bg.r, bg.g, bg.b, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    renderer.end(w, h, 0, fbo);                        // the shapes (Monochrome: grey, as on screen)

    // the labels and the readout: the lab's text on the background list, nothing else (no panels, no windows)
    ImDrawData* src = ImGui::GetDrawData();
    ImDrawList* bgList = ImGui::GetBackgroundDrawList();
    bool has = false;
    for (ImDrawList* l : src->CmdLists) has = has || l == bgList;
    if (has) {
        const float sx = static_cast<float>(a.fbW) / std::max(1, a.winW), sy = static_cast<float>(a.fbH) / std::max(1, a.winH);
        ImDrawData dd;
        dd.Valid = true;
        dd.CmdLists.push_back(bgList);
        dd.CmdListsCount = 1;
        dd.TotalVtxCount = bgList->VtxBuffer.Size;
        dd.TotalIdxCount = bgList->IdxBuffer.Size;
        dd.DisplayPos = ImVec2(0, a.headerH);
        dd.DisplaySize = ImVec2((w + 0.01f) / sx, (h + 0.01f) / sy);
        dd.FramebufferScale = ImVec2(sx, sy);
        dd.OwnerViewport = src->OwnerViewport;
        dd.Textures = src->Textures;
        ImGui_ImplOpenGL3_RenderDrawData(&dd);
        dd.CmdLists.clear();
    }

    // read it back (rows come bottom first) and write the PNG
    std::vector<uint8_t> bottomUp(std::size_t(w) * h * 3), rgb(bottomUp.size());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, bottomUp.data());
    for (int y = 0; y < h; y++) std::copy_n(bottomUp.begin() + std::size_t(h - 1 - y) * w * 3, std::size_t(w) * 3, rgb.begin() + std::size_t(y) * w * 3);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    glViewport(0, 0, a.fbW, a.fbH);

    const std::vector<uint8_t> png = app::encodePng(w, h, rgb.data());
    SDL_Time now = 0;
    SDL_DateTime dt{};
    SDL_GetCurrentTime(&now);
    SDL_TimeToDateTime(now, &dt, true);                // local time
    const std::string lab = app.lab == 1 ? "Atom Lab" : "Orbit Lab";
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

}  // namespace ui
