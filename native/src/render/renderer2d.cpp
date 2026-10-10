#include "renderer2d.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstddef>
#include <cstdlib>

#include "earth_texture.h"
#include "group_compute.h"
#include "outline_compute.h"
#include "rock_compute.h"
#include "shaders.h"

using namespace plgl;

namespace render {

Color rgba(int r, int g, int b, float a) { return {r / 255.f, g / 255.f, b / 255.f, a}; }

Color hexColor(const char* hex, float alpha) {
    if (!hex || hex[0] != '#') return {1, 1, 1, alpha};
    const unsigned long v = std::strtoul(hex + 1, nullptr, 16);
    return rgba(static_cast<int>((v >> 16) & 255), static_cast<int>((v >> 8) & 255), static_cast<int>(v & 255), alpha);
}

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
        blPalWheel_ = glGetUniformLocation(blProgram_, "uPalWheel");
        blPalLight_ = glGetUniformLocation(blProgram_, "uPalLight");
        blPalPlus_ = glGetUniformLocation(blProgram_, "uPalPlus");
        blPalMinus_ = glGetUniformLocation(blProgram_, "uPalMinus");
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
    {   // the retained-mesh program (the chart of nuclides): its own vertex shader, the shapes' fragment shader
        const GLuint v = compile(GL_VERTEX_SHADER, kChartVS), f = compile(GL_FRAGMENT_SHADER, kFragmentShader);
        meshProgram_ = glCreateProgram();
        glAttachShader(meshProgram_, v);
        glAttachShader(meshProgram_, f);
        glLinkProgram(meshProgram_);
        glDeleteShader(v);
        glDeleteShader(f);
        glGetProgramiv(meshProgram_, GL_LINK_STATUS, &ok);
        if (!ok) { std::fprintf(stderr, "mesh shader link error\n"); return false; }
        mSize_ = glGetUniformLocation(meshProgram_, "uSize");
        mRot_ = glGetUniformLocation(meshProgram_, "uRot");
        mCen_ = glGetUniformLocation(meshProgram_, "uCen");
        mFit_ = glGetUniformLocation(meshProgram_, "uFit");
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
    // OpenGL 4.3 (0.13 stage 2): the compute path for superpositions and jumps, if this context has it. A 3.3 context (or any failure) leaves compute_ empty: the CPU paths are used, nothing else changes.
    compute_ = makeGroupCompute();
    outline_ = makeOutlineCompute();
    rocks_ = makeRockCompute();
    return true;
}

void Renderer2D::disableCompute() { delete compute_; compute_ = nullptr; delete outline_; outline_ = nullptr; delete rocks_; rocks_ = nullptr; }

void Renderer2D::shutdown() {
    for (int i = 0; i < int(meshes_.size()); i++) meshDelete(i);
    meshes_.clear();
    if (meshProgram_) glDeleteProgram(meshProgram_);
    meshProgram_ = 0;
    delete compute_; compute_ = nullptr;
    delete outline_; outline_ = nullptr;
    delete rocks_; rocks_ = nullptr;
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

int Renderer2D::meshCreate(const std::vector<MeshVertex>& v) {
    int id = -1;
    for (int i = 0; i < int(meshes_.size()); i++) if (!meshes_[std::size_t(i)].used) { id = i; break; }
    if (id < 0) { id = int(meshes_.size()); meshes_.emplace_back(); }
    Mesh& m = meshes_[std::size_t(id)];
    m.used = true; m.count = static_cast<int>(v.size());
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(MeshVertex)), v.data(), GL_STATIC_DRAW);
    const GLsizei stride = sizeof(MeshVertex);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(MeshVertex, p0)));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(MeshVertex, p1)));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(MeshVertex, rgba)));
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(MeshVertex, corner)));
    for (GLuint i = 0; i < 4; i++) glEnableVertexAttribArray(i);
    glBindVertexArray(0);
    meshUploads_ += static_cast<long>(v.size());
    return id;
}

void Renderer2D::meshDelete(int id) {
    if (id < 0 || id >= int(meshes_.size()) || !meshes_[std::size_t(id)].used) return;
    Mesh& m = meshes_[std::size_t(id)];
    if (m.vbo) glDeleteBuffers(1, &m.vbo);
    if (m.vao) glDeleteVertexArrays(1, &m.vao);
    m = Mesh();
}

int Renderer2D::meshesAlive() const { int n = 0; for (const Mesh& m : meshes_) n += m.used; return n; }

