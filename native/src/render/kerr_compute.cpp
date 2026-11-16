#include "kerr_compute.h"

#include <chrono>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <string>

#include "gl_loader.h"
#include "shaders.h"

using namespace plgl;

namespace render {

std::string kerrShaderId() {
    unsigned long long h = 1469598103934665603ULL;                  // FNV-1a, 64 bit
    for (const char* p = kKerrTableCS; *p; p++) { h ^= static_cast<unsigned char>(*p); h *= 1099511628211ULL; }
    char b[40];
    std::snprintf(b, sizeof b, "shader %016llx", h);
    return b;
}

namespace {
double nowMs() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
class KerrCompute : public IKerrCompute {
public:
    ~KerrCompute() override;
    bool init(const char** why, KerrSteps* steps);
    void passA(const std::vector<double>& par, const std::vector<double>& chi) override;
    void passB(const std::vector<double>& par, const std::vector<double>& rowChi, int nS) override;
    bool passBRows(const std::vector<double>& par, const std::vector<double>& rowChi, int nS) override;
    void passBPiece(int first, int n) override;
    void passBCopy() override;
    bool done(bool wait) override;
    double queuedMs() const override { return fence_ ? nowMs() - fenceAt_ : 0; }
    void queueRays(const std::vector<double>& par, const std::vector<double>& psiChi) override;
    void readRays(std::vector<double>& out) override;
    void queueEdges(const std::vector<double>& par, const std::vector<double>& chi) override;
    void readEdges(std::vector<double>& out) override;
    void queueMath(const std::vector<double>& par, const std::vector<double>& x) override;
    void readMath(std::vector<double>& out) override;
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
    double fenceAt_ = 0;
    int nA_ = 0, nChi_ = 0, nS_ = 0;
    bool mapped_ = false, hang_ = false;
    void put(int b, const std::vector<double>& v);
    void room(int b, GLsizeiptr bytes);
    void bind(int par, int in, int edge, int out);
    void run(int mode, int n, int first);
    void fence();
    bool rowsOnly(const std::vector<double>& par, const std::vector<double>& rowChi, int nS);
    void piece(int first, int n);
    void copyOut();
};

KerrCompute::~KerrCompute() {
    if (mapped_) release();
    if (fence_) glDeleteSync(fence_);
    glDeleteBuffers(kBufs, buf_);
    if (prog_) glDeleteProgram(prog_);
}

bool KerrCompute::init(const char** why, KerrSteps* steps) {
    hang_ = std::getenv("PL_BH_CARDHANG") != nullptr;
    GLint maj = 0, min = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &maj); glGetIntegerv(GL_MINOR_VERSION, &min);
    if (maj < 4 || (maj == 4 && min < 3)) { *why = "the graphics driver has no OpenGL 4.3"; return false; }
    const char* missing = nullptr;
    if (!loadGL43(&missing)) { *why = "an OpenGL 4.3 function is missing"; return false; }
    // (0.55.7: the driver's info logs are told to steps also when it compiled and linked: a warning there can explain a later failure)
    const auto infoText = [](const char* log, bool good) {
        std::string t = good ? "compiled" : "FAILED";
        if (log[0]) { t += ", the driver's log: "; for (const char* c = log; *c; c++) t += (*c == '\n' || *c == '\r') ? ' ' : *c; }
        else t += ", the driver's log is empty";
        return t;
    };
    if (steps) steps->begin("compile the light paths' shader");
    const GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(cs, 1, &kKerrTableCS, nullptr);
    glCompileShader(cs);
    GLint good = 0;
    glGetShaderiv(cs, GL_COMPILE_STATUS, &good);
    {   char log[2048] = {0}; glGetShaderInfoLog(cs, sizeof log, nullptr, log);
        if (!good) std::fprintf(stderr, "light paths compute shader compile error: %s\n", log);
        if (steps) steps->end("compile the light paths' shader", infoText(log, good != 0)); }
    if (!good) {
        glDeleteShader(cs);
        *why = "the graphics driver did not compile the light paths' shader";
        return false;
    }
    if (steps) steps->begin("link the light paths' shader");
    prog_ = glCreateProgram();
    glAttachShader(prog_, cs);
    glLinkProgram(prog_);
    glDeleteShader(cs);
    glGetProgramiv(prog_, GL_LINK_STATUS, &good);
    {   char log[2048] = {0}; glGetProgramInfoLog(prog_, sizeof log, nullptr, log);
        if (!good) std::fprintf(stderr, "light paths compute shader link error: %s\n", log);
        if (steps) steps->end("link the light paths' shader", infoText(log, good != 0)); }
    if (!good) {
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
    fenceAt_ = nowMs();
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

// (0.55.7: refused while the last table is mapped: kHost's storage must not be replaced under the worker that reads it)
bool KerrCompute::rowsOnly(const std::vector<double>& par, const std::vector<double>& rowChi, int nS) {
    if (mapped_) return false;
    nChi_ = static_cast<int>(rowChi.size()); nS_ = nS;
    const int n = nodes_ = 2 * nChi_ * nS_;
    put(kPar, par);
    put(kInB, rowChi);
    room(kEdgeB, static_cast<GLsizeiptr>(nChi_) * 8);
    room(kOutB, static_cast<GLsizeiptr>(n) * 80);
    room(kHost, static_cast<GLsizeiptr>(n) * 80);
    bind(kPar, kInB, kEdgeB, kOutB);
    run(0, nChi_, 0);
    return true;
}

void KerrCompute::piece(int first, int n) {
    bind(kPar, kInB, kEdgeB, kOutB);
    run(3, n, first);
}

void KerrCompute::copyOut() {
    glBindBuffer(GL_COPY_READ_BUFFER, buf_[kOutB]);
    glBindBuffer(GL_COPY_WRITE_BUFFER, buf_[kHost]);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, static_cast<GLsizeiptr>(nodes_) * 80);
}

void KerrCompute::passB(const std::vector<double>& par, const std::vector<double>& rowChi, int nS) {
    if (!rowsOnly(par, rowChi, nS)) return;
    const int n = nodes_;
    for (int first = 0; first < n; first += kPiece) run(3, n - first < kPiece ? n - first : kPiece, first);
    copyOut();
    fence();
}

bool KerrCompute::passBRows(const std::vector<double>& par, const std::vector<double>& rowChi, int nS) {
    if (!rowsOnly(par, rowChi, nS)) return false;
    fence();
    return true;
}
void KerrCompute::passBPiece(int first, int n) { piece(first, n); fence(); }
void KerrCompute::passBCopy() { copyOut(); fence(); }

bool KerrCompute::done(bool wait) {
    if (!fence_) return true;
    const double t0 = nowMs();
    for (;;) {
        // (PL_BH_CARDHANG, tests: as if the fence never signalled; the work itself is flushed as usual)
        const GLenum r = glClientWaitSync(fence_, GL_SYNC_FLUSH_COMMANDS_BIT, wait ? 100000000ull : 0ull);
        if (hang_) { if (!wait || nowMs() - t0 >= kWaitLimitMs) return false; continue; }
        if (r == GL_ALREADY_SIGNALED || r == GL_CONDITION_SATISFIED) break;
        if (r == GL_WAIT_FAILED) return false;                     // (0.55.7: an error is not "finished": the results are not read)
        if (!wait || nowMs() - t0 >= kWaitLimitMs) return false;
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

void KerrCompute::queueRays(const std::vector<double>& par, const std::vector<double>& psiChi) {
    const int n = nA_ = static_cast<int>(psiChi.size() / 2);
    put(kPar, par);
    put(kInA, psiChi);
    room(kEdgeA, 8);
    room(kOutA, static_cast<GLsizeiptr>(n) * 128);
    bind(kPar, kInA, kEdgeA, kOutA);
    run(1, n, 0);
    fence();
}
void KerrCompute::readRays(std::vector<double>& out) {
    out.assign(static_cast<std::size_t>(nA_) * 16, 0.0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf_[kOutA]);
    if (nA_ > 0) glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(nA_) * 128, out.data());
}
// (0.55.7: a wait that ran out of time gives NaN, never the last run's numbers: the check then fails)
void KerrCompute::rays(const std::vector<double>& par, const std::vector<double>& psiChi, std::vector<double>& out) {
    queueRays(par, psiChi);
    if (done(true)) readRays(out); else out.assign(psiChi.size() / 2 * 16, std::nan(""));
}

void KerrCompute::queueEdges(const std::vector<double>& par, const std::vector<double>& chi) {
    const int n = nA_ = static_cast<int>(chi.size());
    put(kPar, par);
    put(kInA, chi);
    room(kEdgeA, static_cast<GLsizeiptr>(n) * 8);
    room(kOutA, 8);
    bind(kPar, kInA, kEdgeA, kOutA);
    run(0, n, 0);
    fence();
}
void KerrCompute::readEdges(std::vector<double>& out) {
    out.assign(static_cast<std::size_t>(nA_), 0.0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf_[kEdgeA]);
    if (nA_ > 0) glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(nA_) * 8, out.data());
}
void KerrCompute::edges(const std::vector<double>& par, const std::vector<double>& chi, std::vector<double>& out) {
    queueEdges(par, chi);
    if (done(true)) readEdges(out); else out.assign(chi.size(), std::nan(""));
}

void KerrCompute::queueMath(const std::vector<double>& par, const std::vector<double>& x) {
    const int n = nA_ = static_cast<int>(x.size());
    put(kPar, par);
    put(kInA, x);
    room(kEdgeA, 8);
    room(kOutA, static_cast<GLsizeiptr>(n) * 64);
    bind(kPar, kInA, kEdgeA, kOutA);
    run(4, n, 0);
    fence();
}
void KerrCompute::readMath(std::vector<double>& out) {
    out.assign(static_cast<std::size_t>(nA_) * 8, 0.0);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, buf_[kOutA]);
    if (nA_ > 0) glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(nA_) * 64, out.data());
}
void KerrCompute::math(const std::vector<double>& par, const std::vector<double>& x, std::vector<double>& out) {
    queueMath(par, x);
    if (done(true)) readMath(out); else out.assign(x.size() * 8, std::nan(""));
}
}  // namespace

IKerrCompute* makeKerrCompute(const char** why, KerrSteps* steps) {
    static const char* none = "";
    if (!why) why = &none;
    KerrCompute* k = new KerrCompute();
    if (!k->init(why, steps)) { delete k; return nullptr; }
    return k;
}

}  // namespace render
