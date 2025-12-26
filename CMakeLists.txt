cmake_minimum_required(VERSION 3.14)
project(CrabLib 
    VERSION 1.0.0
    DESCRIPTION "Rust-inspired memory safety for C++17"
    LANGUAGES CXX
)

# ============================================================================
# Library Configuration
# ============================================================================

add_library(crab INTERFACE)
add_library(crab::crab ALIAS crab)

target_include_directories(crab INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_compile_features(crab INTERFACE cxx_std_17)

# Optional: Enable debug assertions in debug builds
target_compile_definitions(crab INTERFACE
    $<$<CONFIG:Debug>:CRAB_DEBUG=1>
)

# ============================================================================
# Tests (optional)
# ============================================================================

option(CRAB_BUILD_TESTS "Build CrabLib tests" OFF)

if(CRAB_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Installation
# ============================================================================

include(GNUInstallDirs)

install(TARGETS crab
    EXPORT CrabLibTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY src/crab
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT CrabLibTargets
    FILE CrabLibConfig.cmake
    NAMESPACE crab::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/CrabLib
)
