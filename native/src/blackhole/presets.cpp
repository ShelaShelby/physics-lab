#include "blackhole/presets.h"

#include <cmath>
#include <cstring>

#include "blackhole/schwarzschild.h"
#include "orbit/units.h"

namespace blackhole {

static const Preset kPresets[] = {
    {"sgra", "Sagittarius A*", 4.297e6, 0.012e6, 8277, 9, 51.8, 2.3, 5.125, 0.009,
     "GRAVITY Collaboration 2022, A&A 657, L12, Table B.1; ring and theta_g: EHT 2022, ApJL 930, L12, Table 1"},
    {"m87", "M87*", 6.5e9, 0.7e9, 16.8e6, 0.8e6, 42, 3, 3.8, 0.4, "EHT 2019, ApJL 875, L1, Table 1"},
};

const Preset* presets(int& count) { count = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0])); return kPresets; }
const Preset* preset(const char* key) {
    for (const auto& p : kPresets) if (std::strcmp(p.key, key) == 0) return &p;
    return nullptr;
}

double gravRadiusM(double m) { return m * units::kGMSun / (units::kLightSpeed * units::kLightSpeed); }
double parsecM() { return 648000 / units::kPi * units::kAU; }
double horizonKm(const Preset& p) { return 2 * gravRadiusM(p.massSun) / 1000; }
double horizonAU(const Preset& p) { return 2 * gravRadiusM(p.massSun) / units::kAU; }
static constexpr double kMuasPerRad = 180.0 / 3.14159265358979323846 * 3600e6;
double thetaGMuas(const Preset& p) { return gravRadiusM(p.massSun) / (p.distPc * parsecM()) * kMuasPerRad; }
double shadowDiameterMuas(const Preset& p) { return 2 * shadowAngle(p.distPc * parsecM() / gravRadiusM(p.massSun)) * kMuasPerRad; }

}  // namespace blackhole
