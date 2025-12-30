#!/usr/bin/env python3
from __future__ import annotations

import argparse
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Case:
    name: str
    provider_wasm: str
    provider_module_name: str
    consumer_wasm: str
    consumer_module_name: str
    expect_success: bool


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _case_root() -> Path:
    return Path(__file__).resolve().parent


def _compile_wat(case_root: Path) -> None:
    script = case_root / "compile_wat.py"
    subprocess.run([sys.executable, str(script)], cwd=case_root, check=True)


def _run_uwvm(
    repo_root: Path,
    provider_wasm: Path,
    provider_module_name: str,
    consumer_wasm: Path,
    consumer_module_name: str,
) -> subprocess.CompletedProcess[str]:
    provider_wasm = provider_wasm.resolve()
    consumer_wasm = consumer_wasm.resolve()
    args = [
        "xmake",
        "run",
        "uwvm",
        "--log-verbose",
        "--wasm-preload-library",
        str(provider_wasm),
        provider_module_name,
        "--wasm-set-main-module-name",
        consumer_module_name,
        "--run",
        str(consumer_wasm),
    ]
    return subprocess.run(args, cwd=repo_root, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run uwvm initializer import-type validation cases via xmake.")
    parser.add_argument("--case", default="all", help='Case name, or "all" (default)')
    args = parser.parse_args()

    repo_root = _repo_root()
    case_root = _case_root()
    wat_dir = case_root / "wat"

    _compile_wat(case_root)

    cases = [
        Case(
            name="ok.func",
            provider_wasm=str(wat_dir / "provider_ok.wasm"),
            provider_module_name="provider_ok",
            consumer_wasm=str(wat_dir / "consumer_ok.wasm"),
            consumer_module_name="consumer_ok",
            expect_success=True,
        ),
        Case(
            name="mismatch.func.param_and_result",
            provider_wasm=str(wat_dir / "provider_ok.wasm"),
            provider_module_name="provider_ok",
            consumer_wasm=str(wat_dir / "consumer_bad_func.wasm"),
            consumer_module_name="consumer_bad_func",
            expect_success=False,
        ),
        Case(
            name="mismatch.func.result_only",
            provider_wasm=str(wat_dir / "provider_ok.wasm"),
            provider_module_name="provider_ok",
            consumer_wasm=str(wat_dir / "consumer_bad_func_result.wasm"),
            consumer_module_name="consumer_bad_func_result",
            expect_success=False,
        ),
        Case(
            name="mismatch.func.param_count",
            provider_wasm=str(wat_dir / "provider_ok.wasm"),
            provider_module_name="provider_ok",
            consumer_wasm=str(wat_dir / "consumer_bad_func_param_count.wasm"),
            consumer_module_name="consumer_bad_func_param_count",
            expect_success=False,
        ),
    ]

    selected = cases if args.case == "all" else [c for c in cases if c.name == args.case]
    if not selected:
        sys.stderr.write(f"Unknown --case: {args.case}\n")
        sys.stderr.write("Available:\n")
        for c in cases:
            sys.stderr.write(f"  - {c.name}\n")
        return 2

    failed = 0
    for c in selected:
        proc = _run_uwvm(
            repo_root=repo_root,
            provider_wasm=Path(c.provider_wasm),
            provider_module_name=c.provider_module_name,
            consumer_wasm=Path(c.consumer_wasm),
            consumer_module_name=c.consumer_module_name,
        )

        ok = (proc.returncode == 0)
        if ok != c.expect_success:
            failed += 1
            sys.stderr.write(f"[FAIL] {c.name}: returncode={proc.returncode} expected_success={c.expect_success}\n")
            if proc.stdout:
                sys.stderr.write("---- stdout ----\n")
                sys.stderr.write(proc.stdout)
                if not proc.stdout.endswith("\n"):
                    sys.stderr.write("\n")
            if proc.stderr:
                sys.stderr.write("---- stderr ----\n")
                sys.stderr.write(proc.stderr)
                if not proc.stderr.endswith("\n"):
                    sys.stderr.write("\n")
        else:
            sys.stdout.write(f"[OK] {c.name}\n")

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
