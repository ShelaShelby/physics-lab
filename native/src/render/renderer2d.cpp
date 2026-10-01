#include "renderer2d.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "earth_texture.h"

using namespace plgl;

namespace render {

Color rgba(int r, int g, int b, float a) { return {r / 255.f, g / 255.f, b / 255.f, a}; }

Color hexColor(const char* hex, float alpha) {
    if (!hex || hex[0] != '#') return {1, 1, 1, alpha};
    const unsigned long v = std::strtoul(hex + 1, nullptr, 16);
    return rgba(static_cast<int>((v >> 16) & 255), static_cast<int>((v >> 8) & 255), static_cast<int>(v & 255), alpha);
}

// mode: 0 flat colour, 1 round dot, 2 lit ball, 3 Sun disc, 4 Sun glow, 5 soft fading dot
static const char* kVertexShader = R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
layout(location = 3) in vec4 aParam;
uniform vec2 uSize;
out vec2 vUV;
out vec4 vColor;
out vec4 vParam;
void main() {
    vUV = aUV; vColor = aColor; vParam = aParam;
    gl_Position = vec4(aPos.x / uSize.x * 2.0 - 1.0, 1.0 - aPos.y / uSize.y * 2.0, 0.0, 1.0);
}
)";

static const char* kFragmentShader = R"(#version 330 core
in vec2 vUV;
in vec4 vColor;
in vec4 vParam;
out vec4 fragColor;
void main() {
    int mode = int(vParam.x + 0.5);
    if (mode == 0) { fragColor = vColor; return; }
    float d = length(vUV);                                   // 0 in the middle, 1 at the edge
    float aa = max(fwidth(d), 1e-4);
    float edge = 1.0 - smoothstep(1.0 - aa, 1.0 + aa, d);   // smooth edge, about one pixel wide
    if (mode == 1) { fragColor = vec4(vColor.rgb, vColor.a * edge); return; }
    if (mode == 2) {                                         // lit ball: Lambert shading, light from the Sun's direction on screen
        vec3 n = vec3(vUV.x, -vUV.y, sqrt(max(0.0, 1.0 - d * d)));
        vec3 L = normalize(vec3(vParam.y, -vParam.z, 0.35));
        float lit = max(dot(n, L), 0.0);
        vec3 c = vColor.rgb * (0.12 + 0.88 * lit) + vec3(0.2) * pow(lit, 12.0);
        fragColor = vec4(c, vColor.a * edge);
        return;
    }
    if (mode == 3) {                                         // the Sun: hot middle, darker edge (limb darkening)
        vec3 hot = mix(vColor.rgb, vec3(1.0, 1.0, 0.94), 0.6);
        vec3 rim = mix(vColor.rgb, vec3(0.47, 0.16, 0.0), 0.45);
        vec3 c = d < 0.55 ? mix(hot, vColor.rgb, d / 0.55) : d < 0.9 ? mix(vColor.rgb, mix(vColor.rgb, vec3(0.47, 0.16, 0.0), 0.2), (d - 0.55) / 0.35)
                                                       : mix(mix(vColor.rgb, vec3(0.47, 0.16, 0.0), 0.2), rim, (d - 0.9) / 0.1);
        fragColor = vec4(c, vColor.a * edge);
        return;
    }
    if (mode == 5) { fragColor = vec4(vColor.rgb, vColor.a * clamp(1.0 - d, 0.0, 1.0)); return; }   // soft dot fading to nothing at the edge
    // mode 4: glow that starts at 0.283 of the radius (the Sun's edge) and fades out
    float t = clamp((d - 0.283) / 0.717, 0.0, 1.0);
    float a = t < 0.25 ? mix(0.55, 0.18, t / 0.25) : mix(0.18, 0.0, (t - 0.25) / 0.75);
    if (d > 1.0) a = 0.0;
    fragColor = vec4(vColor.rgb, vColor.a * a);
}
)";

