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
- Dose-ceiling estimate, in `vm-core`: before the session, show a predicted ceiling from age and a health score as an opaque level with a range. It starts conservative, the live trend overrules it once dosing starts, and it is never presented as a safe maximum. For the hackathon its model and numbers are invented. It is week 3 work and the first thing cut if time runs short.
- Blinking and drowsiness: eye openness from face landmarks gives blink rate, blink duration and the share of each minute the eyes are closed. Long eye closures together with a dropping head and slowing breathing raise a "may be falling asleep" advisory.
- If time runs short, cut in this order: dose-ceiling estimate, thermal channel, near-infrared camera, heart rate variability, then live nine-camera rendering in favour of replayed recordings.

## Cameras

- Main face camera: Razer Kiyo Pro, 1080p at 60 fps uncompressed over USB 3. For a session, set exposure, white balance and focus to manual and turn HDR off, because automatic adjustments look like a pulse. Analyse the live stream and store a compressed copy.
- On older Linux kernels the Kiyo Pro can stall the USB controller; the boot option `usbcore.quirks=1532:0e05:k` fixes it. It is untested on QNX 8.0. If it fails there, a Linux or Windows machine captures and sends frames to QNX over the network.
- QNX's camera samples are cloned outside the repository, in `~/qnx-samples/`. They are reference only: write our own code in `platform/qnx/` and `services/`. `camera_example1_callback` is the starting point for `vm-capture`.
- If code is adapted from a sample, keep QNX's copyright line in that file and add the sample and its licence (Apache-2.0) to `NOTICE`. Never copy `ai-camera-app/styleImages/`, which includes share-alike images.
- The samples do not show where frame timestamps come from. Find out early, because heart rate variability needs the capture time of every frame.

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
- No personal physiological readings in tracked files: no one's heart rate, RR intervals, heart rate variability or other measurements, and no personal details of how a sensor was fitted. They go in `docs/private/`.
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

Done Nov 15: Logitech C920 through the QNX Camera API in the VM

- The C920 (USB 046d:08e5) is passed from the laptop into the QNX VM and delivers frames through `camera_start_viewfinder`. It is `CAMERA_UNIT_3`; units 1 and 2 stay the simulated cameras. The setup survives a VM reboot.
- What it took: a QEMU EHCI controller (QNX's xHCI driver never enumerates the camera on QEMU's `qemu-xhci`), the package `qnx-sf-camera-usb`, and a sensor unit with `type = usb_camera`, `create_window = false` and `memory_allocator_type = from_sensor`. Without the last two, Screen in the VM refuses YCbYCr buffers and start fails with error 48 (ENOTSUP).
- The camera offers YCbYCr, MJPEG and others, 160x90 to 2560x1472, up to 30 fps.
- In the VM the frame rate is low and uneven (about 5 fps YCbYCr), and frame timestamps fall on a 32 ms grid set by QEMU's USB emulation. This is the emulated environment, not the camera or QNX. Do not use VM camera timing for heart rate variability.
- Full log, start script and probe programs: `docs/private/C920_QNX_TEST.md` on the laptop, with a copy in the VM at `~qnxuser/c920-setup/`.
- Next to discuss: the Linux capture fallback, where Linux captures the camera, timestamps each frame at capture and sends frames to QNX over the network. The real camera path gets its timing test on the Raspberry Pi 5.

Done Nov 15: Polar H10 chest strap as heart-rate ground truth

- The H10 works over standard Bluetooth LE (Heart Rate Measurement, 0x2A37), which carries heart rate and beat-to-beat (RR) intervals. Test details are in `docs/private/hardware/H10_TEST.md`.
- Poor electrode contact produces RR intervals that are noise, not beats. Any code that uses the H10 as ground truth must detect this, for example when the RR intervals disagree with the reported heart rate or vary implausibly from beat to beat.

Done Nov 23: heart-rate bridge

- `tools/hr-bridge` serves strap samples over TCP (modes `replay`, `follow`, `synth`); `vm-core --hr-client <host[:port]>` reads them, checks contact and RR plausibility, and marks a reading stale after 3 s of silence. Line format: `HR1 <t_ns> <bpm> <contact> <rr list>`.
- New: `core/heart_rate` (0x2A37 decoder, RR check, line codec), `core/line_buffer`, `sources/h10_log` (reads the gatttool logs), `platform/net` (BSD sockets for QNX and Linux, Winsock for Windows).
- The strap logger is `docs/private/hardware/h10_gatt_loop.sh ADDR SECONDS OUTFILE`. Do not stop it with `pkill -f`: over SSH that pattern matches the SSH command itself.
- Tested Nov 23 on Linux and in the QNX VM: unit tests pass on both (`scripts/qnx-selftest.sh`), and `vm-core --hr-client` in the VM received a replayed session from `hr-bridge` on the laptop. The VM reaches the laptop at the libvirt gateway address (see `docs/private/ENVIRONMENT.md`). Start the VM without a terminal with `mkqnximage --run=-h` (needs `DISPLAY`); that leaves the C920 with Linux.

