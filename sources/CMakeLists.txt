# SPDX-License-Identifier: Apache-2.0
# Frame sources: synthetic camera, recorded-session replay. The Isaac Sim
# client and the QNX Camera API source follow.
add_library(vitals_sources STATIC
    src/recording.cpp
    src/synthetic.cpp)
target_include_directories(vitals_sources PUBLIC include)
vitals_warnings(vitals_sources)
