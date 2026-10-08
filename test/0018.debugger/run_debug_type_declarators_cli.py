#!/usr/bin/env python3
"""Real -g LLVM-full PTY type declarator tests, original Linux cgroup only."""
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
    source = root / "test/0017.runtime/fixtures/debug_type_declarators_cpp.cc"
    wasm = out / "displays.wasm"
    compiler = [str(args.clang), "--target=wasm32", "-nostdlib", "-std=c++20", "-O1", "-g", "-fno-exceptions",
                "-fno-rtti", "-Wl,--no-entry", "-Wl,--export=_start", str(source), "-o", str(wasm)]
    if args.dwarf_version:
        compiler.insert(compiler.index("-g") + 1, "-gdwarf-" + str(args.dwarf_version))
    subprocess.run(compiler, check=True, timeout=30)
    subprocess.run([str(args.wasm_tools), "validate", "--features", "all", str(wasm)], check=True, timeout=10)
    line = next(i for i, text in enumerate(source.read_text().splitlines(), 1) if "TYPES_READY" in text)
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
            registered = query(f"break-source 0 {source}:{line}")
            check("genuine source breakpoint registration", b"error:" not in registered)
            stop = query("continue"); sid = mod.stop_id(stop)
            check("genuine producer stopped scope", sid != prepared and b"stopped: breakpoint" in stop)
            for expression, spelling in (
                ("function", b"int (*)(int)"),
                ("array_pointer", b"int (*)[2][3]"),
                ("data_member", b"int Widget::*"),
                ("method_member", b"int (Widget::*)(int) const &"),
            ):
                reply = query("ptype " + expression)
                check("actual DWARF declaration: " + expression,
                      b"error:" not in reply and b" type=" + spelling + b" kind=" in reply)
                check("ptype retains actual stop and does not execute/call: " + expression,
                      mod.stop_id(query("status")) == sid)
            matrix = query("ptype matrix")
            check("actual matrix shape is preserved", b"int [2][3]" in matrix and b"error:" not in matrix)
            check("source function-call expression is refused",
                  b"error:" in query("print function(2)") and mod.stop_id(query("status")) == sid)
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
