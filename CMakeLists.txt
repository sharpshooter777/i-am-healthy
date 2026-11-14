# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.25)

project(vitalmon
    VERSION 0.1.0
    DESCRIPTION "Camera-based patient monitor research prototype on QNX"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Every executable lands in build/<preset>/bin, so deploying is one copy.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(VITALS_BUILD_TESTS "Build the unit tests" ON)

# Warnings for our own targets only; third-party code is left alone.
function(vitals_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
    endif()
endfunction()

add_subdirectory(core)
add_subdirectory(platform)
add_subdirectory(sources)
add_subdirectory(services)
add_subdirectory(tools)

if(VITALS_BUILD_TESTS)
    enable_testing()
    add_library(doctest INTERFACE)
    target_include_directories(doctest SYSTEM INTERFACE ${PROJECT_SOURCE_DIR}/third_party/doctest)
    # doctest forward-declares std::ostream by default, which clashes with
    # libc++'s inline namespace in the QNX SDP. Include the real headers.
    target_compile_definitions(doctest INTERFACE DOCTEST_CONFIG_USE_STD_HEADERS)
    add_subdirectory(tests)
endif()
