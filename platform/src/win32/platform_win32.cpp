// SPDX-License-Identifier: Apache-2.0
// Windows implementation.
#include "vitals/platform/platform.hpp"

// MinGW's libstdc++ already defines NOMINMAX, so only define what is missing.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace vitals::platform {

namespace {

std::int64_t qpc_frequency() {
    static const std::int64_t freq = [] {
        LARGE_INTEGER f{};
        QueryPerformanceFrequency(&f);
        return static_cast<std::int64_t>(f.QuadPart);
    }();
    return freq;
}

}  // namespace

const char* name() { return "win32"; }

std::string os_description() {
    // GetVersionEx is deprecated and misreports without an application
    // manifest, so report the architecture only for now.
    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);
    const char* arch = si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64   ? "x86_64"
                       : si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64 ? "arm64"
                                                                                   : "other";
    return std::string("Windows ") + arch;
}

std::int64_t monotonic_ns() {
    LARGE_INTEGER c{};
    QueryPerformanceCounter(&c);
    const std::int64_t freq = qpc_frequency();
    const std::int64_t ticks = static_cast<std::int64_t>(c.QuadPart);
    // Split to avoid overflowing ticks * 1e9.
    return (ticks / freq) * 1000000000LL + (ticks % freq) * 1000000000LL / freq;
}

std::int64_t clock_tick_ns() {
    const std::int64_t freq = qpc_frequency();
    return freq > 0 ? 1000000000LL / freq : -1;
}

}  // namespace vitals::platform
