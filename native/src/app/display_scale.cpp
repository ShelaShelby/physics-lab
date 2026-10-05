#include "app/display_scale.h"

#include <cmath>

namespace app {

float sanitizeDisplayScale(float s) {
    if (!(s > 0.0f)) return 1.0f;                       // 0, negative and NaN (every comparison with NaN is false)
    if (s < 0.5f) return 0.5f;
    if (s > 4.0f) return 4.0f;
    return s;
}

void minimumWindowSize(float scale, int& w, int& h) {
    const float s = sanitizeDisplayScale(scale);
    w = static_cast<int>(std::lround(kMinWindowWu * s));
    h = static_cast<int>(std::lround(kMinWindowHu * s));
}

float panelWidthFor(float scale) { return kPanelWidthU * sanitizeDisplayScale(scale); }

int scaledTextPx(double baseUnits, int floorPx, double scale, double textFactor) {
    const int px = static_cast<int>(std::lround(baseUnits * scale * textFactor));
    return px < floorPx ? floorPx : px;
}

bool DisplayScale::update(float reading) {
    const float s = sanitizeDisplayScale(reading);
    if (std::fabs(s - scale_) <= 0.001f) return false;
    scale_ = s;
    changes_++;
    return true;
}

}  // namespace app
