#version 450
// 0.59.0 (Vulkan step 1, render/vk_backend.cpp): the picture area's colour (the theme's background) from the push constant.
layout(push_constant) uniform Fill { vec4 colour; } pc;
layout(location = 0) out vec4 outColour;
void main() {
    outColour = pc.colour;
}
