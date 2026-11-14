// SPDX-License-Identifier: Apache-2.0
//
// vm-core: the last pipeline stage. It will be the resource manager behind
// /dev/vitals and hold the alarm, trend and projection logic. For now it
// offers the self-test used to prove each build on each platform.
//
// Research prototype. Not a medical device. Not for clinical decisions.
#include <cstring>
#include <iostream>

#include "selftest.hpp"
#include "vitals/core/version.hpp"

namespace {

void usage(std::ostream& out) {
    out << "usage: vm-core [--selftest | --version | --help]\n"
           "  --selftest  run start-up checks; exit status 0 when all pass\n"
           "  --version   print the version\n";
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
    usage(std::cerr);
    std::cerr << "The resource manager is not built yet.\n";
    return 2;
}
