// SPDX-License-Identifier: Apache-2.0
//
// The receiving end of the two bridges. These are the live inputs to
// vm-core: chest-strap samples from hr-bridge and camera frames from
// grid-bridge, arriving from another machine. Both go into one heart monitor
// (core/heart_monitor), which judges each source on its own and combines
// them by the five rules in core/fusion. With the strap alone every reading
// is "single source": shown, but not allowed to raise an alarm. When the
// camera's own reading agrees, the reading is corroborated.
//
// Both inputs are timed by their arrival here, on this machine's clock, so
// the monitor compares like with like. The pulse reading itself depends on
// the frames being evenly spaced, not on when they arrive, which is why the
// frame count is checked for gaps.
#include "hr_client.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ostream>
#include <thread>
#include <vector>

#include "vitals/core/grid_line.hpp"
#include "vitals/core/heart_monitor.hpp"
#include "vitals/core/heart_rate.hpp"
#include "vitals/core/line_buffer.hpp"
#include "vitals/platform/net.hpp"
#include "vitals/platform/platform.hpp"

namespace net = vitals::platform::net;

namespace vitals::vmcore {

namespace {

// A strap sends one notification a second. Three silent seconds means the
// value on screen is no longer current and must be shown as stale.
constexpr std::int64_t stale_after_ns = 3000000000LL;
constexpr std::int64_t retry_after_ns = 2000000000LL;
constexpr std::size_t max_hr_line_bytes = 4096;

struct Stats {
    std::size_t samples = 0;
    std::size_t contact_lost = 0;
    std::size_t rr_missing = 0;
    std::size_t rr_implausible = 0;
    std::size_t bad_lines = 0;
    std::size_t stale_periods = 0;
    std::size_t fused_single = 0, fused_corroborated = 0, fused_disputed = 0, fused_none = 0;
    std::int64_t gap_min_ns = 0, gap_max_ns = 0, gap_sum_ns = 0;
    std::size_t gaps = 0;
    // The camera.
    std::size_t frames = 0;
    std::size_t frames_lost = 0;
    std::size_t frames_wrong_size = 0;
    std::size_t bad_frame_lines = 0;
    std::size_t estimates = 0;
    std::size_t believed = 0;
    std::size_t camera_restarts = 0;
    std::int64_t frame_first_ns = 0, frame_last_ns = 0;        // arrival here
    std::int64_t frame_first_sent_ns = 0, frame_last_sent_ns = 0;  // the sender's clock
    std::int64_t frame_gap_max_ns = 0;
};

// One connection out to a bridge.
struct Link {
    const char* what = "";
    std::string host;
    std::uint16_t port = 0;
    std::size_t max_line_bytes = 0;
    net::Socket s = net::invalid_socket;
    core::LineBuffer lines;
    std::int64_t next_attempt_ns = 0;
    bool gave_up = false;
    std::size_t connections = 0;

