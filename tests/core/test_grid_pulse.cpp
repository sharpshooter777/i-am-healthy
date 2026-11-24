// SPDX-License-Identifier: Apache-2.0
// Synthetic grids: a patch of "skin" with a pulse, on a background without one.
#include <doctest.h>

#include <cmath>
#include <random>
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