// Squares for instanced drawing: a unit square's corners (per vertex) moved to each centre (per instance)
static const char* kSquareVS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec2 aCentre;
uniform vec2 uSize;
uniform float uSide;
void main() {
    vec2 p = aCentre + aCorner * uSide;
    gl_Position = vec4(p.x / uSize.x * 2.0 - 1.0, 1.0 - p.y / uSize.y * 2.0, 0.0, 1.0);
}
)";
static const char* kSquareFS = R"(#version 330 core
uniform vec4 uColor;
out vec4 fragColor;
void main() { fragColor = uColor; }
)";

// Blobs: one instanced quad per blob. The fragment shader fills it like a canvas radial gradient
// (two circles: the focal circle and the blob's edge), or with the fuzzy-nucleon profile, or flat.
static const char* kBlobVS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 aPosKind;      // x, y, r, kind
layout(location = 2) in vec4 aFocal;        // fx, fy, f0, s1
layout(location = 3) in vec4 aC0;
layout(location = 4) in vec4 aC1;
layout(location = 5) in vec4 aC2;
uniform vec2 uSize;
out vec2 vLocal;
flat out vec4 vPosKind;
flat out vec4 vFocal;
flat out vec4 vC0;
flat out vec4 vC1;
flat out vec4 vC2;
void main() {
    float r = aPosKind.z, kind = aPosKind.w;
    float ext = kind > 1.5 && kind < 2.5 ? r : r + 1.0;   // squares are exact; round things get a pixel for the smooth edge
    vec2 p = aPosKind.xy + aCorner * 2.0 * ext;
    vLocal = aCorner * 2.0 * ext / max(r, 1e-6);
    vPosKind = aPosKind; vFocal = aFocal; vC0 = aC0; vC1 = aC1; vC2 = aC2;
    gl_Position = vec4(p.x / uSize.x * 2.0 - 1.0, 1.0 - p.y / uSize.y * 2.0, 0.0, 1.0);
}
)";
static const char* kBlobFS = R"(#version 330 core
in vec2 vLocal;
flat in vec4 vPosKind;
flat in vec4 vFocal;
flat in vec4 vC0;
flat in vec4 vC1;
flat in vec4 vC2;
uniform float uFuzz[17];
out vec4 fragColor;
void main() {
    int kind = int(vPosKind.w + 0.5);
    if (kind == 2) { fragColor = vC0; return; }
    float d = length(vLocal);
    float aa = max(fwidth(d), 1e-4);
    float edge = 1.0 - smoothstep(1.0 - aa, 1.0 + aa, d);
    if (kind == 3) { fragColor = vec4(vC0.rgb, vC0.a * edge); return; }
    if (kind == 1) {                                         // fuzzy nucleon: the tabulated column-density profile
        float x = clamp(d, 0.0, 1.0) * 16.0;
        int i = int(min(floor(x), 15.0));
        float a = mix(uFuzz[i], uFuzz[i + 1], x - float(i));
        fragColor = vec4(vC0.rgb, vC0.a * a * edge);
        return;
    }
    // two-circle radial gradient (as the canvas createRadialGradient): find the largest t with
    // |p - c(t)| = r(t), c(t) = F (1 - t), r(t) = f0 + t (1 - f0)
    vec2 F = vFocal.xy, q = vLocal - F;
    float f0 = vFocal.z, dr = 1.0 - f0;
    float A = dot(F, F) - dr * dr, B = dot(q, F) - f0 * dr, C = dot(q, q) - f0 * f0;
    float t;
    if (abs(A) < 1e-6) t = -C / (2.0 * B);
    else {
        float disc = max(B * B - A * C, 0.0), sq = sqrt(disc);
        float t1 = (-B + sq) / A, t2 = (-B - sq) / A;
        t = max(t1, t2);
        if (f0 + t * dr < 0.0) t = min(t1, t2);
    }
    t = clamp(t, 0.0, 1.0);
    float s1 = clamp(vFocal.w, 1e-4, 0.9999);
    vec4 c = t < s1 ? mix(vC0, vC1, t / s1) : mix(vC1, vC2, (t - s1) / (1.0 - s1));
    fragColor = vec4(c.rgb, c.a * edge);
}
)";
// The soft layer: its texture (premultiplied colours) stretched over the picture
static const char* kCompositeVS = R"(#version 330 core
layout(location = 0) in vec2 aCorner;
out vec2 vUV;
void main() { vUV = aCorner + 0.5; gl_Position = vec4(aCorner * 2.0, 0.0, 1.0); }
)";
static const char* kCompositeFS = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
out vec4 fragColor;
void main() { fragColor = texture(uTex, vUV); }
)";

