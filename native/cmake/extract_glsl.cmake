# 0.60.0 (Vulkan step 2, PLAN_VULKAN.md): one shader's GLSL text taken out of src/render/shaders.h at build time, so the Vulkan SPIR-V is made from
# the very text the OpenGL path compiles (one source per shader body; nothing copied by hand). glslang then compiles it for Vulkan with its relaxed
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
