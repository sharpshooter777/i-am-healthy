// SPDX-License-Identifier: Apache-2.0
//
// Combining heart-rate sources. No single sensor is trusted alone: chest
// straps and cameras check each other. The rules, agreed on November 15:
//
//   1. Use the highest-ranked source that is connected and believable.
//   2. Believable means: its own quality check passed, the value is in a sane
//      range, and it is not changing faster than a heart can.
//   3. When one source disagrees with others that agree, use the agreeing
//      group and flag the odd one.
//   4. Always say which source is in use.
//   5. Never permit an alarm on one source's word: an alarm needs two sources
//      that agree.
//
// Pure logic: no clocks and no OS calls. The caller supplies the time.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vitals::core {

// Why a source is, or is not, a candidate right now.
enum class SourceState : std::uint8_t {
    absent,        // never reported
    stale,         // reported before, silent for too long
    poor_quality,  // the source's own check failed (electrode contact, noise)
    out_of_range,  // not a human heart rate
    jumped,        // changed faster than a heart can
    ok,
};

enum class FusionStatus : std::uint8_t {
    no_source,     // nothing believable: show no number
    single,        // one believable source, nothing to check it against
    corroborated,  // two or more believable sources agree
    disputed,      // believable sources disagree and no two agree
};

const char* to_string(SourceState s);
const char* to_string(FusionStatus s);

struct FusionConfig {
    double min_bpm = 30.0;
    double max_bpm = 220.0;
    // A change is believed only as fast as a heart can make it. The floor
    // covers rounding and ordinary beat-to-beat scatter.
    double max_slew_bpm_per_s = 6.0;
    double slew_floor_bpm = 5.0;
    // Two sources agree when they are within the larger of these.
    double agree_bpm = 5.0;
    double agree_fraction = 0.05;
    // A strap reports once a second; three silent seconds is a lost source.
    double stale_after_s = 3.0;
};

struct SourceReport {
    std::string name;
    int rank = 0;                           // lower is more trusted
    SourceState state = SourceState::absent;
    double bpm = 0.0;                       // last reading, believable or not
    bool odd_one_out = false;               // believable, but outvoted (rule 3)
};

struct FusedHeartRate {
    FusionStatus status = FusionStatus::no_source;
    double bpm = 0.0;             // meaningful unless status is no_source
    int source = -1;              // index of the source in use, or -1 (rule 4)
    bool alarm_permitted = false; // true only when corroborated (rule 5)
    std::vector<SourceReport> sources;
};

class HeartRateFusion {
public:
    explicit HeartRateFusion(FusionConfig config = {});

    // Registers a source and returns its index. Lower rank is more trusted;
    // give every source a different rank. A source that reports at its own
    // pace (a camera estimate every few seconds) passes its own staleness
    // limit; 0 keeps the one in the configuration.
    int add_source(std::string name, int rank, double stale_after_s = 0.0);

    // A new reading. `quality_ok` is the source's own verdict, judged before
    // the reading is used: electrode contact for a strap, signal strength
    // for a camera.
    void update(int source, double t_s, double bpm, bool quality_ok);

    // The combined reading as of time t_s.
    FusedHeartRate evaluate(double t_s) const;

    const FusionConfig& config() const { return config_; }

private:
    struct Source {
        std::string name;
        int rank = 0;
        double stale_after_s = 0.0;
        bool reported = false;
        double last_t = 0.0;
        double last_bpm = 0.0;
        SourceState last_state = SourceState::absent;
        bool has_accepted = false;   // last believable reading, the reference
        double accepted_t = 0.0;     // for the rate-of-change check
        double accepted_bpm = 0.0;
    };

    FusionConfig config_;
    std::vector<Source> sources_;
};

}  // namespace vitals::core
