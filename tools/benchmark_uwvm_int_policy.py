#!/usr/bin/env python3
"""Benchmark uwvm-int compile-policy variants against the u2bench corpus."""

from __future__ import annotations

import argparse
import json
import math
import os
import platform
import re
import statistics
import subprocess
import sys
import time
from collections import defaultdict
from pathlib import Path


TIME_PATTERNS = (
    re.compile(r"^Elapsed time:\s*(\d+(?:\.\d+)?)\s*ms\b", re.MULTILINE | re.IGNORECASE),
    re.compile(r"^Elapsed:\s*(\d+(?:\.\d+)?)\s*ms\b", re.MULTILINE | re.IGNORECASE),
    re.compile(r"^Time:\s*(\d+(?:\.\d+)?)\s*ms\b", re.MULTILINE),
    re.compile(r"^time:\s*(\d+(?:\.\d+)?)\s*ms\b", re.MULTILINE | re.IGNORECASE),
)


def parse_variant(spec: str) -> tuple[str, Path]:
    if "=" not in spec:
        raise argparse.ArgumentTypeError("variant must use LABEL=/absolute/path/to/uwvm")
    label, raw_path = spec.split("=", 1)
    label = label.strip()
    path = Path(raw_path).expanduser().resolve()
    if not label or any(ch in label for ch in "#:="):
        raise argparse.ArgumentTypeError(f"invalid variant label: {label!r}")
    if not path.is_file() or not os.access(path, os.X_OK):
        raise argparse.ArgumentTypeError(f"variant is not executable: {path}")
    return label, path


def extract_internal_ms(output: str) -> float | None:
    last: float | None = None
    for pattern in TIME_PATTERNS:
        for match in pattern.finditer(output):
            last = float(match.group(1))
    return last


def geomean(values: list[float]) -> float:
    positive = [value for value in values if value > 0.0 and math.isfinite(value)]
    if not positive:
        return math.nan
    return math.exp(sum(math.log(value) for value in positive) / len(positive))


def percentile(values: list[float], fraction: float) -> float:
    if not values:
        return math.nan
    ordered = sorted(values)
    index = round((len(ordered) - 1) * fraction)
    return ordered[index]


def benchmark_group(relative_path: str) -> str:
    path = Path(relative_path)
    top = path.parts[0] if path.parts else "unknown"
    name = path.name.lower()
    if top != "micro":
        return top
    if name.startswith("local_"):
        return "micro/local"
    if name.startswith("operand_stack_"):
        return "micro/operand-stack"
    if name.startswith("call_"):
        return "micro/call"
    if name.startswith(("control_", "br_", "big_switch")):
        return "micro/control-flow"
    if name.startswith(("mem_", "memory_", "pointer_", "random_access_")):
        return "micro/memory"
    return "micro/other"


