// SPDX-License-Identifier: Apache-2.0
//
// vm-offline: makes synthetic recordings and measures heart rate from a
// recording, comparing it with the true values when the recording has them.
// A desk tool for developing and checking the signal processing; the live
// pipeline is vm-capture -> vm-dsp -> vm-fusion -> vm-core.
//
// Research prototype. Not a medical device. Not for clinical decisions.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "vitals/core/image.hpp"
#include "vitals/core/pulse.hpp"
#include "vitals/core/trend.hpp"
#include "vitals/sources/recording.hpp"
#include "vitals/sources/synthetic.hpp"

namespace {

using vitals::sources::Frame;

void usage() {
    std::fprintf(stderr,
                 "usage:\n"
                 "  vm-offline synth OUT.vmrec [--seconds S] [--fps F] [--hr BPM] [--slope BPM_PER_MIN]\n"
                 "                             [--strength K] [--noise SD] [--drop P] [--seed N]\n"
                 "  vm-offline analyse IN.vmrec [--window S] [--step S] [--limit BPM] [--max-error BPM] [--quiet]\n"
                 "\n"
                 "analyse prints a heart-rate estimate every step and, with --limit, the projected\n"
                 "time for the trend to reach that heart rate. With --max-error it exits 1 if any\n"
                 "estimate differs from the recorded true value by more than that.\n");
}

// Parses "--name value" pairs after the positional argument.
class Args {
public:
    Args(int argc, char** argv, int first) {
        for (int i = first; i < argc; ++i) items_.emplace_back(argv[i]);
    }

    double number(const char* name, double fallback) {
        for (std::size_t i = 0; i + 1 < items_.size(); ++i) {
            if (items_[i] == name) {
                char* end = nullptr;
                const double v = std::strtod(items_[i + 1].c_str(), &end);
                if (end == items_[i + 1].c_str() || *end != '\0') {
                    std::fprintf(stderr, "%s needs a number, got '%s'\n", name, items_[i + 1].c_str());
                    std::exit(2);
                }
                used_.push_back(i);
                used_.push_back(i + 1);
                return v;
            }
        }
        return fallback;
    }

    bool flag(const char* name) {
        for (std::size_t i = 0; i < items_.size(); ++i) {
            if (items_[i] == name) {
                used_.push_back(i);
                return true;
            }
        }
        return false;
    }

