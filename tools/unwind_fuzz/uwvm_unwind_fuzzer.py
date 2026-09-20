#!/usr/bin/env python3
import argparse
from dataclasses import dataclass
import os
import random
import re
import signal
import shlex
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


MODES = (
    ("full", ["-Rcm", "full", "-Rcc", "jit"]),
    ("lazy", ["-Rcm", "lazy", "-Rcc", "jit"]),
    ("lazy_verification", ["-Rcm", "lazy+verification", "-Rcc", "jit"]),
    ("tiered", ["-Rtiered"]),
    ("tiered_no_t0", ["-Rtiered", "-Rtiered-disable-t0"]),
    ("tiered_no_t2", ["-Rtiered", "-Rtiered-disable-t2"]),
    ("tiered_no_t0_no_t2", ["-Rtiered", "-Rtiered-disable-t0", "-Rtiered-disable-t2"]),
)


DEFAULT_POLICIES = ("instruction", "auto")

STRICT_NATIVE_MODES = {
    "full",
    "lazy",
    "lazy_verification",
    "tiered_no_t0",
    "tiered_no_t0_no_t2",
}


def strip_ansi(s: str) -> str:
    return ANSI_RE.sub("", s)


def parse_output(s: str):
    plain = strip_ansi(s)
    trap_match = TRAP_RE.search(plain)
    trap = trap_match.group(1) if trap_match else ""
    funcs = [int(m.group(1)) for m in FUNC_RE.finditer(plain)]
    return trap, funcs


def random_padding(rng: random.Random, min_n: int = 0, max_n: int = 16) -> str:
    return "".join("    nop\n" for _ in range(rng.randint(min_n, max_n)))


def chain_functions(depth: int, rng: random.Random, leaf_name: str = "$leaf") -> str:
    lines = []
    previous = leaf_name
    for i in range(depth):
        name = f"$f{i}"
        style = rng.choice(("plain", "padded", "block", "if"))
        if style == "plain":
            lines.append(f"  (func {name} (type $v) call {previous})")
        elif style == "padded":
            lines.append(
                f"  (func {name} (type $v)\n"
                f"{random_padding(rng, 1, 8)}"
                f"    i32.const {rng.randint(1, 100)}\n"
                f"    i32.const {rng.randint(1, 100)}\n"
                "    i32.add\n"
                "    drop\n"
                f"    call {previous})"
            )
        elif style == "block":
            lines.append(
                f"  (func {name} (type $v)\n"
                "    block\n"
                f"{random_padding(rng, 0, 6)}"
                f"      call {previous}\n"
                "    end)"
            )
        else:
            lines.append(
                f"  (func {name} (type $v) (local $x i32)\n"
                f"    i32.const {rng.choice([0, 1])}\n"
                "    local.set $x\n"
                "    local.get $x\n"
                "    if\n"
                f"      call {previous}\n"
                "    else\n"
                f"      call {previous}\n"
                "    end)"
            )
        previous = name
    lines.append(f'  (func $_start (export "_start") (type $v) call {previous})')
    return "\n".join(lines)


