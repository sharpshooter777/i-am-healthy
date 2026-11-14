// SPDX-License-Identifier: Apache-2.0
//
// Operating-system services behind one interface. Implementations:
//   src/qnx/    QNX Neutrino 8.0 (native kernel calls)
//   src/posix/  Linux and other POSIX systems
//   src/win32/  Windows
// IPC, shared memory and threads will join the clock here.
#pragma once

#include <cstdint>
#include <string>

namespace vitals::platform {

// "qnx", "posix" or "win32": which implementation was built.
const char* name();

// Operating system name and release, for logs and the self-test.
std::string os_description();

// Monotonic clock in nanoseconds. Every frame is timestamped with this at its
// source, and the timestamp is carried end to end.
std::int64_t monotonic_ns();

// Tick size of the system clock in nanoseconds: the finest step at which
// timers and sleeps can fire.
std::int64_t clock_tick_ns();

}  // namespace vitals::platform
