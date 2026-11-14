# SPDX-License-Identifier: Apache-2.0
# Pure C++17: signal processing, fusion, alarm and trend logic. No OS calls.
add_library(vitals_core STATIC
    src/trend.cpp
    src/version.cpp)
target_include_directories(vitals_core PUBLIC include)
target_compile_definitions(vitals_core PRIVATE VITALS_VERSION="${PROJECT_VERSION}")
vitals_warnings(vitals_core)
