#!/usr/bin/env python3
"""Validate or execute Wasmtime WAT fixtures through uwvm-int-full and LLVM-full."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any


ANSI_RE = re.compile(r"\x1b\[[0-?]*[ -/]*[@-~]")
PRODUCT_PROFILE = "full"
HASH_CHUNK_SIZE = 1024 * 1024
ENTRY_RE = re.compile(r'\((?:start\b|export\s+"(?:_start|main)")')
UWVM_TRAP_RE = re.compile(r"Runtime crash \(([^)]+)\)", re.IGNORECASE)
UWVM_WASI_POINTER_ALIGNMENT_RE = re.compile(
    r"^\s*uwvm:\s*\[fatal\]\s*wasi preview 1 guest pointer alignment trap:"
    r"[^\r\n]*required alignment\s*=\s*(?:2|4|8)\s*bytes\s*$",
    re.IGNORECASE | re.MULTILINE,
)
WASMTIME_WASI_POINTER_ALIGNMENT_RE = re.compile(
    r"^\s*(?:\d+:\s*)?pointer not aligned to (?:2|4|8):\s*"
    r"region\s*\{\s*start:\s*\d+,\s*len:\s*\d+\s*\}\s*$",
    re.IGNORECASE | re.MULTILINE,
)

MVP_VALIDATION_FLAGS = (
    "--disable-mutable-globals",
    "--disable-saturating-float-to-int",
    "--disable-sign-extension",
    "--disable-simd",
    "--disable-multi-value",
    "--disable-bulk-memory",
    "--disable-reference-types",
)

# WABT enables the proposals integrated by WebAssembly 2.0 core by default.
# Experimental tracks such as threads, GC, memory64, multi-memory, tail calls,
# and relaxed SIMD still require an explicit --enable-* (or --enable-all).
VALIDATION_PROFILES: dict[str, tuple[str, ...]] = {
    "mvp": MVP_VALIDATION_FLAGS,
    "wasm2-core": (),
    "all": ("--enable-all",),
}

# Keep the runtime grammar aligned with the WABT profile used to select each
# case. The Wasm 2.0 profile is materially different from Wasm 1.1 for encoded
# immediates such as call_indirect's table index, even when both profiles enable
# the same proposal families.
UWVM_FEATURE_ARGS: dict[str, tuple[str, ...]] = {
    "mvp": ("--wasm-feature-mvp",),
    "wasm2-core": ("--wasm-feature-wasm2",),
    "all": ("--wasm-feature-wasm2",),
}
ALLOWED_UWVM_FEATURE_ARGS = ("--wasm-feature-mvp", "--wasm-feature-wasm2")

SKIP_STATUS_COUNTERS: dict[str, str] = {
    "wat-compile-skipped": "wat_compile_skipped",
    "profile-validation-skipped": "profile_validation_skipped",
    "preceding-profile-skipped": "preceding_profile_skipped",
    "no-binary-entry-skipped": "no_binary_entry_skipped",
}
ALLOWLISTABLE_SKIP_STATUSES = (
    "profile-validation-skipped",
    "preceding-profile-skipped",
    "no-binary-entry-skipped",
)
EXECUTED_STATUSES = (
    "passed",
    "backend-mismatch",
    "reference-mismatch",
    "validation-rejected",
    "operational-failure",
)

# In the Full product, -Rint is an auto policy which may select lazy
# translation for a large module.  A test labelled uwvm-int-full must use the
# independent mode/compiler selectors so the exercised backend is unambiguous.
UWVM_INT_FULL_ARGS = ("-Rcm", "full", "-Rcc", "int")


@dataclass
class ProcessResult:
    argv: list[str]
    returncode: int
    timed_out: bool
    elapsed_ms: float
    stdout: str
    stderr: str
    stdout_bytes_base64: str
    stdout_bytes_sha256: str
    stdout_bytes_size: int
    outcome: str


def file_fingerprint(path: Path) -> dict[str, object]:
    """Return a reproducible fingerprint while reading large binaries in bounded chunks."""

    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        while chunk := stream.read(HASH_CHUNK_SIZE):
            digest.update(chunk)
            size += len(chunk)
    return {
        "path": str(path),
        "sha256": digest.hexdigest(),
        "size": size,
    }


def strip_ansi(text: str) -> str:
    return ANSI_RE.sub("", text)


def canonical_trap(text: str) -> str | None:
    stripped = strip_ansi(text)
    plain = stripped.lower()

    uwvm_match = UWVM_TRAP_RE.search(stripped)
    if uwvm_match:
        plain = uwvm_match.group(1).lower()

    patterns = (
        ("unreachable", ("unreachable",)),
        ("memory-oob", ("out of bounds memory", "memory out of bounds", "out-of-bounds memory")),
        ("divide-by-zero", ("divide by zero", "division by zero", "divided by zero")),
        ("integer-overflow", ("integer overflow",)),
        ("invalid-conversion", ("invalid conversion", "invalid float-to-int")),
        ("indirect-null", ("uninitialized element", "null function", "uninitialized table")),
        ("indirect-oob", ("out of bounds table", "undefined element", "table index is out of bounds")),
        ("indirect-type", ("indirect call type mismatch", "function signature mismatch")),
        ("stack-overflow", ("call stack exhausted", "stack overflow")),
    )
    for name, needles in patterns:
        if any(needle in plain for needle in needles):
            return name
    return None


def canonical_wasi_pointer_alignment_trap(stderr: str) -> str | None:
    """Recognize only the two stable Preview 1 host-ABI diagnostics on stderr."""

    stripped = strip_ansi(stderr)
    # Keep this narrower than a generic "pointer not aligned" match.  Ordinary
    # unaligned core-Wasm loads/stores remain legal, and guest stdout must never
    # be able to manufacture this cross-runtime equivalence class.
    if UWVM_WASI_POINTER_ALIGNMENT_RE.search(stripped):
        return "wasi-pointer-alignment"
    if "error while executing at wasm backtrace" in stripped.lower() and WASMTIME_WASI_POINTER_ALIGNMENT_RE.search(stripped):
        return "wasi-pointer-alignment"
    return None


def classify(returncode: int, timed_out: bool, stdout: str, stderr: str) -> str:
    if timed_out:
        return "timeout"
    if returncode == 0:
        return "ok"
    wasi_pointer_trap = canonical_wasi_pointer_alignment_trap(stderr)
    if wasi_pointer_trap:
        return f"trap:{wasi_pointer_trap}"
    trap = canonical_trap(stdout + "\n" + stderr)
    if trap:
        return f"trap:{trap}"
    if 0 < returncode < 128:
        return f"exit:{returncode}"
    if returncode < 0:
        return f"signal:{-returncode}"
    return f"error:{returncode}"


def decode_captured_output(data: bytes) -> str:
    """Decode diagnostics without collapsing distinct invalid UTF-8 byte strings."""

    return data.decode("utf-8", errors="backslashreplace")


def captured_output_bytes(value: bytes | str | None) -> bytes:
    """Normalize subprocess output, including the rare text-valued timeout payload."""

    if value is None:
        return b""
    if isinstance(value, bytes):
        return value
    return value.encode("utf-8", errors="surrogateescape")


def run_process(argv: list[str], cwd: Path, timeout: float) -> ProcessResult:
    started = time.perf_counter_ns()
    timed_out = False
    try:
        completed = subprocess.run(
            argv,
            cwd=cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
            timeout=timeout,
        )
        returncode = completed.returncode
        stdout_bytes = captured_output_bytes(completed.stdout)
        stderr_bytes = captured_output_bytes(completed.stderr)
    except subprocess.TimeoutExpired as error:
        timed_out = True
        returncode = 124
        stdout_bytes = captured_output_bytes(error.stdout)
        stderr_bytes = captured_output_bytes(error.stderr)
    stdout = decode_captured_output(stdout_bytes)
    stderr = decode_captured_output(stderr_bytes)
    elapsed_ms = (time.perf_counter_ns() - started) / 1_000_000.0
    return ProcessResult(
        argv=argv,
        returncode=returncode,
        timed_out=timed_out,
        elapsed_ms=elapsed_ms,
        stdout=stdout,
        stderr=stderr,
        stdout_bytes_base64=base64.b64encode(stdout_bytes).decode("ascii"),
        stdout_bytes_sha256=hashlib.sha256(stdout_bytes).hexdigest(),
        stdout_bytes_size=len(stdout_bytes),
        outcome=classify(returncode, timed_out, stdout, stderr),
    )


def executable_path(raw: str) -> Path:
    path = Path(raw).expanduser().resolve()
    if not path.is_file() or not os.access(path, os.X_OK):
        raise argparse.ArgumentTypeError(f"not an executable: {path}")
    return path


def same_result(lhs: ProcessResult, rhs: ProcessResult) -> bool:
    return lhs.outcome == rhs.outcome and lhs.stdout_bytes_base64 == rhs.stdout_bytes_base64


def is_operational_failure(result: ProcessResult) -> bool:
    """Return true for harness/engine failures that cannot be test oracles."""

    return result.timed_out or result.outcome.startswith(("signal:", "error:"))


def binary_entry_state(objdump_text: str) -> str:
    """Return callable, confirmed-no-entry, or unparseable for WABT details output."""

    has_header = re.search(r"(?m)^.+:\s+file format wasm(?:\s+0x[0-9A-Fa-f]+)?\s*$", objdump_text)
    if not has_header or "Section Details:" not in objdump_text:
        return "unparseable"

    if re.search(r"(?m)^Start:\s*$", objdump_text):
        return "callable"

    void_signatures = set(re.findall(r"- type\[(\d+)\] \(\) -> nil", objdump_text))
    function_signatures = dict(re.findall(r"- func\[(\d+)\] sig=(\d+)", objdump_text))
    exported_entries = re.findall(r'- func\[(\d+)\].*-> "(?:_start|main)"$', objdump_text, re.MULTILINE)
    if any(function_signatures.get(function_index) in void_signatures for function_index in exported_entries):
        return "callable"
    return "confirmed-no-entry"


def has_binary_entry(objdump_text: str) -> bool:
    """Compatibility wrapper for callers that only need the callable state."""

    return binary_entry_state(objdump_text) == "callable"


def artifact_stem(relative: str) -> str:
    readable = re.sub(r"[^A-Za-z0-9_.-]+", "_", relative).strip("_")[-120:]
    digest = hashlib.sha256(relative.encode("utf-8")).hexdigest()[:12]
    return f"{digest}-{readable}"


def inherited_cpu_affinity() -> list[int] | None:
    if not hasattr(os, "sched_getaffinity"):
        return None
    return sorted(os.sched_getaffinity(0))


def probe_command(argv: list[str], cwd: Path, timeout: float) -> dict[str, object]:
    """Run a non-fatal provenance probe and preserve failures in the report."""

    try:
        result = run_process(argv, cwd, timeout)
    except OSError as error:
        return {
            "status": "unavailable",
            "text": None,
            "error": f"{type(error).__name__}: {error}",
            "result": {
                "argv": argv,
                "returncode": None,
                "timed_out": False,
                "elapsed_ms": 0.0,
                "stdout": "",
                "stderr": "",
                "outcome": "spawn-error",
            },
        }

    combined = strip_ansi(result.stdout + "\n" + result.stderr).strip()
    status = "ok" if result.outcome == "ok" else ("timeout" if result.timed_out else "failed")
    return {
        "status": status,
        "text": combined or None,
        "error": None,
        "result": asdict(result),
    }


def corpus_git_provenance(source_root: Path, timeout: float) -> dict[str, object]:
    revision_probe = probe_command(["git", "-C", str(source_root), "rev-parse", "--verify", "HEAD"], source_root, timeout)
    revision: str | None = None
    result = revision_probe["result"]
    if revision_probe["status"] == "ok" and isinstance(result, dict):
        candidate = str(result.get("stdout", "")).strip()
        if re.fullmatch(r"(?:[0-9a-fA-F]{40}|[0-9a-fA-F]{64})", candidate):
            revision = candidate.lower()
        else:
            revision_probe["status"] = "failed"
            revision_probe["error"] = "git rev-parse returned a non-object-id value"

    status_probe = probe_command(
        [
            "git",
            "-C",
            str(source_root),
            "status",
            "--porcelain=v2",
            "--untracked-files=all",
            "--ignore-submodules=none",
            "--",
            ".",
        ],
        source_root,
        timeout,
    )
    dirty: bool | None = None
    tracked_changes: bool | None = None
    untracked_changes: bool | None = None
    submodule_changes: bool | None = None
    status_entries: list[str] | None = None
    status_result = status_probe["result"]
    if status_probe["status"] == "ok" and isinstance(status_result, dict):
        status_entries = [line.rstrip() for line in str(status_result.get("stdout", "")).splitlines() if line]
        dirty = bool(status_entries)
        tracked_changes = False
        untracked_changes = False
        submodule_changes = False
        for line in status_entries:
            if line.startswith(("1 ", "2 ", "u ")):
                tracked_changes = True
                fields = line.split(maxsplit=3)
                if len(fields) >= 3 and fields[2].startswith("S"):
                    submodule_changes = True
            elif line.startswith("? "):
                untracked_changes = True

    return {
        "revision": revision,
        "dirty": dirty,
        # These are intentionally non-exclusive: a dirty gitlink is a tracked
        # change, while submodule_changes records the more specific cause.
        "tracked_changes": tracked_changes,
        "untracked_changes": untracked_changes,
        "submodule_changes": submodule_changes,
        "status": status_entries,
        "revision_probe": revision_probe,
        "status_probe": status_probe,
    }


def collect_provenance(
    source_root: Path,
    uwvm_int: Path,
    uwvm_llvm: Path,
    wasmtime: Path,
    wat2wasm: Path,
    wasm_validate: Path,
    wasm_objdump: Path,
    timeout: float,
) -> dict[str, object]:
    probe_timeout = min(timeout, 5.0)
    fingerprint_cache: dict[Path, dict[str, object]] = {}

    def fingerprint(path: Path) -> dict[str, object]:
        resolved = path.resolve()
        cached = fingerprint_cache.get(resolved)
        if cached is None:
            cached = file_fingerprint(resolved)
            fingerprint_cache[resolved] = cached
        return dict(cached)

    def version(executable: Path) -> dict[str, object]:
        record = fingerprint(executable)
        record["version"] = probe_command([str(executable), "--version"], source_root, probe_timeout)
        return record

    return {
        "product_profile": PRODUCT_PROFILE,
        "driver": fingerprint(Path(__file__)),
        "corpus": {"path": str(source_root), "git": corpus_git_provenance(source_root, probe_timeout)},
        "tools": {
            "uwvm_int": version(uwvm_int),
            "uwvm_llvm": version(uwvm_llvm),
            "wasmtime": version(wasmtime),
            "wat2wasm": version(wat2wasm),
            "wasm_validate": version(wasm_validate),
            "wasm_objdump": version(wasm_objdump),
        },
    }


def classify_case(
    case: dict[str, object],
    classification: str,
    status: str,
    *,
    skip_detail: str | None = None,
    skip_allowlist_eligible: bool = False,
) -> None:
    """Record whether a selected case reached backend execution."""

    case["classification"] = classification
    case["status"] = status
    if classification == "skipped":
        case["skip_detail"] = skip_detail
        case["skip_allowlist_eligible"] = skip_allowlist_eligible


def release_gate_summary(
    cases: list[dict[str, object]],
    *,
    strict: bool,
    minimum_executed: int,
    allowed_skip_statuses: tuple[str, ...],
    minimum_executed_explicit: bool = True,
    expected_selected: int | None = None,
    allow_reference_mismatch: bool = False,
) -> dict[str, Any]:
    """Summarize release coverage without conflating skips with successful execution."""

    allowed = set(allowed_skip_statuses)
    executed_by_status = {status: 0 for status in EXECUTED_STATUSES}
    skipped_by_status = {status: 0 for status in SKIP_STATUS_COUNTERS}
    expected_by_status = {status: 0 for status in SKIP_STATUS_COUNTERS}
    unexpected_by_status = {status: 0 for status in SKIP_STATUS_COUNTERS}
    unexpected_cases: list[dict[str, str]] = []

    for case in cases:
        classification = case.get("classification")
        status = str(case.get("status", ""))
        if classification == "executed":
            if status not in executed_by_status:
                raise ValueError(f"unknown executed case status: {status!r}")
            executed_by_status[status] += 1
            continue
        if classification != "skipped" or status not in skipped_by_status:
            raise ValueError(f"case has no recognized execution classification: {case!r}")

        skipped_by_status[status] += 1
        expected = status in allowed and case.get("skip_allowlist_eligible") is True
        case["skip_expected"] = expected
        if expected:
            expected_by_status[status] += 1
        else:
            unexpected_by_status[status] += 1
            unexpected_cases.append(
                {
                    "relative_path": str(case.get("relative_path", "")),
                    "status": status,
                    "detail": str(case.get("skip_detail", "")),
                }
            )

    executed = sum(executed_by_status.values())
    skipped = sum(skipped_by_status.values())
    unexpected_skipped = sum(unexpected_by_status.values())
    violations: list[dict[str, object]] = []
    if strict and not minimum_executed_explicit:
        violations.append({"kind": "minimum-executed-not-explicit"})
    if executed < minimum_executed:
        violations.append(
            {
                "kind": "minimum-executed-not-met",
                "actual": executed,
                "minimum": minimum_executed,
            }
        )
    if expected_selected is not None and len(cases) != expected_selected:
        violations.append(
            {
                "kind": "expected-selected-mismatch",
                "actual": len(cases),
                "expected": expected_selected,
            }
        )
    if unexpected_skipped:
        violations.append(
            {
                "kind": "unexpected-skips",
                "count": unexpected_skipped,
                "by_status": {status: count for status, count in unexpected_by_status.items() if count},
            }
        )
    result_failures = {
        "backend-mismatch": executed_by_status["backend-mismatch"],
        "validation-rejected": executed_by_status["validation-rejected"],
        "operational-failure": executed_by_status["operational-failure"],
        "reference-mismatch": (
            0 if allow_reference_mismatch and not strict else executed_by_status["reference-mismatch"]
        ),
    }
    if any(result_failures.values()):
        violations.append(
            {
                "kind": "result-failures",
                "count": sum(result_failures.values()),
                "by_status": {status: count for status, count in result_failures.items() if count},
            }
        )

    return {
        "policy": "strict" if strict else "compatibility",
        "strict_enforcement": strict,
        "qualified": not violations,
        "minimum_executed": minimum_executed,
        "minimum_executed_explicit": minimum_executed_explicit,
        "minimum_executed_met": executed >= minimum_executed,
        "expected_selected": expected_selected,
        "expected_selected_met": expected_selected is None or len(cases) == expected_selected,
        "allowed_skip_statuses": list(allowed_skip_statuses),
        "allow_reference_mismatch_requested": allow_reference_mismatch,
        "allow_reference_mismatch_effective": allow_reference_mismatch and not strict,
        "selected": len(cases),
        "accounted_for": executed + skipped,
        "executed": {
            "total": executed,
            "by_status": executed_by_status,
        },
        "skipped": {
            "total": skipped,
            "by_status": skipped_by_status,
            "expected_total": sum(expected_by_status.values()),
            "expected_by_status": expected_by_status,
            "unexpected_total": unexpected_skipped,
            "unexpected_by_status": unexpected_by_status,
        },
        "unexpected_skip_cases": unexpected_cases,
        "violations": violations,
    }


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--uwvm", type=executable_path, help="combined-backend executable used for both uwvm modes")
    parser.add_argument("--uwvm-int", type=executable_path, help="pure or combined uwvm-int executable")
    parser.add_argument("--uwvm-llvm", type=executable_path, help="pure or combined LLVM-full executable")
    parser.add_argument("--wasmtime", type=executable_path, required=True)
    parser.add_argument("--wat2wasm", type=executable_path, required=True)
    parser.add_argument("--wasm-validate", type=executable_path, required=True)
    parser.add_argument("--wasm-objdump", type=executable_path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    parser.add_argument("--timeout", type=float, default=20.0)
    parser.add_argument(
        "--mode",
        choices=("execute", "validate"),
        default="execute",
        help="execute entry-point fixtures, or validate every profile-compatible WAT without running it",
    )
    parser.add_argument(
        "--uwvm-feature-arg",
        action="append",
        choices=ALLOWED_UWVM_FEATURE_ARGS,
        default=[],
        help=(
            "override the profile-derived uwvm feature switch with one known product switch "
            "(use --uwvm-feature-arg=--wasm-feature-wasm2 because the value begins with '-')"
        ),
    )
    parser.add_argument(
        "--feature-profile",
        choices=tuple(VALIDATION_PROFILES),
        default="mvp",
        help="WABT validation profile used to select runnable modules",
    )
    parser.add_argument(
        "--only-profile-delta",
        action="store_true",
        help="exclude modules already valid under the preceding profile (MVP for wasm2-core, wasm2-core for all)",
    )
    parser.add_argument("--include", action="append", default=[])
    parser.add_argument("--exclude", action="append", default=[])
    parser.add_argument("--max-cases", type=int, default=0)
    parser.add_argument(
        "--expected-selected",
        type=int,
        help="exact selected-case count expected after --max-cases; a mismatch disqualifies the release report",
    )
    parser.add_argument("--allow-reference-mismatch", action="store_true")
    parser.add_argument(
        "--strict",
        action="store_true",
        help=(
            "enforce the release gate: require the minimum executed count and reject every selected-case skip "
            "unless its exact status is explicitly allowed"
        ),
    )
    parser.add_argument(
        "--minimum-executed",
        "--min-executed",
        dest="minimum_executed",
        type=int,
        help="reviewed backend-executed floor; required explicitly by --strict",
    )
    parser.add_argument(
        "--allow-skip",
        action="append",
        choices=ALLOWLISTABLE_SKIP_STATUSES,
        default=[],
        help=(
            "classify one deterministic selection skip status as expected; repeat as needed. "
            "Tool failures and timeouts remain unexpected even when their surrounding status is allowed"
        ),
    )
    args = parser.parse_args(argv)

    if args.timeout <= 0.0:
        parser.error("--timeout must be positive")
    if args.max_cases < 0:
        parser.error("--max-cases must not be negative")
    if args.expected_selected is not None and args.expected_selected < 1:
        parser.error("--expected-selected must be at least 1")
    if args.minimum_executed is not None and args.minimum_executed < 1:
        parser.error("--minimum-executed must be at least 1")
    if args.strict and args.minimum_executed is None:
        parser.error("--strict requires an explicit --minimum-executed reviewed floor")
    if args.strict and args.allow_reference_mismatch:
        parser.error("--strict and --allow-reference-mismatch are mutually exclusive")
    if len(args.uwvm_feature_arg) > 1:
        parser.error("--uwvm-feature-arg may be specified at most once")
    if args.only_profile_delta and args.feature_profile == "mvp":
        parser.error("--only-profile-delta requires wasm2-core or all")
    minimum_executed = args.minimum_executed if args.minimum_executed is not None else 1

    uwvm_int = args.uwvm_int or args.uwvm
    uwvm_llvm = args.uwvm_llvm or args.uwvm
    if uwvm_int is None or uwvm_llvm is None:
        parser.error("provide --uwvm, or provide both --uwvm-int and --uwvm-llvm")

    source_root = args.source_root.expanduser().resolve()
    if not source_root.is_dir():
        parser.error(f"source root does not exist: {source_root}")
    work_dir = args.work_dir.expanduser().resolve()
    artifacts_dir = work_dir / "artifacts"
    artifacts_dir.mkdir(parents=True, exist_ok=True)

    include = [re.compile(value) for value in args.include]
    exclude = [re.compile(value) for value in args.exclude]
    selected: list[tuple[Path, str]] = []
    selection_counts: dict[str, Any] = {
        "wat_total": 0,
        "no_entry": 0,
        "include_filtered": 0,
        "excluded": 0,
        "include_patterns": list(args.include),
        "exclude_patterns": list(args.exclude),
        "max_cases": args.max_cases,
        "expected_selected": args.expected_selected,
    }
    for path in sorted(source_root.rglob("*.wat")):
        selection_counts["wat_total"] += 1
        relative = path.relative_to(source_root).as_posix()
        if include and not any(pattern.search(relative) for pattern in include):
            selection_counts["include_filtered"] += 1
            continue
        if any(pattern.search(relative) for pattern in exclude):
            selection_counts["excluded"] += 1
            continue
        if args.mode == "execute":
            text = path.read_text(encoding="utf-8", errors="replace")
            if not ENTRY_RE.search(text):
                selection_counts["no_entry"] += 1
                continue
        selected.append((path, relative))
    selection_counts["selected_before_limit"] = len(selected)
    if args.max_cases:
        selected = selected[: args.max_cases]
    selection_counts["selected_after_limit"] = len(selected)
    selection_counts["limit_truncated"] = int(selection_counts["selected_before_limit"]) - len(selected)
    if not selected:
        parser.error("no executable WAT fixtures selected")

    started = time.time()
    provenance = collect_provenance(
        source_root,
        uwvm_int,
        uwvm_llvm,
        args.wasmtime,
        args.wat2wasm,
        args.wasm_validate,
        args.wasm_objdump,
        args.timeout,
    )

    cases: list[dict[str, object]] = []
    counters = {
        "selected": len(selected),
        "wat_compile_skipped": 0,
        "profile_validation_skipped": 0,
        "preceding_profile_skipped": 0,
        "no_binary_entry_skipped": 0,
        "passed": 0,
        "backend_mismatch": 0,
        "reference_mismatch": 0,
        "validation_rejected": 0,
        "operational_failure": 0,
    }
    for index, (wat_path, relative) in enumerate(selected, start=1):
        stem = artifact_stem(relative)
        case_dir = artifacts_dir / stem
        case_dir.mkdir(parents=True, exist_ok=True)
        wasm_path = case_dir / "input.wasm"
        # A previous run must never satisfy this run's compilation contract.
        # Publish only a newly created, regular, non-empty file from a unique
        # same-filesystem directory so os.replace remains atomic.
        wasm_path.unlink(missing_ok=True)
        case: dict[str, object] = {
            "relative_path": relative,
            "wat_path": str(wat_path),
            "wasm_path": str(wasm_path),
            "wat": file_fingerprint(wat_path),
        }
        compile_skip_detail: str | None = None
        with tempfile.TemporaryDirectory(prefix=".wat2wasm-", dir=case_dir) as temporary:
            temporary_wasm_path = Path(temporary) / "input.wasm"
            compile_result = run_process(
                [str(args.wat2wasm), "--enable-all", str(wat_path), "-o", str(temporary_wasm_path)],
                source_root,
                args.timeout,
            )
            case["wat2wasm"] = asdict(compile_result)
            if compile_result.outcome != "ok":
                compile_skip_detail = f"wat2wasm-{compile_result.outcome}"
            elif not temporary_wasm_path.exists():
                compile_skip_detail = "wat2wasm-ok-without-output"
            elif temporary_wasm_path.is_symlink() or not temporary_wasm_path.is_file():
                compile_skip_detail = "wat2wasm-non-regular-output"
            elif temporary_wasm_path.stat().st_size == 0:
                compile_skip_detail = "wat2wasm-empty-output"
            else:
                os.replace(temporary_wasm_path, wasm_path)

        if compile_skip_detail is not None:
            counters["wat_compile_skipped"] += 1
            classify_case(
                case,
                "skipped",
                "wat-compile-skipped",
                skip_detail=compile_skip_detail,
            )
            cases.append(case)
            continue
        case["wasm"] = file_fingerprint(wasm_path)

        profile_flags = VALIDATION_PROFILES[args.feature_profile]
        validate_result = run_process(
            [str(args.wasm_validate), *profile_flags, str(wasm_path)],
            source_root,
            args.timeout,
        )
        case["profile_validation"] = {
            "profile": args.feature_profile,
            "flags": list(profile_flags),
            "result": asdict(validate_result),
        }
        if validate_result.outcome != "ok":
            counters["profile_validation_skipped"] += 1
            is_profile_filter = validate_result.outcome == "exit:1" and args.feature_profile != "all"
            if validate_result.outcome == "exit:1" and args.feature_profile == "all":
                skip_detail = "all-profile-validation-rejected"
            elif is_profile_filter:
                skip_detail = "profile-incompatible"
            else:
                skip_detail = f"validator-{validate_result.outcome}"
            classify_case(
                case,
                "skipped",
                "profile-validation-skipped",
                skip_detail=skip_detail,
                skip_allowlist_eligible=is_profile_filter,
            )
            cases.append(case)
            continue

        if args.only_profile_delta:
            preceding_profile = "mvp" if args.feature_profile == "wasm2-core" else "wasm2-core"
            preceding_flags = VALIDATION_PROFILES[preceding_profile]
            preceding_result = run_process(
                [str(args.wasm_validate), *preceding_flags, str(wasm_path)],
                source_root,
                args.timeout,
            )
            case["preceding_profile_validation"] = {
                "profile": preceding_profile,
                "flags": list(preceding_flags),
                "result": asdict(preceding_result),
            }
            if preceding_result.outcome != "exit:1":
                counters["preceding_profile_skipped"] += 1
                preceding_profile_match = preceding_result.outcome == "ok"
                classify_case(
                    case,
                    "skipped",
                    "preceding-profile-skipped",
                    skip_detail=(
                        "valid-under-preceding-profile"
                        if preceding_profile_match
                        else f"preceding-validator-{preceding_result.outcome}"
                    ),
                    skip_allowlist_eligible=preceding_profile_match,
                )
                cases.append(case)
                continue

        uwvm_feature_args = tuple(args.uwvm_feature_arg) or UWVM_FEATURE_ARGS[args.feature_profile]
        if args.mode == "validate":
            engines = {
                "uwvm_int_validation": [
                    str(uwvm_int),
                    *uwvm_feature_args,
                    "--mode",
                    "validation",
                    "--run",
                    str(wasm_path),
                ],
                "uwvm_llvm_validation": [
                    str(uwvm_llvm),
                    *uwvm_feature_args,
                    "--mode",
                    "validation",
                    "--run",
                    str(wasm_path),
                ],
            }
            results = {name: run_process(command, wat_path.parent, args.timeout) for name, command in engines.items()}
            case["engines"] = {name: asdict(result) for name, result in results.items()}
            int_result = results["uwvm_int_validation"]
            llvm_result = results["uwvm_llvm_validation"]
            if any(is_operational_failure(result) for result in results.values()):
                status = "operational-failure"
                counters["operational_failure"] += 1
            elif not same_result(int_result, llvm_result):
                status = "backend-mismatch"
                counters["backend_mismatch"] += 1
            elif int_result.outcome != "ok":
                status = "validation-rejected"
                counters["validation_rejected"] += 1
            else:
                status = "passed"
                counters["passed"] += 1
            classify_case(case, "executed", status)
            cases.append(case)
            print(
                f"[{index}/{len(selected)}] {status} {relative} "
                f"int={int_result.outcome} llvm={llvm_result.outcome}",
                flush=True,
            )
            continue

        objdump_result = run_process([str(args.wasm_objdump), "-x", str(wasm_path)], source_root, args.timeout)
        case["wasm_objdump"] = asdict(objdump_result)
        objdump_text = objdump_result.stdout + "\n" + objdump_result.stderr
        entry_state = "unparseable" if objdump_result.outcome != "ok" else binary_entry_state(objdump_text)
        case["binary_entry_state"] = entry_state
        if entry_state != "callable":
            counters["no_binary_entry_skipped"] += 1
            confirmed_no_entry = entry_state == "confirmed-no-entry"
            classify_case(
                case,
                "skipped",
                "no-binary-entry-skipped",
                skip_detail=(
                    "confirmed-no-callable-entry"
                    if confirmed_no_entry
                    else (
                        "wasm-objdump-unparseable"
                        if objdump_result.outcome == "ok"
                        else f"wasm-objdump-{objdump_result.outcome}"
                    )
                ),
                skip_allowlist_eligible=confirmed_no_entry,
            )
            cases.append(case)
            continue

        engines = {
            "wasmtime": [str(args.wasmtime), str(wasm_path)],
            "uwvm_int_full": [str(uwvm_int), *UWVM_INT_FULL_ARGS, *uwvm_feature_args, "--run", str(wasm_path)],
            "uwvm_llvm_full": [
                str(uwvm_llvm),
                "-Raot",
                "-Rllvm-cache-path",
                "disable",
                *uwvm_feature_args,
                "--run",
                str(wasm_path),
            ],
        }
        results = {name: run_process(command, wat_path.parent, args.timeout) for name, command in engines.items()}
        case["engines"] = {name: asdict(result) for name, result in results.items()}
        int_result = results["uwvm_int_full"]
        llvm_result = results["uwvm_llvm_full"]
        wasmtime_result = results["wasmtime"]

        if any(is_operational_failure(result) for result in results.values()):
            status = "operational-failure"
            counters["operational_failure"] += 1
        elif not same_result(int_result, llvm_result):
            status = "backend-mismatch"
            counters["backend_mismatch"] += 1
        elif not same_result(int_result, wasmtime_result):
            status = "reference-mismatch"
            counters["reference_mismatch"] += 1
        else:
            status = "passed"
            counters["passed"] += 1
        classify_case(case, "executed", status)
        cases.append(case)

        print(
            f"[{index}/{len(selected)}] {status} {relative} "
            f"wasmtime={wasmtime_result.outcome} int={int_result.outcome} llvm={llvm_result.outcome}",
            flush=True,
        )

    allowed_skip_statuses = tuple(dict.fromkeys(args.allow_skip))
    release_gate = release_gate_summary(
        cases,
        strict=args.strict,
        minimum_executed=minimum_executed,
        allowed_skip_statuses=allowed_skip_statuses,
        minimum_executed_explicit=args.minimum_executed is not None,
        expected_selected=args.expected_selected,
        allow_reference_mismatch=args.allow_reference_mismatch,
    )
    counters["executed"] = release_gate["executed"]["total"]
    counters["skipped"] = release_gate["skipped"]["total"]
    counters["expected_skipped"] = release_gate["skipped"]["expected_total"]
    counters["unexpected_skipped"] = release_gate["skipped"]["unexpected_total"]

    report = {
        "schema": "uwvm2-wasmtime-full-backend-matrix-v6",
        "product_profile": PRODUCT_PROFILE,
        "cpu_affinity": inherited_cpu_affinity(),
        "provenance": provenance,
        "source_root": str(source_root),
        "uwvm_int": str(uwvm_int),
        "uwvm_llvm": str(uwvm_llvm),
        "mode": args.mode,
        "feature_profile": args.feature_profile,
        "uwvm_feature_args": list(tuple(args.uwvm_feature_arg) or UWVM_FEATURE_ARGS[args.feature_profile]),
        "only_profile_delta": args.only_profile_delta,
        "strict": args.strict,
        "selection": selection_counts,
        "counters": counters,
        "release_gate": release_gate,
        "elapsed_seconds": time.time() - started,
        "cases": cases,
    }
    report_path = work_dir / "report.json"
    report_path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"report={report_path}")
    print(json.dumps(counters, sort_keys=True))
    console_gate = {
        "policy": release_gate["policy"],
        "qualified": release_gate["qualified"],
        "minimum_executed": release_gate["minimum_executed"],
        "minimum_executed_explicit": release_gate["minimum_executed_explicit"],
        "selected": release_gate["selected"],
        "expected_selected": release_gate["expected_selected"],
        "executed": release_gate["executed"]["total"],
        "skipped": release_gate["skipped"]["total"],
        "unexpected_skipped": release_gate["skipped"]["unexpected_total"],
        "violations": release_gate["violations"],
    }
    print(f"release_gate={json.dumps(console_gate, sort_keys=True)}")

    if counters["backend_mismatch"] or counters["validation_rejected"] or counters["operational_failure"]:
        return 1
    if counters["reference_mismatch"] and not args.allow_reference_mismatch:
        return 1
    if args.strict and not release_gate["qualified"]:
        print("ERROR: strict release gate failed; inspect release_gate.violations in report.json", file=sys.stderr)
        return 1
    if not args.strict and not release_gate["qualified"]:
        print(
            "WARNING: compatibility mode did not enforce the failed release gate; "
            "rerun with --strict before using this report as release evidence",
            file=sys.stderr,
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
