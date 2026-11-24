# SPDX-License-Identifier: Apache-2.0
# Pure C++17: signal processing, fusion, alarm and trend logic. No OS calls.
add_library(vitals_core STATIC
    src/fusion.cpp
    src/grid_pulse.cpp
    src/heart_monitor.cpp
    src/heart_rate.cpp
    src/image.cpp
    src/pulse.cpp
    src/resample.cpp
    src/spectrum.cpp
    src/trend.cpp
    src/version.cpp)
target_include_directories(vitals_core PUBLIC include)
# pocketfft (BSD-3-Clause), header-only, used privately by spectrum.cpp.
target_include_directories(vitals_core SYSTEM PRIVATE ${PROJECT_SOURCE_DIR}/third_party/pocketfft)
target_compile_definitions(vitals_core PRIVATE VITALS_VERSION="${PROJECT_VERSION}")
vitals_warnings(vitals_core)
