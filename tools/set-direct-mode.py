"""Select Direct RGB on the custom Red Blade firmware; optionally persist it.

Close OpenRGB, Aurora and VIA first: they share the Raw HID interface.
This changes only lighting settings, not the firmware or keymap.
"""
import argparse
import ctypes as C
from pathlib import Path

from redblade import Keyboard


def send(kb, payload):
    packet = C.create_string_buffer(bytes([0, *payload]) + bytes(32-len(payload)), 33)
    if kb.hid.hid_write(kb.handle, packet, 33) != 33:
        raise RuntimeError('Lighting command could not be written')
    response = C.create_string_buffer(32)
    if kb.hid.hid_read_timeout(kb.handle, response, 32, 1000) != 32:
        raise RuntimeError('No lighting acknowledgement; close competing Raw HID clients')
    if response.raw[:2] != bytes(payload[:2]):
        raise RuntimeError('Unexpected lighting acknowledgement')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--save', action='store_true', help='Persist Direct lighting in QMK EEPROM')
    parser.add_argument('--hid-dll', type=Path, default=Path(r'C:\Program Files\OpenRGB\hidapi-hotplug.dll'))
    args = parser.parse_args()
    kb = Keyboard(args.hid_dll)
    try:
        identity = kb.request(0xFE, 0)
        if identity[4:12] != b'RDBLFS01' or identity[12] != 1:
            print('Custom Red Blade RGB firmware is not installed; no setting was changed.')
            return 2
        # VialRGB: Direct (1), speed 128, HSV 0/255/255. Pixel colors arrive
        # independently from OpenRGB; keep the global RGB engine enabled.
        send(kb, [0x07, 0x41, 1, 0, 128, 0, 255, 255])
        current = kb.via(0x08, 0x41)
        if int.from_bytes(current[2:4], 'little') != 1:
            raise RuntimeError('Keyboard did not enter Direct RGB mode')
        if args.save:
            # Standard VIA RGB Matrix save. One configuration write only;
            # normal streaming still does not write EEPROM.
            send(kb, [0x09, 0x03, 0])
        print('Direct RGB enabled' + (' and saved for subsequent boots.' if args.save else '.'))
        return 0
    finally:
        kb.close()


if __name__ == '__main__':
    raise SystemExit(main())
