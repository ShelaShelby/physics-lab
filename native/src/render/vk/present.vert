#version 450
// 0.60.0 (Vulkan step 2, render/vk_draw2d.cpp): one triangle over the target; the viewport and scissor keep the picture area.
void main() {
    const vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
