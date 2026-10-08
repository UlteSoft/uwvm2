#!/usr/bin/env python3
"""Real -g LLVM-full PTY display/hit-policy tests, original Linux cgroup only."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("uwvm", "source-root", "out", "clang", "wasm-tools"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--cgroup", required=True)
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--dwarf-version", type=int, choices=(0, 4, 5), default=0)
    args = parser.parse_args()
    assert Path("/proc/self/cgroup").read_text().strip() == "0::" + args.cgroup
    root = args.source_root.resolve(strict=True)
    out = args.out.resolve(); out.mkdir(mode=0o700, exist_ok=False)
    driver_source = root / "test/0017.runtime/run_debug_keyboard_product.py"
    driver = out / "pty_observer.py"
    text = driver_source.read_text()
    assert text.count("            os.execv(command[0], command)") == 1
    driver.write_text(text.replace("            os.execv(command[0], command)",
                                   "            time.sleep(.15)\n            os.execv(command[0], command)"))
    spec = importlib.util.spec_from_file_location("display_pty_observer", driver)
    mod = importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
    source = root / "test/0017.runtime/fixtures/debug_console_displays_cpp.cc"
    wasm = out / "displays.wasm"
    compiler = [str(args.clang), "--target=wasm32", "-nostdlib", "-std=c++20", "-O1", "-g", "-fno-exceptions",
                "-fno-rtti", "-Wl,--no-entry", "-Wl,--export=_start", str(source), "-o", str(wasm)]
    if args.dwarf_version:
        compiler.insert(compiler.index("-g") + 1, "-gdwarf-" + str(args.dwarf_version))
    subprocess.run(compiler, check=True, timeout=30)
    subprocess.run([str(args.wasm_tools), "validate", "--features", "all", str(wasm)], check=True, timeout=10)
    line = next(i for i, text in enumerate(source.read_text().splitlines(), 1) if "DISPLAY_READY" in text)
    prefix = [str(args.uwvm), "-Rdbg"] + (["-Raot"] if args.ros else ["-Rcc", "jit", "-Rcm", "full"]) + \
        ["-Rct", "0", "-Rllvm-cache-path", "disable"]
    rows = []
    for policy in ("instruction", "unwind"):
        con = mod.Console(prefix + ["-Rllvm-call-stack", policy, "--run", str(wasm)], 120)
        pidfd = os.pidfd_open(con.pid)
        row = {"policy": policy, "passed": False, "checks": []}
        try:
            con.wait(lambda b: b"UWVM LLVM full debugger." in b and b"(uwvm-debug) " in b)
            def query(command):
                start = len(con.log); con.write(command.encode() + b"\n")
                return con.wait(lambda b: b"\n(uwvm-debug) " in b, start)
            def check(label, value):
                assert value, label
                row["checks"].append({"case": label, "passed": True})
            initial = query("status"); prepared = mod.stop_id(initial)
            first = query("display parameter + counter")
            check("register text at prepared stop without fabricating a source value",
                  b"display 1 registered" in first and b"source-value stop=" not in first)
            current = query("status")
            check("unavailable initial display preserves genuine prepared stop", mod.stop_id(current) == prepared)
            br = query(f"break-source 0 {source}:{line} ignore 3")
            check("atomic source breakpoint registration", b"error:" not in br and b"breakpoint " in br)
            bp = int(re.search(rb"breakpoint (\d+) source=", br).group(1))
            stop = query("continue"); sid = mod.stop_id(stop)
            check("new real stop automatically evaluates display", sid != prepared and b"stopped: breakpoint" in stop and
                  f"display 1 stop={sid} expression=parameter + counter".encode() in stop and b"source-value end" in stop and b"error:" not in stop)
            hits = query("info breakpoints")
            check("first stop occurs on hit4 after exactly three ignored hits", re.search(rb"hits=4 ignore=0(?:\r?\n|$)", hits) is not None)
            check("status/query at the same stop does not reevaluate displays", b"display 1 stop=" not in hits)
            rearmed = query(f"break-source 0 {source}:{line} ignore 2")
            check("rearm existing target retains its ID atomically", re.search(rb"breakpoint " + str(bp).encode() + rb" source=", rearmed) is not None)
            hits = query("info breakpoints")
            check("explicit rearm resets count and installs ignore2", b"hits=0 ignore=2" in hits)
            stop = query("continue"); sid2 = mod.stop_id(stop)
            check("rearmed target stops on its third hit", sid2 != sid and b"display 1 stop=" in stop and b"hits=3 ignore=0" in query("info breakpoints"))
            check("source value is copied at current stop", b"error:" not in query("print parameter + counter"))
            thread = int(re.search(rb"thread (\d+) module=0", stop).group(1))
            query(f"delete {bp}")
            unavailable = query("display missing_local")
            check("out-of-scope display is retained with explicit error", b"display 2 registered" in unavailable and b"error:" in unavailable)
            check("display rejects mutation/calls", all(b"error:" in query(command) and b"registered" not in query("info display")
                  for command in ("display counter = 1", "display run()")))
            query("disable display 1")
            disabled = query(f"step wasm {thread}")
            check("disabled expression is skipped at new real stop", b"display 1 stop=" not in disabled and b"display 2 stop=" in disabled)
            query("disable display 2"); query("enable display 1")
            enabled = query(f"step wasm {thread}")
            check("reenabled expression queries new stop while unavailable entry stays disabled",
                  b"display 1 stop=" in enabled and b"display 2 stop=" not in enabled and b"error:" not in enabled)
            query("undisplay 1")
            check("deleted display ID cannot be reenabled", b"error: invalid or unknown display identifier" in query("enable display 1"))
            again = query("display 1 + 2")
            check("new expression gets unique ID and numeric input remains expression", b"display 3 registered" in again and b" value=3" in again)
            query("undisplay")
            listed = query("info display")
            check("delete all clears owned expression text", b" enabled " not in listed and b" disabled " not in listed)
            # The new command participates in actual stopped-variable completion.
            start = len(con.log); con.write(b"display par\t\n")
            completed = con.wait(lambda b: b"\n(uwvm-debug) " in b, start)
            check("Tab completes actual current variable in display input", b"display 4 registered" in completed and
                  b"expression=parameter" in completed and b"error:" not in completed)
            for n in range(31):
                check("bounded display entry " + str(n), b"registered" in query("display 0"))
            check("thirty-third active display is refused", b"error: display limit" in query("display 0"))
            check("clear keeps stop and guest state", b"error:" not in query("undisplay") and b"stopped:" in query("status"))
            con.write(b"quit force\n"); con.exit(0)
            row["passed"] = True
        except BaseException as error:
            row["error"] = type(error).__name__ + ": " + str(error)
            raise
        finally:
            (out / (policy + ".console.log")).write_bytes(con.log)
            con.cleanup(); os.close(pidfd); rows.append(row)
            (out / "results.json").write_text(json.dumps({"passed": all(r["passed"] for r in rows), "rows": rows,
                "binary_sha256": sha(args.uwvm), "source_sha256": sha(source), "wasm_sha256": sha(wasm), "compiler": compiler,
                "harness_sha256": sha(__file__),
                "scope": "genuine current Linux x86_64 guest CLI stops; DAP policy translation has separate DATA tests"}, indent=2) + "\n")
    print(json.dumps({"passed": True, "checks": sum(len(row["checks"]) for row in rows)}))


if __name__ == "__main__":
    main()
