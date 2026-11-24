// SPDX-License-Identifier: Apache-2.0
// Strap and camera together. The strap samples and the grids are made up.
#include <doctest.h>

#include <cmath>
#include <random>
#include <vector>

#include "vitals/core/heart_monitor.hpp"

using namespace vitals::core;

namespace {

// A made-up session: a patch of skin with a pulse at `camera_bpm` (strength 0
// for none) and a strap reporting `strap_bpm` once a second until `strap_until_s`.
struct Session {
    HeartMonitor monitor;
    std::mt19937 rng{11};
    std::normal_distribution<double> noise{0.0, 1.0};
    std::size_t frame_index = 0;
    double strap_next_s = 0.0;

    void run(double until_s, double camera_bpm, double pulse_strength, int strap_bpm, double strap_until_s) {
        const GridPulseConfig& c = monitor.config().camera;
        const double two_pi = 6.283185307179586;
        std::vector<double> frame(monitor.frame_values());
        for (;; ++frame_index) {
            const double t = static_cast<double>(frame_index) / c.fs_hz;
            if (t >= until_s) break;
            while (strap_next_s <= t && strap_next_s < strap_until_s) {
                HeartRateSample s;
                s.bpm = strap_bpm;
                s.contact = Contact::ok;
                s.rr_ms = {60000.0 / strap_bpm};
                monitor.strap_sample(strap_next_s, s);
                strap_next_s += 1.0;
            }
            const double s = std::sin(two_pi * camera_bpm / 60.0 * t);
            for (std::size_t row = 0; row < c.rows; ++row) {
                for (std::size_t col = 0; col < c.cols; ++col) {
                    const bool face = row >= 4 && row < 8 && col >= 12 && col < 16;
                    double* p = &frame[(row * c.cols + col) * 3];
                    if (face) {
                        p[0] = 36000.0 * (1.0 + 0.33 * pulse_strength * s) + 40.0 * noise(rng);
                        p[1] = 28000.0 * (1.0 + 0.77 * pulse_strength * s) + 40.0 * noise(rng);
                        p[2] = 24000.0 * (1.0 + 0.53 * pulse_strength * s) + 40.0 * noise(rng);
                    } else {
                        p[0] = 20000.0 + 40.0 * noise(rng);
                        p[1] = 22000.0 + 40.0 * noise(rng);
                        p[2] = 26000.0 + 40.0 * noise(rng);
                    }
                }
            }
            monitor.camera_frame(t, frame.data(), frame.size());
        }
    }
};

}  // namespace

TEST_CASE("heart monitor: strap alone until the camera has a full window") {
    Session s;
    s.run(20.0, 72.0, 0.004, 72, 1e9);
    const FusedHeartRate out = s.monitor.evaluate(20.0);
    CHECK(out.status == FusionStatus::single);
    CHECK(out.source == s.monitor.strap_source());
    CHECK_FALSE(out.alarm_permitted);
    CHECK(out.sources[static_cast<std::size_t>(s.monitor.camera_source())].state == SourceState::absent);
}

TEST_CASE("heart monitor: strap and camera agree, the strap is shown and alarms are permitted") {
    Session s;
    s.run(36.0, 72.0, 0.004, 72, 1e9);
    const FusedHeartRate out = s.monitor.evaluate(36.0);
    CHECK(out.status == FusionStatus::corroborated);
    CHECK(out.source == s.monitor.strap_source());
    CHECK(out.bpm == doctest::Approx(72.0));
    CHECK(out.alarm_permitted);
    CHECK(s.monitor.last_camera().believable);
}

TEST_CASE("heart monitor: strap lost, the camera carries on with alarms held") {
    Session s;
    s.run(46.0, 72.0, 0.004, 72, 36.0);  // strap silent from 36 s
    const FusedHeartRate out = s.monitor.evaluate(46.0);
    CHECK(out.sources[static_cast<std::size_t>(s.monitor.strap_source())].state == SourceState::stale);
    CHECK(out.status == FusionStatus::single);
    CHECK(out.source == s.monitor.camera_source());
    CHECK(out.bpm == doctest::Approx(72.0).epsilon(0.02));
    CHECK_FALSE(out.alarm_permitted);
}

TEST_CASE("heart monitor: a camera that sees no pulse leaves the strap alone") {
    Session s;
    s.run(36.0, 72.0, 0.0, 72, 1e9);
    const FusedHeartRate out = s.monitor.evaluate(36.0);
    CHECK(out.sources[static_cast<std::size_t>(s.monitor.camera_source())].state == SourceState::poor_quality);
    CHECK(out.status == FusionStatus::single);
    CHECK(out.source == s.monitor.strap_source());
    CHECK_FALSE(out.alarm_permitted);
}

TEST_CASE("heart monitor: strap and camera disagree, the strap is shown and alarms are held") {
    Session s;
    s.run(36.0, 96.0, 0.004, 72, 1e9);
    const FusedHeartRate out = s.monitor.evaluate(36.0);
    CHECK(out.status == FusionStatus::disputed);
    CHECK(out.source == s.monitor.strap_source());
    CHECK(out.bpm == doctest::Approx(72.0));
    CHECK_FALSE(out.alarm_permitted);
}

TEST_CASE("heart monitor: both lost means no number") {
    Session s;
    s.run(36.0, 72.0, 0.0, 72, 20.0);
    const FusedHeartRate out = s.monitor.evaluate(36.0);
    CHECK(out.status == FusionStatus::no_source);
    CHECK(out.source == -1);
}
