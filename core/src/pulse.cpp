// SPDX-License-Identifier: Apache-2.0
#include "vitals/core/pulse.hpp"

#include <cmath>

#include "vitals/core/resample.hpp"
#include "vitals/core/spectrum.hpp"

namespace vitals::core {

namespace {

double mean_of(const double* x, std::size_t n) {
    double s = 0.0;
    for (std::size_t i = 0; i < n; ++i) s += x[i];
    return s / static_cast<double>(n);
}

double stddev_of(const std::vector<double>& x) {
    const double m = mean_of(x.data(), x.size());
    double s = 0.0;
    for (double v : x) s += (v - m) * (v - m);
    return std::sqrt(s / static_cast<double>(x.size()));
}

}  // namespace

const char* to_string(PulseStatus s) {
    switch (s) {
        case PulseStatus::Ok: return "ok";
        case PulseStatus::TooShort: return "too short";
        case PulseStatus::Gap: return "gap in frames";
        case PulseStatus::BadInput: return "bad input";
        case PulseStatus::NoPeak: return "no peak";
    }
    return "unknown";
}

std::vector<double> pos_pulse(const std::vector<double>& r, const std::vector<double>& g,
                              const std::vector<double>& b, std::size_t window) {
    const std::size_t n = r.size();
    if (window < 2 || g.size() != n || b.size() != n || n < window) return {};

    std::vector<double> h(n, 0.0);
    std::vector<double> s1(window), s2(window);
    for (std::size_t start = 0; start + window <= n; ++start) {
        const double mr = mean_of(&r[start], window);
        const double mg = mean_of(&g[start], window);
        const double mb = mean_of(&b[start], window);
        if (mr <= 0.0 || mg <= 0.0 || mb <= 0.0) continue;

        // Temporal normalisation removes the skin tone and slow lighting
        // changes; the projection plane is orthogonal to the skin tone.
        for (std::size_t k = 0; k < window; ++k) {
            const double cr = r[start + k] / mr;
            const double cg = g[start + k] / mg;
            const double cb = b[start + k] / mb;
            s1[k] = cg - cb;
            s2[k] = -2.0 * cr + cg + cb;
        }
        const double sd2 = stddev_of(s2);
        const double alpha = sd2 > 0.0 ? stddev_of(s1) / sd2 : 0.0;

        double hm = 0.0;
        for (std::size_t k = 0; k < window; ++k) hm += s1[k] + alpha * s2[k];
        hm /= static_cast<double>(window);
        for (std::size_t k = 0; k < window; ++k) h[start + k] += s1[k] + alpha * s2[k] - hm;
    }
    return h;
}

HeartRate estimate_heart_rate(const std::vector<RgbSample>& samples, const PulseConfig& cfg) {
    HeartRate hr;
    if (samples.size() < 2) return hr;

    const double span = samples.back().t - samples.front().t;
    if (span < cfg.min_span_s) return hr;

    std::vector<double> t, r, g, b;
    t.reserve(samples.size());
    r.reserve(samples.size());
    g.reserve(samples.size());
    b.reserve(samples.size());
    for (const auto& s : samples) {
        t.push_back(s.t);
        r.push_back(s.r);
        g.push_back(s.g);
        b.push_back(s.b);
    }
    if (max_gap(t) > cfg.max_gap_s) {
        hr.status = PulseStatus::Gap;
        return hr;
    }

    const auto n = static_cast<std::size_t>(std::floor(span * cfg.fs_hz)) + 1;
    std::vector<double> ur, ug, ub;
    if (!resample_linear(t, r, t.front(), cfg.fs_hz, n, ur) ||
        !resample_linear(t, g, t.front(), cfg.fs_hz, n, ug) ||
        !resample_linear(t, b, t.front(), cfg.fs_hz, n, ub)) {
        hr.status = PulseStatus::BadInput;
        return hr;
    }

    const auto window = static_cast<std::size_t>(std::lround(cfg.pos_window_s * cfg.fs_hz));
    const std::vector<double> pulse = pos_pulse(ur, ug, ub, window);
    if (pulse.empty()) {
        hr.status = PulseStatus::BadInput;
        return hr;
    }

    const Spectrum spec = power_spectrum(pulse, cfg.fs_hz, cfg.min_nfft);
    const Peak peak = find_peak(spec, cfg.min_hz, cfg.max_hz, cfg.peak_halfwidth_hz);
    if (!peak.valid) {
        hr.status = PulseStatus::NoPeak;
        return hr;
    }
    hr.status = PulseStatus::Ok;
    hr.bpm = peak.hz * 60.0;
    hr.quality = peak.fraction;
    return hr;
}

PulseTracker::PulseTracker(double window_s, PulseConfig cfg) : window_s_(window_s), cfg_(cfg) {}

void PulseTracker::push(const RgbSample& s) {
    if (!buf_.empty() && !(s.t > buf_.back().t)) return;
    buf_.push_back(s);
    while (!buf_.empty() && buf_.back().t - buf_.front().t > window_s_) buf_.pop_front();
}

HeartRate PulseTracker::estimate() const {
    return estimate_heart_rate(std::vector<RgbSample>(buf_.begin(), buf_.end()), cfg_);
}

double PulseTracker::span_s() const {
    return buf_.size() < 2 ? 0.0 : buf_.back().t - buf_.front().t;
}

}  // namespace vitals::core
