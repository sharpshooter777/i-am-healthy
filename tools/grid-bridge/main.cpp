// SPDX-License-Identifier: Apache-2.0
//
// grid-bridge: serves camera frames over TCP for the QNX pipeline to read.
// Each frame has already been reduced to a coarse grid of block-mean colours
// (the stream FFmpeg writes for grid-pulse: rgb48le, 32x18 by default), and
// goes out as one text line (core/grid_line.hpp). It runs on the machine
// with the camera; QNX connects out to it, as it does to hr-bridge.
//
//   grid-bridge follow [--port N] [--cols C] [--rows R]
//       Reads raw grid frames from standard input as the camera produces
//       them and forwards each one at once, stamped with the time it arrived:
//         ffmpeg -i <camera> -vf fps=30,format=rgb48le,scale=32:18:flags=area
//                -f rawvideo -pix_fmt rgb48le - | grid-bridge follow
//   grid-bridge replay <grid file> [--port N] [--fps F] [--speed X] [--loop]
//                      [--cols C] [--rows R]
//       Replays a saved grid stream at its frame rate. Replay starts when a
//       client connects, so every client sees the session from the beginning.
//
// Research prototype. Not a medical device. Not for clinical decisions.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#include "vitals/core/grid_line.hpp"
#include "vitals/platform/net.hpp"

namespace net = vitals::platform::net;
using vitals::core::GridFrame;

namespace {

std::int64_t epoch_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void usage() {
    std::cerr << "usage: grid-bridge follow [--port N] [--cols C] [--rows R]\n"
                 "       grid-bridge replay <grid file> [--port N] [--fps F] [--speed X] [--loop] [--cols C] [--rows R]\n";
}

struct Options {
    std::string mode;
    std::string path;
    std::uint16_t port = 5601;
    std::size_t cols = 32;
    std::size_t rows = 18;
    double fps = 30.0;
    double speed = 1.0;
    bool loop = false;
};

bool parse_args(int argc, char** argv, Options& o) {
    if (argc < 2) return false;
    o.mode = argv[1];
    int i = 2;
    if (o.mode == "replay") {
        if (argc < 3) return false;
        o.path = argv[2];
        i = 3;
    } else if (o.mode != "follow") {
        return false;
    }
    for (; i < argc; ++i) {
        const std::string a = argv[i];
        const bool has_value = i + 1 < argc;
        if (a == "--port" && has_value) o.port = static_cast<std::uint16_t>(std::atoi(argv[++i]));
        else if (a == "--cols" && has_value) o.cols = static_cast<std::size_t>(std::atoi(argv[++i]));
        else if (a == "--rows" && has_value) o.rows = static_cast<std::size_t>(std::atoi(argv[++i]));
        else if (a == "--fps" && has_value) o.fps = std::atof(argv[++i]);
        else if (a == "--speed" && has_value) o.speed = std::atof(argv[++i]);
        else if (a == "--loop") o.loop = true;
        else return false;
    }
    return o.cols > 0 && o.cols <= 1024 && o.rows > 0 && o.rows <= 1024 && o.fps > 0.0 && o.speed > 0.0;
}

// Reads one raw frame (little-endian 16-bit values). False at the end of the input.
bool read_frame(std::FILE* in, std::vector<std::uint8_t>& raw, GridFrame& frame) {
    if (std::fread(raw.data(), 1, raw.size(), in) != raw.size()) return false;
    for (std::size_t i = 0; i < frame.rgb.size(); ++i) {
        frame.rgb[i] = static_cast<std::uint16_t>(raw[2 * i] | (raw[2 * i + 1] << 8));
    }
    return true;
}

GridFrame empty_frame(const Options& o) {
    GridFrame f;
    f.cols = o.cols;
    f.rows = o.rows;
    f.rgb.resize(o.cols * o.rows * 3);
    return f;
}

int follow(const Options& o) {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);  // the frames are bytes, not text
#endif
    std::string error;
    const net::Socket listener = net::listen_tcp(o.port, &error);
    if (listener == net::invalid_socket) {
        std::cerr << "grid-bridge: " << error << '\n';
        return 1;
    }
    std::cout << "grid-bridge: following standard input, " << o.cols << 'x' << o.rows << ", listening on port "
              << net::local_port(listener) << std::endl;

