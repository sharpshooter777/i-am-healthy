// SPDX-License-Identifier: Apache-2.0
#include "vitals/sources/synthetic.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace vitals::sources {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Base skin colour and the fractional colour change per beat in each
// channel. Blood absorbs green most strongly, so green carries the pulse.
constexpr double kSkin[3] = {185.0, 135.0, 110.0};
constexpr double kPulseAmp[3] = {0.0025, 0.0090, 0.0050};

std::uint8_t to_u8(double v) {
    return static_cast<std::uint8_t>(std::clamp(std::lround(v), 0L, 255L));
}

}  // namespace

SyntheticSource::SyntheticSource(const SyntheticConfig& cfg)
    : cfg_(cfg), rng_(cfg.seed),
      total_(static_cast<std::uint64_t>(std::llround(cfg.seconds * cfg.fps))) {}

double SyntheticSource::heart_rate_at(double t) const {
    return cfg_.hr_start_bpm + cfg_.hr_slope_bpm_per_min * t / 60.0;
}

double SyntheticSource::pulse_phase(double t) const {
    // Integral of 2*pi*f(t) with f = heart_rate_at(t) / 60.
    return 2.0 * kPi * (cfg_.hr_start_bpm * t / 60.0 + cfg_.hr_slope_bpm_per_min * t * t / 7200.0);
}

bool SyntheticSource::next(Frame& out) {
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    std::normal_distribution<double> jitter(0.0, cfg_.timing_jitter_ms * 1e-3);
    std::normal_distribution<double> noise(0.0, cfg_.noise_level);

    while (index_ < total_ && cfg_.drop_probability > 0.0 && uniform(rng_) < cfg_.drop_probability) {
        ++index_;  // dropped: the slot passes with no frame
    }
    if (index_ >= total_) return false;

    double t = static_cast<double>(index_) / cfg_.fps;
    if (cfg_.timing_jitter_ms > 0.0 && index_ > 0) t += jitter(rng_);
    ++index_;

    // Pulse wave: a fundamental with a weaker second harmonic for a sharper
    // systolic rise. Lighting drifts slowly and affects all channels alike.
    const double phase = pulse_phase(t);
    const double wave = std::sin(phase) + 0.3 * std::sin(2.0 * phase + 0.6);
    const double light = 1.0 + cfg_.light_drift * std::sin(2.0 * kPi * 0.03 * t);

    out.camera = cfg_.camera;
    out.seq = seq_++;
    out.timestamp_ns = static_cast<std::int64_t>(std::llround(t * 1e9));
    out.width = cfg_.width;
    out.height = cfg_.height;
    out.format = PixelFormat::Rgb24;
    out.data.resize(static_cast<std::size_t>(cfg_.width) * cfg_.height * 3);
    out.truth = Truth{heart_rate_at(t), cfg_.breathing_rate_bpm};

    double level[3];
    for (int c = 0; c < 3; ++c) {
        level[c] = kSkin[c] * light * (1.0 + cfg_.pulse_strength * kPulseAmp[c] * wave);
    }

    std::size_t i = 0;
    for (std::uint16_t y = 0; y < cfg_.height; ++y) {
        for (std::uint16_t x = 0; x < cfg_.width; ++x) {
            // A fixed, gentle shading across the patch, like a curved face.
            const double shade = 1.0 - 0.08 * std::hypot(x - cfg_.width / 2.0, y - cfg_.height / 2.0) / cfg_.width;
            for (int c = 0; c < 3; ++c) {
                out.data[i++] = to_u8(level[c] * shade + noise(rng_));
            }
        }
    }
    return true;
}

std::string SyntheticSource::describe() const {
    std::ostringstream s;
    s << "synthetic camera " << cfg_.camera << ", " << cfg_.width << "x" << cfg_.height << " at " << cfg_.fps
      << " fps, " << cfg_.seconds << " s, heart rate " << cfg_.hr_start_bpm << " bpm";
    if (cfg_.hr_slope_bpm_per_min != 0.0) s << " changing " << cfg_.hr_slope_bpm_per_min << " bpm/min";
    return s.str();
}

}  // namespace vitals::sources
