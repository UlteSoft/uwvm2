#!/usr/bin/env python3
"""Official oracles for declaration-only exn/noexn compiler-policy witnesses.

The component test parses these bytes permissively, then independently tightens
its compiler policy. These official checks establish grammar/type validity only;
they do not qualify our product, stricter-policy rejection, execution or latency.
Zero-count local runs intentionally remain binary fixtures: WAT omits that run.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import resource
import subprocess

# Exactly the first eight independent samples in declaration_policy.cc.
# No tag section, throw, ref.null or imported function can supply a hidden gate.
CASES = {
    "exn-parameter": (bytes([1, 0x69, 0]), bytes([0, 11])),
    "exn-result-bottom": (bytes([0, 1, 0x69]), bytes([0, 0, 11])),
    "exn-zero-local": (bytes([0, 0]), bytes([1, 0, 0x69, 11])),
    "exn-two-locals": (bytes([0, 0]), bytes([1, 2, 0x69, 11])),
    "explicit-exn-parameter": (bytes([1, 0x63, 0x69, 0]), bytes([0, 11])),
    "explicit-noexn-parameter": (bytes([1, 0x63, 0x74, 0]), bytes([0, 11])),
    "explicit-noexn-zero-local": (bytes([0, 0]), bytes([1, 0, 0x63, 0x74, 11])),
    "nonnull-exn-zero-local": (bytes([0, 0]), bytes([1, 0, 0x64, 0x69, 11])),
}


def digest(path):
    with Path(path).open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def module_bytes(signature, body):
    # All vectors/section lengths here fit the one-byte unsigned LEB encoding.
    # These fixed fixtures are not a Wasm decoder or handwritten decimal parser.
    type_section = bytes([1, 0x60]) + signature
    code_section = bytes([1, len(body)]) + body
    assert len(type_section) < 128 and len(code_section) < 128
    return (b"\0asm\1\0\0\0" + bytes([1, len(type_section)]) + type_section +
            bytes([3, 2, 1, 0, 10, len(code_section)]) + code_section)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("source-root", "out", "wasm-tools", "wasmtime"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    assert platform.system() == "Linux", "remote Linux only"
    relative = Path("/proc/self/cgroup").read_text().strip().split("::", 1)[1]
    cg = Path("/sys/fs/cgroup") / relative.lstrip("/")
    assert (cg / "memory.max").read_text().strip() == str(64 << 30)
    assert (cg / "memory.swap.max").read_text().strip() == "0"
    assert (cg / "cpuset.cpus.effective").read_text().strip() == "0,2,4,6,16-31"
    assert os.sched_getaffinity(0) and os.sched_getaffinity(0) <= set(range(16, 32))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_FSIZE, (8 << 20, 8 << 20))
    args.out.mkdir(parents=True, exist_ok=False)
    root = args.source_root.resolve(strict=True)
    args.wasm_tools = args.wasm_tools.resolve(strict=True)
    args.wasmtime = args.wasmtime.resolve(strict=True)
    inputs = [Path(__file__).resolve(), args.wasm_tools, args.wasmtime,
              root / "src/uwvm2/validation/standard/wasm3/declaration_policy.h",
              root / "test/0013.uwvm_int/wasm3/declaration_policy.cc"]
    before = {str(path): digest(path) for path in inputs}
    rows, fixtures = [], []

    def run(label, argv):
        stdout, stderr = args.out / (label + ".stdout.log"), args.out / (label + ".stderr.log")
        with stdout.open("xb") as out, stderr.open("xb") as err:
            result = subprocess.run(list(map(str, argv)), stdout=out, stderr=err, timeout=90)
        rows.append({"label": label, "argv": list(map(str, argv)), "returncode": result.returncode,
                     "stdout_sha256": digest(stdout), "stderr_sha256": digest(stderr),
                     "stdout_bytes": stdout.stat().st_size, "stderr_bytes": stderr.stat().st_size})
        (args.out / "runs.json").write_text(json.dumps(rows, indent=2) + "\n")
        assert result.returncode == 0, (label, result.returncode)

    run("wasm-tools-version", [args.wasm_tools, "--version"])
    run("wasmtime-version", [args.wasmtime, "--version"])
    for name, (signature, body) in CASES.items():
        wasm = args.out / (name + ".wasm")
        wasm.write_bytes(module_bytes(signature, body))
        run(name + "-validate", [args.wasm_tools, "validate", "--features", "all", wasm])
        native = args.out / (name + ".cwasm")
        run(name + "-compile", [args.wasmtime, "compile", "-W", "exceptions=y", "-W", "gc=y",
                                "-o", native, wasm])
        assert native.is_file() and native.stat().st_size > 0
        fixtures.append({"name": name, "wasm_sha256": digest(wasm), "wasm_hex": wasm.read_bytes().hex(),
                         "official_compiled_sha256": digest(native), "official_compiled_bytes": native.stat().st_size})
    after = {str(path): digest(path) for path in inputs}
    assert before == after
    summary = {"passed": True, "kind": "official-declaration-grammar-only", "product_qualified": False,
               "stricter_compiler_policy_qualified": False, "memory_max_bytes": 64 << 30,
               "swap_max_bytes": 0, "affinity": sorted(os.sched_getaffinity(0)),
               "inputs_before": before, "inputs_after": after, "fixtures": fixtures, "runs": rows}
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print("PASS 8 official exn/noexn declaration grammar witnesses")


if __name__ == "__main__":
    main()
