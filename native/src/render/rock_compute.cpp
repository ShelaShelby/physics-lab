#include "rock_compute.h"

#include <algorithm>
#include <cstdio>

#include "gl_loader.h"
#include "shaders.h"

using namespace plgl;

namespace render {

// the OpenGL 4.3 implementation of IRockCompute (needs a current 4.3 context)
class RockCompute : public IRockCompute {
public:
    ~RockCompute() override { shutdown(); }
    bool init();
    void shutdown();
    bool ok() const override { return ok_; }
    void keepOnly(const std::vector<std::uint64_t>& live) override;
    int slotFor(const RockSet& set) override;
    bool dispatch(int slot, const RockFrame& f) override;
    unsigned positionsBuffer(int slot) const override { return slot >= 0 && slot < kSlots && slots_[slot].used ? slots_[slot].pos : 0; }
    bool readPositions(int slot, std::vector<float>& xy) override;
    int uploads() const override { return uploads_; }
private:
    static constexpr int kSlots = 16;
    struct Slot { bool used = false; std::uint64_t serial = 0; int count = 0; bool isStatic = false; GLuint el = 0, nn = 0, pos = 0; };
    void freeSlot(int i);
    void clearSlots();
    bool ok_ = false;
    GLuint program_ = 0;
    GLint uCount_ = -1, uStatic_ = -1, uD3_ = -1, uT_ = -1, uLib_ = -1, uBc_ = -1, uCam_ = -1, uView_ = -1, uOff_ = -1;
    Slot slots_[kSlots];
    int uploads_ = 0;
};

bool RockCompute::init() {
    GLint maj = 0, min = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &maj); glGetIntegerv(GL_MINOR_VERSION, &min);
    if (maj < 4 || (maj == 4 && min < 3)) return false;                     // a 3.3 context: no compute shaders, the CPU path
    const char* missing = nullptr;
    if (!loadGL43(&missing)) { std::fprintf(stderr, "OpenGL 4.3 function missing: %s\n", missing ? missing : "?"); return false; }
    const GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(cs, 1, &kRocksCS, nullptr);
    glCompileShader(cs);
    GLint good = 0;
    glGetShaderiv(cs, GL_COMPILE_STATUS, &good);
    if (!good) { char log[2048]; glGetShaderInfoLog(cs, sizeof log, nullptr, log); std::fprintf(stderr, "rocks compute shader compile error: %s\n", log); glDeleteShader(cs); return false; }   // (no double precision: the CPU path)
    program_ = glCreateProgram();
    glAttachShader(program_, cs);
    glLinkProgram(program_);
    glDeleteShader(cs);
    glGetProgramiv(program_, GL_LINK_STATUS, &good);
    if (!good) { char log[2048]; glGetProgramInfoLog(program_, sizeof log, nullptr, log); std::fprintf(stderr, "rocks compute shader link error: %s\n", log); glDeleteProgram(program_); program_ = 0; return false; }
    uCount_ = glGetUniformLocation(program_, "uCount"); uStatic_ = glGetUniformLocation(program_, "uStatic"); uD3_ = glGetUniformLocation(program_, "uD3");
    uT_ = glGetUniformLocation(program_, "uT"); uLib_ = glGetUniformLocation(program_, "uLibPhase"); uBc_ = glGetUniformLocation(program_, "uBc");
    uCam_ = glGetUniformLocation(program_, "uCam"); uView_ = glGetUniformLocation(program_, "uView"); uOff_ = glGetUniformLocation(program_, "uOff");
    ok_ = true;
    return true;
}

void RockCompute::freeSlot(int i) {
    if (!slots_[i].used) return;
    const GLuint b[3] = {slots_[i].el, slots_[i].nn, slots_[i].pos};
    glDeleteBuffers(3, b);
    slots_[i] = Slot();
}

void RockCompute::clearSlots() { for (int i = 0; i < kSlots; i++) freeSlot(i); }

