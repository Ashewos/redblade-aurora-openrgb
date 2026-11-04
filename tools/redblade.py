"""Read-only Red Blade diagnosis and local VIA backup. Python standard library."""
import argparse
import ctypes as C
import json
from pathlib import Path


class Device(C.Structure):
    pass


Device._fields_ = [
    ('path', C.c_char_p), ('vid', C.c_ushort), ('pid', C.c_ushort),
    ('serial', C.c_wchar_p), ('release', C.c_ushort),
    ('manufacturer', C.c_wchar_p), ('product', C.c_wchar_p),
    ('usage_page', C.c_ushort), ('usage', C.c_ushort),
    ('interface', C.c_int), ('next', C.POINTER(Device)),
]


class Keyboard:
    def __init__(self, library):
        self.hid = C.CDLL(str(library))
        for name, args, result in [
            ('hid_enumerate', [C.c_ushort, C.c_ushort], C.POINTER(Device)),
            ('hid_free_enumeration', [C.POINTER(Device)], None),
            ('hid_open_path', [C.c_char_p], C.c_void_p),
            ('hid_write', [C.c_void_p, C.c_void_p, C.c_size_t], C.c_int),
            ('hid_read_timeout', [C.c_void_p, C.c_void_p, C.c_size_t, C.c_int], C.c_int),
            ('hid_close', [C.c_void_p], None),
        ]:
            fn = getattr(self.hid, name)
            fn.argtypes, fn.restype = args, result
        head = self.hid.hid_enumerate(0x320F, 0x5136)
        ptr, paths = head, []
        try:
            while ptr:
                dev = ptr.contents
                if dev.usage_page == 0xFF60 and dev.usage == 0x61:
                    if dev.product != 'Redblade FS' or dev.manufacturer != 'DesignedbyGG':
                        raise RuntimeError('Unexpected device identity; refusing access')
                    paths.append(dev.path)
                ptr = dev.next
        finally:
            self.hid.hid_free_enumeration(head)
        if len(paths) != 1:
            raise RuntimeError(f'Expected one Redblade FS Raw HID interface, found {len(paths)}')
        self.handle = self.hid.hid_open_path(paths[0])
        if not self.handle:
            raise RuntimeError('Unable to open Raw HID; close VIA and other keyboard tools')

    def request(self, *payload):
        if not payload or len(payload) > 32:
            raise ValueError('Invalid request length')
        # This utility deliberately permits only read operations.
        if payload[0] not in (0x01, 0x02, 0x04, 0x08, 0x0C, 0x0D, 0x0E, 0x11, 0x12, 0xFE):
            raise ValueError('Write commands are not available in this utility')
        packet = C.create_string_buffer(bytes([0, *payload]) + bytes(32-len(payload)), 33)
        if self.hid.hid_write(self.handle, packet, 33) != 33:
            raise RuntimeError('HID request failed')
        response = C.create_string_buffer(32)
        if self.hid.hid_read_timeout(self.handle, response, 32, 1000) != 32:
            raise RuntimeError('No complete HID response; close other HID clients')
        return response.raw

    def via(self, *payload):
        data = self.request(*payload)
        if data[0] != payload[0] or (payload[0] == 8 and len(payload) > 1 and data[1] != payload[1]):
            raise RuntimeError(f'Unsupported or mismatched VIA read: {payload[0]:02x}')
        return data

    def close(self):
        self.hid.hid_close(self.handle)


def backup(kb, destination):
    protocol = int.from_bytes(kb.via(1)[1:3], 'big')
    layers = kb.via(0x11)[1]
    if not 1 <= layers <= 16:
        raise RuntimeError('Invalid layer count')
    size = layers * 6 * 21 * 2
    keymap = bytearray()
    for offset in range(0, size, 28):
        count = min(28, size-offset)
        data = kb.via(0x12, offset >> 8, offset & 255, count)
        keymap.extend(data[4:4+count])
    macro_size = int.from_bytes(kb.via(0x0D)[1:3], 'big')
    if macro_size > 16384:
        raise RuntimeError('Invalid macro buffer size')
    macros = bytearray()
    for offset in range(0, macro_size, 28):
        count = min(28, macro_size-offset)
        data = kb.via(0x0E, offset >> 8, offset & 255, count)
        macros.extend(data[4:4+count])
    document = dict(device='Redblade FS', vid='320f', pid='5136', via_protocol=protocol,
                    rows=6, columns=21, layers=layers, keymap_hex=keymap.hex(),
                    macro_size=macro_size, macros_hex=macros.hex())
    destination.parent.mkdir(parents=True, exist_ok=True)
    # Exclusive creation avoids overwriting an earlier keyboard backup.
    with destination.open('x', encoding='utf-8') as output:
        json.dump(document, output, indent=2)
    print(f'Local backup saved: {layers} layers, {len(keymap)} keymap bytes, {len(macros)} macro bytes')


