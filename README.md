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
tools/      desk tools and the two bridges: hr-bridge (chest strap) and grid-bridge (camera) feed QNX
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

### Heart rate from a camera

`core/grid_pulse` reads a pulse from video without being told where the face is. Each frame is reduced to a 32 by 18 grid of block colours; every skin-coloured block is tried as a pulse sensor, and the blocks with the cleanest single peak are combined. The estimate carries its own verdict, so a camera that cannot see a pulse reports nothing instead of noise. FFmpeg does the capture and the reduction:

```sh
ffmpeg -i session.mkv -vf format=rgb48le,scale=32:18:flags=area -f rawvideo -pix_fmt rgb48le - | ./build/linux-gcc/bin/grid-pulse -
```

Each output line is: seconds into the stream, bpm, quality, agreeing blocks, skin blocks, verdict.

A moving body has rhythms of its own: on a treadmill the camera read the runner's steps or strides as a pulse and believed it. So the estimate also measures movement from the same frames: how much the chosen blocks' brightness changes from frame to frame, and the strongest brightness rhythm over the whole frame. During strong movement no estimate is believed, and during moderate, regular movement none is believed at the movement's rate, half of it or double it. When the patient is still, the verdict is unchanged. The estimate reports the movement level, its rate and the reason for any refusal, so the monitor can show "patient moving" instead of a number.

### Combining sensors

No single sensor is trusted alone. `core/fusion` applies five rules to every heart-rate source, strap or camera:

1. Use the highest-ranked source that is connected and believable.
2. Believable means its own quality check passed, the value is in a sane range, and it is not changing faster than a heart can.
3. When one source disagrees with others that agree, use the agreeing group and flag the odd one.
4. Always show which source is in use.
5. Never permit an alarm on one source's word.

### Strap and camera together

`core/heart_monitor` is the monitor as one object: strap samples and camera frames go in with their times, and the combined reading comes out with the source in use and whether an alarm is permitted. `session-replay` runs a recorded session through it and reports what the monitor would have shown, second by second:

```sh
ffmpeg -i session.mkv -vf format=rgb48le,scale=32:18:flags=area -f rawvideo -pix_fmt rgb48le - | \
  ./build/linux-gcc/bin/session-replay --strap strap.raw --grid - --grid-start <clock time of the first frame, epoch seconds>
```

Add `--strap-off <from s> <for s>` to withhold the strap for a stretch, as if it had fallen off: the monitor carries on with the camera, alarms held, and shows no number when the camera does not believe its own reading. The withheld strap readings score what the camera showed. `--quiet` prints the summary only. `--camera-log <file>` writes one line per camera estimate, with its movement measures and verdict. The same binary runs on QNX.

No recording to hand? `session-replay synth strap.raw grid.raw` writes a made-up session (replay it with `--grid-start 1000000000`); the test suite does exactly that.

### Live: strap and camera into QNX

The same monitor runs live. The machine with the camera reduces each frame to the 32 by 18 grid and `grid-bridge` serves the grids over TCP, one text line per frame; the full picture never crosses the network. `vm-core` on QNX connects out to both bridges and runs strap samples and camera frames through one `HeartMonitor`:

```sh
# on the machine with the strap and the camera (Linux or Windows)
tail -f -n0 strap.raw | ./build/linux-gcc/bin/hr-bridge follow
ffmpeg -f v4l2 -framerate 30 -i /dev/video0 -vf fps=30,format=rgb48le,scale=32:18:flags=area \
       -f rawvideo -pix_fmt rgb48le - | ./build/linux-gcc/bin/grid-bridge follow

# on QNX
./vm-core --hr-client <bridge-address>:5600 --grid <bridge-address>:5601 --reconnect
```

Each strap sample and each camera estimate is printed with its checks and the combined reading. The pulse is read from an unbroken run of evenly spaced frames, so every frame carries a count: after lost frames, or a new connection, the camera starts its 30-second window again instead of reading across the hole, and the summary reports frames lost and the frame rate as sent and as received.

To try it without a camera or a strap, replay a session through both bridges (`hr-bridge replay strap.raw` and `grid-bridge replay grid.raw`); both start when the client connects, so they stay in step. The test suite does this with the made-up session.

## QNX usage

See [docs/QNX_USAGE.md](docs/QNX_USAGE.md) for the files and functions that use QNX.

## Licence

Apache-2.0. See [LICENSE](LICENSE) and, for third-party code, [NOTICE](NOTICE).
