// SPDX-License-Identifier: Apache-2.0
#include "vitals/core/heart_monitor.hpp"

#include <algorithm>
#include <cmath>

namespace vitals::core {

HeartMonitor::HeartMonitor(HeartMonitorConfig config)
    : config_(config), fusion_(config.fusion), grid_(config.camera) {
    // The strap is the more trusted source: it measures the heart's
    // electrical signal directly. The camera is there to check it and to
    // carry on, alarms held, when the strap is lost.
    strap_ = fusion_.add_source("chest strap", 1);
    camera_ = fusion_.add_source("camera", 2, config_.camera_hop_s * config_.camera_stale_hops);
    hop_frames_ = static_cast<std::size_t>(
        std::max(1L, std::lround(config_.camera_hop_s * config_.camera.fs_hz)));
}

void HeartMonitor::strap_sample(double t_s, const HeartRateSample& sample) {
    fusion_.update(strap_, t_s, static_cast<double>(sample.bpm), strap_quality_ok(sample));
}

bool HeartMonitor::camera_frame(double t_s, const double* rgb, std::size_t count) {
    if (rgb == nullptr || count != grid_.frame_values()) return false;
    grid_.push(rgb, count);
    ++frames_;
    if (frames_ % hop_frames_ != 0) return false;
    const GridPulseEstimate e = grid_.estimate();
    if (!e.ready) return false;
    last_camera_ = e;
    // The camera's verdict is its quality check (rule 2).
    fusion_.update(camera_, t_s, e.bpm, e.believable);
    return true;
}

void HeartMonitor::camera_restart() {
    grid_ = GridPulse(config_.camera);
    frames_ = 0;
}

}  // namespace vitals::core
