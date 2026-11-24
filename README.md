# i-am-healthy
82 Trillion to 1 - Home of QNX Hackathon

A camera-based patient monitor on QNX 8.0, for the QNX Global Hackathon (Robotics & Medical).

> **Research prototype. Not a medical device.** It has not been validated and must not be used for clinical decisions or in place of a contact monitor.

The monitor estimates heart rate, heart rate variability, breathing rate and body temperature from up to nine cameras, and real-time blood pressure from a contact sensor (simulated in the demo), during a long treatment session. It warns the operator early, on the trend and the projected time to a limit, so the dose rate can be lowered before the patient has to stop. It only advises; it never changes the dose.

Before the session it offers a conservative starting estimate of the dose rate from age and a health score. The live trend overrules it once the session is under way. It is not a safe maximum.

Work in progress. Today, on Linux, Windows and QNX 8.0: the build, `vm-core --selftest`, and heart rate measured from a synthetic camera recording and compared with its true values.

```sh
vm-offline synth session.vmrec --seconds 60 --hr 75 --slope 3
vm-offline analyse session.vmrec --limit 100
```

The synthetic camera is a stand-in for the simulated patient, and its accuracy says little about real cameras.

## Layout

```text
core/       pure C++17: signal processing, fusion, alarm and trend logic; no OS calls
platform/   OS services behind one interface: qnx/, posix/, win32/
sources/    frame sources: synthetic camera, recorded-session replay
services/   pipeline processes (vm-core so far)
tools/      desk tools: vm-offline makes and analyses recordings
tests/      unit tests, run on every platform
cmake/      QNX toolchain files
scripts/    helpers, such as running the tests on a QNX target
third_party/ vendored permissively licensed code (doctest, pocketfft)
```

## Building

Requires CMake 3.25 or newer and a C++17 compiler.

| Preset | Host | Target |
| --- | --- | --- |
| `linux-gcc` | Linux | Linux, GCC and Ninja |
| `win-msvc` | Windows | Windows, Visual Studio 2022 generator, x64 |
| `win-msvc-ninja` | Windows | Windows, MSVC from the open developer prompt (Visual Studio 2022 or 2026), Ninja |
| `win-mingw` | Windows | Windows, MinGW-w64 GCC and Ninja |
| `qnx-x86_64` | Linux or Windows with the QNX SDP | QNX 8.0 x86_64: the QEMU Developer Desktop or a PC |
| `qnx-aarch64le` | Linux or Windows with the QNX SDP | QNX 8.0 aarch64le: Raspberry Pi 5 |

### Linux

```sh
cmake --preset linux-gcc
cmake --build --preset linux-gcc
ctest --preset linux-gcc
./build/linux-gcc/bin/vm-core --selftest
```

### Windows

There are three Windows presets. Pick the one that matches the compiler you have.

`win-msvc-ninja` uses whichever MSVC the prompt provides, so it works with Visual Studio 2022 and 2026. Run it from an "x64 Native Tools Command Prompt", with Ninja on the `PATH`. Tested with Visual Studio 2026 (MSVC 19.52) and Visual Studio 2022 (MSVC 19.44).

```bat
cmake --preset win-msvc-ninja
cmake --build --preset win-msvc-ninja
ctest --preset win-msvc-ninja
```

`win-msvc` uses the Visual Studio 2022 generator and needs Visual Studio 2022 installed.

```bat
cmake --preset win-msvc
cmake --build --preset win-msvc
ctest --preset win-msvc
```

`win-mingw` uses MinGW-w64 GCC. Put `g++`, `ninja` and `cmake` on the `PATH` first; the MinGW `bin` folder must stay on the `PATH` to run the programs. Tested with GCC 13.1.0, the MinGW that the Qt installer provides.

```bat
cmake --preset win-mingw
cmake --build --preset win-mingw
ctest --preset win-mingw
```

### QNX 8.0

Tested with QNX SDP 8.0.5 (gcc 12.2.0) and the QNX Developer Desktop for QEMU, image of September 2, 2026.

```sh
source ~/qnx805/qnxsdp-env.sh        # Windows: <SDP>\qnxsdp-env.bat
cmake --preset qnx-x86_64
cmake --build --preset qnx-x86_64
scripts/qnx-selftest.sh qnx-x86_64 qnxuser@<target-address>
```

The script copies the binaries to `~/vitalmon/<preset>` on the target over SSH and runs the unit tests and `vm-core --selftest` there. It needs key-based SSH login. Use `qnx-aarch64le` for the Raspberry Pi 5.

## Heart rate from a chest strap

A Bluetooth chest strap gives the reference heart rate that the camera estimates are compared against. The strap talks to a machine with a Bluetooth radio; `hr-bridge` on that machine serves the samples over TCP and the QNX side connects out to it.

```sh
# on the machine with the strap (Linux or Windows)
./build/linux-gcc/bin/hr-bridge synth --bpm 72 --seconds 60     # made-up rhythm, no strap needed
./build/linux-gcc/bin/hr-bridge replay strap.raw                # a recorded session at its original pace
tail -f -n0 strap.raw | ./build/linux-gcc/bin/hr-bridge follow  # live, as the logger writes

# on QNX
./vm-core --hr-client <bridge-address>:5600
```

Each sample is checked on arrival: electrode contact as the strap reports it, and whether the beat-to-beat intervals agree with the reported rate. A reading is marked stale after three seconds without a sample. The strap log format is one line per Bluetooth notification: receive time in seconds, then the Heart Rate Measurement bytes in hex.

## QNX usage

See [docs/QNX_USAGE.md](docs/QNX_USAGE.md) for the files and functions that use QNX.

## Licence

Apache-2.0. See [LICENSE](LICENSE) and, for third-party code, [NOTICE](NOTICE).
