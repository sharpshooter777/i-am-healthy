// SPDX-License-Identifier: Apache-2.0
#include "vitals/core/image.hpp"

#include <algorithm>
#include <cstddef>

namespace vitals::core {

MeanRgb mean_rgb(const std::uint8_t* rgb24, int width, int height, int stride_bytes, Roi roi) {
    MeanRgb m;
    if (rgb24 == nullptr || width <= 0 || height <= 0 || stride_bytes < width * 3) return m;

    const int x0 = std::max(roi.x, 0);
    const int y0 = std::max(roi.y, 0);
    const int x1 = std::min(roi.x + roi.width, width);
    const int y1 = std::min(roi.y + roi.height, height);
    if (x1 <= x0 || y1 <= y0) return m;

    std::uint64_t sr = 0, sg = 0, sb = 0;
    for (int y = y0; y < y1; ++y) {
        const std::uint8_t* p = rgb24 + static_cast<std::ptrdiff_t>(y) * stride_bytes + x0 * 3;
        for (int x = x0; x < x1; ++x, p += 3) {
            sr += p[0];
            sg += p[1];
            sb += p[2];
        }
    }
    const auto count = static_cast<double>(x1 - x0) * static_cast<double>(y1 - y0);
    m.r = static_cast<double>(sr) / count;
    m.g = static_cast<double>(sg) / count;
    m.b = static_cast<double>(sb) / count;
    m.valid = true;
    return m;
}

}  // namespace vitals::core
