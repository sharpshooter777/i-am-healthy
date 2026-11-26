// SPDX-License-Identifier: Apache-2.0
//
// vm-core: the last pipeline stage. It will be the resource manager behind
// /dev/vitals and hold the alarm, trend and projection logic. For now it
// offers the self-test used to prove each build on each platform, and the
// heart-rate client: the live inputs, chest strap from hr-bridge and camera
// frames from grid-bridge, run through the heart monitor together.
//
// Research prototype. Not a medical device. Not for clinical decisions.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "hr_client.hpp"
#include "selftest.hpp"
#include "vitals/core/version.hpp"

namespace {

void usage(std::ostream& out) {
    out << "usage: vm-core [--selftest | --version | --help]\n"
           "       vm-core --hr-client <host[:port]> [--grid <host[:port]>] [--seconds S] [--quiet]\n"
           "               [--reconnect] [--require-corroborated N]\n"
           "  --selftest   run start-up checks; exit status 0 when all pass\n"
           "  --version    print the version\n"
           "  --hr-client  read chest-strap heart rate from hr-bridge (default port 5600)\n"
           "  --grid       also read camera frames from grid-bridge (default port 5601)\n";
}

// Splits host[:port]. Returns false on an empty host or a bad port.
bool parse_target(const std::string& target, std::string& host, std::uint16_t& port) {
    const std::size_t colon = target.rfind(':');
    if (colon == std::string::npos) {
        host = target;
    } else {
        host = target.substr(0, colon);
        const int p = std::atoi(target.c_str() + colon + 1);
        if (p <= 0 || p > 65535) return false;
        port = static_cast<std::uint16_t>(p);
    }
    return !host.empty();
}

// Parses the arguments after --hr-client. Returns false on a bad argument.
bool parse_hr_client(int argc, char** argv, vitals::vmcore::HrClientOptions& o) {
    if (argc < 3) return false;
    if (!parse_target(argv[2], o.host, o.port)) return false;
    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--seconds" && i + 1 < argc) o.seconds = std::atof(argv[++i]);
        else if (a == "--grid" && i + 1 < argc) {
            if (!parse_target(argv[++i], o.grid_host, o.grid_port)) return false;
        } else if (a == "--require-corroborated" && i + 1 < argc) o.require_corroborated = std::atol(argv[++i]);
        else if (a == "--quiet") o.quiet = true;
        else if (a == "--reconnect") o.reconnect = true;
        else return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    // A line at a time, so the output can be followed live through a pipe or over ssh.
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    if (argc == 2 && std::strcmp(argv[1], "--selftest") == 0) {
        return vitals::vmcore::run_selftest(std::cout) ? 0 : 1;
    }
    if (argc == 2 && std::strcmp(argv[1], "--version") == 0) {
        std::cout << "vm-core " << vitals::core::version() << '\n';
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--help") == 0) {
        usage(std::cout);
        return 0;
    }
    if (argc >= 2 && std::strcmp(argv[1], "--hr-client") == 0) {
        vitals::vmcore::HrClientOptions o;
        if (!parse_hr_client(argc, argv, o)) {
            usage(std::cerr);
            return 2;
        }
        return vitals::vmcore::run_hr_client(o, std::cout);
    }
    usage(std::cerr);
    std::cerr << "The resource manager is not built yet.\n";
    return 2;
}
