// SPDX-License-Identifier: Apache-2.0
//
// session-replay: runs a recorded session through the heart monitor, strap
// and camera together, and reports what the monitor would have shown second
// by second: the combined reading, the source in use, and whether an alarm
// was permitted.
//
//   session-replay --strap <strap log> --grid <grid file|-> --grid-start <epoch s>
//                  [--fps F] [--hop S] [--strap-off <from s> <for s>] [--quiet]
//
// The grid is the stream FFmpeg writes for grid-pulse (rgb48le, 32x18).
// --grid-start is the clock time of its first frame, in seconds since the
// epoch, on the clock the strap log was stamped with. --strap-off withholds
// the strap for a stretch (seconds into the grid), as if it had fallen off;
// the withheld readings are still used to score the camera.
//
// Research prototype. Not a medical device. Not for clinical decisions.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "vitals/core/heart_monitor.hpp"
#include "vitals/sources/h10_log.hpp"

using namespace vitals;

namespace {

struct Options {
    std::string strap_path;
    std::string grid_path;
    double grid_start_s = -1.0;
    double fps = 30.0;
    double hop_s = 5.0;
    double off_from_s = -1.0;
    double off_for_s = 0.0;
    bool quiet = false;
};

void usage() {
    std::fprintf(stderr,
                 "usage: session-replay --strap <log> --grid <file|-> --grid-start <epoch s>\n"
                 "                      [--fps F] [--hop S] [--strap-off <from s> <for s>] [--quiet]\n");
}

bool parse_args(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const auto value = [&](double& out) {
            if (i + 1 >= argc) return false;
            out = std::atof(argv[++i]);
            return true;
        };
        if (a == "--quiet") o.quiet = true;
        else if (a == "--strap" && i + 1 < argc) o.strap_path = argv[++i];
        else if (a == "--grid" && i + 1 < argc) o.grid_path = argv[++i];
        else if (a == "--grid-start") { if (!value(o.grid_start_s)) return false; }
        else if (a == "--fps") { if (!value(o.fps)) return false; }
        else if (a == "--hop") { if (!value(o.hop_s)) return false; }
        else if (a == "--strap-off") { if (!value(o.off_from_s) || !value(o.off_for_s)) return false; }
        else return false;
    }
    return !o.strap_path.empty() && !o.grid_path.empty() && o.grid_start_s >= 0.0 && o.fps > 0.0 && o.hop_s > 0.0;
}

struct StrapPoint {
    double t_s = 0.0;  // seconds into the grid
    core::HeartRateSample sample;
};

struct Tally {
    long seconds = 0;
    long corroborated = 0, single_strap = 0, single_camera = 0, disputed = 0, none = 0;
    long alarm_permitted = 0;
    long camera_estimates = 0, camera_believed = 0, camera_agreed = 0, camera_disputed = 0;
    // While the strap is withheld: what the camera showed against the hidden strap.
    long off_seconds = 0, off_strap = 0, off_camera = 0, off_none = 0, off_within_5 = 0;
    double off_abs_error_sum = 0.0, off_abs_error_max = 0.0;
};

}  // namespace

