"""Negative controls for CLI/broker shutdown qualification, using real children."""
import signal
import subprocess
import sys
import unittest

from process_shutdown import wait_for_normal_exit


class ShutdownTests(unittest.TestCase):
    def child(self, statement):
        return subprocess.Popen(
            [sys.executable, "-c", "import os,signal,time;time.sleep(.3);" + statement],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

    def test_normal_exit(self):
        result = wait_for_normal_exit(self.child("raise SystemExit(0)"), 5)
        self.assertEqual(result, {"passed": True, "exit": 0, "forced": False})

    def test_nonzero_exit(self):
        result = wait_for_normal_exit(self.child("raise SystemExit(73)"), 5)
        self.assertEqual(result, {"passed": False, "exit": 73, "forced": False})

    @unittest.skipUnless(sys.platform != "win32", "POSIX signal exit status")
    def test_crashed_exit(self):
        result = wait_for_normal_exit(self.child("os.kill(os.getpid(),signal.SIGTERM)"), 5)
        self.assertEqual(result, {"passed": False, "exit": -signal.SIGTERM, "forced": False})

    def test_forced_exit(self):
        result = wait_for_normal_exit(self.child("time.sleep(60)"), .6)
        self.assertFalse(result["passed"])
        self.assertTrue(result["forced"])
        self.assertNotEqual(result["exit"], 0)


if __name__ == "__main__":
    unittest.main()
