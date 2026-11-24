// SPDX-License-Identifier: Apache-2.0
#include "vitals/core/grid_pulse.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "vitals/core/pulse.hpp"
#include "vitals/core/spectrum.hpp"

namespace vitals::core {

GridPulse::GridPulse(GridPulseConfig config) : config_(config) {
    window_ = static_cast<std::size_t>(std::lround(config_.window_s * config_.fs_hz));
    ring_.assign(window_ * frame_values(), 0.0);
    score_.assign(blocks(), 0.0);
}

void GridPulse::push(const double* rgb, std::size_t count) {
    if (rgb == nullptr || count != frame_values() || window_ == 0) return;
    std::copy(rgb, rgb + count, ring_.begin() + static_cast<std::ptrdiff_t>(head_ * count));
    head_ = (head_ + 1) % window_;
    if (filled_ < window_) ++filled_;
}

GridPulseEstimate GridPulse::estimate() {
    GridPulseEstimate out;
    if (filled_ < window_) return out;
    out.ready = true;

    const std::size_t nblocks = blocks();
    const std::size_t per_frame = frame_values();
    const auto pos_window = static_cast<std::size_t>(std::lround(config_.pos_window_s * config_.fs_hz));

    struct Block {
        bool usable = false;
        double sharpness = 0.0;      // share of its in-band power at its own peak
        double peak_hz = 0.0;
        std::vector<double> band;    // in-band spectrum, normalised to sum 1
    };
    std::vector<Block> info(nblocks);
    std::size_t lo_bin = 0, hi_bin = 0;
    double bin_hz = 0.0;

    std::vector<double> r(window_), g(window_), b(window_);
    for (std::size_t k = 0; k < nblocks; ++k) {
        double sr = 0.0, sg = 0.0, sb = 0.0;
        for (std::size_t i = 0; i < window_; ++i) {
            const double* p = &ring_[((head_ + i) % window_) * per_frame + k * 3];  // oldest first
            r[i] = p[0];
            g[i] = p[1];
            b[i] = p[2];
            sr += p[0];
            sg += p[1];
            sb += p[2];
        }
        if (!(sr > 0.0 && sg > 0.0 && sb > 0.0)) continue;         // black or unset
        if (!(sr > config_.skin_red_over_blue * sb)) continue;     // not skin-coloured
        ++out.skin_blocks;

        const std::vector<double> pulse = pos_pulse(r, g, b, pos_window);
        if (pulse.empty()) continue;
        const Spectrum sp = power_spectrum(pulse, config_.fs_hz, config_.min_nfft);
        const Peak pk = find_peak(sp, config_.min_hz, config_.max_hz, config_.peak_halfwidth_hz);
        if (!pk.valid) continue;

        if (bin_hz == 0.0) {
            bin_hz = sp.bin_hz;
            lo_bin = static_cast<std::size_t>(std::ceil(config_.min_hz / bin_hz));
            hi_bin = std::min(sp.power.size() - 1, static_cast<std::size_t>(std::floor(config_.max_hz / bin_hz)));
        }
        double total = 0.0;
        for (std::size_t i = lo_bin; i <= hi_bin; ++i) total += sp.power[i];
        if (!(total > 0.0)) continue;

        Block& blk = info[k];
        blk.usable = true;
        blk.sharpness = pk.fraction;
        blk.peak_hz = pk.hz;
        blk.band.resize(hi_bin - lo_bin + 1);
        for (std::size_t i = lo_bin; i <= hi_bin; ++i) blk.band[i - lo_bin] = sp.power[i] / total;
    }

    // Rank by track record; on the first window, by the window itself.
    std::vector<std::size_t> order;
    for (std::size_t k = 0; k < nblocks; ++k) {
        if (info[k].usable) order.push_back(k);
    }
    const auto rank_value = [&](std::size_t k) { return have_score_ ? score_[k] : info[k].sharpness; };
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t c) {
        const double va = rank_value(a), vc = rank_value(c);
        return va != vc ? va > vc : a < c;
    });
    if (order.size() > config_.top_blocks) order.resize(config_.top_blocks);

    // Track records move after the ranking, so this window is judged by the past.
    for (std::size_t k = 0; k < nblocks; ++k) {
        const double now = info[k].usable ? info[k].sharpness : 0.0;
        score_[k] = have_score_ ? config_.score_memory * score_[k] + (1.0 - config_.score_memory) * now : now;
    }
    have_score_ = true;

    if (order.empty() || bin_hz == 0.0) return out;

    Spectrum combined;
    combined.bin_hz = bin_hz;
    combined.power.assign(hi_bin + 2, 0.0);
    for (std::size_t k : order) {
        for (std::size_t i = 0; i < info[k].band.size(); ++i) combined.power[lo_bin + i] += info[k].band[i];
    }
    const Peak pk = find_peak(combined, config_.min_hz, config_.max_hz, config_.peak_halfwidth_hz);
    if (!pk.valid) return out;

    out.blocks = order;
    out.bpm = pk.hz * 60.0;
    out.quality = pk.fraction;
    for (std::size_t k : order) {
        if (std::fabs(info[k].peak_hz - pk.hz) <= config_.agree_hz) ++out.agreeing;
    }
    out.believable = out.quality >= config_.min_quality && out.agreeing >= config_.min_agreeing &&
                     pk.hz >= config_.min_hz + config_.edge_guard_hz;
    return out;
}

}  // namespace vitals::core