void RockCompute::keepOnly(const std::vector<std::uint64_t>& live) {
    if (!ok_) return;
    for (int i = 0; i < kSlots; i++)
        if (slots_[i].used && std::find(live.begin(), live.end(), slots_[i].serial) == live.end()) freeSlot(i);
}

void RockCompute::shutdown() {
    if (!ok_) return;
    clearSlots();
    glDeleteProgram(program_);
    program_ = 0; ok_ = false;
}

int RockCompute::slotFor(const RockSet& s) {
    if (!ok_ || s.count <= 0 || !s.el) return -1;
    int freeAt = -1;
    for (int i = 0; i < kSlots; i++) {
        if (!slots_[i].used) { if (freeAt < 0) freeAt = i; continue; }
        if (slots_[i].serial == s.serial && slots_[i].count == s.count && slots_[i].isStatic == s.isStatic) return i;   // already on the GPU
    }
    if (freeAt < 0) return -1;
    Slot& sl = slots_[freeAt];
    sl.used = true;
    GLuint b[3];
    glGenBuffers(3, b);
    sl.serial = s.serial; sl.count = s.count; sl.isStatic = s.isStatic; sl.el = b[0]; sl.nn = b[1]; sl.pos = b[2];
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sl.el);
    glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(s.count) * kRockStride * 4, s.el, GL_STATIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sl.nn);
    glBufferData(GL_SHADER_STORAGE_BUFFER, s.nn ? static_cast<GLsizeiptr>(s.count) * 8 : 8, s.nn, GL_STATIC_DRAW);            // (a still group has no mean motions: 8 bytes so the binding is valid)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, sl.pos);
    glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(s.count) * 8, nullptr, GL_DYNAMIC_COPY);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    uploads_++;
    return freeAt;
}

bool RockCompute::dispatch(int slot, const RockFrame& f) {
    if (!ok_ || slot < 0 || slot >= kSlots || !slots_[slot].used) return false;
    const Slot& sl = slots_[slot];
    glUseProgram(program_);
    const float tHi = static_cast<float>(f.t), tLo = static_cast<float>(f.t - static_cast<double>(tHi));       // t = tHi + tLo exactly (24 + 24 bits)
    glUniform1i(uCount_, sl.count); glUniform1i(uStatic_, sl.isStatic ? 1 : 0); glUniform1i(uD3_, f.d3 ? 1 : 0);
    glUniform2f(uT_, tHi, tLo);
    glUniform1f(uLib_, static_cast<float>(f.libPhase));
    glUniform4f(uBc_, static_cast<float>(f.bc[0]), static_cast<float>(f.bc[1]), static_cast<float>(f.bc[2]), 0);
    glUniform4f(uCam_, static_cast<float>(f.ca), static_cast<float>(f.sa), static_cast<float>(f.cb), static_cast<float>(f.sb));
    glUniform4f(uView_, static_cast<float>(f.px), static_cast<float>(f.W), static_cast<float>(f.H), static_cast<float>(f.D));
    glUniform4f(uOff_, static_cast<float>(f.cd), static_cast<float>(f.ox), static_cast<float>(f.oy), 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, sl.el);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, sl.nn);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, sl.pos);
    glDispatchCompute(static_cast<GLuint>((sl.count + 63) / 64), 1, 1);
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);                          // the draw reads the positions as vertex data; no CPU wait
    glUseProgram(0);
    return true;
}

bool RockCompute::readPositions(int slot, std::vector<float>& xy) {
    if (!ok_ || slot < 0 || slot >= kSlots || !slots_[slot].used) return false;
    xy.resize(static_cast<std::size_t>(slots_[slot].count) * 2);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, slots_[slot].pos);
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(xy.size() * 4), xy.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    return true;
}

IRockCompute* makeRockCompute() {
    RockCompute* r = new RockCompute();
    if (!r->init()) { delete r; return nullptr; }
    return r;
}

}  // namespace render
