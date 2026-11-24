// SPDX-License-Identifier: Apache-2.0
//
// The heart-rate monitor as one object: a chest strap and a camera, each
// judged on its own, then combined by the five rules in fusion.hpp. Strap
// samples and camera frames go in with their times; the combined reading
// comes out, with the source in use and whether an alarm is permitted.
//
// The same object serves a live session and a replayed one: it has no clock
// of its own. Give both inputs times from one clock.
#pragma once

#include <cstddef>

#include "vitals/core/fusion.hpp"
#include "vitals/core/grid_pulse.hpp"
#include "vitals/core/heart_rate.hpp"

namespace vitals::core {

struct HeartMonitorConfig {
    GridPulseConfig camera;
    FusionConfig fusion;
    // A camera estimate is made this often. Each one describes the last
    // camera.window_s seconds, so it trails a changing heart rate.
    double camera_hop_s = 5.0;
    // A camera estimate stands until this many hops pass without another.
    double camera_stale_hops = 2.0;
};

class HeartMonitor {
public:
    explicit HeartMonitor(HeartMonitorConfig config = {});

    // One strap sample, received at t_s.
    void strap_sample(double t_s, const HeartRateSample& sample);

    // One camera frame as block-mean colours (see GridPulse::push), taken at
    // t_s. Returns true when this frame completed a new camera estimate.
    bool camera_frame(double t_s, const double* rgb, std::size_t count);

    // The combined reading as of t_s.
    FusedHeartRate evaluate(double t_s) const { return fusion_.evaluate(t_s); }

    const GridPulseEstimate& last_camera() const { return last_camera_; }
    int strap_source() const { return strap_; }
    int camera_source() const { return camera_; }
    std::size_t frame_values() const { return grid_.frame_values(); }
    const HeartMonitorConfig& config() const { return config_; }

private:
    HeartMonitorConfig config_;
    HeartRateFusion fusion_;
    GridPulse grid_;
    int strap_ = -1;
    int camera_ = -1;
    std::size_t hop_frames_ = 1;
    std::size_t frames_ = 0;
    GridPulseEstimate last_camera_;
};

}  // namespace vitals::core
