// SPDX-License-Identifier: Apache-2.0
#include "vitals/core/grid_line.hpp"

#include <cstdio>
#include <cstdlib>

namespace vitals::core {

namespace {

constexpr char hex_digits[] = "0123456789abcdef";
constexpr std::size_t max_side = 1024;  // far above any grid in use; bounds the allocation

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Reads an unsigned decimal field ending at a space. Advances `pos` past the space.
bool read_field(const std::string& s, std::size_t& pos, unsigned long long& value) {
    if (pos >= s.size() || s[pos] < '0' || s[pos] > '9') return false;
    unsigned long long v = 0;
    std::size_t digits = 0;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
        if (++digits > 19) return false;  // would overflow
        v = v * 10 + static_cast<unsigned long long>(s[pos] - '0');
        ++pos;
    }
    if (pos >= s.size() || s[pos] != ' ') return false;
    ++pos;
    value = v;
    return true;
}

}  // namespace

std::string encode_grid_line(const GridFrame& f) {
    if (f.cols == 0 || f.rows == 0 || f.rgb.size() != f.cols * f.rows * 3 || f.t_ns < 0) return {};
    char head[96];
    const int n = std::snprintf(head, sizeof head, "GR1 %llu %lld %zu %zu ", static_cast<unsigned long long>(f.seq),
                                static_cast<long long>(f.t_ns), f.cols, f.rows);
    if (n <= 0) return {};
    std::string line(head, static_cast<std::size_t>(n));
    line.reserve(line.size() + f.rgb.size() * 4);
    for (const std::uint16_t v : f.rgb) {
        line.push_back(hex_digits[(v >> 12) & 0xf]);
        line.push_back(hex_digits[(v >> 8) & 0xf]);
        line.push_back(hex_digits[(v >> 4) & 0xf]);
        line.push_back(hex_digits[v & 0xf]);
    }
    return line;
}

bool decode_grid_line(const std::string& line, GridFrame& out) {
    std::size_t end = line.size();
    while (end > 0 && (line[end - 1] == '\n' || line[end - 1] == '\r')) --end;
    if (end < 4 || line.compare(0, 4, "GR1 ") != 0) return false;

    std::size_t pos = 4;
    unsigned long long seq = 0, t_ns = 0, cols = 0, rows = 0;
    if (!read_field(line, pos, seq) || !read_field(line, pos, t_ns) || !read_field(line, pos, cols) ||
        !read_field(line, pos, rows)) {
        return false;
    }
    if (cols == 0 || rows == 0 || cols > max_side || rows > max_side) return false;
    if (t_ns > 9223372036854775807ULL) return false;
    const std::size_t values = static_cast<std::size_t>(cols * rows * 3);
    if (end - pos != values * 4) return false;

    out.seq = seq;
    out.t_ns = static_cast<std::int64_t>(t_ns);
    out.cols = static_cast<std::size_t>(cols);
    out.rows = static_cast<std::size_t>(rows);
    out.rgb.resize(values);
    for (std::size_t i = 0; i < values; ++i) {
        const int a = hex_value(line[pos]), b = hex_value(line[pos + 1]);
        const int c = hex_value(line[pos + 2]), d = hex_value(line[pos + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0) return false;
        out.rgb[i] = static_cast<std::uint16_t>((a << 12) | (b << 8) | (c << 4) | d);
        pos += 4;
    }
    return true;
}

}  // namespace vitals::core
