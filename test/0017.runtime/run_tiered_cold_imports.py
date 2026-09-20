#!/usr/bin/env python3
"""Exercise a Tiered T1 caller importing a still-cold T0 function.

Use an existing Full binary; do not build the runtime or start compiler workers.
Run under a single-CPU affinity (or pass --cpu on Linux). A zero exit alone does
not pass: the logs must prove native caller compilation and provider T0 demand,
without compiling the provider into LLVM or disabling the configured T0 tier.
The trapping provider additionally checks the exact mixed T0/T1 activation chain,
including checked native unwind with no generated JIT logical frames.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    parser.add_argument("--wat2wasm", default="wat2wasm")
    parser.add_argument("--cpu", help="Linux taskset CPU; otherwise inherit caller affinity")
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    fixtures = Path(__file__).resolve().parent / "fixtures"
    output = Path(tempfile.mkdtemp(prefix="uwvm-tiered-cold-imports-", dir="/tmp"))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    print(f"Artifacts: {output}", flush=True)
    prefix = ["taskset", "-c", args.cpu] if args.cpu else []
    for name in ("provider", "provider_trap", "consumer"):
        subprocess.run(prefix + [args.wat2wasm, str(fixtures / f"tiered_cold_import_{name}.wat"),
                       "--debug-names", "-o", str(output / f"{name}.wasm")],
                       check=True, timeout=30)
    cases = [
        ("control-int-full", "full", "int", "instruction", [], False, False),
        ("control-llvm-full", "full", "jit", "instruction", [], False, False),
        ("control-llvm-lazy", "lazy", "jit", "instruction", [], False, False),
        ("control-tiered-no-t0", "lazy", "tiered", "instruction",
         ["-Rtiered-disable-t0", "-Rtiered-disable-t2"], False, False),
    ]
    for mode in ("lazy", "lazy+verification"):
        for policy in ("instruction", "unwind", "unwind-uncheck"):
            cases.append((f"tiered-t0-{mode}-{policy}", mode, "tiered", policy,
                          ["-Rtiered-disable-t2"], True, False))
            cases.append((f"tiered-t0-trap-{mode}-{policy}", mode, "tiered", policy,
                          ["-Rtiered-disable-t2"], True, True))
    results = []
    for name, mode, compiler, policy, extra, needs_proof, expect_trap in cases:
        provider = output / ("provider_trap.wasm" if expect_trap else "provider.wasm")
        command = prefix + [str(binary), "-Rcm", mode, "-Rcc", compiler,
                            "-Rct", "0", "-Rclog", "err",
                            "-Rllvm-call-stack", policy,
                            "-Rllvm-cache-path", "disable", *extra,
                            "--wasip1-noinherit-system-environment",
                            "--wasm-preload-library", str(provider), "P",
                            "--run", str(output / "consumer.wasm")]
        try:
            proc = subprocess.run(command, capture_output=True, timeout=90, check=False)
            status, stdout, stderr = proc.returncode, proc.stdout, proc.stderr
        except subprocess.TimeoutExpired as error:
            status, stdout, stderr = 124, error.stdout or b"", error.stderr or b""
        stem = output / name
        stem.with_suffix(".stdout").write_bytes(stdout)
        stem.with_suffix(".stderr").write_bytes(stderr)
        stem.with_suffix(".command.json").write_text(json.dumps(command, indent=2) + "\n")
        log = re.sub(r"\x1b\[[0-?]*[ -/]*[@-~]", "", stderr.decode(errors="replace"))
        # Qualify the backend: an interpreter compile-end for fn=1 is not
        # evidence that LLVM materialized the caller.
        demand = re.search(r'\[llvm-jit-lazy\] tiered-demand-request module="(?P<module>[^"]+)"[^\n]* fn=1 local_fn=0[^\n]* lane=inline', log)
        compiled = re.search(r'\[llvm-jit-lazy\] compile-end module="(?P<module>[^"]+)"[^\n]* local_fn=0 fn=1[^\n]* state=compiled', log)
        native_caller = bool(demand and compiled and demand.group("module") == compiled.group("module"))
        provider_llvm = bool(re.search(r'\[llvm-jit-lazy\] compile-start module="P"', log))
        provider_t0 = bool(re.search(r'\[uwvm-int-lazy\] compile-end module="P"[^\n]* local_fn=0 fn=0[^\n]* state=compiled', log))
        # Provider has no start function and is first called only after the
        # native caller has returned once. T2 is disabled. This excludes both
        # all-T0 execution and forcing every imported callee into LLVM.
        proof = native_caller and provider_t0 and not provider_llvm
        # The custom module names distinguish the provider's function 0 from
        # the consumer's import slot 0. Match every numbered frame, in order;
        # omitted, duplicated, or extra native frames must not pass silently.
        frames = [(int(index), module, int(function)) for index, module, function in
                  re.findall(r'^uwvm: \[info\]\s+#(\d+) module=(.*?) func_idx=(\d+)(?: func_name="[^"]*")?\s*$',
                             log, re.MULTILINE)]
        expected_frames = [(0, "tiered_cold_provider", 0),
                           (1, "tiered_cold_consumer", 1),
                           (2, "tiered_cold_consumer", 2)]
        trap_matches = "Runtime crash (catch unreachable)" in log and frames == expected_frames
        outcome = (status != 0 and status != 124 and trap_matches) if expect_trap else status == 0
        passed = outcome and stdout == b"" and (not needs_proof or proof)
        item = dict(name=name, status=status, passed=passed,
                    native_caller_proven=native_caller,
                    provider_t0_materialized=provider_t0,
                    provider_llvm_materialized=provider_llvm,
                    expected_trap=expect_trap, frames=frames,
                    stderr_sha256=hashlib.sha256(stderr).hexdigest())
        results.append(item)
        print(json.dumps(item), flush=True)
    (output / "summary.json").write_text(json.dumps(results, indent=2) + "\n")
    raise SystemExit(0 if all(item["passed"] for item in results) else 1)


if __name__ == "__main__":
    main()
