#include "kerr_sky.h"

#include <chrono>
#include <cstdio>

#include "shaders.h"

using namespace plgl;

namespace render {

namespace {
double nowMs() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
std::string oneLine(const char* log) {
    std::string t;
    for (const char* c = log; *c; c++) t += (*c == '\n' || *c == '\r') ? ' ' : *c;
    return t;
}
}  // namespace

std::string kerrSkySource(int stage) {
    if (stage == 0) return kKerrSkyTinyFS;
    if (stage == 1) return std::string(kKerrSkyHeadPart) + kEllipticPart + kKerrSkyMathPart;
    return std::string(kKerrSkyHeadPart) + kEllipticPart + kKerrSkyTracePart + kKerrSkyMainPart;
}

std::string kerrSkyShaderId() {
    unsigned long long h = 1469598103934665603ULL;                  // FNV-1a, 64 bit
    for (int s = 0; s < KerrSky::kStages; s++)
        for (char c : kerrSkySource(s)) { h ^= static_cast<unsigned char>(c); h *= 1099511628211ULL; }
    char b[40];
    std::snprintf(b, sizeof b, "sky %016llx", h);
    return b;
}

const char* KerrSky::stageName(int stage) {
    static const char* const n[kStages] = {"the tiny shader", "the elliptic functions alone", "the whole sky shader"};
    return stage >= 0 && stage < kStages ? n[stage] : "?";
}

void KerrSky::reset() {
    for (GLuint& p : prog_) if (p) { glDeleteProgram(p); p = 0; }
    for (double& m : ms_) m = 0;
}

bool KerrSky::build(int stage, KerrSteps* steps, std::string& why) {
    if (stage < 0 || stage >= kStages) return false;
    if (prog_[stage]) return true;
    const double t0 = nowMs();
    const std::string src = kerrSkySource(stage), name = stageName(stage);
    const std::string cName = "compile " + name, lName = "link " + name;
    GLint good = 0;
    char log[4096] = {0};
    if (steps) steps->begin(cName);
    double t = nowMs();
    const GLuint vs = glCreateShader(GL_VERTEX_SHADER), fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vs, 1, &kCompositeVS, nullptr);
    glCompileShader(vs);
    const char* p = src.c_str();
    glShaderSource(fs, 1, &p, nullptr);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &good);
    glGetShaderInfoLog(fs, sizeof log, nullptr, log);
    if (steps) steps->end(cName, std::string(good ? "compiled" : "FAILED") + " in " + std::to_string(static_cast<int>(nowMs() - t)) + " ms" +
                                 (log[0] ? ", the driver's log: " + oneLine(log) : std::string(", the driver's log is empty")));
    if (!good) {
        std::fprintf(stderr, "sky shader (%s) compile error: %s\n", name.c_str(), log);
        glDeleteShader(vs); glDeleteShader(fs);
        why = "the graphics driver did not compile " + name;
        return false;
    }
    if (steps) steps->begin(lName);
    t = nowMs();
    const GLuint pr = glCreateProgram();
    glAttachShader(pr, vs); glAttachShader(pr, fs);
    glLinkProgram(pr);
    glDeleteShader(vs); glDeleteShader(fs);
    glGetProgramiv(pr, GL_LINK_STATUS, &good);
    log[0] = 0;
    glGetProgramInfoLog(pr, sizeof log, nullptr, log);
    if (steps) steps->end(lName, std::string(good ? "linked" : "FAILED") + " in " + std::to_string(static_cast<int>(nowMs() - t)) + " ms" +
                                 (log[0] ? ", the driver's log: " + oneLine(log) : std::string(", the driver's log is empty")));
    if (!good) {
        std::fprintf(stderr, "sky shader (%s) link error: %s\n", name.c_str(), log);
        glDeleteProgram(pr);
        why = "the graphics driver did not link " + name;
        return false;
    }
    prog_[stage] = pr;
    ms_[stage] = nowMs() - t0;
    return true;
}

void KerrSky::math(const std::vector<float>& in, std::vector<float>& out, GLuint vao) {
    const int n = static_cast<int>(in.size() / 12);
    out.assign(static_cast<std::size_t>(n) * 8, 0.0f);
    if (!prog_[1] || n == 0) return;
    GLuint tex = 0, otex = 0, fbo = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 3 * n, 1, 0, GL_RGBA, GL_FLOAT, in.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenTextures(1, &otex);
    glBindTexture(GL_TEXTURE_2D, otex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, n, 2, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, otex, 0);
    glViewport(0, 0, n, 2);
    glDisable(GL_BLEND);
    glUseProgram(prog_[1]);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(prog_[1], "uArgs"), 0);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    std::vector<float> px(static_cast<std::size_t>(n) * 8);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, n, 2, GL_RGBA, GL_FLOAT, px.data());
    for (int i = 0; i < n; i++)
        for (int k = 0; k < 4; k++) {
            out[static_cast<std::size_t>(8 * i + k)] = px[static_cast<std::size_t>(4 * i + k)];
            out[static_cast<std::size_t>(8 * i + 4 + k)] = px[static_cast<std::size_t>(4 * (n + i) + k)];
        }
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    glDeleteTextures(1, &otex);
    glEnable(GL_BLEND);
}

}  // namespace render
