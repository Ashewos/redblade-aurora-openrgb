# RedBlade+ 0.8.0 — code review and validation

Review date: **2026-11-04**. Scope: actual protocol, controls, Recorder, keymap/rendering, Windows tools, migration, source packaging and documentation. Build/host results and on-device checks are recorded separately.

## Changes and evidence

| Area | Implementation / checks |
| --- | --- |
| RGB bounds | Validate the entire 9-color packet range before changing the 327-byte host buffer. Exhaustive 76,800 start/count boundary cases remain covered. |
| Direct brightness | Ignore unused global HSV/speed fields in Direct mode. Linear QMK conversion avoids double CIE dimming. Actual custom-effect tests cover zero scalar fields, red wave geometry, visible four-media fallback and host handover. |
| USB/watchdog | Unsigned elapsed time, five-second fallback, stale-frame clearing and explicit USB-state hooks. Suspension/unconfigured has priority over waves, fallback and overlays. Diagnostics do not renew the watchdog. |
| Scan / input | Requested polling remains 1 ms; factory eager debounce, NKRO and the shared 720,896 Hz matrix/PWM timer are preserved. Eight base LEDs are rendered per cooperative task; no added timer stops key scanning. |
| Windows / releases | Both Windows RGB indices and Fn are checked against actual base-layer assignments (Fn is LED 102; Right Ctrl is 103). All non-trigger Fn actions own their physical release. Held GUI modifiers are cleared without clearing Ctrl/Alt/Shift. |
| Turbo | Cached interval/remainder; one edge per housekeeping pass, no division in that scheduler, no burst catch-up. Physical target presses take over. Forward hold and Turbo cannot own the same usage. |
| Recorder storage | Three offset/length slots share a fixed 192-byte byte arena. A typical key edge uses two bytes; delays >=255 ms use four. Replacing one slot compacts the arena once on a user action and preserves the other slots. No allocator or EEPROM writes are used. |
| Recorder capacity | Each capture reserves space to finish all currently held keys. Full memory or the 30-second limit saves a balanced sequence; it does not release a still-physical key from the live keyboard report. Multi-key-full and long-held-key cases are tested. |
| Recorder ownership | Recording observes accepted basic key edges and physical modifiers. Playback refuses a nonempty keyboard/held modifiers, owns a private bitset of generated usages and releases only those on stop. A real key takes over before QMK processes it. Media/Fn/advanced macro actions are not recorded. |
| Recorder timing | Millisecond edge deltas; 1x is exact before USB scheduling. Cached Q8 multipliers implement target 0.5x–8x plus Hyper speeds, with rounded intermediate ratios. Playback emits at most one edge per pass and resets timing to the actual edge, never draining missed events in a burst. |
| Loop chord | Explicit Shift+Fn+slot playback waits for Fn/Shift release rather than clearing real modifiers. A physical Shift stops an existing loop and retains that stop gesture until release, so the same loop shortcut toggles off in either Fn/Shift order. Pending playback retains its GUI-lock check. |
| Loop boundary | Release generated usages between passes; minimum 5 ms inter-loop gap is retained across live speed changes. Looping is an explicit runtime flag, never persisted or automatically resumed. |
| Lock / lifecycle | Game Mode refuses a recording containing GUI events, including a deferred loop. Enabling Game Mode stops Recorder. Simple USB suspension stops and can preserve completed RAM slots; USB reset/deconfiguration clears the arena. |
| Stops / defaults | Esc finishes capture or stops playback; short Fn+Esc also unlocks. Holding Fn+Esc two seconds enters the bootloader; releasing either key cancels. F8 resets lighting/rates/speed and retains current RAM recordings; F9 saves lighting only. |
| Privacy / tools | FE82 exposes state, sizes, speed and errors, not the recorded key/timing payload. HID benchmarks are read-only and reject active/armed/waiting Recorder/Turbo/forward states. Personal backups remain outside the repository. |
| Migration | Check 33 current Fn bindings plus 3 explicit clears for removed Up/Down/Enter commands. Full private VIA backup before writes, ACK/readback validation and rollback including a write whose ACK was lost. |
| Packaging / diagrams | Exact clean project commit, pinned QMK and only the approved timer driver patch; include tracked dependency sources and the selected keymap. Other untracked keymaps/notes, backups, credentials, Git metadata and build caches are excluded. EN/FR diagrams use the actual 109-position layout. |

## 0.8 edition and speed changes

- Full: persistent tap/hold rates, 400 ms initial hold and 150 ms repeated steps.
  Turbo targets 5–250/s, 16-bit fractional accumulation; Recorder adds 4x, 8x and
  Hyper (one edge per millisecond target). Loop gap stays at least 5 ms.
- Mouse Turbo sends button-only shared HID reports through the existing Turbo
  scheduler. Left/right volume targets use amber/purple LEDs and matching Fn.
  Stops release the generated button. The mouse report adds SRAM overhead, so
  the Recorder arena is 192 bytes. Stacks remain 768 + 1536 bytes.
