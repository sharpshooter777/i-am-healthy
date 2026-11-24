// SPDX-License-Identifier: Apache-2.0
#include "vitals/core/fusion.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace vitals::core {

const char* to_string(SourceState s) {
    switch (s) {
        case SourceState::absent: return "absent";
        case SourceState::stale: return "stale";
        case SourceState::poor_quality: return "poor quality";
        case SourceState::out_of_range: return "out of range";
        case SourceState::jumped: return "jumped";
        case SourceState::ok: return "ok";
    }
    return "?";
}

const char* to_string(FusionStatus s) {
    switch (s) {
        case FusionStatus::no_source: return "no source";
        case FusionStatus::single: return "single source";
        case FusionStatus::corroborated: return "corroborated";
        case FusionStatus::disputed: return "disputed";
    }
    return "?";
}

HeartRateFusion::HeartRateFusion(FusionConfig config) : config_(config) {}

int HeartRateFusion::add_source(std::string name, int rank) {
    Source s;
    s.name = std::move(name);
    s.rank = rank;
    sources_.push_back(std::move(s));
    return static_cast<int>(sources_.size()) - 1;
}

void HeartRateFusion::update(int source, double t_s, double bpm, bool quality_ok) {
    if (source < 0 || source >= static_cast<int>(sources_.size())) return;
    Source& s = sources_[static_cast<std::size_t>(source)];
    s.reported = true;
    s.last_t = t_s;
    s.last_bpm = bpm;

    // Rule 2, in order: quality first, then range, then rate of change.
    if (!quality_ok) {
        s.last_state = SourceState::poor_quality;
        return;
    }
    if (!(bpm >= config_.min_bpm && bpm <= config_.max_bpm)) {  // also refuses NaN
        s.last_state = SourceState::out_of_range;
        return;
    }
    if (s.has_accepted) {
        // The allowance grows with the time since the last believable
        // reading, so a real change is accepted once enough time has passed
        // for a heart to have made it; a one-second leap is not.
        const double dt = std::max(0.0, t_s - s.accepted_t);
        const double allowed = config_.slew_floor_bpm + config_.max_slew_bpm_per_s * dt;
        if (std::fabs(bpm - s.accepted_bpm) > allowed) {
            s.last_state = SourceState::jumped;
            return;
        }
    }
    s.has_accepted = true;
    s.accepted_t = t_s;
    s.accepted_bpm = bpm;
    s.last_state = SourceState::ok;
}

FusedHeartRate HeartRateFusion::evaluate(double t_s) const {
    FusedHeartRate out;
    out.sources.reserve(sources_.size());

    // Candidates: connected and believable, as (bpm, index).
    std::vector<std::pair<double, int>> candidates;
    for (std::size_t i = 0; i < sources_.size(); ++i) {
        const Source& s = sources_[i];
        SourceReport r;
        r.name = s.name;
        r.rank = s.rank;
        r.bpm = s.last_bpm;
        if (!s.reported) r.state = SourceState::absent;
        else if (t_s - s.last_t > config_.stale_after_s) r.state = SourceState::stale;
        else r.state = s.last_state;
        if (r.state == SourceState::ok) candidates.emplace_back(s.last_bpm, static_cast<int>(i));
        out.sources.push_back(std::move(r));
    }
    if (candidates.empty()) return out;  // no_source

    const auto rank_of = [&](int index) { return sources_[static_cast<std::size_t>(index)].rank; };
    const auto best_rank_in = [&](std::size_t first, std::size_t last) {  // [first, last)
        int best = candidates[first].second;
        for (std::size_t k = first + 1; k < last; ++k) {
            if (rank_of(candidates[k].second) < rank_of(best)) best = candidates[k].second;
        }
        return best;
    };

    // Rule 3: the largest group of sources that agree with one another.
    // Sorted by value, a group is a run whose spread is within the tolerance.
    // Between groups of equal size, the one holding the most trusted source wins.
    std::sort(candidates.begin(), candidates.end());
    std::size_t best_first = 0, best_size = 1;
    int best_source = best_rank_in(0, 1);
    for (std::size_t first = 0; first < candidates.size(); ++first) {
        const double low = candidates[first].first;
        const double tolerance = std::max(config_.agree_bpm, config_.agree_fraction * low);
        std::size_t last = first + 1;
        while (last < candidates.size() && candidates[last].first - low <= tolerance) ++last;
        const std::size_t size = last - first;
        const int leader = best_rank_in(first, last);
        if (size > best_size || (size == best_size && rank_of(leader) < rank_of(best_source))) {
            best_first = first;
            best_size = size;
            best_source = leader;
        }
    }

    if (best_size >= 2) {
        out.status = FusionStatus::corroborated;
        out.alarm_permitted = true;  // rule 5
        out.source = best_source;    // rule 1, within the agreeing group
        for (std::size_t k = 0; k < candidates.size(); ++k) {
            if (k < best_first || k >= best_first + best_size) {
                out.sources[static_cast<std::size_t>(candidates[k].second)].odd_one_out = true;
            }
        }
    } else {
        // One believable source, or several that all disagree: no way to tell
        // which is right, so take the most trusted and permit no alarm.
        out.status = candidates.size() == 1 ? FusionStatus::single : FusionStatus::disputed;
        out.source = best_rank_in(0, candidates.size());
    }
    out.bpm = sources_[static_cast<std::size_t>(out.source)].last_bpm;
    return out;
}

}  // namespace vitals::core
