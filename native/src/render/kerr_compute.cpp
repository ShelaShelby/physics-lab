#include "kerr_compute.h"

#include <cstdio>
#include <string>

#include "shaders.h"

namespace render {

std::string kerrShaderId() {
    unsigned long long h = 1469598103934665603ULL;                  // FNV-1a, 64 bit
    for (const char* p = kKerrTableCS; *p; p++) { h ^= static_cast<unsigned char>(*p); h *= 1099511628211ULL; }
    char b[40];
    std::snprintf(b, sizeof b, "shader %016llx", h);
    return b;
}

}  // namespace render
