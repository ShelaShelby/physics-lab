#include "porkchop_card.h"

#include <cstdio>

#include "shaders.h"

namespace render {

std::string porkchopShaderId() {
    unsigned long long h = 1469598103934665603ULL;                  // FNV-1a, 64 bit (as render/atom_card.cpp)
    for (const char* c = kPorkchopCS; *c; c++) { h ^= static_cast<unsigned char>(*c); h *= 1099511628211ULL; }
    char b[48];
    std::snprintf(b, sizeof b, "porkchop %016llx", h);
    return b;
}

}  // namespace render
