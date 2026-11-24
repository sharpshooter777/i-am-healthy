// SPDX-License-Identifier: Apache-2.0
//
// grid-pulse: camera heart rate from a stream of coarse colour grids.
// FFmpeg does the capture and the reduction; this reads the result:
//
//   ffmpeg -i <camera or file> -vf format=rgb48le,scale=32:18:flags=area
//          -f rawvideo -pix_fmt rgb48le - | grid-pulse -
//
// One line per estimate: time into the stream (s), bpm, quality, agreeing
// blocks, skin blocks, and the verdict (1 believable, 0 not).
//
// Research prototype. Not a medical device. Not for clinical decisions.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "vitals/core/grid_pulse.hpp"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: grid-pulse <file|-> [--cols N] [--rows N] [--fps F] [--hop S]\n");
        return 2;
    }
    vitals::core::GridPulseConfig cfg;
    double hop_s = 5.0;
    for (int i = 2; i + 1 < argc; i += 2) {
        const std::string a = argv[i];
        if (a == "--cols") cfg.cols = static_cast<std::size_t>(std::atoi(argv[i + 1]));
        else if (a == "--rows") cfg.rows = static_cast<std::size_t>(std::atoi(argv[i + 1]));
        else if (a == "--fps") cfg.fs_hz = std::atof(argv[i + 1]);
        else if (a == "--hop") hop_s = std::atof(argv[i + 1]);
        else if (a == "--min-hz") cfg.min_hz = std::atof(argv[i + 1]);
        else if (a == "--top") cfg.top_blocks = static_cast<std::size_t>(std::atoi(argv[i + 1]));
        else if (a == "--min-quality") cfg.min_quality = std::atof(argv[i + 1]);
        else if (a == "--min-agreeing") cfg.min_agreeing = static_cast<std::size_t>(std::atoi(argv[i + 1]));
        else {
            std::fprintf(stderr, "grid-pulse: unknown option %s\n", a.c_str());
            return 2;
        }
    }
    std::FILE* in = std::strcmp(argv[1], "-") == 0 ? stdin : std::fopen(argv[1], "rb");
    if (in == nullptr) {
        std::fprintf(stderr, "grid-pulse: cannot open %s\n", argv[1]);
        return 1;
    }

    vitals::core::GridPulse grid(cfg);
    const std::size_t values = grid.frame_values();
    std::vector<std::uint8_t> raw(values * 2);
    std::vector<double> frame(values);
    const auto hop = static_cast<std::size_t>(hop_s * cfg.fs_hz + 0.5);
    std::size_t frames = 0;
    while (std::fread(raw.data(), 1, raw.size(), in) == raw.size()) {
        for (std::size_t i = 0; i < values; ++i) {
            frame[i] = static_cast<double>(raw[2 * i] | (raw[2 * i + 1] << 8));  // little-endian 16-bit
        }
        grid.push(frame.data(), values);
        ++frames;
        if (hop > 0 && frames % hop == 0) {
            const vitals::core::GridPulseEstimate e = grid.estimate();
            if (e.ready) {
                std::printf("%.1f %.1f %.3f %zu %zu %d\n", static_cast<double>(frames) / cfg.fs_hz, e.bpm, e.quality,
                            e.agreeing, e.skin_blocks, e.believable ? 1 : 0);
                std::fflush(stdout);
            }
        }
    }
    if (in != stdin) std::fclose(in);
    return 0;
}
