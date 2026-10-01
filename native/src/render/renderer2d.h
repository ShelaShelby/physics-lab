// A small batched 2D renderer on OpenGL 3.3 core.
//
// Coordinates are "picture points" with (0, 0) at the top-left and y going down,
// like the web version's canvas. All shapes of a frame go into one vertex buffer
// and are drawn with one draw call, in the order they were added.
// Positions arrive as doubles (the orbit maths is in double precision) and are
// only turned into floats once they are on or near the screen.
#pragma once

#include <vector>

#include "gl_loader.h"

namespace render {

struct Color {
    float r = 1, g = 1, b = 1, a = 1;
    Color withAlpha(float alpha) const { return {r, g, b, alpha}; }
};
Color hexColor(const char* hex, float alpha = 1);   // "#rrggbb"
Color rgba(int r, int g, int b, float a = 1);

class Renderer2D {
public:
    bool init();
    void shutdown();

    // Start a frame for a picture `width` × `height` points.
    void begin(double width, double height);
    // Draw everything that was added. The framebuffer is fbWidth × fbHeight pixels.
    void end(int fbWidth, int fbHeight);

    void rect(double x, double y, double w, double h, Color c);
    void line(double x0, double y0, double x1, double y1, double width, Color c);
    void triangle(double x0, double y0, Color c0, double x1, double y1, Color c1, double x2, double y2, Color c2);
    void disc(double x, double y, double r, Color c);                       // flat round dot, smooth edge
    void ring(double x, double y, double r, double width, Color c);         // circle outline
    void sphere(double x, double y, double r, Color c, double lightX, double lightY);  // a ball lit from (lightX, lightY) direction on screen
    void sunDisc(double x, double y, double r, Color c);                    // the Sun: brighter middle, darker edge
    void glow(double x, double y, double r, Color c);                       // the Sun's corona: starts at r/3.5 and fades out to radius r
    void fadeDisc(double x, double y, double r, Color c);                   // a soft dot: full colour in the middle, fading to nothing at radius r

    // Many small squares of one colour, drawn with ONE instanced draw call (used for the belts'
    // thousands of rocks). xy holds the squares' centres, 2 floats each, in picture points.
    void squares(const float* xy, int count, double size, Color c);

private:
    struct Segment { bool squares; int first, count; float size; Color c; };   // draw calls, in the order they were added
    void closeTriangles();
    std::vector<Segment> segments_;
    std::vector<float> inst_;                 // square centres for all square draws of this frame
    std::size_t triStart_ = 0;
    GLuint sqProgram_ = 0, sqVao_ = 0, sqCornerVbo_ = 0, sqInstVbo_ = 0;
    GLint sqSize_ = -1, sqSide_ = -1, sqColor_ = -1;

    struct Vertex { float x, y, u, v, r, g, b, a, mode, p1, p2, p3; };
    void quad(double cx, double cy, double half, Color c, float mode, float p1, float p2, float uvScale);
    void push(double x, double y, float u, float v, Color c, float mode, float p1 = 0, float p2 = 0);

    std::vector<Vertex> verts_;
    double width_ = 1, height_ = 1;
    GLuint program_ = 0, vao_ = 0, vbo_ = 0;
    GLint uSize_ = -1;
};

}  // namespace render
