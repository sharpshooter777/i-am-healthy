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
| `platform/CMakeLists.txt` | Builds `platform/src/qnx/` when the target is QNX, and links QNX's `libsocket` for the network calls |
| `scripts/qnx-selftest.sh` | Copies a QNX build to a target over SSH and runs the unit tests, `vm-core --selftest`, and a synthetic recording through `vm-offline` |

## Runtime

| File | Function | QNX interface |
| --- | --- | --- |
| `platform/src/qnx/platform_qnx.cpp` | `monotonic_ns()` | `ClockTime(CLOCK_MONOTONIC, ...)` kernel call: the timestamp given to every frame at its source |
| `platform/src/qnx/platform_qnx.cpp` | `clock_tick_ns()` | `ClockPeriod()` kernel call: the system tick that sets timer resolution |
| `platform/src/qnx/platform_qnx.cpp` | `os_description()` | `uname()`. QNX 8.0.x reports 8.0.0 (QNXE-554) |
| `platform/src/bsd/net_bsd.cpp` | `connect_tcp()`, `recv_some()`, `send_all()` | QNX's `io-sock` network stack through `libsocket`: `socket()`, `connect()`, `poll()`, `recv()`. The QNX side opens the connection outward, so it works from behind the VM's NAT |
| `services/vm-core/hr_client.cpp` | `run_hr_client()` | Runs on QNX as `vm-core --hr-client <host>`: reads chest-strap heart rate sent by `hr-bridge` on another machine, times arrivals with `monotonic_ns()` (`ClockTime`), and marks a reading stale after three silent seconds |

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

The signal processing in `core/` and the frame sources in `sources/` are portable C++17 and use no QNX-specific calls; they are built for QNX with the toolchain above and tested on the target.

`vm-core --hr-client <host>` on the QNX target prints one line per heart-rate sample with its contact and interval checks, then a summary with the arrival-gap statistics:

```text
hr-client: connected to <bridge-address>:5600 (qnx)
hr  72 bpm  contact ok    rr 1 ok
...
hr-client summary
  samples           6
  ...
```

## Planned

Native message passing and pulses between pipeline stages, shared-memory frame buffers, the `/dev/vitals` resource manager, per-stage thread priorities, `slog2` logging, a supervisor that restarts stalled stages, and the QNX Camera API for real cameras.