    // Reports anything not consumed by number() or flag().
    bool all_used() const {
        bool ok = true;
        for (std::size_t i = 0; i < items_.size(); ++i) {
            bool used = false;
            for (std::size_t u : used_) used = used || u == i;
            if (!used) {
                std::fprintf(stderr, "unknown argument '%s'\n", items_[i].c_str());
                ok = false;
            }
        }
        return ok;
    }

private:
    std::vector<std::string> items_;
    std::vector<std::size_t> used_;
};

int synth(const std::string& path, Args& args) {
    vitals::sources::SyntheticConfig cfg;
    cfg.seconds = args.number("--seconds", 60.0);
    cfg.fps = args.number("--fps", 30.0);
    cfg.hr_start_bpm = args.number("--hr", 72.0);
    cfg.hr_slope_bpm_per_min = args.number("--slope", 0.0);
    cfg.pulse_strength = args.number("--strength", 1.0);
    cfg.noise_level = args.number("--noise", 2.0);
    cfg.drop_probability = args.number("--drop", 0.0);
    cfg.seed = static_cast<std::uint32_t>(args.number("--seed", 1.0));
    if (!args.all_used()) return 2;

    vitals::sources::SyntheticSource src(cfg);
    vitals::sources::RecordingWriter out(path);
    if (!out.ok()) {
        std::fprintf(stderr, "cannot write %s\n", path.c_str());
        return 1;
    }
    Frame f;
    std::size_t n = 0;
    while (src.next(f)) {
        if (!out.write(f)) break;
        ++n;
    }
    if (!out.close()) {
        std::fprintf(stderr, "write to %s failed\n", path.c_str());
        return 1;
    }
    std::printf("wrote %zu frames to %s: %s\n", n, path.c_str(), src.describe().c_str());
    return 0;
}

int analyse(const std::string& path, Args& args) {
    const double window_s = args.number("--window", 10.0);
    const double step_s = args.number("--step", 1.0);
    const double limit = args.number("--limit", 0.0);
    const double max_error = args.number("--max-error", 0.0);
    const bool quiet = args.flag("--quiet");
    if (!args.all_used()) return 2;

    vitals::sources::RecordingReader src(path);
    vitals::core::PulseTracker tracker(window_s);

    Frame f;
    double t_first = NAN;
    double next_report = NAN;
    std::uint64_t expected_seq = 0;
    std::size_t frames = 0, dropped = 0, estimates = 0, compared = 0, within3 = 0, failed = 0;
    double sum_abs = 0.0, worst = 0.0;
    std::vector<double> est_t, est_bpm;
    std::deque<std::pair<double, double>> truth_hist;  // (time, true bpm) over the window

    if (!quiet) std::printf("%8s  %8s  %8s  %7s  %7s\n", "time s", "est bpm", "true bpm", "error", "quality");
    while (src.next(f)) {
        ++frames;
        if (f.seq > expected_seq) dropped += f.seq - expected_seq;
        expected_seq = f.seq + 1;

        const auto m = vitals::core::mean_rgb(f.data.data(), f.width, f.height, f.width * 3,
                                              {0, 0, f.width, f.height});
        if (!m.valid) continue;
        const double t = f.timestamp_s();
        if (std::isnan(t_first)) {
            t_first = t;
            next_report = t + window_s;
        }
        tracker.push({t, m.r, m.g, m.b});
        if (f.truth) {
            truth_hist.emplace_back(t, f.truth->heart_rate_bpm);
            while (truth_hist.back().first - truth_hist.front().first > window_s) truth_hist.pop_front();
        }
        if (t < next_report) continue;
        next_report += step_s;

        const auto hr = tracker.estimate();
        if (!hr.ok()) {
            ++failed;
            if (!quiet) std::printf("%8.1f  %s\n", t - t_first, vitals::core::to_string(hr.status));
            continue;
        }
        ++estimates;
        est_t.push_back(t);
        est_bpm.push_back(hr.bpm);

        if (!truth_hist.empty()) {
            // The estimate describes the whole window, so compare it with the
            // true rate averaged over the same window.
            double truth = 0.0;
            for (const auto& p : truth_hist) truth += p.second;
            truth /= static_cast<double>(truth_hist.size());
            const double err = hr.bpm - truth;
            ++compared;
            sum_abs += std::fabs(err);
            worst = std::fmax(worst, std::fabs(err));
            if (std::fabs(err) <= 3.0) ++within3;
            if (!quiet) {
                std::printf("%8.1f  %8.1f  %8.1f  %+7.1f  %7.2f\n", t - t_first, hr.bpm, truth, err, hr.quality);
            }
        } else if (!quiet) {
            std::printf("%8.1f  %8.1f  %8s  %7s  %7.2f\n", t - t_first, hr.bpm, "-", "-", hr.quality);
        }
    }
    if (!src.error().empty()) {
        std::fprintf(stderr, "%s\n", src.error().c_str());
        return 1;
    }

    std::printf("\n%zu frames, %zu dropped, %zu estimates, %zu not estimated\n", frames, dropped, estimates, failed);
    if (compared > 0) {
        std::printf("error against truth: mean %.2f bpm, worst %.2f bpm, %zu of %zu within 3 bpm\n",
                    sum_abs / static_cast<double>(compared), worst, within3, compared);
    }

    if (limit > 0.0 && est_t.size() >= 2) {
        const auto fit = vitals::core::fit_line(est_t.data(), est_bpm.data(), est_t.size());
        const auto eta = vitals::core::time_to_limit(fit, est_t.back(), limit, vitals::core::Bound::Upper);
        std::printf("trend %+.2f bpm/min; ", fit.slope * 60.0);
        if (!eta) {
            std::printf("not heading for %.0f bpm\n", limit);
        } else if (*eta == 0.0) {
            std::printf("at or above %.0f bpm\n", limit);
        } else {
            std::printf("reaches %.0f bpm in about %.0f min\n", limit, *eta / 60.0);
        }
    }

    if (max_error > 0.0 && (compared == 0 || worst > max_error)) {
        std::fprintf(stderr, "FAIL: worst error %.2f bpm exceeds %.2f bpm\n", worst, max_error);
        return 1;
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        usage();
        return 2;
    }
    const std::string cmd = argv[1];
    Args args(argc, argv, 3);
    if (cmd == "synth") return synth(argv[2], args);
    if (cmd == "analyse" || cmd == "analyze") return analyse(argv[2], args);
    usage();
    return 2;
}
