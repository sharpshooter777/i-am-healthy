// SPDX-License-Identifier: Apache-2.0
#include <doctest.h>

#include <string>

#include "vitals/platform/platform.hpp"

namespace platform = vitals::platform;

TEST_CASE("platform reports a name and an OS") {
    const std::string n = platform::name();
    CHECK((n == "qnx" || n == "posix" || n == "win32"));
    CHECK_FALSE(platform::os_description().empty());
}

TEST_CASE("monotonic clock never goes backwards") {
    std::int64_t prev = platform::monotonic_ns();
    REQUIRE(prev >= 0);
    for (int i = 0; i < 100000; ++i) {
        const std::int64_t now = platform::monotonic_ns();
        REQUIRE(now >= prev);
        prev = now;
    }
}

TEST_CASE("clock tick is positive") {
    CHECK(platform::clock_tick_ns() > 0);
}
