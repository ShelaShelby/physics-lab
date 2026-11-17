#include "atom_card.h"

#include <chrono>
#include <cstddef>
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
std::string idOf(const char* prefix, const char* text) {
    unsigned long long h = 1469598103934665603ULL;                  // FNV-1a, 64 bit
    for (const char* c = text; *c; c++) { h ^= static_cast<unsigned char>(*c); h *= 1099511628211ULL; }
    char b[48];
    std::snprintf(b, sizeof b, "%s %016llx", prefix, h);
    return b;
}
constexpr GLbitfield kAfterCompute = GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
}  // namespace

std::string sliceShaderId() { return idOf("slice", kSliceDotsCS); }
std::string jumpShaderId() { return idOf("jump", kJumpShowCS); }

const char* AtomCard::programName(int which) { return which == kSlice ? "the Slice's dots shader" : "the jump's groups shader"; }

bool AtomCard::build(int which, KerrSteps* steps, std::string& why) {
    which &= 1;
    if (prog_[which]) return true;
    const std::string name = programName(which), cName = "compile " + name, lName = "link " + name;
    const char* src = which == kSlice ? kSliceDotsCS : kJumpShowCS;
    GLint good = 0;
    char log[4096] = {0};
    if (steps) steps->begin(cName);
    double t = nowMs();
    const GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(cs, 1, &src, nullptr);
    glCompileShader(cs);
    glGetShaderiv(cs, GL_COMPILE_STATUS, &good);
    glGetShaderInfoLog(cs, sizeof log, nullptr, log);
    if (steps) steps->end(cName, std::string(good ? "compiled" : "FAILED") + " in " + std::to_string(static_cast<int>(nowMs() - t)) + " ms" +
                                 (log[0] ? ", the driver's log: " + oneLine(log) : std::string(", the driver's log is empty")));
    if (!good) {
        std::fprintf(stderr, "atom card shader (%s) compile error: %s\n", name.c_str(), log);
        glDeleteShader(cs);
        why = "the graphics driver did not compile " + name;
        return false;
    }
    if (steps) steps->begin(lName);
    t = nowMs();
    const GLuint pr = glCreateProgram();
    glAttachShader(pr, cs);
    glLinkProgram(pr);
    glDeleteShader(cs);
    glGetProgramiv(pr, GL_LINK_STATUS, &good);
    log[0] = 0;
    glGetProgramInfoLog(pr, sizeof log, nullptr, log);
    if (steps) steps->end(lName, std::string(good ? "linked" : "FAILED") + " in " + std::to_string(static_cast<int>(nowMs() - t)) + " ms" +
                                 (log[0] ? ", the driver's log: " + oneLine(log) : std::string(", the driver's log is empty")));
    if (!good) {
        std::fprintf(stderr, "atom card shader (%s) link error: %s\n", name.c_str(), log);
        glDeleteProgram(pr);
        why = "the graphics driver did not link " + name;
        return false;
    }
    prog_[which] = pr;
    return true;
}

void AtomCard::reset() {
    while (!jobs_.empty()) release(jobs_.back().id);
    for (GLuint& p : prog_) if (p) { glDeleteProgram(p); p = 0; }
    if (jumpFixed_) glDeleteBuffers(1, &jumpFixed_);
    if (jumpOut_) glDeleteBuffers(1, &jumpOut_);
    jumpFixed_ = jumpOut_ = 0; jumpEpoch_ = 0; jumpN_ = 0;
    if (fence_) glDeleteSync(fence_);
    fence_ = nullptr;
    sliceReady_ = jumpReady_ = sliceUsed_ = false;
}

void AtomCard::release(int id) {
    for (std::size_t i = 0; i < jobs_.size(); i++)
        if (jobs_[i].id == id) {
            if (jobs_[i].sync) glDeleteSync(jobs_[i].sync);
            if (jobs_[i].buf) glDeleteBuffers(1, &jobs_[i].buf);
            jobs_.erase(jobs_.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
}

JobState AtomCard::poll(Job& j) {
    const GLenum r = glClientWaitSync(j.sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0);   // a zero timeout: it never waits
    if (r == GL_ALREADY_SIGNALED || r == GL_CONDITION_SATISFIED) return JobState::Done;
    if (r == GL_TIMEOUT_EXPIRED) return JobState::Pending;
    return JobState::Failed;
}

int AtomCard::beginSlice(const SliceDotsJob& s) {
    const GLuint pr = prog_[kSlice];
    if (!pr || s.tries <= 0 || s.tries > (1 << 20) || s.nh <= 0 || s.nh > 16 || jobs_.size() >= 8) return 0;
    if (sliceReady_ && !sliceUsed_) { sliceUsed_ = true; if (beforeFirstUse) beforeFirstUse(); }   // (the guard's marker before the first batch the lab asks for)
    Job j;
    j.id = nextId_++;
    j.bytes = static_cast<long>(s.tries) * 16;
    glGenBuffers(1, &j.buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, j.buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(j.bytes), nullptr, GL_DYNAMIC_READ);
    glUseProgram(pr);
    glUniform1ui(glGetUniformLocation(pr, "uSeed"), s.seed);
    glUniform1i(glGetUniformLocation(pr, "uTries"), s.tries);
    glUniform1i(glGetUniformLocation(pr, "uAxis"), s.axis);
    glUniform1f(glGetUniformLocation(pr, "uOff"), s.off);
    glUniform1f(glGetUniformLocation(pr, "uX0"), s.x0);
    glUniform1f(glGetUniformLocation(pr, "uLnG0"), s.lnG0);
    glUniform1f(glGetUniformLocation(pr, "uScale"), s.scale);
    glUniform1f(glGetUniformLocation(pr, "uBound"), s.bound);
    glUniform1i(glGetUniformLocation(pr, "uAng"), s.ang);
    glUniform1i(glGetUniformLocation(pr, "uAngIm"), s.angIm);
    glUniform1f(glGetUniformLocation(pr, "uCIm"), s.cIm);
    glUniform1i(glGetUniformLocation(pr, "uNh"), s.nh);
    glUniform1fv(glGetUniformLocation(pr, "uH"), 16, s.h);
    glUniform1fv(glGetUniformLocation(pr, "uC"), 16, s.c);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, j.buf);
    glDispatchCompute(static_cast<GLuint>((s.tries + 63) / 64), 1, 1);
    glMemoryBarrier(kAfterCompute);
    glUseProgram(0);
    j.sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    jobs_.push_back(j);
    return j.id;
}

JobState AtomCard::pollSlice(int job, std::vector<float>& res) {
    Job* j = find(job);
    if (!j) return JobState::Failed;
    const JobState st = poll(*j);
    if (st == JobState::Pending) return st;
    if (st == JobState::Done) {
        res.resize(static_cast<std::size_t>(j->bytes / 4));
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, j->buf);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(j->bytes), res.data());   // the fence has signalled: a copy, not a wait
    }
    release(job);
    return st;
}

