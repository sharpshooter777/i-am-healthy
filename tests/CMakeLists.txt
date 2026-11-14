# SPDX-License-Identifier: Apache-2.0
# Unit tests run on every platform. On QNX they are cross-compiled here and
# run on the target with scripts/qnx-selftest.sh.
add_executable(core_tests
    main.cpp
    core/test_trend.cpp)
target_link_libraries(core_tests PRIVATE vitals_core doctest)
vitals_warnings(core_tests)

add_executable(platform_tests
    main.cpp
    platform/test_clock.cpp)
target_link_libraries(platform_tests PRIVATE vitals_platform doctest)
vitals_warnings(platform_tests)

if(NOT CMAKE_CROSSCOMPILING)
    add_test(NAME core_tests COMMAND core_tests)
    add_test(NAME platform_tests COMMAND platform_tests)
    add_test(NAME vm-core-selftest COMMAND vm-core --selftest)
endif()
