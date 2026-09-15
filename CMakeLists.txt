cmake_minimum_required(VERSION 3.16)
project(tanks CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(raylib 6 REQUIRED)

add_library(tanks_logic STATIC
  src/arena.cpp src/physics.cpp src/combat.cpp
  src/ai.cpp src/levels.cpp src/player.cpp src/replay.cpp)
target_include_directories(tanks_logic PUBLIC src)

add_executable(tanks
  src/main.cpp src/models.cpp src/render.cpp
  src/particles.cpp src/audio.cpp src/ui.cpp)
target_link_libraries(tanks tanks_logic raylib)
if(TARGET glfw)
  # for the GLFW press-latch in main.cpp (touchpad taps emit press+release
  # inside one poll batch, which raylib's per-frame state model cannot see)
  target_link_libraries(tanks glfw)
  target_compile_definitions(tanks PRIVATE TANKS_HAVE_GLFW)
  # Conan's glfw package ships the headers but exports no include dirs (the
  # CCI recipe never sets cpp_info.includedirs), so point at the package.
  if(DEFINED glfw_PACKAGE_FOLDER_RELEASE AND NOT glfw_INCLUDE_DIRS_RELEASE)
    target_include_directories(tanks SYSTEM PRIVATE "${glfw_PACKAGE_FOLDER_RELEASE}/include")
  endif()
endif()

enable_testing()
add_executable(tanks_tests
  tests/test_main.cpp tests/test_arena.cpp tests/test_physics.cpp
  tests/test_combat.cpp tests/test_mines.cpp tests/test_levels.cpp
  tests/test_ai.cpp tests/test_aim.cpp tests/test_sim.cpp tests/test_replay.cpp)
target_link_libraries(tanks_tests tanks_logic)
add_test(NAME unit COMMAND tanks_tests)
