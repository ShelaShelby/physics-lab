// The automatic benchmark (Performance card → Benchmark): fixed scenes, one after the other, VSync off,
// with numbers to compare between runs, versions and PCs. The numbers, the timing and the text are in app/bench.h.
#pragma once

#include <string>
#include <vector>

#include "render/renderer2d.h"
#include "ui/orbit_panel.h"

struct SDL_Window;
namespace render { class VkBackend; }

namespace ui {

// The Benchmark button and a note, for the Performance card (both labs). Disabled, with a tooltip that says why, while the Learn page
// covers the lab, during a Journey or while a picture is being saved.
void benchmarkButton(AppState& app);
// The benchmark's own window: what it will do (Start / Cancel), the progress while it runs, the result. Call every frame.
void benchmarkWindow(AppState& app);
// Call at the start of every frame (sets up the next scene) and at the end of it with that frame's numbers.
void benchmarkBeginFrame(AppState& app, render::Renderer2D& renderer, int fbW, int fbH);
void benchmarkEndFrame(AppState& app, double frameMs, double cpuMs);
// Around the whole GL work of the frame (from the clear to just before the buffer swap, the picture, its soft layer and the panels included):
// the GPU time (GL_TIME_ELAPSED and timestamps), only measured while the benchmark runs. The first frame of a run also checks the timer with a known load.
// 0.63.0 (Vulkan step 5): on Vulkan around vk.render(): timestamp queries (render/vk_backend.h benchTimer), the card's batches included.
void benchmarkGpuBegin(AppState& app);
void benchmarkGpuEnd(AppState& app);
// 0.63.0: the window, and the Vulkan backend when this start draws with Vulkan (null: OpenGL); set once at start
void benchmarkUse(SDL_Window* window, render::VkBackend* vk);

// For automatic checks (PL_BENCH=ask opens the dialog, PL_BENCH=start starts at once; PL_BENCH_WARMUP / PL_BENCH_MEASURE shorten the times): call every frame.
void benchmarkTestHook(AppState& app);

// 0.56.1: the lab on screen has its finished picture (nothing is being made for it): the start-up time, the waits and the labs left alone use it.
bool benchLabFinished(AppState& app, render::Renderer2D& renderer);

}  // namespace ui
