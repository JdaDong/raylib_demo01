cmake_minimum_required(VERSION 3.20)
project(raylib_demo01 VERSION 1.0.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(RD_FETCH_RAYLIB "Download raylib with FetchContent when not installed" ON)
option(RD_BUILD_TESTS  "Build unit tests" ON)
option(RD_BUILD_GAME   "Build the game executable (off = headless CI: core + tests only)" ON)
option(RD_SMOKE_TEST   "Register GUI smoke test in CTest (needs a display)" OFF)
option(RD_WERROR       "Treat warnings as errors" OFF)
option(RD_SANITIZE     "Enable Address/UB sanitizers for tests" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# raylib resolution (RD_RAYLIB_SOURCE):
#   AUTO (default): git submodule third_party/raylib (pinned 6.0) -> installed package -> FetchContent
#   SUBMODULE | SYSTEM | FETCH force one source
# ---------------------------------------------------------------------------
set(RD_RAYLIB_SOURCE "AUTO" CACHE STRING "raylib source: AUTO|SUBMODULE|SYSTEM|FETCH")
set_property(CACHE RD_RAYLIB_SOURCE PROPERTY STRINGS AUTO SUBMODULE SYSTEM FETCH)
set(RD_RAYLIB_SUBMODULE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/raylib")
set(RD_RAYLIB_FROM "")

if(RD_RAYLIB_SOURCE STREQUAL "SUBMODULE" OR
   (RD_RAYLIB_SOURCE STREQUAL "AUTO" AND EXISTS "${RD_RAYLIB_SUBMODULE}/CMakeLists.txt"))
  if(NOT EXISTS "${RD_RAYLIB_SUBMODULE}/CMakeLists.txt")
    message(FATAL_ERROR "raylib submodule missing: git submodule update --init --depth 1")
  endif()
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  add_subdirectory("${RD_RAYLIB_SUBMODULE}" "${CMAKE_BINARY_DIR}/_raylib" EXCLUDE_FROM_ALL)
  set(RD_RAYLIB_FROM "submodule")
elseif(NOT RD_RAYLIB_SOURCE STREQUAL "FETCH")
  find_package(raylib 5.5 QUIET)
  if(raylib_FOUND)
    set(RD_RAYLIB_FROM "installed package ${raylib_VERSION}")
  elseif(RD_RAYLIB_SOURCE STREQUAL "SYSTEM")
    message(FATAL_ERROR "raylib package not found (brew install raylib)")
  endif()
endif()

if(RD_RAYLIB_FROM)
  message(STATUS "raylib: ${RD_RAYLIB_FROM}")
elseif(RD_FETCH_RAYLIB OR RD_RAYLIB_SOURCE STREQUAL "FETCH")
  message(STATUS "raylib: FetchContent from GitHub (tag 6.0)")
  include(FetchContent)
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 6.0
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(raylib)
else()
  message(FATAL_ERROR "raylib not found. brew install raylib, or -DRD_FETCH_RAYLIB=ON")
endif()

function(rd_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
    if(RD_WERROR)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

# ---------------------------------------------------------------------------
# rd_core: CPU-only logic (no window / GL / audio device needed) -> unit-testable
# ---------------------------------------------------------------------------
add_library(rd_core STATIC
  src/core/spatial_grid.cpp
  src/game/world.cpp
  src/game/wave_director.cpp
  src/game/particles.cpp
  src/game/config.cpp
  src/game/highscore.cpp
  src/game/storage.cpp
  src/game/hashing.cpp
  src/gfx/procgen.cpp
  src/gfx/image_lab.cpp
  src/audio/wavgen.cpp
  src/ui/ui_logic.cpp
)
target_include_directories(rd_core PUBLIC src)
target_link_libraries(rd_core PUBLIC raylib)
target_compile_definitions(rd_core PUBLIC RD_VERSION="${PROJECT_VERSION}")
rd_warnings(rd_core)

# ---------------------------------------------------------------------------
# Game executable: window / GPU / audio side
# ---------------------------------------------------------------------------
if(RD_BUILD_GAME)
add_executable(raylib_demo01
  src/main.cpp
  src/app/app.cpp
  src/app/logger.cpp
  src/gfx/assets.cpp
  src/gfx/postfx.cpp
  src/gfx/renderer.cpp
  src/gfx/backdrop3d.cpp
  src/audio/audio_system.cpp
  src/platform/input_mapper.cpp
  src/ui/widgets.cpp
  src/scenes/title_scene.cpp
  src/scenes/gameplay_scene.cpp
  src/scenes/gameover_scene.cpp
  src/scenes/settings_scene.cpp
  src/scenes/showcase_scene.cpp
)
target_link_libraries(raylib_demo01 PRIVATE rd_core)
rd_warnings(raylib_demo01)
if(APPLE)
  target_link_libraries(raylib_demo01 PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(RD_BUILD_TESTS)
  enable_testing()
  add_executable(unit_tests
    tests/test_main.cpp
    tests/test_core.cpp
    tests/test_spatial_grid.cpp
    tests/test_world.cpp
    tests/test_wave_director.cpp
    tests/test_particles.cpp
    tests/test_config.cpp
    tests/test_highscore.cpp
    tests/test_storage.cpp
    tests/test_hashing.cpp
    tests/test_procgen.cpp
    tests/test_image_lab.cpp
    tests/test_wavgen.cpp
    tests/test_ui_logic.cpp
    tests/test_raylib_text.cpp
    tests/test_raylib_shapes.cpp
    tests/test_raylib_color.cpp
  )
  target_link_libraries(unit_tests PRIVATE rd_core)
  target_compile_definitions(unit_tests PRIVATE RD_TEST_TMP="${CMAKE_CURRENT_BINARY_DIR}/test_tmp")
  rd_warnings(unit_tests)
  set(RD_SANITIZERS "address,undefined" CACHE STRING "sanitizers used when RD_SANITIZE=ON")
  if(RD_SANITIZE AND NOT MSVC)
    message(STATUS "unit_tests sanitizers: ${RD_SANITIZERS}")
    target_compile_options(unit_tests PRIVATE -fsanitize=${RD_SANITIZERS} -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    target_link_options(unit_tests PRIVATE -fsanitize=${RD_SANITIZERS})
  endif()

  # one CTest entry per suite -> failures are reported per module
  set(RD_SUITES core spatial_grid world wave_director particles config highscore
                storage hashing procgen image_lab wavgen ui_logic
                raylib_text raylib_shapes raylib_color)
  foreach(suite IN LISTS RD_SUITES)
    add_test(NAME ut.${suite} COMMAND unit_tests --suite ${suite})
    set_tests_properties(ut.${suite} PROPERTIES LABELS "unit" TIMEOUT 60)
  endforeach()

  if(RD_SMOKE_TEST AND RD_BUILD_GAME)
    add_test(NAME smoke.gui COMMAND raylib_demo01 --smoke 600 --save-dir ${CMAKE_CURRENT_BINARY_DIR}/smoke_saves)
    set_tests_properties(smoke.gui PROPERTIES LABELS "gui" TIMEOUT 120)
  endif()
endif()
