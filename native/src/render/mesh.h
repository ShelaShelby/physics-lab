// A retained mesh for the chart of nuclides (0.13 stage 4 part B): triangles that stay on the GPU between frames. Up to 0.13 stage 3 the chart's 3,557 squares / blocks were turned into vertices
// every frame (about 1 MB flat, 3.2 MB as a landscape) and uploaded again. Now the vertices are made once, in the chart's own "world" coordinates (a across, b down, z up, in the flat layout's points),
// and the vertex shader (shaders.h, kChartVS) applies the projection each frame from a few uniforms: a tilt, zoom or pan changes uniforms, not the buffer. No graphics calls in here.
#pragma once

namespace render {

// One vertex. A triangle vertex has corner < 0. A LINE (a hairline 1 point wide, as Renderer2D::line makes it) is six vertices with corner 0..3 and both end points: bit 0 of the corner says which end
// the vertex is at, bit 1 which side of the line, and the shader pushes it half a point out along the line's normal on the screen (the width cannot be made in world space: it is in points).
struct MeshVertex {
    float p0[3];        // the point (a, b, z)
    float p1[3];        // a line vertex: the other end of the line
    float rgba[4];
    float corner;       // < 0: a triangle vertex; 0..3: a line vertex (see above)
    float height;       // < 0: always drawn; >= 0: a side face of a block this tall (flat-layout points): drawn only when the chart is tilted and height * sin(pitch) > 0.05, the rule of ChartScene::build
};

// atom::ChartProjection as the shader needs it: final = F + K (raw - C), raw = the turn and tip about C = (cx, cy). identity: the picture's own points (the flat chart); flat: tilt 0.
struct MeshProjection {
    double cosY = 1, sinY = 0, cosP = 1, sinP = 0, cx = 0, cy = 0, K = 1, fx = 0, fy = 0;
    bool identity = true, flat = true;
};

}  // namespace render
