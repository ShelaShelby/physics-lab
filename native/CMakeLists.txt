cmake_minimum_required(VERSION 3.24)
project(PhysicsLabNative VERSION 0.86.0 LANGUAGES C CXX)

# Physics Lab, native Windows version (C++17, SDL3, Vulkan, Dear ImGui).
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
# 0.65.0: on by default with the picture tests on Linux, so the card-vs-processor comparison (vk_cards) runs in the full suite
if(PL_PICTURE_TESTS AND UNIX AND NOT APPLE)
  set(PL_VK_TESTS_DEFAULT ON)
else()
  set(PL_VK_TESTS_DEFAULT OFF)
endif()
# vk_compare_tests: the GPU programs and the real renderer's pictures on Vulkan (Mesa's
# lavapipe here, no window) against the processor's references. Needs the app's dependencies (-DPL_BUILD_APP=ON).
option(PL_VK_TESTS "Also build vk_compare_tests: the card programs and the renderer's pictures on Vulkan (lavapipe on Linux) against the processor" ${PL_VK_TESTS_DEFAULT})

if(MSVC)
  add_compile_options(/utf-8 /W3)           # the source has UTF-8 text (°, ☉, ×, ...)
  # one static C runtime, so the .exe runs on any Windows without extra DLLs
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ---- the physics: no graphics, used by both the app and the tests ----
# 0.85.0: the measured sunspot groups (data/sun/spots/sunspot_groups.bin, tools/make_sunspot_table.py), built into the program as the atoms' orbitals are
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/embedded_spots.cpp
  COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/data/sun/spots -DOUT=${CMAKE_BINARY_DIR}/embedded_spots.cpp
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_spots.cmake
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/data/sun/spots/sunspot_groups.bin ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_spots.cmake
  COMMENT "Embedding the sunspot records")
