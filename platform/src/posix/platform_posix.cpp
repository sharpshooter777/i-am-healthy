// SPDX-License-Identifier: Apache-2.0
// POSIX implementation, used on Linux.
#include "vitals/platform/platform.hpp"

#include <sys/utsname.h>
#include <time.h>

namespace vitals::platform {

const char* name() { return "posix"; }

std::string os_description() {
    struct utsname u {};
    if (uname(&u) != 0) {
        return "POSIX (uname failed)";
    }
    return std::string(u.sysname) + " " + u.release + " " + u.machine;
}

std::int64_t monotonic_ns() {
    struct timespec ts {};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return -1;
    }
    return static_cast<std::int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

std::int64_t clock_tick_ns() {
    struct timespec ts {};
    if (clock_getres(CLOCK_MONOTONIC, &ts) != 0) {
        return -1;
    }
    return static_cast<std::int64_t>(ts.tv_sec) * 1000000000LL + ts.tv_nsec;
}

}  // namespace vitals::platform
