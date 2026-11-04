# Two firmware editions

[English README](../README.md) · [Français](../README.fr.md)

Both target the **Red Blade Full Size ISO, 320F:5136, SN32F248B, 109 LEDs**.
Both retain the factory base layout, three VIA layers, 1 ms requested keyboard
polling, 8-LED cooperative rendering and the original shared scan/PWM timer.

| | OpenRGB Core | RedBlade+ Full |
| --- | --- | --- |
| BIN | `redblade-fs-openrgb-core.bin` | `redblade-fs-aurora-via.bin` |
| Per-key OpenRGB / Aurora | Yes | Yes |
| VIA remapping/macros | Yes | Yes |
| No RGB for five seconds | LEDs off | Reactive white, or optional dark fallback |
| USB suspend / unconfigured | LEDs off | LEDs off |
| Local effects / startup wave | No | Yes |
| Added RGB status / lock lights | No | Yes |
| Recorder / keyboard Turbo / mouse Turbo | No | Yes |
| Dedicated Windows indicator | Off; no added Windows lock | On while Windows keys are disabled |
| Additional mouse HID report | No | Button-only report for click Turbo |
| Fn + Esc held two seconds | Bootloader | Bootloader; short press also stops/unlocks |
| Core source directory | `firmware/openrgb_core` | `firmware/aurora_via` |
| Separate full source archive | `redblade-openrgb-core-v0.8.1-complete-source.zip` | `redblade-plus-v0.8.1-complete-source.zip` |

Core is deliberately a separate keymap and source module. No Recorder arena,
Turbo scheduler, reactive buffer, mouse report or game status code is linked.
Its OpenRGB modes are Direct and Off. Full also offers a native standalone mode.

## Build

Follow [the pinned build environment](INSTALL.md), then choose one:

```sh
python tools/prepare_firmware.py /path/to/qmk_firmware --edition core
qmk compile -kb designedbygg/redblade/rbfs -km openrgb_core -e SKIP_GIT=yes
```

```sh
python tools/prepare_firmware.py /path/to/qmk_firmware --edition full
qmk compile -kb designedbygg/redblade/rbfs -km aurora_via -e SKIP_GIT=yes
```

Each binary has a separate manifest and checksum. Full has the local device
validation described in its manifest. Core's host/build checks are recorded
separately; do not treat Full's physical tests as Core hardware validation.

## Switching editions

VIA's existing layer survives a flash. Close RGB applications and VIA, flash the
desired BIN using [the installation guide](INSTALL.md#backup-and-flashing), then
install the matching layer map with a new private backup filename:

```powershell
python tools/install-fn-layer.py --edition core --apply --backup C:\private-path\before-core.json
# Or, after flashing Full:
python tools/install-fn-layer.py --edition full --apply --backup C:\private-path\before-full.json
```

The Core map retains only guarded Fn + Esc, clearing the known RedBlade+
shortcuts and the three retired commands. Base keys, third layer, VIA macros and
unrelated Fn assignments are preserved. The installer refuses a map for the
wrong firmware edition. Existing personal Fn commands may still exist; inspect
them in VIA if you want a completely empty custom layer.

Then use `tools/set-direct-mode.py --save` while clients are stopped, and restart
OpenRGB/Aurora. Temporary Recorder slots are cleared when flashing/resetting.

## Français

**Core** fournit uniquement OpenRGB/Aurora touche par touche et VIA, sans tes
fonctions ajoutées. Sans RGB depuis 5 secondes, les LED s'éteignent. **Full**
contient Recorder, Turbo clavier/souris, blanc réactif, vague et indicateurs.
Chaque édition possède son BIN, son manifeste et ses sources complètes propres.
Les deux gardent **Fn + Échap maintenu 2 secondes** pour le flash. Après un
changement d'édition, installe le calque Fn correspondant avec `--edition core`
ou `--edition full`, avec sauvegarde privée. Les tests physiques de Full ne
valident pas automatiquement Core.
