# RedBlade+ shortcuts

[Project](../README.md) · [Installation](INSTALL.md) · [Performance checks](PERFORMANCE.md)

These are the **default firmware 0.4.0 bindings**. Fn is the key between Right Windows and Right Ctrl. Base-layer typing, Calculator, Mute and Volume keys retain their usual behavior.

If an older VIA Fn layer was preserved when flashing, install the new bindings with the backed-up [Fn layer installer](#upgrading-an-existing-via-layer). The checked machine-readable map is [fn-shortcuts.json](../fn-shortcuts.json).

## Start here

1. **Fn + Calculator** toggles Game Mode. Calculator glows red while Windows keys are locked.
2. **Fn + E** starts repeating E. Its LED turns red. Press the same shortcut again to stop.
3. **Esc** immediately stops Turbo and forward hold. **Fn + Esc** also unlocks Windows keys.
4. **Fn + F1** restores OpenRGB control. **Fn + F8** restores the default lighting settings and stops game helpers.

The firmware cannot detect game focus: helpers send keys to the active application. They start disabled and stop on USB suspension/disconnection. They never resume automatically.

## The four keys above the numpad

| Key | Normal | With Fn | With Fn + Shift |
| --- | --- | --- | --- |
| Calculator | Open calculator | Toggle Game Mode | Unlock Windows keys |
| Mute | Toggle audio mute | Play / pause | Stop playback |
| Volume − | Lower volume | Previous track | Dim keyboard lighting |
| Volume + | Raise volume | Next track | Brighten keyboard lighting |

With Fn held, these four LEDs show their secondary roles. Game Mode keeps Calculator red. Indicators respect lighting Off, zero brightness and USB suspension.

## Game helpers

| Shortcut | Action | Active indicator |
| --- | --- | --- |
| Fn + E | Toggle Turbo E | E turns red |
| Fn + F | Toggle Turbo F | F turns red |
| Fn + Space | Toggle Turbo Space | Space turns red |
| Fn + W (QWERTY) / Fn + Z (AZERTY) | Toggle forward-key hold | That key turns green |
| Fn + R | Next Turbo rate: 25 → 50 → 100 presses/second | Hold Fn: R is green / amber / red |
| Fn + Shift + R | Previous Turbo rate | Same rate colors |
| Esc | Stop Turbo and forward hold; Escape still reaches the application | Helper indicators clear |
| Fn + Esc | Stop helpers and unlock Windows; no Escape is sent | Helper/Game indicators clear |

**Default Turbo rate: 50 presses/second.** One Turbo target runs at a time; choosing another switches the target. Forward hold can run alongside Turbo. A normal physical press of the repeated/held target cancels that helper and gives control back to your finger.

Turbo sends alternating key-down/key-up reports, rather than changing Windows repeat settings. It schedules at most one edge per housekeeping pass and drops missed deadlines instead of sending catch-up bursts. Rates are targets; effective actions depend on USB scheduling and the application. Current keyboard modifiers still apply.

W/Z refers to the same physical forward-key position and USB usage. The firmware does not know the host language layout. The E, F, R and Space positions are identical on the usual French AZERTY and QWERTY layouts.

Game Mode blocks standalone left/right Windows keys and simple GUI-modified QMK shortcuts. It clears already-held GUI modifiers when enabled, without clearing Ctrl/Alt/Shift. It is not a filter for arbitrary VIA macros or every advanced modifier action. Game Mode is not saved and exits on USB suspension/disconnection.

## Lighting and preferences

| Shortcut | Action |
| --- | --- |
| Fn + F1 | OpenRGB / Aurora host profile |
| Fn + F2 | Local white profile |
| Fn + F3 | Local red profile |
| Fn + F4 | Lighting Off |
| Fn + Backspace | Toggle lighting Off / previous profile |
| Fn + Page Up / Right | Increase master brightness |
| Fn + Page Down / Left | Decrease master brightness |
| Fn + Up / Down | Next / previous profile: host, white, red |
| Fn + Enter | Replay the red wave, then return to the selected profile |
| Fn + F5 | Enable / disable automatic startup and resume waves |
| Fn + F6 | Enable / disable white fallback for the four media keys |
| Fn + F7 | Choose white / dark after 5 seconds without RGB data |
| Fn + F8 | Restore RAM defaults, stop helpers, unlock Windows, reset Turbo to 50/s |
| Fn + F9 | Save lighting preferences; Calculator briefly turns green if lighting is on |
| Fn + Pause | Stop media playback |
| Fn + either Windows key | Toggle Game Mode |

Local white/red profiles keep their colors even while OpenRGB sends updates. Returning to the host profile uses fresh host colors; old timed-out colors are cleared. Brightness applies to host colors, local profiles, the wave and status indicators. At zero brightness everything in the RGB backlight is dark; Fn + F8 is the recovery shortcut.

Ordinary shortcuts and RGB streaming do not write EEPROM. **Fn + F9 is the explicit save action.** It stores only lighting profile, brightness and the three lighting options in QMK's user preference word. It does not save Game Mode, Turbo, forward hold, VIA layers or macros. Fn + F8 changes RAM defaults; press Fn + F9 afterward if you want those defaults saved too. Flashing may erase preferences.

## Bootloader

**Hold Fn + Shift + Esc for 2 seconds** to enter the bootloader on RedBlade+ 0.4.0+. Releasing Fn, Shift or Esc early cancels the request. This first stops game helpers and unlocks Windows. Fn + Esc alone is always the quick stop action.

Older firmware may enter the bootloader immediately with Fn + Esc. Confirm which firmware is currently installed before following the flash guide. No shortcut here resets VIA layers or erases macros.

## Upgrading an existing VIA layer

Stop OpenRGB, Aurora and VIA, then run from the project root:

```powershell
python tools/install-fn-layer.py
python tools/install-fn-layer.py --apply --backup C:\private-path\before-redblade-plus.json
```

The first command is a dry run. Applying requires RedBlade+ 0.4.0+, creates a complete private VIA backup before the first write, changes only the 30 published bindings on layer 1, verifies each write and attempts rollback after an error. Base keys, macros and other Fn bindings are retained. Do not upload personal VIA backups to an issue.