    std::vector<net::Socket> clients;
    GridFrame frame = empty_frame(o);
    std::vector<std::uint8_t> raw(frame.rgb.size() * 2);
    std::uint64_t forwarded = 0;
    while (read_frame(stdin, raw, frame)) {
        frame.t_ns = epoch_ns();  // arrival on this machine: as close to the capture time as a pipe allows
        frame.seq = forwarded++;
        // Pick up anyone who connected since the last frame.
        for (;;) {
            std::string peer;
            const net::Socket c = net::accept_tcp(listener, 0, &peer);
            if (c == net::invalid_socket) break;
            std::cout << "grid-bridge: client " << peer << " connected at frame " << frame.seq << std::endl;
            clients.push_back(c);
        }
        if (clients.empty()) continue;
        const std::string line = vitals::core::encode_grid_line(frame) + "\n";
        for (std::size_t i = 0; i < clients.size();) {
            if (net::send_all(clients[i], line.data(), line.size())) {
                ++i;
            } else {
                std::cout << "grid-bridge: a client disconnected" << std::endl;
                net::close_socket(clients[i]);
                clients.erase(clients.begin() + static_cast<std::ptrdiff_t>(i));
            }
        }
    }
    for (net::Socket c : clients) net::close_socket(c);
    net::close_socket(listener);
    std::cout << "grid-bridge: input ended after " << forwarded << " frames" << std::endl;
    return 0;
}

// Sends the file to one client at its frame rate. Returns the number of frames sent.
std::uint64_t replay_to(net::Socket client, const Options& o) {
    std::FILE* in = std::fopen(o.path.c_str(), "rb");
    if (in == nullptr) return 0;
    GridFrame frame = empty_frame(o);
    std::vector<std::uint8_t> raw(frame.rgb.size() * 2);
    const auto start = std::chrono::steady_clock::now();
    const double frame_ns = 1e9 / (o.fps * o.speed);
    std::uint64_t sent = 0;
    while (read_frame(in, raw, frame)) {
        std::this_thread::sleep_until(
            start + std::chrono::nanoseconds(static_cast<std::int64_t>(static_cast<double>(sent) * frame_ns)));
        frame.t_ns = epoch_ns();  // "now" on the bridge: the replay stands in for a live camera
        frame.seq = sent;
        const std::string line = vitals::core::encode_grid_line(frame) + "\n";
        if (!net::send_all(client, line.data(), line.size())) break;
        ++sent;
    }
    std::fclose(in);
    return sent;
}

int replay(const Options& o) {
    std::FILE* probe = std::fopen(o.path.c_str(), "rb");
    if (probe == nullptr) {
        std::cerr << "grid-bridge: cannot open " << o.path << '\n';
        return 1;
    }
    std::fclose(probe);
    std::string error;
    const net::Socket listener = net::listen_tcp(o.port, &error);
    if (listener == net::invalid_socket) {
        std::cerr << "grid-bridge: " << error << '\n';
        return 1;
    }
    std::cout << "grid-bridge: " << o.path << ", " << o.cols << 'x' << o.rows << " at " << o.fps << " frames/s, speed "
              << o.speed << ", listening on port " << net::local_port(listener) << std::endl;
    do {
        std::string peer;
        const net::Socket client = net::accept_tcp(listener, -1, &peer);
        if (client == net::invalid_socket) continue;
        std::cout << "grid-bridge: client " << peer << " connected" << std::endl;
        const std::uint64_t sent = replay_to(client, o);
        net::close_socket(client);
        std::cout << "grid-bridge: sent " << sent << " frames to " << peer << std::endl;
    } while (o.loop);
    net::close_socket(listener);
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
        std::cerr << "grid-bridge: network start-up failed\n";
        return 1;
    }
    return o.mode == "follow" ? follow(o) : replay(o);
}
