// SPDX-License-Identifier: Apache-2.0
// Every frame here is made up.
#include <doctest.h>

#include <cstdint>
#include <string>

#include "vitals/core/grid_line.hpp"

using namespace vitals::core;

namespace {

GridFrame sample_frame(std::size_t cols, std::size_t rows) {
    GridFrame f;
    f.seq = 41;
    f.t_ns = 1700000000123456789LL;
    f.cols = cols;
    f.rows = rows;
    f.rgb.resize(cols * rows * 3);
    for (std::size_t i = 0; i < f.rgb.size(); ++i) f.rgb[i] = static_cast<std::uint16_t>((i * 2731u + 17u) & 0xffffu);
    return f;
}

}  // namespace

TEST_CASE("grid line: a frame survives the round trip") {
    const GridFrame f = sample_frame(32, 18);
    const std::string line = encode_grid_line(f);
    REQUIRE(!line.empty());
    CHECK(line.size() <= grid_line_max_bytes(32, 18));
    CHECK(line.find('\n') == std::string::npos);

    GridFrame g;
    REQUIRE(decode_grid_line(line, g));
    CHECK(g.seq == f.seq);
    CHECK(g.t_ns == f.t_ns);
    CHECK(g.cols == 32);
    CHECK(g.rows == 18);
    CHECK(g.rgb == f.rgb);
}

TEST_CASE("grid line: extremes, line endings and upper-case digits") {
    GridFrame f;
    f.seq = 0;
    f.t_ns = 0;
    f.cols = 1;
    f.rows = 1;
    f.rgb = {0x0000, 0xffff, 0x00a5};
    const std::string line = encode_grid_line(f);
    CHECK(line == "GR1 0 0 1 1 0000ffff00a5");

    GridFrame g;
    REQUIRE(decode_grid_line(line + "\r\n", g));
    CHECK(g.rgb == f.rgb);
    REQUIRE(decode_grid_line("GR1 7 9 1 1 0000FFFF00A5", g));
    CHECK(g.seq == 7);
    CHECK(g.t_ns == 9);
    CHECK(g.rgb == f.rgb);
}

TEST_CASE("grid line: a frame of the wrong size is not written") {
    GridFrame f = sample_frame(4, 2);
    f.rgb.pop_back();
    CHECK(encode_grid_line(f).empty());
    f = sample_frame(4, 2);
    f.cols = 0;
    CHECK(encode_grid_line(f).empty());
}

TEST_CASE("grid line: damaged lines are refused") {
    const std::string good = encode_grid_line(sample_frame(4, 2));
    GridFrame g;
    REQUIRE(decode_grid_line(good, g));

    CHECK_FALSE(decode_grid_line("", g));
    CHECK_FALSE(decode_grid_line("HR1 1 60 1 -", g));                   // the other line type
    CHECK_FALSE(decode_grid_line(good.substr(0, good.size() - 1), g));  // cut short
    CHECK_FALSE(decode_grid_line(good + "0", g));                       // too long
    std::string bad = good;
    bad[bad.size() - 2] = 'g';                                          // not a hex digit
    CHECK_FALSE(decode_grid_line(bad, g));
    CHECK_FALSE(decode_grid_line("GR1 1 2 0 2 ", g));                   // no columns
    CHECK_FALSE(decode_grid_line("GR1 1 2 5000 5000 00", g));           // absurd size
    CHECK_FALSE(decode_grid_line("GR1 1 -2 1 1 000000000000", g));      // negative time
    CHECK_FALSE(decode_grid_line("GR1 1 99999999999999999999 1 1 000000000000", g));
    CHECK_FALSE(decode_grid_line("GR1 1 2 1 1", g));                    // no values at all
}
