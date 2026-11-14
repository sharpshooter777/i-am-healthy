# QNX usage

The hackathon asks for a note naming the files, functions and output that use QNX. This file is kept current with every change that touches QNX.

Target: QNX 8.0, built with QNX SDP 8.0.5. Tested on the QNX Developer Desktop under QEMU (x86_64). The Raspberry Pi 5 (aarch64le) build compiles and is not yet run on hardware.

## Build

| File | What it does with QNX |
| --- | --- |
| `cmake/toolchains/qnx-common.cmake` | Sets `CMAKE_SYSTEM_NAME QNX` and uses the SDP's `qcc` and `q++` from `QNX_HOST`. Defines `_QNX_SOURCE` so QNX and POSIX declarations stay visible under strict C++17 |
| `cmake/toolchains/qnx-x86_64.cmake` | Compiler variant `gcc_ntox86_64` |
| `cmake/toolchains/qnx-aarch64le.cmake` | Compiler variant `gcc_ntoaarch64le` |
| `CMakePresets.json` | Presets `qnx-x86_64` and `qnx-aarch64le` |
| `platform/CMakeLists.txt` | Builds `platform/src/qnx/` when the target is QNX |
| `scripts/qnx-selftest.sh` | Copies a QNX build to a target over SSH and runs the tests there |

## Runtime

| File | Function | QNX interface |
| --- | --- | --- |
| `platform/src/qnx/platform_qnx.cpp` | `monotonic_ns()` | `ClockTime(CLOCK_MONOTONIC, ...)` kernel call: the timestamp given to every frame at its source |
| `platform/src/qnx/platform_qnx.cpp` | `clock_tick_ns()` | `ClockPeriod()` kernel call: the system tick that sets timer resolution |
| `platform/src/qnx/platform_qnx.cpp` | `os_description()` | `uname()`. QNX 8.0.x reports 8.0.0 (QNXE-554) |

## Output

`vm-core --selftest` on the QNX target prints `platform qnx` and the QNX version, then the results of the clock and trend checks:

```text
vm-core 0.1.0 self-test
  platform  qnx
  os        QNX 8.0.0 x86pc
  pass  monotonic clock reads
  ...
self-test passed
```

## Planned

Native message passing and pulses between pipeline stages, shared-memory frame buffers, the `/dev/vitals` resource manager, per-stage thread priorities, `slog2` logging, a supervisor that restarts stalled stages, and the QNX Camera API for real cameras.
