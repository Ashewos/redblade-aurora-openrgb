"""RedBlade+ performance checks: native regression and read-only HID round trips.

USB timing includes Windows, HIDAPI and firmware request/reply handling. It is
not a measurement of a physical key switch, scan/debounce, or game latency.
Stop OpenRGB, Aurora and VIA before --hid. No keymap or lighting writes occur.
"""
import argparse
import csv
from datetime import datetime, timezone
import io
import json
import math
from pathlib import Path
import platform
import statistics
import subprocess
import time


def percentile(values, percent):
    ordered=sorted(values)
    position=(len(ordered)-1)*percent/100
    low=math.floor(position); high=math.ceil(position)
    return ordered[low]+(ordered[high]-ordered[low])*(position-low)


def summarize(samples):
    if not samples: raise ValueError('No timing samples')
    return {k:round(v,4) for k,v in dict(count=len(samples),minimum_ms=min(samples),
        median_ms=statistics.median(samples),p95_ms=percentile(samples,95),
        p99_ms=percentile(samples,99),maximum_ms=max(samples),mean_ms=statistics.mean(samples)).items()}


def ensure_idle():
    if platform.system()!='Windows': raise RuntimeError('HID measurements currently support Windows only')
    result=subprocess.run(['tasklist','/fo','csv','/nh'],capture_output=True,text=True,check=True)
    names={row[0].lower() for row in csv.reader(io.StringIO(result.stdout)) if row}
    competing=names & {'openrgb.exe','aurorargb.exe','auroradevicemanager.exe','via.exe'}
    if competing: raise RuntimeError('Stop competing RGB/HID applications first: '+', '.join(sorted(competing)))


def hid_round_trips(kb, samples):
    identity=kb.request(0xFE,0)
    if identity[4:12]!=b'RDBLFS01' or identity[12]!=1: raise RuntimeError('Unexpected firmware signature')
    controls=kb.request(0xFE,0x81)
    if controls[:3]==bytes([0xFE,0x81,1]) and controls[24:32]==b'RDBLFS01' and (controls[16] or controls[18] or (controls[19]&5)):
        raise RuntimeError('Press Esc to stop Turbo/forward hold before running the HID benchmark')
    firmware='.'.join(map(str,controls[3:6])) if controls[:3]==bytes([0xFE,0x81,1]) and controls[24:32]==b'RDBLFS01' else None
    if firmware is None:
        legacy=kb.request(0xFE,0x80)
        if legacy[:3]==bytes([0xFE,0x80,1]) and legacy[24:32]==b'RDBLFS01': firmware='.'.join(map(str,legacy[3:6]))
    timing=[]
    for _ in range(32): kb.via(8,0x40)
    cpu_start=time.process_time_ns(); wall_start=time.perf_counter_ns()
    for _ in range(samples):
        start=time.perf_counter_ns(); data=kb.via(8,0x40)
        elapsed=(time.perf_counter_ns()-start)/1e6
        if data[:2]!=bytes([8,0x40]) or int.from_bytes(data[2:4],'little')!=1:
            raise RuntimeError('Mismatched RGB response; close competing HID clients')
        timing.append(elapsed)
    wall=(time.perf_counter_ns()-wall_start)/1e9
    cpu=(time.process_time_ns()-cpu_start)/1e9
    return dict(kind='host_raw_hid_read_round_trip',device_firmware=firmware,**summarize(timing),
        elapsed_seconds=round(wall,4),host_process_cpu_seconds=round(cpu,4),
        physical_key_latency_measured=False,mcu_execution_time_measured=False)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-exe',type=Path,help='Run the native benchmark executable from this release')
    parser.add_argument('--hid',action='store_true',help='Measure read-only USB request/reply latency')
    parser.add_argument('--samples',type=int,default=1000)
    parser.add_argument('--hid-dll',type=Path,default=Path(r'C:\Program Files\OpenRGB\hidapi-hotplug.dll'))
    parser.add_argument('--output',type=Path)
    args=parser.parse_args()
    if not args.native_exe and not args.hid: parser.error('Choose --native-exe <file> and/or --hid')
    if not 10<=args.samples<=100000: parser.error('--samples must be between 10 and 100000')
    report=dict(project='RedBlade+',created_utc=datetime.now(timezone.utc).isoformat(),
        platform=platform.system(),architecture=platform.machine(),measurements=[])
    if args.native_exe:
        result=subprocess.run([str(args.native_exe.resolve())],capture_output=True,text=True,check=True,timeout=60)
        native=json.loads(result.stdout)
        if native.get('kind')!='host_native_regression': raise RuntimeError('Unexpected native benchmark output')
        report['measurements'].append(native)
    if args.hid:
        ensure_idle()
        from redblade import Keyboard
        kb=Keyboard(args.hid_dll)
        try: report['measurements'].append(hid_round_trips(kb,args.samples))
        finally: kb.close()
    output=json.dumps(report,indent=2)+'\n'
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        with args.output.open('x',encoding='utf-8') as file: file.write(output)
    print(output,end='')
    return 0


if __name__=='__main__': raise SystemExit(main())
