cmake_minimum_required(VERSION 3.24)
project(PhysicsLabNative VERSION 0.55.1 LANGUAGES C CXX)

# Physics Lab, native Windows version (C++17, SDL3, OpenGL 3.3 core, Dear ImGui).
# The dependencies are downloaded at fixed versions by FetchContent, so every build
# uses exactly the same code.

# 0.15 stage 4.1: compile through ccache when it is installed (apt install ccache): a rebuild after reverting a change, or after a clean, hits the cache. No effect without it, and not for Visual Studio.
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM AND NOT MSVC AND NOT CMAKE_GENERATOR MATCHES "Visual Studio")
  set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
  set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

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
  src/orbit/saturn_profile.cpp
  src/orbit/nbody.cpp
  src/orbit/moon_elp.cpp
  src/orbit/moon_sats.cpp
  src/orbit/solar_system.cpp
  src/orbit/time_scale.cpp
  src/orbit/format.cpp
  src/orbit/card_text.cpp
  src/orbit/subject_card.cpp
  src/orbit/belts.cpp
  src/orbit/sky.cpp
  src/orbit/spin.cpp
)
target_include_directories(orbit_core PUBLIC src)
find_package(Threads REQUIRED)                       # 0.29: the Real planets' clock makes the next stretch on a background thread (std::async)
target_link_libraries(orbit_core PUBLIC Threads::Threads)
if(NOT MSVC)
  set(PL_WARNINGS -Wall -Wextra)
endif()
target_compile_options(orbit_core PRIVATE ${PL_WARNINGS})

# Atom Lab physics (uses the number formats and the random generator of orbit_core)
# 0.41: the many-electron atoms' Dirac-Hartree-Fock orbitals (data/dhf/dhf.bin, tools/dhf/make_dhf_data.py), built into the program as the molecule densities are
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/embedded_dhf.cpp
  COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/data/dhf -DOUT=${CMAKE_BINARY_DIR}/embedded_dhf.cpp
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_dhf.cmake
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/data/dhf/dhf.bin ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_dhf.cmake
  COMMENT "Embedding the Dirac-Hartree-Fock atoms")
