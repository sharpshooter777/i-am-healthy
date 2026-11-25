// SPDX-License-Identifier: Apache-2.0
// Synthetic grids: a patch of "skin" with a pulse, on a background without one.
#include <doctest.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <string>
#include <vector>

#include "vitals/core/grid_pulse.hpp"

using namespace vitals::core;

namespace {

// Fills a GridPulse with `seconds` of frames. Blocks in `face` are skin and
// carry a pulse of the given strength at `bpm`; the rest are a bluish wall.
// With pulse_strength 0 nothing carries a pulse.
void feed(GridPulse& grid, double seconds, double bpm, double pulse_strength, unsigned seed = 7) {
    const GridPulseConfig& c = grid.config();
    std::mt19937 rng(seed);
    std::normal_distribution<double> noise(0.0, 1.0);
    const double two_pi = 6.283185307179586;
    std::vector<double> frame(grid.frame_values());
    const auto n = static_cast<std::size_t>(seconds * c.fs_hz);
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / c.fs_hz;
        const double s = std::sin(two_pi * bpm / 60.0 * t);
        for (std::size_t row = 0; row < c.rows; ++row) {
            for (std::size_t col = 0; col < c.cols; ++col) {
                const bool face = row >= 4 && row < 8 && col >= 12 && col < 16;
                double* p = &frame[(row * c.cols + col) * 3];
                if (face) {
                    // Blood absorbs green most, then blue, then red.
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
        grid.push(frame.data(), frame.size());
    }
}

// Like feed(), but the face is a 4x4-block patch that bobs up and down by
// `move_blocks` at `move_hz`, as a head does on a treadmill. Each block's
// colour is the mix of face and wall it covers, so as the face moves its
// blocks change colour with every step.
void feed_moving(GridPulse& grid, double seconds, double bpm, double pulse_strength, double move_hz,
                 double move_blocks, unsigned seed = 7) {
    const GridPulseConfig& c = grid.config();
    std::mt19937 rng(seed);
    std::normal_distribution<double> noise(0.0, 1.0);
    const double two_pi = 6.283185307179586;
    std::vector<double> frame(grid.frame_values());
    const auto n = static_cast<std::size_t>(seconds * c.fs_hz);
    for (std::size_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(i) / c.fs_hz;
        const double s = std::sin(two_pi * bpm / 60.0 * t);
        const double lift = std::sin(two_pi * move_hz * t);
        const double top = 4.0 + move_blocks * lift;  // face covers rows [top, top + 4)
        const double wall[3] = {20000.0, 22000.0, 26000.0};
        for (std::size_t row = 0; row < c.rows; ++row) {
            const double r0 = static_cast<double>(row);
            // The face is four bands one block high (forehead, eyes, cheeks,
            // mouth), each a slightly different colour and brightness.
            static const double brightness[4] = {1.10, 0.80, 1.05, 0.90};
            double mix[3] = {0.0, 0.0, 0.0};
            double covered = 0.0;
            for (int band = 0; band < 4; ++band) {
                const double b0 = top + band;
                const double f = std::max(0.0, std::min(r0 + 1.0, b0 + 1.0) - std::max(r0, b0));
                const double j = band - 1.5;
                const double w = f * brightness[band];
                mix[0] += w * 36000.0 * (1.0 + 0.04 * j) * (1.0 + 0.33 * pulse_strength * s);
                mix[1] += w * 28000.0 * (1.0 - 0.05 * j) * (1.0 + 0.77 * pulse_strength * s);
                mix[2] += w * 24000.0 * (1.0 + 0.02 * j) * (1.0 + 0.53 * pulse_strength * s);
                covered += f;
            }
            for (std::size_t col = 0; col < c.cols; ++col) {
                const bool face_col = col >= 12 && col < 16;
                const double f = face_col ? covered : 0.0;
                double* p = &frame[(row * c.cols + col) * 3];
                for (int ch = 0; ch < 3; ++ch) {
                    p[ch] = (face_col ? mix[ch] : 0.0) + (1.0 - f) * wall[ch] + 40.0 * noise(rng);
                }
            }
        }
        grid.push(frame.data(), frame.size());
    }
}

}  // namespace

TEST_CASE("grid pulse: not ready before a full window") {
    GridPulse grid;
    feed(grid, 10.0, 72.0, 0.004);
    const GridPulseEstimate e = grid.estimate();
    CHECK_FALSE(e.ready);
    CHECK_FALSE(e.believable);
}

TEST_CASE("grid pulse: finds the face and reads its rate") {
    GridPulse grid;
    feed(grid, 31.0, 72.0, 0.004);
    const GridPulseEstimate e = grid.estimate();
    REQUIRE(e.ready);
    CHECK(e.believable);
    CHECK(e.bpm == doctest::Approx(72.0).epsilon(0.02));
    CHECK(e.skin_blocks == 16);
    CHECK(e.agreeing >= 3);
    for (std::size_t k : e.blocks) {  // every block it chose is on the face
        const std::size_t row = k / grid.config().cols, col = k % grid.config().cols;
        CHECK((row >= 4 && row < 8 && col >= 12 && col < 16));
    }
}

TEST_CASE("grid pulse: skin with no pulse is not believed") {
    GridPulse grid;
    feed(grid, 31.0, 72.0, 0.0);
    const GridPulseEstimate e = grid.estimate();
    REQUIRE(e.ready);
    CHECK(e.skin_blocks == 16);
    CHECK_FALSE(e.believable);
}

TEST_CASE("grid pulse: a scene with no skin gives no reading") {
    GridPulseConfig c;
    c.skin_red_over_blue = 5.0;  // nothing in the scene passes
    GridPulse grid(c);
    feed(grid, 31.0, 72.0, 0.004);
    const GridPulseEstimate e = grid.estimate();
    REQUIRE(e.ready);
    CHECK(e.skin_blocks == 0);
    CHECK_FALSE(e.believable);
}

TEST_CASE("grid pulse: frames of the wrong size are ignored") {
    GridPulse grid;
    std::vector<double> bad(10, 1.0);
    grid.push(bad.data(), bad.size());
    grid.push(nullptr, grid.frame_values());
    CHECK_FALSE(grid.estimate().ready);
}

// The movement check. A treadmill runner's steps and strides passed the
// verdict above; these grids reproduce that with a bobbing face.

TEST_CASE("grid pulse: a still face with a pulse is believed and not moving") {
    GridPulse grid;
    feed(grid, 31.0, 72.0, 0.004);
    const GridPulseEstimate e = grid.estimate();
    REQUIRE(e.ready);
    CHECK(e.believable);
    CHECK(e.refusal == GridPulseRefusal::none);
    CHECK_FALSE(e.moving);
    CHECK(e.movement < grid.config().rhythm_min_movement);
}

TEST_CASE("grid pulse: steady movement with no pulse is not believed") {
    // Step rate (2.4 Hz) and stride rate (1.2 Hz), as on the treadmill; a
    // stride sways further than a step bobs.
    struct Movement {
        double hz, blocks;
    };
    for (const Movement m : {Movement{2.4, 0.5}, Movement{1.2, 1.0}}) {
        const double move_hz = m.hz;
        CAPTURE(move_hz);
        // Without the movement check the rhythm of the movement is believed:
        // this is the failure the check is for.
        GridPulseConfig unchecked;
        unchecked.movement_limit = 1e9;
        unchecked.rhythm_limit = 2.0;
        GridPulse blind(unchecked);
        feed_moving(blind, 31.0, 72.0, 0.0, move_hz, m.blocks);
        const GridPulseEstimate b = blind.estimate();
        REQUIRE(b.ready);
        CHECK(b.believable);
        CHECK(b.bpm / 60.0 == doctest::Approx(move_hz).epsilon(0.05));

        GridPulse grid;
        feed_moving(grid, 31.0, 72.0, 0.0, move_hz, m.blocks);
        const GridPulseEstimate e = grid.estimate();
        REQUIRE(e.ready);
        CHECK_FALSE(e.believable);
        CHECK(e.moving);
        CHECK(e.refusal == GridPulseRefusal::moving_at_pulse_rate);
        CHECK(e.movement_hz == doctest::Approx(move_hz).epsilon(0.05));
    }
}

TEST_CASE("grid pulse: moderate rhythmic movement at the candidate's rate is not believed") {
    // Below movement_limit, but regular, and the peak is the movement's.
    GridPulse grid;
    feed_moving(grid, 31.0, 72.0, 0.0, 2.4, 0.2);
    const GridPulseEstimate e = grid.estimate();
    REQUIRE(e.ready);
    CHECK_FALSE(e.moving);
    CHECK(e.movement >= grid.config().rhythm_min_movement);
    CHECK(e.rhythm >= grid.config().rhythm_limit);
    CHECK_FALSE(e.believable);
    CHECK(e.refusal == GridPulseRefusal::moving_at_pulse_rate);
}

TEST_CASE("grid pulse: strong movement refuses even a pulse at another rate") {
    // Decided from the treadmill recordings: while movement was above the
    // limit, not one believed camera estimate agreed with the chest strap,
    // so no reading taken during strong movement is believed, whatever its
    // rate. The reason tells the two cases apart. The made-up pulse is far
    // stronger than a real one, so that it, not the movement, holds the peak.
    GridPulseConfig unchecked;
    unchecked.movement_limit = 1e9;
    unchecked.rhythm_limit = 2.0;
    GridPulse blind(unchecked);
    feed_moving(blind, 31.0, 72.0, 0.1, 1.7, 0.5);
    CHECK(blind.estimate().believable);  // the cost: a right reading is thrown away

    GridPulse grid;
    feed_moving(grid, 31.0, 72.0, 0.1, 1.7, 0.5);
    const GridPulseEstimate e = grid.estimate();
    REQUIRE(e.ready);
    CHECK_FALSE(e.believable);
    CHECK(e.moving);
    CHECK(e.movement_hz == doctest::Approx(1.7).epsilon(0.05));
    CHECK(e.bpm == doctest::Approx(72.0).epsilon(0.02));
    CHECK(e.refusal == GridPulseRefusal::moving);
}

TEST_CASE("grid pulse: slight movement leaves the verdict exactly as it was") {
    GridPulseConfig unchecked;
    unchecked.movement_limit = 1e9;
    unchecked.rhythm_limit = 2.0;
    GridPulse with_check, without_check(unchecked);
    feed_moving(with_check, 31.0, 72.0, 0.004, 0.3, 0.02);
    feed_moving(without_check, 31.0, 72.0, 0.004, 0.3, 0.02);
    const GridPulseEstimate a = with_check.estimate(), b = without_check.estimate();
    REQUIRE(a.ready);
    CHECK_FALSE(a.moving);
    CHECK(a.believable);
    CHECK(a.believable == b.believable);
    CHECK(a.bpm == b.bpm);
    CHECK(a.quality == b.quality);
    CHECK(a.agreeing == b.agreeing);
    CHECK(a.refusal == b.refusal);
}

TEST_CASE("grid pulse: refusal reasons have names") {
    CHECK(std::string(to_string(GridPulseRefusal::none)) == "believed");
    CHECK(std::string(to_string(GridPulseRefusal::moving)) == "moving");
    CHECK(std::string(to_string(GridPulseRefusal::not_ready)) == "not_ready");
    GridPulse grid;
    CHECK(grid.estimate().refusal == GridPulseRefusal::not_ready);
}
