#!/usr/bin/env python3
"""Execute Core 3 initializer fixtures and check complete trap stacks in actual backends."""

import argparse
import json
import pathlib
import re
import resource
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("uwvm", type=pathlib.Path)
    parser.add_argument("fixtures", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("--backend", choices=("jit", "int", "tiered"), default="jit")
    parser.add_argument("--modes", nargs="+")
    parser.add_argument("--ros", action="store_true", help="use the reduced full-only ROS CLI")
    parser.add_argument("--tiered-variant", choices=("all", "no-t0", "no-t2", "no-t0-no-t2"), default="all")
    args = parser.parse_args()
    if args.modes is None:
        args.modes = ("full",) if args.ros else (("lazy", "lazy+verification") if args.backend == "tiered" else ("full", "lazy", "lazy+verification"))
    if args.ros and (args.modes != ("full",) and args.modes != ["full"] or args.backend == "tiered"):
        parser.error("ROS supports only full int/jit modes")
    args.output.mkdir(parents=True, exist_ok=True)
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    rows = []
    ansi = re.compile(r"\x1b\[[0-9;?]*[ -/]*[@-~]")
    features = ["-WFE-extended-const", "-WFE-table-initializer"]
    policies = ("instruction", "unwind", "none") if args.backend == "jit" else ("instruction",)
    for mode in args.modes:
        for policy in policies:
            for fixture, expected_trap in (("wasm3_initializers", False), ("wasm3_initializer_trap", True)):
                name = f"{args.backend}-{mode}-{policy}-{fixture}"
                if args.backend == "tiered":
                    name += f"-{args.tiered_variant}"
                compile_log = args.output / f"{name}.compile.log"
                command = [str(args.uwvm.resolve())]
                command += (["-Raot" if args.backend == "jit" else "-Rint"] if args.ros else ["-Rcc", args.backend, "-Rcm", mode])
                if args.backend == "tiered":
                    if "no-t0" in args.tiered_variant:
                        command += ["-Rtiered-disable-t0"]
                    if "no-t2" in args.tiered_variant:
                        command += ["-Rtiered-disable-t2"]
                if args.backend != "int":
                    command += ["-Rllvm-cache-path", "disable", "-Rllvm-call-stack", policy]
                command += ["-Rclog", "file", str(compile_log), *features, "--run", str(args.fixtures / f"{fixture}.wasm")]
                run = subprocess.run(command, capture_output=True, timeout=60)
                output = ansi.sub("", (run.stdout + run.stderr).decode(errors="replace"))
                (args.output / f"{name}.run.log").write_text(output)
                stack = [int(n) for n in re.findall(r"func_idx=(\d+)", output)]
                expected_stack = [] if policy == "none" else [0, 1, 2]
                if expected_trap:
                    if run.returncode == 0 or "catch unreachable" not in output or stack != expected_stack:
                        raise RuntimeError(f"{name}: exit={run.returncode}, stack={stack}\n{output}")
                elif run.returncode != 0:
                    raise RuntimeError(f"{name}: exit={run.returncode}\n{output}")
                if args.backend == "jit":
                    compilation = compile_log.read_text()
                    if "llvm-jit" not in compilation or not ("optimize-start" in compilation if mode == "full" else "compile-end" in compilation):
                        raise RuntimeError(f"{name}: missing evidence of actual JIT compilation")
                    declarations = [line for line in compilation.splitlines() if "optimize-start" in line]
                    expected_frames = "emit" if policy == "instruction" else "omit"
                    if mode == "full" and not all(f"call_stack={policy} " in line and f"call_stack_frames={expected_frames}" in line for line in declarations):
                        raise RuntimeError(f"{name}: effective call-stack policy differs: {declarations}")
                if args.backend == "tiered" and "no-t0" in args.tiered_variant:
                    compilation = compile_log.read_text()
                    if "[llvm-jit-lazy] compile-end" not in compilation or "[uwvm-int-lazy] demand-request" in compilation:
                        raise RuntimeError(f"{name}: no-T0 variant did not compile through LLVM")
                rows.append({"case": name, "exit": run.returncode, "stack": stack, "passed": True})
    # Verify that either feature can be disabled without a runtime silently accepting its encoding.
    for omitted in features:
        command = [str(args.uwvm.resolve()), "-m", "validation"]
        command += [flag for flag in features if flag != omitted]
        command += ["--run", str(args.fixtures / "wasm3_initializers.wasm")]
        run = subprocess.run(command, capture_output=True, timeout=30)
        output = ansi.sub("", (run.stdout + run.stderr).decode(errors="replace"))
        required_flag = "--wasm-feature-enable-" + omitted.removeprefix("-WFE-")
        if run.returncode == 0 or "Parsing error in WebAssembly File" not in output or required_flag not in output:
            raise RuntimeError(f"Disabled feature accepted or failure was not diagnosed: {omitted}\n{output}")
        rows.append({"case": f"disabled:{omitted}", "exit": run.returncode, "passed": True})
    print(json.dumps({"passed": len(rows), "cases": rows}, indent=2))


if __name__ == "__main__":
    main()