# 0.47.2: the GRASP2018 clouds of Li, Na, K, Rb, Cs, Fr (data/jumps/grasp/<X>/clouds.bin, tools/grasp); 0.47.3: and of the level atoms (levels.h) that have them
file(GLOB PL_GRASP_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/data/jumps/grasp/*/clouds.bin)
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/embedded_grasp.cpp
  COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/data/jumps/grasp -DOUT=${CMAKE_BINARY_DIR}/embedded_grasp.cpp
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_grasp.cmake
  DEPENDS ${PL_GRASP_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_grasp.cmake
  COMMENT "Embedding the GRASP2018 clouds")
add_library(atom_core STATIC
  src/atom/atom_core.cpp
  src/atom/atom_sim.cpp
  src/atom/atom_learn.cpp
  src/atom/atom_tools.cpp
  src/atom/atom_bench.cpp
  src/atom/cloud_dots.cpp
  src/atom/cloud_sample.cpp
  src/atom/hf.cpp
  src/atom/alkali.cpp
  src/atom/levels.cpp
  src/atom/outline.cpp
  src/atom/radial_graph.cpp
  src/atom/asd.cpp
  src/atom/dirac.cpp
  src/atom/threej.cpp
  src/atom/zeeman.cpp
  src/atom/dirac_cloud.cpp
  src/atom/hfs.cpp
  src/atom/lifetime.cpp
  src/atom/radii.cpp
  src/atom/quadrupole.cpp
  src/atom/nubase.cpp
  src/atom/ame.cpp
  src/atom/chart.cpp
  src/atom/chart_view.cpp
  src/atom/chart_mesh.cpp
  src/atom/decay_chain.cpp
  ${CMAKE_BINARY_DIR}/embedded_dhf.cpp
  ${CMAKE_BINARY_DIR}/embedded_grasp.cpp
)
target_link_libraries(atom_core PUBLIC orbit_core)
target_compile_options(atom_core PRIVATE ${PL_WARNINGS})

# App-wide settings and themes (the saved settings file; no graphics)
add_library(app_core STATIC
  src/app/prefs.cpp
  src/app/theme_style.cpp
  src/app/look_options.cpp
  src/app/html_entities.cpp
  src/app/display_scale.cpp
  src/app/power.cpp
  src/app/shell.cpp
  src/app/shot.cpp
  src/app/fps.cpp
  src/app/bench.cpp
  src/app/bench_scenes.cpp
  src/app/gpu_timer.cpp
  src/app/calc.cpp
  src/app/calc_lab.cpp
)
target_include_directories(app_core PUBLIC src)
target_link_libraries(app_core PUBLIC atom_core)   # 0.16 stage 4: bench_scenes.cpp reads the Atom Lab scene texts
target_compile_options(app_core PRIVATE ${PL_WARNINGS})

# Molecule Lab (0.18 stage 1): 15 small molecules in their measured (CCCBDB) geometry, the camera, picking and the settings text (no graphics)
# 0.21 stage 1: the Realistic style's computed density data (data/molecules/density/*.pld, tools/make_density_data.py), built into the program as the fonts are
file(GLOB PL_DENSITY_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/data/molecules/density/*.pld)
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/embedded_density.cpp
  COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/data/molecules/density -DOUT=${CMAKE_BINARY_DIR}/embedded_density.cpp
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_density.cmake
  DEPENDS ${PL_DENSITY_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_density.cmake
  COMMENT "Embedding the molecule density data")
add_library(molecule_core STATIC
  src/molecule/molecule.cpp
  src/molecule/density.cpp
  src/molecule/vibration.cpp
  src/molecule/rotation.cpp
  src/molecule/morse.cpp
  ${CMAKE_BINARY_DIR}/embedded_density.cpp
)
target_include_directories(molecule_core PUBLIC src)
target_link_libraries(molecule_core PUBLIC atom_core)   # the CODATA constants (h, c, u) in atom/atom_core.h
target_compile_options(molecule_core PRIVATE ${PL_WARNINGS})

# 0.51: the Data panel's logic: tables of the labs' data, curve fitting, export and import of text (no graphics)
add_library(analysis_core STATIC
  src/analysis/fit.cpp
  src/analysis/table.cpp
  src/analysis/lab_tables.cpp
)
target_include_directories(analysis_core PUBLIC src)
target_link_libraries(analysis_core PUBLIC app_core molecule_core atom_core orbit_core)
target_compile_options(analysis_core PRIVATE ${PL_WARNINGS})

# 0.54.3: the Black Hole Lab's physics (no graphics): special radii, the ray integrator, the non-spinning hole's closed forms, the bent sky, the presets
add_library(blackhole_core STATIC
  src/blackhole/elliptic.cpp
  src/blackhole/kerr.cpp
  src/blackhole/geodesic.cpp
  src/blackhole/schwarzschild.cpp
  src/blackhole/presets.cpp
  src/blackhole/lab.cpp
  src/blackhole/disc.cpp
)
target_include_directories(blackhole_core PUBLIC src)
target_link_libraries(blackhole_core PUBLIC orbit_core)   # the night sky's projection (orbit/sky.h skyDirection) and the constants of orbit/units.h
target_compile_options(blackhole_core PRIVATE ${PL_WARNINGS})

# 0.26: the picture maps (assets/maps/*.jpg: planet surfaces and the Gaia sky, sources in assets/maps/README.md), built into the program as the fonts are,
# and read with stb_image (third_party/stb_image.h, v2.30, public domain / MIT)
file(GLOB PL_MAP_FILES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/maps/*.jpg)
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/embedded_maps.cpp
  COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/assets/maps -DOUT=${CMAKE_BINARY_DIR}/embedded_maps.cpp
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_maps.cmake
  DEPENDS ${PL_MAP_FILES} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_maps.cmake
  COMMENT "Embedding the picture maps")
add_library(maps_core STATIC src/render/planet_maps.cpp ${CMAKE_BINARY_DIR}/embedded_maps.cpp)
target_include_directories(maps_core PUBLIC src)
target_include_directories(maps_core SYSTEM PRIVATE third_party)

# ---- tests ----
if(PL_BUILD_TESTS)
  enable_testing()
  add_executable(orbit_tests tests/test_orbit.cpp)
  target_include_directories(orbit_tests PRIVATE tests)
  target_link_libraries(orbit_tests PRIVATE orbit_core maps_core)
  target_compile_definitions(orbit_tests PRIVATE PL_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")   # 0.50: the saved CIE 1931 table and other sources, read as received
  target_compile_options(orbit_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME orbit_physics COMMAND orbit_tests)
  add_executable(atom_tests tests/test_atom.cpp)
  target_include_directories(atom_tests PRIVATE tests)
  target_link_libraries(atom_tests PRIVATE atom_core)
  target_compile_definitions(atom_tests PRIVATE PL_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")   # 0.41: the saved Visscher-Dyall tables (data/dhf) are read as published
  target_compile_options(atom_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME atom_physics COMMAND atom_tests)
  add_executable(molecule_tests tests/test_molecule.cpp)
  target_link_libraries(molecule_tests PRIVATE molecule_core)
  target_compile_definitions(molecule_tests PRIVATE PL_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")   # 0.50: the saved CCCBDB geometry pages are read as received
  target_compile_options(molecule_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME molecule_logic COMMAND molecule_tests)
  add_executable(vibration_tests tests/test_vibration.cpp)   # 0.23: the vibrations (counts, conservation laws, selection rules, the zero-point swing)
  target_link_libraries(vibration_tests PRIVATE molecule_core)
  target_compile_options(vibration_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME vibration_logic COMMAND vibration_tests)
  add_executable(rotation_tests tests/test_rotation.cpp)   # 0.53: rotation of the linear molecules (levels, weights, populations, lines, isotopes; the saved pages read as received)
  target_link_libraries(rotation_tests PRIVATE molecule_core)
  target_compile_definitions(rotation_tests PRIVATE PL_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
  target_compile_options(rotation_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME rotation_logic COMMAND rotation_tests)
  add_executable(blackhole_tests tests/test_blackhole.cpp)   # 0.54.3: the Black Hole Lab's physics against the saved papers (data/blackhole)
  target_include_directories(blackhole_tests PRIVATE tests)
  target_include_directories(blackhole_tests SYSTEM PRIVATE third_party)   # stb_image's zlib reader, for the saved papers' .txt.gz
  target_link_libraries(blackhole_tests PRIVATE blackhole_core)
  target_compile_definitions(blackhole_tests PRIVATE PL_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data" PL_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src")
  target_compile_options(blackhole_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME blackhole_physics COMMAND blackhole_tests)
  # 0.54.3: the reference picture (docs/blackhole) and the speed measurements; a tool run by hand, not a test and not part of the app
  add_executable(blackhole_picture tests/blackhole_picture.cpp)
  target_link_libraries(blackhole_picture PRIVATE blackhole_core maps_core app_core)
  target_compile_options(blackhole_picture PRIVATE ${PL_WARNINGS})
  add_executable(density_tests tests/test_density.cpp)   # 0.21: the Realistic style (electron counts, dipoles, symmetry, dots, the surface)
  target_link_libraries(density_tests PRIVATE molecule_core)
  target_compile_options(density_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME density_logic COMMAND density_tests)
  add_executable(molecule_xyz tools/molxyz/export_xyz.cpp)   # 0.21: the viewer geometry for tools/make_density_data.py (data/molecules/xyz)
  target_link_libraries(molecule_xyz PRIVATE molecule_core)
  add_executable(app_tests tests/test_app.cpp)
  target_include_directories(app_tests PRIVATE tests)
  target_link_libraries(app_tests PRIVATE app_core atom_core blackhole_core)   # (0.55.0: the About page's Black Hole Lab numbers against presets.h)
  target_compile_options(app_tests PRIVATE ${PL_WARNINGS})
  target_compile_definitions(app_tests PRIVATE PL_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")   # 0.14 stage 2: the bundled font and its licence are checked
  add_test(NAME app_settings COMMAND app_tests)
  add_executable(calc_tests tests/test_calc.cpp)   # 0.49: the calculator (rules, units, constants against CODATA 2022, physics against independent values, fuzz)
  target_link_libraries(calc_tests PRIVATE app_core atom_core)
  target_compile_options(calc_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME calc_logic COMMAND calc_tests)
  add_executable(analysis_tests tests/test_analysis.cpp)   # 0.51: fitting against the NIST StRD certified values, the lab tables against independent physics
  target_link_libraries(analysis_tests PRIVATE analysis_core)
  target_compile_definitions(analysis_tests PRIVATE PL_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
  target_compile_options(analysis_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME analysis_logic COMMAND analysis_tests)
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
    add_executable(stage4_timing tests/timing_stage4.cpp src/atom/chart_draw.cpp src/render/renderer2d.cpp src/render/group_compute.cpp src/render/outline_compute.cpp src/render/rock_compute.cpp src/orbit/belt_layer.cpp)
    target_include_directories(stage4_timing PRIVATE src/render)
    target_link_libraries(stage4_timing PRIVATE atom_core maps_core)
  endif()
  if(PL_GL_TESTS)
    find_library(PL_EGL_LIBRARY EGL REQUIRED)
    add_executable(gl_compare_tests tests/gl_compare.cpp src/atom/chart_draw.cpp src/render/renderer2d.cpp src/render/group_compute.cpp src/render/outline_compute.cpp src/render/rock_compute.cpp src/orbit/belt_layer.cpp)
    target_include_directories(gl_compare_tests PRIVATE src/render)
    target_link_libraries(gl_compare_tests PRIVATE atom_core maps_core ${PL_EGL_LIBRARY})
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
  # 0.15 stage 5j: glyph coverage of the look fonts, read from the font files with stb_truetype (no window, no app start)
  add_executable(font_tests tests/test_fonts.cpp)
  target_include_directories(font_tests SYSTEM PRIVATE ${imgui_SOURCE_DIR})
  target_include_directories(font_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_definitions(font_tests PRIVATE PL_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets" PL_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src")
  add_test(NAME font_coverage COMMAND font_tests)

  # the free fonts (assets/fonts, OFL) built into the .exe
  file(GLOB PL_FONT_FILES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/assets/fonts/*.ttf ${CMAKE_SOURCE_DIR}/assets/fonts/*.otf)
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
   
    src/render/group_compute.cpp
    src/render/outline_compute.cpp
    src/render/rock_compute.cpp
    src/orbit/belt_layer.cpp
    src/orbit/orbit_view.cpp
    src/atom/atom_view.cpp
    src/atom/chart_draw.cpp
    src/molecule/molecule_view.cpp
    src/ui/shell_molecule.cpp
    src/ui/realest.cpp
    src/ui/molecule_learn.cpp
    src/blackhole/blackhole_view.cpp
    src/ui/shell_blackhole.cpp
    src/ui/blackhole_learn.cpp
    src/ui/orbit_panel.cpp
    src/ui/orbit_actions.cpp
    src/ui/shell.cpp
    src/ui/calc_panel.cpp
    src/ui/data_panel.cpp
    src/ui/shell_widgets.cpp
    src/ui/shell_orbit.cpp
    src/ui/shell_atom.cpp
    src/ui/shell_test.cpp
    src/ui/reading.cpp
    src/ui/input_log.cpp
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
    src/ui/benchmark.cpp
    src/ui/power.cpp)
  target_link_libraries(PhysicsLab PRIVATE analysis_core orbit_core atom_core app_core molecule_core blackhole_core maps_core imgui SDL3::SDL3-static)
  target_compile_options(PhysicsLab PRIVATE ${PL_WARNINGS})
  if(WIN32)
    target_link_libraries(PhysicsLab PRIVATE opengl32)
  endif()
  if(WIN32)
    enable_language(RC)                                  # the icon of PhysicsLab.exe
    target_sources(PhysicsLab PRIVATE assets/physics_lab.rc)
  endif()
  target_compile_definitions(PhysicsLab PRIVATE PL_VERSION="${PROJECT_VERSION}")
  # 0.16 stage 1b: theme_pictures and window_pictures (pictures of the OLD interface) were deleted with it. The New UI's picture tests are below. Linux only (the cloud session); the GitHub Windows build does not run them. Needs -DPL_BUILD_APP=ON -DPL_PICTURE_TESTS=ON.
  option(PL_PICTURE_TESTS "Register the theme picture test (needs a headless Mesa and the app built on Linux)" OFF)
  if(PL_PICTURE_TESTS AND UNIX)
    enable_testing()
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
      # 0.38.1 stage 3 (the owner: "merge them"): ONE test per screen. tests/run_group.py runs the check scripts of the screen one after the other, each exactly as it ran as
      # its own test before (99 tests became 15); a failure prints the name of its check. To run one check by hand: python3 tests/<script> <PhysicsLab> <folder> [part].
      # ui_shell: the shell: text sizes and floors, windows, keys, start view, date entry, panel toggles
      add_test(NAME ui_shell COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_shell
               check_shell.py check_shell_state.py check_022.py:keys check_022.py:start check_022s7.py:dateenter check_5k.py:toggle check_5k.py:calc check_5k.py:matrix check_5k.py:data)
      set_tests_properties(ui_shell PROPERTIES TIMEOUT 3600)
      # ui_controls: the Controls panel: layout, sections, sliders, End key, its scrollbar
      add_test(NAME ui_controls COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_controls
               check_controls.py check_5g.py:panel check_5q.py:sliders check_5q.py:endkey check_5q.py:scrollbar)
      set_tests_properties(ui_controls PROPERTIES TIMEOUT 3600)
      # ui_orbit_view: the Orbit picture: wheel zoom, scrolling, zoom out, centre, axes, orbit paths, scale bar
      add_test(NAME ui_orbit_view COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_orbit_view
               check_5p.py:wheel check_5p.py:scroll check_5p.py:zoomout check_022.py:centre check_022.py:axes check_023.py:orbits
               check_022s7.py:scalebar)
      set_tests_properties(ui_orbit_view PROPERTIES TIMEOUT 3600)
      # ui_look: the look: chosen options, ticks, Settings tiles, solid pages, themes, lit buttons, pills, glass, layout tokens
      add_test(NAME ui_look COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_look
               check_5t.py:grey check_5t.py:tiles check_5t.py:solid check_5m.py:themes check_5q.py:theme check_5q.py:lit
               check_5p.py:lit check_5r.py:pill check_shell_pictures.py check_5m2.py:layout check_5m2.py:tilt)
      set_tests_properties(ui_look PROPERTIES TIMEOUT 3600)
      # ui_popups: windows and pop-ups: Settings, legend, pickers, menus, Benchmark
      add_test(NAME ui_popups COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_popups
               check_5m2.py:windows check_5m2.py:popups check_5l.py:popups check_5l.py:legend check_5p.py:picker check_5q.py:picker
               check_5k.py:windows check_6b.py:bench)
      set_tests_properties(ui_popups PROPERTIES TIMEOUT 3600)
      # ui_pages: the reading pages: wheel and keys, scrollbar, Contents menu and rail
      add_test(NAME ui_pages COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_pages
               check_5k.py:pages check_5k.py:log check_5g.py:pages check_6b.py:scrollbar check_5k.py:contents check_5g.py:contents
               check_6b.py:rail)
      set_tests_properties(ui_pages PROPERTIES TIMEOUT 3600)
      # ui_search: search (Ctrl+K) in every lab
      add_test(NAME ui_search COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_search
               check_5l.py:search check_022s7.py:search check_audit19.py:molsearch check_orbit_ui.py:search)
      set_tests_properties(ui_search PROPERTIES TIMEOUT 3600)
      # ui_toolbar: the bottom toolbars
      add_test(NAME ui_toolbar COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_toolbar
               check_5l.py:toolbar check_5g.py:toolbar check_atom_toolbar.py)
      set_tests_properties(ui_toolbar PROPERTIES TIMEOUT 3600)
      # ui_cards: the subject cards and data sheets
      add_test(NAME ui_cards COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_cards
               check_cards.py:orbit check_cards.py:atom check_5i.py:sheet check_layout020.py:sheet check_022.py:stage6)
      set_tests_properties(ui_cards PROPERTIES TIMEOUT 3600)
      # ui_facts: the fact line
      add_test(NAME ui_facts COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_facts
               check_orbit_ui.py:facts check_orbit_ui.py:facts_long check_022.py:facts check_022.py:factsrc check_022s7.py:factmol)
      set_tests_properties(ui_facts PROPERTIES TIMEOUT 3600)
      # ui_orbit: the Orbit Lab: every control of INVENTORY.md, the data sheet words, Time panel, chip, readability, links, Most realistic; 0.50: no rim of air, a dark night side
      add_test(NAME ui_orbit COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_orbit
               check_orbit_ui.py:coverage check_orbit_ui.py:card check_orbit_ui.py:time check_orbit_ui.py:chip check_orbit_ui.py:readable check_orbit_ui.py:links check_orbit_ui.py:journey check_orbit_ui.py:farnote check_orbit_ui.py:airedge check_orbit_ui.py:nightside
               check_025.py:orbit)
      set_tests_properties(ui_orbit PROPERTIES TIMEOUT 3600)
      # ui_atom_panel: the Atom Lab Controls panel against the old panel's saved states
      add_test(NAME ui_atom_panel COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_atom_panel
               check_atom_panel.py)
      set_tests_properties(ui_atom_panel PROPERTIES TIMEOUT 3600)
      # ui_atom: the Atom Lab: element chip, its old-panel features, view words, jump check, Most realistic, Monochrome key, honesty notes, density, nucleus controls and full zoom
      add_test(NAME ui_atom COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_atom
               check_atom_chip.py check_6a.py:gaps check_022.py:words check_022s7.py:jumpcheck check_025.py:atom
               check_022.py:honest check_022.py:spectrum check_022.py:density check_6b.py:nucleus check_6c.py:fullzoom check_6c.py:ds281sea)
      set_tests_properties(ui_atom PROPERTIES TIMEOUT 3600)
      # ui_atom_layout: the Atom Lab layout: centre, info text, card tabs, fit, rings, default view, U-238 info text
      add_test(NAME ui_atom_layout COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_atom_layout
               check_5h.py:centre check_5h.py:infosize check_5h.py:cards check_5h.py:fit check_5g.py:ring check_5g.py:atomtext
               check_5g.py:default check_layout020.py:u238)
      set_tests_properties(ui_atom_layout PROPERTIES TIMEOUT 3600)
      # ui_molecule: the Molecule Lab: pictures, Realistic, its bar, vibrations, density, no builder, Most realistic, guide, pan, Space, rotation (0.53), the anharmonic bond (0.53.1)
      add_test(NAME ui_molecule COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_molecule
               check_molecule.py check_realistic.py check_audit19.py:molbar check_022.py:molbar check_023.py:vib check_024.py:densvib
               check_024.py:densfit check_024.py:nobuild check_025.py:molecule check_025.py:guide check_layout020.py:molpan check_022s7.py:space
               check_molecule.py:rotation check_molecule.py:anharm)
      set_tests_properties(ui_molecule PROPERTIES TIMEOUT 3600)
      # ui_blackhole (0.55.0): the Black Hole Lab: its tab, presets, distances against Synge, bending and sky, Most realistic, the Orbit Lab's card, settings, layout, look,
      # and the card's bent sky against the double-precision reference
      add_test(NAME ui_blackhole COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_blackhole
               check_blackhole.py:open check_blackhole.py:presets check_blackhole.py:distance check_blackhole.py:light check_blackhole.py:disc check_blackhole.py:realest check_blackhole.py:orbitcard
               check_blackhole.py:settings check_blackhole.py:layout check_blackhole.py:look check_blackhole.py:precision)
      set_tests_properties(ui_blackhole PROPERTIES TIMEOUT 3600)
    endif()
  endif()
endif()

# 0.15 stage 4.1: labels and a fast, identical environment for the tests. quick = the unit test programs, glsl and the source scan (about a minute, no app start);
# pictures = everything that starts the real app under Mesa's software OpenGL. One llvmpipe thread per app start (so `ctest -j<cores>` uses every core instead of every start fighting for
# all of them) and a shared Mesa shader cache (a start no longer rebuilds its shaders): the pictures are the same (the picture guards prove it).
foreach(t orbit_physics atom_physics blackhole_physics calc_logic analysis_logic molecule_logic vibration_logic rotation_logic density_logic app_settings font_coverage nbody_physics jpl_validation moon_validation moon_real_validation glsl_shaders windows_decorated)
  if(TEST ${t})
    set_tests_properties(${t} PROPERTIES LABELS "quick")
  endif()
endforeach()
# 0.38.1: every test that is not "quick" starts the real app, so it gets the pictures label and settings automatically (the hand-written list of 0.15 had missed the 5m2, 5p,
# 5q, 5r, 5t and 6c tests, about 25, which then ran with every llvmpipe thread each and fought over the cores)
get_property(pl_all_tests DIRECTORY PROPERTY TESTS)
foreach(t ${pl_all_tests})
  get_test_property(${t} LABELS pl_lab)
  if(NOT pl_lab STREQUAL "quick")
    set_tests_properties(${t} PROPERTIES LABELS "pictures" ENVIRONMENT "LP_NUM_THREADS=1;MESA_SHADER_CACHE_DIR=${CMAKE_BINARY_DIR}/mesa_cache")
  endif()
endforeach()
