// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <ostream>

namespace vitals::vmcore {

// Runs the start-up checks, prints one line per check to out, and returns
// true when every check passes.
bool run_selftest(std::ostream& out);

}  // namespace vitals::vmcore
