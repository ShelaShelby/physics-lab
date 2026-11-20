// 0.67.0: the crash-safe helper for the graphics card's risky first steps. A driver that crashes while it makes a pipeline (AMD's compiler on the light paths'
// kKerrTableCS, 0.55.7 on) used to close the app. Now, at a Vulkan start, each guarded card program whose path has not passed with this driver and that
// the helper knows (the light paths, the Classic dots, the nucleus' Density look) is made first by a helper: the same exe started with --gpu-helper, no
// window, its own Vulkan device on the same card, one process per program, one after the other while the app shows its first pictures. It makes the
// program's pipelines (into its own pipeline cache file, which the app then merges into its own) and exits. The program's guard holds its first step
// (GpuGuard::hold says no) until its helper has finished: passed, the app runs its guarded steps as before; the driver said no, kept as "fail"; the
// helper died, kept as "off" with "the helper died making <step>": the app never makes it (until Try again or a new driver).
// The light paths' detective work: while they are off on this driver (and once per driver), helpers make five cut-down variants of kKerrTableCS
// (render/vk_helper.cpp), one process each, and the log says which the driver made and which killed its helper.
#pragma once

#include <string>
#include <vector>

namespace app {

class GpuGuard;

// the helper's side (main.cpp, with --gpu-helper first): the program's exit code
int gpuHelperMain(int argc, char** argv);               // (render/vk_helper.cpp)
bool gpuHelperKnows(const std::string& name);           // the programs it can make: lightpaths, classic, nucdensity

// the app's side. queue: the programs (by their guard's settings name) to make in helpers, with their guards; folder: the settings folder (the helpers'
// result and cache files); device: the card's name as the app's Vulkan start chose it
void gpuHelperQueue(const std::vector<std::pair<std::string, GpuGuard*>>& programs, const std::string& folder, const std::string& device, const std::string& driver);
// the light paths' variants, when they are off on this driver and were not tried with it yet (gpu-helper-variants.txt)
void gpuHelperVariants(GpuGuard* lightpaths, const std::string& folder, const std::string& device, const std::string& driver);
// once a frame: starts the next helper, reads a finished one; true when a program's helper finished this frame (the settings then keep its guard's record)
bool gpuHelperPump();
bool gpuHelperPending(const std::string& name);   // its helper has not finished yet (queued or running)
bool gpuHelperBusy();                              // any helper queued or running
std::string gpuHelperCacheFile(const std::string& folder);   // the helpers' pipeline cache (the app merges it into its own)
long gpuHelperFinishedCount();                     // helpers finished so far (the app merges the cache when it grows)

// the helper process (tests and the app): start the same exe with these arguments, no window, no handles inherited; poll: -1 running, else its exit code
// (a crash: a code other than 0, 1 and 2; Windows gives the exception code)
bool gpuHelperSpawn(const std::vector<std::string>& args);
int gpuHelperExitCode();

}  // namespace app
