# SPDX-License-Identifier: Apache-2.0
# Desk tools for development; not part of the live pipeline.
add_executable(vm-offline vm-offline/main.cpp)
target_link_libraries(vm-offline PRIVATE vitals_core vitals_sources)
vitals_warnings(vm-offline)

# Serves chest-strap heart rate to the QNX pipeline over TCP. Runs on the
# machine with the Bluetooth radio.
add_executable(hr-bridge hr-bridge/main.cpp)
target_link_libraries(hr-bridge PRIVATE vitals_core vitals_sources vitals_platform)
vitals_warnings(hr-bridge)

# Serves camera frames, reduced to coarse colour grids, to the QNX pipeline
# over TCP. Runs on the machine with the camera.
add_executable(grid-bridge grid-bridge/main.cpp)
target_link_libraries(grid-bridge PRIVATE vitals_core vitals_platform)
vitals_warnings(grid-bridge)

# Camera heart rate from coarse colour grids piped out of FFmpeg.
add_executable(grid-pulse grid-pulse/main.cpp)
target_link_libraries(grid-pulse PRIVATE vitals_core)
vitals_warnings(grid-pulse)

# A recorded session through the heart monitor, strap and camera together.
add_executable(session-replay session-replay/main.cpp)
target_link_libraries(session-replay PRIVATE vitals_core vitals_sources)
vitals_warnings(session-replay)
