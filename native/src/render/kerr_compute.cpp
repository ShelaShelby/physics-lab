#include "kerr_compute.h"

#include <cstdio>

#include "gl_loader.h"
#include "shaders.h"

using namespace plgl;

namespace render {

namespace {
class KerrCompute : public IKerrCompute {
public:
    ~KerrCompute() override;
    bool init(const char** why);
    void passA(const std::vector<double>& par, const std::vector<double>& chi) override;
    void passB(const std::vector<double>& par, const std::vector<double>& rowChi, int nS) override;
    bool done(bool wait) override;
    void readA(std::vector<double>& edge, std::vector<double>& probe) override;
    const double* mapB(std::vector<double>& rowEdge) override;
    void release() override;
    bool mapped() const override { return mapped_; }
    void rays(const std::vector<double>& par, const std::vector<double>& psiChi, std::vector<double>& out) override;
    void edges(const std::vector<double>& par, const std::vector<double>& chi, std::vector<double>& out) override;
    void math(const std::vector<double>& par, const std::vector<double>& x, std::vector<double>& out) override;
private:
    enum { kPar, kInA, kEdgeA, kOutA, kInB, kEdgeB, kOutB, kHost, kBufs };
    GLuint prog_ = 0, buf_[kBufs] = {};
    GLsizeiptr size_[kBufs] = {};
    GLint uMode_ = -1, uN_ = -1, uFirst_ = -1, uNChi_ = -1, uNS_ = -1;
    GLsync fence_ = nullptr;
    int nA_ = 0, nChi_ = 0, nS_ = 0;
    bool mapped_ = false;
    void put(int b, const std::vector<double>& v);
    void room(int b, GLsizeiptr bytes);
    void bind(int par, int in, int edge, int out);
    void run(int mode, int n, int first);
    void fence();
};

KerrCompute::~KerrCompute() {
    if (mapped_) release();
    if (fence_) glDeleteSync(fence_);
    glDeleteBuffers(kBufs, buf_);
    if (prog_) glDeleteProgram(prog_);
}

bool KerrCompute::init(const char** why) {
    GLint maj = 0, min = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &maj); glGetIntegerv(GL_MINOR_VERSION, &min);
    if (maj < 4 || (maj == 4 && min < 3)) { *why = "the graphics driver has no OpenGL 4.3"; return false; }
    const char* missing = nullptr;
    if (!loadGL43(&missing)) { *why = "an OpenGL 4.3 function is missing"; return false; }
    const GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(cs, 1, &kKerrTableCS, nullptr);
    glCompileShader(cs);
    GLint good = 0;
    glGetShaderiv(cs, GL_COMPILE_STATUS, &good);
    if (!good) {
        char log[2048]; glGetShaderInfoLog(cs, sizeof log, nullptr, log);
        std::fprintf(stderr, "light paths compute shader compile error: %s\n", log);
        glDeleteShader(cs);
        *why = "the graphics driver did not compile the light paths' shader";
        return false;
    }
    prog_ = glCreateProgram();
    glAttachShader(prog_, cs);
    glLinkProgram(prog_);
    glDeleteShader(cs);
    glGetProgramiv(prog_, GL_LINK_STATUS, &good);
    if (!good) {
        char log[2048]; glGetProgramInfoLog(prog_, sizeof log, nullptr, log);
        std::fprintf(stderr, "light paths compute shader link error: %s\n", log);
        *why = "the graphics driver did not link the light paths' shader";
        return false;
    }
    uMode_ = glGetUniformLocation(prog_, "uMode"); uN_ = glGetUniformLocation(prog_, "uN"); uFirst_ = glGetUniformLocation(prog_, "uFirst");
    uNChi_ = glGetUniformLocation(prog_, "uNChi"); uNS_ = glGetUniformLocation(prog_, "uNS");
    glGenBuffers(kBufs, buf_);
    return true;
}

void KerrCompute::room(int b, GLsizeiptr bytes) {
    if (bytes < 8) bytes = 8;
    if (size_[b] >= bytes) return;
    glBindBuffer(b == kHost ? GL_COPY_WRITE_BUFFER : GL_SHADER_STORAGE_BUFFER, buf_[b]);
    glBufferData(b == kHost ? GL_COPY_WRITE_BUFFER : GL_SHADER_STORAGE_BUFFER, bytes, nullptr, b == kHost ? GL_STREAM_READ : GL_DYNAMIC_COPY);
    size_[b] = bytes;
}

void KerrCompute::put(int b, const std::vector<double>& v) {
    const GLsizeiptr bytes = static_cast<GLsizeiptr>(v.size() * sizeof(double));
    room(b, bytes);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf_[b]);
    if (bytes > 0) glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, bytes, v.data());
}

void KerrCompute::bind(int par, int in, int edge, int out) {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buf_[par]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, buf_[in]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, buf_[edge]);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, buf_[out]);
}

