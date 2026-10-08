#!/usr/bin/env python3
"""Reproduce the historical Win64 link with only static libgcc and a new output."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import signal
import subprocess
import time


LIMIT = 56 << 30


def digest(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def argv_digest(argv: list[str]) -> str:
    payload = json.dumps(argv, separators=(",", ":"), ensure_ascii=False).encode()
    return hashlib.sha256(payload).hexdigest()


def changed_link(argv: list[str], driver: Path, output: Path) -> list[str]:
    if argv.count("-o") != 1 or "-static-libgcc" in argv:
        raise ValueError("original link must have one output and dynamic libgcc default")
    result = [str(driver), "-static-libgcc", *argv[1:]]
    result[result.index("-o") + 1] = str(output)
    return result


def events(cgroup: Path) -> dict[str, int]:
    return {parts[0]: int(parts[1]) for line in (cgroup / "memory.events").read_text().splitlines()
            if len(parts := line.split()) == 2}


def run_link(name: str, argv: list[str], cwd: Path, output: Path,
             cgroup: Path, evidence: Path) -> dict:
    if output.exists():
        raise ValueError(f"refusing to overwrite {output}")
    before = events(cgroup)
    peak = int((cgroup / "memory.current").read_text())
    started = time.monotonic()
    with (evidence / f"{name}.stdout").open("xb") as stdout, \
         (evidence / f"{name}.stderr").open("xb") as stderr:
        process = subprocess.Popen(argv, cwd=cwd, stdout=stdout, stderr=stderr,
                                   start_new_session=True)
        exceeded = False
        while process.poll() is None:
            current = int((cgroup / "memory.current").read_text())
            peak = max(peak, current)
            if current >= LIMIT:
                exceeded = True
                os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                break
            time.sleep(0.2)
        code = process.wait()
    after = events(cgroup)
    record = {"exit_code": code, "elapsed_seconds": time.monotonic() - started,
              "cgroup_peak_bytes": peak, "watchdog_exceeded": exceeded,
              "oom_before": before.get("oom", 0), "oom_after": after.get("oom", 0),
              "oom_kill_before": before.get("oom_kill", 0),
              "oom_kill_after": after.get("oom_kill", 0),
              "output": str(output),
              "output_sha256": digest(output) if output.is_file() else None,
              "stdout_sha256": digest(evidence / f"{name}.stdout"),
              "stderr_sha256": digest(evidence / f"{name}.stderr")}
    (evidence / f"{name}.json").write_text(json.dumps(record, indent=2, sort_keys=True) + "\n")
    if code != 0 or exceeded or after.get("oom", 0) != before.get("oom", 0) or \
       after.get("oom_kill", 0) != before.get("oom_kill", 0):
        raise RuntimeError(f"{name} link failed or exceeded cgroup budget: {record}")
    return record


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--main-manifest", type=Path, required=True)
    parser.add_argument("--broker-build-log", type=Path, required=True)
    parser.add_argument("--working-dir", type=Path, required=True)
    parser.add_argument("--driver", type=Path, required=True)
    parser.add_argument("--cgroup", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--execute", action="store_true")
    args = parser.parse_args()
    manifest_path = args.main_manifest.resolve(strict=True)
    manifest = json.loads(manifest_path.read_text())
    source_id = manifest.get("source_id", "")
    if re.fullmatch(r"sha256:[0-9a-f]{64}", source_id) is None:
        raise ValueError("original PE link lacks exact source ID")
    if manifest.get("returncode") != 0 or \
       digest(Path(manifest["output"])) != manifest.get("output_sha256"):
        raise ValueError("historical diagnostic PE link artifact changed")
    if Path(manifest["output"]).name == "uwvm.exe":
        raise ValueError("original wrapper could have changed the link inputs")
    main_original = manifest["argv"]
    lines = args.broker_build_log.read_text().splitlines()
    broker_lines = [line for line in lines if
                    " -o " in line and "uwvm-debug-server.exe" in line and
                    "secure_server_windows.cpp.obj" in line]
    if len(broker_lines) != 1:
        raise ValueError("expected one exact historical broker link command")
    broker_original = shlex.split(broker_lines[0])
    # Preserve the clang++ basename: resolving its symlink to clang changes
    # driver mode and would no longer reproduce the original C++ link.
    driver = args.driver.absolute()
    if not driver.is_file() or not os.access(driver, os.X_OK):
        raise ValueError("missing executable clang++ driver")
    working_dir = args.working_dir.resolve(strict=True)
    cgroup = args.cgroup.resolve(strict=True)
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if shutil.disk_usage(output).free < (2 << 30):
        raise ValueError("insufficient disk headroom for diagnostic PE")
    links = {}
    for name, original in (("broker", broker_original), ("uwvm", main_original)):
        if original[0] != main_original[0]:
            raise ValueError("historical PE and broker used different wrappers")
        executable = output / ("uwvm-debug-server.exe" if name == "broker" else "uwvm.exe")
        static = changed_link(original, driver, executable)
        normalized = [str(driver), *original[1:]]
        verify = static.copy()
        verify.pop(1)
        verify[verify.index("-o") + 1] = normalized[normalized.index("-o") + 1]
        if verify != normalized:
            raise ValueError("static link changes more than -static-libgcc and output path")
        links[name] = {"original_argv_sha256": argv_digest(original),
                       "normalized_argv_sha256": argv_digest(normalized),
                       "static_argv_sha256": argv_digest(static),
                       "original_output": original[original.index("-o") + 1],
                       "static_output": str(executable),
                       "static_argv": static}
    plan = {"schema": 1, "source_id": source_id,
            "caveat": "Historical v7 diagnostic overlay; not a final source qualification",
            "driver_normalization": "Original wrapper passes through both non-uwvm.exe output names to this exact clang++ driver",
            "main_manifest_sha256": digest(manifest_path),
            "broker_build_log_sha256": digest(args.broker_build_log),
            "wrapper_sha256": digest(Path(main_original[0])),
            "driver_sha256": digest(driver), "working_dir": str(working_dir),
            "cgroup": str(cgroup), "limit_bytes": LIMIT, "links": links}
    plan_path = output / "plan.json"
    if plan_path.exists():
        if json.loads(plan_path.read_text()) != plan:
            raise ValueError("existing link plan differs")
    else:
        plan_path.write_text(json.dumps(plan, indent=2, sort_keys=True) + "\n")
    if not args.execute:
        print(json.dumps({"source_id": source_id, "plan_sha256": digest(plan_path),
                          "execute": False}, sort_keys=True))
        return
    if str(os.getpid()) not in (cgroup / "cgroup.procs").read_text().splitlines():
        raise ValueError("link controller is outside the 64 GiB test cgroup")
    results = {}
    for name in ("broker", "uwvm"):
        link = links[name]
        results[name] = run_link(name, link["static_argv"], working_dir,
                                 Path(link["static_output"]), cgroup, output)
    (output / "results.json").write_text(json.dumps(results, indent=2, sort_keys=True) + "\n")
    print(json.dumps({"source_id": source_id, "plan_sha256": digest(plan_path),
                      "results_sha256": digest(output / "results.json")}, sort_keys=True))


if __name__ == "__main__":
    main()
