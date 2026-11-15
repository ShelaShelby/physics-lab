#include "renderer2d.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstddef>
#include <cstdlib>

#include "group_compute.h"
#include "outline_compute.h"
#include "rock_compute.h"
#include "shaders.h"
#include "shading.h"

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
    uSpace_ = glGetUniformLocation(program_, "uSpace");
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
        blSph_ = glGetUniformLocation(blProgram_, "uSph");
        blSphN_ = glGetUniformLocation(blProgram_, "uSphN");
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
        // 0.46 (C4): the GPU cloud: its own vertex shader with the blob fragment shader; the grid pass. A link failure only turns the GPU cloud off (the CPU path stays).
        {   const GLuint v2 = compile(GL_VERTEX_SHADER, kCloudVS), f2 = compile(GL_FRAGMENT_SHADER, kBlobFS);
            clProgram_ = glCreateProgram(); glAttachShader(clProgram_, v2); glAttachShader(clProgram_, f2); glLinkProgram(clProgram_); glDeleteShader(v2); glDeleteShader(f2);
            GLint ok2 = 0; glGetProgramiv(clProgram_, GL_LINK_STATUS, &ok2);
            const GLuint v3 = compile(GL_VERTEX_SHADER, kCloudGridVS), f3 = compile(GL_FRAGMENT_SHADER, kCloudGridFS);
            grProgram_ = glCreateProgram(); glAttachShader(grProgram_, v3); glAttachShader(grProgram_, f3); glLinkProgram(grProgram_); glDeleteShader(v3); glDeleteShader(f3);
            GLint ok3 = 0; glGetProgramiv(grProgram_, GL_LINK_STATUS, &ok3);
            if (!ok2 || !ok3) { std::fprintf(stderr, "GPU cloud shader link error: the CPU path is used\n"); glDeleteProgram(clProgram_); glDeleteProgram(grProgram_); clProgram_ = grProgram_ = 0; }
            else {
                clSize_ = glGetUniformLocation(clProgram_, "uSize"); clCam0_ = glGetUniformLocation(clProgram_, "uCam0"); clCam1_ = glGetUniformLocation(clProgram_, "uCam1");
                clView_ = glGetUniformLocation(clProgram_, "uView"); clLight_ = glGetUniformLocation(clProgram_, "uLight"); clPalWheel_ = glGetUniformLocation(clProgram_, "uPalWheel");
                clPalLight_ = glGetUniformLocation(clProgram_, "uPalLight"); clPalPlus_ = glGetUniformLocation(clProgram_, "uPalPlus"); clPalMinus_ = glGetUniformLocation(clProgram_, "uPalMinus");
                clFuzz_ = glGetUniformLocation(clProgram_, "uFuzz"); clSphN_ = glGetUniformLocation(clProgram_, "uSphN");
                grCam0_ = glGetUniformLocation(grProgram_, "uCam0"); grCam1_ = glGetUniformLocation(grProgram_, "uCam1"); grView_ = glGetUniformLocation(grProgram_, "uView"); grGrid_ = glGetUniformLocation(grProgram_, "uGrid");
                glGenBuffers(1, &clPosVbo_); glGenBuffers(1, &clGrpVbo_);
                glGenVertexArrays(1, &clVao_);
                glBindVertexArray(clVao_);
                glBindBuffer(GL_ARRAY_BUFFER, sqCornerVbo_);
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0)); glEnableVertexAttribArray(0);
                glBindBuffer(GL_ARRAY_BUFFER, clPosVbo_);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(0)); glEnableVertexAttribArray(1); glVertexAttribDivisor(1, 1);
                glBindBuffer(GL_ARRAY_BUFFER, clGrpVbo_);
                glVertexAttribPointer(2, 1, GL_UNSIGNED_BYTE, GL_FALSE, 1, reinterpret_cast<void*>(0)); glEnableVertexAttribArray(2); glVertexAttribDivisor(2, 1);
                glGenVertexArrays(1, &grVao_);                 // the grid pass: one point per dot (no corners, no divisor)
                glBindVertexArray(grVao_);
                glBindBuffer(GL_ARRAY_BUFFER, clPosVbo_);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(0)); glEnableVertexAttribArray(1);
                glBindBuffer(GL_ARRAY_BUFFER, clGrpVbo_);
                glVertexAttribPointer(2, 1, GL_UNSIGNED_BYTE, GL_FALSE, 1, reinterpret_cast<void*>(0)); glEnableVertexAttribArray(2);
                glBindVertexArray(0);
            }
        }
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
        // 0.55.0: the Black Hole Lab's bent sky (drawn over the whole picture with the composite's square)
        const GLuint lv = compile(GL_VERTEX_SHADER, kCompositeVS), lf = compile(GL_FRAGMENT_SHADER, kLensFS);
        lensProgram_ = glCreateProgram();
        glAttachShader(lensProgram_, lv);
        glAttachShader(lensProgram_, lf);
        glLinkProgram(lensProgram_);
        glDeleteShader(lv);
        glDeleteShader(lf);
        glGetProgramiv(lensProgram_, GL_LINK_STATUS, &ok);
        if (!ok) { std::fprintf(stderr, "lens shader link error\n"); glDeleteProgram(lensProgram_); lensProgram_ = 0; }   // (the Black Hole Lab then shows black; the other labs are unaffected)
        else {
            lnTex_ = glGetUniformLocation(lensProgram_, "uTex"); lnTable_ = glGetUniformLocation(lensProgram_, "uTable"); lnK_ = glGetUniformLocation(lensProgram_, "uK");
            lnCentre_ = glGetUniformLocation(lensProgram_, "uCentre"); lnKpx_ = glGetUniformLocation(lensProgram_, "uKpx"); lnEdge_ = glGetUniformLocation(lensProgram_, "uEdge");
            lnT_ = glGetUniformLocation(lensProgram_, "uT"); lnN_ = glGetUniformLocation(lensProgram_, "uN"); lnBend_ = glGetUniformLocation(lensProgram_, "uBend");
            lnDebug_ = glGetUniformLocation(lensProgram_, "uDebug"); lnBright_ = glGetUniformLocation(lensProgram_, "uBright"); lnLod_ = glGetUniformLocation(lensProgram_, "uLod");
            lnDisc_ = glGetUniformLocation(lensProgram_, "uDisc"); lnDU_ = glGetUniformLocation(lensProgram_, "uDU"); lnDP_ = glGetUniformLocation(lensProgram_, "uDP");
            lnDC_ = glGetUniformLocation(lensProgram_, "uDC"); lnDN_ = glGetUniformLocation(lensProgram_, "uDN"); lnDS_ = glGetUniformLocation(lensProgram_, "uDS");
            lnDA_ = glGetUniformLocation(lensProgram_, "uDA"); lnDR_ = glGetUniformLocation(lensProgram_, "uDR"); lnDT_ = glGetUniformLocation(lensProgram_, "uDT");
            lnSky_ = glGetUniformLocation(lensProgram_, "uSky");
        }
    }
    {   // the planet program (same vertex shader as the shapes; it also draws the sky photo) and the picture maps
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
        plSpace_ = glGetUniformLocation(plProgram_, "uSpace");
        plLunar_ = glGetUniformLocation(plProgram_, "uLunar");
        plK_ = glGetUniformLocation(plProgram_, "uK");
        plMode_ = glGetUniformLocation(plProgram_, "uMode");
        plLod_ = glGetUniformLocation(plProgram_, "uLod");
        plTex_ = glGetUniformLocation(plProgram_, "uTex");
        plLon0_ = glGetUniformLocation(plProgram_, "uLon0");
        plRingN_ = glGetUniformLocation(plProgram_, "uRingN");
        plRingB_ = glGetUniformLocation(plProgram_, "uRingB");
        plShape_ = glGetUniformLocation(plProgram_, "uShape");
        plOccN_ = glGetUniformLocation(plProgram_, "uOccN");
        plOcc_ = glGetUniformLocation(plProgram_, "uOcc");
        plSunAng_ = glGetUniformLocation(plProgram_, "uSunAng");
        plAir_ = glGetUniformLocation(plProgram_, "uAir");
        plAirT_ = glGetUniformLocation(plProgram_, "uAirT");
        plAirK_ = glGetUniformLocation(plProgram_, "uAirK");
        {   // 0.51.2: Earth's air table (render/shading.h airTableM, airTransmit): set once, the program keeps it
            float t[3 * kAirTableN];
            for (int i = 0; i < kAirTableN; i++) for (int c = 0; c < 3; c++) t[3 * i + c] = static_cast<float>(airTransmit(c, airTableM(i)));
            glUseProgram(plProgram_);
            glUniform3fv(plAirT_, kAirTableN, t);
            const float k3[3] = {static_cast<float>(kEarthAirK), static_cast<float>(airTableLnLo()), static_cast<float>(airTableLnHi())};
            glUniform3fv(plAirK_, 1, k3);
            glUseProgram(0);
        }
        // 0.26: the picture maps (planet_maps.h). A map that cannot be read is left out: that body is drawn as a plain ball, the sky without the photo.
        for (int m = 1; m < static_cast<int>(Map::Count); m++) {
            MapImage img;
            if (!decodeMap(static_cast<Map>(m), img)) { std::fprintf(stderr, "map %s missing\n", mapInfo(static_cast<Map>(m)).file); continue; }
            glGenTextures(1, &tex_[m]);
            glBindTexture(GL_TEXTURE_2D, tex_[m]);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            // 0.50: the planet maps are sRGB-encoded photos: GL_SRGB8_ALPHA8 makes the GPU decode them to linear light (and average the mipmaps in linear light) for the
            // planet shader's Lambert step (render/shading.h). The sky photo stays GL_RGBA8: it is shown as it is, not lit.
            glTexImage2D(GL_TEXTURE_2D, 0, m == static_cast<int>(Map::Sky) ? GL_RGBA8 : GL_SRGB8_ALPHA8, img.w, img.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);         // longitude wraps round
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            texW_[m] = img.w;
        }
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
    if (lensProgram_) glDeleteProgram(lensProgram_);
    if (lensFbo_) glDeleteFramebuffers(1, &lensFbo_);
    if (lensTex_) glDeleteTextures(1, &lensTex_);
    if (lensTableTex_) glDeleteTextures(1, &lensTableTex_);
    for (GLuint* tx : {&discUTex_, &discPTex_, &discCTex_}) if (*tx) { glDeleteTextures(1, tx); *tx = 0; }
    lensProgram_ = lensFbo_ = lensTex_ = lensTableTex_ = 0; lensW_ = lensH_ = lensN_ = 0; lensValid_ = false; lensTableVer_ = ~std::uint64_t(0);
    discVer_ = colVer_ = ~std::uint64_t(0);
    for (int i = 0; i < int(meshes_.size()); i++) meshDelete(i);
    meshes_.clear();
    if (meshProgram_) glDeleteProgram(meshProgram_);
    meshProgram_ = 0;
    delete compute_; compute_ = nullptr;
    delete outline_; outline_ = nullptr;
    delete rocks_; rocks_ = nullptr;
    for (GLuint& t : tex_) { if (t) glDeleteTextures(1, &t); t = 0; }
    if (plProgram_) glDeleteProgram(plProgram_);
    plProgram_ = 0;
    if (clPosVbo_) glDeleteBuffers(1, &clPosVbo_);
    if (clGrpVbo_) glDeleteBuffers(1, &clGrpVbo_);
    if (clVao_) glDeleteVertexArrays(1, &clVao_);
    if (grVao_) glDeleteVertexArrays(1, &grVao_);
    if (clProgram_) glDeleteProgram(clProgram_);
    if (grProgram_) glDeleteProgram(grProgram_);
    if (grTex_) glDeleteTextures(1, &grTex_);
    if (grFbo_) glDeleteFramebuffers(1, &grFbo_);
    clPosVbo_ = clGrpVbo_ = clVao_ = grVao_ = clProgram_ = grProgram_ = grTex_ = grFbo_ = 0; grW_ = grH_ = 0; clK_ = -1; clEpoch_ = 0;
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
    if (hdrTex_) glDeleteTextures(1, &hdrTex_);
    if (hdrFbo_) glDeleteFramebuffers(1, &hdrFbo_);
    hdrTex_ = hdrFbo_ = 0; hdrW_ = hdrH_ = 0;
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
    clouds_.clear();
    lens_.clear();
    segments_.clear();
    triStart_ = 0;
}

