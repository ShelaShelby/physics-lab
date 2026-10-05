// 0.15 stage 2, C3: the Windows side of app/power.h. Asks Windows not to power-throttle this process; elsewhere it does nothing.
#pragma once

#include <string>

namespace ui {

// Call once at start. Returns the line for the benchmark report (app::powerNote). NOT tested on Windows from the cloud session.
std::string requestNoPowerThrottling();

}  // namespace ui