void AtomCard::dispatchJump(GLuint fixedBuf, GLuint outBuf, long jn, std::uint64_t t0, std::uint64_t t1) {
    const GLuint pr = prog_[kJump];
    glUseProgram(pr);
    glUniform1i(glGetUniformLocation(pr, "uJn"), static_cast<GLint>(jn));
    glUniform2ui(glGetUniformLocation(pr, "uT0"), static_cast<GLuint>(t0 & 0xFFFFFFFFu), static_cast<GLuint>(t0 >> 32));
    glUniform2ui(glGetUniformLocation(pr, "uT1"), static_cast<GLuint>(t1 & 0xFFFFFFFFu), static_cast<GLuint>(t1 >> 32));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, fixedBuf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, outBuf);
    const long words = (2 * jn + 3) / 4;
    glDispatchCompute(static_cast<GLuint>((words + 63) / 64), 1, 1);
    glMemoryBarrier(kAfterCompute);
    glUseProgram(0);
}

bool AtomCard::jumpGroups(const unsigned char* fixedGroups, long jn, std::uint64_t epoch, std::uint64_t t0, std::uint64_t t1) {
    if (!prog_[kJump] || jn <= 0 || !fixedGroups) return false;
    const long bytes = (2 * jn + 3) / 4 * 4;
    if (!jumpFixed_) { glGenBuffers(1, &jumpFixed_); glGenBuffers(1, &jumpOut_); }
    if (epoch != jumpEpoch_ || jn != jumpN_) {                       // a new jump: its fixed groups sent once (the last word padded with "not shown")
        std::vector<unsigned char> padded(fixedGroups, fixedGroups + 2 * jn);
        padded.resize(static_cast<std::size_t>(bytes), 255);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, jumpFixed_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(bytes), padded.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, jumpOut_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(bytes), nullptr, GL_DYNAMIC_COPY);
        jumpEpoch_ = epoch; jumpN_ = jn;
    }
    dispatchJump(jumpFixed_, jumpOut_, jn, t0, t1);
    return true;
}

int AtomCard::beginJumpRead(const unsigned char* fixedGroups, long jn, std::uint64_t t0, std::uint64_t t1) {
    if (!prog_[kJump] || jn <= 0 || !fixedGroups || jobs_.size() >= 8) return 0;
    const long bytes = (2 * jn + 3) / 4 * 4;
    std::vector<unsigned char> padded(fixedGroups, fixedGroups + 2 * jn);
    padded.resize(static_cast<std::size_t>(bytes), 255);
    GLuint fixedBuf = 0;
    glGenBuffers(1, &fixedBuf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, fixedBuf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(bytes), padded.data(), GL_STATIC_DRAW);
    Job j;
    j.id = nextId_++;
    j.bytes = 2 * jn;
    glGenBuffers(1, &j.buf);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, j.buf);
    glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(bytes), nullptr, GL_DYNAMIC_READ);
    dispatchJump(fixedBuf, j.buf, jn, t0, t1);
    j.sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glDeleteBuffers(1, &fixedBuf);                                  // (deleted once the card has used it)
    jobs_.push_back(j);
    return j.id;
}

JobState AtomCard::pollJumpRead(int job, std::vector<unsigned char>& groups) {
    Job* j = find(job);
    if (!j) return JobState::Failed;
    const JobState st = poll(*j);
    if (st == JobState::Pending) return st;
    if (st == JobState::Done) {
        groups.resize(static_cast<std::size_t>(j->bytes));
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, j->buf);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(j->bytes), groups.data());
    }
    release(job);
    return st;
}

void AtomCard::fence() {
    if (fence_) glDeleteSync(fence_);
    fence_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
}

int AtomCard::fenceState() {
    if (!fence_) return -1;
    const GLenum r = glClientWaitSync(fence_, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
    if (r == GL_ALREADY_SIGNALED || r == GL_CONDITION_SATISFIED) { glDeleteSync(fence_); fence_ = nullptr; return 1; }
    if (r == GL_TIMEOUT_EXPIRED) return 0;
    glDeleteSync(fence_); fence_ = nullptr;
    return -1;
}

}  // namespace render
