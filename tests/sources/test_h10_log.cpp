// SPDX-License-Identifier: Apache-2.0
// The lines here are made-up examples in the logger's format, not readings.
#include <doctest.h>

#include <cstdint>
#include <vector>

#include "vitals/sources/h10_log.hpp"

using vitals::sources::parse_gatt_line;

TEST_CASE("strap log: a notification line gives time and bytes") {
    std::int64_t t = 0;
    std::vector<std::uint8_t> b;
    REQUIRE(parse_gatt_line("1700000000.250000000 Notification handle = 0x0010 value: 10 3c 00 04 ", t, b));
    CHECK(t == 1700000000250000000LL);
    REQUIRE(b.size() == 4);
    CHECK(b[0] == 0x10);
    CHECK(b[3] == 0x04);
}

TEST_CASE("strap log: short fractions and other lines") {
    std::int64_t t = 0;
    std::vector<std::uint8_t> b;
    REQUIRE(parse_gatt_line("1700000001.5 Notification handle = 0x0010 value: 04 00", t, b));
    CHECK(t == 1700000001500000000LL);
    CHECK_FALSE(parse_gatt_line("1700000002.000000000 LOGGER restart", t, b));
    CHECK_FALSE(parse_gatt_line("1700000002.000000000 Characteristic value was written successfully", t, b));
    CHECK_FALSE(parse_gatt_line("Notification handle = 0x0010 value: 10 3c", t, b));
    CHECK_FALSE(parse_gatt_line("1700000003.0 Notification handle = 0x0010 value: 10 zz", t, b));
    CHECK_FALSE(parse_gatt_line("", t, b));
}
