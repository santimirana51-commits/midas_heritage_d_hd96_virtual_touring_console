cmake_minimum_required(VERSION 3.22)
project(OPENLIVE_REFACTORED VERSION 1.1.0 LANGUAGES C CXX)

option(USE_ASIO_SDK "Enable real ASIO SDK integration" OFF)
set(NOVA_MAX_DELAY_MS "500" CACHE STRING
    "Maximum input delay in milliseconds")
if(NOT NOVA_MAX_DELAY_MS MATCHES "^[1-9][0-9]*$")
  message(FATAL_ERROR "NOVA_MAX_DELAY_MS must be a positive integer")
endif()
if(NOVA_MAX_DELAY_MS GREATER 500)
  message(FATAL_ERROR "NOVA_MAX_DELAY_MS cannot exceed 500")
endif()

set(JUCE_SOURCE_DIR "" CACHE PATH
    "Optional path to an existing JUCE source checkout")

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
if(JUCE_SOURCE_DIR)
  if(NOT EXISTS "${JUCE_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
      "JUCE_SOURCE_DIR does not contain a JUCE CMake project: ${JUCE_SOURCE_DIR}")
  endif()
  FetchContent_Declare(
    JUCE
    SOURCE_DIR "${JUCE_SOURCE_DIR}"
  )
else()
  FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
    GIT_SHALLOW TRUE
  )
endif()
FetchContent_MakeAvailable(JUCE)

# Target GUI Application
juce_add_gui_app(OPENLIVE_REFACTORED
  PRODUCT_NAME "OpenLive HD Digital Mixing System"
  COMPANY_NAME "OpenLive Audio"
  BUNDLE_ID "com.openlive.mixing"
)

target_sources(OPENLIVE_REFACTORED PRIVATE
  Source/Application/Main.cpp
)

target_include_directories(OPENLIVE_REFACTORED PRIVATE
  Source
  Source/Application
  Source/Audio
  Source/Mixer
  Source/DSP
  Source/Effects
  Source/Routing
  Source/GUI
  Source/MIDI
  Source/Recording
  Source/Utilities
)

target_compile_definitions(OPENLIVE_REFACTORED PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:OPENLIVE_REFACTORED,JUCE_PRODUCT_NAME>"
  JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
  NOVA_MAX_DELAY_MS=${NOVA_MAX_DELAY_MS}
)

if(WIN32)
  target_compile_definitions(OPENLIVE_REFACTORED PRIVATE JUCE_WASAPI=1)
  if(MSVC)
    target_link_options(OPENLIVE_REFACTORED PRIVATE /STACK:8388608)
  endif()
endif()

target_link_libraries(OPENLIVE_REFACTORED PRIVATE
  juce::juce_audio_utils
  juce::juce_audio_devices
  juce::juce_dsp
  juce::juce_gui_extra
  juce::juce_recommended_config_flags
  juce::juce_recommended_lto_flags
  juce::juce_recommended_warning_flags
)

# Optional: Real ASIO SDK integration
if(USE_ASIO_SDK)
  message(STATUS "Building with ASIO SDK support")
  set(ASIO_SDK_DIR "C:/ASIO_SDK" CACHE PATH "Path to Steinberg ASIO SDK")
  target_include_directories(OPENLIVE_REFACTORED PRIVATE ${ASIO_SDK_DIR})
  target_compile_definitions(OPENLIVE_REFACTORED PRIVATE USE_ASIO_SDK)
endif()

