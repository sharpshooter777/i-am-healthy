# SPDX-License-Identifier: Apache-2.0
# Unit tests run on every platform. On QNX they are cross-compiled here and
# run on the target with scripts/qnx-selftest.sh.
add_executable(core_tests
    main.cpp
    core/test_dsp.cpp
    core/test_fusion.cpp
    core/test_grid_pulse.cpp
    core/test_heart_monitor.cpp
    core/test_heart_rate.cpp
    core/test_trend.cpp)
target_link_libraries(core_tests PRIVATE vitals_core doctest)
vitals_warnings(core_tests)

add_executable(platform_tests
    main.cpp
    platform/test_clock.cpp
    platform/test_net.cpp)
target_link_libraries(platform_tests PRIVATE vitals_platform doctest)
vitals_warnings(platform_tests)

add_executable(sources_tests
    main.cpp
    sources/test_h10_log.cpp
    sources/test_sources.cpp)
target_link_libraries(sources_tests PRIVATE vitals_sources vitals_core doctest)
vitals_warnings(sources_tests)

if(NOT CMAKE_CROSSCOMPILING)
    add_test(NAME core_tests COMMAND core_tests)
    add_test(NAME platform_tests COMMAND platform_tests)
    add_test(NAME sources_tests COMMAND sources_tests)
    add_test(NAME vm-core-selftest COMMAND vm-core --selftest)

    # The desk tool end to end: write a synthetic recording, measure it.
    add_test(NAME vm-offline-synth
             COMMAND vm-offline synth offline-test.vmrec --seconds 40 --hr 75 --slope 3)
    add_test(NAME vm-offline-analyse
             COMMAND vm-offline analyse offline-test.vmrec --max-error 3 --limit 100 --quiet)
    set_tests_properties(vm-offline-synth PROPERTIES FIXTURES_SETUP offline_rec)
    set_tests_properties(vm-offline-analyse PROPERTIES FIXTURES_REQUIRED offline_rec)

    # Strap and camera together, end to end: write a made-up session, replay
    # it, then replay it with the strap withheld for the last 20 seconds.
    add_test(NAME session-replay-synth
             COMMAND session-replay synth replay-test-strap.raw replay-test-grid.raw --seconds 60 --bpm 72)
    add_test(NAME session-replay-agree
             COMMAND session-replay --strap replay-test-strap.raw --grid replay-test-grid.raw
                     --grid-start 1000000000 --quiet --require-corroborated 25)
    add_test(NAME session-replay-strap-off
             COMMAND session-replay --strap replay-test-strap.raw --grid replay-test-grid.raw
                     --grid-start 1000000000 --quiet --strap-off 40 20 --require-camera-alone 10)
    set_tests_properties(session-replay-synth PROPERTIES FIXTURES_SETUP replay_session)
    set_tests_properties(session-replay-agree session-replay-strap-off PROPERTIES FIXTURES_REQUIRED replay_session)
endif()
