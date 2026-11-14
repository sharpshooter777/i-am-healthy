// SPDX-License-Identifier: Apache-2.0
//
// One frame-source interface for every camera: simulated, replayed from a
// recording, or real. The pipeline cannot tell them apart.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace vitals::sources {

enum class PixelFormat : std::uint8_t {
    Rgb24 = 1,  // packed 8-bit R, G, B; stride = width * 3
};

// True values from a simulated patient, carried with the frame they belong
// to so measured and true can be compared on one clock.
struct Truth {
    double heart_rate_bpm = 0.0;
    double breathing_rate_bpm = 0.0;
};

struct Frame {
    std::uint16_t camera = 0;
    std::uint64_t seq = 0;           // per camera, from 0; a jump means dropped frames
    std::int64_t timestamp_ns = 0;   // taken at the source, carried end to end
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    PixelFormat format = PixelFormat::Rgb24;
    std::vector<std::uint8_t> data;
    std::optional<Truth> truth;      // simulated sources only

    double timestamp_s() const { return static_cast<double>(timestamp_ns) * 1e-9; }
};

class FrameSource {
public:
    virtual ~FrameSource() = default;

    // Fills `out` with the next frame. Returns false at the end of the
    // stream or on an error; error() then says which.
    virtual bool next(Frame& out) = 0;

    // Empty at a clean end of stream.
    virtual std::string error() const { return {}; }

    virtual std::string describe() const = 0;
};

}  // namespace vitals::sources