# ==============================================================================
# Unit & Integration Tests
# ==============================================================================
if(BUILD_TESTING)
  enable_testing()

  # Main Engine & Integration Tests
  add_executable(OpenLive_Tests Tests/EngineTests.cpp)
  target_include_directories(OpenLive_Tests PRIVATE
    Source
    Source/Application
    Source/Audio
    Source/Mixer
    Source/DSP
    Source/Effects
    Source/Routing
    Source/GUI
    Source/MIDI
    Source/Recording
    Source/Utilities
  )
  target_compile_definitions(OpenLive_Tests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    NOVA_MAX_DELAY_MS=${NOVA_MAX_DELAY_MS}
  )
  target_link_libraries(OpenLive_Tests PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
  )
  if(MSVC)
    target_link_options(OpenLive_Tests PRIVATE /STACK:8388608)
  endif()
  add_test(NAME OpenLive_Tests COMMAND OpenLive_Tests)

  # Graviton DSP unit tests
  add_executable(GravitonDSP_Tests Tests/GravitonDSPTests.cpp)
  target_include_directories(GravitonDSP_Tests PRIVATE
    Source
    Source/DSP
    Source/Mixer
    Source/Effects
    Source/Audio
    Source/Routing
    Source/Utilities
  )
  target_compile_features(GravitonDSP_Tests PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(GravitonDSP_Tests PRIVATE /W4 /permissive-)
  else()
    target_compile_options(GravitonDSP_Tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME GravitonDSP_Tests COMMAND GravitonDSP_Tests)

  # Graviton stress tests
  add_executable(GravitonStress_Tests Tests/GravitonStressTests.cpp)
  target_include_directories(GravitonStress_Tests PRIVATE
    Source
    Source/DSP
    Source/Mixer
    Source/Effects
    Source/Audio
    Source/Routing
    Source/Utilities
  )
  target_compile_features(GravitonStress_Tests PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(GravitonStress_Tests PRIVATE /W4 /permissive-)
  else()
    target_compile_options(GravitonStress_Tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME GravitonStress_Tests COMMAND GravitonStress_Tests)

  # Graviton DSP benchmark
  add_executable(GravitonBenchmark Tests/GravitonBenchmark.cpp)
  target_include_directories(GravitonBenchmark PRIVATE
    Source
    Source/DSP
    Source/Mixer
    Source/Effects
    Source/Audio
    Source/Routing
    Source/Utilities
  )
  target_compile_features(GravitonBenchmark PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(GravitonBenchmark PRIVATE /W4 /permissive-)
  else()
    target_compile_options(GravitonBenchmark PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  # AUX routing benchmark
  add_executable(GravitonAuxRoutingBenchmark Tests/GravitonAuxRoutingBenchmark.cpp)
  target_include_directories(GravitonAuxRoutingBenchmark PRIVATE
    Source
    Source/DSP
    Source/Mixer
    Source/Effects
    Source/Audio
    Source/Routing
    Source/Utilities
  )
  target_compile_features(GravitonAuxRoutingBenchmark PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(GravitonAuxRoutingBenchmark PRIVATE /W4 /permissive-)
  else()
    target_compile_options(GravitonAuxRoutingBenchmark PRIVATE -Wall -Wextra -Wpedantic)
  endif()

  # Real-time-safe AUX routing snapshot tests
  add_executable(GravitonAuxSnapshot_Tests Tests/GravitonAuxSnapshotTests.cpp)
  target_include_directories(GravitonAuxSnapshot_Tests PRIVATE
    Source
    Source/Routing
    Source/Mixer
    Source/DSP
    Source/Audio
    Source/Utilities
  )
  target_compile_features(GravitonAuxSnapshot_Tests PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(GravitonAuxSnapshot_Tests PRIVATE /W4 /permissive-)
  else()
    target_compile_options(GravitonAuxSnapshot_Tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME GravitonAuxSnapshot_Tests COMMAND GravitonAuxSnapshot_Tests)

  # Real-time-safe Matrix routing tests
  add_executable(GravitonMatrix_Tests Tests/GravitonMatrixTests.cpp)
  target_include_directories(GravitonMatrix_Tests PRIVATE
    Source
    Source/Routing
    Source/Mixer
    Source/DSP
    Source/Audio
    Source/Utilities
  )
  target_compile_features(GravitonMatrix_Tests PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(GravitonMatrix_Tests PRIVATE /W4 /permissive-)
  else()
    target_compile_options(GravitonMatrix_Tests PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  add_test(NAME GravitonMatrix_Tests COMMAND GravitonMatrix_Tests)

  # Matrix routing benchmark
  add_executable(GravitonMatrixBenchmark Tests/GravitonMatrixBenchmark.cpp)
  target_include_directories(GravitonMatrixBenchmark PRIVATE
    Source
    Source/Routing
    Source/Mixer
    Source/DSP
    Source/Audio
    Source/Utilities
  )
  target_compile_features(GravitonMatrixBenchmark PRIVATE cxx_std_20)
  if(MSVC)
    target_compile_options(GravitonMatrixBenchmark PRIVATE /W4 /permissive-)
  else()
    target_compile_options(GravitonMatrixBenchmark PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

