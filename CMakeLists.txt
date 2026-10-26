# =============================================================================
# DSP ENGINE — Highly optimized 64-bit audio processing engine
# Features: Direct Volume Control (DVC), multi-band EQ, ultra-low latency,
#           bit-perfect path, near-real-time priority, lock-free buffers
# =============================================================================

cmake_minimum_required(VERSION 3.16)

project(DSP_ENGINE
    VERSION 0.3.2
    DESCRIPTION "64-bit DSP processing engine with Direct Volume Control and parametric EQ"
    LANGUAGES C CXX
)

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------
option(DSP_BUILD_SHARED     "Build shared library"                  ON)
option(DSP_BUILD_STATIC     "Build static library"                  OFF)
option(DSP_BUILD_EXAMPLES   "Build example programs"                ON)
option(DSP_BUILD_TESTS      "Build unit tests"                      OFF)
option(DSP_ENABLE_SIMD      "Enable SIMD optimizations (AVX2/NEON)" ON)
option(DSP_ENABLE_RT        "Enable real-time thread priority"      ON)
option(DSP_BIT_PERFECT      "Compile with bit-perfect path enabled" ON)
option(DSP_DOUBLE_PRECISION "Use float64 for internal processing"   ON)

# Platform backends — auto-enabled on the matching host OS unless overridden.
# Cross-compiling: pass -DDSP_BACKEND_*=ON/OFF explicitly.
if(ANDROID)
    set(_DSP_DEFAULT_AAUDIO ON)
else()
    set(_DSP_DEFAULT_AAUDIO OFF)
endif()
if(WIN32 AND NOT ANDROID)
    set(_DSP_DEFAULT_WASAPI ON)
else()
    set(_DSP_DEFAULT_WASAPI OFF)
endif()
if(APPLE)
    set(_DSP_DEFAULT_COREAUDIO ON)
else()
    set(_DSP_DEFAULT_COREAUDIO OFF)
endif()

option(DSP_BACKEND_AAUDIO    "Android AAudio backend"               ${_DSP_DEFAULT_AAUDIO})
option(DSP_BACKEND_WASAPI    "Windows WASAPI exclusive/shared"      ${_DSP_DEFAULT_WASAPI})
option(DSP_BACKEND_COREAUDIO "macOS/iOS Core Audio"                 ${_DSP_DEFAULT_COREAUDIO})
option(DSP_BACKEND_ALSA      "Linux ALSA"                           OFF)
option(DSP_BACKEND_JACK      "Linux JACK"                           OFF)
option(DSP_BACKEND_PIPEWIRE  "Linux PipeWire"                       OFF)

# -----------------------------------------------------------------------------
# C++ standard & global flags
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Optimization flags
set(DSP_OPT_FLAGS "")
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    list(APPEND DSP_OPT_FLAGS -O3 -ffast-math -fno-math-errno -fno-trapping-math)
    if(DSP_ENABLE_SIMD)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
            list(APPEND DSP_OPT_FLAGS -mavx2 -mfma)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
            list(APPEND DSP_OPT_FLAGS -march=armv8-a+simd)
        endif()
    endif()
elseif(MSVC)
    list(APPEND DSP_OPT_FLAGS /O2 /fp:fast)
endif()
add_compile_options(${DSP_OPT_FLAGS})

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
set(DSP_LIB_SOURCES
    src/core/processor.cpp
    src/core/dvc.cpp
    src/core/resampler.cpp
    src/utils/math_utils.cpp
    src/audio/ring_buffer.cpp
    src/audio/backends/backend_stubs.cpp
    src/audio/backends/backend_aaudio.cpp
    src/audio/backends/backend_coreaudio.cpp
    src/audio/backends/backend_wasapi.cpp
    src/audio/backends/backend_c_api.cpp
    src/realtime/thread_priority.cpp
)

