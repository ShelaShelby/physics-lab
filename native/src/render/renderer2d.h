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
constexpr float kBlobPalSquare = 5, kBlobPalSoft = 6;   // Blob::kind of a square / soft dot with a colour picked by the shader (see Renderer2D::blobsPalette)
Color rgba(int r, int g, int b, float a = 1);

class Renderer2D {
public:
    bool init();
    void shutdown();

    // Start a frame for a picture `width` × `height` points.
    void begin(double width, double height);
    // Draw everything that was added. The framebuffer is fbWidth × fbHeight pixels.
    // fbY: the picture's bottom edge, pixels from the window's bottom. outFbo: draw into this framebuffer
    // (fbWidth × fbHeight, from its bottom-left corner) instead of the window: used by 📷 Save image.
    void end(int fbWidth, int fbHeight, int fbY = 0, GLuint outFbo = 0);

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

    // Many round or square "blobs" with their own colours, drawn with ONE instanced draw call (the
    // Atom Lab's cloud dots, glowing nucleons, gluon-field particles, quarks). Each blob is a disc of
    // radius r filled like a canvas radial gradient: colour c0 at the focal circle (centre fx, fy and
    // radius f0, in units of r from the blob's centre) through c1 at stop s1 to c2 at the edge.
    // additive: colours add up (the canvas "lighter" mode).
    struct Blob {
        float x, y, r;
        float kind;          // 0 gradient disc, 1 fuzzy nucleon profile (colour c0), 2 square of side 2r (c0), 3 flat disc (c0), 4 soft Gaussian dot (c0; fades to 0 at r),
                             // 5 and 6: a square (like 2) or a soft dot (like 4) whose colour the SHADER picks from the group index in fx and the palette of blobsPalette(); only c0.a is used
        float fx, fy, f0, s1;
        Color c0, c1, c2;
    };
    void blobs(const Blob* b, int count, bool additive = false);
    // Blobs of kind 5 and 6 (the cloud dots, 0.13 stage 1): the colour is picked in the vertex shader from the group number in fx. Wheel: group 0..23 is the colour of the phase at the middle
    // of that bin (a port of atom::phaseRgb in atom/phase_wheel.h; `light` = the light-page colours); otherwise the group 0 is sign[0] and any other group sign[1].
    struct PhasePalette { bool wheel = false, light = false; Color sign[2]; };
    void blobsPalette(const Blob* b, int count, bool additive, const PhasePalette& pal);
    // Everything drawn between beginSoftLayer() and endSoftLayer() goes into an offscreen picture at
    // `scale` of the resolution, which is then stretched over the picture in one go (the web's soft layer:
    // blurry things cost a fraction of the pixels, and out-of-focus things look soft).
    void beginSoftLayer(float scale);
    void endSoftLayer();

    // Monochrome theme: the whole picture in shades of grey (the web's canvas{filter:grayscale(1)})
    void setGrayscale(bool on, Color bg) { gray_ = on; grayBg_ = bg; }   // bg: the page colour under the picture

    // What the last end() drew: draw calls, and shapes drawn by the instanced calls (squares and blobs)
    int lastDrawCalls() const { return lastCalls_; }
    int lastInstances() const { return lastInstances_; }

    // A turning planet. K turns screen directions into the planet's own frame (rows: its 0° meridian,
    // 90° east, north pole; columns: screen right, up, towards you). earth: the Earth map, else a plain
    // colour with a grid of latitude and longitude and the 0° meridian marked (as in the web version).
    void planet(double x, double y, double r, Color base, double lightX, double lightY, const float K[9], bool earth);

private:
    enum class Kind { Triangles, Squares, Planet, Blobs, LayerBegin, LayerEnd };
    struct Segment { Kind kind; int first, count; float size; Color c; float K[9]; bool earth; float r; bool additive; PhasePalette pal; };   // draw calls, in the order they were added
    void closeTriangles();
    std::vector<Segment> segments_;
    std::vector<float> inst_;                 // square centres for all square draws of this frame
    std::size_t triStart_ = 0;
    GLuint sqProgram_ = 0, sqVao_ = 0, sqCornerVbo_ = 0, sqInstVbo_ = 0;
    GLint sqSize_ = -1, sqSide_ = -1, sqColor_ = -1;
    GLuint plProgram_ = 0, earthTex_ = 0;
    GLint plSize_ = -1, plK_ = -1, plEarth_ = -1, plPa_ = -1, plLod_ = -1, plTex_ = -1;

    std::vector<Blob> blobs_;                 // all blobs of this frame
    GLuint blProgram_ = 0, blVao_ = 0, blInstVbo_ = 0;
    GLint blSize_ = -1, blFuzz_ = -1, blPalWheel_ = -1, blPalLight_ = -1, blPalPlus_ = -1, blPalMinus_ = -1;
    GLuint layerFbo_ = 0, layerTex_ = 0, cmpProgram_ = 0, cmpVao_ = 0;
    GLint cmpTex_ = -1;
    int layerW_ = 0, layerH_ = 0;
    float layerScale_ = 0.5f;
    int lastCalls_ = 0, lastInstances_ = 0;
    bool gray_ = false;
    Color grayBg_{0, 0, 0, 1};
    GLuint grayFbo_ = 0, grayTex_ = 0, grayProgram_ = 0;
    GLint grayTexLoc_ = -1;
    int grayW_ = 0, grayH_ = 0;

    struct Vertex { float x, y, u, v, r, g, b, a, mode, p1, p2, p3; };
    void quad(double cx, double cy, double half, Color c, float mode, float p1, float p2, float uvScale);
    void push(double x, double y, float u, float v, Color c, float mode, float p1 = 0, float p2 = 0);

    std::vector<Vertex> verts_;
    double width_ = 1, height_ = 1;
    GLuint program_ = 0, vao_ = 0, vbo_ = 0;
    GLint uSize_ = -1;
};

}  // namespace render