void KerrCompute::run(int mode, int n, int first) {
    glUseProgram(prog_);
    glUniform1i(uMode_, mode); glUniform1i(uN_, n); glUniform1i(uFirst_, first); glUniform1i(uNChi_, nChi_); glUniform1i(uNS_, nS_);
    if (n > 0) glDispatchCompute(static_cast<GLuint>((n + 63) / 64), 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    glUseProgram(0);
}

void KerrCompute::fence() {
    if (fence_) glDeleteSync(fence_);
    fence_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

void KerrCompute::passA(const std::vector<double>& par, const std::vector<double>& chi) {
    nA_ = static_cast<int>(chi.size());
    put(kPar, par);
    put(kInA, chi);
    room(kEdgeA, static_cast<GLsizeiptr>(nA_) * 8);
    room(kOutA, static_cast<GLsizeiptr>(nA_) * 80);
    bind(kPar, kInA, kEdgeA, kOutA);
    run(0, nA_, 0);
    run(2, nA_, 0);
    fence();
}

void KerrCompute::passB(const std::vector<double>& par, const std::vector<double>& rowChi, int nS) {
    nChi_ = static_cast<int>(rowChi.size()); nS_ = nS;
    const int n = 2 * nChi_ * nS_;
    put(kPar, par);
    put(kInB, rowChi);
    room(kEdgeB, static_cast<GLsizeiptr>(nChi_) * 8);
    room(kOutB, static_cast<GLsizeiptr>(n) * 80);
    room(kHost, static_cast<GLsizeiptr>(n) * 80);
    bind(kPar, kInB, kEdgeB, kOutB);
    run(0, nChi_, 0);
    for (int first = 0; first < n; first += kPiece) run(3, n - first < kPiece ? n - first : kPiece, first);
    glBindBuffer(GL_COPY_READ_BUFFER, buf_[kOutB]);
    glBindBuffer(GL_COPY_WRITE_BUFFER, buf_[kHost]);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, static_cast<GLsizeiptr>(n) * 80);
    fence();
}

bool KerrCompute::done(bool wait) {
    if (!fence_) return true;
    for (;;) {
        const GLenum r = glClientWaitSync(fence_, GL_SYNC_FLUSH_COMMANDS_BIT, wait ? 1000000000ull : 0ull);
        if (r == GL_ALREADY_SIGNALED || r == GL_CONDITION_SATISFIED || r == GL_WAIT_FAILED) break;
        if (!wait) return false;
    }
    glDeleteSync(fence_);
    fence_ = nullptr;
    return true;
}

void KerrCompute::readA(std::vector<double>& edge, std::vector<double>& probe) {
    edge.resize(static_cast<std::size_t>(nA_)); probe.resize(static_cast<std::size_t>(nA_) * 10);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf_[kEdgeA]);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(nA_) * 8, edge.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf_[kOutA]);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(nA_) * 80, probe.data());
}

const double* KerrCompute::mapB(std::vector<double>& rowEdge) {
    rowEdge.resize(static_cast<std::size_t>(nChi_));
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf_[kEdgeB]);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(nChi_) * 8, rowEdge.data());
    glBindBuffer(GL_COPY_READ_BUFFER, buf_[kHost]);
    const void* p = glMapBufferRange(GL_COPY_READ_BUFFER, 0, static_cast<GLsizeiptr>(2) * nChi_ * nS_ * 80, GL_MAP_READ_BIT);
    mapped_ = p != nullptr;
    return static_cast<const double*>(p);
}

void KerrCompute::release() {
    if (!mapped_) return;
    glBindBuffer(GL_COPY_READ_BUFFER, buf_[kHost]);
    glUnmapBuffer(GL_COPY_READ_BUFFER);
    mapped_ = false;
}

void KerrCompute::rays(const std::vector<double>& par, const std::vector<double>& psiChi, std::vector<double>& out) {
    const int n = static_cast<int>(psiChi.size() / 2);
    put(kPar, par);
    put(kInA, psiChi);
    room(kEdgeA, 8);
    room(kOutA, static_cast<GLsizeiptr>(n) * 128);
    bind(kPar, kInA, kEdgeA, kOutA);
    run(1, n, 0);
    fence();
    done(true);
    out.resize(static_cast<std::size_t>(n) * 16);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf_[kOutA]);
    if (n > 0) glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(n) * 128, out.data());
}

void KerrCompute::edges(const std::vector<double>& par, const std::vector<double>& chi, std::vector<double>& out) {
    const int n = static_cast<int>(chi.size());
    put(kPar, par);
    put(kInA, chi);
    room(kEdgeA, static_cast<GLsizeiptr>(n) * 8);
    room(kOutA, 8);
    bind(kPar, kInA, kEdgeA, kOutA);
    run(0, n, 0);
    fence();
    done(true);
    out.resize(static_cast<std::size_t>(n));
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf_[kEdgeA]);
    if (n > 0) glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(n) * 8, out.data());
}

void KerrCompute::math(const std::vector<double>& par, const std::vector<double>& x, std::vector<double>& out) {
    const int n = static_cast<int>(x.size());
    put(kPar, par);
    put(kInA, x);
    room(kEdgeA, 8);
    room(kOutA, static_cast<GLsizeiptr>(n) * 64);
    bind(kPar, kInA, kEdgeA, kOutA);
    run(4, n, 0);
    fence();
    done(true);
    out.resize(static_cast<std::size_t>(n) * 8);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf_[kOutA]);
    if (n > 0) glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(n) * 64, out.data());
}
}  // namespace

IKerrCompute* makeKerrCompute(const char** why) {
    static const char* none = "";
    if (!why) why = &none;
    KerrCompute* k = new KerrCompute();
    if (!k->init(why)) { delete k; return nullptr; }
    return k;
}

}  // namespace render
