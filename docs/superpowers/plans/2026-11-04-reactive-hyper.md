# RedBlade+ 0.8: latched high speed and reactive white

Goal: extend Recorder/Turbo speed without a temporary boost, show physical and
replayed keys on a white fallback, and publish a verified firmware release.

Architecture: keep the cooperative schedulers, one generated edge per task,
fixed storage, existing USB polling and reserved stacks. Store speed selections
until a later adjustment/reset. Aurora retains priority in the default Host
profile. White fallback and explicit White support red key trails.

Tech Stack: pinned SonixQMK C, native C fixtures, Python tooling, SVG/Sharp docs.

Spec: latest owner instructions in this chat; 0.7 hardware validation passed for
Recorder, looping, cadence, removed shortcuts and Aurora integration. Three
volatile slots remain. No Fn+Up/Down or Fn+Enter actions return.

Global Constraints: no heap, no recording payload export, no automatic playback
on wake, no catch-up bursts, ordinary typing takes over. Preserve VIA base/macros,
stable custom keycodes and Fn+Esc two-second flash shortcut. Keep at least 64
unassigned SRAM bytes without reducing reserved stacks. Publish source and BIN.

Review Focus: rate-change release ordering; numerical overflow at 175/s;
modifier ownership; loop gap at Hyper; host RGB handover; lights off/suspend;
recording capacity and memory accounting; documentation versus actual behavior.

Owner addition: Fn+T then Volume- selects left-click Turbo, Volume+ right-click
Turbo, starting on release. Use amber left / purple right status on the volume
key and Fn. Reuse the single Turbo scheduler/rate and stop paths. Expose a minimal
shared QMK mouse HID report, no cursor acceleration engine or new background task.

## Task 1: Persistent rates

- [ ] Add failing real-controls tests for hold/release, high Turbo cadence,
  Recorder 4x/8x/Hyper, reverse stepping and the loop release gap.
- [ ] Implement bounded hold-repeat and latched rates. Tap or hold Fn+numpad +/-;
  plain numpad +/- is a speed control only during active generated playback.
- [ ] Run native controls/Recorder tests. Expected: all pass.

## Task 2: Reactive white

- [ ] Add failing rendering tests for physical/replayed keys, fade, host priority,
  output disable and suspension. Use the actual renderer.
- [ ] Add bounded per-LED fades. Generated replay uses the factory keyboard's
  usage-to-LED map in flash; physical typing uses its actual matrix position.
- [ ] Run all native/Python tests and ARM build. Expected: pass, >=64 bytes free.

## Task 3: Documentation and release preparation

- [ ] Update English/French guides, changelog, manifest, shortcut and feature SVGs.
- [ ] Render PNGs, inspect visually, validate documentation and build artifacts.
- [ ] Measure native performance; obtain an independent whole-change review and
  address material findings with failing regression tests first.

## Task 4: Device validation and publication

- [ ] Back up VIA, pause RGB clients, request physical boot entry, flash and verify.
- [ ] Restore services and validate new behavior with the owner. Record evidence.
- [ ] Publish exact verified source commit, successful CI and public release with
  BIN, full source, checksums, benchmark and both visual guides. Verify assets.
