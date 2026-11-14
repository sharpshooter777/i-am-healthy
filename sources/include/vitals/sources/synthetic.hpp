// SPDX-License-Identifier: Apache-2.0
//
// A synthetic face camera: a patch of skin whose colour changes with a
// simulated pulse, under slowly drifting light, with sensor noise and uneven
// frame timing. It stands in for the Isaac Sim frame server in tests and
// carries the true heart rate with every frame.
//
// The skin colour and pulse amplitudes are plausible round numbers, not
// measurements.
#pragma once

#include <cstdint>
#include <random>

#include "vitals/sources/frame.hpp"

namespace vitals::sources {

struct SyntheticConfig {
    std::uint16_t camera = 0;
    std::uint16_t width = 64;
    std::uint16_t height = 64;
    double fps = 30.0;
    double seconds = 60.0;

    // Heart rate starts here and changes linearly.
    double hr_start_bpm = 72.0;
    double hr_slope_bpm_per_min = 0.0;
    double breathing_rate_bpm = 14.0;

    double pulse_strength = 1.0;    // scales the colour change of each beat
    double noise_level = 2.0;       // per-pixel sensor noise, standard deviation in 8-bit levels
    double light_drift = 0.03;      // fractional amplitude of a slow lighting change
    double timing_jitter_ms = 1.0;  // standard deviation of frame timestamp jitter
    double drop_probability = 0.0;  // chance that any frame is dropped

    std::uint32_t seed = 1;
};

class SyntheticSource : public FrameSource {
public:
    explicit SyntheticSource(const SyntheticConfig& cfg);

    bool next(Frame& out) override;
    std::string describe() const override;

    // True heart rate at session time t (seconds).
    double heart_rate_at(double t) const;

private:
    double pulse_phase(double t) const;

    SyntheticConfig cfg_;
    std::mt19937 rng_;
    std::uint64_t index_ = 0;  // frame slot, counting dropped ones
    std::uint64_t seq_ = 0;    // emitted frames
    std::uint64_t total_ = 0;
};

}  // namespace vitals::sources