Done Nov 23: sensor combining, core logic

- `core/fusion` (`HeartRateFusion`) implements the five combining rules agreed Nov 15, with a test per rule in `tests/core/test_fusion.cpp`. Sources are registered with a rank; `update()` takes each reading with the source's own quality verdict; `evaluate()` returns the value, the source in use, a status (`single`, `corroborated`, `disputed`, `no_source`) and whether an alarm is permitted (only when corroborated).
- `vm-core --hr-client` feeds the strap through it as the only source so far, so every line reads "single source, alarms held".
- Next: the camera pulse estimate as a second source, so a recording replays through strap + camera to a corroborated reading (the first beta).

Done Nov 24: camera pulse estimate, core and desk tool

- `core/grid_pulse` (`GridPulse`): per-block POS on a 32x18 colour grid, skin mask (mean red over mean blue), blocks ranked by a running record of peak sharpness, top 8 spectra summed, verdict from quality and block agreement. `tools/grid-pulse` reads rgb48le grids piped from FFmpeg.
- Checked against the chest strap on two ten-minute stretches of a seated reference recording (grids in /tmp on the laptop are scratch; regenerate with the FFmpeg line in the README). All estimates: 60 to 68 % within 3 bpm. With the default verdict (quality >= 0.25, agreeing >= 4): roughly a quarter believed, about nine in ten of those within 3 bpm.
- Wrong estimates are nearly always too low (45 to 60 bpm): slow movement at the bottom of the band. Tried and rejected: tilting the spectrum by a power of frequency (every tilt >= 2 pushed all estimates to the top of the band), a background-block reference, and whole-skin agreement counts (neither separated right from wrong).
- Not yet: exercise footage, the C920 view.

Done Nov 24: first beta, strap and camera through the combining rules

