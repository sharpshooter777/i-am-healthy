// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>

namespace vitals::vmcore {

struct HrClientOptions {
    std::string host;
    std::uint16_t port = 5600;
    double seconds = 0.0;   // stop after this long; 0 runs until the stream ends
    bool quiet = false;     // summary only
    bool reconnect = false; // keep trying when the bridge goes away
};

// Connects out to hr-bridge, reads heart-rate samples and prints each one
// with its checks. Returns 0 when at least one sample arrived.
int run_hr_client(const HrClientOptions& options, std::ostream& out);

}  // namespace vitals::vmcore
