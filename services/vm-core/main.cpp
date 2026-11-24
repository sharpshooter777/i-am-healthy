// SPDX-License-Identifier: Apache-2.0
//
// vm-core: the last pipeline stage. It will be the resource manager behind
// /dev/vitals and hold the alarm, trend and projection logic. For now it
// offers the self-test used to prove each build on each platform, and the
// heart-rate client: the first live input, read from hr-bridge.
//
// Research prototype. Not a medical device. Not for clinical decisions.
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
           "       vm-core --hr-client <host[:port]> [--seconds S] [--quiet] [--reconnect]\n"
           "  --selftest   run start-up checks; exit status 0 when all pass\n"
           "  --version    print the version\n"
           "  --hr-client  read chest-strap heart rate from hr-bridge (default port 5600)\n";
}

// Parses the arguments after --hr-client. Returns false on a bad argument.
bool parse_hr_client(int argc, char** argv, vitals::vmcore::HrClientOptions& o) {
    if (argc < 3) return false;
    const std::string target = argv[2];
    const std::size_t colon = target.rfind(':');
    if (colon == std::string::npos) {
        o.host = target;
    } else {
        o.host = target.substr(0, colon);
        const int port = std::atoi(target.c_str() + colon + 1);
        if (port <= 0 || port > 65535) return false;
        o.port = static_cast<std::uint16_t>(port);
    }
    if (o.host.empty()) return false;
    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--seconds" && i + 1 < argc) o.seconds = std::atof(argv[++i]);
        else if (a == "--quiet") o.quiet = true;
        else if (a == "--reconnect") o.reconnect = true;
        else return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
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
