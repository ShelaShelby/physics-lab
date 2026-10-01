cmake_minimum_required(VERSION 3.24)
project(PhysicsLabNative VERSION 0.3 LANGUAGES C CXX)

# Physics Lab, native Windows version (C++17, SDL3, OpenGL 3.3 core, Dear ImGui).
# The dependencies are downloaded at fixed versions by FetchContent, so every build
# uses exactly the same code.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(PL_BUILD_APP "Build the Physics Lab app (needs SDL3 and Dear ImGui)" ON)
option(PL_BUILD_TESTS "Build the physics unit tests" ON)

if(MSVC)
  add_compile_options(/utf-8 /W3)           # the source has UTF-8 text (°, ☉, ×, ...)
  # one static C runtime, so the .exe runs on any Windows without extra DLLs
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ---- the physics: no graphics, used by both the app and the tests ----
add_library(orbit_core STATIC
  src/orbit/solar_data.cpp
  src/orbit/kepler.cpp
  src/orbit/solar_system.cpp
  src/orbit/format.cpp
  src/orbit/card_text.cpp
  src/orbit/belts.cpp
  src/orbit/sky.cpp
  src/orbit/spin.cpp
)
target_include_directories(orbit_core PUBLIC src)
if(NOT MSVC)
  set(PL_WARNINGS -Wall -Wextra)
endif()
target_compile_options(orbit_core PRIVATE ${PL_WARNINGS})

# ---- tests ----
if(PL_BUILD_TESTS)
  enable_testing()
  add_executable(orbit_tests tests/test_orbit.cpp)
  target_include_directories(orbit_tests PRIVATE tests)
  target_link_libraries(orbit_tests PRIVATE orbit_core)
  target_compile_options(orbit_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME orbit_physics COMMAND orbit_tests)
endif()

# ---- the app ----
if(PL_BUILD_APP)
  include(FetchContent)
  set(FETCHCONTENT_QUIET OFF)

  # SDL3: window, OpenGL context, mouse and keyboard
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.16
    GIT_SHALLOW TRUE)
  # Dear ImGui: panels, settings and text
  FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9b
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(SDL3 imgui)

  add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)
  target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_compile_definitions(imgui PUBLIC IMGUI_USE_WCHAR32)   # characters beyond U+FFFF (e.g. 🌍 in the Learn headings)
  target_link_libraries(imgui PUBLIC SDL3::SDL3-static)

  add_executable(PhysicsLab WIN32
    src/main.cpp
    src/render/gl_loader.cpp
    src/render/renderer2d.cpp
    src/render/earth_texture.cpp
    src/orbit/orbit_view.cpp
    src/ui/orbit_panel.cpp
    src/ui/changelog.cpp
    src/ui/fonts.cpp
    src/ui/journey_dates.cpp
    src/ui/learn.cpp
    src/ui/rich_text.cpp
    src/ui/settings.cpp)
  target_link_libraries(PhysicsLab PRIVATE orbit_core imgui SDL3::SDL3-static)
  target_compile_options(PhysicsLab PRIVATE ${PL_WARNINGS})
  if(WIN32)
    target_link_libraries(PhysicsLab PRIVATE opengl32)
  endif()
  target_compile_definitions(PhysicsLab PRIVATE PL_VERSION="${PROJECT_VERSION}")
endif()
