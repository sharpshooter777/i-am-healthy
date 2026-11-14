# i-am-healthy
82 Trillion to 1 - Home of QNX Hackathon

A camera-based patient monitor on QNX 8.0, for the QNX Global Hackathon (Robotics & Medical).

> **Research prototype. Not a medical device.** It has not been validated and must not be used for clinical decisions or in place of a contact monitor.

The monitor estimates heart rate, heart rate variability, breathing rate, real time blood pressure and body temperature from up to nine cameras during a long treatment session. It warns the operator early, on the trend and the projected treatment time to a limit, so the dose rate can be lowered before the patient has to stop. It only advises; it never changes the dose.  It can predict the maximum dose rate the patient can handle based on age and 'Health'.

Work in progress. The build skeleton and `vm-core --selftest` run on Linux and QNX 8.0 today.

## Layout

```text
core/       pure C++17: signal processing, fusion, alarm and trend logic; no OS calls
platform/   OS services behind one interface: qnx/, posix/, win32/
services/   pipeline processes (vm-core so far)
tests/      unit tests, run on every platform
cmake/      QNX toolchain files
scripts/    helpers, such as running the tests on a QNX target
third_party/ vendored permissively licensed code (doctest)
```

## Building

Requires CMake 3.25 or newer and a C++17 compiler.

| Preset | Host | Target |
| --- | --- | --- |
| `linux-gcc` | Linux | Linux, GCC and Ninja |
| `win-msvc` | Windows | Windows, Visual Studio 2022, x64 |
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

From a Developer Command Prompt for Visual Studio 2022:

```bat
cmake --preset win-msvc
cmake --build --preset win-msvc
ctest --preset win-msvc
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

## QNX usage

See [docs/QNX_USAGE.md](docs/QNX_USAGE.md) for the files and functions that use QNX.

## Licence

Apache-2.0. See [LICENSE](LICENSE) and, for third-party code, [NOTICE](NOTICE).
