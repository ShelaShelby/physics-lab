#include "outline_compute.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "gl_loader.h"
#include "shaders.h"

using namespace plgl;

namespace render {

// The OpenGL 4.3 implementation of IOutlineCompute. Each job is one batch of GPU work and a fence; the CPU never waits for it (pollGrid / pollPlane ask the fence with a zero timeout, and read the
// results back only once it has signalled, so the read is a copy and not a wait).
class OutlineCompute : public IOutlineCompute {
public:
    ~OutlineCompute() override { shutdown(); }
    bool init();
    void shutdown();
    bool ok() const override { return ok_; }
    int beginGrid(const OutlineDensity& d, int N, float half, float scale, float target) override;
    int beginPlane(const OutlineDensity& d, int n, float half, bool horizontal, float offset) override;
    JobState pollGrid(int job, std::vector<float>& grid, float& level) override;
    JobState pollPlane(int job, std::vector<float>& values) override;
    void cancel(int job) override;
private:
    struct Job { int id = 0; bool grid = false; GLsync sync = nullptr; GLuint vals = 0, level = 0; long count = 0; };
    static constexpr std::size_t kMaxJobs = 8;                         // more than this in flight (it never happens in the app): the CPU path does the rest
    Job* find(int id) { for (Job& j : jobs_) if (j.id == id) return &j; return nullptr; }
    void release(int id);
    bool compile(const char* src, GLuint& program);
    void setDensity(const OutlineDensity& d, int mode, int n, float half, bool horizontal, float offset);
    int begin(const OutlineDensity& d, int mode, int n, float half, bool horizontal, float offset, float scale, float target);
    JobState poll(Job& j, bool wantGrid, GLenum& r);
    bool ok_ = false;
    GLuint densityProg_ = 0, levelProg_ = 0;
    GLint uMode_ = -1, uN_ = -1, uHalf_ = -1, uHor_ = -1, uOff_ = -1, uLag_ = -1, uNq_ = -1, uAngRe_ = -1, uAngIm_ = -1, uCw_ = -1;
    GLint lCount_ = -1, lScale_ = -1, lTarget_ = -1;
    std::vector<Job> jobs_;
    int nextId_ = 1;
};

bool OutlineCompute::compile(const char* src, GLuint& program) {
    const GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(cs, 1, &src, nullptr);
    glCompileShader(cs);
    GLint good = 0;
    glGetShaderiv(cs, GL_COMPILE_STATUS, &good);
    if (!good) { char log[2048]; glGetShaderInfoLog(cs, sizeof log, nullptr, log); std::fprintf(stderr, "outline compute shader compile error: %s\n", log); glDeleteShader(cs); return false; }
    program = glCreateProgram();
    glAttachShader(program, cs);
    glLinkProgram(program);
    glDeleteShader(cs);
    glGetProgramiv(program, GL_LINK_STATUS, &good);
    if (!good) { char log[2048]; glGetProgramInfoLog(program, sizeof log, nullptr, log); std::fprintf(stderr, "outline compute shader link error: %s\n", log); glDeleteProgram(program); program = 0; return false; }
    return true;
}

bool OutlineCompute::init() {
    GLint maj = 0, min = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &maj); glGetIntegerv(GL_MINOR_VERSION, &min);
    if (maj < 4 || (maj == 4 && min < 3)) return false;                     // a 3.3 context: no compute shaders, the CPU path
    const char* missing = nullptr;
    if (!loadGL43(&missing)) { std::fprintf(stderr, "OpenGL 4.3 function missing: %s\n", missing ? missing : "?"); return false; }
    if (!compile(kOutlineDensityCS, densityProg_) || !compile(kOutlineLevelCS, levelProg_)) { shutdown(); return false; }
    uMode_ = glGetUniformLocation(densityProg_, "uMode"); uN_ = glGetUniformLocation(densityProg_, "uN"); uHalf_ = glGetUniformLocation(densityProg_, "uHalf");
    uHor_ = glGetUniformLocation(densityProg_, "uHorizontal"); uOff_ = glGetUniformLocation(densityProg_, "uOffset"); uLag_ = glGetUniformLocation(densityProg_, "uLag");
    uNq_ = glGetUniformLocation(densityProg_, "uNq"); uAngRe_ = glGetUniformLocation(densityProg_, "uAngRe"); uAngIm_ = glGetUniformLocation(densityProg_, "uAngIm"); uCw_ = glGetUniformLocation(densityProg_, "uCw");
    lCount_ = glGetUniformLocation(levelProg_, "uCount"); lScale_ = glGetUniformLocation(levelProg_, "uScale"); lTarget_ = glGetUniformLocation(levelProg_, "uTarget");
    ok_ = true;
    return true;
}

void OutlineCompute::shutdown() {
    while (!jobs_.empty()) release(jobs_.back().id);
    if (densityProg_) glDeleteProgram(densityProg_);
    if (levelProg_) glDeleteProgram(levelProg_);
    densityProg_ = levelProg_ = 0; ok_ = false;
}