// The grey pass: the picture's colours turned into their brightness (CSS grayscale(1) uses these weights)
static const char* kGrayFS = R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
out vec4 fragColor;
void main() { vec4 c = texture(uTex, vUV); float y = dot(c.rgb, vec3(0.2126, 0.7152, 0.0722)); fragColor = vec4(vec3(y), c.a); }
)";

// A turning planet: each pixel of the disc -> a point on the ball -> latitude and longitude on the
// planet (through the matrix uK) -> the Earth map, or a latitude/longitude grid. Then lit from the Sun.
static const char* kPlanetFS = R"(#version 330 core
in vec2 vUV;
in vec4 vColor;
in vec4 vParam;
uniform mat3 uK;
uniform int uEarth;
uniform float uPa;
uniform float uLod;
uniform sampler2D uTex;
out vec4 fragColor;
const float PI = 3.14159265358979;
void main() {
    float d = length(vUV);
    float aa = max(fwidth(d), 1e-4);
    float edge = 1.0 - smoothstep(1.0 - aa, 1.0 + aa, d);
    if (edge <= 0.0) discard;
    vec3 n = vec3(vUV.x, -vUV.y, sqrt(max(0.0, 1.0 - d * d)));   // screen: right, up, towards you
    vec3 b = uK * n;                                              // the planet's own frame: meridian, east, pole
    float lat = atan(b.z, sqrt(max(0.0, 1.0 - b.z * b.z)));
    float lon = atan(b.y, b.x);
    vec3 c;
    if (uEarth == 1) {
        c = textureLod(uTex, vec2(lon / (2.0 * PI) + 0.5, 0.5 - lat / PI), uLod).rgb;
    } else {                                                      // grid every 30°, the equator, and the 0° meridian brighter
        float L30 = PI / 6.0, cl = cos(lat), al = abs(lat);
        float dlat = abs(lat - round(lat / L30) * L30), dlon = abs(lon - round(lon / L30) * L30) * cl;
        float f = max(0.0, 1.0 - dlat / uPa) * 0.3;
        if (al < 1.45) f = max(f, max(0.0, 1.0 - dlon / uPa) * 0.3);
        if (al < uPa) f = max(f, (1.0 - al / uPa) * 0.6);
        float m0 = abs(lon) * cl;
        if (al < 1.5 && m0 < uPa * 1.4) f = max(f, (1.0 - m0 / (uPa * 1.4)) * 0.8);
        c = vColor.rgb + (vec3(1.0) - vColor.rgb) * f;
    }
    vec3 L = normalize(vec3(vParam.y, -vParam.z, 0.35));
    float lit = max(dot(n, L), 0.0);
    fragColor = vec4(c * (0.12 + 0.88 * lit) + vec3(0.2) * pow(lit, 12.0), vColor.a * edge);
}
)";

static GLuint compile(GLenum type, const char* src) {
    const GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        std::fprintf(stderr, "shader compile error: %s\n", log);
    }
    return s;
}

