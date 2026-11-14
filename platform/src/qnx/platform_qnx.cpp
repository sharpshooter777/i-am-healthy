// SPDX-License-Identifier: Apache-2.0
// QNX Neutrino 8.0 implementation.
#include "vitals/platform/platform.hpp"

#include <sys/neutrino.h>
#include <sys/utsname.h>
#include <time.h>

namespace vitals::platform {

const char* name() { return "qnx"; }

std::string os_description() {
    struct utsname u {};
    if (uname(&u) != 0) {
        return "QNX (uname failed)";
    }
    // uname reports 8.0.0 on every 8.0.x image (QNXE-554); the real
    // release comes from `use -i /system/bin/qconn`.
    return std::string(u.sysname) + " " + u.release + " " + u.machine;
}

std::int64_t monotonic_ns() {
    // ClockTime() is the kernel call underneath clock_gettime(); reading
    // CLOCK_MONOTONIC directly avoids the library wrapper.
    std::uint64_t ns = 0;
    if (ClockTime(CLOCK_MONOTONIC, nullptr, &ns) == -1) {
        return -1;
    }
    return static_cast<std::int64_t>(ns);
}

std::int64_t clock_tick_ns() {
    // ClockPeriod() reports the kernel's clock tick, the resolution at which
    // timers fire. QNX lets a privileged process change it.
    struct _clockperiod period {};
    if (ClockPeriod(CLOCK_REALTIME, nullptr, &period, 0) == -1) {
        return -1;
    }
    return static_cast<std::int64_t>(period.nsec);
}

}  // namespace vitals::platform