void Renderer2D::lensSky(const LensSky& L) {
    if (!hasMap(Map::Sky) || L.kRad <= 0) return;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::Lens; sg.slot = static_cast<int>(lens_.size());
    lens_.push_back(L);
    segments_.push_back(sg);
}

void Renderer2D::drawLens(const LensSky& L, int fbW, int fbH, GLuint target, int vx, int vy) {
    if (!lensProgram_) return;
    const int n = L.table ? static_cast<int>(L.table->size()) : 0;
    const bool bend = L.bend && n >= 4;
    if (bend && L.tableVersion != lensTableVer_) {                         // a new table: to the GPU once
        std::vector<float> f(L.table->begin(), L.table->end());
        if (!lensTableTex_) glGenTextures(1, &lensTableTex_);
        glBindTexture(GL_TEXTURE_2D, lensTableTex_);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, n, 1, 0, GL_RED, GL_FLOAT, f.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        lensTableVer_ = L.tableVersion; lensN_ = n;
    }
    const bool disc = bend && L.disc && L.discU && L.discP && L.discCol && L.discS >= 4 && L.discT >= 4 && L.discC >= 2;
    const auto upload = [](GLuint& tex, int w, int h, GLenum ifmt, GLenum fmt, const float* data) {
        if (!tex) glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexImage2D(GL_TEXTURE_2D, 0, ifmt, w, h, 0, fmt, GL_FLOAT, data);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    };
    if (disc && L.discVersion != discVer_) {                               // a new disc table: to the GPU once
        upload(discUTex_, L.discT, 2 * L.discS, GL_R32F, GL_RED, L.discU->data());
        upload(discPTex_, 2 * L.discS, 1, GL_R32F, GL_RED, L.discP->data());
        discVer_ = L.discVersion;
    }
    if (disc && L.colVersion != colVer_) { upload(discCTex_, L.discC, 1, GL_RGB32F, GL_RGB, L.discCol->data()); colVer_ = L.colVersion; }
    const auto pass = [&](int ox, int oy) {                                // one full-picture pass into the bound framebuffer, whose picture starts at (ox, oy)
        const double ppp = fbW / width_;                                   // pixels per point
        const double cxPx = std::round(L.cx * ppp), cyPx = std::round(L.cy * ppp), Kpx = L.kRad * ppp;
        const double e = L.alphaSh * Kpx, e2 = e * e;
        const float e2hi = static_cast<float>(e2), e2lo = static_cast<float>(e2 - static_cast<double>(e2hi));
        glUseProgram(lensProgram_);
        glUniformMatrix3fv(lnK_, 1, GL_TRUE, L.M);
        glUniform2f(lnCentre_, static_cast<float>(ox + cxPx), static_cast<float>(oy + fbH - cyPx));
        glUniform1f(lnKpx_, static_cast<float>(Kpx));
        glUniform3f(lnEdge_, static_cast<float>(e), e2hi, e2lo);
        glUniform2f(lnT_, static_cast<float>(std::log(Kpx) + L.s0), static_cast<float>(L.ds));
        glUniform1i(lnN_, lensN_);
        glUniform1i(lnBend_, bend ? 1 : 0);
        glUniform1i(lnDebug_, L.debugDisc ? 2 : L.debug ? 1 : 0);
        glUniform1i(lnSky_, L.sky ? 1 : 0);
        glUniform1i(lnDisc_, disc ? 1 : 0);
        if (disc) {
            glUniform3i(lnDN_, L.discS, L.discT, L.discC);
            glUniform4f(lnDS_, static_cast<float>(L.discS0), static_cast<float>(L.discDs0), static_cast<float>(L.discDs1), static_cast<float>(L.discXc));
            glUniform3f(lnDA_, L.discAxis[0], L.discAxis[1], L.discAxis[2]);
            glUniform4f(lnDR_, static_cast<float>(L.rO), static_cast<float>(L.rO / std::sqrt(1 - 2 / L.rO)), static_cast<float>(L.rIn), static_cast<float>(L.rOut));
            glUniform4f(lnDT_, static_cast<float>(L.tScale), static_cast<float>(1 / L.white), static_cast<float>(L.colLog0), static_cast<float>(L.colDlog));
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, discUTex_); glUniform1i(lnDU_, 2);
            glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, discPTex_); glUniform1i(lnDP_, 3);
            glActiveTexture(GL_TEXTURE4); glBindTexture(GL_TEXTURE_2D, discCTex_); glUniform1i(lnDC_, 4);
        }
        glUniform1f(lnBright_, static_cast<float>(std::clamp(L.bright, 0.0, 1.0)));
        const float tw = static_cast<float>(std::max(texW_[static_cast<int>(Map::Sky)], 1));
        glUniform1f(lnLod_, std::max(0.0f, std::log2(tw / 6.2831853f / std::max(static_cast<float>(L.kRad), 1.0f))));   // (skyMap's level: the plain sky as the Orbit Lab draws it)
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex_[static_cast<int>(Map::Sky)]);
        glUniform1i(lnTex_, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, lensTableTex_);
        glUniform1i(lnTable_, 1);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(cmpVao_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, 0); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, 0);
        if (disc) { for (GLenum u : {GL_TEXTURE2, GL_TEXTURE3, GL_TEXTURE4}) { glActiveTexture(u); glBindTexture(GL_TEXTURE_2D, 0); } glActiveTexture(GL_TEXTURE0); }
    };
    glDisable(GL_BLEND);                                                   // the bent sky is opaque: black space where there is no light
    if (L.debug || L.debugDisc) { pass(vx, vy); glEnable(GL_BLEND); return; }   // (tests: straight into the float framebuffer given, every time)
    if (!lensFbo_) { glGenFramebuffers(1, &lensFbo_); glGenTextures(1, &lensTex_); }
    if (fbW != lensW_ || fbH != lensH_) {
        lensW_ = fbW; lensH_ = fbH; lensValid_ = false;
        glBindTexture(GL_TEXTURE_2D, lensTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fbW, fbH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, lensFbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lensTex_, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    const LensSky& P = lensLast_;
    const bool same = lensValid_ && P.cx == L.cx && P.cy == L.cy && P.kRad == L.kRad && std::equal(L.M, L.M + 9, P.M) && P.bright == L.bright && P.bend == L.bend &&
                      P.alphaSh == L.alphaSh && P.s0 == L.s0 && P.ds == L.ds && P.tableVersion == L.tableVersion && width_ == lensPtW_ &&
                      P.sky == L.sky && P.disc == L.disc && (!L.disc || (P.discVersion == L.discVersion && P.colVersion == L.colVersion && std::equal(L.discAxis, L.discAxis + 3, P.discAxis) &&
                      P.rO == L.rO && P.rIn == L.rIn && P.rOut == L.rOut && P.tScale == L.tScale && P.white == L.white));
    if (!same) {                                                           // (e) of the plan: drawn anew only when the view, the distance or the picture changed
        glBindFramebuffer(GL_FRAMEBUFFER, lensFbo_);
        glViewport(0, 0, fbW, fbH);
        pass(0, 0);
        lensLast_ = L; lensLast_.table = nullptr; lensLast_.discU = lensLast_.discP = lensLast_.discCol = nullptr; lensPtW_ = width_; lensValid_ = true; lensRedraws_++;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, target);
    glViewport(vx, vy, fbW, fbH);
    glUseProgram(cmpProgram_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, lensTex_);
    glUniform1i(cmpTex_, 0);
    glBindVertexArray(cmpVao_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindTexture(GL_TEXTURE_2D, 0);
    glEnable(GL_BLEND);
}

void Renderer2D::closeTriangles() {
    if (verts_.size() > triStart_) {
        Segment sg{};
        sg.kind = Kind::Triangles; sg.first = static_cast<int>(triStart_); sg.count = static_cast<int>(verts_.size() - triStart_);
        segments_.push_back(sg);
    }
    triStart_ = verts_.size();
}

void Renderer2D::planet(double x, double y, double r, Color base, double lx, double ly, double lz, const float K[9], Map map, const float* rings, int nRings, double shapeY, double shapeZ, bool lunar,
                        const PlanetLight* light) {
    if (r <= 0) return;
    closeTriangles();
    const double half = r + 1.5;
    quad(x, y, half, base, 2, static_cast<float>(lx), static_cast<float>(ly), static_cast<float>(half / r), static_cast<float>(lz));
    Segment sg{};
    const bool has = map != Map::None && map != Map::Sky && hasMap(map);
    sg.kind = Kind::Planet; sg.first = static_cast<int>(triStart_); sg.count = 6; sg.map = has ? static_cast<int>(map) : 0; sg.mode = has ? 1 : 0; sg.r = static_cast<float>(r);
    for (int i = 0; i < 9; i++) sg.K[i] = K[i];
    sg.nring = rings ? std::min(nRings, kMaxRingBands) : 0;
    for (int i = 0; i < 3 * sg.nring; i++) sg.ring[i] = rings[i];
    sg.lunar = lunar;
    if (light) {
        sg.nocc = std::clamp(light->nOcc, 0, 4);
        for (int i = 0; i < 4 * sg.nocc; i++) sg.occ[i] = light->occ[i];
        sg.sunAng = light->sunAng; sg.air = light->air;
    }
    sg.shape[0] = static_cast<float>(shapeY); sg.shape[1] = static_cast<float>(shapeZ);   // (the quad of half side r + 1.5 holds it: no radius is over r)
    segments_.push_back(sg);
    triStart_ = verts_.size();
}

void Renderer2D::skyMap(double W, double H, double kRad, const float M[9], double bright) {
    if (!hasMap(Map::Sky) || bright <= 0 || kRad <= 0) return;
    closeTriangles();
    const float u0 = static_cast<float>(-W / 2 / kRad), u1 = static_cast<float>(W / 2 / kRad), v0 = static_cast<float>(-H / 2 / kRad), v1 = static_cast<float>(H / 2 / kRad);
    const Color c{1, 1, 1, static_cast<float>(std::min(1.0, bright))};
    push(0, 0, u0, v0, c, 2); push(W, 0, u1, v0, c, 2); push(W, H, u1, v1, c, 2);
    push(0, 0, u0, v0, c, 2); push(W, H, u1, v1, c, 2); push(0, H, u0, v1, c, 2);
    Segment sg{};
    sg.kind = Kind::Planet; sg.first = static_cast<int>(triStart_); sg.count = 6; sg.map = static_cast<int>(Map::Sky); sg.mode = 2; sg.r = static_cast<float>(kRad);
    for (int i = 0; i < 9; i++) sg.K[i] = M[i];
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

void Renderer2D::spheres(const Blob* b, int count) {
    if (count <= 0) return;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::Blobs; sg.first = static_cast<int>(blobs_.size()); sg.count = count; sg.spheres = true;
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

void Renderer2D::cloudGpu(const CloudGpuDraw& d) {
    if (!cloudGpuOk() || d.cnt <= 0 || !d.pts || !d.grp) return;
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::CloudGpu; sg.slot = static_cast<int>(clouds_.size()); sg.count = static_cast<int>(d.cnt); sg.additive = d.additive; sg.pal = d.pal;
    segments_.push_back(sg);
    clouds_.push_back(d);
}

void Renderer2D::beginSoftLayer(float scale) {
    closeTriangles();
    layerScale_ = scale;
    Segment sg{};
    sg.kind = Kind::LayerBegin;
    segments_.push_back(sg);
}

void Renderer2D::beginCloudLayer() {
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::LayerBegin; sg.hdr = true;
    segments_.push_back(sg);
}

void Renderer2D::endCloudLayer(bool additive) {
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::LayerEnd; sg.hdr = true; sg.additive = additive;
    segments_.push_back(sg);
}

void Renderer2D::endSoftLayer() {
    closeTriangles();
    Segment sg{};
    sg.kind = Kind::LayerEnd;
    segments_.push_back(sg);
}

// the fuzzy nucleon's profile: a proton's charge density falls off as exp(−r/r0), r0 = rms radius/√12 (a neutron has no net charge: for it this is only a size profile);
// seen from the side that is the column density x·K1(x), x = b/r0. Tabulated out to 1.9× the rms radius.
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

float Renderer2D::fuzzAt(float d) {
    if (!(d < 1.f)) return 0.f;
    const float* t = fuzzTable();
    const float x = std::max(0.f, d) * 16.f;
    const int i = std::min(15, int(x));
    return t[i] + (t[i + 1] - t[i]) * (x - float(i));
}

void Renderer2D::end(int fbW, int fbH, int fbY, GLuint outFbo) {
    if (outFbo) fbY = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, outFbo);
    glViewport(0, fbY, fbW, fbH);                         // the picture fills fbW × fbH pixels of the window, fbY above its bottom edge
    closeTriangles();
    lastCalls_ = 0; lastInstances_ = 0; lastVerts_ = static_cast<long>(verts_.size());
    for (const Segment& sg : segments_) {
        if (sg.kind != Kind::LayerBegin) lastCalls_++;
        if (sg.kind == Kind::Squares || sg.kind == Kind::Blobs || sg.kind == Kind::RocksGpu || sg.kind == Kind::CloudGpu) lastInstances_ += sg.count;
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
        if (sg.kind == Kind::Lens) { drawLens(lens_[std::size_t(sg.slot)], fbW, fbH, target, vx, vy); normalBlend(); continue; }   // 0.55.0
        if (sg.kind == Kind::LayerBegin && sg.hdr) {                          // 0.40: the cloud's float picture, the screen's size
            if (!hdrFbo_) { glGenFramebuffers(1, &hdrFbo_); glGenTextures(1, &hdrTex_); }
            if (fbW != hdrW_ || fbH != hdrH_) {
                hdrW_ = fbW; hdrH_ = fbH;
                glBindTexture(GL_TEXTURE_2D, hdrTex_);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, fbW, fbH, 0, GL_RGBA, GL_FLOAT, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo_);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdrTex_, 0);
                glBindTexture(GL_TEXTURE_2D, 0);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo_);
            glViewport(0, 0, fbW, fbH);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            continue;
        }
        if (sg.kind == Kind::LayerEnd && sg.hdr) {
            glBindFramebuffer(GL_FRAMEBUFFER, target);
            glViewport(vx, vy, fbW, fbH);
            if (sg.additive) glBlendFunc(GL_ONE, GL_ONE);                  // the summed light is added to the page
            else glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);               // premultiplied over (the light page's alpha blending, which is associative)
            glUseProgram(cmpProgram_);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, hdrTex_);
            glUniform1i(cmpTex_, 0);
            glBindVertexArray(cmpVao_);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindTexture(GL_TEXTURE_2D, 0);
            normalBlend();
            continue;
        }
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
            if (sg.spheres) {                                                     // 0.40: kind 9 tests each pixel against every sphere of its call
                float sph[4 * kMaxSpheres];
                const int n = std::min(sg.count, kMaxSpheres);
                for (int k = 0; k < n; k++) { const Blob& o = blobs_[std::size_t(sg.first + k)]; sph[4 * k] = o.x; sph[4 * k + 1] = o.y; sph[4 * k + 2] = o.fy; sph[4 * k + 3] = o.r; }
                glUniform4fv(blSph_, n, sph);
                glUniform1i(blSphN_, n);
            } else glUniform1i(blSphN_, 0);
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
        if (sg.kind == Kind::CloudGpu) {                                      // 0.46 (C4)
            const CloudGpuDraw& d = clouds_[std::size_t(sg.slot)];
            // 1. the places: the whole ring when it is another cloud (or it was changed other than by adding), else only the dots added since the last frame (the ring's slots k % cap)
            glBindBuffer(GL_ARRAY_BUFFER, clPosVbo_);
            if (d.epoch != clEpoch_ || d.cap != clCap_ || d.k < clK_ || d.k - clK_ >= d.cap) {
                glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(d.cap * 3 * sizeof(float)), d.pts, GL_DYNAMIC_DRAW);
                clBytes_ += static_cast<long>(d.cap * 3 * sizeof(float));
            } else if (d.k > clK_) {
                const long lo = clK_ % d.cap, hi = d.k % d.cap;
                const auto send = [&](long a, long b) { if (b > a) { glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(a * 3 * sizeof(float)), static_cast<GLsizeiptr>((b - a) * 3 * sizeof(float)), d.pts + 3 * a); clBytes_ += static_cast<long>((b - a) * 3 * sizeof(float)); } };
                if (lo < hi) send(lo, hi); else { send(lo, d.cap); send(0, hi); }
            }
            clEpoch_ = d.epoch; clCap_ = d.cap; clK_ = d.k;
            glBindBuffer(GL_ARRAY_BUFFER, clGrpVbo_);                         // 2. the colour groups (one byte a dot: they change with the beat or a jump)
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(d.cnt), d.grp, GL_STREAM_DRAW);
            // 3. the exposure's grid, summed on the GPU, read back
            const int gw = static_cast<int>(d.w / 8) + 2, gh = static_cast<int>(d.h / 8) + 2;
            if (!grFbo_) { glGenFramebuffers(1, &grFbo_); glGenTextures(1, &grTex_); }
            if (gw != grW_ || gh != grH_) {
                grW_ = gw; grH_ = gh;
                glBindTexture(GL_TEXTURE_2D, grTex_);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, gw, gh, 0, GL_RED, GL_FLOAT, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glBindFramebuffer(GL_FRAMEBUFFER, grFbo_);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, grTex_, 0);
                glBindTexture(GL_TEXTURE_2D, 0);
            }
            GLint prevFbo = 0, vp[4];
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo); glGetIntegerv(GL_VIEWPORT, vp);
            glBindFramebuffer(GL_FRAMEBUFFER, grFbo_);
            glViewport(0, 0, gw, gh);
            glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
            glBlendFunc(GL_ONE, GL_ONE);
            glUseProgram(grProgram_);
            glUniform4f(grCam0_, d.cx, d.cy, d.pxm, d.ext); glUniform4f(grCam1_, d.ca, d.sa, d.cb, d.sb); glUniform2f(grView_, d.w, d.h); glUniform2f(grGrid_, static_cast<float>(gw), static_cast<float>(gh));
            glBindVertexArray(grVao_);
            glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(d.cnt));
            grCells_.resize(static_cast<std::size_t>(gw) * gh);
            glPixelStorei(GL_PACK_ALIGNMENT, 4);
            glReadPixels(0, 0, gw, gh, GL_RED, GL_FLOAT, grCells_.data());
            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFbo)); glViewport(vp[0], vp[1], vp[2], vp[3]);
            normalBlend();
            const double light = d.exposure ? d.exposure(grCells_) : -1;
            if (!(light >= 0)) continue;
            // 4. the dots, instanced, in dot order (as the CPU list)
            if (sg.additive) glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
            glUseProgram(clProgram_);
            glUniform2f(clSize_, static_cast<float>(width_), static_cast<float>(height_));
            glUniform4f(clCam0_, d.cx, d.cy, d.pxm, d.ext); glUniform4f(clCam1_, d.ca, d.sa, d.cb, d.sb); glUniform2f(clView_, d.w, d.h);
            glUniform1f(clLight_, static_cast<float>(light));
            glUniform1i(clPalWheel_, sg.pal.wheel ? 1 : 0); glUniform1i(clPalLight_, sg.pal.light ? 1 : 0);
            glUniform4f(clPalPlus_, sg.pal.sign[0].r, sg.pal.sign[0].g, sg.pal.sign[0].b, sg.pal.sign[0].a);
            glUniform4f(clPalMinus_, sg.pal.sign[1].r, sg.pal.sign[1].g, sg.pal.sign[1].b, sg.pal.sign[1].a);
            glUniform1fv(clFuzz_, 17, fuzzTable()); glUniform1i(clSphN_, 0);
            glBindVertexArray(clVao_);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(d.cnt));
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
            { const float sp[3] = {space_.r, space_.g, space_.b}; glUniform3fv(plSpace_, 1, sp); }
            // uK * n: GLSL matrices are column-major, so the rows of K go in as columns (transpose = true)
            glUniformMatrix3fv(plK_, 1, GL_TRUE, sg.K);
            glUniform1i(plMode_, sg.mode);
            glUniform1i(plLunar_, sg.lunar ? 1 : 0);
            const float tw = static_cast<float>(std::max(texW_[sg.map], 1));
            // the mipmap level: texels per pixel. A planet: across its middle; the sky: along the galactic equator (sg.r = points per radian; a constant level keeps the wrap-round seam invisible)
            glUniform1f(plLod_, sg.mode == 2 ? std::max(0.0f, std::log2(tw / 6.2831853f / std::max(sg.r, 1.0f)))
                                             : std::max(0.0f, std::log2(tw / (3.14159f * std::max(2.0f * sg.r, 1.0f)))));
            glUniform1f(plLon0_, static_cast<float>(mapInfo(static_cast<Map>(sg.map)).lonCentreDeg * 3.14159265358979 / 180));
            glUniform1i(plRingN_, sg.mode == 2 ? 0 : sg.nring);
            if (sg.nring > 0 && sg.mode != 2) glUniform3fv(plRingB_, sg.nring, sg.ring);
            glUniform2f(plShape_, sg.mode == 2 || sg.shape[0] <= 0 ? 1.0f : sg.shape[0], sg.mode == 2 || sg.shape[1] <= 0 ? 1.0f : sg.shape[1]);
            glUniform1i(plOccN_, sg.mode == 2 ? 0 : sg.nocc);
            if (sg.nocc > 0 && sg.mode != 2) { glUniform4fv(plOcc_, sg.nocc, sg.occ); glUniform1f(plSunAng_, sg.sunAng); }
            glUniform1i(plAir_, sg.mode != 2 && sg.air ? 1 : 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex_[sg.map]);
            glUniform1i(plTex_, 0);
            glBindVertexArray(vao_);
            glDrawArrays(GL_TRIANGLES, sg.first, sg.count);
        } else if (sg.kind == Kind::Triangles) {
            glUseProgram(program_);
            glUniform2f(uSize_, static_cast<float>(width_), static_cast<float>(height_));
            { const float sp[3] = {space_.r, space_.g, space_.b}; glUniform3fv(uSpace_, 1, sp); }
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

void Renderer2D::push(double x, double y, float u, float v, Color c, float mode, float p1, float p2, float p3) {
    verts_.push_back({static_cast<float>(x), static_cast<float>(y), u, v, c.r, c.g, c.b, c.a, mode, p1, p2, p3});
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

void Renderer2D::quad(double cx, double cy, double half, Color c, float mode, float p1, float p2, float s, float p3) {
    push(cx - half, cy - half, -s, -s, c, mode, p1, p2, p3); push(cx + half, cy - half, s, -s, c, mode, p1, p2, p3); push(cx + half, cy + half, s, s, c, mode, p1, p2, p3);
    push(cx - half, cy - half, -s, -s, c, mode, p1, p2, p3); push(cx + half, cy + half, s, s, c, mode, p1, p2, p3); push(cx - half, cy + half, -s, s, c, mode, p1, p2, p3);
}

void Renderer2D::disc(double x, double y, double r, Color c) {
    if (r <= 0) return;
    const double half = r + 1.5;                          // a little room for the smooth edge
    quad(x, y, half, c, 1, 0, 0, static_cast<float>(half / r));
}

void Renderer2D::sphere(double x, double y, double r, Color c, double lx, double ly, double lz) {
    if (r <= 0) return;
    const double half = r + 1.5;
    quad(x, y, half, c, 2, static_cast<float>(lx), static_cast<float>(ly), static_cast<float>(half / r), static_cast<float>(lz));
}

void Renderer2D::sunDisc(double x, double y, double r, Color c) {
    if (r <= 0) return;
    const double half = r + 1.5;
    quad(x, y, half, c, 3, 0, 0, static_cast<float>(half / r));
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
