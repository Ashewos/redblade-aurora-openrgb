# RedBlade+ 0.4.0 — code review and validation

Review date: **2026-11-04**. Scope: custom firmware protocol/controls/keymap, Windows tools, Fn-layer migration, artifact contents and documentation. This is an implementation review with host tests, not an independent certification or a physical latency measurement.

## Findings addressed

| Area | Change / evidence |
| --- | --- |
| RGB packet boundaries | Whole-range validation before touching the 327-byte color array; at most 9 colors/report; malformed packets do not keep the stream alive. Exhaustive host boundary checks remain in place. |
| Timer wrap / stale frames | Unsigned elapsed-time checks, latched idle fallback and clearing stream state at USB suspension. Boot/Turbo timing and recovery are tested across wraparound. |
| Shared scan/PWM clock | Driver override preserves 720,896 Hz while using smaller rendering chunks. Keyboard pins, eager debounce and USB polling configuration remain unchanged. |
| Windows-key lock | Clears held GUI modifiers while retaining Ctrl/Alt/Shift. Filters standalone left/right GUI and simple GUI-modified QMK codes, including a modified GUI key as the basic code. |
| Media release ordering | Tracks generated consumer keys by physical position so releasing Fn/Shift first does not leave a key held. |
| Turbo ownership | One repeat target; normal target key presses cancel it before QMK handles the physical press. Held physical targets cannot be stolen by a remapped trigger. |
| Turbo scheduling | Cached 5/10/20 ms half-periods; no rate division, explicit delay or busy wait in the scheduler; at most one edge/pass, with no catch-up bursts. Forward hold needs no periodic report when unchanged. |
| Emergency stop | Esc stops generated keys; Fn+Esc also unlocks Windows. USB suspension/disconnection and defaults stop helpers. Bootloader needs the separate Fn+Shift+Esc two-second hold. |
| Preferences | Only explicit Fn+F9 writes the tagged four-byte QMK user word. Invalid/unrelated words load defaults. Game/helper state is never persisted. |
| Zero-brightness recovery | Fn+F8 restores the master setting and QMK engine brightness without EEPROM writes, including recovery from an externally set zero value. |
| LED mapping | All four media positions and E/F/R/forward/Space indices are checked against the pinned layout. Space is LED **99**, not a neighboring modifier. |
| Raw HID utilities | Device identity checks, read-only diagnostics, response-subcommand validation and enumeration cleanup on identity errors. Competing clients must be stopped. |
| Fn installer | Only 30 checked layer-1 cells; complete private backup before writes; read-back verification; attempted writes are included in rollback even when an ACK is lost. |
| Measurement tooling | Native and HID timings are labeled separately from physical key latency. Native-only launcher path was executed; HID requests are read-only and active game helpers cause an abort. |
| Source bundles | Only tracked QMK/dependency sources plus the selected keymap are packaged. Other local keymaps/notes are excluded; the approved timer patch, native artifact digest and source checkout are checked. Output cannot be inside a source tree. |

## Build and resources

- Board: Red Blade Full Size ISO, `320F:5136`, SN32F248B, 109 LEDs.
- SonixQMK: `62973ee48a976c6f7cc6d4f7b198bd48a2fdfe60`; ARM GCC 15.2.0 / qmk_toolchains a55a2b0.
- Raw image: 65,536 bytes including Sonix padding/flag; the linked program fits within this flash budget.
- Linked SRAM use: **7,700 / 8,192 bytes**, including 768-byte main stack, 1,536-byte process stack and linker alignment. **492 bytes remain unassigned.** This is 80 bytes more than the installed 0.1.0 build.
- Runtime stack high-water marks and exact MCU execution times have not been measured. Reserved stack sizes are not a proof of maximum runtime usage.

`tools/check_release.py --elf ...` checks the actual linker heap boundary, vectors, image flag, SHA-256, version and shortcut map. The [manifest](../dist/manifest.json) records artifact metadata.

## Verification performed

- Actual C handler and controls tests: VIA passthrough, 109 LED positions, 76,800 start/count boundaries, modes, red wave/handover, exact timeout, preferences, modifiers, physical release ordering, Turbo targets/rates/ownership/stop and guarded boot.
- Six Python tooling tests: mapping agreement, timing statistics, read-only benchmark commands, failed-ACK rollback, HID cleanup and response matching.
- Full ARM firmware compilation with warnings treated as errors.
- Native benchmark using the pinned QMK HSV → RGB implementation and CIE curve. The [reference result](performance-reference.json) is host CPU timing, not MCU timing.
- PowerShell parser check and native-only measurement launcher execution. The elevated `-Hid` service stop/restart path has not been exercised on the target machine.
- The GitHub workflow runs C tests with AddressSanitizer/UndefinedBehaviorSanitizer and runs the Python/tooling/native checks.

## Remaining limits

**0.4.0 has not been flashed on the target keyboard.** Real shortcuts, LED appearance, sustained Turbo under RGB load, sleep/wake, actual PC shutdown and physical key latency still need on-device checks. Earlier 0.1.0 typing/color validation is not reused as a pass for new features.

Standard QMK report transmission still depends on USB scheduling. Cooperative code and a 1 ms descriptor do not guarantee a particular physical input latency. Actual Turbo rates can be lower than the selected target; applications may coalesce events.

The firmware does not detect game focus. Helpers affect the active application. Game Mode does not filter arbitrary VIA macros or all advanced modifier actions. RGB Off/zero brightness also hides helper indicators, but Esc still cancels helpers.

The keyboard observes USB state rather than Windows shutdown. A bus kept configured and active after shutdown cannot be distinguished from missing RGB data alone. That host behavior needs power configuration or an explicit PC shutdown signal.

No firmware flashing, automated key output or HID performance run was started as part of this review. Corresponding sources/licenses are included in release bundles; personal backups and credentials are excluded.
