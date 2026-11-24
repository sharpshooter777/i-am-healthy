// SPDX-License-Identifier: Apache-2.0
//
// The receiving end of the heart-rate bridge. This is the first live input to
// vm-core: chest-strap samples arriving from another machine, each checked
// before anything downstream may use it.
#include "hr_client.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ostream>
#include <thread>

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
constexpr std::size_t max_line_bytes = 4096;

struct Stats {
    std::size_t samples = 0;
    std::size_t contact_lost = 0;
    std::size_t rr_missing = 0;
    std::size_t rr_implausible = 0;
    std::size_t bad_lines = 0;
    std::size_t stale_periods = 0;
    std::size_t connections = 0;
    std::int64_t gap_min_ns = 0, gap_max_ns = 0, gap_sum_ns = 0;
    std::size_t gaps = 0;
};

const char* contact_text(core::Contact c) {
    switch (c) {
        case core::Contact::ok: return "contact ok";
        case core::Contact::lost: return "CONTACT LOST";
        default: return "contact n/a";
    }
}

void print_summary(const Stats& st, std::ostream& out) {
    out << "hr-client summary\n"
        << "  samples           " << st.samples << '\n'
        << "  connections       " << st.connections << '\n'
        << "  contact lost      " << st.contact_lost << '\n'
        << "  no intervals      " << st.rr_missing << '\n'
        << "  intervals suspect " << st.rr_implausible << '\n'
        << "  stale periods     " << st.stale_periods << '\n'
        << "  unreadable lines  " << st.bad_lines << '\n';
    if (st.gaps > 0) {
        char buf[160];
        std::snprintf(buf, sizeof buf, "  arrival gap ms    min %.1f  mean %.1f  max %.1f\n",
                      static_cast<double>(st.gap_min_ns) * 1e-6,
                      static_cast<double>(st.gap_sum_ns) / static_cast<double>(st.gaps) * 1e-6,
                      static_cast<double>(st.gap_max_ns) * 1e-6);
        out << buf;
    }
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

    Stats st;
    std::int64_t last_arrival = 0;
    bool stale = false;

    while (!expired()) {
        std::string error;
        const net::Socket s = net::connect_tcp(o.host, o.port, &error);
        if (s == net::invalid_socket) {
            out << "hr-client: " << error << std::endl;
            if (!o.reconnect) break;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            continue;
        }
        ++st.connections;
        out << "hr-client: connected to " << o.host << ':' << o.port << " (" << platform::name() << ")" << std::endl;

        core::LineBuffer lines;
        char buf[1024];
        bool open = true;
        while (open && !expired()) {
            const long n = net::recv_some(s, buf, sizeof buf, 500);
            const std::int64_t now = platform::monotonic_ns();
            if (n == -2) {  // nothing yet: check for a stale reading
                if (!stale && last_arrival != 0 && now - last_arrival > stale_after_ns) {
                    stale = true;
                    ++st.stale_periods;
                    out << "hr-client: STALE, no sample for "
                        << static_cast<double>(now - last_arrival) * 1e-9 << " s" << std::endl;
                }
                continue;
            }
            if (n <= 0) {
                out << "hr-client: " << (n == 0 ? "bridge closed the connection" : "receive error") << std::endl;
                open = false;
                break;
            }
            lines.append(buf, static_cast<std::size_t>(n));
            if (lines.pending() > max_line_bytes) {  // a sender that never ends its lines
                lines.clear();
                ++st.bad_lines;
            }
            std::string line;
            while (lines.next(line)) {
                core::HeartRateSample sample;
                if (!core::decode_hr_line(line, sample)) {
                    ++st.bad_lines;
                    continue;
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

                if (!o.quiet) {
                    char text[160];
                    std::snprintf(text, sizeof text, "hr %3d bpm  %-12s  rr %zu %s",
                                  sample.bpm, contact_text(sample.contact), sample.rr_ms.size(),
                                  no_rr ? "none" : (rr_ok ? "ok" : "SUSPECT"));
                    out << text << std::endl;
                }
            }
        }
        net::close_socket(s);
        if (!o.reconnect) break;
        if (!expired()) std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    print_summary(st, out);
    return st.samples > 0 ? 0 : 1;
}

}  // namespace vitals::vmcore
