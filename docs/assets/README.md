# Shortcut maps

The maps show the 109-key Red Blade Full Size ISO layout and firmware 0.6.0 shortcuts. Colors identify available roles; they do not imply that every state is active simultaneously. Letter labels use French AZERTY positions.

Regenerate the editable SVG files from the pinned SonixQMK tree:

```sh
python tools/make-shortcut-map.py --qmk-tree /path/to/qmk_firmware
```

The generator reads the pinned board geometry and this project's actual base keymap, checks all matrix positions against `led-map.json`, and checks that Fn is the base-layer `MO(1)` key. Board/layout sources retain the SonixQMK GPL notices included in the complete source bundle. The project logo is documented separately in [assets/LOGO.md](../../assets/LOGO.md).

PNG exports are included for Markdown/release previews. They were rendered from these SVG files with Sharp 0.35.5; no AI-generated key positions or labels are used.