int main(int argc, char** argv) {
    Options o;
    if (!parse_args(argc, argv, o)) {
        usage();
        return 2;
    }

    // The strap log, whole, on the grid's timeline.
    std::vector<StrapPoint> strap;
    {
        std::ifstream in(o.strap_path);
        if (!in) {
            std::fprintf(stderr, "session-replay: cannot open %s\n", o.strap_path.c_str());
            return 1;
        }
        std::string line;
        std::vector<std::uint8_t> bytes;
        while (std::getline(in, line)) {
            std::int64_t epoch_ns = 0;
            StrapPoint p;
            if (!sources::parse_gatt_line(line, epoch_ns, bytes)) continue;
            if (!core::parse_hrm(bytes.data(), bytes.size(), p.sample)) continue;
            p.sample.t_ns = epoch_ns;
            p.t_s = static_cast<double>(epoch_ns) * 1e-9 - o.grid_start_s;
            strap.push_back(std::move(p));
        }
    }
    if (strap.empty()) {
        std::fprintf(stderr, "session-replay: no strap samples in %s\n", o.strap_path.c_str());
        return 1;
    }

    std::FILE* grid = o.grid_path == "-" ? stdin : std::fopen(o.grid_path.c_str(), "rb");
    if (grid == nullptr) {
        std::fprintf(stderr, "session-replay: cannot open %s\n", o.grid_path.c_str());
        return 1;
    }

    core::HeartMonitorConfig cfg;
    cfg.camera.fs_hz = o.fps;
    cfg.camera_hop_s = o.hop_s;
    core::HeartMonitor monitor(cfg);
    const std::size_t values = monitor.frame_values();
    std::vector<std::uint8_t> raw(values * 2);
    std::vector<double> frame(values);

    const auto strap_index = static_cast<std::size_t>(monitor.strap_source());
    const auto camera_index = static_cast<std::size_t>(monitor.camera_source());
    const auto withheld = [&](double t_s) {
        return o.off_from_s >= 0.0 && t_s >= o.off_from_s && t_s < o.off_from_s + o.off_for_s;
    };

    Tally tally;
    std::size_t next_strap = 0;
    // Start a little before the grid so the strap has a history when the first frame arrives.
    while (next_strap < strap.size() && strap[next_strap].t_s < -10.0) ++next_strap;
    double hidden_bpm = 0.0;     // the strap's latest reading, fed to the monitor or not
    bool have_hidden = false;
    bool camera_started = false;
    long next_second = 0;
    std::size_t frames = 0;

    while (std::fread(raw.data(), 1, raw.size(), grid) == raw.size()) {
        const double t = static_cast<double>(frames) / o.fps;
        while (next_strap < strap.size() && strap[next_strap].t_s <= t) {
            const StrapPoint& p = strap[next_strap++];
            hidden_bpm = p.sample.bpm;
            have_hidden = true;
            if (!withheld(p.t_s)) monitor.strap_sample(p.t_s, p.sample);
        }
        for (std::size_t i = 0; i < values; ++i) {
            frame[i] = static_cast<double>(raw[2 * i] | (raw[2 * i + 1] << 8));  // little-endian 16-bit
        }
        ++frames;
        if (monitor.camera_frame(t, frame.data(), values)) {
            if (!camera_started) {
                camera_started = true;
                next_second = static_cast<long>(std::floor(t)) + 1;  // judge from here on, not the warm-up
            }
            ++tally.camera_estimates;
            if (monitor.last_camera().believable) {
                ++tally.camera_believed;
                const core::FusedHeartRate f = monitor.evaluate(t);
                if (f.sources[camera_index].state == core::SourceState::ok) {
                    if (f.status == core::FusionStatus::corroborated) ++tally.camera_agreed;
                    else if (f.status == core::FusionStatus::disputed) ++tally.camera_disputed;
                }
            }
        }

        // Once a second, from the camera's first estimate on: what the monitor shows.
        if (!camera_started || t < static_cast<double>(next_second)) continue;
        const double now = static_cast<double>(next_second);
        ++next_second;
        const core::FusedHeartRate f = monitor.evaluate(now);
        ++tally.seconds;
        if (f.alarm_permitted) ++tally.alarm_permitted;
        switch (f.status) {
            case core::FusionStatus::corroborated: ++tally.corroborated; break;
            case core::FusionStatus::disputed: ++tally.disputed; break;
            case core::FusionStatus::no_source: ++tally.none; break;
            case core::FusionStatus::single:
                if (f.source == monitor.camera_source()) ++tally.single_camera;
                else ++tally.single_strap;
                break;
        }
        if (withheld(now)) {
            ++tally.off_seconds;
            if (f.status == core::FusionStatus::no_source) {
                ++tally.off_none;
            } else if (f.source == monitor.strap_source()) {
                ++tally.off_strap;  // its last reading has not gone stale yet
            } else if (f.source == monitor.camera_source() && have_hidden) {
                ++tally.off_camera;
                const double error = std::fabs(f.bpm - hidden_bpm);
                tally.off_abs_error_sum += error;
                tally.off_abs_error_max = std::max(tally.off_abs_error_max, error);
                if (error <= 5.0) ++tally.off_within_5;
            }
        }
        if (!o.quiet) {
            const core::SourceReport& s = f.sources[strap_index];
            const core::SourceReport& c = f.sources[camera_index];
            std::printf("%6ld s  strap %5.1f %-12s  camera %5.1f %-12s | ", next_second - 1, s.bpm,
                        core::to_string(s.state), c.bpm, core::to_string(c.state));
            if (f.source >= 0) {
                std::printf("shown %5.1f from %s, %s%s\n", f.bpm,
                            f.sources[static_cast<std::size_t>(f.source)].name.c_str(), core::to_string(f.status),
                            f.alarm_permitted ? "" : ", alarms held");
            } else {
                std::printf("no number shown\n");
            }
        }
    }
    if (grid != stdin) std::fclose(grid);

    if (tally.seconds == 0) {
        std::fprintf(stderr, "session-replay: the grid is shorter than the camera's first window\n");
        return 1;
    }
    const auto share = [&](long n) { return 100.0 * static_cast<double>(n) / static_cast<double>(tally.seconds); };
    std::printf("session-replay: %.0f s of video, %ld s judged (from the camera's first estimate)\n",
                static_cast<double>(frames) / o.fps, tally.seconds);
    std::printf("  shown as    corroborated %ld s (%.0f%%), strap alone %ld s (%.0f%%), camera alone %ld s (%.0f%%),\n"
                "              disputed %ld s (%.0f%%), no number %ld s (%.0f%%)\n",
                tally.corroborated, share(tally.corroborated), tally.single_strap, share(tally.single_strap),
                tally.single_camera, share(tally.single_camera), tally.disputed, share(tally.disputed), tally.none,
                share(tally.none));
    std::printf("  alarms      permitted %ld s (%.0f%%), held %ld s\n", tally.alarm_permitted,
                share(tally.alarm_permitted), tally.seconds - tally.alarm_permitted);
    std::printf("  camera      %ld estimates, %ld believed: %ld agreed with the strap, %ld disputed it\n",
                tally.camera_estimates, tally.camera_believed, tally.camera_agreed, tally.camera_disputed);
    if (tally.off_seconds > 0) {
        std::printf("  strap off   %ld s: last strap reading stood %ld s, camera shown %ld s, no number %ld s\n",
                    tally.off_seconds, tally.off_strap, tally.off_camera, tally.off_none);
        if (tally.off_camera > 0) {
            std::printf("              camera against the withheld strap: mean difference %.1f bpm, worst %.1f,\n"
                        "              within 5 bpm for %ld of %ld s\n",
                        tally.off_abs_error_sum / static_cast<double>(tally.off_camera), tally.off_abs_error_max,
                        tally.off_within_5, tally.off_camera);
        }
    }
    return 0;
}