bool Renderer2D::init() {
    const GLuint vs = compile(GL_VERTEX_SHADER, kVertexShader), fs = compile(GL_FRAGMENT_SHADER, kFragmentShader);
    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(program_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(program_, sizeof log, nullptr, log);
        std::fprintf(stderr, "shader link error: %s\n", log);
        return false;
    }
    uSize_ = glGetUniformLocation(program_, "uSize");
    {   // the instanced-squares program
        const GLuint v = compile(GL_VERTEX_SHADER, kSquareVS), f = compile(GL_FRAGMENT_SHADER, kSquareFS);
        sqProgram_ = glCreateProgram();
        glAttachShader(sqProgram_, v);
        glAttachShader(sqProgram_, f);
        glLinkProgram(sqProgram_);
        glDeleteShader(v);
        glDeleteShader(f);
        glGetProgramiv(sqProgram_, GL_LINK_STATUS, &ok);
        if (!ok) { std::fprintf(stderr, "square shader link error\n"); return false; }
        sqSize_ = glGetUniformLocation(sqProgram_, "uSize");
        sqSide_ = glGetUniformLocation(sqProgram_, "uSide");
        sqColor_ = glGetUniformLocation(sqProgram_, "uColor");
        const float corners[12] = {-0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f};
        glGenVertexArrays(1, &sqVao_);
        glGenBuffers(1, &sqCornerVbo_);
        glGenBuffers(1, &sqInstVbo_);
        glBindVertexArray(sqVao_);
        glBindBuffer(GL_ARRAY_BUFFER, sqCornerVbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof corners, corners, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, sqInstVbo_);
        glEnableVertexAttribArray(1);
        glVertexAttribDivisor(1, 1);                      // one centre per square, not per corner
        glBindVertexArray(0);
    }
    {   // the blob program: same corner buffer as the squares, per-instance data from blobs_
        const GLuint v = compile(GL_VERTEX_SHADER, kBlobVS), f = compile(GL_FRAGMENT_SHADER, kBlobFS);
        blProgram_ = glCreateProgram();
        glAttachShader(blProgram_, v);
        glAttachShader(blProgram_, f);
        glLinkProgram(blProgram_);
        glDeleteShader(v);
        glDeleteShader(f);
        glGetProgramiv(blProgram_, GL_LINK_STATUS, &ok);
        if (!ok) { std::fprintf(stderr, "blob shader link error\n"); return false; }
        blSize_ = glGetUniformLocation(blProgram_, "uSize");
        blFuzz_ = glGetUniformLocation(blProgram_, "uFuzz");
        glGenVertexArrays(1, &blVao_);
        glGenBuffers(1, &blInstVbo_);
        glBindVertexArray(blVao_);
        glBindBuffer(GL_ARRAY_BUFFER, sqCornerVbo_);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, blInstVbo_);
        for (GLuint i = 1; i <= 5; i++) { glEnableVertexAttribArray(i); glVertexAttribDivisor(i, 1); }
        glBindVertexArray(0);
        // the soft layer's composite program
        const GLuint cv = compile(GL_VERTEX_SHADER, kCompositeVS), cf = compile(GL_FRAGMENT_SHADER, kCompositeFS);
        cmpProgram_ = glCreateProgram();
        glAttachShader(cmpProgram_, cv);
        glAttachShader(cmpProgram_, cf);
        glLinkProgram(cmpProgram_);
        glDeleteShader(cv);
        glDeleteShader(cf);
        glGetProgramiv(cmpProgram_, GL_LINK_STATUS, &ok);
        if (!ok) { std::fprintf(stderr, "composite shader link error\n"); return false; }
        cmpTex_ = glGetUniformLocation(cmpProgram_, "uTex");
        const GLuint gv = compile(GL_VERTEX_SHADER, kCompositeVS), gf = compile(GL_FRAGMENT_SHADER, kGrayFS);
        grayProgram_ = glCreateProgram();
        glAttachShader(grayProgram_, gv);
        glAttachShader(grayProgram_, gf);
        glLinkProgram(grayProgram_);
        glDeleteShader(gv);
        glDeleteShader(gf);
        glGetProgramiv(grayProgram_, GL_LINK_STATUS, &ok);
        if (!ok) { std::fprintf(stderr, "grey shader link error\n"); return false; }
        grayTexLoc_ = glGetUniformLocation(grayProgram_, "uTex");
        glGenVertexArrays(1, &cmpVao_);
        glBindVertexArray(cmpVao_);
        glBindBuffer(GL_ARRAY_BUFFER, sqCornerVbo_);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }
    {   // the planet program (same vertex shader as the shapes) and the Earth map
        const GLuint v = compile(GL_VERTEX_SHADER, kVertexShader), f = compile(GL_FRAGMENT_SHADER, kPlanetFS);
        plProgram_ = glCreateProgram();
        glAttachShader(plProgram_, v);
        glAttachShader(plProgram_, f);
        glLinkProgram(plProgram_);
        glDeleteShader(v);
        glDeleteShader(f);
        glGetProgramiv(plProgram_, GL_LINK_STATUS, &ok);
        if (!ok) { std::fprintf(stderr, "planet shader link error\n"); return false; }
        plSize_ = glGetUniformLocation(plProgram_, "uSize");
        plK_ = glGetUniformLocation(plProgram_, "uK");
        plEarth_ = glGetUniformLocation(plProgram_, "uEarth");
        plPa_ = glGetUniformLocation(plProgram_, "uPa");
        plLod_ = glGetUniformLocation(plProgram_, "uLod");
        plTex_ = glGetUniformLocation(plProgram_, "uTex");
        const std::vector<uint8_t> map = makeEarthTexture();
        glGenTextures(1, &earthTex_);
        glBindTexture(GL_TEXTURE_2D, earthTex_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kEarthTexW, kEarthTexH, 0, GL_RGBA, GL_UNSIGNED_BYTE, map.data());
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);         // longitude wraps round
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    const GLsizei stride = sizeof(Vertex);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(4 * sizeof(float)));
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(8 * sizeof(float)));
    for (GLuint i = 0; i < 4; i++) glEnableVertexAttribArray(i);
    glBindVertexArray(0);
    return true;
}

