#include "molecule_card.h"

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <initializer_list>

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
constexpr GLbitfield kAfterCompute = GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT;
constexpr std::size_t kSets = 18;                // the still cloud and a vibration's 17 moments (molecule_view.h RealCache::kVibLevels)
}  // namespace

std::string molCloudShaderId() {
    unsigned long long h = 1469598103934665603ULL;                  // FNV-1a, 64 bit, over the three texts and the two fragment shaders they link with
    for (const char* t : {kMolDotsCS, kMolDotVS, kBlobFS, kMolGridVS, kCloudGridFS})
        for (const char* c = t; *c; c++) { h ^= static_cast<unsigned char>(*c); h *= 1099511628211ULL; }
    char b[48];
    std::snprintf(b, sizeof b, "molcloud %016llx", h);
    return b;
}

bool MoleculeCardGl::build(KerrSteps* steps, std::string& why) {
    if (built()) return true;
    const std::string cName = "compile the Density cloud's shaders", lName = "link the Density cloud's shaders";
    struct Src { GLenum kind; const char* text; const char* name; };
    const Src src[5] = {{GL_COMPUTE_SHADER, kMolDotsCS, "kMolDotsCS"}, {GL_VERTEX_SHADER, kMolDotVS, "kMolDotVS"}, {GL_FRAGMENT_SHADER, kBlobFS, "kBlobFS"},
                        {GL_VERTEX_SHADER, kMolGridVS, "kMolGridVS"}, {GL_FRAGMENT_SHADER, kCloudGridFS, "kCloudGridFS"}};
    GLuint sh[5] = {0, 0, 0, 0, 0};
    std::string logs;
    bool good = true;
    char log[4096] = {0};
    if (steps) steps->begin(cName);
    double t = nowMs();
    for (int i = 0; i < 5 && good; i++) {
        sh[i] = glCreateShader(src[i].kind);
        glShaderSource(sh[i], 1, &src[i].text, nullptr);
        glCompileShader(sh[i]);
        GLint ok = 0;
        glGetShaderiv(sh[i], GL_COMPILE_STATUS, &ok);
        log[0] = 0;
        glGetShaderInfoLog(sh[i], sizeof log, nullptr, log);
        if (log[0]) logs += std::string(logs.empty() ? "" : "; ") + src[i].name + ": " + oneLine(log);
        if (!ok) { good = false; why = std::string("the graphics driver did not compile the Density cloud's ") + src[i].name; std::fprintf(stderr, "molecule card shader %s compile error: %s\n", src[i].name, log); }
    }
    if (steps) steps->end(cName, std::string(good ? "compiled" : "FAILED") + " in " + std::to_string(static_cast<int>(nowMs() - t)) + " ms" +
                                 (logs.empty() ? std::string(", the driver's logs are empty") : ", the driver's logs: " + logs));
    if (!good) { for (GLuint s : sh) if (s) glDeleteShader(s); return false; }
    if (steps) steps->begin(lName);
    t = nowMs();
    logs.clear();
    const auto link = [&](std::initializer_list<GLuint> parts, const char* name) -> GLuint {
        const GLuint pr = glCreateProgram();
        for (GLuint s : parts) glAttachShader(pr, s);
        glLinkProgram(pr);
        GLint ok = 0;
        glGetProgramiv(pr, GL_LINK_STATUS, &ok);
        log[0] = 0;
        glGetProgramInfoLog(pr, sizeof log, nullptr, log);
        if (log[0]) logs += std::string(logs.empty() ? "" : "; ") + name + ": " + oneLine(log);
        if (!ok) { std::fprintf(stderr, "molecule card program %s link error: %s\n", name, log); glDeleteProgram(pr); return 0; }
        return pr;
    };
    cs_ = link({sh[0]}, "the dots (kMolDotsCS)");
    dot_ = cs_ ? link({sh[1], sh[2]}, "the soft dots (kMolDotVS + kBlobFS)") : 0;
    grid_ = dot_ ? link({sh[3], sh[4]}, "the grid (kMolGridVS + kCloudGridFS)") : 0;
    for (GLuint s : sh) glDeleteShader(s);
    good = built();
    if (steps) steps->end(lName, std::string(good ? "linked" : "FAILED") + " in " + std::to_string(static_cast<int>(nowMs() - t)) + " ms" +
                                 (logs.empty() ? std::string(", the driver's logs are empty") : ", the driver's logs: " + logs));
    if (!good) {
        for (GLuint* p : {&cs_, &dot_, &grid_}) if (*p) { glDeleteProgram(*p); *p = 0; }
        why = "the graphics driver did not link the Density cloud's shaders";
        return false;
    }
    uR0_ = glGetUniformLocation(cs_, "uR0"); uR1_ = glGetUniformLocation(cs_, "uR1"); uR2_ = glGetUniformLocation(cs_, "uR2");
    uScr_ = glGetUniformLocation(cs_, "uScr"); uView_ = glGetUniformLocation(cs_, "uView"); uN_ = glGetUniformLocation(cs_, "uN");
    dSize_ = glGetUniformLocation(dot_, "uSize"); dLight_ = glGetUniformLocation(dot_, "uLight"); dColour_ = glGetUniformLocation(dot_, "uColour");
    dFuzz_ = glGetUniformLocation(dot_, "uFuzz"); dSphN_ = glGetUniformLocation(dot_, "uSphN"); gGrid_ = glGetUniformLocation(grid_, "uGrid");
    return true;
}

