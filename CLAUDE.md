# qnxhack

Camera-based patient monitor for the QNX Global Hackathon, Robotics & Medical category.
It is a research prototype, not a medical device, and must say so in the README, the interface and the video.

## Read these first

- `docs/private/PLAN.md` is the full plan: purpose, architecture, machine roles, dependencies, schedule, risks.
- `docs/private/ENVIRONMENT.md` describes the machines, how to reach the QNX VM, and what is installed.
- `docs/private/` is git-ignored because it holds network addresses and machine details. Keep it that way. This repository will be public.

## What we are building

A monitor that estimates heart rate, heart rate variability, breathing rate and body temperature from up to nine cameras during a three-hour treatment, and warns the operator early enough to lower the dose rate before the patient stops the session.

- The response lags the dose change, so warn on the trend and on projected time to a limit, not only on limits already crossed.
- Three levels: advisory (trend turns), caution (limit projected to be crossed), alarm (limit crossed).
- Dose rate is a recorded input channel. Every change is a time-stamped event.
- Limits are set relative to a per-patient baseline recorded before dosing starts.
- Advisory only. The monitor never changes the dose.
- Cameras record the whole session without exception, for legal reasons. There is no pause feature. Log every gap and dropped frame with its cause.
- The patient may be the person who changes the dose rate. One camera covers the dose control and the hands, and every dose-change event links to that view.
- Movement, such as a washroom break, changes the dose rate. Treat movement events as likely dose changes and prompt for the rate to be re-entered.
- An abrupt jump with no dose change is a check-the-patient alert, not a dose advisory.
- The advisory must be readable from the treatment chair. An unacknowledged alarm goes on to a second person.
- Blood pressure is a fifth sign, from a contact sensor. Simulate it in the demo; a real device comes after the hackathon.

## Dates

- Submission closes December 15, 2026 at 11:00 am Pacific. Submit on December 13.
- Features freeze December 8.
- Milestone: nine-view demo with trend warnings running in the QNX VM by December 1.

## Architecture

One process per stage on QNX, talking by native message passing and pulses, with frame buffers in shared memory.

```text
sources (simulation client | file replay | QNX Camera API)
  -> vm-capture -> vm-dsp -> vm-fusion -> vm-core (resource manager: /dev/vitals, alarms, trend)
vm-supervisor watches a heartbeat from every stage and restarts one that stalls
consumers: Qt 6 interface, shell (cat /dev/vitals/patient0/hr), recorder
```

Planned layout:

```text
core/       pure C++17: signal processing, fusion, alarm and trend logic; no OS calls
platform/   IPC, shared memory, clocks, threads: qnx/ posix/ win32/ behind one interface
sources/    frame sources
services/   vm-capture, vm-dsp, vm-fusion, vm-core, vm-supervisor
ui/         Qt 6 Quick application
sim/        Isaac Sim scene, patient model, frame server, recorded sessions
images/     QNX image recipes
docs/       QNX_USAGE.md, build guides
tests/
```

## Conventions

- C++17 and CMake with presets: `linux-gcc`, `qnx-x86_64`, `qnx-aarch64le`, and on Windows `win-msvc-ninja` (MSVC from a developer prompt, VS 2022 or 2026), `win-mingw` and `win-msvc` (VS 2022 generator).
- `core/` depends only on the standard library and header-only maths. Its unit tests run on every platform.
- `platform/` is the only place for `#ifdef`. QNX uses native message passing; Linux and Windows use local sockets behind the same interface.
- The interface is a client of the pipeline over a socket, so it can run on another machine.
- Qt 6.10 on every platform, to match the QNX VM's packages. Draw camera frames and waveforms in custom Qt Quick items.
- Timestamp every frame at its source and carry the timestamp end to end.
- Keep `docs/QNX_USAGE.md` current: the hackathon requires a note naming the files and functions that use QNX.

## Rules

- No GPL dependencies in the build. That rules out FFTW and, pending a licence check, Qt Charts and Qt Graphs. Vendor a permissively licensed single-file FFT.
- No dataset video and nothing from `docs/private/` in the repository.
- The dose protocol is confidential. Model the dose rate as an opaque level or a plain changed event. Never ask for, record or describe protocol details, and keep them out of code, comments, the simulation and the video.
- Do not quote latency figures measured in the QEMU VM. Timing claims come from the Raspberry Pi 5 or bare metal.
- Do not type passwords. Hand `sudo` steps and password prompts to Dale to run.
- The repository licence is Apache-2.0, decided by Dale on November 14.

## Status on November 14, 2026

Done Nov 14:

- Repository at github.com/sharpshooter777/i-am-healthy, Apache-2.0. Commits use Dale's GitHub noreply address, set in this repository's git config.
- This laptop's SSH key logs in to the QNX VM.
- Development packages installed in the VM (Qt 6.10 with qmake6, Eigen, doctest, FFmpeg and Sensor Framework headers).
- CMake skeleton with the four presets. `vm-core --selftest` and the unit tests pass on Linux and in the QNX VM (`scripts/qnx-selftest.sh`). The `qnx-aarch64le` build compiles. On the Windows PC, `win-msvc-ninja` (VS 2026 with MSVC 19.52, and VS 2022 with MSVC 19.44) and `win-mingw` (GCC 13.1.0) pass all six tests; `win-msvc` needs VS 2022 and is untested.

Build notes:

- Use SDP 8.0.5: `source ~/qnx805/qnxsdp-env.sh` before the QNX presets.
- The QNX toolchain defines `_QNX_SOURCE`, and doctest is built with `DOCTEST_CONFIG_USE_STD_HEADERS`; both are needed for the SDP's libc++ under strict C++17.
- `cc: warning - lang-c++ is deprecated` comes from CMake's QCC support and is harmless.

- Week 1 started: `core/` has resampling, a pocketfft spectrum, region colour mean, POS pulse extraction and a sliding-window `PulseTracker`. `sources/` has the `FrameSource` interface, a synthetic face camera carrying true heart rate, and the `.vmrec` recording format with writer and replay reader. `tools/vm-offline` makes and analyses recordings. All tests pass on Linux and in the QNX VM.
- The synthetic camera is easy: whole-patch averaging recovers the rate down to about a tenth of the assumed pulse strength. Real and rendered cameras will be much harder.

Next, for week 1:

1. QNX IPC in `platform/`: native message passing and pulses, with a local-socket version for Linux and Windows.
2. `vm-core` as a resource manager serving `/dev/vitals/patient0/hr`.
3. Split the pipeline into `vm-capture` and `vm-dsp` and feed it from a recording, aiming for the week 1 exit test (`cat /dev/vitals/patient0/hr` within 3 bpm of truth).
4. Isaac Sim frame server for one camera, on the simulation host.

Also outstanding: install Qt 6.10 on this laptop.