def leaf_body(trap: str, rng: random.Random) -> tuple[str, str]:
    pad = random_padding(rng)
    if trap == "unreachable":
        return "", f"{pad}    unreachable"
    if trap == "i32_div_s_zero":
        return "", f"{pad}    i32.const {rng.randint(-1000, 1000)}\n    i32.const 0\n    i32.div_s\n    drop"
    if trap == "i32_div_u_zero":
        return "", f"{pad}    i32.const {rng.randint(0, 1000)}\n    i32.const 0\n    i32.div_u\n    drop"
    if trap == "i32_overflow":
        return "", f"{pad}    i32.const -2147483648\n    i32.const -1\n    i32.div_s\n    drop"
    if trap == "i64_div_s_zero":
        return "", f"{pad}    i64.const {rng.randint(-1000, 1000)}\n    i64.const 0\n    i64.div_s\n    drop"
    if trap == "i64_div_u_zero":
        return "", f"{pad}    i64.const {rng.randint(0, 1000)}\n    i64.const 0\n    i64.div_u\n    drop"
    if trap == "i64_overflow":
        return "", f"{pad}    i64.const -9223372036854775808\n    i64.const -1\n    i64.div_s\n    drop"
    if trap == "invalid_i32_trunc_f32_s":
        return "", f"{pad}    f32.const nan\n    i32.trunc_f32_s\n    drop"
    if trap == "invalid_i32_trunc_f32_u":
        return "", f"{pad}    f32.const nan\n    i32.trunc_f32_u\n    drop"
    if trap == "invalid_i32_trunc_f64_s":
        return "", f"{pad}    f64.const nan\n    i32.trunc_f64_s\n    drop"
    if trap == "invalid_i32_trunc_f64_u":
        return "", f"{pad}    f64.const nan\n    i32.trunc_f64_u\n    drop"
    if trap == "invalid_i64_trunc_f32_s":
        return "", f"{pad}    f32.const nan\n    i64.trunc_f32_s\n    drop"
    if trap == "invalid_i64_trunc_f32_u":
        return "", f"{pad}    f32.const nan\n    i64.trunc_f32_u\n    drop"
    if trap == "invalid_i64_trunc_f64_s":
        return "", f"{pad}    f64.const nan\n    i64.trunc_f64_s\n    drop"
    if trap == "invalid_i64_trunc_f64_u":
        return "", f"{pad}    f64.const nan\n    i64.trunc_f64_u\n    drop"
    if trap == "oob_load":
        return "  (memory 1)\n", f"{pad}    i32.const {rng.choice([65536, 131072, 2147483647])}\n    i32.load\n    drop"
    if trap == "oob_store":
        return "  (memory 1)\n", f"{pad}    i32.const {rng.choice([65536, 131072, 2147483647])}\n    i64.const {rng.randint(1, 100)}\n    i64.store"
    if trap == "call_indirect_type":
        prefix = (
            "  (type $i (func (param i32)))\n"
            "  (table 1 funcref)\n"
            "  (elem (i32.const 0) $target)\n"
            "  (func $target (type $i) (param $x i32)\n"
            "    local.get $x\n"
            "    drop)\n"
        )
        return prefix, f"{pad}    i32.const 0\n    call_indirect (type $v)"
    if trap == "call_indirect_null":
        return "  (table 1 funcref)\n", f"{pad}    i32.const 0\n    call_indirect (type $v)"
    if trap == "call_indirect_oob":
        return "  (table 1 funcref)\n", f"{pad}    i32.const {rng.choice([1, 2, 100])}\n    call_indirect (type $v)"
    raise AssertionError(trap)


def random_wasm2_content(trap: str, rng: random.Random) -> tuple[str, str]:
    memory_traps = {
        "oob_load",
        "oob_store",
    }
    table_traps = {
        "call_indirect_type",
        "call_indirect_null",
        "call_indirect_oob",
    }

    features = ["bulk_memory", "multivalue", "ref"]
    if trap not in table_traps:
        features.append("table")
    selected = set(rng.sample(features, rng.randint(1, len(features))))
    prefix = []
    body = []

    if "bulk_memory" in selected:
        if trap not in memory_traps:
            prefix.append("  (memory 1)\n")
        prefix.append("  (data $fuzz_data \"\\01\\02\\03\\04\\05\\06\\07\\08\")\n")
        bulk_op = rng.choice(("init", "copy", "fill"))
        if bulk_op == "init":
            offset = rng.randint(0, 32)
            length = rng.randint(0, 8)
            body.append(
                f"    i32.const {offset}\n"
                "    i32.const 0\n"
                f"    i32.const {length}\n"
                "    memory.init $fuzz_data\n"
            )
        elif bulk_op == "copy":
            source = rng.randint(0, 32)
            destination = rng.randint(64, 96)
            length = rng.randint(0, 16)
            body.append(
                f"    i32.const {destination}\n"
                f"    i32.const {source}\n"
                f"    i32.const {length}\n"
                "    memory.copy\n"
            )
        else:
            destination = rng.randint(0, 96)
            length = rng.randint(0, 16)
            body.append(
                f"    i32.const {destination}\n"
                f"    i32.const {rng.randint(0, 255)}\n"
                f"    i32.const {length}\n"
                "    memory.fill\n"
            )

    if "multivalue" in selected:
        prefix.append(
            "  (type $fuzz_pair_t (func (param i32 i64) (result i32 i64)))\n"
            "  (func $fuzz_pair (type $fuzz_pair_t) (param $i i32) (param $l i64) (result i32 i64)\n"
            "    local.get $i\n"
            "    local.get $l)\n"
        )
        body.append(
            f"    i32.const {rng.randint(-1000, 1000)}\n"
            f"    i64.const {rng.randint(-1000, 1000)}\n"
            "    call $fuzz_pair\n"
            "    drop\n"
            "    drop\n"
        )

    if "ref" in selected:
        body.append(
            "    ref.null func\n"
            "    ref.is_null\n"
            "    drop\n"
            "    ref.null extern\n"
            "    ref.is_null\n"
            "    drop\n"
        )

    if "table" in selected:
        prefix.append(
            "  (table 2 funcref)\n"
            "  (func $fuzz_table_target (type $v))\n"
            "  (elem $fuzz_elem func $fuzz_table_target)\n"
        )
        table_op = rng.choice(("init", "fill", "set_get"))
        if table_op == "init":
            body.append(
                "    i32.const 0\n"
                "    i32.const 0\n"
                "    i32.const 1\n"
                "    table.init $fuzz_elem\n"
            )
        elif table_op == "fill":
            body.append(
                "    i32.const 0\n"
                "    ref.null func\n"
                "    i32.const 2\n"
                "    table.fill\n"
            )
        else:
            index = rng.randint(0, 1)
            body.append(
                f"    i32.const {index}\n"
                "    ref.null func\n"
                "    table.set 0\n"
                f"    i32.const {index}\n"
                "    table.get 0\n"
                "    ref.is_null\n"
                "    drop\n"
            )

    return "".join(prefix), "".join(body)


