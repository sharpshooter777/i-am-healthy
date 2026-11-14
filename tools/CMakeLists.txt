# SPDX-License-Identifier: Apache-2.0
# Desk tools for development; not part of the live pipeline.
add_executable(vm-offline vm-offline/main.cpp)
target_link_libraries(vm-offline PRIVATE vitals_core vitals_sources)
vitals_warnings(vm-offline)
