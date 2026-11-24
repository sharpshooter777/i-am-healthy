// SPDX-License-Identifier: Apache-2.0
//
// Reading the chest-strap logs written on the Linux laptop. Each notification
// line is the receive time in seconds since the epoch, then gatttool's line:
//   1000000000.250000000 Notification handle = 0x0010 value: 10 48 55 03
// Other lines (connection notices, "LOGGER restart") are skipped.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace vitals::sources {

// Returns true for a notification line, with the receive time in
// nanoseconds since the epoch and the packet bytes.
bool parse_gatt_line(const std::string& line, std::int64_t& epoch_ns, std::vector<std::uint8_t>& bytes);

}  // namespace vitals::sources
