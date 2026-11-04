# Changelog

All new release notes are written in English. Version 0.x is experimental; hardware validation is recorded separately from build and host-test results.

## 0.4.0 — 2026-11-04 — RedBlade+

### Added

- Project identity: **RedBlade+ — OpenRGB Firmware** and an updated keyboard/RGB/red-blade logo.
- Fn media deck: Calculator toggles Game Mode; Mute plays/pauses; Volume −/+ select previous/next track. Shift adds Windows unlock, media Stop and lighting brightness controls.
- Windows-key lock, including simple GUI-modified QMK shortcuts, with a red Calculator indicator.
- Turbo E/F/Space with red target LEDs, one target at a time, selectable 25/50/100 target presses/second and no catch-up bursts.
- Forward-key hold (W on QWERTY / Z on AZERTY) with a green indicator. Physical target keys take over; Esc stops helpers, and Fn+Esc also unlocks Windows.
- Local white/red profiles, host profile, lighting toggle, global brightness and profile cycling.
- Optional startup wave, optional four-key white fallback and selectable white/dark idle fallback.
- Manual red-wave replay, RAM defaults and explicit Fn+F9 lighting-preference saving. Game helpers are never saved.
- A checked 30-binding Fn map and a backed-up, verified layer-1 installer with rollback attempts.
- Read-only controls diagnostics, a Windows native benchmark, read-only HID timing tools and a code audit.
- English landing page/changelog and a complete French README switch.

### Changed

- Bootloader entry now requires **Fn+Shift+Esc held for 2 seconds**. Fn+Esc alone is the immediate stop/unlock shortcut.
- Fn arrows now control brightness/profile navigation; the old standalone hue/saturation/speed bindings are replaced.
- Master brightness scales host colors and status indicators as well as local effects.
- Game Mode and game helpers clear on USB suspension/disconnection. Helpers never resume automatically.

### Verified / limitations

- Actual C protocol, controls, malformed-packet boundaries, helper ownership/release order, timer wrap, preferences and tooling recovery are tested.
- Firmware builds for the pinned Red Blade FS board. Native timing results are host-only; physical key latency remains unmeasured.
- This release has not yet been flashed and physically verified on the target keyboard. Version 0.1.0 hardware checks do not count as validation of the new features.

## 0.3.0 — 2026-11-04

- Added the red startup/resume wave, white idle fallback, four-media-key fallback and explicit USB-state handling.
- Added project artwork and native host tests. Build-tested; not flashed on the target keyboard at publication.

## 0.2.0 — 2026-11-04

- Added the Direct-mode RGB watchdog, startup sweep and read-only firmware diagnostics.
- Added timer-wrap, timeout and suspend/resume host tests. Build-tested; not flashed on the target keyboard at publication.

## 0.1.0 — 2026-11-04

- Initial SonixQMK + VIA + VialRGB integration for the 109-LED Red Blade FS.
- Added Windows setup, native OpenRGB/Aurora integration and physical Aurora mapping.
- Programming/checksum, VIA/VialRGB discovery, LED positions, typing and Aurora colors were checked on one keyboard. Hardware latency was not measured.
