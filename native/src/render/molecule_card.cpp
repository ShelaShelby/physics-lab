#include "molecule_card.h"

#include <cstdio>
#include <initializer_list>

#include "shaders.h"

namespace render {

std::string molCloudShaderId() {
    unsigned long long h = 1469598103934665603ULL;                  // FNV-1a, 64 bit, over the three texts and the two fragment shaders they link with
    for (const char* t : {kMolDotsCS, kMolDotVS, kBlobFS, kMolGridVS, kCloudGridFS})
        for (const char* c = t; *c; c++) { h ^= static_cast<unsigned char>(*c); h *= 1099511628211ULL; }
    char b[48];
    std::snprintf(b, sizeof b, "molcloud %016llx", h);
    return b;
}

}  // namespace render
