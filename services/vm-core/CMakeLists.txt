# SPDX-License-Identifier: Apache-2.0
add_executable(vm-core
    hr_client.cpp
    main.cpp
    selftest.cpp)
target_link_libraries(vm-core PRIVATE vitals_core vitals_platform)
vitals_warnings(vm-core)
