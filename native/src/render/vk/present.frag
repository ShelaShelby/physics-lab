#version 450
// 0.60.0 (Vulkan step 2, render/vk_draw2d.cpp): the lab's picture copied pixel for pixel into the picture area. The picture is drawn with OpenGL's rows
// (its first row at the bottom: renderer2d's shaders are OpenGL's own text), the window's first row is its top: the rows are turned over here.
// pc.area: the picture area's left and top pixel in the target, its height in pixels. Every value typed (no implicit int/float conversion).
layout(push_constant) uniform Present { ivec4 area; } pc;
layout(set = 0, binding = 0) uniform sampler2D uPicture;
layout(location = 0) out vec4 outColour;
void main() {
    const ivec2 q = ivec2(gl_FragCoord.xy) - pc.area.xy;
    outColour = vec4(texelFetch(uPicture, ivec2(q.x, pc.area.z - 1 - q.y), 0).rgb, 1.0);
}
