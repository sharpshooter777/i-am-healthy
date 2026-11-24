// SPDX-License-Identifier: Apache-2.0
//
// hr-bridge: serves chest-strap heart-rate samples over TCP, one text line per
// sample, for the QNX pipeline to read. It runs on the machine that has the
// Bluetooth radio (Linux or Windows); QNX connects out to it.
//
//   hr-bridge replay <log> [--port N] [--speed X] [--loop]
//       Replays a strap log with its original timing. Replay starts when a
//       client connects, so every client sees the session from the beginning.
//   hr-bridge follow [--port N]
//       Reads strap-log lines from standard input as they are written and
//       forwards each one at once:  tail -f -n0 strap.raw | hr-bridge follow
//   hr-bridge synth [--port N] [--bpm B] [--seconds S]
//       Serves a made-up steady rhythm, for tests and demos without a strap.
//
// Research prototype. Not a medical device. Not for clinical decisions.
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "vitals/core/heart_rate.hpp"
#include "vitals/platform/net.hpp"
#include "vitals/sources/h10_log.hpp"

namespace net = vitals::platform::net;
using vitals::core::HeartRateSample;

namespace {

std::int64_t epoch_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void usage() {
    std::cerr << "usage: hr-bridge replay <log> [--port N] [--speed X] [--loop]\n"
                 "       hr-bridge follow [--port N]\n"
                 "       hr-bridge synth [--port N] [--bpm B] [--seconds S]\n";
}

struct Options {
    std::string mode;
    std::string path;
    std::uint16_t port = 5600;
    double speed = 1.0;
    bool loop = false;
    double bpm = 66.0;
    double seconds = 60.0;
};

bool parse_args(int argc, char** argv, Options& o) {
    if (argc < 2) return false;
    o.mode = argv[1];
    int i = 2;
    if (o.mode == "replay") {
        if (argc < 3) return false;
        o.path = argv[2];
        i = 3;
    } else if (o.mode != "follow" && o.mode != "synth") {
        return false;
    }
    for (; i < argc; ++i) {
        const std::string a = argv[i];
        const bool has_value = i + 1 < argc;
        if (a == "--port" && has_value) o.port = static_cast<std::uint16_t>(std::atoi(argv[++i]));
        else if (a == "--speed" && has_value) o.speed = std::atof(argv[++i]);
        else if (a == "--bpm" && has_value) o.bpm = std::atof(argv[++i]);
        else if (a == "--seconds" && has_value) o.seconds = std::atof(argv[++i]);
        else if (a == "--loop") o.loop = true;
        else return false;
    }
    return o.speed > 0.0 && o.bpm > 0.0;
}

bool send_sample(net::Socket client, const HeartRateSample& s) {
    const std::string line = vitals::core::encode_hr_line(s) + "\n";
    return net::send_all(client, line.data(), line.size());
}

// Samples with t_ns holding the offset from the first sample.
bool load_log(const std::string& path, std::vector<HeartRateSample>& out, std::string& error) {
    std::ifstream in(path);
    if (!in) {
        error = "cannot open " + path;
        return false;
    }
    std::string line;
    std::vector<std::uint8_t> bytes;
    std::int64_t t = 0, first = 0;
    while (std::getline(in, line)) {
        if (!vitals::sources::parse_gatt_line(line, t, bytes)) continue;
        HeartRateSample s;
        if (!vitals::core::parse_hrm(bytes.data(), bytes.size(), s)) continue;
        if (out.empty()) first = t;
        s.t_ns = t - first;
        out.push_back(std::move(s));
    }
    if (out.empty()) {
        error = "no heart-rate notifications in " + path;
        return false;
    }
    return true;
}

std::vector<HeartRateSample> make_synthetic(double bpm, double seconds) {
    std::vector<HeartRateSample> out;
    const double rr = 60000.0 / bpm;
    double beat_ms = 0.0;
    for (int sec = 0; sec < static_cast<int>(seconds); ++sec) {
        HeartRateSample s;
        s.t_ns = static_cast<std::int64_t>(sec) * 1000000000LL;
        s.bpm = static_cast<int>(std::lround(bpm));
        s.contact = vitals::core::Contact::ok;
        // One notification a second carries the beats that ended in that second.
        while (beat_ms + rr <= (sec + 1) * 1000.0) {
            beat_ms += rr;
            s.rr_ms.push_back(rr);
        }
        out.push_back(std::move(s));
    }
    return out;
}

// Sends the samples to one client, paced by their offsets. Returns the number sent.
std::size_t replay_to(net::Socket client, const std::vector<HeartRateSample>& samples, double speed) {
    const auto start = std::chrono::steady_clock::now();
    std::size_t sent = 0;
    for (const HeartRateSample& s : samples) {
        const auto due = start + std::chrono::nanoseconds(static_cast<std::int64_t>(static_cast<double>(s.t_ns) / speed));
        std::this_thread::sleep_until(due);
        HeartRateSample live = s;
        live.t_ns = epoch_ns();  // "now" on the bridge: the replay stands in for a live strap
        if (!send_sample(client, live)) break;
        ++sent;
    }
    return sent;
}

int serve_samples(const Options& o, const std::vector<HeartRateSample>& samples) {
    std::string error;
    const net::Socket listener = net::listen_tcp(o.port, &error);
    if (listener == net::invalid_socket) {
        std::cerr << "hr-bridge: " << error << '\n';
        return 1;
    }
    const double span_s = static_cast<double>(samples.back().t_ns) * 1e-9 / o.speed;
    std::cout << "hr-bridge: " << samples.size() << " samples, " << span_s << " s at speed " << o.speed
              << ", listening on port " << net::local_port(listener) << std::endl;
    do {
        std::string peer;
        const net::Socket client = net::accept_tcp(listener, -1, &peer);
        if (client == net::invalid_socket) continue;
        std::cout << "hr-bridge: client " << peer << " connected" << std::endl;
        const std::size_t sent = replay_to(client, samples, o.speed);
        net::close_socket(client);
        std::cout << "hr-bridge: sent " << sent << " of " << samples.size() << " samples to " << peer << std::endl;
    } while (o.loop);
    net::close_socket(listener);
    return 0;
}

int follow(const Options& o) {
    std::string error;
    const net::Socket listener = net::listen_tcp(o.port, &error);
    if (listener == net::invalid_socket) {
        std::cerr << "hr-bridge: " << error << '\n';
        return 1;
    }
    std::cout << "hr-bridge: following standard input, listening on port " << net::local_port(listener) << std::endl;

    std::vector<net::Socket> clients;
    std::string line;
    std::vector<std::uint8_t> bytes;
    std::size_t forwarded = 0;
    while (std::getline(std::cin, line)) {
        // Pick up anyone who connected since the last line.
        for (;;) {
            std::string peer;
            const net::Socket c = net::accept_tcp(listener, 0, &peer);
            if (c == net::invalid_socket) break;
            std::cout << "hr-bridge: client " << peer << " connected" << std::endl;
            clients.push_back(c);
        }
        std::int64_t t = 0;
        if (!vitals::sources::parse_gatt_line(line, t, bytes)) continue;
        HeartRateSample s;
        if (!vitals::core::parse_hrm(bytes.data(), bytes.size(), s)) continue;
        s.t_ns = t;  // the logger's receive time, on this machine's clock
        for (std::size_t i = 0; i < clients.size();) {
            if (send_sample(clients[i], s)) {
                ++i;
            } else {
                std::cout << "hr-bridge: a client disconnected" << std::endl;
                net::close_socket(clients[i]);
                clients.erase(clients.begin() + static_cast<std::ptrdiff_t>(i));
            }
        }
        ++forwarded;
    }
    for (net::Socket c : clients) net::close_socket(c);
    net::close_socket(listener);
    std::cout << "hr-bridge: input ended after " << forwarded << " samples" << std::endl;
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    Options o;
    if (!parse_args(argc, argv, o)) {
        usage();
        return 2;
    }
    if (!net::startup()) {
        std::cerr << "hr-bridge: network start-up failed\n";
        return 1;
    }
    if (o.mode == "follow") return follow(o);

    std::vector<HeartRateSample> samples;
    if (o.mode == "replay") {
        std::string error;
        if (!load_log(o.path, samples, error)) {
            std::cerr << "hr-bridge: " << error << '\n';
            return 1;
        }
    } else {
        samples = make_synthetic(o.bpm, o.seconds);
        if (samples.empty()) {
            std::cerr << "hr-bridge: nothing to send\n";
            return 1;
        }
    }
    return serve_samples(o, samples);
}
