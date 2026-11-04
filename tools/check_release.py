"""Check a RedBlade+ firmware artifact; --update refreshes build metadata."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
from check_fn_map import check as check_fn

ROOT=Path(__file__).resolve().parents[1]


def validate(root=ROOT, elf=None, size_tool='arm-none-eabi-size', compiler='arm-none-eabi-gcc', update=False):
    check_fn(root)
    config=(root/'firmware/aurora_via/config.h').read_text(encoding='utf-8')
    version='v'+'.'.join(re.search(r'#define REDBLADE_VERSION_'+part+r'\s+(\d+)',config).group(1) for part in ('MAJOR','MINOR','PATCH'))
    path=root/'dist/manifest.json';manifest=json.loads(path.read_text(encoding='utf-8'))
    binary=(root/'dist/redblade-fs-aurora-via.bin').read_bytes()
    assert len(binary)==65536 and binary[-4:]==bytes.fromhex('5555aaaa'), 'Wrong Sonix image size/flag'
    stack,reset=struct.unpack_from('<II',binary)
    assert 0x20000000<stack<=0x20002000 and reset&1 and reset<65532, 'Unexpected ARM vector table'
    values=dict(release=version,binary_bytes=len(binary),sha256=hashlib.sha256(binary).hexdigest())
    if elf:
        report=subprocess.check_output([str(size_tool),'-A',str(elf)],text=True)
        sections={name:(int(size),int(address)) for name,size,address in re.findall(r'^(\.[\w.]+)\s+(\d+)\s+(\d+)',report,re.M)}
        remaining,address=sections['.heap']
        used=address-0x20000000
        assert used+remaining==8192 and remaining>0
        values.update(ram_bytes_including_reserved_stacks=used,ram_remaining_bytes=remaining)
    if update:
        manifest.update(values)
        native=root/'dist/redblade-plus-benchmark-windows.exe'
        if native.is_file(): manifest['native_benchmark']['sha256']=hashlib.sha256(native.read_bytes()).hexdigest()
        manifest['toolchain']=subprocess.check_output([str(compiler),'--version'],text=True).splitlines()[0]
        path.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
        (root/'dist/SHA256SUMS').write_text(values['sha256']+'  redblade-fs-aurora-via.bin\n',encoding='utf-8')
    for name,value in values.items(): assert manifest[name]==value, f'Manifest mismatch: {name}'
    assert manifest['ram_bytes_including_reserved_stacks']+manifest['ram_remaining_bytes']==8192
    assert manifest['led_count']==109 and manifest['qmk_commit']=='62973ee48a976c6f7cc6d4f7b198bd48a2fdfe60'
    expected=(root/'dist/SHA256SUMS').read_text(encoding='utf-8').split()[0]
    assert expected==values['sha256']
    native=root/'dist/redblade-plus-benchmark-windows.exe'
    if native.is_file(): assert hashlib.sha256(native.read_bytes()).hexdigest()==manifest['native_benchmark']['sha256']
    return manifest


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf',type=Path)
    parser.add_argument('--size-tool',default='arm-none-eabi-size')
    parser.add_argument('--compiler',default='arm-none-eabi-gcc')
    parser.add_argument('--update',action='store_true')
    args=parser.parse_args()
    if args.update and not args.elf: parser.error('--update requires the compiled --elf')
    result=validate(elf=args.elf,size_tool=args.size_tool,compiler=args.compiler,update=args.update)
    print(json.dumps({key:result[key] for key in ('release','sha256','binary_bytes','ram_remaining_bytes')}))
