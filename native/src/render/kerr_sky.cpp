#include "kerr_sky.h"

#include <cstdio>
#include <cstdlib>

#include "shaders.h"

namespace render {

std::string kerrSkySource(int stage) {
    if (stage == 0) return kKerrSkyTinyFS;
    if (stage == 1) return std::string(kKerrSkyHeadPart) + kEllipticPart + kKerrSkyMathPart;
    static const char* stop = std::getenv("PL_BH_SKYSTOP");             // (tests, 0.56.2: the pass's parts timed: the ray stops after part 1, 2 or 3)
    return std::string(kKerrSkyHeadPart) + (stop ? std::string("#define SKY_STOP ") + stop + "\n" : std::string()) + kEllipticPart + kKerrSkyTracePart + kKerrSkyMainPart;
}

std::string kerrSkyShaderId() {
    unsigned long long h = 1469598103934665603ULL;                  // FNV-1a, 64 bit
    for (int s = 0; s < KerrSky::kStages; s++)
        for (char c : kerrSkySource(s)) { h ^= static_cast<unsigned char>(c); h *= 1099511628211ULL; }
    char b[40];
    std::snprintf(b, sizeof b, "sky %016llx", h);
    return b;
}

const char* KerrSky::stageName(int stage) {
    static const char* const n[kStages] = {"the tiny shader", "the elliptic functions alone", "the whole sky shader"};
    return stage >= 0 && stage < kStages ? n[stage] : "?";
}

void KerrSky::reset() {
    for (double& m : ms_) m = 0;
}

bool KerrSky::build(int stage, KerrSteps* steps, std::string& why) {
    if (stage < 0 || stage >= kStages) return false;
    if (!vkBuild_) { why = "no Vulkan"; return false; }               // 0.69.0: Vulkan's only (render/vk_blackhole.cpp buildSky)
    const int r = vkBuild_(stage, steps, why);                       // 0.62.0
    building_ = r == 0;
    return r >= 0;
}

}  // namespace render
