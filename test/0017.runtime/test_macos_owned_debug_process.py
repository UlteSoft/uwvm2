#!/usr/bin/env python3
"""Pure ownership/oracle negatives; no native Mac API or payload is executed."""
import ctypes
from pathlib import Path
import tempfile
import unittest
import macos_owned_debug_process as owner
import run_macos_debug_current_cli as current
import stage_macos_debug_current_product as stage


class Ownership(unittest.TestCase):
    def test_exact_kernel_birth(self):
        value = owner.Birth(17, 200, 3, 501, 17)
        self.assertTrue(owner.identity_matches(value, value))

    def test_pid_reuse_same_pgid_is_rejected(self):
        self.assertFalse(owner.identity_matches(owner.Birth(17, 200, 3, 501, 17), owner.Birth(17, 201, 3, 501, 17)))

    def test_same_second_different_microsecond_is_rejected(self):
        self.assertFalse(owner.identity_matches(owner.Birth(17, 200, 3, 501, 17), owner.Birth(17, 200, 4, 501, 17)))

    def test_group_escape_is_rejected(self):
        self.assertFalse(owner.identity_matches(owner.Birth(17, 200, 3, 501, 17), owner.Birth(17, 200, 3, 501, 18)))

    def test_uid_change_is_rejected(self):
        self.assertFalse(owner.identity_matches(owner.Birth(17, 200, 3, 501, 17), owner.Birth(17, 200, 3, 0, 17)))

    def test_invalid_birth_never_matches(self):
        for value in (owner.Birth(0, 200, 3, 501, 17), owner.Birth(17, 0, 3, 501, 17),
                      owner.Birth(17, 200, 1000000, 501, 17), owner.Birth(17, 200, 3, 501, 0)):
            self.assertFalse(owner.identity_matches(value, value))

    def test_reparent_is_not_a_new_birth(self):
        first, orphan = owner.BsdInfo(), owner.BsdInfo()
        for value in (first, orphan):
            value.pid, value.uid, value.pgid, value.start_sec, value.start_usec = 18, 501, 17, 200, 3
        first.ppid, orphan.ppid = 17, 1
        self.assertEqual(owner.MacNative.birth(first), owner.MacNative.birth(orphan))

    def test_total_rss_includes_supervisor(self):
        self.assertEqual(owner.check_rss(1 << 30, [1 << 30, (1 << 30) - 1]), owner.RSS_STOP - 1)
        with self.assertRaises(RuntimeError):
            owner.check_rss(1 << 30, [1 << 30, 1 << 30])

    def test_rss_invalid_or_unbounded_inventory(self):
        for supervisor, members in ((-1, []), (0, [-1]), (0, [0] * 9), (0, [1 << 100])):
            with self.assertRaises(RuntimeError):
                owner.check_rss(supervisor, members)

    def test_public_sdk_layout_without_native_call(self):
        self.assertEqual((ctypes.sizeof(owner.BsdInfo), owner.BsdInfo.start_sec.offset), (136, 120))
        self.assertEqual((ctypes.sizeof(owner.TaskInfo), ctypes.sizeof(owner.SwapUsage)), (96, 32))
        self.assertEqual((ctypes.sizeof(owner.VmInfo64), owner.VmInfo64.swapins.offset), (160, 112))


class Oracles(unittest.TestCase):
    def test_thin_arm64_executable_only(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'candidate'
            header = bytearray(32)
            header[:4] = b'\xcf\xfa\xed\xfe'
            header[4:8] = (0x0100000c).to_bytes(4, 'little')
            header[12:16] = (2).to_bytes(4, 'little')
            path.write_bytes(header)
            stage.macho_arm64(path)  # inspect synthetic bytes, never execute
            for offset, value in ((4, 0x01000007), (12, 1)):
                bad = bytearray(header); bad[offset:offset + 4] = value.to_bytes(4, 'little'); path.write_bytes(bad)
                with self.assertRaises(RuntimeError):
                    stage.macho_arm64(path)

    def test_numeric_stop_actual_label_required(self):
        class Fake:
            def __init__(self, reply):
                self.reply = reply
            def send(self, command):
                return self.reply
        line = b'thread 3 module=0 function=1 byte-offset=9 generation=4\n'
        self.assertEqual(current.actual_stop(Fake(b'stop-id 7\n' + line), 3)['stop_id'], 7)
        for reply in (line, b'stop-id 0\n' + line, b'stop-id 7\n' + line + line, b'stop-id 7\n' + line.replace(b'thread 3', b'thread 4')):
            with self.assertRaises(AssertionError):
                current.actual_stop(Fake(reply), 3)


if __name__ == '__main__':
    unittest.main()
