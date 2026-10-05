#include "ui/power.h"

#include "app/power.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <processthreadsapi.h>
#endif

namespace ui {

std::string requestNoPowerThrottling() {
#ifdef _WIN32
    const bool ok = app::disablePowerThrottling([](const app::PowerThrottlingRequest& r) {
        PROCESS_POWER_THROTTLING_STATE s = {};
        s.Version = r.version;
        s.ControlMask = r.controlMask;
        s.StateMask = r.stateMask;
        return SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &s, sizeof s) != 0;
    });
    return app::powerNote(true, ok);
#else
    return app::powerNote(false, false);
#endif
}

}  // namespace ui
