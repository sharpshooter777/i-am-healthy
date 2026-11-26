// SPDX-License-Identifier: Apache-2.0
//
// One camera frame, already reduced to a coarse grid of block-mean colours,
// as a line of text. This is how camera frames travel between machines: the
// machine with the camera reduces each frame and sends the grid; the pipeline
// on QNX reads it and looks for the pulse (grid_pulse.hpp). The full picture
// never crosses the network, only a few thousand numbers a frame.
//
// Text, like the heart-rate line (heart_rate.hpp), so both streams share one
// reader, can be watched with ordinary tools, and do not depend on byte order.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace vitals::core {

struct GridFrame {
    // Counts frames from the sender's start. A jump means frames were lost on
    // the way, which matters: the pulse reading assumes evenly spaced frames.
    std::uint64_t seq = 0;
    std::int64_t t_ns = 0;  // time the frame was taken, on the sender's clock
    std::size_t cols = 0;
    std::size_t rows = 0;
    // cols * rows * 3 values: row by row, left to right, red green blue.
    std::vector<std::uint16_t> rgb;
};

// One frame per line, no trailing newline:
//   GR1 <seq> <t_ns> <cols> <rows> <values as four hex digits each>
// Returns an empty string when rgb does not hold cols * rows * 3 values.
std::string encode_grid_line(const GridFrame& frame);

// Parses a line written by encode_grid_line. A trailing '\r' or '\n' is
// ignored. Returns false, leaving `out` unspecified, on anything else.
bool decode_grid_line(const std::string& line, GridFrame& out);

// The longest line a grid of this size can produce, for readers that cap
// how much they hold back while waiting for a newline.
constexpr std::size_t grid_line_max_bytes(std::size_t cols, std::size_t rows) {
    return cols * rows * 3 * 4 + 96;
}

}  // namespace vitals::core
