#include "blackhole/bench_settings.h"

namespace blackhole {

namespace {
void gasAllOn(Settings& s, bool fastest) {
    s.gasMoving = true; s.gasRun = true; s.gasClump = true; s.gasPattern = true;
    if (fastest) setGasRate(s, kGasRateMax);
}
}  // namespace

Settings benchSettings(const std::string& id, bool allLight, bool& ok) {
    ok = true;
    Settings s = settingsDefaults();
    if (id == "blackhole.edge_on") {                   // the 0.82.1 bug's view: Sagittarius A* (the defaults), the disc on, feeding 0.11, 57.5 GM/c^2, pitch 1.575 rad
        setDisc(s, true);
        setFeed(s, 0.11);
        setDistance(s, 57.5);
        s.pitch = 1.575;
        gasAllOn(s, false);                            // (the streaks and the clump; the rate stays automatic)
        return s;
    }
    applyPreset(s, kFeedingKey);                       // the others: a feeding hole, the disc on, Real
    if (id == "blackhole.dolly_spin") { setSpin(s, 0.9); setDistance(s, kDistMax); }   // the wheel then moves it in to kDistMin and back
    else if (id == "blackhole.over_poles") setSpin(s, kSpinTop);                         // the drag then tilts it from kPitchMin to kPitchMax and back
    else if (id == "blackhole.everything") {
        setSpin(s, kSpinTop);
        setOuter(s, kOuterMax);
        gasAllOn(s, true);
        setDistance(s, kDistMin);
        setFov(s, kFovMax);
        s.allLight = allLight; s.glare = !allLight;
    } else if (id == "ui.resize_blackhole") { setSpin(s, 0.9); gasAllOn(s, false); }
    else if (id == "blackhole.close_spin") { setSpin(s, kSpinTop); setDistance(s, kDistMin); setFov(s, kFovMax); }   // 0.84.0: then dragged round as blackhole.circle_spin
    else ok = false;
    return s;
}

}  // namespace blackhole
