import importlib.util
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import benchmark
import ctypes
import redblade
from unittest.mock import MagicMock
from check_fn_map import check
spec=importlib.util.spec_from_file_location('fn_installer',ROOT/'tools/install-fn-layer.py')
installer=importlib.util.module_from_spec(spec);spec.loader.exec_module(installer)


class ToolTests(unittest.TestCase):
    def test_hid_enumeration_is_freed_on_wrong_identity(self):
        device=redblade.Device()
        device.usage_page=0xFF60;device.usage=0x61
        device.manufacturer='DesignedbyGG';device.product='Wrong model'
        pointer=ctypes.pointer(device)
        library=MagicMock();library.hid_enumerate.return_value=pointer
        with patch.object(redblade.C,'CDLL',return_value=library):
            with self.assertRaises(RuntimeError): redblade.Keyboard(Path('unused.dll'))
        library.hid_free_enumeration.assert_called_once_with(pointer)

    def test_hid_read_rejects_wrong_rgb_subcommand(self):
        device=object.__new__(redblade.Keyboard)
        device.request=lambda *args:bytes([8,0x41])+bytes(30)
        with self.assertRaises(RuntimeError):device.via(8,0x40)

    def test_public_fn_map_matches_actual_firmware(self):
        self.assertEqual(len(check()['bindings']),33)

    def test_timing_statistics_and_units(self):
        result=benchmark.summarize([1,2,3,4,5])
        self.assertEqual(result['median_ms'],3)
        self.assertEqual(result['p95_ms'],4.8)
        self.assertEqual(result['p99_ms'],4.96)
        self.assertEqual(result['count'],5)
        with self.assertRaises(ValueError): benchmark.summarize([])

    def test_hid_benchmark_only_uses_read_commands(self):
        class Device:
            count=0
            def request(self,*payload):
                if payload==(0xFE,0): return bytes([4,0,0,0])+b'RDBLFS01'+bytes([1])+bytes(19)
                assert payload==(0xFE,0x81)
                return bytes([0xFE,0x81,1,0,4,0])+bytes(18)+b'RDBLFS01'
            def via(self,*payload):
                assert payload==(8,0x40)
                self.count+=1
                return bytes([8,0x40,1])+bytes(29)
        device=Device();report=benchmark.hid_round_trips(device,10)
        self.assertEqual(device.count,42)
        self.assertEqual(report['count'],10)
        self.assertFalse(report['physical_key_latency_measured'])

    def test_fn_install_rolls_back_a_write_with_lost_ack(self):
        bindings=[dict(row=0,col=2,code=0x7E44),dict(row=0,col=3,code=0x7E45)]
        keys={(0,2):100,(0,3):101}
        backup_done=[]
        fail_once=[True]
        def write(kb,row,col,code):
            self.assertTrue(backup_done)
            keys[(row,col)]=code
            if col==3 and fail_once[0]:
                fail_once[0]=False
                raise RuntimeError('ACK lost after device accepted the write')
        path=Path('private-test-backup.json') # Mocked backup: no filesystem access.
        with patch.object(installer,'backup',side_effect=lambda *a:backup_done.append(True)), \
             patch.object(installer,'read_key',side_effect=lambda kb,row,col:keys[(row,col)]), \
             patch.object(installer,'write_key',side_effect=write):
            with self.assertRaises(RuntimeError): installer.apply(object(),bindings,path)
        self.assertEqual(keys,{(0,2):100,(0,3):101})


if __name__=='__main__': unittest.main()
