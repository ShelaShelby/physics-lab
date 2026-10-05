cmake_minimum_required(VERSION 3.24)
project(PhysicsLabNative VERSION 0.14 LANGUAGES C CXX)

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
option(PL_GL_TESTS "Also build gl_compare_tests: draws pictures with the real renderer on Mesa's software OpenGL (llvmpipe, through EGL) and compares them (Linux, needs libEGL)" OFF)

if(MSVC)
  add_compile_options(/utf-8 /W3)           # the source has UTF-8 text (°, ☉, ×, ...)
  # one static C runtime, so the .exe runs on any Windows without extra DLLs
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ---- the physics: no graphics, used by both the app and the tests ----
add_library(orbit_core STATIC
  src/orbit/solar_data.cpp
  src/orbit/kepler.cpp
  src/orbit/nbody.cpp
  src/orbit/moon_elp.cpp
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

# Atom Lab physics (uses the number formats and the random generator of orbit_core)
add_library(atom_core STATIC
  src/atom/atom_core.cpp
  src/atom/atom_sim.cpp
  src/atom/atom_learn.cpp
  src/atom/atom_bench.cpp
  src/atom/cloud_dots.cpp
  src/atom/outline.cpp
  src/atom/radial_graph.cpp
  src/atom/asd.cpp
  src/atom/dirac.cpp
  src/atom/lifetime.cpp
  src/atom/radii.cpp
  src/atom/quadrupole.cpp
  src/atom/nubase.cpp
  src/atom/ame.cpp
  src/atom/chart.cpp
  src/atom/chart_view.cpp
  src/atom/chart_mesh.cpp
  src/atom/decay_chain.cpp
)
target_link_libraries(atom_core PUBLIC orbit_core)
target_compile_options(atom_core PRIVATE ${PL_WARNINGS})

# App-wide settings, themes and achievements (the saved settings file; no graphics)
add_library(app_core STATIC
  src/app/prefs.cpp
  src/app/theme_style.cpp
  src/app/shot.cpp
  src/app/fps.cpp
  src/app/bench.cpp
  src/app/gpu_timer.cpp
)
target_include_directories(app_core PUBLIC src)
target_compile_options(app_core PRIVATE ${PL_WARNINGS})

# ---- tests ----
if(PL_BUILD_TESTS)
  enable_testing()
  add_executable(orbit_tests tests/test_orbit.cpp)
  target_include_directories(orbit_tests PRIVATE tests)
  target_link_libraries(orbit_tests PRIVATE orbit_core)
  target_compile_options(orbit_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME orbit_physics COMMAND orbit_tests)
  add_executable(atom_tests tests/test_atom.cpp)
  target_include_directories(atom_tests PRIVATE tests)
  target_link_libraries(atom_tests PRIVATE atom_core)
  target_compile_options(atom_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME atom_physics COMMAND atom_tests)
  add_executable(app_tests tests/test_app.cpp)
  target_include_directories(app_tests PRIVATE tests)
  target_link_libraries(app_tests PRIVATE app_core atom_core)
  target_compile_options(app_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME app_settings COMMAND app_tests)
  add_executable(nbody_tests tests/test_nbody.cpp)
  target_link_libraries(nbody_tests PRIVATE orbit_core)
  target_compile_options(nbody_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME nbody_physics COMMAND nbody_tests)
  add_executable(jpl_tests tests/test_jpl.cpp)
  target_include_directories(jpl_tests PRIVATE tests)
  target_link_libraries(jpl_tests PRIVATE orbit_core)
  target_compile_options(jpl_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME jpl_validation COMMAND jpl_tests)
  add_executable(moon_tests tests/test_moon.cpp)
  target_include_directories(moon_tests PRIVATE tests)
  target_link_libraries(moon_tests PRIVATE orbit_core)
  target_compile_options(moon_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME moon_validation COMMAND moon_tests)
  add_executable(moon_real_tests tests/test_moon_real.cpp)
  target_include_directories(moon_real_tests PRIVATE tests)
  target_link_libraries(moon_real_tests PRIVATE orbit_core)
  target_compile_options(moon_real_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME moon_real_validation COMMAND moon_real_tests)
  # every shader compiled as GLSL 330 core by glslang (skipped, not failed, if glslang is not installed; PL_REQUIRE_GLSLANG=1 makes that a failure)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_FOUND)
    add_test(NAME glsl_shaders COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_shaders.py ${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders.h)
    set_tests_properties(glsl_shaders PROPERTIES SKIP_RETURN_CODE 77)
  endif()
  # 0.14 stage 1.1: every Dear ImGui window and pop-up in the app is painted by the theme (a source scan; needs python3)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_FOUND)
    add_test(NAME windows_decorated COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_windows_decorated.py ${CMAKE_CURRENT_SOURCE_DIR}/src)
  endif()
  # 0.13 stage 2: where the CPU time of a frame goes (a measuring tool, not a test): cmake -DPL_TIMING=ON, then run stage2_timing
  option(PL_TIMING "Build stage2_timing, the per-frame CPU time breakdown of the jump and superposition scenes" OFF)
  if(PL_TIMING)
    add_executable(stage2_timing tests/timing_stage2.cpp)
    target_link_libraries(stage2_timing PRIVATE atom_core)
    add_executable(stage3_timing tests/timing_stage3.cpp)
    target_link_libraries(stage3_timing PRIVATE atom_core)
    add_executable(stage4_timing tests/timing_stage4.cpp src/atom/chart_draw.cpp src/render/renderer2d.cpp src/render/earth_texture.cpp src/render/group_compute.cpp src/render/outline_compute.cpp src/render/rock_compute.cpp src/orbit/belt_layer.cpp)
    target_include_directories(stage4_timing PRIVATE src/render)
    target_link_libraries(stage4_timing PRIVATE atom_core)
  endif()
  if(PL_GL_TESTS)
    find_library(PL_EGL_LIBRARY EGL REQUIRED)
    add_executable(gl_compare_tests tests/gl_compare.cpp src/atom/chart_draw.cpp src/render/renderer2d.cpp src/render/earth_texture.cpp src/render/group_compute.cpp src/render/outline_compute.cpp src/render/rock_compute.cpp src/orbit/belt_layer.cpp)
    target_include_directories(gl_compare_tests PRIVATE src/render)
    target_link_libraries(gl_compare_tests PRIVATE atom_core ${PL_EGL_LIBRARY})
    target_compile_options(gl_compare_tests PRIVATE ${PL_WARNINGS})
    add_test(NAME gl_phase_colours COMMAND gl_compare_tests ${CMAKE_BINARY_DIR}/gl_pictures)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/gl_pictures)
  endif()
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

  # the free fonts (assets/fonts, OFL) built into the .exe
  file(GLOB PL_FONT_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/assets/fonts/*.ttf)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/embedded_fonts.cpp
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_SOURCE_DIR}/assets/fonts -DOUT=${CMAKE_BINARY_DIR}/embedded_fonts.cpp
            -P ${CMAKE_SOURCE_DIR}/cmake/embed_fonts.cmake
    DEPENDS ${PL_FONT_FILES} ${CMAKE_SOURCE_DIR}/cmake/embed_fonts.cmake
    COMMENT "Embedding fonts")

  add_executable(PhysicsLab WIN32
    ${CMAKE_BINARY_DIR}/embedded_fonts.cpp
    src/main.cpp
    src/render/gl_loader.cpp
    src/render/renderer2d.cpp
    src/render/earth_texture.cpp
    src/render/group_compute.cpp
    src/render/outline_compute.cpp
    src/render/rock_compute.cpp
    src/orbit/belt_layer.cpp
    src/orbit/orbit_view.cpp
    src/atom/atom_view.cpp
    src/atom/chart_draw.cpp
    src/ui/orbit_panel.cpp
    src/ui/app_pages.cpp
    src/ui/app_tabs.cpp
    src/ui/atom_panel.cpp
    src/ui/atom_learn.cpp
    src/ui/html_view.cpp
    src/ui/changelog.cpp
    src/ui/fonts.cpp
    src/ui/journey_dates.cpp
    src/ui/learn.cpp
    src/ui/look.cpp
    src/ui/style_draw.cpp
    src/ui/rich_text.cpp
    src/ui/svg_icon.cpp
    src/ui/settings.cpp
    src/ui/shot.cpp
    src/ui/fps_counter.cpp
    src/ui/benchmark.cpp)
  target_link_libraries(PhysicsLab PRIVATE orbit_core atom_core app_core imgui SDL3::SDL3-static)
  target_compile_options(PhysicsLab PRIVATE ${PL_WARNINGS})
  if(WIN32)
    target_link_libraries(PhysicsLab PRIVATE opengl32)
  endif()
  if(WIN32)
    enable_language(RC)                                  # the icon of PhysicsLab.exe
    target_sources(PhysicsLab PRIVATE assets/physics_lab.rc)
  endif()
  target_compile_definitions(PhysicsLab PRIVATE PL_VERSION="${PROJECT_VERSION}")
  # 0.14 stage 1: pictures of the real app's main screens for every theme, drawn headless (SDL's offscreen driver, Mesa llvmpipe), compared with the pictures of the
  # 0.13 code. Linux only (the cloud session); the GitHub Windows build does not run it. Needs -DPL_BUILD_APP=ON -DPL_PICTURE_TESTS=ON.
  option(PL_PICTURE_TESTS "Register the theme picture test (needs a headless Mesa and the app built on Linux)" OFF)
  if(PL_PICTURE_TESTS AND UNIX)
    enable_testing()
    add_test(NAME theme_pictures COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_theme_pictures.sh $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/theme_pictures
             ${CMAKE_CURRENT_SOURCE_DIR}/tests/reference ${CMAKE_CURRENT_SOURCE_DIR}/tools)
    # 0.14 stage 1.1: a window that holds text hides what is behind it (Aero's guide showed the Learn cards through its text); needs python3
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
      add_test(NAME window_pictures COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_window_pictures.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/window_pictures)
    endif()
  endif()
endif()
