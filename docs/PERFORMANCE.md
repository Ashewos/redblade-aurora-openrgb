# Performance checks

[Project](../README.md) · [Shortcuts](SHORTCUTS.md) · [Audit](AUDIT.md)

RedBlade+ requests 1 ms USB polling and preserves the shared Sonix matrix/PWM timer. Those are configuration facts, **not a measured 1 ms key latency claim**.

## Run tonight on Windows

Extract the release ZIP. It includes the tools and `dist/redblade-plus-benchmark-windows.exe`. Python 3.10+ and PowerShell 7 are required.

### 1. Native code check — RGB software keeps running

```powershell
pwsh -File tools/measure-performance.ps1
```

This runs the actual protocol handler, controls and pinned QMK color conversion on the PC. It measures average host CPU cost for 109-LED frames, dimming, local white and wave handover, plus the ordinary key-processing hook with Game Mode off/on. QMK hardware and USB transport are mocked. Use it to compare code changes on the same machine/build, not to predict microcontroller timing.

The [reference report](performance-reference.json) is a real native run on the preparation PC. It contains no HID or physical key-latency result. CPU-clock measurements are averaged over many iterations and include the fixture/call overhead.

### 2. Read-only USB request/reply timing

Press **Esc** to stop game helpers and close VIA, including browser tabs using it. Then:

```powershell
pwsh -File tools/measure-performance.ps1 -Hid -Samples 2000
```

Windows may request administrator permission to stop the existing OpenRGB service. The script records startup commands in memory, temporarily stops the service and current-session RGB clients, measures HID timing, and restores the applications in `finally`. Let the window finish. Reports are saved under `%LOCALAPPDATA%\RedBladePlus\Measurements`; the console prints the exact filename.

The HID test only sends RGB information **read** requests. It does not flash, change lighting, remap keys or start a game helper. It reports median, p95, p99 and maximum Windows → HIDAPI → firmware → Windows round-trip time, plus host-process CPU time. Competing RGB clients or active Turbo/forward hold cause an abort rather than silently contaminating the result.

For manual control of applications and output:

```powershell
python tools/benchmark.py --native-exe dist/redblade-plus-benchmark-windows.exe --output native-report.json
# With OpenRGB, Aurora and VIA already stopped:
python tools/benchmark.py --hid --samples 2000 --output hid-report.json
```

Output files use exclusive creation; existing measurements are not overwritten. Firmware 0.1.x can be measured for a baseline; richer diagnostics are available in later versions. Use the same USB port and system conditions for comparisons.

## What these checks cannot measure

Neither test measures mechanical switch actuation, matrix/debounce latency, the MCU's exact task execution time, game input processing or end-to-end display response. Native nanoseconds are PC timings. HID round trips are a different path from keyboard input.

For a physical comparison, use a controlled actuator/electrical trigger and USB analyzer or a high-speed acquisition setup, with the original and custom firmware under equivalent RGB load. No such measurement has been performed for this release.

The implementation bounds work: eight base LEDs per RGB task iteration, limited status overlays only when needed, at most nine incoming colors per packet, and at most one Turbo edge per housekeeping pass. Standard QMK report I/O still depends on USB scheduling. Turbo's 25/50/100 targets may run slower under load; missed pulses are not queued.

## Build the native benchmark yourself

With GCC/Clang and the repository root as working directory:

```sh
cc -std=c11 -O2 -DUSE_CIE1931_CURVE -I tests/stubs -I tests -I firmware/aurora_via \
  tests/benchmark.c tests/qmk_fixture.c tests/upstream/color.c tests/upstream/led_tables.c \
  firmware/aurora_via/redblade_rgb.c firmware/aurora_via/redblade_controls.c -o native-benchmark
./native-benchmark
```

The Windows release executable uses MinGW GCC with static runtime linkage. The sources, QMK color implementation and retained upstream licenses are included.