add_library(orbit_core STATIC
  src/orbit/sunspots.cpp
  ${CMAKE_BINARY_DIR}/embedded_spots.cpp
  src/orbit/solar_data.cpp
  src/orbit/kepler.cpp
  src/orbit/comet_tail.cpp
  src/orbit/alphacen.cpp
  src/orbit/saturn_profile.cpp
  src/orbit/ring_bands.cpp
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
  src/atom/photon_pattern.cpp
  src/atom/atom_sim.cpp
  src/atom/atom_learn.cpp
  src/atom/atom_tools.cpp
  src/atom/atom_bench.cpp
  src/atom/cloud_dots.cpp
  src/atom/cloud_sample.cpp
  src/atom/atom_card.cpp
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
  src/app/gpu_guard.cpp
  src/app/gpu_helper.cpp
  src/app/frame_cost.cpp
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
  src/blackhole/mino.cpp
  src/blackhole/kerr_closed.cpp
  src/blackhole/gas.cpp
  src/blackhole/schwarzschild.cpp
  src/blackhole/presets.cpp
  src/blackhole/lab.cpp
  src/blackhole/bench_settings.cpp
  src/blackhole/looks.cpp
  src/blackhole/light.cpp
  src/blackhole/tone.cpp
  src/blackhole/disc.cpp
  src/blackhole/hot.cpp
  src/blackhole/spin.cpp
  src/blackhole/spin_sky.cpp
  src/blackhole/low_priority.cpp
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
  # 0.77.0: the CPU picture of a planet with its map, for check_orbit_ui.py planets (the GPU against it)
  add_executable(planet_picture tests/planet_picture.cpp)
  target_link_libraries(planet_picture PRIVATE maps_core)
  target_compile_options(planet_picture PRIVATE ${PL_WARNINGS})
  # 0.56.3: moving gas worked out on the processor (PLAN_MOVINGGAS.md; a tool run by hand, its self-checks a quick test)
  add_executable(movinggas tools/movinggas/movinggas.cpp)
  target_link_libraries(movinggas PRIVATE blackhole_core)
  target_compile_options(movinggas PRIVATE ${PL_WARNINGS})
  add_test(NAME movinggas_physics COMMAND movinggas --check)
  # 0.78.1 research: prototype pictures of a realistic gas disc (PLAN_DISC.md; not a test)
  add_executable(discproto tools/discproto/discproto.cpp)
  target_link_libraries(discproto PRIVATE blackhole_core app_core)
  target_compile_options(discproto PRIVATE ${PL_WARNINGS})
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
  endif()
endif()

# ---- the app ----
if(PL_BUILD_APP)
  include(FetchContent)
  set(FETCHCONTENT_QUIET OFF)

  # SDL3: window (Vulkan surface), mouse and keyboard
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
  # 0.59.0: Vulkan, step 1 of 5 (the Vulkan plan, done in 0.69.0 (git keeps it)). No Vulkan SDK on the build machines: the Khronos headers, volk (loads the Vulkan loader of the PC at run
  # time; 0.69.0: a PC without Vulkan gets a message naming what is needed) and glslang (the Khronos reference compiler, built here) to turn the GLSL into SPIR-V at build time.
  # One SDK release for all three (tags checked to exist on 18 Nov 2026).
  FetchContent_Declare(VulkanHeaders
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG vulkan-sdk-1.4.350.1
    GIT_SHALLOW TRUE)
  FetchContent_Declare(volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG vulkan-sdk-1.4.350.1
    GIT_SHALLOW TRUE)
  set(ENABLE_OPT OFF CACHE BOOL "" FORCE)                 # glslang: no SPIRV-Tools optimiser (not needed, saves the build)
  set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
  set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
  set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
  set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  set(ENABLE_GLSLANG_BINARIES ON CACHE BOOL "" FORCE)
  FetchContent_Declare(glslang
    GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
    GIT_TAG vulkan-sdk-1.4.350.1
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(SDL3 imgui VulkanHeaders)
  set(VULKAN_HEADERS_INSTALL_DIR ${vulkanheaders_SOURCE_DIR} CACHE PATH "" FORCE)   # volk: these headers, never an SDK's found on the machine
  FetchContent_MakeAvailable(volk glslang)

  add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp)  # 0.59.0: the same version's Vulkan backend, through volk
  target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
  target_compile_definitions(imgui PUBLIC IMGUI_USE_WCHAR32 IMGUI_IMPL_VULKAN_USE_VOLK)   # characters beyond U+FFFF (e.g. 🌍 in the Learn headings); 0.59.0: Vulkan functions from volk
  target_link_libraries(imgui PUBLIC SDL3::SDL3-static volk::volk)
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

  # 0.59.0: the Vulkan shaders, GLSL to SPIR-V at build time by glslang (built above), each into a header with a uint32_t array (vk_shaders/<name>_<stage>.h)
  set(PL_VK_SHADERS src/render/vk/fill.vert src/render/vk/fill.frag)   # (0.69.0: present.* gone: the picture's copy is a blit)
  set(PL_VK_SHADER_HEADERS)
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/vk_shaders)
  foreach(sh ${PL_VK_SHADERS})
    get_filename_component(shName ${sh} NAME_WE)
    get_filename_component(shExt ${sh} EXT)
    string(SUBSTRING ${shExt} 1 -1 shStage)
    set(shOut ${CMAKE_BINARY_DIR}/vk_shaders/${shName}_${shStage}.h)
    add_custom_command(OUTPUT ${shOut}
      COMMAND $<TARGET_FILE:glslang-standalone> -V --target-env vulkan1.0 --vn k_${shName}_${shStage} -o ${shOut} ${CMAKE_CURRENT_SOURCE_DIR}/${sh}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${sh} glslang-standalone
      COMMENT "SPIR-V: ${sh}")
    list(APPEND PL_VK_SHADER_HEADERS ${shOut})
  endforeach()
  # 0.60.0 (Vulkan step 2): renderer2d's shaders for Vulkan, made from shaders.h's GLSL text (src/render/shaders.h, taken out by cmake/extract_glsl.cmake):
  # glslang's relaxed Vulkan rules (-R) put the loose uniforms into one block per stage (vertex: set 0 binding 2; fragment: set 0 binding 1), the
  # fragment shader's texture at binding 0 (glslang's own choice); the in/out locations by declaration order (the app checks the bindings, and that the
  # locations match between the stages, from the SPIR-V itself: vk_draw2d.cpp).
  set(PL_VK2D_SHADERS "tri_vert:kVertexShader" "tri_frag:kFragmentShader" "sq_vert:kSquareVS" "sq_frag:kSquareFS" "blob_vert:kBlobVS" "blob_frag:kBlobFS"
                      "cmp_vert:kCompositeVS" "cmp_frag:kCompositeFS" "gray_frag:kGrayFS" "planet_frag:kPlanetFS" "chart_vert:kChartVS"
                      # 0.61.0 (Vulkan step 3): the card programs, the same way (compute shaders: their loose uniforms in a block at set 0 binding 7; their
                      # storage buffers keep the bindings of their text)
                      "cloud_vert:kCloudVS" "moldot_vert:kMolDotVS" "cgrid_vert:kCloudGridVS" "mgrid_vert:kMolGridVS" "grid_frag:kCloudGridFS"
                      "groups_comp:kGroupsCS" "outdens_comp:kOutlineDensityCS" "outlevel_comp:kOutlineLevelCS" "rocks_comp:kRocksCS" "grains_comp:kGrainsCS" "slice_comp:kSliceDotsCS"
                      "jump_comp:kJumpShowCS" "moldots_comp:kMolDotsCS"
                      # 0.62.0 (Vulkan step 4): the Black Hole Lab's, the same way (kKerrSkyFS from its parts, as render/kerr_sky.cpp; the light paths' compute
                      # shader in double precision: shaderFloat64)
                      "lens_frag:kLensHeadPart+kGasPart+kLensMainPart" "kerr_frag:kKerrFS" "skytiny_frag:kKerrSkyTinyFS" "skymath_frag:kKerrSkyHeadPart+kEllipticPart+kKerrSkyMathPart"
                      "sky_frag:kKerrSkyHeadPart+kEllipticPart+kKerrSkyTracePart+kGasPart+kKerrSkyMainPart" "maxdown_frag:kMaxDownFS" "expsum_frag:kExpSumFS"
                      "glaredown_frag:kGlareDownFS" "glareadd_frag:kGlareAddFS"
                      "glarelevel_frag:kGlareHeadPart+kGlareLevelPart" "glaresum_frag:kGlareHeadPart+kGlareSumPart" "kerrtable_comp:kKerrTableCS"   # 0.85.5: the glare veil on the card
                      # 0.64.0: Saturn's rings per pixel and their probe (the start-up check)
                      "rings_frag:kRingsHeadFSPart+kRingsPart+kRingsMainFSPart" "ringprobe_comp:kRingsHeadCSPart+kRingsPart+kRingsMainCSPart"
                      "classic_comp:kClassicCS"   # 0.64.0: the Classic dots from the cloud buffer
                      "nucdens_comp:kNucDensCS"   # 0.67.0: the nucleus' Density look
                      # 0.68.0: the Orbit Lab's lines (the compute steps and the vertex shader that draws them, Vulkan only: storage buffers in set 1)
                      "lines_comp:kLinesHeadCSPart+kLinesPart+kLinesMainCSPart" "lines_vert:kLinesHeadVSPart+kLinesPart+kLinesMainVSPart"
                      # 0.68.0: the Molecule Lab's Density surface (turned, coloured and sorted on the card; its vertex shader Vulkan only)
                      "surface_comp:kSurfaceCS" "surface_vert:kSurfaceVSHeadPart+kSurfaceVSMainPart")
  foreach(pair ${PL_VK2D_SHADERS})
    string(REPLACE ":" ";" parts ${pair})
    list(GET parts 0 shId)
    list(GET parts 1 shVar)
    string(REGEX MATCH "[a-z]+$" shStage ${shId})
    set(shGlsl ${CMAKE_BINARY_DIR}/vk_shaders/r2d_${shId}.${shStage})
    set(shOut ${CMAKE_BINARY_DIR}/vk_shaders/r2d_${shId}.h)
    if(shStage STREQUAL "vert")
      set(shFlags --sdub uDefault 0 2)
    elseif(shStage STREQUAL "comp")
      set(shFlags --sdub uDefault 0 7)
    else()
      set(shFlags --sdub uDefault 0 1)
    endif()
    add_custom_command(OUTPUT ${shOut}
      COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders.h -DNAME=${shVar} -DOUT=${shGlsl} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/extract_glsl.cmake
      COMMAND $<TARGET_FILE:glslang-standalone> -V -R --aml --amb ${shFlags} --target-env vulkan1.0 -S ${shStage} --vn k_r2d_${shId} -o ${shOut} ${shGlsl}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders.h ${CMAKE_CURRENT_SOURCE_DIR}/cmake/extract_glsl.cmake glslang-standalone
      COMMENT "SPIR-V (Vulkan, from shaders.h ${shVar}): r2d_${shId}")
    list(APPEND PL_VK_SHADER_HEADERS ${shOut})
  endforeach()
  # 0.67.0: the light paths' detective work (render/vk_helper.cpp): kKerrTableCS cut down in five ways (cmake/extract_glsl.cmake VARIANT), made only by the
  # helper process while the light paths are off on this driver
  foreach(kv math step edge ray short)
    set(shGlsl ${CMAKE_BINARY_DIR}/vk_shaders/r2d_kerrvar_${kv}.comp)
    set(shOut ${CMAKE_BINARY_DIR}/vk_shaders/r2d_kerrvar_${kv}.h)
    add_custom_command(OUTPUT ${shOut}
      COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders.h -DNAME=kKerrTableCS -DVARIANT=${kv} -DOUT=${shGlsl} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/extract_glsl.cmake
      COMMAND $<TARGET_FILE:glslang-standalone> -V -R --aml --amb --sdub uDefault 0 7 --target-env vulkan1.0 -S comp --vn k_r2d_kerrvar_${kv} -o ${shOut} ${shGlsl}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/render/shaders.h ${CMAKE_CURRENT_SOURCE_DIR}/cmake/extract_glsl.cmake glslang-standalone
      COMMENT "SPIR-V (Vulkan, kKerrTableCS variant ${kv}): r2d_kerrvar_${kv}")
    list(APPEND PL_VK_SHADER_HEADERS ${shOut})
  endforeach()

  add_executable(PhysicsLab WIN32
    ${CMAKE_BINARY_DIR}/embedded_fonts.cpp
    ${PL_VK_SHADER_HEADERS}
    src/main.cpp
    src/render/vk_backend.cpp
    src/render/vk_draw2d.cpp
    src/render/renderer2d_vk.cpp
    src/render/vk_card.cpp
    src/render/vk_lines.cpp
    src/render/vk_surface.cpp
    src/render/vk_blackhole.cpp
    src/render/vk_helper.cpp
    src/render/renderer2d.cpp
   
    src/render/kerr_compute.cpp
    src/render/kerr_sky.cpp
    src/render/atom_card.cpp
    src/render/molecule_card.cpp
    src/blackhole/spin_card.cpp
    src/blackhole/sky_card.cpp
    src/orbit/belt_layer.cpp
    src/orbit/tail_layer.cpp
    src/orbit/orbit_view.cpp
    src/orbit/orbit_lines.cpp
    src/orbit/lines_check.cpp
    src/molecule/surface_check.cpp
    src/orbit/ring_check.cpp
    src/atom/classic_check.cpp
    src/atom/nucdens_check.cpp
    src/atom/atom_view.cpp
    src/atom/atom_card_paths.cpp
    src/molecule/molecule_card_paths.cpp
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
    src/ui/sysinfo.cpp
    src/ui/power.cpp)
  target_link_libraries(PhysicsLab PRIVATE analysis_core orbit_core atom_core app_core molecule_core blackhole_core maps_core imgui SDL3::SDL3-static)
  target_compile_options(PhysicsLab PRIVATE ${PL_WARNINGS})
  target_include_directories(PhysicsLab PRIVATE ${CMAKE_BINARY_DIR}/vk_shaders)
  if(WIN32)
    enable_language(RC)                                  # the icon of PhysicsLab.exe
    target_sources(PhysicsLab PRIVATE assets/physics_lab.rc)
  endif()
  target_compile_definitions(PhysicsLab PRIVATE PL_VERSION="${PROJECT_VERSION}")
  if(PL_VK_TESTS)                                      # vk_compare_tests: the GPU programs against the CPU
    add_executable(vk_compare_tests tests/vk_compare.cpp src/atom/chart_draw.cpp src/render/renderer2d.cpp src/render/renderer2d_vk.cpp src/render/kerr_compute.cpp
                   src/render/kerr_sky.cpp src/render/atom_card.cpp src/render/molecule_card.cpp src/orbit/belt_layer.cpp src/orbit/tail_layer.cpp src/orbit/ring_check.cpp src/atom/classic_check.cpp
                   src/atom/nucdens_check.cpp src/orbit/orbit_lines.cpp src/orbit/lines_check.cpp src/render/vk_lines.cpp src/molecule/surface_check.cpp src/render/vk_surface.cpp
                   src/render/vk_helper.cpp src/render/vk_card.cpp src/render/vk_draw2d.cpp src/render/vk_blackhole.cpp src/blackhole/spin_card.cpp src/blackhole/sky_card.cpp
                   ${PL_VK_SHADER_HEADERS})
    target_include_directories(vk_compare_tests PRIVATE src/render ${CMAKE_BINARY_DIR}/vk_shaders)
    target_link_libraries(vk_compare_tests PRIVATE atom_core maps_core app_core molecule_core blackhole_core orbit_core volk::volk)
    target_compile_options(vk_compare_tests PRIVATE ${PL_WARNINGS})
    # the card programs and the renderer's pictures on Vulkan against the processor (lavapipe here; validation on where the layers are installed)
    add_test(NAME vk_cards COMMAND vk_compare_tests ${CMAKE_BINARY_DIR}/vk_pictures)
    set_tests_properties(vk_cards PROPERTIES TIMEOUT 2400)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/vk_pictures)
  endif()
  # 0.16 stage 1b: theme_pictures and window_pictures (pictures of the OLD interface) were deleted with it. The New UI's picture tests are below. Linux only (the cloud session); the GitHub Windows build does not run them. Needs -DPL_BUILD_APP=ON -DPL_PICTURE_TESTS=ON.
  option(PL_PICTURE_TESTS "Register the theme picture test (needs a headless Mesa and the app built on Linux)" OFF)
  if(PL_PICTURE_TESTS AND UNIX)
    enable_testing()
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_FOUND)
      # 0.76.0: the UI suite cut to what catches real breakage (DECISIONS 23 Nov 2026). tests/TESTS.md lists every part, its time and why it is kept.
      # tests/run_group.py runs the parts of a group one after the other and stops a part after 5 minutes. To run one part by hand:
      # python3 tests/<script> <PhysicsLab> <folder> [part]. COST: the measured seconds, so ctest starts the longest groups first.
      # ui_smoke: the app starts, each of the four labs draws a frame that is not empty with zero Vulkan validation errors, the app ends cleanly (every job runs it: check.sh job)
      add_test(NAME ui_smoke COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_smoke
               check_shell.py:smoke)
      set_tests_properties(ui_smoke PROPERTIES TIMEOUT 900 COST 30)
      # ui_shell: the app's start on Vulkan, the message without Vulkan, the GPU guards and the crash-safe helper; the calculator, the Data panel, About and the changelog
      add_test(NAME ui_shell COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_shell
               check_shell.py:vulkan check_shell.py:novulkan check_shell.py:guardturn check_shell.py:gpuhelper check_5k.py:calc check_5k.py:data check_5k.py:about check_5k.py:log)
      set_tests_properties(ui_shell PROPERTIES TIMEOUT 900 COST 232)
      # ui_screens: Settings, the Benchmark window, search in every lab, the look of the themes
      add_test(NAME ui_screens COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_screens
               check_5t.py:grey check_5t.py:tiles check_6b.py:bench check_6b.py:benchrest check_6b.py:benchrestore check_022s7.py:search check_audit19.py:molsearch)
      set_tests_properties(ui_screens PROPERTIES TIMEOUT 900 COST 112)
      # ui_orbit: the Orbit Lab's controls against the old panel's states, its card, keys, orbit paths, Most realistic, the fact line
      add_test(NAME ui_orbit COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_orbit
               check_shell_state.py check_cards.py:orbit check_022.py:keys check_023.py:orbits check_025.py:orbit check_022.py:facts check_022.py:factsrc)
      set_tests_properties(ui_orbit PROPERTIES TIMEOUT 900 COST 312)
      # ui_orbit_card: the Orbit Lab on the card against the processor (Saturn's rings, the fly-by, the lines) and against the formulas (the rings' slant, the air's edge, the night side)
      add_test(NAME ui_orbit_card COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_orbit_card
               check_orbit_ui.py:rings check_orbit_ui.py:flyby check_orbit_ui.py:cardlines check_orbit_ui.py:ringslant check_orbit_ui.py:airedge check_orbit_ui.py:nightside check_orbit_ui.py:planets check_orbit_ui.py:review085)
      set_tests_properties(ui_orbit_card PROPERTIES TIMEOUT 900 COST 461)
      # ui_atom: the Atom Lab: the element chip, the toolbar, the honest captions; the Slice's Glow dots on the card against the processor
      add_test(NAME ui_atom COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_atom
               check_atom_chip.py check_atom_toolbar.py check_022.py:honest check_atom_chip.py:classicsliceglow)
      set_tests_properties(ui_atom PROPERTIES TIMEOUT 900 COST 249)
      # ui_atom_card: the Atom Lab's card programs against the processor (the Slice's dots, the jump's groups, the nucleus' Density look) and their guards
      add_test(NAME ui_atom_card COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_atom_card
               check_atom_chip.py:card check_6c.py:nucdens check_6c.py:nucdenscard)
      set_tests_properties(ui_atom_card PROPERTIES TIMEOUT 900 COST 183)
      # ui_atom_classic: the Classic dots on the card against the processor: Cloud 3D and the side view
      add_test(NAME ui_atom_classic COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_atom_classic
               check_atom_chip.py:classic check_atom_chip.py:classicside)
      set_tests_properties(ui_atom_classic PROPERTIES TIMEOUT 900 COST 356)
      # ui_atom_classic2: the Classic dots on the card against the processor: the side view in Glow and the Slice
      add_test(NAME ui_atom_classic2 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_atom_classic2
               check_atom_chip.py:classicsideglow check_atom_chip.py:classicslice)
      set_tests_properties(ui_atom_classic2 PROPERTIES TIMEOUT 900 COST 414)
      # ui_molecule: the Molecule Lab: pictures, picking, settings, rotation, the anharmonic bond; the Density cloud's program against the processor and its guard
      add_test(NAME ui_molecule COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_molecule
               check_molecule.py check_molecule.py:rotation check_molecule.py:anharm check_molecule.py:card check_molecule.py:cardpiclight)
      set_tests_properties(ui_molecule PROPERTIES TIMEOUT 900 COST 207)
      # ui_molecule_card: the Molecule Lab's Density cloud and surface on the card against the processor's pictures
      add_test(NAME ui_molecule_card COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_molecule_card
               check_molecule.py:cardpic check_molecule.py:cardsurface)
      set_tests_properties(ui_molecule_card PROPERTIES TIMEOUT 900 COST 288)
      # ui_blackhole: the Black Hole Lab: its tab, presets, distances against Synge, bending and sky, Most realistic, the Orbit Lab's card, settings, the disc, spin, all light; the card's bent sky against the double-precision reference
      add_test(NAME ui_blackhole COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_blackhole
               check_blackhole.py:open check_blackhole.py:presets check_blackhole.py:distance check_blackhole.py:light check_blackhole.py:realest check_blackhole.py:orbitcard
               check_blackhole.py:settings check_blackhole.py:disc check_blackhole.py:gasrow check_blackhole.py:edgeon check_blackhole.py:spin check_blackhole.py:alllight check_blackhole.py:precision)
      set_tests_properties(ui_blackhole PROPERTIES TIMEOUT 900 COST 340)
      # ui_blackhole_sky: the Black Hole Lab's sky per pixel and the card's light paths against the double-precision reference, and no white-outs against the processor's settled picture
      add_test(NAME ui_blackhole_sky COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_group.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui_blackhole_sky
               check_blackhole.py:sky check_blackhole.py:precisioncard check_blackhole.py:whiteout)
      set_tests_properties(ui_blackhole_sky PROPERTIES TIMEOUT 900 COST 371)
    endif()
  endif()
