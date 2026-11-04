# Contributing to RedBlade+

Target: Designed by GG Red Blade **Full Size ISO**, USB `320F:5136`, SN32F248B, 109 LEDs. Other models require their own board validation and firmware.

For a report, include firmware version, OpenRGB/Aurora versions, USB connection type, reproduction steps and expected/actual behavior. `python tools/redblade.py status` provides read-only diagnostics after RGB clients are stopped. Keep personal VIA backups private.

Before proposing firmware changes:

```sh
python3 -m unittest discover -s tests -p 'test_*.py'
python3 tools/check_fn_map.py
for suite in protocol controls rendering recorder speed_typing keymap_hooks; do
  cc -std=c11 -Wall -Wextra -Werror -O1 -g -DUSE_CIE1931_CURVE -fsanitize=address,undefined \
    -I tests/stubs -I tests -I tests/upstream -I firmware/aurora_via \
    tests/${suite}_test.c tests/qmk_fixture.c tests/upstream/color.c tests/upstream/led_tables.c \
    firmware/aurora_via/redblade_rgb.c firmware/aurora_via/redblade_controls.c firmware/aurora_via/redblade_recorder.c -o /tmp/${suite}-test
  /tmp/${suite}-test
done
```

Build against the pinned QMK commit using `tools/build.sh`. Check flash/RAM usage, maintain bounded cooperative work, and preserve the shared matrix/PWM timer. Shortcut changes must update `fn-shortcuts.json`, `docs/SHORTCUTS.md` and tests together.

Describe host tests and real hardware tests separately. Do not infer physical key latency from native or HID round-trip benchmarks. Do not mark an unperformed hardware check as passed.

Release bundles include the exact source commit, raw firmware, manifest, SHA-256, native benchmark, corresponding QMK/dependency sources and licenses. No personal backups, credentials, Git metadata or intermediate build objects belong in an asset.
