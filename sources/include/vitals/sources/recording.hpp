// SPDX-License-Identifier: Apache-2.0
//
// Recorded sessions: frames saved to disk in one packet format and replayed
// as a frame source. Recordings are the test fixtures, the fallback demo,
// and the way to run nine cameras when they cannot be rendered live.
//
// File layout, all integers little-endian:
//   header   "VMREC\0\0\0" (8 bytes), u32 version = 1
//   frame    u8 type = 1, u16 camera, u64 seq, i64 timestamp_ns,
//            u16 width, u16 height, u8 format, u8 flags (bit 0: truth present),
//            [f64 heart_rate_bpm, f64 breathing_rate_bpm if truth],
//            u32 payload bytes, payload
#pragma once

#include <cstdint>
#include <fstream>
#include <string>

#include "vitals/sources/frame.hpp"

namespace vitals::sources {

class RecordingWriter {
public:
    explicit RecordingWriter(const std::string& path);

    bool ok() const { return ok_; }
    bool write(const Frame& f);
    bool close();

private:
    std::ofstream out_;
    bool ok_ = false;
};

class RecordingReader : public FrameSource {
public:
    explicit RecordingReader(const std::string& path);

    bool next(Frame& out) override;
    std::string error() const override { return error_; }
    std::string describe() const override { return "recording " + path_; }

private:
    std::string path_;
    std::ifstream in_;
    std::string error_;
};

}  // namespace vitals::sources