def make_wat(trap: str, depth: int, rng: random.Random) -> str:
    prefix, body = leaf_body(trap, rng)
    fuzz_prefix, fuzz_body = random_wasm2_content(trap, rng)
    return (
        "(module\n"
        "  (type $v (func))\n"
        f"{prefix}"
        f"{fuzz_prefix}"
        "  (func $leaf (type $v)\n"
        f"{fuzz_body}"
        f"{body})\n"
        f"{chain_functions(depth, rng)}\n"
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


def detect_call_stack_policies(uwvm_command: list[str], cwd: Path, timeout: int, max_output_bytes: int):
    result = run_cmd([*uwvm_command, "--help", "runtime"], cwd, timeout, max_output_bytes)
    if result.returncode != 0:
        raise RuntimeError(f"uwvm runtime help probe failed ({result.returncode}):\n{result.stdout}")
    help_text = strip_ansi(result.stdout)
    if re.search(r"runtime-llvm-jit-call-stack[\s\S]{0,500}\bunwind\b", help_text):
        return ("instruction", "unwind", "auto")
    return DEFAULT_POLICIES


def detect_wasm2_feature_args(uwvm_command: list[str], cwd: Path, timeout: int, max_output_bytes: int) -> list[str]:
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
    log = log_path.read_text(errors="replace") if log_path.exists() else ""
    match = re.findall(r"\bcall_stack=(unwind|none|instruction)\b", log)
    effective = match[-1] if match else "unknown"
    if result.timed_out or result.returncode == 0 or trap != "memory access out of bounds":
        raise RuntimeError(f"auto live probe did not produce the expected OOB trap:\n{log}\n{result.stdout}")
    if effective == "instruction":
        raise RuntimeError(f"auto made a forbidden conversion to Instruction frames:\n{log}")
    if effective == "unwind":
        strict_source = (
            "capture_source=seeded-libunwind backend=libunwind resolved_jit_caller=yes" in log
        )
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
        description="Compare LLVM-JIT trap stacks across runtime modes and call-stack policies."
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
    ap.add_argument(
        "--cases",
        type=int,
        default=512,
        help="number of randomized trap-stack cases",
    )
    ap.add_argument("--seed", type=lambda value: int(value, 0), default=0xC0DEF00D)
    ap.add_argument("--timeout", type=int, default=120, help="per-process timeout in seconds")
    ap.add_argument("--max-output-mib", type=int, default=4, help="maximum retained output per subprocess")
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
    work = base / f"uwvm_unwind_fuzz_{os.getpid()}"
    work.mkdir(parents=True, exist_ok=True)
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
    print(f"[fuzz] work={work}")
    print(f"[fuzz] seed={ns.seed} cases={ns.cases}")
    print(f"[fuzz] uwvm-command={shlex.join(uwvm_command)}")
    print(f"[fuzz] wasm2-feature-args={shlex.join(wasm2_feature_args)}")
    print(f"[fuzz] call-stack-policies={','.join(policies)}")
    print(f"[fuzz] auto-effective-policy={auto_effective_policy}")

    failures = []
    total_runs = 0
    for case_id in range(ns.cases):
        trap = rng.choice(TRAPS)
        depth = rng.randint(1, 12)
        wat = make_wat(trap, depth, rng)
        wat_path = work / f"case_{case_id:04d}_{trap}_d{depth}.wat"
        wasm_path = work / f"case_{case_id:04d}_{trap}_d{depth}.wasm"
        wat_path.write_text(wat)

        r = run_cmd([str(ns.wat2wasm), str(wat_path), "-o", str(wasm_path)], ns.root, ns.timeout, max_output_bytes)
        if r.timed_out or r.returncode != 0:
            failures.append((case_id, trap, "wat2wasm", r.stdout, wat_path))
            continue

        case_failure_count = len(failures)
        for mode_name, mode_args in MODES:
            baseline = None
            baseline_out = None
            baseline_stdout = None
            for policy in policies:
                out_path = work / f"case_{case_id:04d}_{trap}_d{depth}.{mode_name}.{policy}.out"
                log_path = work / f"case_{case_id:04d}_{trap}_d{depth}.{mode_name}.{policy}.log"
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
                if r.timed_out:
                    out_path.write_text(r.stdout)
                    failures.append((case_id, trap, f"{mode_name}/{policy}/timeout", r.stdout, wat_path))
                    total_runs += 1
                    continue
                total_runs += 1
                parsed = parse_output(r.stdout)
                llvm_trap = mode_name in STRICT_NATIVE_MODES
                auto_none_jit = policy == "auto" and auto_effective_policy == "none" and llvm_trap
                if r.returncode == 0 or not parsed[0] or (not parsed[1] and not auto_none_jit):
                    out_path.write_text(r.stdout)
                    failures.append((case_id, trap, f"{mode_name}/{policy}/no-trap-or-empty-stack", r.stdout, wat_path))
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
                log = log_path.read_text(errors="replace") if log_path.exists() else ""
                native_policy = policy == "unwind" or (policy == "auto" and auto_effective_policy == "unwind")
                if native_policy and llvm_trap:
                    if "capture_source=seeded-libunwind backend=libunwind resolved_jit_caller=yes" not in log:
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
                if auto_none_jit and parsed[1]:
                    out_path.write_text(r.stdout)
                    failures.append(
                        (case_id, trap, f"{mode_name}/{policy}/auto-none-emitted-instruction-frames", log + "\n" + r.stdout, wat_path)
                    )
                    continue
                if auto_none_jit and mode_name == "full" and (
                    "call_stack=none" not in log or "call_stack_frames=omit" not in log
                ):
                    out_path.write_text(r.stdout)
                    failures.append((case_id, trap, f"{mode_name}/{policy}/auto-none-log-mismatch", log + "\n" + r.stdout, wat_path))
                    continue
                if policy == "instruction":
                    baseline = parsed
                    baseline_out = out_path
                    baseline_stdout = r.stdout
                elif not auto_none_jit and baseline is not None and parsed != baseline:
                    if baseline_stdout is not None and baseline_out is not None:
                        baseline_out.write_text(baseline_stdout)
                    out_path.write_text(r.stdout)
                    failures.append(
                        (
                            case_id,
                            trap,
                            f"{mode_name}/{policy}/mismatch baseline={baseline} actual={parsed} baseline_out={baseline_out} out={out_path}",
                            r.stdout,
                            wat_path,
                        )
                    )
                else:
                    log_path.unlink(missing_ok=True)

        if len(failures) == case_failure_count:
            wat_path.unlink(missing_ok=True)
            wasm_path.unlink(missing_ok=True)

        if (case_id + 1) % 8 == 0:
            print(f"[fuzz] completed {case_id + 1}/{ns.cases} cases")

    if failures:
        print(f"[fuzz] FAIL failures={len(failures)} total_runs={total_runs}")
        for case_id, trap, where, output, wat_path in failures[:10]:
            print(f"[fuzz] failure case={case_id} trap={trap} where={where} wat={wat_path}")
            print(output[:1200])
        return 1

    shutil.rmtree(work, ignore_errors=True)
    print(f"[fuzz] PASS cases={ns.cases} total_runs={total_runs}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
