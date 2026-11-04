# 0.8.1: dedicated Windows-lock indicator

Goal: the small upper-right Windows indicator reflects disabled Windows keys,
while both Windows key RGB backlights keep their existing red locked state.

Architecture: reuse the board's existing active-low B12 indicator, named
LED_FN_PIN upstream, after physical identification. Update it only on lock or
lifecycle changes. Remove Fn-layer ownership of the dedicated indicator. Core
has no added Windows lock and leaves this indicator off. No new timer or buffer.

Tech Stack: pinned SonixQMK C, native real-keymap/controls hook test, ARM builds.
Spec: owner's explicit upper-right Windows-logo request in this chat.
Global Constraints: keep Fn+Windows behavior, RGB Fn hints, USB/Off semantics,
both editions/BINs/source releases, reserved stacks and polling unchanged.
Review Focus: every former B12 writer, lock versus Fn release, reset/suspend/wake,
Core's absence of a game-mode state, source/BIN correspondence.

- [ ] Confirm Fn-only lights the requested physical indicator.
- [ ] Run an actual keymap-hook test failing against current behavior.
- [ ] Implement lock-only ownership and update Core; run all relevant tests.
- [ ] Build both editions, audit GPIO paths, refresh docs/manifests and benchmark.
- [ ] Back up, flash Full, confirm hardware behavior and restore Aurora.
- [ ] Publish a new immutable patch release with both editions and sources.
