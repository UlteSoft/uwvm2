"""Require real normal exit when verifying a debugger session."""
import subprocess


def wait_for_normal_exit(process, timeout):
    forced = False
    try:
        process.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        forced = True
        process.kill()
        process.wait(timeout=10)
    return {
        "passed": process.returncode == 0 and not forced,
        "exit": process.returncode,
        "forced": forced,
    }
