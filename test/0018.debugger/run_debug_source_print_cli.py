#!/usr/bin/env python3
"""Production source-print preference/override CLI checks in the original 64GiB cgroup."""
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
    args = parser.parse_args()
    root = args.source_root.resolve(strict=True)
    out = args.out.resolve()
    out.mkdir(mode=0o700, exist_ok=False)
    driver_source = root / "test/0017.runtime/run_debug_keyboard_product.py"
    driver = out / "pty_observer.py"
    text = driver_source.read_text()
    assert text.count("            os.execv(command[0], command)") == 1
    driver.write_text(text.replace("            os.execv(command[0], command)",
                                   "            time.sleep(.15)\n            os.execv(command[0], command)"))
    spec = importlib.util.spec_from_file_location("source_print_pty_observer", driver)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    mod.cgroup_check(args.cgroup)
    source = root / "test/0017.runtime/fixtures/debug_console_displays_cpp.cc"
    wasm = out / "source_print.wasm"
    compiler = [str(args.clang), "--target=wasm32", "-nostdlib", "-std=c++20", "-O1", "-g", "-fno-exceptions",
                "-fno-rtti", "-Wl,--no-entry", "-Wl,--export=_start", str(source), "-o", str(wasm)]
    subprocess.run(compiler, check=True, timeout=30)
    subprocess.run([str(args.wasm_tools), "validate", "--features", "all", str(wasm)], check=True, timeout=10)
    line = next(i for i, text in enumerate(source.read_text().splitlines(), 1) if "DISPLAY_READY" in text)
    command = [str(args.uwvm), "-Rdbg"] + (["-Raot"] if args.ros else ["-Rcc", "jit", "-Rcm", "full"]) + \
        ["-Rct", "0", "-Rllvm-cache-path", "disable", "-Rllvm-call-stack", "instruction", "--run", str(wasm)]
    con = mod.Console(command, 120)
    pidfd = os.pidfd_open(con.pid)
    row = {"passed": False, "checks": []}
    try:
        con.wait(lambda data: b"UWVM" in data and b"debugger." in data and b"(uwvm-debug) " in data)

        def query(command):
            start = len(con.log)
            con.write(command.encode() + b"\n")
            return con.wait(lambda data: b"\n(uwvm-debug) " in data, start)

        def check(label, good):
            assert good, label
            row["checks"].append({"case": label, "passed": True})

        initial = mod.stop_id(query("status"))
        check("default source printing is enabled", b"is on." in query("show print pretty"))
        check("default element budget is 200", b"limit is 200." in query("show print elements"))
        registry = query("info pretty-printer")
        check("registry works before guest execution", b"Built-in source pretty-printers: enabled" in registry and b"Coroutine" in registry)
        check("set raw default works at initial stop", b"is off." in query("set print pretty off"))
        check("set bounded element budget", b"limit is 3." in query("set print elements 3"))
        for text in ("set print pretty yes", "set print elements -1", "set print elements 4097", "show print elements 2"):
            check("reject " + text, b"error:" in query(text))
        check("invalid preference leaves old values", b"is off." in query("show print pretty") and b"limit is 3." in query("show print elements"))
        check("register quoted custom field profile without reading guest memory", b"custom registered." in
              query('pretty-printer sequence custom "My Buffer<*>" storage.data length capacity'))
        registered = query("pretty-printer list")
        check("custom registry lists exact owned type/path data", b'custom enabled sequence type="My Buffer<*>"' in registered and b"data=storage.data length=length capacity=capacity" in registered)
        start = len(con.log)
        con.write(b"pretty-printer disable cus\t\n")
        disabled = con.wait(lambda data: b"\n(uwvm-debug) " in data, start)
        check("profile completion uses copied host names", b"custom disabled." in disabled and b"error:" not in disabled)
        check("re-enable named custom profile", b"custom enabled." in query("pretty-printer enable custom"))
        check("disable builtin independently of custom profile", b"builtin disabled." in query("pretty-printer disable builtin") and
              b"custom enabled sequence" in query("pretty-printer list"))
        check("re-enable builtin group", b"builtin enabled." in query("pretty-printer enable builtin"))
        check("invalid custom selector refuses without publishing a profile", b"error:" in
              query("pretty-printer pointer bad Type payload()") and b"bad enabled" not in query("pretty-printer list"))
        check("host preferences never advance the guest", mod.stop_id(query("status")) == initial)
        check("print help explains one-query raw override", b"print /r" in query("help set print pretty full"))
        point = query(f"breaksource 0 {source}:{line}")
        check("real source breakpoint installed", b"error:" not in point and b"breakpoint " in point)
        stopped = query("continue")
        sid = mod.stop_id(stopped)
        check("actual current source stop", sid != initial and b"stopped: breakpoint" in stopped)
        for text in ("print /r parameter + counter", "print /p parameter + counter", "print --raw parameter", "print --pretty counter"):
            value = query(text)
            check("source expression with " + text, b"error:" not in value and re.search(rb"\b(?:parameter|counter)(?: \+ counter)? = -?[0-9]+\b", value) is not None)
        check("one-query pretty leaves raw default", b"is off." in query("show print pretty"))
        start = len(con.log)
        con.write(b"print /r par\t\n")
        completed = con.wait(lambda data: b"\n(uwvm-debug) " in data, start)
        check("raw source completion reaches actual current local", re.search(rb"\bparameter = 7\b", completed) is not None and b"error:" not in completed)
        check("print controls preserve actual stopped identity", mod.stop_id(query("status")) == sid)
        restarted = query("run")
        check("run launches and stops with retained breakpoint", b"stopped: breakpoint" in restarted and b"error:" not in restarted)
        check("run preserves print preference and element budget", b"is off." in query("show print pretty") and b"limit is 3." in query("show print elements"))
        check("run preserves immutable custom registry configuration", b"custom enabled sequence" in query("pretty-printer list"))
        check("zero is explicit summary-only budget", b"summary only" in query("set print elements 0"))
        check("one-query pretty at zero budget still evaluates scalar", re.search(rb"\bcounter = -?[0-9]+\b", query("print /p counter")) is not None)
        check("re-enable default", b"is on." in query("set print pretty on"))
        killed = query("kill")
        check("owned guest termination succeeds", b"Program terminated." in killed)
        check("preferences remain available after kill", b"is on." in query("show print pretty") and b"limit is 0" in query("show print elements"))
        check("host registry survives guest termination", b"custom enabled sequence" in query("pretty-printer list") and b"custom deleted." in query("pretty-printer delete custom"))
        con.write(b"quit\n")
        con.exit(0)
        row["passed"] = True
    except BaseException as error:
        row["error"] = type(error).__name__ + ": " + str(error)
        raise
    finally:
        (out / "console.log").write_bytes(con.log)
        con.cleanup()
        os.close(pidfd)
        (out / "results.json").write_text(json.dumps({
            "passed": row["passed"], "row": row, "command": command, "compiler": compiler,
            "binary_sha256": sha(args.uwvm), "source_sha256": sha(source), "wasm_sha256": sha(wasm),
            "harness_sha256": sha(__file__),
            "scope": "Production CLI host print preferences, actual source expression overrides/completion and restart persistence; container layout tests are separate."
        }, indent=2) + "\n")
    print(json.dumps({"passed": True, "checks": len(row["checks"])}))


if __name__ == "__main__":
    main()
