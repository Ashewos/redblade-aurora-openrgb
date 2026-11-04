# RedBlade+ 0.6.0 — code review and validation

Review date: **2026-11-04**. Scope: custom firmware protocol/controls/keymap, Windows tools, Fn-layer migration, artifact contents and documentation. This is an implementation review with host tests, not an independent certification or a physical latency measurement.

## Findings addressed

| Area | Change / evidence |
| --- | --- |
| RGB packet boundaries | Whole-range validation before touching the 327-byte color array; at most 9 colors/report; malformed packets do not keep the stream alive. Exhaustive host boundary checks remain in place. |
| Timer wrap / stale frames | Unsigned elapsed-time checks, latched idle fallback and clearing stream state at USB suspension. Boot/Turbo timing and recovery are tested across wraparound. |
| Shared scan/PWM clock | Driver override preserves 720,896 Hz while using smaller rendering chunks. Keyboard pins, eager debounce and USB polling configuration remain unchanged. |
| Windows-key lock | Clears held GUI modifiers while retaining Ctrl/Alt/Shift. Filters standalone left/right GUI and simple GUI-modified QMK codes, including a modified GUI key as the basic code. Fn+Windows toggles it; both physical Windows LEDs are red while locked. |
| Media release ordering | Tracks generated consumer keys by physical position so releasing Fn/Shift first does not leave a key held. |
| Turbo ownership | One repeat target; normal target key presses cancel it before QMK handles the physical press. Held physical targets cannot be stolen by a remapped trigger. |
| Turbo scheduling | Cached interval/remainder for 5–100 presses/s, fractional milliseconds distributed across edges; no rate division, explicit delay or busy wait in the scheduler; at most one edge/pass, with no catch-up bursts. Forward hold needs no periodic report when unchanged. |
| Emergency stop | Esc stops generated keys; Fn+Esc also unlocks Windows. USB suspension/disconnection and defaults stop helpers. Bootloader needs a two-second Fn+Esc hold; a short press only stops/unlocks. |
| Preferences | Only explicit Fn+F9 writes the tagged four-byte QMK user word. Invalid/unrelated words load defaults. Game/helper state is never persisted. |
| Zero-brightness recovery | Fn+F8 restores the master setting and QMK engine brightness without EEPROM writes, including recovery from an externally set zero value. |
| LED mapping | All four media positions, Fn, T, numpad −/+ and E/F/R/forward/Space indices are checked against the pinned layout. Space is LED **99**, not a neighboring modifier. |
| Raw HID utilities | Device identity checks, read-only diagnostics, response-subcommand validation and enumeration cleanup on identity errors. Competing clients must be stopped. |
| Fn installer | Only 33 checked layer-1 cells; complete private backup before writes; read-back verification; attempted writes are included in rollback even when an ACK is lost. |
| Measurement tooling | Native and HID timings are labeled separately from physical key latency. Native-only launcher path was executed; HID requests are read-only and active game helpers cause an abort. |
| Source bundles | Only tracked QMK/dependency sources plus the selected keymap are packaged. Other local keymaps/notes are excluded; the approved timer patch, native artifact digest and source checkout are checked. Output cannot be inside a source tree. |

## Build and resources

- Board: Red Blade Full Size ISO, `320F:5136`, SN32F248B, 109 LEDs.
- SonixQMK: `62973ee48a976c6f7cc6d4f7b198bd48a2fdfe60`; ARM GCC 15.2.0 / qmk_toolchains a55a2b0.
- Raw image: 65,536 bytes including Sonix padding/flag; the linked program fits within this flash budget.
- Linked SRAM use: **7,724 / 8,192 bytes**, including 768-byte main stack, 1,536-byte process stack and linker alignment. **468 bytes remain unassigned.** This is 24 bytes more than 0.4.0 and 104 bytes more than the original 0.1.0 build.
- Runtime stack high-water marks and exact MCU execution times have not been measured. Reserved stack sizes are not a proof of maximum runtime usage.

`tools/check_release.py --elf ...` checks the actual linker heap boundary, vectors, image flag, SHA-256, version and shortcut map. The [manifest](../dist/manifest.json) records artifact metadata.

## Verification performed

- Actual C handler and controls tests: VIA passthrough, 109 LED positions, 76,800 start/count boundaries, modes, red wave/handover, exact timeout, preferences, modifiers, physical release ordering, Turbo targets/rates/ownership/stop and guarded boot.
- Six Python tooling tests: mapping agreement, timing statistics, read-only benchmark commands, failed-ACK rollback, HID cleanup and response matching.
- Full ARM firmware compilation with warnings treated as errors.
- Native benchmark using the pinned QMK HSV → RGB implementation with no second CIE curve for the custom renderer. The [reference result](performance-reference.json) is host CPU timing, not MCU timing.
- PowerShell parser check and native-only measurement launcher execution. The elevated `-Hid` service stop/restart path has not been exercised on the target machine.
- The GitHub workflow runs C tests with AddressSanitizer/UndefinedBehaviorSanitizer and runs the Python/tooling/native checks.