def summarize_ratios(ratios: list[float]) -> dict[str, float | int]:
    return {
        "count": len(ratios),
        "geomean": geomean(ratios),
        "median": statistics.median(ratios) if ratios else math.nan,
        "p05": percentile(ratios, 0.05),
        "p95": percentile(ratios, 0.95),
        "minimum": min(ratios, default=math.nan),
        "maximum": max(ratios, default=math.nan),
    }


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True, help="u2bench wasm/corpus directory")
    parser.add_argument("--variant", action="append", type=parse_variant, required=True, help="repeatable LABEL=UWVM_PATH")
    parser.add_argument("--baseline", required=True, help="variant label used as the ratio denominator")
    parser.add_argument("--repetitions", type=int, default=3)
    parser.add_argument("--timeout", type=float, default=30.0)
    parser.add_argument("--include", action="append", default=[], help="repeatable regular expression matched against relative Wasm paths")
    parser.add_argument("--max-wasm", type=int, default=0)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)

    if args.repetitions < 1:
        parser.error("--repetitions must be positive")
    if args.timeout <= 0.0:
        parser.error("--timeout must be positive")

    root = args.root.expanduser().resolve()
    if not root.is_dir():
        parser.error(f"corpus root does not exist: {root}")

    variants: list[tuple[str, Path]] = args.variant
    labels = [label for label, _ in variants]
    if len(labels) != len(set(labels)):
        parser.error("variant labels must be unique")
    if args.baseline not in labels:
        parser.error(f"baseline is not a configured variant: {args.baseline}")

    include_patterns = [re.compile(pattern) for pattern in args.include]
    wasms: list[tuple[Path, str]] = []
    for wasm in sorted(root.rglob("*.wasm")):
        relative = wasm.relative_to(root).as_posix()
        if include_patterns and not any(pattern.search(relative) for pattern in include_patterns):
            continue
        wasms.append((wasm, relative))
    if args.max_wasm > 0:
        wasms = wasms[: args.max_wasm]
    if not wasms:
        parser.error("no Wasm files selected")

    samples: list[dict[str, object]] = []
    total = len(wasms) * len(variants) * args.repetitions
    completed = 0
    started = time.time()

    for wasm_index, (_, relative) in enumerate(wasms):
        print(f"[{wasm_index + 1}/{len(wasms)}] {relative}", flush=True)
        for repetition in range(args.repetitions):
            rotation = (wasm_index + repetition) % len(variants)
            ordered_variants = variants[rotation:] + variants[:rotation]
            for label, executable in ordered_variants:
                command = [str(executable), "-Rint", "-I1dir", ".", ".", "--", relative]
                wall_start = time.perf_counter_ns()
                timed_out = False
                try:
                    completed_process = subprocess.run(
                        command,
                        cwd=root,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE,
                        check=False,
                        timeout=args.timeout,
                    )
                    return_code = completed_process.returncode
                    stdout = completed_process.stdout.decode("utf-8", errors="replace")
                    stderr = completed_process.stderr.decode("utf-8", errors="replace")
                except subprocess.TimeoutExpired as error:
                    timed_out = True
                    return_code = 124
                    stdout = (error.stdout or b"").decode("utf-8", errors="replace")
                    stderr = (error.stderr or b"").decode("utf-8", errors="replace")
                wall_ms = (time.perf_counter_ns() - wall_start) / 1_000_000.0
                internal_ms = extract_internal_ms(stdout + "\n" + stderr)
                execution_ok = return_code == 0
                timing_parsed = internal_ms is not None
                samples.append(
                    {
                        "variant": label,
                        "wasm": relative,
                        "group": benchmark_group(relative),
                        "repetition": repetition,
                        # Keep `ok` for consumers of the v1 report while exposing
                        # execution and timing collection as separate outcomes.
                        "ok": execution_ok and timing_parsed,
                        "execution_ok": execution_ok,
                        "timing_parsed": timing_parsed,
                        "return_code": return_code,
                        "timed_out": timed_out,
                        "internal_ms": internal_ms,
                        "wall_ms": wall_ms,
                        "stdout_tail": stdout[-400:],
                        "stderr_tail": stderr[-400:],
                    }
                )
                completed += 1
                if completed % 100 == 0 or completed == total:
                    elapsed = time.time() - started
                    print(f"  completed={completed}/{total} elapsed={elapsed:.1f}s", flush=True)

    values: dict[str, dict[str, list[float]]] = defaultdict(lambda: defaultdict(list))
    failures: list[dict[str, object]] = []
    execution_failures: list[dict[str, object]] = []
    timing_parse_failures: list[dict[str, object]] = []
    for sample in samples:
        if not sample["ok"]:
            failures.append(sample)
        if not sample["execution_ok"]:
            execution_failures.append(sample)
            continue
        if not sample["timing_parsed"]:
            timing_parse_failures.append(sample)
            continue
        values[str(sample["variant"])][str(sample["wasm"])].append(float(sample["internal_ms"]))

    medians: dict[str, dict[str, float]] = {}
    for label in labels:
        medians[label] = {wasm: statistics.median(times) for wasm, times in values[label].items()}

    baseline_medians = medians[args.baseline]
    summary: dict[str, object] = {}
    for label in labels:
        ratios: list[float] = []
        ratios_by_group: dict[str, list[float]] = defaultdict(list)
        per_wasm: dict[str, float] = {}
        for _, relative in wasms:
            baseline_value = baseline_medians.get(relative)
            variant_value = medians[label].get(relative)
            if baseline_value is None or variant_value is None or baseline_value <= 0.0:
                continue
            ratio = variant_value / baseline_value
            ratios.append(ratio)
            ratios_by_group[benchmark_group(relative)].append(ratio)
            per_wasm[relative] = ratio
        summary[label] = {
            "vs_baseline": summarize_ratios(ratios),
            "by_group": {group: summarize_ratios(group_ratios) for group, group_ratios in sorted(ratios_by_group.items())},
            "per_wasm_ratio": per_wasm,
        }

    result = {
        "schema": "uwvm-int-policy-benchmark-v1",
        "host": {"node": platform.node(), "platform": platform.platform(), "python": platform.python_version()},
        "corpus": str(root),
        "wasm_count": len(wasms),
        "repetitions": args.repetitions,
        "timeout_seconds": args.timeout,
        "baseline": args.baseline,
        "variants": {label: str(path) for label, path in variants},
        "elapsed_seconds": time.time() - started,
        "execution_failures": execution_failures,
        "timing_parse_failures": timing_parse_failures,
        # Compatibility union used by existing v1 report consumers.
        "failures": failures,
        "medians_ms": medians,
        "summary": summary,
        "samples": samples,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print(f"output={args.output}")
    print(f"failures={len(failures)}")
    print(f"execution_failures={len(execution_failures)}")
    print(f"timing_parse_failures={len(timing_parse_failures)}")
    for label in labels:
        aggregate = summary[label]["vs_baseline"]  # type: ignore[index]
        print(
            f"{label}: common={aggregate['count']} geomean={aggregate['geomean']:.6f} "
            f"median={aggregate['median']:.6f} p95={aggregate['p95']:.6f}"
        )
    return 0 if not execution_failures and not timing_parse_failures else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
