#!/usr/bin/env python3
"""Real -g LLVM-full PTY conditional breakpoint tests, original Linux cgroup only."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import time


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
    source = root / "test/0017.runtime/fixtures/debug_conditional_breakpoint_cpp.cc"
    wasm = out / "displays.wasm"
    compiler = [str(args.clang), "--target=wasm32", "-nostdlib", "-std=c++20", "-O1", "-g", "-fno-exceptions",
                "-fno-rtti", "-Wl,--no-entry", "-Wl,--export=_start", str(source), "-o", str(wasm)]
    if args.dwarf_version:
        compiler.insert(compiler.index("-g") + 1, "-gdwarf-" + str(args.dwarf_version))
    subprocess.run(compiler, check=True, timeout=30)
    subprocess.run([str(args.wasm_tools), "validate", "--features", "all", str(wasm)], check=True, timeout=10)
    line = next(i for i, text in enumerate(source.read_text().splitlines(), 1) if "CONDITION_READY" in text)
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
            baseline = query("info breakpoints")
            for expression in ("counter = 4", "run()", "counter; continue", "counter +"):
                reply = query(f"break-source 0 {source}:{line} if {expression}")
                check("syntax rejects before registration: " + expression, b"error:" in reply and query("info breakpoints") == baseline)
            registered = query(f"break-source 0 {source}:{line} if counter == 4 && parameter == 7")
            check("condition registers atomically before guest execution", b"error:" not in registered and b"breakpoint " in registered)
            bp = int(re.search(rb"breakpoint (\d+) source=", registered).group(1))
            stop = query("continue"); sid = mod.stop_id(stop)
            check("three genuine false stops retire before first true stop", sid != prepared and b"stopped: breakpoint" in stop and b"unavailable" not in stop)
            check("actual fourth visit and variable value", b"hits=4 ignore=0" in query("info breakpoints") and b" value=4" in query("print counter + 0"))
            check("condition policy listed without retaining captured values", b"condition=counter == 4 && parameter == 7" in query("info breakpoints"))
            for draft, spelling in (
                (f"condition {bp} cou", f"condition {bp} counter"),
                (f"break-source 0 {source}:{line} if cou", f"break-source 0 {source}:{line} if counter"),
            ):
                start = len(con.log); con.write(draft.encode() + b"\t")
                completed = con.wait(lambda b: b"(uwvm-debug) " + spelling.encode() in b, start)
                check("actual current-frame condition Tab: " + draft.split()[0], b"error:" not in completed)
                con.write(b"\x15")  # Cancel the edited line; Tab itself must not submit.
                check("condition Tab preserves breakpoint policy and real stop",
                      mod.stop_id(query("status")) == sid and
                      b"condition=counter == 4 && parameter == 7" in query("info breakpoints"))
            check("in-place condition update", b"error:" not in query(f"condition {bp} counter == 8"))
            stop = query("continue")
            check("updated condition skips visits5..7", b"hits=8 ignore=0" in query("info breakpoints") and b" value=8" in query("print counter + 0"))
            check("same stop query does not reevaluate or resume", mod.stop_id(query("status")) == mod.stop_id(stop))
            check("clear condition preserves breakpoint", b"error:" not in query(f"condition {bp}"))
            stop = query("continue")
            check("cleared condition next visit stops unconditionally", b" value=9" in query("print counter + 0") and b"hits=9 ignore=0" in query("info breakpoints"))
            query(f"condition {bp} missing_local != 0")
            stop = query("continue"); sid = mod.stop_id(stop)
            check("unavailable condition retains genuine stop with explanation", b"breakpoint-condition " in stop and b"actual stop retained" in stop and mod.stop_id(query("status")) == sid)
            query(f"condition {bp} 1 / 0")
            stop = query("continue")
            check("arithmetic error never auto-resumes", b"actual stop retained" in stop and mod.stop_id(query("status")) == mod.stop_id(stop))
            rearm = query(f"break-source 0 {source}:{line} ignore 2 if counter >= 14")
            check("combined ignore/condition rearm retains identifier", re.search(rb"breakpoint "+str(bp).encode()+rb" source=", rearm) is not None)
            check("combined rearm installs both atomically", b"hits=0 ignore=2 condition=counter >= 14" in query("info breakpoints"))
            stop = query("continue")
            check("ignore plus language predicate uses current values", b" value=14" in query("print counter + 0") and b"hits=3 ignore=0" in query("info breakpoints"))
            query(f"condition {bp} cell.value == 18 && parameter == 7")
            stop = query("continue")
            check("aggregate memory predicate uses genuine coherent guest reader", b"actual stop retained" not in stop and b" value=18" in query("print counter + 0"))
            query(f"condition {bp} counter == 0b1_0100 && parameter == 0o7")
            stop = query("continue")
            check("binary/octal/separators accepted in actual condition", b" value=20" in query("print counter + 0"))
            query(f"condition {bp} false")
            background = query("continue&")
            check("background resumes without publishing false stops", b"running" in background)
            time.sleep(.35)  # No status or value queries drive the guest here.
            start = len(con.log); con.write(b"\x03")
            interrupted = con.wait(lambda b: b"stopped: pause" in b and b"(uwvm-debug) " in b, start)
            current = query("print counter + 0")
            observed = re.search(rb" value=(-?\d+)", current)
            check("host coordinator skips multiple false hits without frontend polling",
                  observed is not None and int(observed.group(1)) > 23 and b"actual stop retained" not in interrupted)
            start = len(con.log); con.write(b"continue\n")
            # No false predicate may be exposed as a successful breakpoint stop.
            con.wait(lambda b: b"running" in b, start)
            con.write(b"\x03")
            stopped = con.wait(lambda b: b"stopped: pause" in b and b"(uwvm-debug) " in b, start)
            check("Ctrl+C defeats false predicate auto-resume", b"stopped: pause" in stopped and b"actual stop retained" not in stopped)
            sid = mod.stop_id(stopped)
            query("status")
            check("interrupted actual pause remains stable", mod.stop_id(query("status")) == sid)
            query(f"condition {bp} true")
            check("unknown breakpoint condition update rejected", b"error:" in query("condition 18446744073709551615 true"))
            check("malformed condition update leaves old policy", b"error:" in query(f"condition {bp} counter = 1") and b"condition=true" in query("info breakpoints"))
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
