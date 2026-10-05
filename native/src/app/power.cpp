#include "app/power.h"

namespace app {

PowerThrottlingRequest powerThrottlingOff() {
    return {kPowerThrottlingVersion, kPowerThrottlingExecutionSpeed | kPowerThrottlingIgnoreTimerResolution, 0u};   // a state bit of 0 = throttling off, never on
}

bool disablePowerThrottling(const std::function<bool(const PowerThrottlingRequest&)>& setter) {
    if (!setter) return false;
    return setter(powerThrottlingOff());
}

std::string powerNote(bool isWindows, bool accepted) {
    if (!isWindows) return "Windows power throttling: not applicable (not Windows)";
    return accepted ? "Windows power throttling: switched off for this app (accepted)" : "Windows power throttling: switch-off was asked and refused";
}

}  // namespace app
