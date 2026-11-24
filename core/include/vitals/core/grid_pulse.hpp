// SPDX-License-Identifier: Apache-2.0
//
// Heart rate from a camera without knowing where the face is. The frame is
// reduced to a coarse grid of block-mean colours. Every block is tried as a
// pulse sensor: the POS method turns its colour history into a pulse signal,
// and the blocks whose spectra show one clean peak, and look like skin, are
// combined. Blocks that keep doing well are preferred, so the choice settles
// on the face and follows it slowly if the patient shifts.
//
// The estimate carries its own verdict. A camera that cannot see a pulse
// must say so, not report the strongest noise: `believable` is that verdict,
// and it is what the combining stage (fusion.hpp) takes as the quality check.
#pragma once

#include <cstddef>
#include <vector>

namespace vitals::core {

struct GridPulseConfig {
    std::size_t cols = 32;
    std::size_t rows = 18;
    double fs_hz = 30.0;            // frames arrive evenly at this rate
    double window_s = 30.0;         // history each estimate is made from
    double pos_window_s = 1.6;
    double min_hz = 0.75;           // 45 bpm
    double max_hz = 3.0;            // 180 bpm
    double peak_halfwidth_hz = 0.1;
    std::size_t min_nfft = 8192;
    std::size_t top_blocks = 8;     // blocks combined into the estimate
    double skin_red_over_blue = 1.15;  // a block counts as skin when mean red exceeds mean blue by this factor
    double score_memory = 0.8;      // how strongly past performance outweighs the current window
    // The verdict. The peak must hold this share of the combined in-band
    // power, and this many of the combined blocks must show the same rate on
    // their own (within agree_hz). Set from two ten-minute stretches of a
    // seated reference recording checked against a chest strap: with these
    // values about a quarter of the estimates are believed and about nine in
    // ten of those are within 3 bpm. The rest are for the combining stage to
    // catch, which is why the camera never stands alone.
    double min_quality = 0.25;
    std::size_t min_agreeing = 4;
    double agree_hz = 0.05;         // 3 bpm
    // Slow body movement puts power at the bottom of the band. A peak this
    // close to min_hz is not believed.
    double edge_guard_hz = 0.10;
};

struct GridPulseEstimate {
    bool ready = false;        // a full window of frames has been seen
    bool believable = false;   // the verdict: use this reading or not
    double bpm = 0.0;
    double quality = 0.0;      // share of combined in-band power at the peak, 0..1
    std::size_t agreeing = 0;  // combined blocks whose own peak matches
    std::size_t skin_blocks = 0;
    std::vector<std::size_t> blocks;  // indices combined, row-major
};

class GridPulse {
public:
    explicit GridPulse(GridPulseConfig config = {});

    std::size_t blocks() const { return config_.cols * config_.rows; }
    std::size_t frame_values() const { return blocks() * 3; }

    // One frame: mean red, green, blue for each block, row-major, any
    // positive scale. `count` must equal frame_values(); other frames are ignored.
    void push(const double* rgb, std::size_t count);

    // The estimate from the most recent window. Call it at a steady pace
    // (every few seconds): each call also updates the blocks' track records.
    GridPulseEstimate estimate();

    const GridPulseConfig& config() const { return config_; }

private:
    GridPulseConfig config_;
    std::size_t window_ = 0;            // frames in one window
    std::size_t filled_ = 0;
    std::size_t head_ = 0;              // next write position in the ring
    std::vector<double> ring_;          // window_ frames of frame_values()
    std::vector<double> score_;         // per block, running peak sharpness
    bool have_score_ = false;
};

}  // namespace vitals::core
