# 0.60.0 (Vulkan step 2, PLAN_VULKAN.md): one shader's GLSL text taken out of src/render/shaders.h at build time, so the Vulkan SPIR-V is made from
# the very text the OpenGL path compiles (one source per shader body; nothing copied by hand). glslang then compiles it for Vulkan with its relaxed
# rules (-R: the loose uniforms go into a uniform block whose layout the app reads from the SPIR-V itself, render/vk_draw2d.cpp).
#   cmake -DSRC=<shaders.h> -DNAME=<kName> -DOUT=<file> -P extract_glsl.cmake
file(READ ${SRC} text)
set(head "static const char* const ${NAME} = R\"(")
string(FIND "${text}" "${head}" at)
if(at LESS 0)
  message(FATAL_ERROR "extract_glsl: ${NAME} not found in ${SRC}")
endif()
string(LENGTH "${head}" headLen)
math(EXPR from "${at} + ${headLen}")
string(SUBSTRING "${text}" ${from} -1 rest)
string(FIND "${rest}" ")\";" stop)
if(stop LESS 0)
  message(FATAL_ERROR "extract_glsl: the end of ${NAME} not found")
endif()
string(SUBSTRING "${rest}" 0 ${stop} body)
string(REPLACE "\r" "" body "${body}")
file(WRITE ${OUT} "${body}")
