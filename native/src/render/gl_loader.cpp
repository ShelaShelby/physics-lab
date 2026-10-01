#include "gl_loader.h"

#include <SDL3/SDL.h>

namespace plgl {

#define PL_GL_DEFINE(ret, name, args) PFN_##name name = nullptr;
PL_GL_FUNCTIONS(PL_GL_DEFINE)
#undef PL_GL_DEFINE

bool loadGL(const char** missing) {
#define PL_GL_LOAD(ret, name, args)                                                  \
    name = reinterpret_cast<PFN_##name>(SDL_GL_GetProcAddress(#name));               \
    if (!name) { if (missing) *missing = #name; return false; }
    PL_GL_FUNCTIONS(PL_GL_LOAD)
#undef PL_GL_LOAD
    return true;
}

}  // namespace plgl