void Renderer2D::shutdown() {
    if (earthTex_) glDeleteTextures(1, &earthTex_);
    if (plProgram_) glDeleteProgram(plProgram_);
    earthTex_ = plProgram_ = 0;
    if (blInstVbo_) glDeleteBuffers(1, &blInstVbo_);
    if (blVao_) glDeleteVertexArrays(1, &blVao_);
    if (blProgram_) glDeleteProgram(blProgram_);
    if (cmpVao_) glDeleteVertexArrays(1, &cmpVao_);
    if (grayProgram_) glDeleteProgram(grayProgram_);
    if (grayTex_) glDeleteTextures(1, &grayTex_);
    if (grayFbo_) glDeleteFramebuffers(1, &grayFbo_);
    grayProgram_ = grayTex_ = grayFbo_ = 0;
    if (cmpProgram_) glDeleteProgram(cmpProgram_);
    if (layerTex_) glDeleteTextures(1, &layerTex_);
    if (layerFbo_) glDeleteFramebuffers(1, &layerFbo_);
    blInstVbo_ = blVao_ = blProgram_ = cmpVao_ = cmpProgram_ = layerTex_ = layerFbo_ = 0;
    if (sqCornerVbo_) glDeleteBuffers(1, &sqCornerVbo_);
    if (sqInstVbo_) glDeleteBuffers(1, &sqInstVbo_);
    if (sqVao_) glDeleteVertexArrays(1, &sqVao_);
    if (sqProgram_) glDeleteProgram(sqProgram_);
    sqCornerVbo_ = sqInstVbo_ = sqVao_ = sqProgram_ = 0;
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (program_) glDeleteProgram(program_);
    vbo_ = vao_ = program_ = 0;
}

void Renderer2D::begin(double w, double h) {
    width_ = w > 1 ? w : 1;
    height_ = h > 1 ? h : 1;
    verts_.clear();
    inst_.clear();
    blobs_.clear();
    segments_.clear();
    triStart_ = 0;
}

void Renderer2D::closeTriangles() {
    if (verts_.size() > triStart_) {
        Segment sg{};
        sg.kind = Kind::Triangles; sg.first = static_cast<int>(triStart_); sg.count = static_cast<int>(verts_.size() - triStart_);
        segments_.push_back(sg);
    }
    triStart_ = verts_.size();
}

void Renderer2D::planet(double x, double y, double r, Color base, double lx, double ly, const float K[9], bool earth) {
    if (r <= 0) return;
    closeTriangles();
    const double half = r + 1.5;
    quad(x, y, half, base, 2, static_cast<float>(lx), static_cast<float>(ly), static_cast<float>(half / r));
    Segment sg{};
    sg.kind = Kind::Planet; sg.first = static_cast<int>(triStart_); sg.count = 6; sg.earth = earth; sg.r = static_cast<float>(r);
    for (int i = 0; i < 9; i++) sg.K[i] = K[i];
    segments_.push_back(sg);
    triStart_ = verts_.size();
}

