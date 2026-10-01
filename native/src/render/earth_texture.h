// Earth's map (1024 × 512, longitude −180…180 left to right, latitude 90…−90 top to bottom), painted
// once at start-up like the web version's earthTexture(): ocean, the real coastlines (Natural Earth
// 110 m), green land with forests, deserts and ice, and soft white clouds. RGBA, 4 bytes per pixel.
#pragma once

#include <cstdint>
#include <vector>

namespace render {

constexpr int kEarthTexW = 1024, kEarthTexH = 512;
std::vector<uint8_t> makeEarthTexture();

}  // namespace render
