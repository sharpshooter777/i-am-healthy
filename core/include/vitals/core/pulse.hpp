// SPDX-License-Identifier: Apache-2.0
//
// Heart rate from the mean skin colour of a face region (remote
// photoplethysmography). Each heartbeat changes skin colour by about one
// percent; the POS method (Wang et al., "Algorithmic Principles of Remote
// PPG", IEEE TBME 2017) separates that change from lighting and motion, and
// the strongest spectral peak between 0.7 and 3 Hz gives the rate.
#pragma once

#include <cstddef>
#include <deque>
#include <vector>

namespace vitals::core {

// Mean skin colour at time t (seconds, from the frame's source timestamp).
struct RgbSample {
    double t = 0.0;
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
};

struct PulseConfig {
    double fs_hz = 30.0;          // uniform rate the samples are resampled to
    double pos_window_s = 1.6;    // POS projection window
    double min_hz = 0.7;          // 42 bpm
    double max_hz = 3.0;          // 180 bpm
    double max_gap_s = 0.5;       // a longer gap between frames invalidates the window
    double min_span_s = 5.0;      // shortest window worth estimating from
    double peak_halfwidth_hz = 0.1;
    std::size_t min_nfft = 4096;  // zero-padding for a fine frequency grid
};

// POS pulse signal from evenly sampled colour channels, using overlapping
// windows of `window` samples. Returns an empty vector if the inputs differ
// in length or are shorter than one window.
std::vector<double> pos_pulse(const std::vector<double>& r, const std::vector<double>& g,
                              const std::vector<double>& b, std::size_t window);

enum class PulseStatus {
    Ok,
    TooShort,   // less than min_span_s of samples
    Gap,        // frames missing for longer than max_gap_s
    BadInput,   // timestamps not increasing, or colour not positive
    NoPeak,     // nothing in the heart-rate band
};

const char* to_string(PulseStatus s);

struct HeartRate {
    PulseStatus status = PulseStatus::TooShort;
    double bpm = 0.0;
    double quality = 0.0;  // 0..1, share of in-band power at the peak

    bool ok() const { return status == PulseStatus::Ok; }
};

// Estimates heart rate from samples in time order.
HeartRate estimate_heart_rate(const std::vector<RgbSample>& samples, const PulseConfig& cfg = {});

// Keeps the most recent window of samples and estimates on demand.
class PulseTracker {
public:
    explicit PulseTracker(double window_s = 10.0, PulseConfig cfg = {});

    // Samples must arrive in time order; an older or repeated timestamp is dropped.
    void push(const RgbSample& s);
    HeartRate estimate() const;

    double span_s() const;
    void clear() { buf_.clear(); }

private:
    double window_s_;
    PulseConfig cfg_;
    std::deque<RgbSample> buf_;
};

}  // namespace vitals::core