    bool connected() const { return s != net::invalid_socket; }
};

const char* contact_text(core::Contact c) {
    switch (c) {
        case core::Contact::ok: return "contact ok";
        case core::Contact::lost: return "CONTACT LOST";
        default: return "contact n/a";
    }
}

void count(Stats& st, const core::FusedHeartRate& fused) {
    switch (fused.status) {
        case core::FusionStatus::single: ++st.fused_single; break;
        case core::FusionStatus::corroborated: ++st.fused_corroborated; break;
        case core::FusionStatus::disputed: ++st.fused_disputed; break;
        case core::FusionStatus::no_source: ++st.fused_none; break;
    }
}

// What the monitor shows, as the tail of a printed line.
void describe(const core::FusedHeartRate& fused, int strap, char* text, std::size_t size) {
    if (fused.source >= 0) {
        std::snprintf(text, size, "in use: %s, %s%s", fused.sources[static_cast<std::size_t>(fused.source)].name.c_str(),
                      core::to_string(fused.status), fused.alarm_permitted ? "" : ", alarms held");
    } else {
        std::snprintf(text, size, "no believable source (%s %s)",
                      fused.sources[static_cast<std::size_t>(strap)].name.c_str(),
                      core::to_string(fused.sources[static_cast<std::size_t>(strap)].state));
    }
}

void print_summary(const Stats& st, const Link& strap, const Link* camera, std::ostream& out) {
    out << "hr-client summary\n"
        << "  samples           " << st.samples << '\n'
        << "  connections       " << strap.connections << '\n'
        << "  contact lost      " << st.contact_lost << '\n'
        << "  no intervals      " << st.rr_missing << '\n'
        << "  intervals suspect " << st.rr_implausible << '\n'
        << "  stale periods     " << st.stale_periods << '\n'
        << "  unreadable lines  " << st.bad_lines << '\n'
        << "  combined reading  single source " << st.fused_single << ", corroborated " << st.fused_corroborated
        << ", disputed " << st.fused_disputed << ", none " << st.fused_none << '\n';
    if (st.gaps > 0) {
        char buf[160];
        std::snprintf(buf, sizeof buf, "  arrival gap ms    min %.1f  mean %.1f  max %.1f\n",
                      static_cast<double>(st.gap_min_ns) * 1e-6,
                      static_cast<double>(st.gap_sum_ns) / static_cast<double>(st.gaps) * 1e-6,
                      static_cast<double>(st.gap_max_ns) * 1e-6);
        out << buf;
    }
    if (camera == nullptr) return;
    out << "  camera frames     " << st.frames << " (lost on the way " << st.frames_lost << ", wrong size "
        << st.frames_wrong_size << ", unreadable " << st.bad_frame_lines << ")\n"
        << "  camera links      " << camera->connections << ", window restarts " << st.camera_restarts << '\n'
        << "  camera estimates  " << st.estimates << ", believed " << st.believed << '\n';
    if (st.frames > 1) {
        const double n = static_cast<double>(st.frames - 1);
        const double sent_s = static_cast<double>(st.frame_last_sent_ns - st.frame_first_sent_ns) * 1e-9;
        const double here_s = static_cast<double>(st.frame_last_ns - st.frame_first_ns) * 1e-9;
        char buf[200];
        std::snprintf(buf, sizeof buf, "  camera frame rate %.2f/s as sent, %.2f/s as received, longest gap %.0f ms\n",
                      sent_s > 0.0 ? n / sent_s : 0.0, here_s > 0.0 ? n / here_s : 0.0,
                      static_cast<double>(st.frame_gap_max_ns) * 1e-6);
        out << buf;
    }
}

// Opens the link when it is down and due for a try.
void ensure_connected(Link& l, std::int64_t now, bool reconnect, std::ostream& out) {
    if (l.connected() || l.gave_up || now < l.next_attempt_ns) return;
    std::string error;
    l.s = net::connect_tcp(l.host, l.port, &error);
    if (!l.connected()) {
        out << "hr-client: " << l.what << ": " << error << std::endl;
        if (reconnect) l.next_attempt_ns = platform::monotonic_ns() + retry_after_ns;
        else l.gave_up = true;
        return;
    }
    ++l.connections;
    l.lines.clear();
    out << "hr-client: " << l.what << " connected to " << l.host << ':' << l.port << " (" << platform::name() << ")"
        << std::endl;
}

void drop(Link& l, bool reconnect) {
    net::close_socket(l.s);
    l.s = net::invalid_socket;
    if (reconnect) l.next_attempt_ns = platform::monotonic_ns() + retry_after_ns;
    else l.gave_up = true;
}

enum class Read { lines, nothing, closed };

// Reads what has arrived, waiting up to timeout_ms, and hands each complete
// line to `on_line` with its arrival time. `oversized` counts senders that
// never end their lines.
template <typename OnLine>
Read read_lines(Link& l, int timeout_ms, bool reconnect, std::size_t& oversized, std::ostream& out, OnLine on_line) {
    char buf[16384];
    const long n = net::recv_some(l.s, buf, sizeof buf, timeout_ms);
    if (n == -2) return Read::nothing;
    if (n <= 0) {
        out << "hr-client: " << l.what << ": " << (n == 0 ? "bridge closed the connection" : "receive error")
            << std::endl;
        drop(l, reconnect);
        return Read::closed;
    }
    const std::int64_t now = platform::monotonic_ns();
    l.lines.append(buf, static_cast<std::size_t>(n));
    std::string line;
    while (l.lines.next(line)) on_line(line, now);
    if (l.lines.pending() > l.max_line_bytes) {
        l.lines.clear();
        ++oversized;
    }
    return Read::lines;
}

}  // namespace

int run_hr_client(const HrClientOptions& o, std::ostream& out) {
    if (!net::startup()) {
        out << "hr-client: network start-up failed\n";
        return 1;
    }
    const std::int64_t started = platform::monotonic_ns();
    const std::int64_t deadline = o.seconds > 0.0 ? started + static_cast<std::int64_t>(o.seconds * 1e9) : 0;
    const auto expired = [&] { return deadline != 0 && platform::monotonic_ns() >= deadline; };

    // Strap and camera, each judged on its own, then combined. Without a
    // camera the strap stands alone and no alarm is permitted on its word.
    core::HeartMonitor monitor;
    const int strap_index = monitor.strap_source();
    const core::GridPulseConfig& grid = monitor.config().camera;

    Link strap;
    strap.what = "strap";
    strap.host = o.host;
    strap.port = o.port;
    strap.max_line_bytes = max_hr_line_bytes;

    const bool want_camera = !o.grid_host.empty();
    Link camera;
    camera.what = "camera";
    camera.host = o.grid_host;
    camera.port = o.grid_port;
    camera.max_line_bytes = 2 * core::grid_line_max_bytes(grid.cols, grid.rows);

    Stats st;
    std::int64_t last_arrival = 0;
    bool stale = false;
    core::GridFrame frame;
    std::vector<double> colours(monitor.frame_values());
    bool have_seq = false;
    std::uint64_t last_seq = 0;

    const auto on_strap_line = [&](const std::string& line, std::int64_t now) {
        core::HeartRateSample sample;
        if (!core::decode_hr_line(line, sample)) {
            ++st.bad_lines;
            return;
        }
        ++st.samples;
        if (last_arrival != 0) {
            const std::int64_t gap = now - last_arrival;
            st.gap_min_ns = st.gaps == 0 ? gap : std::min(st.gap_min_ns, gap);
            st.gap_max_ns = std::max(st.gap_max_ns, gap);
            st.gap_sum_ns += gap;
            ++st.gaps;
        }
        last_arrival = now;
        if (stale) {
            stale = false;
            out << "hr-client: samples resumed" << std::endl;
        }

        const bool lost = sample.contact == core::Contact::lost;
        const bool no_rr = sample.rr_ms.empty();
        const bool rr_ok = core::rr_plausible(sample);
        if (lost) ++st.contact_lost;
        if (no_rr) ++st.rr_missing;
        else if (!rr_ok) ++st.rr_implausible;

        // Quality is judged before the reading is used: lost contact or
        // intervals that contradict the rate rule the sample out.
        const double t_s = static_cast<double>(now) * 1e-9;
        monitor.strap_sample(t_s, sample);
        const core::FusedHeartRate fused = monitor.evaluate(t_s);
        count(st, fused);
        if (o.quiet) return;
        char text[240];
        const int len = std::snprintf(text, sizeof text, "hr %3d bpm  %-12s  rr %zu %-7s | ", sample.bpm,
                                      contact_text(sample.contact), sample.rr_ms.size(),
                                      no_rr ? "none" : (rr_ok ? "ok" : "SUSPECT"));
        const std::size_t used = len > 0 ? static_cast<std::size_t>(len) : 0;
        describe(fused, strap_index, text + used, sizeof text - used);
        out << text << std::endl;
    };

    const auto on_camera_line = [&](const std::string& line, std::int64_t now) {
        if (!core::decode_grid_line(line, frame)) {
            ++st.bad_frame_lines;
            return;
        }
        if (frame.rgb.size() != colours.size()) {  // a grid of another size cannot be read as this one
            ++st.frames_wrong_size;
            return;
        }
        // Frames must follow one another without a hole. After lost frames,
        // or a new connection, the camera starts its window again.
        const bool unbroken = have_seq && frame.seq == last_seq + 1;
        if (have_seq && frame.seq > last_seq + 1) st.frames_lost += static_cast<std::size_t>(frame.seq - last_seq - 1);
        if (!unbroken && st.frames > 0) {
            monitor.camera_restart();
            ++st.camera_restarts;
            if (!o.quiet) out << "hr-client: camera: frames missing, starting its window again" << std::endl;
        }
        have_seq = true;
        last_seq = frame.seq;
        if (st.frames == 0) {
            st.frame_first_ns = now;
            st.frame_first_sent_ns = frame.t_ns;
        } else {
            st.frame_gap_max_ns = std::max(st.frame_gap_max_ns, now - st.frame_last_ns);
        }
        st.frame_last_ns = now;
        st.frame_last_sent_ns = frame.t_ns;
        ++st.frames;

        for (std::size_t i = 0; i < colours.size(); ++i) colours[i] = static_cast<double>(frame.rgb[i]);
        const double t_s = static_cast<double>(now) * 1e-9;
        if (!monitor.camera_frame(t_s, colours.data(), colours.size())) return;
        const core::GridPulseEstimate& e = monitor.last_camera();
        ++st.estimates;
        if (e.believable) ++st.believed;
        if (o.quiet) return;
        char text[240];
        const int len = std::snprintf(text, sizeof text, "cam %5.1f bpm  quality %.2f  agreeing %zu  %-21s | ", e.bpm,
                                      e.quality, e.agreeing, e.believable ? "believed" : core::to_string(e.refusal));
        const std::size_t used = len > 0 ? static_cast<std::size_t>(len) : 0;
        describe(monitor.evaluate(t_s), strap_index, text + used, sizeof text - used);
        out << text << std::endl;
    };

    while (!expired()) {
        const std::int64_t now = platform::monotonic_ns();
        ensure_connected(strap, now, o.reconnect, out);
        if (want_camera) ensure_connected(camera, now, o.reconnect, out);
        // The strap is the session: when its bridge is gone for good, stop.
        if (!strap.connected() && strap.gave_up) break;
        if (!strap.connected() && !camera.connected()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        // Frames come thirty times a second, strap samples once: wait on the
        // camera briefly, then look at the strap without holding the camera up.
        if (camera.connected()) {
            if (read_lines(camera, strap.connected() ? 10 : 200, o.reconnect, st.bad_frame_lines, out, on_camera_line) ==
                Read::closed) {
                have_seq = false;  // a new connection starts its own count
            }
        }
        if (strap.connected()) {
            const Read r = read_lines(strap, camera.connected() ? 2 : 500, o.reconnect, st.bad_lines, out, on_strap_line);
            if (r == Read::nothing) {
                const std::int64_t t = platform::monotonic_ns();
                if (!stale && last_arrival != 0 && t - last_arrival > stale_after_ns) {
                    stale = true;
                    ++st.stale_periods;
                    out << "hr-client: STALE, no sample for " << static_cast<double>(t - last_arrival) * 1e-9 << " s"
                        << std::endl;
                }
            }
        }
    }
    if (strap.connected()) net::close_socket(strap.s);
    if (camera.connected()) net::close_socket(camera.s);

    print_summary(st, strap, want_camera ? &camera : nullptr, out);
    if (st.samples == 0) return 1;
    if (o.require_corroborated > 0 && static_cast<long>(st.fused_corroborated) < o.require_corroborated) {
        out << "hr-client: strap and camera agreed on " << st.fused_corroborated << " samples, fewer than the "
            << o.require_corroborated << " required\n";
        return 3;
    }
    return 0;
}

}  // namespace vitals::vmcore
