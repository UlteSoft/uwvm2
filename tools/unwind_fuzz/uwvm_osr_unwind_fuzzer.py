#!/usr/bin/env python3
import argparse
from dataclasses import dataclass
import os
import random
import re
import shlex
import signal
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ANSI_RE = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
FUNC_RE = re.compile(r"func_idx=(\d+)")
TRAP_RE = re.compile(r"Runtime crash \(([^)]+)\)")


TRAPS = (
    "unreachable",
    "i32_div_s_zero",
    "i32_div_u_zero",
    "i32_overflow",
    "i64_div_s_zero",
    "i64_div_u_zero",
    "i64_overflow",
    "invalid_i32_trunc_f32_s",
    "invalid_i32_trunc_f32_u",
    "invalid_i32_trunc_f64_s",
    "invalid_i32_trunc_f64_u",
    "invalid_i64_trunc_f32_s",
    "invalid_i64_trunc_f32_u",
    "invalid_i64_trunc_f64_s",
    "invalid_i64_trunc_f64_u",
    "oob_load",
    "oob_store",
    "call_indirect_type",
    "call_indirect_null",
    "call_indirect_oob",
)

EXPECTED_TRAPS = {
    "unreachable": "catch unreachable",
    "i32_div_s_zero": "integer divide by zero",
    "i32_div_u_zero": "integer divide by zero",
    "i32_overflow": "integer overflow",
    "i64_div_s_zero": "integer divide by zero",
    "i64_div_u_zero": "integer divide by zero",
    "i64_overflow": "integer overflow",
    "invalid_i32_trunc_f32_s": "invalid conversion to integer",
    "invalid_i32_trunc_f32_u": "invalid conversion to integer",
    "invalid_i32_trunc_f64_s": "invalid conversion to integer",
    "invalid_i32_trunc_f64_u": "invalid conversion to integer",
    "invalid_i64_trunc_f32_s": "invalid conversion to integer",
    "invalid_i64_trunc_f32_u": "invalid conversion to integer",
    "invalid_i64_trunc_f64_s": "invalid conversion to integer",
    "invalid_i64_trunc_f64_u": "invalid conversion to integer",
    "oob_load": "memory access out of bounds",
    "oob_store": "memory access out of bounds",
    "call_indirect_type": "call_indirect: signature mismatch",
    "call_indirect_null": "call_indirect: uninitialized element",
    "call_indirect_oob": "call_indirect: table index out of bounds",
}


DEFAULT_POLICIES = ("instruction", "auto")


OSR_MODES = (
    ("tiered", ["-Rtiered"]),
    ("tiered_full_ready", ["-Rtiered", "-Rct", "2", "-Rllvm-policy", "max"]),
    ("tiered_no_t2", ["-Rtiered", "-Rtiered-disable-t2"]),
)


def strip_ansi(s: str) -> str:
    return ANSI_RE.sub("", s)


def parse_output(s: str):
    plain = strip_ansi(s)
    trap_match = TRAP_RE.search(plain)
    return (trap_match.group(1) if trap_match else "", [int(m.group(1)) for m in FUNC_RE.finditer(plain)])


