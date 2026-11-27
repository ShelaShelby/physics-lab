// 0.83.0: the Black Hole Lab settings of the benchmark's new hard scenes, without graphics, so tests/test_app.cpp can read them back.
// ui/benchmark.cpp sets the lab from these and then moves the camera (the wheel, the drag, the turn) by the scene's clock.
#pragma once

#include <string>

#include "blackhole/lab.h"
#include "blackhole/spin_sky.h"   // kSpinTop

namespace blackhole {

// id: blackhole.dolly_spin, blackhole.over_poles, blackhole.everything, blackhole.edge_on, ui.resize_blackhole, blackhole.close_spin (0.84.0), blackhole.thick_spin (0.95.0); allLight: blackhole.everything with Light:
// All light instead of the eye's glare (they exclude each other; the glare was the heavier on the build machine, REPORT 0.83.0). ok false: no such scene.
Settings benchSettings(const std::string& id, bool allLight, bool& ok);

}  // namespace blackhole
