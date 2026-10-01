// A minimal OpenGL 3.3 core loader: just the functions this app uses, fetched
// through SDL at start-up. (Dear ImGui's OpenGL backend has its own loader.)
#pragma once

#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#define PL_GLAPI __stdcall
#else
#define PL_GLAPI
#endif

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef char GLchar;
typedef std::ptrdiff_t GLsizeiptr;
typedef std::ptrdiff_t GLintptr;

#define GL_FALSE 0
#define GL_TRUE 1
#define GL_TRIANGLES 0x0004
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ONE 1
#define GL_FLOAT 0x1406
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_ARRAY_BUFFER 0x8892
#define GL_STREAM_DRAW 0x88E0
#define GL_STATIC_DRAW 0x88E4
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_DEPTH_TEST 0x0B71
#define GL_CULL_FACE 0x0B44
#define GL_SCISSOR_TEST 0x0C11
#define GL_MULTISAMPLE 0x809D
#define GL_VERSION 0x1F02
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_LINEAR 0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_UNSIGNED_BYTE 0x1401
#define GL_RENDERER 0x1F01
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0

#define PL_GL_FUNCTIONS(X)                                                                         \
    X(void, glClear, (GLbitfield mask))                                                            \
    X(void, glClearColor, (GLfloat r, GLfloat g, GLfloat b, GLfloat a))                            \
    X(void, glViewport, (GLint x, GLint y, GLsizei w, GLsizei h))                                  \
    X(void, glEnable, (GLenum cap))                                                                \
    X(void, glDisable, (GLenum cap))                                                               \
    X(void, glBlendFuncSeparate, (GLenum sRGB, GLenum dRGB, GLenum sA, GLenum dA))                 \
    X(void, glDrawArrays, (GLenum mode, GLint first, GLsizei count))                               \
    X(const unsigned char*, glGetString, (GLenum name))                                           \
    X(GLuint, glCreateShader, (GLenum type))                                                       \
    X(void, glShaderSource, (GLuint s, GLsizei n, const GLchar* const* src, const GLint* len))    \
    X(void, glCompileShader, (GLuint s))                                                           \
    X(void, glGetShaderiv, (GLuint s, GLenum p, GLint* v))                                         \
    X(void, glGetShaderInfoLog, (GLuint s, GLsizei n, GLsizei* len, GLchar* log))                 \
    X(void, glDeleteShader, (GLuint s))                                                            \
    X(GLuint, glCreateProgram, (void))                                                             \
    X(void, glAttachShader, (GLuint p, GLuint s))                                                  \
    X(void, glLinkProgram, (GLuint p))                                                             \
    X(void, glGetProgramiv, (GLuint p, GLenum q, GLint* v))                                        \
    X(void, glGetProgramInfoLog, (GLuint p, GLsizei n, GLsizei* len, GLchar* log))                \
    X(void, glDeleteProgram, (GLuint p))                                                           \
    X(void, glUseProgram, (GLuint p))                                                              \
    X(GLint, glGetUniformLocation, (GLuint p, const GLchar* name))                                 \
    X(void, glUniform2f, (GLint loc, GLfloat a, GLfloat b))                                        \
    X(void, glGenVertexArrays, (GLsizei n, GLuint* a))                                             \
    X(void, glDeleteVertexArrays, (GLsizei n, const GLuint* a))                                    \
    X(void, glBindVertexArray, (GLuint a))                                                         \
    X(void, glGenBuffers, (GLsizei n, GLuint* b))                                                  \
    X(void, glDeleteBuffers, (GLsizei n, const GLuint* b))                                         \
    X(void, glBindBuffer, (GLenum t, GLuint b))                                                    \
    X(void, glBufferData, (GLenum t, GLsizeiptr size, const void* data, GLenum usage))            \
    X(void, glVertexAttribPointer, (GLuint i, GLint n, GLenum t, GLboolean norm, GLsizei stride, const void* p)) \
    X(void, glEnableVertexAttribArray, (GLuint i))                                                 \
    X(void, glDrawArraysInstanced, (GLenum mode, GLint first, GLsizei count, GLsizei instances))  \
    X(void, glVertexAttribDivisor, (GLuint index, GLuint divisor))                                 \
    X(void, glUniform4f, (GLint loc, GLfloat a, GLfloat b, GLfloat c, GLfloat d))                  \
    X(void, glUniform1f, (GLint loc, GLfloat a))                                                   \
    X(void, glBufferSubData, (GLenum t, GLintptr offset, GLsizeiptr size, const void* data))      \
    X(void, glGenTextures, (GLsizei n, GLuint* t))                                                 \
    X(void, glDeleteTextures, (GLsizei n, const GLuint* t))                                        \
    X(void, glBindTexture, (GLenum target, GLuint t))                                              \
    X(void, glActiveTexture, (GLenum unit))                                                        \
    X(void, glTexImage2D, (GLenum t, GLint lvl, GLint ifmt, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum ty, const void* d)) \
    X(void, glTexParameteri, (GLenum t, GLenum p, GLint v))                                        \
    X(void, glGenerateMipmap, (GLenum t))                                                          \
    X(void, glGenFramebuffers, (GLsizei n, GLuint* f))                                             \
    X(void, glDeleteFramebuffers, (GLsizei n, const GLuint* f))                                    \
    X(void, glBindFramebuffer, (GLenum t, GLuint f))                                               \
    X(void, glFramebufferTexture2D, (GLenum t, GLenum att, GLenum tt, GLuint tex, GLint lvl))      \
    X(void, glBlendFunc, (GLenum s, GLenum d))                                                     \
    X(void, glUniform1fv, (GLint loc, GLsizei n, const GLfloat* v))                                \
    X(void, glPixelStorei, (GLenum p, GLint v))                                                    \
    X(void, glUniform1i, (GLint loc, GLint v))                                                     \
    X(void, glUniformMatrix3fv, (GLint loc, GLsizei n, GLboolean tr, const GLfloat* v))           \
    X(void, glReadPixels, (GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, void* data))

// The function pointers live in their own namespace, so they never clash with the
// system's OpenGL library (which exports functions with the same names).
namespace plgl {
#define PL_GL_DECLARE(ret, name, args) typedef ret(PL_GLAPI* PFN_##name) args; extern PFN_##name name;
PL_GL_FUNCTIONS(PL_GL_DECLARE)
#undef PL_GL_DECLARE

// Loads every function above. Returns false (and the name in `missing`) if one is not there.
bool loadGL(const char** missing);
}  // namespace plgl
