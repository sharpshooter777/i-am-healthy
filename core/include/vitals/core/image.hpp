// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>

namespace vitals::core {

// Rectangle in pixels.
struct Roi {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct MeanRgb {
    bool valid = false;
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
};

// Mean colour of a region of a packed 8-bit RGB image. The region is
// clipped to the image; an empty result is invalid.
MeanRgb mean_rgb(const std::uint8_t* rgb24, int width, int height, int stride_bytes, Roi roi);

}  // namespace vitals::core
