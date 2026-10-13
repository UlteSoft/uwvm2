#!/usr/bin/env python3
"""Check original Core 3 exception ownership and traces in the Linux cgroup.

These are semantic witnesses, not per-throw benchmarks. Force native EH for
JIT entries so the optional pending numeric path cannot hide a propagation
failure. Keep this supplementary runner separate from the frozen fused corpus.
https://webassembly.github.io/spec/core/exec/instructions.html#exec-throw
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess


ANSI = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
FEATURES = ("-WFE-reference-types", "-WFE-function-references", "-WFE-gc", "-WFE-exceptions")
PREFIX = "eh-native-trace-"
PROVIDER = "tag-alias-provider"
# The first trace frame must refer to the ORIGINAL defined function, not to a
# later throw_ref, a wrapper, or an import declaration in the consuming module.
CASES = {
    "cross-call-nonref": None,
    "cross-call-gc-payload": None,
    "tag-alias-local-ref": (41, "TraceConsumer", 0, None, (("TraceConsumer", 1),)),
    "tag-alias-callee-ref": (41, "EHOwner", 0, None, (("TraceConsumer", 1), ("TraceConsumer", 2))),
    "old-exn-payload": (43, "TraceConsumer", 0, 1, (("TraceConsumer", 2),)),
    "catch-all-ref-shadow": (47, "TraceConsumer", 0, None, (("TraceConsumer", 1),)),
}
BAD_FAILURES = (
    "invalid parameter:", "unknown parameter", "unrecognized option",
    "parsing error in webassembly file", "validation error in webassembly code",
    "illegal webassembly file format", "runtime crash", "wasm trap:",
    "jit materialization failed", "failed to materialize",
)
FRAME = re.compile(r'#(\d+) module_id=(\d+) module="([^"\\]*)" func_idx=(\d+)\b')


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def configurations(ros, full_only):
    if ros:
        yield "int-full", ("-Rint",), False
    else:
        for mode in (("full",) if full_only else ("full", "lazy", "lazy+verification")):
            yield "int-" + mode, ("-Rcc", "int", "-Rcm", mode, "-Rct", "0"), False
    modes = ("full",) if ros or full_only else ("full", "lazy", "lazy+verification")
    for mode in modes:
        for policy in ("instruction", "unwind"):
            engine = ("-Raot",) if ros else ("-Rcc", "jit", "-Rcm", mode)
            flags = (*engine, "-Rct", "0", "-Rllvm-cache-path", "disable",
                     "-Rllvm-call-stack", policy, "-Rllvm-exception-dispatch", "native-unwind",
                     "-Rclog", "err", "-log-vb")
            yield "jit-" + mode + "-" + policy, flags, mode == "full"


def native_full_materialization_proof(plain, modules):
    # The optional sealed single-source optimization owns an extra source pin.
    # Preloaded modules use the ordinary native ABI and have no such pin/log.
    # Require actual full-only translation and every expected engine's finished
    # materialization instead; a requested CLI mode alone is not a witness.
    if not modules:
        return True
    if "mode=full_compile, compiler=llvm_jit_only, uwvm-int-translation=disabled, llvm-jit-ir-translation=enabled" not in plain:
        return False
    if "pending-plan=r2-phase" in plain or "body-fallback=yes" in plain:
        return False
    for module in modules:
        name = re.escape(module)
        if not re.search(r'^\[llvm-jit-full\] optimize-start module="' + name + r'" ', plain, re.M):
            return False
        if not re.search(r'^\[llvm-jit-full\] finalize-object-end module="' + name + r'" time=', plain, re.M):
            return False
        if not re.search(r'LLVM JIT materialization for module "' + name + r'" done\.', plain):
            return False
    return "llvm-jit full compilation done." in plain


def product_outcome(code, raw, expected, native_modules=()):
    plain = ANSI.sub("", raw.decode(errors="replace"))
    lower = plain.lower()
    proof = native_full_materialization_proof(plain, native_modules)
    failure = code is None or any(marker in lower for marker in BAD_FAILURES)
    if expected is None:
        return code == 0 and not failure and proof, [], proof
    payload, module, function, forbidden, required_callers = expected
    frames = [(int(index), name, int(func)) for index, _, name, func in FRAME.findall(plain)]
    # Imported aliases may add a boundary frame. Require the original callers
    # in order without prescribing optional implementation boundary frames.
    caller_index = 0
    for _, name, func in frames[1:]:
        if caller_index < len(required_callers) and (name, func) == required_callers[caller_index]:
            caller_index += 1
    # A same-type unrelated trap/validator error or an empty reconstructed stack
    # is never accepted as the expected escaping guest exception.
    diagnostic = (
        code is not None and code != 0 and not failure and "\x1b[" in raw.decode(errors="replace") and
        "Uncaught WebAssembly exception" in plain and "payload_fields=1" in plain and
        re.search(r"payload\[0\] i32 bits=0x[0-9a-fA-F]+ signed=" + str(payload) + r"\b", plain) is not None and
        "Wasm call stack captured at throw" in plain and "truncated" not in lower and
        len(frames) >= 2 and frames[0] == (0, module, function) and
        caller_index == len(required_callers) and
        [frame[0] for frame in frames] == list(range(len(frames))) and
        (forbidden is None or not any(name == module and func == forbidden for _, name, func in frames))
    )
    return diagnostic and proof, frames, proof


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--uwvm", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--wasmtime", type=Path, required=True)
    parser.add_argument("--source-id", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--full-only", action="store_true", help="Prioritize the actual native full paths")
    parser.add_argument("--only-case", action="append", choices=tuple(CASES))
    parser.add_argument("--timeout", type=int, default=45)
    args = parser.parse_args()
    if not 1 <= args.timeout <= 90:
        parser.error("timeout must be between 1 and 90 seconds")
    root = args.source_root.resolve(strict=True)
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    # A failing subprocess can write at most 8 MiB to either raw stream. This
    # runner never retains an unbounded guest diagnostic in a PIPE or in RAM.
    resource.setrlimit(resource.RLIMIT_FSIZE, (8 * 1024 * 1024, 8 * 1024 * 1024))
    args.uwvm = args.uwvm.resolve(strict=True)
    args.wasm_tools = args.wasm_tools.resolve(strict=True)
    args.wasmtime = args.wasmtime.resolve(strict=True)
    args.out.mkdir(parents=True, exist_ok=False)
    selected = list(args.only_case or CASES)
    fixtures = root / "test/0017.runtime/fixtures"
    names = list(dict.fromkeys([PROVIDER, *selected]))
    immutable = [args.uwvm, args.wasm_tools, args.wasmtime, Path(__file__).resolve(),
                 *[fixtures / (PREFIX + name + ".wat") for name in names]]
    initial = {str(path): digest(path) for path in immutable}
    rows, artifacts, binaries = [], {}, {}

    def check(label, command, expected=None, oracle=False, native_modules=()):
        argv = [str(part) for part in command]
        stdout, stderr = args.out / (label + ".stdout.log"), args.out / (label + ".stderr.log")
        with stdout.open("xb") as out, stderr.open("xb") as err:
            try:
                process = subprocess.run(argv, stdout=out, stderr=err, timeout=args.timeout)
                code = process.returncode
            except subprocess.TimeoutExpired:
                code = None
        raw = stdout.read_bytes() + stderr.read_bytes()
        if oracle:
            plain = ANSI.sub("", raw.decode(errors="replace")).lower()
            passed = (code == 0 if expected is None else
                      code is not None and code != 0 and "wasm backtrace" in plain and
                      "thrown wasm exception" in plain)
            frames, proof = [], None
        else:
            passed, frames, proof = product_outcome(code, raw, expected, native_modules)
        plain = ANSI.sub("", raw.decode(errors="replace"))
        row = {"label": label, "argv": argv, "exit": code, "passed": passed,
               "expected": "success" if expected is None else "original-uncaught-exception",
               "original_trace": frames, "native_full_materialization_proof": proof,
               "native_full_expected_modules": list(native_modules),
               "optional_owned_single_source_native_log_present":
                   "owning-source=yes pending-plan=native" in plain and "body-fallback=no" in plain,
               "stdout_sha256": digest(stdout), "stderr_sha256": digest(stderr),
               "stdout_bytes": stdout.stat().st_size, "stderr_bytes": stderr.stat().st_size}
        rows.append(row)
        (args.out / "runs.json").write_text(json.dumps(rows, indent=2) + "\n")
        print(json.dumps({"label": label, "passed": passed}), flush=True)
        return passed

    for name in names:
        wat, wasm = fixtures / (PREFIX + name + ".wat"), args.out / (PREFIX + name + ".wasm")
        parsed = check(name + "-parse", [args.wasm_tools, "parse", wat, "-o", wasm])
        valid = parsed and check(name + "-validate", [args.wasm_tools, "validate", "--features", "all", wasm])
        if valid:
            binaries[name] = wasm
            artifacts[name] = {"wat_sha256": digest(wat), "wasm_sha256": digest(wasm)}
    for name in selected:
        if name not in binaries or PROVIDER not in binaries:
            continue
        alias = name.startswith("tag-alias-")
        preload = ("--wasm-preload-library", binaries[PROVIDER], "EHOwner") if alias else ()
        oracle_preload = ("--preload", "EHOwner=" + str(binaries[PROVIDER])) if alias else ()
        oracle_ok = check(name + "-wasmtime", [args.wasmtime, "run", "-C", "cache=n",
            "-W", "exceptions=y", "-W", "gc=y", *oracle_preload, binaries[name]], CASES[name], oracle=True)
        if not oracle_ok:
            continue
        check(name + "-validation", [args.uwvm, "-m", "validation", *FEATURES,
              "--wasm-set-main-module-name", "TraceConsumer", *preload, "--run", binaries[name]])
        for mode, flags, full in configurations(args.ros, args.full_only):
            native_modules = (("TraceConsumer", "EHOwner") if alias else ("TraceConsumer",)) if full else ()
            check(name + "-" + mode.replace("+", "-"), [args.uwvm, "-m", "run", *flags, *FEATURES,
                "--log-color", "enable", "--wasm-set-main-module-name", "TraceConsumer",
                *preload, "--run", binaries[name]], CASES[name], native_modules=native_modules)
    final = {str(path): digest(path) for path in immutable}
    complete = len(binaries) == len(names) and all(row["passed"] for row in rows)
    summary = {"passed": complete and final == initial, "repository": "ros" if args.ros else "ordinary",
               "source_id": args.source_id, "source_id_is_build_receipt_input": True,
               "runtime_cases": len(selected), "checks": len(rows), "artifacts": artifacts,
               "immutable_before": initial, "immutable_after": final,
               "failures": [row for row in rows if not row["passed"]],
               "cgroup": {key: Path("/sys/fs/cgroup", key).read_text().strip()
                          for key in ("memory.max", "memory.swap.max", "cpuset.cpus.effective")}}
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps({key: summary[key] for key in ("passed", "runtime_cases", "checks")}), flush=True)
    return 0 if summary["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