## Remaining limits

**0.6.0 was flashed with verified programming/checksum (0x819a), VIA/VialRGB identity, 109 positions and all 33 Fn bindings.** The user confirmed Caps/Num fallback, the corrected Fn/Right-Ctrl placement, both Windows lock LEDs and the Calculator guide. On 0.5.0, wave/media lighting and generic Turbo/live cadence/stop were user-confirmed. Sustained RGB/Turbo load, sleep/wake, actual PC shutdown, runtime stack usage and physical input latency remain unmeasured. The new Fn+Esc hold is host-tested but has not been used to re-enter the bootloader on 0.6.0.

Standard QMK report transmission still depends on USB scheduling. Cooperative code and a 1 ms descriptor do not guarantee a particular physical input latency. Actual Turbo rates can be lower than the selected target; applications may coalesce events.

The firmware does not detect game focus. Helpers affect the active application. Game Mode does not filter arbitrary VIA macros or all advanced modifier actions. RGB Off/zero brightness also hides helper indicators, but Esc still cancels helpers.

The keyboard observes USB state rather than Windows shutdown. A bus kept configured and active after shutdown cannot be distinguished from missing RGB data alone. That host behavior needs power configuration or an explicit PC shutdown signal.

The 0.4.0 flash and Aurora reconnection were performed before this review. No automated key output was started by the tooling. Read-only HID diagnostics were measured on 0.5.0 and installed 0.6.0 with RGB clients stopped and helpers off: median 1.9966 ms / 1.9966 ms; p99 2.0222 ms / 2.0134 ms. See [the scoped comparison](hid-reference.json); these are not physical input timings. Corresponding sources/licenses are included in release bundles; personal backups and credentials are excluded.

## 0.5.0 regression checks

- Reproduce a Direct-mode request with every unused scalar field zero: engine brightness is preserved. Compile the actual eight-LED custom-effect include with pinned QMK color code; assert a moving red crest, all four media outputs at RGB 64/64/64, linear host brightness and black output at USB suspension.
- Generic selection resolves the VIA base layer only during the armed choice. It swallows the choosing press/release, starts on release, expires after ten seconds and preserves Esc/boot stop behavior. Unsupported consumer/system/power actions are not executed.
- Keyboard-report ownership prevents taking a physically held target; Turbo and forward hold cannot own the same generated usage. Physical target press cancels the helper. All non-trigger Fn actions capture release, so adjusting cadence after releasing Fn cannot inject a stray target release.
- Live rate changes are RAM-only, clamped to 5–100/s and divided only on a user action. Host tests exercise intermediate 60/s scheduling, bounds, timer wrap, stop before target release and Fn status priority. Persistent Fn status adds bounded overlay work; native benchmark includes it.
- Fn RGB is the ordinary matrix LED at index 102 (base-layer MO(1)); 103 is Right Ctrl. The previous test checked a hard-coded position but did not verify the base-layer identity; semantic checks now cover Fn, both Windows keys and all lock indicators. B12 is a separate monochrome indicator; no unsupported GPIO color control is claimed.

## 0.6.0 dynamic indicators and shortcuts

- Correct the Fn RGB index using the actual base-layer MO(1), rather than a manually assumed coordinate. Right Ctrl returns to normal host lighting. Both Windows indicators are checked against their base assignments too.
- Fn+Windows owns Game Mode. Fn+Calculator toggles a bounded, RAM-only 20-second shortcut guide; it does not lock Windows or start automation. Escape, defaults and USB suspension clear it. A timer-wrap-safe expiry is tested.
- Fn+Esc stops/unlocks immediately and enters the bootloader only after a continuous two-second hold. Either Fn or Esc release cancels; Shift remains an optional alias. Test coverage includes exact hold boundary and cancellation.
- Caps/Num/Scroll backlights only override the host profile’s white idle fallback, after five seconds without RGB. Fresh packets, a chosen local profile, native effects, Off, zero master brightness, dark-idle choice and USB suspension take priority. All 17 numpad LEDs become very dim white with Num Lock off.
- Rendering tests call the actual shared overlay renderer. They verify Caps/Num green, Scroll amber, 17 dim numpad keys, host takeover, Windows-red lock, Fn-versus-Right-Ctrl placement and zero output in Off mode. Settings are read from QMK’s host lock state; no host process, polling service or synthesized lock-key press is added.
- Diagrams are generated from the pinned board’s physical layout and actual base keymap, with all 109 positions checked, and exported in English/French. They depict roles, not simultaneous active states.