void Renderer2D::drawMesh(int id, int first, int count, const MeshProjection& P) {
    if (id < 0 || id >= int(meshes_.size()) || !meshes_[std::size_t(id)].used || count <= 0) return;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::Mesh; sg.slot = id; sg.first = first; sg.count = count; sg.mp = P;
    segments_.push_back(sg);
}

bool Renderer2D::rocksGpu(const RockSet& set, const RockFrame& f, double size, Color c) {
    if (!rocks_ || !rocks_->ok() || set.count <= 0) return false;
    const int slot = rocks_->slotFor(set);
    if (slot < 0) return false;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::RocksGpu; sg.first = 0; sg.count = set.count; sg.size = static_cast<float>(size); sg.c = c; sg.slot = slot; sg.rf = f;
    segments_.push_back(sg);
    return true;
}

void Renderer2D::blobs(const Blob* b, int count, bool additive) { blobsPalette(b, count, additive, PhasePalette()); }

void Renderer2D::blobsPalette(const Blob* b, int count, bool additive, const PhasePalette& pal) {
    if (count <= 0) return;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::Blobs; sg.first = static_cast<int>(blobs_.size()); sg.count = count; sg.additive = additive; sg.pal = pal;
    segments_.push_back(sg);
    blobs_.insert(blobs_.end(), b, b + count);
}

void Renderer2D::blobsPaletteTake(std::vector<Blob>& b, bool additive, const PhasePalette& pal) {
    if (b.empty()) return;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::Blobs; sg.first = static_cast<int>(blobs_.size()); sg.count = static_cast<int>(b.size()); sg.additive = additive; sg.pal = pal;
    segments_.push_back(sg);
    if (blobs_.empty()) blobs_.swap(b);
    else blobs_.insert(blobs_.end(), b.begin(), b.end());
    b.clear();
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
    lastCalls_ = 0; lastInstances_ = 0; lastVerts_ = static_cast<long>(verts_.size());
    for (const Segment& sg : segments_) {
        if (sg.kind != Kind::LayerBegin) lastCalls_++;
        if (sg.kind == Kind::Squares || sg.kind == Kind::Blobs || sg.kind == Kind::RocksGpu) lastInstances_ += sg.count;
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
            glUniform1i(blPalWheel_, sg.pal.wheel ? 1 : 0);
            glUniform1i(blPalLight_, sg.pal.light ? 1 : 0);
            glUniform4f(blPalPlus_, sg.pal.sign[0].r, sg.pal.sign[0].g, sg.pal.sign[0].b, sg.pal.sign[0].a);
            glUniform4f(blPalMinus_, sg.pal.sign[1].r, sg.pal.sign[1].g, sg.pal.sign[1].b, sg.pal.sign[1].a);
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
        if (sg.kind == Kind::Mesh) {
            const Mesh& m = meshes_[std::size_t(sg.slot)];
            if (!m.used) continue;
            const MeshProjection& P = sg.mp;
            glUseProgram(meshProgram_);
            glUniform2f(mSize_, static_cast<float>(width_), static_cast<float>(height_));
            glUniform4f(mRot_, static_cast<float>(P.cosY), static_cast<float>(P.sinY), static_cast<float>(P.cosP), static_cast<float>(P.sinP));
            glUniform4f(mCen_, static_cast<float>(P.cx), static_cast<float>(P.cy), static_cast<float>(P.K), static_cast<float>(P.fx));
            glUniform4f(mFit_, static_cast<float>(P.fy), P.identity ? 1.f : 0.f, P.flat ? 1.f : 0.f, 0.f);
            glBindVertexArray(m.vao);
            glDrawArrays(GL_TRIANGLES, sg.first, sg.count);
            continue;
        }
        if (sg.kind == Kind::RocksGpu) {
            if (!rocks_ || !rocks_->dispatch(sg.slot, sg.rf)) continue;     // the positions of every rock of this belt, on the GPU
            glUseProgram(sqProgram_);
            glUniform2f(sqSize_, static_cast<float>(width_), static_cast<float>(height_));
            glUniform1f(sqSide_, sg.size);
            glUniform4f(sqColor_, sg.c.r, sg.c.g, sg.c.b, sg.c.a);
            glBindVertexArray(sqVao_);
            glBindBuffer(GL_ARRAY_BUFFER, rocks_->positionsBuffer(sg.slot));
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, sg.count);
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