- `core/heart_monitor` (`HeartMonitor`) owns a `GridPulse` and a `HeartRateFusion`: strap is rank 1, camera rank 2. A camera estimate is made every 5 s and stands for two hops (per-source staleness, new in `add_source`). The camera's verdict is its quality check. `strap_quality_ok()` in `core/heart_rate` is the strap's, shared with `vm-core --hr-client`.
- `tools/session-replay` replays a strap log and a grid stream on one clock (`--grid-start` is the first frame's clock time) and counts, per second, corroborated / strap alone / camera alone / disputed / no number. `--strap-off` withholds the strap and scores the camera against it.
- On the two ten-minute seated stretches: corroborated (alarms permitted) for about a fifth to a quarter of the time, strap alone for the rest, disputed 1 to 3 %. Of the camera estimates it believed, 52 of 56 agreed with the strap. With the strap withheld for three minutes the camera carried about 30 s of it, within 5 bpm throughout; the remainder showed no number, by design.
- Same result on Linux and in the QNX VM; ten minutes of grids replay in about 32 s in the VM.
- Observed, not acted on: many camera estimates the camera itself doubts are in fact close to the strap. The verdict is strict. Whether a doubted reading that agrees with the strap may count as corroboration is an open design question (chance agreement must be ruled out first).
- Not yet: live camera frames on QNX into `HeartMonitor` (replay only so far), exercise footage, the C920 view.
- `session-replay synth` writes a made-up session; three ctest cases replay it end to end (agreeing, and with the strap withheld). The `--require-...` options turn the exit code into the test.

Done Nov 24: movement check

- Problem: on two treadmill recordings (three camera views) the camera believed about three estimates in four while running, and none agreed with the strap: it read step or stride rate.
- `GridPulse` now measures movement from the same frames: `movement` (RMS frame-to-frame change in relative brightness of the combined blocks) and `movement_hz` / `rhythm` (peak of the summed brightness spectra of all blocks, and its share of the 0.5 to 4.5 Hz band). Refused when `movement >= 0.02` whatever the rate, or when `movement >= 0.01`, `rhythm >= 0.40` and the pulse peak is within 0.1 Hz of the movement rate, half or double. Otherwise the verdict is exactly as before. `GridPulseEstimate` carries `movement`, `movement_hz`, `rhythm`, `moving` and `refusal` (why not believed; `to_string`). The evidence for each threshold is in the comment in `grid_pulse.hpp`.
- Rule choice: during strong movement no believed estimate agreed with the strap on any run, so strong movement refuses at any rate. The rate-match part was needed only for the wide camera, where much of the running stayed under 0.02.
- Results (believed / agreed; treadmill: believed and wrong as a share of all estimates): seated stretch 1 25/22 -> 24/22, stretch 2 31/30 -> 31/30. Treadmill Nov 23 wide 340 of 415 (82 %) -> 0; Nov 24 close 353 of 475 (74 %) -> 3 (0.6 %); Nov 24 wide 361 of 475 (76 %) -> 15 (3.2 %). None of these agreed with the strap, before or after.
- `session-replay --camera-log <file>` writes one line per estimate with the movement measures and the reason. The log holds strap readings, so keep it out of the repository.
- Tests use a made-up bobbing face (four bands of different colour and brightness). A plain one-colour patch does not reproduce the failure, because POS cancels pure brightness change.
- Not solved: (1) on the wide Nov 24 view, ten wrong estimates sit just under both limits at stride rate; (2) five wrong estimates there, and one on the close view, come with almost no movement, during the stillness after running. That failure is not movement, and the check cannot catch it. (3) The thresholds come from one person, two cameras and one room. (4) The seated one-hour recording of Nov 24 was not available and is untested. (5) A real pulse during strong movement is thrown away by design.

Done Nov 26: live camera frames into the monitor on QNX

- `tools/grid-bridge` serves the camera's 32x18 colour grids over TCP (port 5601), one text line per frame (`core/grid_line`: `GR1 <seq> <t_ns> <cols> <rows> <hex>`). `follow` reads FFmpeg's raw grid stream from standard input, `replay` serves a saved one at its frame rate. `vm-core --hr-client <host> --grid <host>` connects out to both bridges and runs strap and camera through one `HeartMonitor`; both are timed by arrival on the QNX clock. One loop polls both sockets (camera first, 10 ms; strap 2 ms).
- Frames carry a count. After lost frames or a new connection `HeartMonitor::camera_restart()` empties the camera window, because a window with a hole gives a wrong rate that can look believable. The camera's last estimate then goes stale in the usual way.
- Tests: `core_tests` has the line format and the restart; ctest `live-bridges` (Unix only, `tests/e2e/live-bridges.sh`) replays the made-up session through both bridges at four times speed and requires 15 agreeing samples (`--require-corroborated`).
- In the QNX VM, bridges on the laptop: made-up 90 s session, 2,671 frames, none lost, 30.00 frames/s as received, longest gap 41 ms, 60 of 90 samples corroborated. Ten minutes of the Nov 24 seated recording (minutes 10 to 20): 17,984 frames, none lost, longest gap 210 ms, 165 samples corroborated, 33 of 114 estimates believed. `session-replay` on Linux with the same data: 165 s corroborated, 33 of 115 believed, all 33 agreeing. The live path and the replay give the same answer.
- Live C920 through FFmpeg into `grid-bridge follow`, 100 s, empty chair: 2,971 frames, none lost, 30.01 frames/s, longest gap 200 ms; 14 estimates, none believed (`weak_peak`), which is right for a picture with nobody in it.
- The combined reading is still counted once per strap sample. With the strap gone and the camera carrying on, only the camera lines show it. A once-a-second evaluation belongs with the display and the supervisor.
- Not yet: a session with a person in front of the live camera; recording the full video and feeding the grid from one capture (the C920 can be opened once: either one FFmpeg with two outputs, where a stalled bridge must not stop the recording, or read the growing file); Windows build of `grid-bridge` (written for it, stdin set to binary, untested); what happens when `connect` to an unreachable host blocks the loop.
- The laptop's capture script must name the C920 by `/dev/v4l/by-id/...`: the `/dev/videoN` numbers change with what is plugged in (on Nov 26 `/dev/video2` was the built-in webcam).

Next, for week 1:

1. QNX IPC in `platform/`: native message passing and pulses, with a local-socket version for Linux and Windows.
2. `vm-core` as a resource manager serving `/dev/vitals/patient0/hr`.
3. Split the pipeline into `vm-capture` and `vm-dsp` and feed it from a recording, aiming for the week 1 exit test (`cat /dev/vitals/patient0/hr` within 3 bpm of truth).
4. Isaac Sim frame server for one camera, on the simulation host.

Also outstanding: install Qt 6.10 on this laptop.