void OutlineCompute::release(int id) {
    for (std::size_t i = 0; i < jobs_.size(); i++)
        if (jobs_[i].id == id) {
            Job& j = jobs_[i];
            if (j.sync) glDeleteSync(j.sync);
            if (j.vals) glDeleteBuffers(1, &j.vals);
            if (j.level) glDeleteBuffers(1, &j.level);
            jobs_.erase(jobs_.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
}

void OutlineCompute::cancel(int job) { release(job); }

void OutlineCompute::setDensity(const OutlineDensity& d, int mode, int n, float half, bool horizontal, float offset) {
    glUseProgram(densityProg_);
    glUniform1i(uMode_, mode); glUniform1i(uN_, n); glUniform1f(uHalf_, half); glUniform1i(uHor_, horizontal ? 1 : 0); glUniform1f(uOff_, offset);
    glUniform4f(uLag_, d.lag[0], d.lag[1], d.lag[2], d.lag[3]); glUniform1f(uNq_, d.n);
    glUniform1i(uAngRe_, d.ang); glUniform1i(uAngIm_, d.angIm); glUniform2f(uCw_, d.cIm, d.weight);
}

int OutlineCompute::begin(const OutlineDensity& d, int mode, int n, float half, bool horizontal, float offset, float scale, float target) {
    if (!ok_ || n < 4 || n % 4 != 0 || jobs_.size() >= kMaxJobs) return 0;
    Job j;
    j.id = nextId_++; j.grid = mode == 0;
    j.count = mode == 0 ? static_cast<long>(n) * n * n : static_cast<long>(n) * n;
    glGenBuffers(1, &j.vals);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, j.vals);
    glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(j.count) * 4, nullptr, GL_DYNAMIC_READ);
    setDensity(d, mode, n, half, horizontal, offset);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, j.vals);
    glDispatchCompute(static_cast<GLuint>(n / 4), static_cast<GLuint>(n / 4), static_cast<GLuint>(mode == 0 ? n / 4 : 1));
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    if (mode == 0) {                                                         // the 90 % level, in one work group, straight from the grid on the GPU
        glGenBuffers(1, &j.level);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, j.level);
        glBufferData(GL_SHADER_STORAGE_BUFFER, 8, nullptr, GL_DYNAMIC_READ);
        glUseProgram(levelProg_);
        glUniform1i(lCount_, static_cast<GLint>(j.count)); glUniform1f(lScale_, scale); glUniform1f(lTarget_, target);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, j.vals);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, j.level);
        glDispatchCompute(1, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    } else {
        glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    }
    glUseProgram(0);
    j.sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    jobs_.push_back(j);
    return j.id;
}

int OutlineCompute::beginGrid(const OutlineDensity& d, int N, float half, float scale, float target) { return begin(d, 0, N, half, false, 0, scale, target); }
int OutlineCompute::beginPlane(const OutlineDensity& d, int n, float half, bool horizontal, float offset) { return begin(d, 1, n, half, horizontal, offset, 0, 0); }

JobState OutlineCompute::poll(Job& j, bool wantGrid, GLenum& r) {
    if (j.grid != wantGrid) return JobState::Failed;
    r = glClientWaitSync(j.sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0);              // a zero timeout: it never waits
    if (r == GL_ALREADY_SIGNALED || r == GL_CONDITION_SATISFIED) return JobState::Done;
    if (r == GL_TIMEOUT_EXPIRED) return JobState::Pending;
    return JobState::Failed;
}

JobState OutlineCompute::pollGrid(int job, std::vector<float>& grid, float& level) {
    Job* j = find(job);
    if (!j) return JobState::Failed;
    GLenum r = 0;
    const JobState st = poll(*j, true, r);
    if (st == JobState::Pending) return st;
    if (st == JobState::Done) {
        grid.resize(static_cast<std::size_t>(j->count));
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, j->vals);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(j->count) * 4, grid.data());      // the fence has signalled: this is a copy, not a wait
        unsigned res[2] = {0, 0};
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, j->level);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 8, res);
        std::memcpy(&level, &res[0], 4);
    }
    release(job);
    return st;
}

JobState OutlineCompute::pollPlane(int job, std::vector<float>& values) {
    Job* j = find(job);
    if (!j) return JobState::Failed;
    GLenum r = 0;
    const JobState st = poll(*j, false, r);
    if (st == JobState::Pending) return st;
    if (st == JobState::Done) {
        values.resize(static_cast<std::size_t>(j->count));
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, j->vals);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(j->count) * 4, values.data());
    }
    release(job);
    return st;
}

IOutlineCompute* makeOutlineCompute() {
    OutlineCompute* o = new OutlineCompute();
    if (!o->init()) { delete o; return nullptr; }
    return o;
}

}  // namespace render
