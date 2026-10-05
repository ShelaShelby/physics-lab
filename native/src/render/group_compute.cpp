#include "group_compute.h"

#include <algorithm>
#include <cstdio>

#include "gl_loader.h"
#include "shaders.h"

using namespace plgl;

namespace render {

// the OpenGL 4.3 implementation of IGroupCompute (needs a current 4.3 context; the loader's 4.3 functions)
class GroupCompute : public IGroupCompute {
public:
    ~GroupCompute() override { shutdown(); }
    bool init();
    void shutdown();
    bool ok() const override { return ok_; }
    bool superGroups(const float* amps, long count, std::uint64_t epoch, long dirtyLo, long dirtyHi, const SuperGroupParams& p, unsigned char* out) override;
    bool jumpGroups(const float* ampsA, const float* ampsB, long jn, std::uint64_t epoch, const JumpGroupParams& p, unsigned char* out) override;
private:
    bool run(long count, int mode, int jn, float p0[4], float p1[4], int flags, unsigned char* out);
    void upload(GLuint buf, const float* data, long dots, bool full, long lo, long hi);
    bool ok_ = false;
    GLuint program_ = 0, bufSuper_ = 0, bufJumpA_ = 0, bufJumpB_ = 0, bufOut_ = 0;
    GLint uMode_ = -1, uCount_ = -1, uJn_ = -1, uP0_ = -1, uP1_ = -1, uFlags_ = -1;
    std::uint64_t superEpoch_ = 0, jumpEpoch_ = 0;
    long superCount_ = -1, jumpCount_ = -1, outBytes_ = 0;
};

bool GroupCompute::init() {
    GLint maj = 0, min = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &maj); glGetIntegerv(GL_MINOR_VERSION, &min);
    if (maj < 4 || (maj == 4 && min < 3)) return false;                     // a 3.3 context: no compute shaders, the CPU path
    const char* missing = nullptr;
    if (!loadGL43(&missing)) { std::fprintf(stderr, "OpenGL 4.3 function missing: %s\n", missing ? missing : "?"); return false; }
    const GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(cs, 1, &kGroupsCS, nullptr);
    glCompileShader(cs);
    GLint good = 0;
    glGetShaderiv(cs, GL_COMPILE_STATUS, &good);
    if (!good) { char log[2048]; glGetShaderInfoLog(cs, sizeof log, nullptr, log); std::fprintf(stderr, "compute shader compile error: %s\n", log); glDeleteShader(cs); return false; }
    program_ = glCreateProgram();
    glAttachShader(program_, cs);
    glLinkProgram(program_);
    glDeleteShader(cs);
    glGetProgramiv(program_, GL_LINK_STATUS, &good);
    if (!good) { char log[2048]; glGetProgramInfoLog(program_, sizeof log, nullptr, log); std::fprintf(stderr, "compute shader link error: %s\n", log); glDeleteProgram(program_); program_ = 0; return false; }
    uMode_ = glGetUniformLocation(program_, "uMode"); uCount_ = glGetUniformLocation(program_, "uCount"); uJn_ = glGetUniformLocation(program_, "uJn");
    uP0_ = glGetUniformLocation(program_, "uP0"); uP1_ = glGetUniformLocation(program_, "uP1"); uFlags_ = glGetUniformLocation(program_, "uFlags");
    GLuint b[4];
    glGenBuffers(4, b);
    bufSuper_ = b[0]; bufJumpA_ = b[1]; bufJumpB_ = b[2]; bufOut_ = b[3];
    ok_ = true;
    return true;
}

void GroupCompute::shutdown() {
    if (!ok_) return;
    const GLuint b[4] = {bufSuper_, bufJumpA_, bufJumpB_, bufOut_};
    glDeleteBuffers(4, b);
    glDeleteProgram(program_);
    program_ = 0; ok_ = false;
}

// Puts the dots on the GPU: all of them when the array is new (full), else only [lo, hi).
void GroupCompute::upload(GLuint buf, const float* data, long dots, bool full, long lo, long hi) {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf);
    if (full) { glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(dots) * 16, data, GL_DYNAMIC_DRAW); return; }
    lo = std::max(0L, lo); hi = std::min(dots, hi);
    if (hi > lo) glBufferSubData(GL_SHADER_STORAGE_BUFFER, static_cast<GLintptr>(lo) * 16, static_cast<GLsizeiptr>(hi - lo) * 16, data + 4 * lo);
}

bool GroupCompute::run(long count, int mode, int jn, float p0[4], float p1[4], int flags, unsigned char* out) {
    const long words = (count + 3) / 4;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufOut_);
    if (outBytes_ < words * 4) { glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(words) * 4, nullptr, GL_DYNAMIC_READ); outBytes_ = words * 4; }
    glUseProgram(program_);
    glUniform1i(uMode_, mode); glUniform1i(uCount_, static_cast<GLint>(count)); glUniform1i(uJn_, jn);
    glUniform4f(uP0_, p0[0], p0[1], p0[2], p0[3]); glUniform4f(uP1_, p1[0], p1[1], p1[2], p1[3]); glUniform1i(uFlags_, flags);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mode == 0 ? bufSuper_ : bufJumpA_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, bufJumpB_);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, bufOut_);
    glDispatchCompute(static_cast<GLuint>((words + 63) / 64), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, bufOut_);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(count), out);         // waits for the kernel: the CPU needs the groups to order the dots
    glUseProgram(0);
    return true;
}

bool GroupCompute::superGroups(const float* amps, long count, std::uint64_t epoch, long dirtyLo, long dirtyHi, const SuperGroupParams& p, unsigned char* out) {
    if (!ok_ || count <= 0) return false;
    const bool full = epoch != superEpoch_ || count != superCount_;
    upload(bufSuper_, amps, count, full, dirtyLo, dirtyHi);
    superEpoch_ = epoch; superCount_ = count;
    float p0[4] = {p.a, p.b, p.emRe, p.emIm}, p1[4] = {0, 0, 0, 0};
    return run(count, 0, 0, p0, p1, p.overlap ? 1 : 0, out);
}

bool GroupCompute::jumpGroups(const float* ampsA, const float* ampsB, long jn, std::uint64_t epoch, const JumpGroupParams& p, unsigned char* out) {
    if (!ok_ || jn <= 0) return false;
    if (epoch != jumpEpoch_ || jn != jumpCount_) {
        upload(bufJumpA_, ampsA, jn, true, 0, 0); upload(bufJumpB_, ampsB, jn, true, 0, 0);
        jumpEpoch_ = epoch; jumpCount_ = jn;
    }
    float p0[4] = {p.cth, p.sth, p.lam, 0}, p1[4] = {p.emRe, p.emIm, p.epRe, p.epIm};
    return run(2 * jn, 1, static_cast<int>(jn), p0, p1, p.wheel ? 2 : 0, out);
}

IGroupCompute* makeGroupCompute() {
    GroupCompute* g = new GroupCompute();
    if (!g->init()) { delete g; return nullptr; }
    return g;
}

}  // namespace render