void MoleculeCardGl::reset() {
    while (!jobs_.empty()) release(jobs_.back().id);
    for (GLuint* p : {&cs_, &dot_, &grid_}) if (*p) { glDeleteProgram(*p); *p = 0; }
    for (Set& s : sets_) if (s.buf) glDeleteBuffers(1, &s.buf);
    sets_.clear();
    if (out_) glDeleteBuffers(1, &out_);
    if (corner_) glDeleteBuffers(1, &corner_);
    if (dotVao_) glDeleteVertexArrays(1, &dotVao_);
    if (gridVao_) glDeleteVertexArrays(1, &gridVao_);
    if (gridTex_) glDeleteTextures(1, &gridTex_);
    if (gridFbo_) glDeleteFramebuffers(1, &gridFbo_);
    out_ = corner_ = dotVao_ = gridVao_ = gridTex_ = gridFbo_ = 0; outN_ = 0; gridW_ = gridH_ = 0;
    if (fence_) glDeleteSync(fence_);
    fence_ = nullptr;
    ready_ = false;
}

void MoleculeCardGl::release(int id) {
    for (std::size_t i = 0; i < jobs_.size(); i++)
        if (jobs_[i].id == id) {
            if (jobs_[i].sync) glDeleteSync(jobs_[i].sync);
            if (jobs_[i].pts) glDeleteBuffers(1, &jobs_[i].pts);
            if (jobs_[i].out) glDeleteBuffers(1, &jobs_[i].out);
            jobs_.erase(jobs_.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
}

void MoleculeCardGl::dispatch(GLuint ptsBuf, GLuint outBuf, long n, const MolCloudCam& c) {
    glUseProgram(cs_);
    glUniform4f(uR0_, c.R[0], c.R[1], c.R[2], c.panX);
    glUniform4f(uR1_, c.R[3], c.R[4], c.R[5], c.panY);
    glUniform4f(uR2_, c.R[6], c.R[7], c.R[8], c.ext);
    glUniform4f(uScr_, c.cx, c.cy, c.s, 0.f);
    glUniform2f(uView_, c.w, c.h);
    glUniform1i(uN_, static_cast<GLint>(n));
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ptsBuf);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, outBuf);
    glDispatchCompute(static_cast<GLuint>((n + 63) / 64), 1, 1);
    glMemoryBarrier(kAfterCompute);
    glUseProgram(0);
}

int MoleculeCardGl::beginProject(const float* pts, long n, const MolCloudCam& cam) {
    if (!cs_ || !pts || n <= 0 || n > (1 << 20) || jobs_.size() >= 16) return 0;
    Job j;
    j.id = nextId_++;
    j.n = n;
    glGenBuffers(1, &j.pts);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, j.pts);
    glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(n * 3 * static_cast<long>(sizeof(float))), pts, GL_STATIC_DRAW);
    glGenBuffers(1, &j.out);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, j.out);
    glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(n * 4 * static_cast<long>(sizeof(float))), nullptr, GL_DYNAMIC_READ);
    dispatch(j.pts, j.out, n, cam);
    j.sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    jobs_.push_back(j);
    return j.id;
}

JobState MoleculeCardGl::pollProject(int job, std::vector<float>& out) {
    Job* j = nullptr;
    for (Job& q : jobs_) if (q.id == job) j = &q;
    if (!j) return JobState::Failed;
    const GLenum r = glClientWaitSync(j->sync, GL_SYNC_FLUSH_COMMANDS_BIT, 0);   // a zero timeout: it never waits
    if (r == GL_TIMEOUT_EXPIRED) return JobState::Pending;
    const bool done = r == GL_ALREADY_SIGNALED || r == GL_CONDITION_SATISFIED;
    if (done) {
        out.resize(static_cast<std::size_t>(4 * j->n));
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, j->out);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(j->n * 4 * static_cast<long>(sizeof(float))), out.data());   // the fence has signalled: a copy
    }
    release(job);
    return done ? JobState::Done : JobState::Failed;
}