void Renderer2D::squares(const float* xy, int count, double size, Color c) {
    if (count <= 0) return;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::Squares; sg.first = static_cast<int>(inst_.size() / 2); sg.count = count; sg.size = static_cast<float>(size); sg.c = c;
    segments_.push_back(sg);
    inst_.insert(inst_.end(), xy, xy + 2 * count);
}

void Renderer2D::blobs(const Blob* b, int count, bool additive) {
    if (count <= 0) return;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::Blobs; sg.first = static_cast<int>(blobs_.size()); sg.count = count; sg.additive = additive;
    segments_.push_back(sg);
    blobs_.insert(blobs_.end(), b, b + count);
}

void Renderer2D::beginSoftLayer(float scale) {
    closeTriangles();
    layerScale_ = scale;
    Segment sg{};
    sg.kind = Kind::LayerBegin;
    segments_.push_back(sg);
}

void Renderer2D::endSoftLayer() {
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::LayerEnd;
    segments_.push_back(sg);
}

// the fuzzy nucleon's profile: a proton's charge density falls off as exp(−r/r0), r0 = rms radius/√12;
// seen from the side that is the column density x·K1(x), x = b/r0. Tabulated out to 1.9× the rms radius (the web's FUZZ).
static const float* fuzzTable() {
    static float t[17];
    static bool done = false;
    if (!done) {
        auto K1 = [](double x) { double sum = 0; for (double u = 0; u < 12; u += 0.02) sum += std::exp(-x * std::cosh(u)) * std::cosh(u) * 0.02; return sum; };
        for (int i = 0; i < 17; i++) { const double x = i / 16.0 * 1.9 / (1 / std::sqrt(12.0)); t[i] = i == 0 ? 1.f : static_cast<float>(std::min(1.0, x * K1(x))); }
        done = true;
    }
    return t;
}

