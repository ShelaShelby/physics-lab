#include "app/fps.h"

#include <cmath>

namespace app {
std::string fpsText(double fps) {
    if (!(fps >= 0)) return "";
    return std::to_string(static_cast<long long>(std::floor(fps + 0.5))) + " fps";   // JavaScript's Math.round: halves go up
}
}  // namespace app
