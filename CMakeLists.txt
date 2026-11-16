cmake_minimum_required(VERSION 3.22)
project(BassForge VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(BASSFORGE_BUILD_PLUGIN "Build the JUCE plug-in (VST3/AU/Standalone)" ON)
option(BASSFORGE_BUILD_TESTS "Build headless DSP tests and tools" ON)

# ---------------------------------------------------------------- framework-free DSP core
add_library(bassforge_dsp STATIC
  Source/Dsp/HalfbandResampler.cpp
  Source/Dsp/LookaheadEnvelope.cpp
  Source/Dsp/TruePeakGuard.cpp
  Source/Dsp/DriveVoices.cpp
  Source/Dsp/Dynamics.cpp
  Source/Dsp/Cabinet.cpp
  Source/Dsp/Eq.cpp
  Source/Dsp/Room.cpp
  Source/Dsp/Engine.cpp)
target_include_directories(bassforge_dsp PUBLIC Source/Dsp)
if(MSVC)
  target_compile_options(bassforge_dsp PRIVATE /W4 /fp:precise /wd4100)
else()
  target_compile_options(bassforge_dsp PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

if(BASSFORGE_BUILD_TESTS)
  enable_testing()
  add_executable(bassforge_tests Tests/TestMain.cpp Tests/TestDsp.cpp Tests/TestEngine.cpp)
  target_link_libraries(bassforge_tests PRIVATE bassforge_dsp)
  target_include_directories(bassforge_tests PRIVATE Tests tools)
  add_test(NAME bassforge_tests COMMAND bassforge_tests)

  add_executable(bf_render tools/bf_render.cpp)
  target_include_directories(bf_render PRIVATE tools)
  target_link_libraries(bf_render PRIVATE bassforge_dsp)

  add_executable(bf_bench tools/bf_bench.cpp)
  target_link_libraries(bf_bench PRIVATE bassforge_dsp)
endif()

if(BASSFORGE_BUILD_PLUGIN)
  include(cmake/Plugin.cmake)
endif()