void Renderer2D::end(int fbW, int fbH, int fbY, GLuint outFbo) {
    if (outFbo) fbY = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, outFbo);
    glViewport(0, fbY, fbW, fbH);                         // the picture fills fbW × fbH pixels of the window, fbY above its bottom edge
    closeTriangles();
    lastCalls_ = 0; lastInstances_ = 0;
    for (const Segment& sg : segments_) {
        if (sg.kind != Kind::LayerBegin) lastCalls_++;
        if (sg.kind == Kind::Squares || sg.kind == Kind::Blobs) lastInstances_ += sg.count;
    }
    if (segments_.empty()) return;
    // Monochrome: draw into an offscreen picture first, then copy it over in grey (at the bottom of end())
    GLuint target = outFbo;
    int vx = 0, vy = fbY;
    if (gray_) {
        if (!grayFbo_) { glGenFramebuffers(1, &grayFbo_); glGenTextures(1, &grayTex_); }
        if (fbW != grayW_ || fbH != grayH_) {
            grayW_ = fbW; grayH_ = fbH;
            glBindTexture(GL_TEXTURE_2D, grayTex_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fbW, fbH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindFramebuffer(GL_FRAMEBUFFER, grayFbo_);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, grayTex_, 0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        target = grayFbo_; vy = 0;
        glBindFramebuffer(GL_FRAMEBUFFER, grayFbo_);
        glViewport(0, 0, fbW, fbH);
        glClearColor(grayBg_.r, grayBg_.g, grayBg_.b, 1);   // the page behind the picture, so the copy is opaque
        glClear(GL_COLOR_BUFFER_BIT);
    }
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    // upload this frame's shapes and square centres once, then draw them in order
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts_.size() * sizeof(Vertex)), verts_.data(), GL_STREAM_DRAW);
    if (!inst_.empty()) {
        glBindBuffer(GL_ARRAY_BUFFER, sqInstVbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(inst_.size() * sizeof(float)), inst_.data(), GL_STREAM_DRAW);
    }
    if (!blobs_.empty()) {
        glBindBuffer(GL_ARRAY_BUFFER, blInstVbo_);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(blobs_.size() * sizeof(Blob)), blobs_.data(), GL_STREAM_DRAW);
    }
    auto normalBlend = [] { glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA); };
    for (const Segment& sg : segments_) {
        if (sg.kind == Kind::LayerBegin) {
            const int w = std::max(1, static_cast<int>(std::ceil(fbW * layerScale_))), h = std::max(1, static_cast<int>(std::ceil(fbH * layerScale_)));
            if (!layerFbo_) { glGenFramebuffers(1, &layerFbo_); glGenTextures(1, &layerTex_); }
            if (w != layerW_ || h != layerH_) {
                layerW_ = w; layerH_ = h;
                glBindTexture(GL_TEXTURE_2D, layerTex_);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glBindFramebuffer(GL_FRAMEBUFFER, layerFbo_);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, layerTex_, 0);
                glBindTexture(GL_TEXTURE_2D, 0);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, layerFbo_);
            glViewport(0, 0, w, h);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            continue;
        }
        if (sg.kind == Kind::LayerEnd) {
            glBindFramebuffer(GL_FRAMEBUFFER, target);
            glViewport(vx, vy, fbW, fbH);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);     // the layer holds premultiplied colours
            glUseProgram(cmpProgram_);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, layerTex_);
            glUniform1i(cmpTex_, 0);
            glBindVertexArray(cmpVao_);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindTexture(GL_TEXTURE_2D, 0);
            normalBlend();
            continue;
        }
        if (sg.kind == Kind::Blobs) {
            if (sg.additive) glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
            glUseProgram(blProgram_);
            glUniform2f(blSize_, static_cast<float>(width_), static_cast<float>(height_));
            glUniform1fv(blFuzz_, 17, fuzzTable());
            glBindVertexArray(blVao_);
            glBindBuffer(GL_ARRAY_BUFFER, blInstVbo_);
            const std::size_t base = static_cast<std::size_t>(sg.first) * sizeof(Blob);
            const GLsizei stride = sizeof(Blob);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(base));
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(base + 4 * sizeof(float)));
            glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(base + 8 * sizeof(float)));
            glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(base + 12 * sizeof(float)));
            glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(base + 16 * sizeof(float)));
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, sg.count);
            if (sg.additive) normalBlend();
            continue;
        }
        if (sg.kind == Kind::Planet) {
            glUseProgram(plProgram_);
            glUniform2f(plSize_, static_cast<float>(width_), static_cast<float>(height_));
            // uK * n: GLSL matrices are column-major, so the rows of K go in as columns (transpose = true)
            glUniformMatrix3fv(plK_, 1, GL_TRUE, sg.K);
            glUniform1i(plEarth_, sg.earth ? 1 : 0);
            glUniform1f(plPa_, 2.2f / std::max(2.0f * sg.r, 1.0f));        // grid line width: about one pixel
            glUniform1f(plLod_, std::max(0.0f, std::log2(static_cast<float>(kEarthTexW) / (3.14159f * std::max(2.0f * sg.r, 1.0f)))));   // texels per pixel across the middle
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, earthTex_);
            glUniform1i(plTex_, 0);
            glBindVertexArray(vao_);
            glDrawArrays(GL_TRIANGLES, sg.first, sg.count);
        } else if (sg.kind == Kind::Triangles) {
            glUseProgram(program_);
            glUniform2f(uSize_, static_cast<float>(width_), static_cast<float>(height_));
            glBindVertexArray(vao_);
            glDrawArrays(GL_TRIANGLES, sg.first, sg.count);
        } else {
            glUseProgram(sqProgram_);
            glUniform2f(sqSize_, static_cast<float>(width_), static_cast<float>(height_));
            glUniform1f(sqSide_, sg.size);
            glUniform4f(sqColor_, sg.c.r, sg.c.g, sg.c.b, sg.c.a);
            glBindVertexArray(sqVao_);
            glBindBuffer(GL_ARRAY_BUFFER, sqInstVbo_);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(static_cast<std::size_t>(sg.first) * 2 * sizeof(float)));
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, sg.count);
        }
    }
    if (gray_) {                                          // copy the offscreen picture over the window, in grey
        glBindFramebuffer(GL_FRAMEBUFFER, outFbo);
        glViewport(0, fbY, fbW, fbH);
        glDisable(GL_BLEND);
        glUseProgram(grayProgram_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, grayTex_);
        glUniform1i(grayTexLoc_, 0);
        glBindVertexArray(cmpVao_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindTexture(GL_TEXTURE_2D, 0);
        glEnable(GL_BLEND);
        normalBlend();
    }
    glBindVertexArray(0);
    glUseProgram(0);
}

