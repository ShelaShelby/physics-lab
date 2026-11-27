# 0.60.0 (Vulkan step 2, the Vulkan plan, done in 0.69.0 (git keeps it)): one shader's GLSL text taken out of src/render/shaders.h at build time, so the Vulkan SPIR-V is made from
# the very text the tests compile with glslang (one source per shader body; nothing copied by hand). glslang then compiles it for Vulkan with its relaxed
# rules (-R: the loose uniforms go into a uniform block whose layout the app reads from the SPIR-V itself, render/vk_draw2d.cpp).
#   cmake -DSRC=<shaders.h> -DNAME=<kName> -DOUT=<file> -P extract_glsl.cmake
# 0.62.0: NAME may join several parts with "+" (kKerrSkyFS: kKerrSkyHeadPart+kEllipticPart+kKerrSkyTracePart+kKerrSkyMainPart, as render/kerr_sky.cpp
# kerrSkySource puts them together)
file(READ ${SRC} text)
string(REPLACE "+" ";" parts "${NAME}")
set(all "")
foreach(part ${parts})
  set(head "static const char* const ${part} = R\"(")
  string(FIND "${text}" "${head}" at)
  if(at LESS 0)
    message(FATAL_ERROR "extract_glsl: ${part} not found in ${SRC}")
  endif()
  string(LENGTH "${head}" headLen)
  math(EXPR from "${at} + ${headLen}")
  string(SUBSTRING "${text}" ${from} -1 rest)
  string(FIND "${rest}" ")\";" stop)
  if(stop LESS 0)
    message(FATAL_ERROR "extract_glsl: the end of ${part} not found")
  endif()
  string(SUBSTRING "${rest}" 0 ${stop} body)
  string(APPEND all "${body}")
endforeach()
string(REPLACE "\r" "" all "${all}")
file(WRITE ${OUT} "${all}")
# 0.67.0: VARIANT (the light paths' detective work, render/vk_helper.cpp): kKerrTableCS cut down, made only by the helper process while the light paths are
# off (their real pipeline crashed the driver): its own main() takes the place of the real one, which stays as mainFull() (unused)
# 0.95.0: VARIANT skystop1 .. skystop5 (the sky pass's split, blackhole_view.cpp skyTimeHook; PL_BH_SKYSTOP picks one): kKerrSkyFS with SKY_STOP defined
if(DEFINED VARIANT AND VARIANT MATCHES "^skystop([1-5])$")
  file(READ ${OUT} all)
  string(REPLACE "#version 330 core\n" "#version 330 core\n#define SKY_STOP ${CMAKE_MATCH_1}\n" all "${all}")
  file(WRITE ${OUT} "${all}")
elseif(DEFINED VARIANT)
  file(READ ${OUT} all)
  string(REPLACE "void main() {" "void mainFull() {" all "${all}")
  if(VARIANT STREQUAL "math")          # the double-precision functions alone (sine and cosine, exp, log, arctangent, cube root)
    string(APPEND all [=[
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= uint(uN)) return;
    double s, c;
    dsincos(In[i], s, c);
    Ou[i] = s + c + dexp(In[i]) + dlog(1.0LF + In[i] * In[i]) + datan2(In[i], 1.0LF + In[i]) + dcbrt(2.0LF + In[i]);
}
]=])
  elseif(VARIANT STREQUAL "step")      # a ray's start and one Dormand-Prince step (the right-hand side), no loop
    string(APPEND all [=[
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= uint(uN)) return;
    double y[5], dy[5], yn[5], k7[5], b, q;
    launch(In[2u * i], In[2u * i + 1u], y, b, q);
    rhs(b, q, y, dy);
    double e = dpStep(b, q, y, dy, 0.01LF, yn, k7);
    for (int k = 0; k < 5; k++) Ou[5u * i + uint(k)] = yn[k] + e;
}
]=])
  elseif(VARIANT STREQUAL "edge")      # the shadow's edge alone (uMode 0)
    string(APPEND all [=[
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= uint(uN)) return;
    Ed[i] = shadowEdge(In[i]);
}
]=])
  elseif(VARIANT STREQUAL "ray")       # one whole ray with its end handling (uMode 1), nothing else
    string(APPEND all [=[
void main() {
    int k = int(gl_GlobalInvocationID.x);
    if (k >= uN) return;
    ray(In[2 * k], In[2 * k + 1], 16 * k, true, false);
}
]=])
  elseif(VARIANT STREQUAL "short")     # the whole shader with its step loops bounded at 64 instead of 200,000
    string(REPLACE "void mainFull() {" "void main() {" all "${all}")
    string(REPLACE "#define MAXSTEPS 200000" "#define MAXSTEPS 64" all "${all}")
  else()
    message(FATAL_ERROR "extract_glsl: no variant ${VARIANT}")
  endif()
  file(WRITE ${OUT} "${all}")
endif()