# Helper function to configure a library target (shared or static)
function(configure_dsp_target target_name)
    target_include_directories(${target_name}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_compile_definitions(${target_name}
        PRIVATE
            $<$<BOOL:${DSP_DOUBLE_PRECISION}>:DSP_USE_DOUBLE>
            $<$<BOOL:${DSP_BIT_PERFECT}>:DSP_BIT_PERFECT_PATH>
            $<$<BOOL:${DSP_ENABLE_RT}>:DSP_ENABLE_REALTIME>
            $<$<BOOL:${DSP_ENABLE_SIMD}>:DSP_ENABLE_SIMD>
    )

    # Platform backends
    if(ANDROID AND DSP_BACKEND_AAUDIO)
        target_compile_definitions(${target_name} PRIVATE DSP_HAS_AAUDIO)
        find_library(AAUDIO_LIB aaudio)
        if(AAUDIO_LIB)
            target_link_libraries(${target_name} PRIVATE ${AAUDIO_LIB})
        else()
            target_link_libraries(${target_name} PRIVATE aaudio)
        endif()
        find_library(ANDROID_LOG_LIB log)
        if(ANDROID_LOG_LIB)
            target_link_libraries(${target_name} PRIVATE ${ANDROID_LOG_LIB})
        endif()
        message(STATUS "AAudio backend enabled for ${target_name}")
    endif()

    if(WIN32 AND DSP_BACKEND_WASAPI)
        target_compile_definitions(${target_name} PRIVATE DSP_HAS_WASAPI)
        target_link_libraries(${target_name} PRIVATE ole32 avrt)
        message(STATUS "WASAPI backend enabled for ${target_name}")
    endif()

    if(APPLE AND DSP_BACKEND_COREAUDIO)
        target_compile_definitions(${target_name} PRIVATE DSP_HAS_COREAUDIO)
        target_link_libraries(${target_name} PRIVATE
            "-framework AudioUnit"
            "-framework AudioToolbox"
            "-framework CoreAudio"
        )
        message(STATUS "Core Audio backend enabled for ${target_name}")
    endif()

    if(UNIX AND DSP_ENABLE_RT)
        find_package(Threads REQUIRED)
        target_link_libraries(${target_name} PRIVATE Threads::Threads)
    endif()

    set_target_properties(${target_name} PROPERTIES
        PUBLIC_HEADER "include/dsp_engine.h"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
endfunction()

# -----------------------------------------------------------------------------
# Library targets
# -----------------------------------------------------------------------------
set(DSP_INSTALL_TARGETS "")

if(DSP_BUILD_SHARED)
    add_library(dsp_engine SHARED ${DSP_LIB_SOURCES})
    target_compile_definitions(dsp_engine PRIVATE DSP_ENGINE_EXPORTS)
    set_target_properties(dsp_engine PROPERTIES
        OUTPUT_NAME "dsp_engine"
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    # Avoid symlink creation issues on restricted filesystems
    if(UNIX AND NOT APPLE)
        set_target_properties(dsp_engine PROPERTIES
            SKIP_BUILD_RPATH FALSE
        )
    endif()
    configure_dsp_target(dsp_engine)
    list(APPEND DSP_INSTALL_TARGETS dsp_engine)
endif()

if(DSP_BUILD_STATIC)
    add_library(dsp_engine_static STATIC ${DSP_LIB_SOURCES})
    set_target_properties(dsp_engine_static PROPERTIES
        OUTPUT_NAME "dsp_engine"
    )
    configure_dsp_target(dsp_engine_static)
    list(APPEND DSP_INSTALL_TARGETS dsp_engine_static)

    # Provide a consistent name for consumers when only static is built
    if(NOT DSP_BUILD_SHARED)
        add_library(dsp_engine ALIAS dsp_engine_static)
    endif()
endif()

if(NOT DSP_INSTALL_TARGETS AND NOT TARGET dsp_engine)
    message(FATAL_ERROR "At least one of DSP_BUILD_SHARED or DSP_BUILD_STATIC must be ON")
endif()

# -----------------------------------------------------------------------------
# Installation
# -----------------------------------------------------------------------------
include(GNUInstallDirs)

if(DSP_INSTALL_TARGETS)
    install(TARGETS ${DSP_INSTALL_TARGETS}
        EXPORT DSPEngineTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

install(EXPORT DSPEngineTargets
    FILE DSPEngineTargets.cmake
    NAMESPACE DSP::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DSPEngine
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/DSPEngineConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/DSPEngineConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/DSPEngineConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DSPEngine
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/DSPEngineConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/DSPEngineConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DSPEngine
)

# -----------------------------------------------------------------------------
# Examples & tests
# -----------------------------------------------------------------------------
if(DSP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(DSP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

message(STATUS "")
message(STATUS "=== DSP ENGINE ${PROJECT_VERSION} ===")
message(STATUS "Build type          : ${CMAKE_BUILD_TYPE}")
message(STATUS "Shared library      : ${DSP_BUILD_SHARED}")
message(STATUS "Static library      : ${DSP_BUILD_STATIC}")
message(STATUS "Double precision    : ${DSP_DOUBLE_PRECISION}")
message(STATUS "Bit-perfect path    : ${DSP_BIT_PERFECT}")
message(STATUS "SIMD                : ${DSP_ENABLE_SIMD}")
message(STATUS "Real-time priority  : ${DSP_ENABLE_RT}")
message(STATUS "Examples            : ${DSP_BUILD_EXAMPLES}")
message(STATUS "Tests               : ${DSP_BUILD_TESTS}")
message(STATUS "Backend AAudio      : ${DSP_BACKEND_AAUDIO}")
message(STATUS "Backend WASAPI      : ${DSP_BACKEND_WASAPI}")
message(STATUS "Backend CoreAudio   : ${DSP_BACKEND_COREAUDIO}")
message(STATUS "==============================")
message(STATUS "")
