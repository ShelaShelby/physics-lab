#include "atom_card.h"

#include <cstdio>

#include "shaders.h"

namespace render {

namespace {
std::string idOf(const char* prefix, const char* text) {
    unsigned long long h = 1469598103934665603ULL;                  // FNV-1a, 64 bit
    for (const char* c = text; *c; c++) { h ^= static_cast<unsigned char>(*c); h *= 1099511628211ULL; }
    char b[48];
    std::snprintf(b, sizeof b, "%s %016llx", prefix, h);
    return b;
}
}  // namespace

std::string sliceShaderId() { return idOf("slice", kSliceDotsCS); }
std::string jumpShaderId() { return idOf("jump", kJumpShowCS); }

const char* AtomCard::programName(int which) { return which == kSlice ? "the Slice's dots shader" : "the jump's groups shader"; }

}  // namespace render
