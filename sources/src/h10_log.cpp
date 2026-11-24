// SPDX-License-Identifier: Apache-2.0
#include "vitals/sources/h10_log.hpp"

#include <cctype>
#include <cstdlib>

namespace vitals::sources {

bool parse_gatt_line(const std::string& line, std::int64_t& epoch_ns, std::vector<std::uint8_t>& bytes) {
    const std::size_t marker = line.find("value:");
    if (marker == std::string::npos || line.find("Notification") == std::string::npos) return false;

    // Leading timestamp: whole seconds, a dot, then up to nine digits.
    std::size_t i = 0;
    std::int64_t sec = 0;
    if (i >= line.size() || !std::isdigit(static_cast<unsigned char>(line[i]))) return false;
    while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) {
        sec = sec * 10 + (line[i] - '0');
        ++i;
    }
    std::int64_t frac = 0;
    int digits = 0;
    if (i < line.size() && line[i] == '.') {
        ++i;
        while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i]))) {
            if (digits < 9) {
                frac = frac * 10 + (line[i] - '0');
                ++digits;
            }
            ++i;
        }
    }
    for (; digits < 9; ++digits) frac *= 10;
    epoch_ns = sec * 1000000000LL + frac;

    bytes.clear();
    std::size_t p = marker + 6;
    while (p < line.size()) {
        while (p < line.size() && std::isspace(static_cast<unsigned char>(line[p]))) ++p;
        if (p >= line.size()) break;
        if (p + 1 >= line.size() || !std::isxdigit(static_cast<unsigned char>(line[p])) ||
            !std::isxdigit(static_cast<unsigned char>(line[p + 1]))) {
            return false;
        }
        bytes.push_back(static_cast<std::uint8_t>(std::strtoul(line.substr(p, 2).c_str(), nullptr, 16)));
        p += 2;
    }
    return !bytes.empty();
}

}  // namespace vitals::sources
