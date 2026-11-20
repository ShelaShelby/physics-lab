// 0.67.0: the pipeline cache files of the app and of its helper process (app/gpu_helper.h): our header (magic, size, FNV-1a of the data), then the driver's
// data, whose own header must name this card and driver (render/vk_blackhole.cpp keeps the app's own file the same way)
#pragma once

#include <string>
#include <vector>

#include <volk.h>

namespace render {

// the driver's data of a cache file, when it is whole and made by this card and driver (note says what was found)
bool vkReadCacheFile(const std::string& path, VkPhysicalDevice phys, std::vector<char>& data, std::string& note);
void vkWriteCacheFile(const std::string& path, VkDevice dev, VkPipelineCache cache);

}  // namespace render
