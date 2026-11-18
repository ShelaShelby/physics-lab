#version 450
// 0.59.0 (Vulkan step 1, render/vk_backend.cpp): one triangle over the whole target; the scissor keeps the picture area.
// Every value typed, no implicit int/float conversion (AMD's compiler is stricter than Mesa's).
void main() {
    const vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
