// The automatic benchmark (Performance card → Benchmark): fixed scenes, one after the other, VSync off,
// with numbers to compare between runs, versions and PCs. The numbers, the timing and the text are in app/bench.h.
#pragma once

#include <string>
#include <vector>

#include "render/renderer2d.h"
#include "ui/orbit_panel.h"

namespace ui {

// The Benchmark button and a note, for the Performance card (both labs). Disabled, with a tooltip that says why, while Learn or
// Achievements covers the lab, during a Journey or while a picture is being saved.
void benchmarkButton(AppState& app);
// The benchmark's own window: what it will do (Start / Cancel), the progress while it runs, the result. Call every frame.
void benchmarkWindow(AppState& app);
// Call at the start of every frame (sets up the next scene) and at the end of it with that frame's numbers.
// The renderer and the framebuffer size are for the GPU timer check (added in the next stage).
void benchmarkBeginFrame(AppState& app, render::Renderer2D& renderer, int fbW, int fbH);
void benchmarkEndFrame(AppState& app, double frameMs, double cpuMs);
// Around the drawing of the frame: the GPU time (GL_TIME_ELAPSED), only measured while the benchmark runs.
void benchmarkGpuBegin(AppState& app);
void benchmarkGpuEnd(AppState& app);

// For automatic checks (PL_BENCH=ask opens the dialog, PL_BENCH=start starts at once; PL_BENCH_WARMUP / PL_BENCH_MEASURE shorten the times): call every frame.
void benchmarkTestHook(AppState& app);

// The scenes' names and settings in words (the list in the report and in the result).
struct BenchSceneInfo { const char* name; const char* what; };
const std::vector<BenchSceneInfo>& benchmarkScenes();

}  // namespace ui
