// Asking Windows not to power-throttle the app (0.15 stage 2, C3). Windows 10 / 11 can run a background app on the efficiency cores or at a low clock ("EcoQoS",
// process power throttling); the owner saw frame rates fall a lot when the window merely lost focus on his second monitor. The request is Windows'
// PROCESS_POWER_THROTTLING_STATE with a control bit set and its state bit 0 = "do not throttle". The numbers below are written from knowledge of the Win32 API
// (winnt.h); the documentation page was NOT reachable from the cloud session and nothing here was run on Windows. The logic (never ask for throttling ON, one call, the
// note for the benchmark report) is unit-tested with a stand-in setter (tests/test_stage2.inc).
#pragma once

#include <functional>
#include <string>

namespace app {

struct PowerThrottlingRequest { unsigned version, controlMask, stateMask; };

constexpr unsigned kPowerThrottlingVersion = 1;                 // PROCESS_POWER_THROTTLING_CURRENT_VERSION
constexpr unsigned kPowerThrottlingExecutionSpeed = 0x1;        // PROCESS_POWER_THROTTLING_EXECUTION_SPEED
constexpr unsigned kPowerThrottlingIgnoreTimerResolution = 0x4; // PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION (Windows 11: a background app's timer requests are ignored)

// Control the execution speed and the timer resolution, state 0 = off.
PowerThrottlingRequest powerThrottlingOff();
// Calls `setter` once with that request; the setter says whether the system accepted it. No setter (not Windows): false.
bool disablePowerThrottling(const std::function<bool(const PowerThrottlingRequest&)>& setter);
// The line for the benchmark report. isWindows: the app was built for Windows; accepted: the system took the request.
std::string powerNote(bool isWindows, bool accepted);

}  // namespace app
