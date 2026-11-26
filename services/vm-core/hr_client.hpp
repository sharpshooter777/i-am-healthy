// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>

namespace vitals::vmcore {

struct HrClientOptions {
    std::string host;
    std::uint16_t port = 5600;
    // The camera, optional: where grid-bridge is serving frames.
    std::string grid_host;
    std::uint16_t grid_port = 5601;
    double seconds = 0.0;   // stop after this long; 0 runs until the strap stream ends
    bool quiet = false;     // summary only
    bool reconnect = false; // keep trying when a bridge goes away
    // For tests: fail unless strap and camera agreed on at least this many strap samples.
    long require_corroborated = 0;
};

// Connects out to hr-bridge, and to grid-bridge when one is named, and runs
// what arrives through the heart monitor: every strap sample and every
// camera estimate is printed with its checks and the combined reading.
// Returns 0 when at least one strap sample arrived and any requirement was met.
int run_hr_client(const HrClientOptions& options, std::ostream& out);

}  // namespace vitals::vmcore
