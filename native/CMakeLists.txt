cmake_minimum_required(VERSION 3.24)
project(PhysicsLabNative VERSION 0.20 LANGUAGES C CXX)

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
  src/orbit/nbody.cpp
  src/orbit/moon_elp.cpp
  src/orbit/solar_system.cpp
  src/orbit/format.cpp
  src/orbit/card_text.cpp
  src/orbit/subject_card.cpp
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
  src/atom/quark_wander.cpp
  src/atom/atom_learn.cpp
  src/atom/atom_tools.cpp
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
  src/app/look_options.cpp
  src/app/html_entities.cpp
  src/app/display_scale.cpp
  src/app/power.cpp
  src/app/page_cache.cpp
  src/app/shell.cpp
  src/app/shot.cpp
  src/app/fps.cpp
  src/app/bench.cpp
  src/app/bench_scenes.cpp
  src/app/gpu_timer.cpp
)
target_include_directories(app_core PUBLIC src)
target_link_libraries(app_core PUBLIC atom_core)   # 0.16 stage 4: bench_scenes.cpp reads the Atom Lab scene texts
target_compile_options(app_core PRIVATE ${PL_WARNINGS})

# Molecule Lab (0.18 stage 1): 15 small molecules in their measured (CCCBDB) geometry, the camera, picking and the settings text (no graphics)
add_library(molecule_core STATIC
  src/molecule/molecule.cpp
  src/molecule/builder.cpp
)
target_include_directories(molecule_core PUBLIC src)
target_link_libraries(molecule_core PUBLIC atom_core)   # the CODATA constants (h, c, u) in atom/atom_core.h
target_compile_options(molecule_core PRIVATE ${PL_WARNINGS})

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
  add_executable(molecule_tests tests/test_molecule.cpp)
  target_link_libraries(molecule_tests PRIVATE molecule_core)
  target_compile_options(molecule_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME molecule_logic COMMAND molecule_tests)
  add_executable(molbuild_tests tests/test_molbuild.cpp)   # 0.20: the Molecule Lab builder (valences, names, the estimated shape, the settings line)
  target_link_libraries(molbuild_tests PRIVATE molecule_core)
  target_compile_options(molbuild_tests PRIVATE ${PL_WARNINGS})
  add_test(NAME molbuild_logic COMMAND molbuild_tests)
  add_executable(app_tests tests/test_app.cpp)
  target_include_directories(app_tests PRIVATE tests)
  target_link_libraries(app_tests PRIVATE app_core atom_core)
  target_compile_options(app_tests PRIVATE ${PL_WARNINGS})
  target_compile_definitions(app_tests PRIVATE PL_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")   # 0.14 stage 2: the bundled font and its licence are checked
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
    src/render/earth_texture.cpp
    src/render/group_compute.cpp
    src/render/outline_compute.cpp
    src/render/rock_compute.cpp
    src/orbit/belt_layer.cpp
    src/orbit/orbit_view.cpp
    src/atom/atom_view.cpp
    src/atom/chart_draw.cpp
    src/molecule/molecule_view.cpp
    src/ui/shell_molecule.cpp
    src/ui/orbit_panel.cpp
    src/ui/orbit_actions.cpp
    src/ui/shell.cpp
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
  target_link_libraries(PhysicsLab PRIVATE orbit_core atom_core app_core molecule_core imgui SDL3::SDL3-static)
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
      # 0.15 stage 5m: y2k_pictures, page_cache_pictures and effects_pictures were archived with the themes (native/archive/themes/tests)
      # 0.15 stage 3: the new UI (Settings -> New UI (preview)): text sizes and floors at every Text size and Windows scale, nothing leaves the window, collapsing, Esc / Ctrl+K / Space,
      # the centre shift, the reading view; the Orbit Lab top-10 controls give the same state as the old panel; pictures (glass opacity, regression guards)
      add_test(NAME shell_ui COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_shell.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/shell_ui)
      add_test(NAME shell_state COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_shell_state.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/shell_state)
      add_test(NAME atom_toolbar COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_atom_toolbar.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/atom_toolbar)
      add_test(NAME atom_chip COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_atom_chip.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/atom_chip)
      add_test(NAME controls_panel COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_controls.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/controls_panel)
      # 0.15 stage 5d / 5e: the Atom Lab's options, marks and photons in the Controls panel give the old panel's settings (the old panel of the same build is the reference); the two-line fact line
      add_test(NAME atom_panel COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_atom_panel.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/atom_panel)
      # 0.15 stage 5f part 2: the subject card, legend and data sheet: place, size, text, open / close, follows the chosen thing; the Atom sheet equals the old panel's strings
      foreach(part orbit atom)
        add_test(NAME cards_${part} COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_cards.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/cards_${part} ${part})
      endforeach()
      # 0.15 stage 5g: the owner's bug list (scroll, Contents, toolbar place, Cloud 3D, top right, themes, handle, panel scroll)
      foreach(part pages contents toolbar default ring panel atomtext)
        add_test(NAME ui5g_${part} COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_5g.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui5g_${part} ${part})
      endforeach()
      # 0.15 stage 5h: the Atom picture's centre and nucleus label clear of the cards and bars; the info text at 14 px
      foreach(part centre infosize cards fit)
        add_test(NAME ui5h_${part} COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_5h.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui5h_${part} ${part})
      endforeach()
      # 0.15 stage 5i: the data sheet ends above the bottom toolbar (3 sizes, 8 tabs, Orbit sheet)
      add_test(NAME ui5i_sheet COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_5i.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui5i_sheet sheet)
      # 0.15 stage 5j / 5m: the look M1 (the New UI's look, always on): the app starts, draws it, the 5g / 5h rect tests pass, and no text is cut in the panel / card / sheet (tests/check_5j.py)
      add_test(NAME ui5j_look_m1 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_5j.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui5j_look_m1 m1)
      # 0.15 stage 5m: the three archived themes are gone (list, settings file, search, facts) and M1 is on without any environment variable (tests/check_5m.py)
      add_test(NAME ui5m_themes COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_5m.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui5m_themes themes)
      set_tests_properties(ui5m_themes PROPERTIES TIMEOUT 3600)
      # 0.15 stage 5k part 1: reading pages scroll by wheel / keys whichever window has the focus, the scrollbar, the Contents list, the hidden input log
      foreach(part pages contents log windows toggle)
        add_test(NAME ui5k_${part} COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_5k.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui5k_${part} ${part})
        set_tests_properties(ui5k_${part} PROPERTIES TIMEOUT 3600)
      endforeach()
      # 0.15 stage 5l: search finds every Settings row and follows the text, the Legend closes on an outside click (tests/check_5l.py)
      foreach(part search legend toolbar popups)
        add_test(NAME ui5l_${part} COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_5l.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui5l_${part} ${part})
        set_tests_properties(ui5l_${part} PROPERTIES TIMEOUT 3600)
      endforeach()
      # 0.15 stage 5p: the owner's bug list of the PC test (wheel after a window was closed, About scroll, Atom zoom-out, lit buttons, Element picker) (tests/check_5p.py)
      foreach(part wheel scroll zoomout lit picker)
        add_test(NAME ui5p_${part} COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_5p.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui5p_${part} ${part})
        set_tests_properties(ui5p_${part} PROPERTIES TIMEOUT 3600)
      endforeach()
      # 0.15 stage 5q: the owner's PC findings (lit buttons in every theme, theme list below the top bar, About scrollbar width, End key, hovering Element picker, grey sliders) (tests/check_5q.py)
      foreach(part lit theme scrollbar endkey picker sliders)
        add_test(NAME ui5q_${part} COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_5q.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui5q_${part} ${part})
        set_tests_properties(ui5q_${part} PROPERTIES TIMEOUT 3600)
      endforeach()
      # 0.15 stage 5r: pills are flat: no lighter block behind the buttons, no outline except Monochrome / High contrast (tests/check_5r.py)
      foreach(part pill)
        add_test(NAME ui5r_${part} COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_5r.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui5r_${part} ${part})
        set_tests_properties(ui5r_${part} PROPERTIES TIMEOUT 3600)
      endforeach()
      # 0.15 stage 5t: the selected segment and the checked boxes of the Controls panel are grey (tests/check_5t.py)
      foreach(part grey)
        add_test(NAME ui5t_${part} COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_5t.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui5t_${part} ${part})
        set_tests_properties(ui5t_${part} PROPERTIES TIMEOUT 3600)
      endforeach()
      # 0.16 stage 1a: the Atom Lab's old-panel gaps (Dirac switch, lab spectrum card, Restart atom, draw-call line) have a New UI home (tests/check_6a.py)
      foreach(part gaps)
        add_test(NAME ui6a_atom_${part} COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_6a.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui6a_${part} ${part})
        set_tests_properties(ui6a_atom_${part} PROPERTIES TIMEOUT 3600)
      endforeach()
      # 0.16 stage 2: About scrollbar after reopen, Benchmark window height, Contents rail width, Atom Lab Nucleus controls at any zoom (tests/check_6b.py)
      foreach(part scrollbar bench rail nucleus)
        add_test(NAME ui6b_${part} COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_6b.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui6b_${part} ${part})
        set_tests_properties(ui6b_${part} PROPERTIES TIMEOUT 3600)
      endforeach()
      # 0.16 stage 3: the nucleus view at full zoom uploads only what can reach the picture (tests/check_6c.py; counts, not time)
      add_test(NAME ui6c_fullzoom COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_6c.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui6c_fullzoom fullzoom)
      # 0.16 stage 5: Ds-281 with "+ Sea", turning: the antiquark rings and the flux tubes go into the blob batch (draw calls and vertices a frame, not time)
      add_test(NAME ui6c_ds281sea COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_6c.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui6c_ds281sea ds281sea)
      set_tests_properties(ui6c_fullzoom PROPERTIES TIMEOUT 3600)
      # 0.15 stage 5m part 2 / 5n: handle size, the subject card never covers the Legend, the windows, popups and the chart tilt slider (tests/check_5m2.py)
      foreach(part layout tilt windows popups)
        add_test(NAME ui5m2_${part} COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_5m2.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui5m2_${part} ${part})
        set_tests_properties(ui5m2_${part} PROPERTIES TIMEOUT 3600)
      endforeach()
      # 0.18 stage 1: the Molecule Lab: water (Dark) and benzene space-filling (Light) to look at, draw counts, clicks on an atom and a bond, settings kept and a damaged line (tests/check_molecule.py)
      add_test(NAME molecule_pictures COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_molecule.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/molecule_pictures)
      # 0.20: the Molecule Lab builder (sketch, editing, a refusal, the estimated shape, no shape, the molbuild line) and the three layout fixes (tests/check_molbuild.py)
      foreach(part sketch edit refuse shape noshape settings u238 sheet molpan)
        add_test(NAME molbuild_${part} COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_molbuild.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/molbuild_${part} ${part})
      endforeach()
      # 0.19 audit fixes in the real app, one part per fix (tests/check_audit19.py): the guide's "monochrome" word, search in the Molecule Lab, its bar's lit buttons
      foreach(part monokey molsearch molbar)
        add_test(NAME ui19_${part} COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_audit19.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/ui19_${part} ${part})
      endforeach()
      add_test(NAME shell_pictures COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_shell_pictures.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/shell_pictures ${CMAKE_CURRENT_SOURCE_DIR}/tests/reference)
      # 0.15 stage 4: the whole Orbit Lab in the new UI: every INVENTORY.md item reachable by real clicks (incl. the 47 Go to names), the subject card's data sheet equal to the old card's lines
      # (8 things), the Time panel fits, the fact line (visible, changes after 20 s, stands still while hovered, readable, never overlaps), search (20 things, arrows, recent), links
      foreach(part coverage card time chip facts facts_long readable search links)
        add_test(NAME orbit_ui_${part} COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_orbit_ui.py $<TARGET_FILE:PhysicsLab> ${CMAKE_BINARY_DIR}/orbit_ui_${part} ${part})
        set_tests_properties(orbit_ui_${part} PROPERTIES TIMEOUT 3600)
      endforeach()
    endif()
  endif()
endif()

# 0.15 stage 4.1: labels and a fast, identical environment for the tests. quick = the unit test programs, glsl and the source scan (about a minute, no app start);
# pictures = everything that starts the real app under Mesa's software OpenGL. One llvmpipe thread per app start (so `ctest -j<cores>` uses every core instead of every start fighting for
# all of them) and a shared Mesa shader cache (a start no longer rebuilds its shaders): the pictures are the same (the picture guards prove it).
foreach(t orbit_physics atom_physics molecule_logic molbuild_logic app_settings font_coverage nbody_physics jpl_validation moon_validation moon_real_validation glsl_shaders windows_decorated)
  if(TEST ${t})
    set_tests_properties(${t} PROPERTIES LABELS "quick")
  endif()
endforeach()
foreach(t gl_phase_colours molecule_pictures molbuild_sketch molbuild_edit molbuild_refuse molbuild_shape molbuild_noshape molbuild_settings molbuild_u238 molbuild_sheet molbuild_molpan ui19_monokey ui19_molsearch ui19_molbar shell_ui shell_state atom_toolbar atom_chip controls_panel atom_panel shell_pictures cards_orbit cards_atom ui5g_pages ui5g_contents ui5g_toolbar ui5g_default ui5g_ring ui5g_panel ui5g_atomtext ui5h_centre ui5h_infosize ui5h_cards ui5h_fit ui5i_sheet ui6a_atom_gaps ui5j_look_m1 ui5m_themes ui5k_pages ui5k_contents ui5k_log ui5k_windows ui5k_toggle ui5l_search ui5l_legend ui5l_toolbar ui5l_popups
          orbit_ui_coverage orbit_ui_card orbit_ui_time orbit_ui_chip orbit_ui_facts orbit_ui_facts_long orbit_ui_readable orbit_ui_search orbit_ui_links ui6b_scrollbar ui6b_bench ui6b_rail ui6b_nucleus)
  if(TEST ${t})
    set_tests_properties(${t} PROPERTIES LABELS "pictures" ENVIRONMENT "LP_NUM_THREADS=1;MESA_SHADER_CACHE_DIR=${CMAKE_BINARY_DIR}/mesa_cache")
  endif()
endforeach()