void Renderer2D::push(double x, double y, float u, float v, Color c, float mode, float p1, float p2) {
    verts_.push_back({static_cast<float>(x), static_cast<float>(y), u, v, c.r, c.g, c.b, c.a, mode, p1, p2, 0});
}

void Renderer2D::rect(double x, double y, double w, double h, Color c) {
    push(x, y, 0, 0, c, 0); push(x + w, y, 0, 0, c, 0); push(x + w, y + h, 0, 0, c, 0);
    push(x, y, 0, 0, c, 0); push(x + w, y + h, 0, 0, c, 0); push(x, y + h, 0, 0, c, 0);
}

void Renderer2D::triangle(double x0, double y0, Color c0, double x1, double y1, Color c1, double x2, double y2, Color c2) {
    push(x0, y0, 0, 0, c0, 0); push(x1, y1, 0, 0, c1, 0); push(x2, y2, 0, 0, c2, 0);
}

void Renderer2D::line(double x0, double y0, double x1, double y1, double width, Color c) {
    const double dx = x1 - x0, dy = y1 - y0, len = std::hypot(dx, dy);
    if (len < 1e-9) return;
    // thin lines are drawn 1 point wide with lower alpha, so they do not flicker
    double w = width;
    if (w < 1) { c.a *= static_cast<float>(w); w = 1; }
    const double nx = -dy / len * w / 2, ny = dx / len * w / 2;
    push(x0 + nx, y0 + ny, 0, 0, c, 0); push(x1 + nx, y1 + ny, 0, 0, c, 0); push(x1 - nx, y1 - ny, 0, 0, c, 0);
    push(x0 + nx, y0 + ny, 0, 0, c, 0); push(x1 - nx, y1 - ny, 0, 0, c, 0); push(x0 - nx, y0 - ny, 0, 0, c, 0);
}

void Renderer2D::quad(double cx, double cy, double half, Color c, float mode, float p1, float p2, float s) {
    push(cx - half, cy - half, -s, -s, c, mode, p1, p2); push(cx + half, cy - half, s, -s, c, mode, p1, p2); push(cx + half, cy + half, s, s, c, mode, p1, p2);
    push(cx - half, cy - half, -s, -s, c, mode, p1, p2); push(cx + half, cy + half, s, s, c, mode, p1, p2); push(cx - half, cy + half, -s, s, c, mode, p1, p2);
}

void Renderer2D::disc(double x, double y, double r, Color c) {
    if (r <= 0) return;
    const double half = r + 1.5;                          // a little room for the smooth edge
    quad(x, y, half, c, 1, 0, 0, static_cast<float>(half / r));
}

void Renderer2D::sphere(double x, double y, double r, Color c, double lx, double ly) {
    if (r <= 0) return;
    const double half = r + 1.5;
    quad(x, y, half, c, 2, static_cast<float>(lx), static_cast<float>(ly), static_cast<float>(half / r));
}

void Renderer2D::sunDisc(double x, double y, double r, Color c) {
    if (r <= 0) return;
    const double half = r + 1.5;
    quad(x, y, half, c, 3, 0, 0, static_cast<float>(half / r));
}

void Renderer2D::glow(double x, double y, double r, Color c) {
    if (r <= 0) return;
    quad(x, y, r, c, 4, 0, 0, 1);
}

void Renderer2D::fadeDisc(double x, double y, double r, Color c) {
    if (r <= 0) return;
    quad(x, y, r, c, 5, 0, 0, 1);
}

void Renderer2D::ring(double x, double y, double r, double width, Color c) {
    const int n = static_cast<int>(std::fmin(720, std::fmax(24, r * 0.75)));
    double px = x + r, py = y;
    for (int i = 1; i <= n; i++) {
        const double t = i * 6.283185307179586 / n, qx = x + r * std::cos(t), qy = y + r * std::sin(t);
        line(px, py, qx, qy, width, c);
        px = qx; py = qy;
    }
}

}  // namespace render