void MoleculeCardGl::drawCloud(const MolCloudDraw& d, double width, double height, const float* fuzz, std::vector<float>& cells) {
    if (!built() || !d.pts || d.cnt <= 0) return;
    // 1. the dots of this set on the card (sent once per set; the least recently used set's buffer is taken for a new one)
    Set* set = nullptr;
    for (Set& s : sets_) if (s.epoch == d.epoch && s.n == d.cnt) set = &s;
    if (!set) {
        if (sets_.size() < kSets) sets_.push_back(Set{});
        Set* old = &sets_[0];
        for (Set& s : sets_) if (s.used < old->used) old = &s;
        set = old;
        if (!set->buf) glGenBuffers(1, &set->buf);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, set->buf);
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(d.cnt * 3 * static_cast<long>(sizeof(float))), d.pts, GL_STATIC_DRAW);
        bytes_ += d.cnt * 3 * static_cast<long>(sizeof(float));
        set->epoch = d.epoch; set->n = d.cnt;
    }
    set->used = ++useClock_;
    if (!out_ || outN_ < d.cnt) {
        if (!out_) glGenBuffers(1, &out_);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, out_);
        glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(d.cnt * 4 * static_cast<long>(sizeof(float))), nullptr, GL_DYNAMIC_COPY);
        outN_ = d.cnt;
    }
    if (!dotVao_) {
        const float corners[12] = {-0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f};   // (the blob program's two triangles)
        glGenBuffers(1, &corner_);
        glBindBuffer(GL_ARRAY_BUFFER, corner_);
        glBufferData(GL_ARRAY_BUFFER, sizeof corners, corners, GL_STATIC_DRAW);
        glGenVertexArrays(1, &dotVao_);
        glBindVertexArray(dotVao_);
        glBindBuffer(GL_ARRAY_BUFFER, corner_);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, out_);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribDivisor(1, 1);
        glGenVertexArrays(1, &gridVao_);
        glBindVertexArray(gridVao_);
        glBindBuffer(GL_ARRAY_BUFFER, out_);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    } else {                                                     // (the output buffer may have been made again larger: point both at it)
        for (GLuint vao : {dotVao_, gridVao_}) {
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, out_);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
        }
        glBindVertexArray(0);
    }
    // 2. every dot's place, size and depth weight
    dispatch(set->buf, out_, d.cnt, d.cam);
    // 3. the exposure's grid, summed on the card from those, read back
    const int gw = static_cast<int>(d.cam.w / 8) + 2, gh = static_cast<int>(d.cam.h / 8) + 2;
    if (!gridFbo_) { glGenFramebuffers(1, &gridFbo_); glGenTextures(1, &gridTex_); }
    GLint prevFbo = 0, vp[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo); glGetIntegerv(GL_VIEWPORT, vp);
    if (gw != gridW_ || gh != gridH_) {
        gridW_ = gw; gridH_ = gh;
        glBindTexture(GL_TEXTURE_2D, gridTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, gw, gh, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, gridFbo_);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gridTex_, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, gridFbo_);
    glViewport(0, 0, gw, gh);
    glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
    glBlendFunc(GL_ONE, GL_ONE);
    glUseProgram(grid_);
    glUniform2f(gGrid_, static_cast<float>(gw), static_cast<float>(gh));
    glBindVertexArray(gridVao_);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(d.cnt));
    cells.resize(static_cast<std::size_t>(gw) * gh);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glReadPixels(0, 0, gw, gh, GL_RED, GL_FLOAT, cells.data());
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFbo)); glViewport(vp[0], vp[1], vp[2], vp[3]);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    const double light = d.exposure ? d.exposure(cells) : -1;
    if (light >= 0) {
        // 4. the soft dots, instanced, in dot order (as the processor's list)
        if (d.additive) glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
        glUseProgram(dot_);
        glUniform2f(dSize_, static_cast<float>(width), static_cast<float>(height));
        glUniform1f(dLight_, static_cast<float>(light));
        glUniform4f(dColour_, d.colour[0], d.colour[1], d.colour[2], d.colour[3]);
        glUniform1fv(dFuzz_, 17, fuzz);
        glUniform1i(dSphN_, 0);
        glBindVertexArray(dotVao_);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(d.cnt));
    }
    glBindVertexArray(0);
    glUseProgram(0);
    if (d.fenceAfter && !fence_) fence_ = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);   // (once: a fence made again each frame may never be seen signalled
                                                                                            // when the card stays a frame behind)
}

int MoleculeCardGl::fenceState() {
    if (!fence_) return -1;
    const GLenum r = glClientWaitSync(fence_, GL_SYNC_FLUSH_COMMANDS_BIT, 0);
    if (r == GL_ALREADY_SIGNALED || r == GL_CONDITION_SATISFIED) { glDeleteSync(fence_); fence_ = nullptr; return 1; }
    if (r == GL_TIMEOUT_EXPIRED) return 0;
    glDeleteSync(fence_); fence_ = nullptr;
    return -1;
}

}  // namespace render