- Reactive white stores 109 four-bit frame lifetimes in 55 bytes plus a count.
  Physical input uses its actual matrix position; replay uses a factory lookup
  in flash. Trails are absent over live host RGB and cleared across Off/suspend.
- Core is a separately linked module with RGB/VIA and guarded boot entry only.
  It has no game/Recorder/mouse/reactive modules or local animation. Its five
  second RGB watchdog goes dark. [Edition manifests and source bundles](EDITIONS.md)
  distinguish build/host checks from actual hardware validation.
- Local native benchmark includes reactive-white frames. These PC timings do
  not measure switch latency, MCU time, USB load or game acceptance at high rates.

## Resources and performance

- Board: Full Size ISO `320F:5136`, SN32F248B, 109 LEDs. SonixQMK `62973ee48a976c6f7cc6d4f7b198bd48a2fdfe60`, ARM GCC 15.2.0.
- Raw image: 65,536 bytes with Sonix flag/padding; vectors, size, SHA-256, version, keymap and actual linker heap boundary are checked by `tools/check_release.py`.
- Reserved stacks remain **768 bytes main + 1,536 bytes process**. The manifest reports total linked SRAM including these reservations and alignment. The artifact validator requires at least **64 bytes unassigned** after all static data and stack reservations; this unassigned amount is not the stack capacity.
- Recorder arena is fixed at 192 bytes plus bounded metadata. No dynamic allocation path was added. The [native reference](performance-reference.json) includes ordinary key hooks, 109-LED frames and idle/active-loop Recorder task costs on the PC, with transport mocked.
- [USB reference](hid-reference.json) measures information-request round trips with clients stopped and helpers off. It does not measure physical switch input, key scan/debounce delay, active RGB/Recorder load or the MCU's exact execution time.
- Runtime stack high-water marks, physical key latency and sustained RGB/Recorder stress remain unmeasured. Standard QMK report I/O still depends on USB scheduling.

## Validation performed

- Six actual-C suites: protocol, controls, rendering, Recorder, high-speed/reactive/mouse, and independent Core. Coverage includes exact timings, >255 ms gaps, wraparound, Ctrl/GUI ownership, live speed, full buffers with held keys, slot compaction, single/loop playback, Shift chord release/toggle, sleep stop/reset wipe, dynamic fallback and RGB host priority.
- Eleven Python checks cover mapping agreement, percentiles, read-only timing, active Recorder refusal, explicit removals, lost-ACK rollback, HID cleanup/response matching, edition migration/refusal and checksum update order.
- Full ARM build with warnings as errors, native Windows C tests and native benchmark. GitHub runs C suites with AddressSanitizer/UndefinedBehaviorSanitizer and the Python/artifact/native checks.
- On 0.6.0, programming/checksum, protocol, positions and Fn map were verified; the user confirmed Caps/Num fallback, corrected Fn placement, Windows LEDs and Calculator's previous guide. On 0.5.0, visible wave/media lighting and generic Turbo/live rate/stop were confirmed. Those are historical checks, not automatic passes for Recorder.
- Current on-device Recorder validation is recorded in [the manifest](../dist/manifest.json). The owner confirmed capture/replay/loop/speed/indicators, removed shortcuts, typing, numpad and Aurora on the installed 0.7.0 local build. Current 0.8.0 hardware checks are recorded separately in the manifest.

## Practical limits

An independent review found two ownership/mode defects before publication.
Regressions now verify that VIA modified/advanced physical actions stop Recorder
before QMK processes them, including a waiting loop; layer holds remain control
gestures. Core intercepts the standard VIA effect channel, forces Direct after
remapped mode keys and disables configurable native effects, so its RGB watchdog
cannot be bypassed by selecting a local animation. Manifest refreshes preserve
both editions' checksum entries in either update order. Each correction was
reproduced by a failing test before the passing implementation.

Approximately 48 simple presses are shared across all three Recorder slots; holds/pauses/modifiers consume variable space. One recording lasts at most 30 seconds. Basic physical keyboard/modifier edges are supported, not text capture, mouse input or arbitrary QMK macros. Host language/lock state and application focus still apply. Ordinary physical input cancels playback; it does not pause/resume it.

USB state does not reveal Windows shutdown when the host leaves the bus genuinely configured and active. Sleep/wake, actual PC shutdown and power-cycle behavior still need target-host checks. Game Mode is not a filter for every arbitrary VIA/advanced action. All RGB status respects Off/brightness zero/USB suspension, while keyboard stops continue to work.

## Current device result

Full 0.8.0 was flashed on the target Red Blade with programming/checksum
**0x34ed** verified. VIA 13, VialRGB 1, 109 unique LED positions and the 33 Fn
bindings/three explicit removals were verified; base/third layers and VIA macros
were preserved. The owner confirmed physical/replayed red trails on white,
latched Hyper speed/reverse adjustment, left/right mouse targets, their
amber/purple indicators and stops. OpenRGB/Aurora were restored and the owner
confirmed host priority. Core remains build/host-tested only. Physical latency,
power-cycle/shutdown, runtime stack high-water and sustained high-rate RGB load
are not claimed as tested. See both manifests and the scoped HID comparison.