def verify(kb):
    via = int.from_bytes(kb.via(1)[1:3], 'big')
    identity = kb.request(0xFE, 0)
    if identity[4:12] != b'RDBLFS01' or identity[12] != 1:
        print(f'Redblade FS / VIA {via}: custom RGB firmware is not active')
        return 2
    info = kb.via(8, 0x40)
    count = int.from_bytes(kb.via(8, 0x43)[2:4], 'little')
    if int.from_bytes(info[2:4], 'little') != 1 or count != 109:
        raise RuntimeError('Unexpected RGB protocol or LED count')
    matrix_positions = set()
    for led in range(count):
        data = kb.via(8, 0x44, led & 255, led >> 8)
        row, col = data[5:7]
        if row >= 6 or col >= 21 or (row, col) in matrix_positions:
            raise RuntimeError(f'Invalid LED matrix position at index {led}')
        matrix_positions.add((row, col))
    print(f'RGB firmware verified: VIA {via}, VialRGB 1, {count} unique mapped LEDs')
    return 0


def status(kb):
    """Private diagnostics supported by firmware 0.2.0+, without renewing RGB timeout."""
    data = kb.request(0xFE, 0x80)
    if data[:3] != bytes([0xFE, 0x80, 1]) or data[24:32] != b'RDBLFS01':
        raise RuntimeError('Diagnostics require Red Blade Aurora firmware 0.2.0 or newer')
    flags = data[6]
    age = int.from_bytes(data[8:12], 'little')
    result = dict(
        firmware='.'.join(map(str, data[3:6])),
        fresh_rgb_data=bool(flags & 1), boot_animation=bool(flags & 2),
        usb_suspended=bool(flags & 4), lighting_enabled=bool(flags & 8),
        mode={0: 'Off', 1: 'Direct', 2: 'Standalone'}.get(data[7], 'Unknown'),
        last_rgb_packet_age_ms=None if age == 0xFFFFFFFF else age,
        rgb_timeout_ms=int.from_bytes(data[12:16], 'little'),
        accepted_rgb_packets=int.from_bytes(data[16:20], 'little'),
        boot_animation_ms=int.from_bytes(data[20:24], 'little'),
    )
    if tuple(data[3:6]) >= (0, 4, 0):
        controls = kb.request(0xFE, 0x81)
        if controls[:3] != bytes([0xFE,0x81,1]) or controls[24:32] != b'RDBLFS01':
            raise RuntimeError('Unexpected RedBlade+ controls diagnostics')
        result.update(game_mode=bool(controls[6]),
            lighting_profile={0:'OpenRGB',1:'White',2:'Red',3:'Off'}.get(controls[7],'Unknown'),
            master_brightness=controls[8], master_brightness_percent=round(controls[8]*100/255,1),
            startup_wave_enabled=bool(controls[9]&1), media_fallback_enabled=bool(controls[9]&2),
            idle_white_fallback=bool(controls[9]&4), bootloader_hold_ms=int.from_bytes(controls[10:12],'little'),
            turbo_key=None if not controls[16] else {8:'E',9:'F',44:'Space'}.get(controls[16],f'HID 0x{controls[16]:02X}'),turbo_target_presses_per_second=controls[17],
            forward_hold=bool(controls[18]),turbo_selection_armed=bool(controls[19]&1),shortcut_guide_active=bool(controls[19]&2))
    print(json.dumps(result, indent=2))
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['verify', 'backup', 'status'])
    parser.add_argument('--hid-dll', type=Path, default=Path(r'C:\Program Files\OpenRGB\hidapi-hotplug.dll'))
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.action == 'backup' and args.output is None:
        parser.error('backup requires --output (keep this file private)')
    kb = Keyboard(args.hid_dll)
    try:
        if args.action == 'backup':
            backup(kb, args.output)
            return 0
        if args.action == 'status':
            return status(kb)
        return verify(kb)
    finally:
        kb.close()


if __name__ == '__main__':
    raise SystemExit(main())