def leaf_prefix_and_body(trap: str):
    if trap == "unreachable":
        return "", "    unreachable"
    if trap == "i32_div_s_zero":
        return "", "    i32.const 123\n    i32.const 0\n    i32.div_s\n    drop"
    if trap == "i32_div_u_zero":
        return "", "    i32.const 123\n    i32.const 0\n    i32.div_u\n    drop"
    if trap == "i32_overflow":
        return "", "    i32.const -2147483648\n    i32.const -1\n    i32.div_s\n    drop"
    if trap == "i64_div_s_zero":
        return "", "    i64.const 123\n    i64.const 0\n    i64.div_s\n    drop"
    if trap == "i64_div_u_zero":
        return "", "    i64.const 123\n    i64.const 0\n    i64.div_u\n    drop"
    if trap == "i64_overflow":
        return "", "    i64.const -9223372036854775808\n    i64.const -1\n    i64.div_s\n    drop"
    if trap == "invalid_i32_trunc_f32_s":
        return "", "    f32.const nan\n    i32.trunc_f32_s\n    drop"
    if trap == "invalid_i32_trunc_f32_u":
        return "", "    f32.const nan\n    i32.trunc_f32_u\n    drop"
    if trap == "invalid_i32_trunc_f64_s":
        return "", "    f64.const nan\n    i32.trunc_f64_s\n    drop"
    if trap == "invalid_i32_trunc_f64_u":
        return "", "    f64.const nan\n    i32.trunc_f64_u\n    drop"
    if trap == "invalid_i64_trunc_f32_s":
        return "", "    f32.const nan\n    i64.trunc_f32_s\n    drop"
    if trap == "invalid_i64_trunc_f32_u":
        return "", "    f32.const nan\n    i64.trunc_f32_u\n    drop"
    if trap == "invalid_i64_trunc_f64_s":
        return "", "    f64.const nan\n    i64.trunc_f64_s\n    drop"
    if trap == "invalid_i64_trunc_f64_u":
        return "", "    f64.const nan\n    i64.trunc_f64_u\n    drop"
    if trap == "oob_load":
        return "  (memory 1)\n", "    i32.const 2147483647\n    i32.load\n    drop"
    if trap == "oob_store":
        return "  (memory 1)\n", "    i32.const 2147483647\n    i64.const 7\n    i64.store"
    if trap == "call_indirect_type":
        return (
            "  (type $i (func (param i32)))\n"
            "  (table 1 funcref)\n"
            "  (elem (i32.const 0) $target)\n"
            "  (func $target (type $i) (param $x i32)\n"
            "    local.get $x\n"
            "    drop)\n",
            "    i32.const 0\n    call_indirect (type $v)",
        )
    if trap == "call_indirect_null":
        return "  (table 1 funcref)\n", "    i32.const 0\n    call_indirect (type $v)"
    if trap == "call_indirect_oob":
        return "  (table 1 funcref)\n", "    i32.const 7\n    call_indirect (type $v)"
    raise AssertionError(trap)


def caller_chain(depth: int) -> str:
    previous = "$loop_then_trap"
    lines = []
    for i in range(depth):
        name = f"$caller{i}"
        lines.append(f"  (func {name} (type $v) call {previous})")
        previous = name
    lines.append(f'  (func $_start (export "_start") (type $v) call {previous})')
    return "\n".join(lines)


def make_wat(trap: str, depth: int, nops: int, loop_count: int) -> str:
    prefix, leaf = leaf_prefix_and_body(trap)
    nop_text = "".join("    nop\n" for _ in range(nops))
    return (
        "(module\n"
        "  (type $v (func))\n"
        f"{prefix}"
        "  (func $leaf (type $v)\n"
        f"{leaf})\n"
        "  (func $loop_then_trap (type $v)\n"
        "    (local $i i32)\n"
        f"{nop_text}"
        "    i32.const 0\n"
        "    local.set $i\n"
        "    block\n"
        "      loop\n"
        "        local.get $i\n"
        f"        i32.const {loop_count}\n"
        "        i32.ge_u\n"
        "        br_if 1\n"
        "        local.get $i\n"
        "        i32.const 1\n"
        "        i32.add\n"
        "        local.set $i\n"
        "        br 0\n"
        "      end\n"
        "    end\n"
        "    call $leaf)\n"
        f"{caller_chain(depth)}\n"
        ")\n"
    )


@dataclass
class CommandResult:
    returncode: int
    stdout: str
    timed_out: bool = False


