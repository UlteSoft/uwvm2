#!/usr/bin/env python3
"""Ensure a timed-out compiler's children cannot outlive the FP test job.

No compiler, QEMU or Wasm execution is required. The fake driver and its child
both inherit the captured output pipes, just as a driver and cc1 do. Killing
only the driver would leave the child alive and delay EOF for eight seconds.
"""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time

if os.name != "posix":
    print("SKIP: process-group cleanup requires POSIX")
    raise SystemExit(77)

directory = Path(__file__).resolve().parent
driver = "import subprocess,sys,time; subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(8)']); time.sleep(8)"
with tempfile.TemporaryDirectory(prefix="uwvm-fp-timeout-") as temporary:
    root = Path(temporary)
    config = root / "config.json"
    config.write_text(json.dumps([{"name": "timeout-child", "cxx": [sys.executable, "-c", driver],
                                   "run": [], "optimizations": ["O0"], "suites": ["tail"]}]))
    start = time.monotonic()
    result = subprocess.run([sys.executable, str(directory / "run_fp_cross_matrix.py"),
                             "--config", str(config), "--build-dir", str(root / "build"),
                             "--jobs", "1", "--build-timeout", "1"],
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=15)
    elapsed = time.monotonic() - start
    records = json.loads((root / "build/results.json").read_text())
    if result.returncode != 1 or len(records) != 1 or records[0].get("build") != "timeout" or "run" in records[0]:
        raise SystemExit("FAIL: timeout was not reported as a failed build: " + result.stdout + result.stderr)
    # Leave ample scheduler margin over the one-second deadline, but do not
    # accept waiting for the fake compiler child's natural exit as cleanup.
    if elapsed >= 6:
        raise SystemExit(f"FAIL: compiler child delayed pipe EOF ({elapsed:.2f} s)")
    print(f"PASS: complete compiler process group terminated ({elapsed:.2f} s)")
