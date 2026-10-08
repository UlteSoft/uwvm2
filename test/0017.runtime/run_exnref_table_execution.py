#!/usr/bin/env python3
"""Exercise Core 3 exception-reference tables against Wasmtime and UWVM.

Run only in the bounded Linux test cgroup. The imported-table fixture also
checks retention when the destination table belongs to a different module.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import resource
import subprocess


MODES = {
    "int-full": ["-Rcc", "int", "-Rcm", "full"],
    "int-lazy": ["-Rcc", "int", "-Rcm", "lazy"],
    "int-lazy-verified": ["-Rcc", "int", "-Rcm", "lazy+verification"],
    "jit-full": ["-Rcc", "jit", "-Rcm", "full"],
    "jit-lazy": ["-Rcc", "jit", "-Rcm", "lazy"],
    "jit-lazy-verified": ["-Rcc", "jit", "-Rcm", "lazy+verification"],
    "tiered-lazy": ["-Rcc", "tiered", "-Rcm", "lazy", "-Rct", "0"],
    "tiered-lazy-verified": ["-Rcc", "tiered", "-Rcm", "lazy+verification", "-Rct", "0"],
}
ROS_MODES = {"int-full": ["-Rint"], "jit-full": ["-Raot"]}
ENABLED = ["-WFE-exceptions", "-WFE-reference-types", "-WFE-table-instructions"]
UNRELATED_DISABLED = ["-WFD-gc", "-WFD-function-references", "-WFD-threads"]
REQUIRED = ("exceptions", "reference-types", "table-instructions")
FIXTURES = ("exnref_table_execution", "exnref_table64_execution", "exnref_table_element_execution",
            "exnref_table_import_provider", "exnref_table_import_consumer", "exnref_table_foreign_consumer",
            "exnref_table_import_mismatch", "exnref_table_alias", "exnref_table_alias_consumer",
            "exnref_table_alias_nonnull_import")


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--uwvm", type=Path, required=True)
    parser.add_argument("--wasm-tools", type=Path, required=True)
    parser.add_argument("--wasmtime", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--mode", action="append", default=[])
    parser.add_argument("--ros", action="store_true")
    parser.add_argument("--wrapper-arg", action="append", default=[],
                        help="repeat an exact target launcher argument for QEMU execution")
    args = parser.parse_args()
    root = args.source_root.resolve(strict=True)
    subprocess.run(["bash", str(root / "tools/ci/require_wasm3_test_cgroup.sh")], cwd=root, check=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    modes = ROS_MODES if args.ros else MODES
    selected = args.mode or list(modes)
    if any(mode not in modes for mode in selected):
        parser.error("unsupported mode for this product")
    args.out.mkdir(parents=True, exist_ok=False)
    rows = []

    def check(name, command, expect_success, required_output=()):
        process = subprocess.run([str(part) for part in command], capture_output=True, timeout=120)
        output = process.stdout + process.stderr
        (args.out / f"{name}.log").write_bytes(output)
        plain_output = re.sub(rb"\x1b\[[0-9;]*m", b"", output).decode(errors="replace")
        passed = ((process.returncode == 0) == expect_success and
                  all(fragment in plain_output for fragment in required_output))
        rows.append({"name": name, "command": [str(part) for part in command],
                     "exit": process.returncode, "required_output": required_output,
                     "passed": passed})
        (args.out / "runs.json").write_text(json.dumps(rows, indent=2) + "\n")
        if not passed:
            print(f"FAIL {name}: {output.decode(errors='replace')[-1200:]}", flush=True)
        return passed

    binaries = {}
    for name in FIXTURES:
        wat = root / "test/0017.runtime/fixtures" / f"{name}.wat"
        wasm = args.out / f"{name}.wasm"
        binaries[name] = wasm
        if not check(f"{name}-parse", [args.wasm_tools, "parse", wat, "-o", wasm], True):
            raise SystemExit(1)
        if not check(f"{name}-validate", [args.wasm_tools, "validate", wasm], True):
            raise SystemExit(1)

    if not check("wasmtime-local", [args.wasmtime, "run", "-C", "cache=n", "-W", "exceptions=y",
                                    binaries["exnref_table_execution"]], True):
        raise SystemExit(1)
    if not check("wasmtime-elements", [args.wasmtime, "run", "-C", "cache=n", "-W", "exceptions=y",
                                       binaries["exnref_table_element_execution"]], True):
        raise SystemExit(1)
    if not check("wasmtime-table64", [args.wasmtime, "run", "-C", "cache=n", "-W", "exceptions=y",
                                      binaries["exnref_table64_execution"]], True):
        raise SystemExit(1)
    if not check("wasmtime-import", [args.wasmtime, "run", "-C", "cache=n", "-W", "exceptions=y",
                                     "--preload", f"P={binaries['exnref_table_import_provider']}",
                                     binaries["exnref_table_import_consumer"]], True):
        raise SystemExit(1)
    if not check("wasmtime-import-mismatch", [args.wasmtime, "run", "-C", "cache=n", "-W", "exceptions=y",
                                              "--preload", f"P={binaries['exnref_table_import_provider']}",
                                              binaries["exnref_table_import_mismatch"]], False):
        raise SystemExit(1)
    if not check("wasmtime-foreign-import", [args.wasmtime, "run", "-C", "cache=n", "-W", "exceptions=y",
                                             "--preload", f"P={binaries['exnref_table_import_provider']}",
                                             binaries["exnref_table_foreign_consumer"]], True):
        raise SystemExit(1)
    alias_preloads = ["--preload", f"P={binaries['exnref_table_import_provider']}",
                      "--preload", f"B={binaries['exnref_table_alias']}"]
    if not check("wasmtime-three-module-alias", [args.wasmtime, "run", "-C", "cache=n", "-W", "exceptions=y",
                                                  *alias_preloads, binaries["exnref_table_alias_consumer"]], True):
        raise SystemExit(1)
    if not check("wasmtime-alias-nonnull-mismatch", [args.wasmtime, "run", "-C", "cache=n", "-W", "exceptions=y",
                                                     *alias_preloads, binaries["exnref_table_alias_nonnull_import"]], False,
                 ("expected table of type `(ref exn)`", "found table of type `(ref null exn)`")):
        raise SystemExit(1)

    for mode in selected:
        base = [*args.wrapper_arg, args.uwvm, *modes[mode], *ENABLED, *UNRELATED_DISABLED]
        local = ["--run", binaries["exnref_table_execution"]]
        imported = ["-WFE-multiple-tables", "--wasm-set-main-module-name", "C",
                    "--wasm-preload-library", binaries["exnref_table_import_provider"], "P",
                    "--run", binaries["exnref_table_import_consumer"]]
        if not check(f"{mode}-local", [*base, *local], True):
            raise SystemExit(1)
        if not check(f"{mode}-elements", [*base, "--run", binaries["exnref_table_element_execution"]], True):
            raise SystemExit(1)
        if not check(f"{mode}-table64", [*base, "-WFE-table64", "--run",
                                         binaries["exnref_table64_execution"]], True):
            raise SystemExit(1)
        if not check(f"{mode}-table64-off", [*base, "-WFD-table64", "--run",
                                             binaries["exnref_table64_execution"]], False):
            raise SystemExit(1)
        if not check(f"{mode}-import", [*base, *imported], True):
            raise SystemExit(1)
        if not check(f"{mode}-foreign-import", [*base, *imported[:-1],
                                               binaries["exnref_table_foreign_consumer"]], True):
            raise SystemExit(1)
        if not check(f"{mode}-import-mismatch", [*base, *imported[:-1],
                                                binaries["exnref_table_import_mismatch"]], False):
            raise SystemExit(1)
        alias_imported = ["-WFE-multiple-tables", "--wasm-set-main-module-name", "C",
                          "--wasm-preload-library", binaries["exnref_table_import_provider"], "P",
                          "--wasm-preload-library", binaries["exnref_table_alias"], "B",
                          "--run", binaries["exnref_table_alias_consumer"]]
        if not check(f"{mode}-three-module-alias", [*base, *alias_imported], True):
            raise SystemExit(1)
        if not check(f"{mode}-alias-nonnull-mismatch",
                     [*base, *alias_imported[:-1], binaries["exnref_table_alias_nonnull_import"]], False,
                     ("expected (ref exn)", "got exnref (ref null exn)")):
            raise SystemExit(1)
        for feature in REQUIRED:
            without = [flag for flag in ENABLED if flag != f"-WFE-{feature}"]
            flags = [*without, f"-WFD-{feature}", *UNRELATED_DISABLED]
            if not check(f"{mode}-local-{feature}-off",
                         [*args.wrapper_arg, args.uwvm, *modes[mode], *flags, *local], False):
                raise SystemExit(1)
            if not check(f"{mode}-import-{feature}-off",
                         [*args.wrapper_arg, args.uwvm, *modes[mode], *flags, *imported], False):
                raise SystemExit(1)
        if not check(f"{mode}-import-multiple-tables-off",
                     [*base, "-WFD-multiple-tables", *imported[1:]], False):
            raise SystemExit(1)

    summary = {"passed": all(row["passed"] for row in rows), "checks": len(rows),
               "scope": "uwvm2-ros" if args.ros else "uwvm2",
               "product_sha256": digest(args.uwvm),
               "fixtures": {name: digest(root / "test/0017.runtime/fixtures" / f"{name}.wat") for name in FIXTURES}}
    (args.out / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(f"Core 3 exnref tables {summary['scope']}: "
          f"{sum(row['passed'] for row in rows)}/{len(rows)}", flush=True)
    if not summary["passed"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