endif()

# 0.15 stage 4.1: labels and a fast, identical environment for the tests. quick = the unit test programs, glsl and the source scan (about a minute, no app start);
# pictures = everything that starts the real app (on Mesa's software Vulkan, lavapipe). One thread per app start (so `ctest -j<cores>` uses every core instead of every start fighting for
# all of them) and a shared Mesa shader cache (a start no longer rebuilds its shaders): the pictures are the same (the picture guards prove it).
foreach(t orbit_physics atom_physics blackhole_physics movinggas_physics calc_logic analysis_logic molecule_logic vibration_logic rotation_logic density_logic app_settings font_coverage nbody_physics jpl_validation moon_validation moon_real_validation glsl_shaders windows_decorated)
  if(TEST ${t})
    set_tests_properties(${t} PROPERTIES LABELS "quick")
  endif()
endforeach()
# 0.76.0: the longest unit test starts first in a parallel run (its measured seconds), so it does not end the full suite alone
if(TEST atom_physics)
  set_tests_properties(atom_physics PROPERTIES COST 270)
endif()
# 0.38.1: every test that is not "quick" starts the real app, so it gets the pictures label and settings automatically (the hand-written list of 0.15 had missed the 5m2, 5p,
# 5q, 5r, 5t and 6c tests, about 25, which then ran with every llvmpipe thread each and fought over the cores)
get_property(pl_all_tests DIRECTORY PROPERTY TESTS)
foreach(t ${pl_all_tests})
  get_test_property(${t} LABELS pl_lab)
  if(NOT pl_lab STREQUAL "quick")
    get_test_property(${t} ENVIRONMENT pl_env)             # 0.65.0: a test's own settings kept
    if(NOT pl_env)
      set(pl_env "")
    endif()
    set(pl_labels "pictures")
    if(t STREQUAL "ui_smoke")
      set(pl_labels "pictures;smoke")                      # 0.76.0: check.sh job runs it with quick
    endif()
    set_tests_properties(${t} PROPERTIES LABELS "${pl_labels}" ENVIRONMENT "LP_NUM_THREADS=1;MESA_SHADER_CACHE_DIR=${CMAKE_BINARY_DIR}/mesa_cache;${pl_env}")
  endif()
endforeach()
