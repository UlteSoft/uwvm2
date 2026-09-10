#!/usr/bin/env python3
"""Exercise imported calls without replacing an already published lazy registry.

Use an existing Full binary; this test neither builds the runtime nor starts
compiler workers. Keep artifacts, including failed guest diagnostics, in /tmp.
"""

import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path)
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    fixtures = Path(__file__).resolve().parent / "fixtures"
    output = Path(tempfile.mkdtemp(prefix="uwvm-llvm-raw-imports-", dir="/tmp"))
    print(f"Artifacts: {output}", flush=True)
    for name in ("wasi", "provider", "consumer"):
        subprocess.run(["wat2wasm", str(fixtures / f"llvm_raw_import_{name}.wat"),
                        "-o", str(output / f"{name}.wasm")], check=True, timeout=30)

    # The verification bit is part of the publication signature. Both lazy
    # entry policies must survive an imported call with that bit unchanged.
    cases = [("llvm-lazy", "lazy", "jit", []),
             ("llvm-lazy-verified", "lazy+verification", "jit", []),
             ("llvm-full", "full", "jit", []),
             ("tiered-lazy", "lazy", "tiered", ["-Rtiered-disable-t2"]),
             ("tiered-lazy-verified", "lazy+verification", "tiered", ["-Rtiered-disable-t2"]),
             ("tiered-t1", "lazy", "tiered", ["-Rtiered-disable-t0", "-Rtiered-disable-t2"]),
             ("tiered-t1-verified", "lazy+verification", "tiered", ["-Rtiered-disable-t0", "-Rtiered-disable-t2"])]
    for name, mode, compiler, extra in cases:
        options = [str(binary), "-Rcm", mode, "-Rcc", compiler, "-Rct", "0",
                   "-Rllvm-call-stack", "instruction", "-Rllvm-cache-path", "disable",
                   "--wasip1-noinherit-system-environment", *extra]
        for guest in ("wasi", "consumer"):
            command = options.copy()
            if guest == "consumer":
                command += ["--wasm-preload-library", str(output / "provider.wasm"), "P"]
            command += ["--run", str(output / f"{guest}.wasm")]
            result = subprocess.run(command, capture_output=True, timeout=60, check=False)
            prefix = output / f"{name}-{guest}"
            prefix.with_suffix(".stdout").write_bytes(result.stdout)
            prefix.with_suffix(".stderr").write_bytes(result.stderr)
            prefix.with_suffix(".status").write_text(f"{result.returncode}\n")
            expected = b"raw import ok\n" if guest == "wasi" else b""
            if result.returncode != 0 or result.stdout != expected:
                raise RuntimeError(f"{name}/{guest}: exit={result.returncode}; see {prefix}.*")
            print(f"PASS {name}/{guest}", flush=True)


if __name__ == "__main__":
    main()