def run_cmd(args, cwd: Path, timeout: int, max_output_bytes: int) -> CommandResult:
    with tempfile.TemporaryFile() as output:
        process = subprocess.Popen(
            args,
            cwd=cwd,
            stdout=output,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )
        timed_out = False
        try:
            returncode = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            returncode = 124
            try:
                os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                process.wait(timeout=2)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(process.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                process.wait()

        size = output.tell()
        offset = max(0, size - max_output_bytes)
        output.seek(offset)
        text = output.read().decode(errors="replace")
        if offset:
            text = f"[output truncated to last {max_output_bytes} bytes]\n{text}"
        return CommandResult(returncode, text, timed_out)


def read_bounded(path: Path, max_output_bytes: int) -> str:
    if not path.exists():
        return ""
    with path.open("rb") as source:
        source.seek(0, os.SEEK_END)
        size = source.tell()
        offset = max(0, size - max_output_bytes)
        source.seek(offset)
        text = source.read().decode(errors="replace")
    if offset:
        return f"[log truncated to last {max_output_bytes} bytes]\n{text}"
    return text


def detect_call_stack_policies(uwvm_command: list[str], cwd: Path, timeout: int, max_output_bytes: int):
    result = run_cmd([*uwvm_command, "--help", "runtime"], cwd, timeout, max_output_bytes)
    if result.returncode != 0:
        raise RuntimeError(f"uwvm runtime help probe failed ({result.returncode}):\n{result.stdout}")
    help_text = strip_ansi(result.stdout)
    if re.search(r"runtime-llvm-jit-call-stack[\s\S]{0,500}\bunwind\b", help_text):
        return ("instruction", "unwind", "auto")
    return DEFAULT_POLICIES


def detect_wasm2_feature_args(
    uwvm_command: list[str], cwd: Path, timeout: int, max_output_bytes: int
) -> list[str]:
    result = run_cmd([*uwvm_command, "--help", "wasm"], cwd, timeout, max_output_bytes)
    if result.returncode != 0:
        raise RuntimeError(f"uwvm wasm help probe failed ({result.returncode}):\n{result.stdout}")
    help_text = strip_ansi(result.stdout)
    if "--wasm-feature-wasm2" in help_text:
        return ["--wasm-feature-wasm2"]
    return [
        "--wasm-feature-enable-bulk-memory",
        "--wasm-feature-enable-multi-value",
        "--wasm-feature-enable-multiple-tables",
        "--wasm-feature-enable-reference-types",
        "--wasm-feature-enable-table-instructions",
    ]


def split_run_prefix(ap: argparse.ArgumentParser, value: str) -> list[str]:
    try:
        command = shlex.split(value)
    except ValueError as exc:
        ap.error(f"--run-prefix: {exc}")
    if not command:
        ap.error("--run-prefix must not be empty")
    return command


def probe_auto_policy(
    uwvm_command: list[str],
    wat2wasm: str,
    wasm2_feature_args: list[str],
    cwd: Path,
    work: Path,
    timeout: int,
    max_output_bytes: int,
) -> str:
    wat_path = work / "auto_live_probe.wat"
    wasm_path = work / "auto_live_probe.wasm"
    log_path = work / "auto_live_probe.log"
    wat_path.write_text(
        "(module\n"
        "  (type $v (func))\n"
        "  (memory 1)\n"
        "  (func $leaf (type $v) i32.const -1 i32.load drop)\n"
        "  (func $f1 (type $v) call $leaf)\n"
        "  (func $f2 (type $v) call $f1)\n"
        '  (func $_start (export "_start") (type $v) call $f2))\n'
    )
    compiled = run_cmd([wat2wasm, str(wat_path), "-o", str(wasm_path)], cwd, timeout, max_output_bytes)
    if compiled.timed_out or compiled.returncode != 0:
        raise RuntimeError(f"auto live-probe wat2wasm failed ({compiled.returncode}):\n{compiled.stdout}")

    log_path.unlink(missing_ok=True)
    result = run_cmd(
        [
            *uwvm_command,
            "-Rcm",
            "full",
            "-Rcc",
            "jit",
            "-Rllvm-cache-path",
            "disable",
            "-Rllvm-call-stack",
            "auto",
            "-Rclog",
            "file",
            str(log_path),
            *wasm2_feature_args,
            "--run",
            str(wasm_path),
        ],
        cwd,
        timeout,
        max_output_bytes,
    )
    trap, funcs = parse_output(result.stdout)
    log = read_bounded(log_path, max_output_bytes)
    match = re.findall(r"\bcall_stack=(unwind|none|instruction)\b", log)
    effective = match[-1] if match else "unknown"
    if result.timed_out or result.returncode == 0 or trap != "memory access out of bounds":
        raise RuntimeError(f"auto live probe did not produce the expected OOB trap:\n{log}\n{result.stdout}")
    if effective == "instruction":
        raise RuntimeError(f"auto made a forbidden conversion to Instruction frames:\n{log}")
    if effective == "unwind":
        strict_source = "capture_source=seeded-libunwind backend=libunwind resolved_jit_caller=yes" in log
        if funcs != [0, 1, 2, 3] or not strict_source:
            raise RuntimeError(f"auto unwind live probe did not resolve the seeded JIT stack:\n{log}\n{result.stdout}")
        return effective
    if effective == "none":
        if funcs or "call_stack_frames=omit" not in log:
            raise RuntimeError(f"auto none emitted Instruction frames or failed to omit JIT frames:\n{log}\n{result.stdout}")
        return effective
    raise RuntimeError(f"auto live probe did not log an effective policy:\n{log}")


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Compare LLVM tiered-OSR trap stacks across call-stack policies."
    )
    ap.add_argument("--root", required=True, type=Path, help="uwvm2 source-tree root used as the command working directory")
    ap.add_argument("--uwvm", required=True, type=Path, help="uwvm executable")
    ap.add_argument(
        "--run-prefix",
        default="",
        metavar="COMMAND",
        help="shell-like wrapper prepended to --uwvm, for example 'qemu-aarch64 -L SYSROOT'",
    )
    ap.add_argument("--wat2wasm", default=shutil.which("wat2wasm"), help="wat2wasm executable")
    ap.add_argument("--cases", type=int, default=256, help="number of randomized OSR trap cases")
    ap.add_argument("--seed", type=lambda value: int(value, 0), default=0x05A0F00D)
    ap.add_argument("--timeout", type=int, default=120, help="per-process timeout in seconds")
    ap.add_argument("--max-output-mib", type=int, default=4, help="maximum retained output per subprocess or log")
    ap.add_argument("--work-dir", type=Path, default=None)
    ns = ap.parse_args()

    if not ns.wat2wasm:
        print("wat2wasm not found", file=sys.stderr)
        return 2
    if ns.cases <= 0:
        ap.error("--cases must be greater than zero")
    if ns.timeout <= 0:
        ap.error("--timeout must be greater than zero")
    if ns.max_output_mib <= 0:
        ap.error("--max-output-mib must be greater than zero")
    ns.root = ns.root.resolve()
    ns.uwvm = ns.uwvm.resolve()
    ns.wat2wasm = str(Path(ns.wat2wasm).resolve())
    if not ns.root.is_dir():
        ap.error(f"--root is not a directory: {ns.root}")
    if not ns.uwvm.is_file():
        ap.error(f"--uwvm is not a file: {ns.uwvm}")
    if not Path(ns.wat2wasm).is_file():
        ap.error(f"--wat2wasm is not a file: {ns.wat2wasm}")
    run_prefix = split_run_prefix(ap, ns.run_prefix) if ns.run_prefix else []
    if run_prefix and shutil.which(run_prefix[0]) is None:
        ap.error(f"--run-prefix executable not found: {run_prefix[0]}")
    uwvm_command = [*run_prefix, str(ns.uwvm)]
    max_output_bytes = ns.max_output_mib * 1024 * 1024

    rng = random.Random(ns.seed)
    base = ns.work_dir if ns.work_dir is not None else Path("/tmp")
    work = base / f"uwvm_osr_unwind_fuzz_{os.getpid()}"
    work.mkdir(parents=True, exist_ok=True)
    try:
        policies = detect_call_stack_policies(uwvm_command, ns.root, ns.timeout, max_output_bytes)
        wasm2_feature_args = detect_wasm2_feature_args(uwvm_command, ns.root, ns.timeout, max_output_bytes)
        auto_effective_policy = probe_auto_policy(
            uwvm_command,
            ns.wat2wasm,
            wasm2_feature_args,
            ns.root,
            work,
            ns.timeout,
            max_output_bytes,
        )
    except RuntimeError as exc:
        print(f"[osr-fuzz] setup failed: {exc}", file=sys.stderr)
        return 2

    print(f"[osr-fuzz] work={work}")
    print(f"[osr-fuzz] seed={ns.seed} cases={ns.cases}")
    print(f"[osr-fuzz] uwvm-command={shlex.join(uwvm_command)}")
    print(f"[osr-fuzz] wasm2-feature-args={shlex.join(wasm2_feature_args)}")
    print(f"[osr-fuzz] call-stack-policies={','.join(policies)}")
    print(f"[osr-fuzz] auto-effective-policy={auto_effective_policy}")

    failures = []
    skipped_no_osr = 0
    total_runs = 0
    for case_id in range(ns.cases):
        trap = rng.choice(TRAPS)
        depth = rng.randint(0, 4)
        nops = rng.randint(1500, 1800)
        loop_count = rng.randint(300000, 500000)
        wat = make_wat(trap, depth, nops, loop_count)
        wat_path = work / f"osr_{case_id:04d}_{trap}_d{depth}.wat"
        wasm_path = work / f"osr_{case_id:04d}_{trap}_d{depth}.wasm"
        wat_path.write_text(wat)
        r = run_cmd(
            [ns.wat2wasm, str(wat_path), "-o", str(wasm_path)],
            ns.root,
            ns.timeout,
            max_output_bytes,
        )
        if r.timed_out or r.returncode != 0:
            failures.append((case_id, trap, "wat2wasm", r.stdout, wat_path))
            continue

        case_failure_count = len(failures)
        skipped_modes = []
        for mode_name, mode_args in OSR_MODES:
            results = []
            for policy in policies:
                out_path = work / f"osr_{case_id:04d}_{trap}_d{depth}.{mode_name}.{policy}.out"
                log_path = work / f"osr_{case_id:04d}_{trap}_d{depth}.{mode_name}.{policy}.log"
                args = [
                    *uwvm_command,
                    *mode_args,
                    "-Rllvm-cache-path",
                    "disable",
                    "-Rllvm-call-stack",
                    policy,
                    "-Rclog",
                    "file",
                    str(log_path),
                    *wasm2_feature_args,
                    "--run",
                    str(wasm_path),
                ]
                log_path.unlink(missing_ok=True)
                r = run_cmd(args, ns.root, ns.timeout, max_output_bytes)
                total_runs += 1
                log = read_bounded(log_path, max_output_bytes)
                parsed = parse_output(r.stdout)
                has_osr = "tiered-osr-request" in log
                if r.timed_out:
                    out_path.write_text(r.stdout)
                    failures.append((case_id, trap, f"{mode_name}/{policy}/timeout", r.stdout, wat_path))
                    continue
                if r.returncode == 0 or not parsed[0]:
                    out_path.write_text(r.stdout)
                    failures.append((case_id, trap, f"{mode_name}/{policy}/no-trap", r.stdout, wat_path))
                    continue
                if parsed[0] != EXPECTED_TRAPS[trap]:
                    out_path.write_text(r.stdout)
                    failures.append(
                        (
                            case_id,
                            trap,
                            f"{mode_name}/{policy}/wrong-trap expected={EXPECTED_TRAPS[trap]!r} actual={parsed[0]!r}",
                            r.stdout,
                            wat_path,
                        )
                    )
                    continue

                auto_none_osr = policy == "auto" and auto_effective_policy == "none" and has_osr
                if auto_none_osr:
                    if parsed[1]:
                        out_path.write_text(r.stdout)
                        failures.append(
                            (
                                case_id,
                                trap,
                                f"{mode_name}/{policy}/auto-none-emitted-instruction-frames",
                                log + "\n" + r.stdout,
                                wat_path,
                            )
                        )
                        continue
                    if "call_stack=none" not in log or "call_stack_frames=omit" not in log:
                        out_path.write_text(r.stdout)
                        failures.append(
                            (
                                case_id,
                                trap,
                                f"{mode_name}/{policy}/auto-none-log-mismatch",
                                log + "\n" + r.stdout,
                                wat_path,
                            )
                        )
                        continue
                elif not parsed[1]:
                    out_path.write_text(r.stdout)
                    failures.append((case_id, trap, f"{mode_name}/{policy}/empty-stack", log + "\n" + r.stdout, wat_path))
                    continue

                native_policy = policy == "unwind" or (
                    policy == "auto" and auto_effective_policy == "unwind"
                )
                if native_policy and has_osr:
                    strict_source = (
                        "capture_source=seeded-libunwind backend=libunwind resolved_jit_caller=yes" in log
                    )
                    if not strict_source:
                        out_path.write_text(r.stdout)
                        failures.append(
                            (
                                case_id,
                                trap,
                                f"{mode_name}/{policy}/missing-strict-seeded-libunwind-source",
                                log + "\n" + r.stdout,
                                wat_path,
                            )
                        )
                        continue

                results.append((policy, parsed, has_osr, out_path, r.stdout, log))
                log_path.unlink(missing_ok=True)

            if len(results) == len(policies):
                osr_count = sum(1 for _, _, has_osr, _, _, _ in results if has_osr)
                if osr_count == 0:
                    skipped_no_osr += 1
                    skipped_modes.append(mode_name)
                    continue
                if osr_count != len(results):
                    for policy, _, has_osr, _, stdout, log in results:
                        if not has_osr:
                            failures.append((case_id, trap, f"{mode_name}/{policy}/missing-osr-request", log + "\n" + stdout, wat_path))
                    continue

                baseline = None
                baseline_out = None
                baseline_stdout = None
                for policy, parsed, _, out_path, stdout, _ in results:
                    if policy == "instruction":
                        baseline = parsed
                        baseline_out = out_path
                        baseline_stdout = stdout
                    elif policy == "auto" and auto_effective_policy == "none":
                        continue
                    elif baseline is not None and parsed != baseline:
                        if baseline_stdout is not None and baseline_out is not None:
                            baseline_out.write_text(baseline_stdout)
                        out_path.write_text(stdout)
                        failures.append(
                            (
                                case_id,
                                trap,
                                f"{mode_name}/{policy}/mismatch baseline={baseline} actual={parsed} baseline_out={baseline_out} out={out_path}",
                                stdout,
                                wat_path,
                            )
                        )

        if len(failures) == case_failure_count:
            wat_path.unlink(missing_ok=True)
            wasm_path.unlink(missing_ok=True)

        skipped_suffix = f" skipped=no-osr:{','.join(skipped_modes)}" if skipped_modes else ""
        print(f"[osr-fuzz] completed {case_id + 1}/{ns.cases}{skipped_suffix}")

    if failures:
        print(f"[osr-fuzz] FAIL failures={len(failures)} total_runs={total_runs} skipped_no_osr={skipped_no_osr}")
        for case_id, trap, where, output, wat_path in failures[:8]:
            print(f"[osr-fuzz] failure case={case_id} trap={trap} where={where} wat={wat_path}")
            print(output[:1200])
        return 1

    shutil.rmtree(work, ignore_errors=True)
    print(f"[osr-fuzz] PASS cases={ns.cases} total_runs={total_runs} skipped_no_osr={skipped_no_osr}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
