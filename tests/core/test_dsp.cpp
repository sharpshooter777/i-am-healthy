// SPDX-License-Identifier: Apache-2.0
#include <doctest.h>

#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

#include "vitals/core/image.hpp"
#include "vitals/core/pulse.hpp"
#include "vitals/core/resample.hpp"
#include "vitals/core/spectrum.hpp"

using namespace vitals::core;

namespace {

constexpr double kPi = 3.14159265358979323846;

// Mean skin colour of a face with a pulse at `bpm`, sampled at about `fps`
// with timing jitter, lighting drift and noise on the mean.
std::vector<RgbSample> make_skin(double bpm, double seconds, double fps, std::uint32_t seed,
                                 double noise = 0.05, double jitter_s = 0.002) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> n(0.0, noise);
    std::normal_distribution<double> j(0.0, jitter_s);
    std::vector<RgbSample> out;
    const auto count = static_cast<int>(seconds * fps);
    for (int k = 0; k < count; ++k) {
        const double t = k / fps + (k > 0 ? j(rng) : 0.0);
        const double p = std::sin(2.0 * kPi * bpm / 60.0 * t);
        const double light = 1.0 + 0.03 * std::sin(2.0 * kPi * 0.05 * t);
        out.push_back({t, 185.0 * light * (1 + 0.0025 * p) + n(rng), 135.0 * light * (1 + 0.009 * p) + n(rng),
                       110.0 * light * (1 + 0.005 * p) + n(rng)});
    }
    return out;
}

}  // namespace

TEST_CASE("max_gap and resample_linear") {
    const std::vector<double> t{0.0, 0.1, 0.25, 0.3};
    const std::vector<double> y{0.0, 1.0, 2.5, 3.0};
    CHECK(max_gap(t) == doctest::Approx(0.15));

    std::vector<double> out;
    REQUIRE(resample_linear(t, y, 0.0, 10.0, 4, out));  // 0, 0.1, 0.2, 0.3
    CHECK(out[0] == doctest::Approx(0.0));
    CHECK(out[1] == doctest::Approx(1.0));
    CHECK(out[2] == doctest::Approx(2.0));
    CHECK(out[3] == doctest::Approx(3.0));

    CHECK_FALSE(resample_linear(t, y, 0.0, 10.0, 5, out));  // runs past the end
    CHECK(out.empty());
    CHECK_FALSE(resample_linear({0.0, 0.0}, {1.0, 2.0}, 0.0, 10.0, 1, out));  // not increasing
}

TEST_CASE("find_peak locates a tone between bins") {
    const double fs = 30.0;
    std::vector<double> x(300);
    for (std::size_t i = 0; i < x.size(); ++i) x[i] = std::sin(2.0 * kPi * 1.234 * static_cast<double>(i) / fs);
    const auto s = power_spectrum(x, fs, 4096);
    const auto p = find_peak(s, 0.7, 3.0, 0.1);
    REQUIRE(p.valid);
    CHECK(p.hz == doctest::Approx(1.234).epsilon(0.002));
    CHECK(p.fraction > 0.9);
}

TEST_CASE("find_peak ignores components outside the band") {
    const double fs = 30.0;
    std::vector<double> x(300);
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double t = static_cast<double>(i) / fs;
        x[i] = 5.0 * std::sin(2.0 * kPi * 0.2 * t) + std::sin(2.0 * kPi * 1.5 * t);
    }
    const auto p = find_peak(power_spectrum(x, fs, 4096), 0.7, 3.0, 0.1);
    REQUIRE(p.valid);
    CHECK(p.hz == doctest::Approx(1.5).epsilon(0.01));
}

TEST_CASE("mean_rgb averages and clips the region") {
    // 2x2 image: red, green / blue, white.
    const std::uint8_t img[] = {255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255};
    const auto all = mean_rgb(img, 2, 2, 6, {0, 0, 2, 2});
    REQUIRE(all.valid);
    CHECK(all.r == doctest::Approx(127.5));
    CHECK(all.g == doctest::Approx(127.5));
    CHECK(all.b == doctest::Approx(127.5));

    const auto clipped = mean_rgb(img, 2, 2, 6, {1, -5, 10, 10});  // right column: green, white
    REQUIRE(clipped.valid);
    CHECK(clipped.r == doctest::Approx(127.5));
    CHECK(clipped.g == doctest::Approx(255.0));
    CHECK(clipped.b == doctest::Approx(127.5));
    CHECK_FALSE(mean_rgb(img, 2, 2, 6, {5, 5, 1, 1}).valid);
}

TEST_CASE("heart rate within 3 bpm across the range") {
    for (double bpm : {48.0, 60.0, 72.0, 95.0, 120.0, 160.0}) {
        CAPTURE(bpm);
        const auto hr = estimate_heart_rate(make_skin(bpm, 12.0, 30.0, static_cast<std::uint32_t>(bpm)));
        REQUIRE(hr.ok());
        CHECK(std::fabs(hr.bpm - bpm) < 3.0);
        CHECK(hr.quality > 0.5);
    }
}

TEST_CASE("heart rate at a different camera rate") {
    const auto hr = estimate_heart_rate(make_skin(80.0, 15.0, 15.0, 7));
    REQUIRE(hr.ok());
    CHECK(std::fabs(hr.bpm - 80.0) < 3.0);
}

TEST_CASE("estimate_heart_rate reports why it cannot estimate") {
    CHECK(estimate_heart_rate(make_skin(72.0, 3.0, 30.0, 1)).status == PulseStatus::TooShort);

    auto gappy = make_skin(72.0, 12.0, 30.0, 1);
    for (auto& s : gappy) {
        if (s.t > 5.0) s.t += 1.0;  // one second of frames missing
    }
    CHECK(estimate_heart_rate(gappy).status == PulseStatus::Gap);
}

TEST_CASE("noise alone gives low quality") {
    std::mt19937 rng(3);
    std::normal_distribution<double> n(0.0, 0.5);
    std::vector<RgbSample> s;
    for (int k = 0; k < 360; ++k) s.push_back({k / 30.0, 180 + n(rng), 130 + n(rng), 110 + n(rng)});
    const auto hr = estimate_heart_rate(s);
    REQUIRE(hr.ok());
    CHECK(hr.quality < 0.5);
}

TEST_CASE("PulseTracker keeps a sliding window") {
    PulseTracker tracker(10.0);
    for (const auto& s : make_skin(66.0, 30.0, 30.0, 11)) tracker.push(s);
    CHECK(tracker.span_s() <= 10.0);
    CHECK(tracker.span_s() > 9.9);
    const auto hr = tracker.estimate();
    REQUIRE(hr.ok());
    CHECK(std::fabs(hr.bpm - 66.0) < 3.0);

    tracker.push({0.0, 1, 1, 1});  // older than the window: dropped
    CHECK(tracker.span_s() > 9.9);
}
