#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"

WABT_ROOT="${UWVM_BACKEND_FUZZER_WABT_ROOT:-${ROOT_DIR}/build/test/third-parties/wabt}"
WORK_DIR="${UWVM_BACKEND_FUZZER_WORK_DIR:-${ROOT_DIR}/build/test/0015.backend_fuzzer}"
FUZZ_CASES="${UWVM_BACKEND_FUZZ_CASES:-32}"
FUZZ_SEED="${UWVM_BACKEND_FUZZ_SEED:-}"
COMBINE="${UWVM_BACKEND_FUZZER_COMBINE:-none}"
DELAY="${UWVM_BACKEND_FUZZER_DELAY:-none}"
MEMORY_MODEL="${UWVM_BACKEND_FUZZER_MEMORY_MODEL:-default}"
USE_THREAD_LOCAL="${UWVM_BACKEND_FUZZER_USE_THREAD_LOCAL:-0}"
INCLUDE_TRAPS="${UWVM_BACKEND_FUZZER_INCLUDE_TRAPS:-1}"
INCLUDE_STRATEGY="${UWVM_BACKEND_FUZZER_INCLUDE_STRATEGY:-1}"
KEEP_WORK=0
BUILD_DEFINES=()
BUILD_CXXFLAGS=()
BUILD_LDFLAGS=()

WABT_MVP_FLAGS=(
  --disable-mutable-globals
  --disable-saturating-float-to-int
  --disable-sign-extension
  --disable-simd
  --disable-multi-value
  --disable-bulk-memory
  --disable-reference-types
)

DEFAULT_MODES=(
  uwvm-int-ring-matrix
  uwvm-int-lazy
  uwvm-int-full
  llvm-jit-lazy
  llvm-jit-full
  tiered
  tiered-no-t0
  tiered-no-t2
)

usage() {
  cat <<'EOF'
usage: run_backend_fuzzer.sh [options]

Options:
  --wabt-root <path>   WABT checkout/build root. Default: build/test/third-parties/wabt.
  --work-dir <path>    Generated corpus/build/log directory. Default: build/test/0015.backend_fuzzer.
  --fuzz-cases <n>     Random non-trap cases to generate. Default: 32.
  --seed <n>           Random seed accepted by Python int(..., 0). Default: random.
  --combine <mode>     uwvm-int combine macros: none|soft|heavy|extra.
  --delay <mode>       uwvm-int delay-local macros: none|soft|heavy.
  --memory-model <m>   default|mmap|single-thread-alloc|multi-thread-alloc.
  --use-thread-local   Build runner with UWVM_USE_THREAD_LOCAL.
  --no-use-thread-local
                       Build runner without UWVM_USE_THREAD_LOCAL.
  --define <macro>     Extra -D macro for build_runner.py. Repeatable.
  --extra-cxxflag <f>  Extra compiler flag for build_runner.py. Repeatable.
  --extra-ldflag <f>   Extra linker flag for build_runner.py. Repeatable.
  --keep               Keep existing work directory.
  -h, --help           Show this help.

Environment:
  UWVM_BACKEND_FUZZER_MODES="mode1 mode2"
  UWVM_BACKEND_FUZZER_INCLUDE_TRAPS=0|1
  UWVM_BACKEND_FUZZER_INCLUDE_STRATEGY=0|1
  UWVM_BACKEND_FUZZER_MEMORY_MODEL=default|mmap|single-thread-alloc|multi-thread-alloc
  UWVM_BACKEND_FUZZER_USE_THREAD_LOCAL=0|1
  UWVM_BACKEND_FUZZER_EXTRA_DEFINES="UWVM_FOO -DUWVM_BAR=1"
  UWVM_BACKEND_FUZZER_EXTRA_CXXFLAGS="-Wall"
  UWVM_BACKEND_FUZZER_EXTRA_LDFLAGS="-Wl,..."
  LLVM_CONFIG=/path/to/llvm-config
  CXX=/path/to/c++

This runner does not call xmake and does not execute the uwvm CLI. It builds a
direct-storage C++ runner and compares it with WABT.
EOF
}

random_seed() {
  python3 - <<'PY'
import secrets
print(hex(secrets.randbits(64)))
PY
}

while (($#)); do
  case "$1" in
    --wabt-root)
      [[ $# -ge 2 ]] || { echo "ERR: --wabt-root requires a path" >&2; exit 2; }
      WABT_ROOT="$2"
      shift 2
      ;;
    --work-dir)
      [[ $# -ge 2 ]] || { echo "ERR: --work-dir requires a path" >&2; exit 2; }
      WORK_DIR="$2"
      shift 2
      ;;
    --fuzz-cases)
      [[ $# -ge 2 ]] || { echo "ERR: --fuzz-cases requires an integer" >&2; exit 2; }
      FUZZ_CASES="$2"
      shift 2
      ;;
    --seed)
      [[ $# -ge 2 ]] || { echo "ERR: --seed requires a value" >&2; exit 2; }
      FUZZ_SEED="$2"
      shift 2
      ;;
    --combine)
      [[ $# -ge 2 ]] || { echo "ERR: --combine requires a value" >&2; exit 2; }
      COMBINE="$2"
      shift 2
      ;;
    --delay)
      [[ $# -ge 2 ]] || { echo "ERR: --delay requires a value" >&2; exit 2; }
      DELAY="$2"
      shift 2
      ;;
    --memory-model)
      [[ $# -ge 2 ]] || { echo "ERR: --memory-model requires a value" >&2; exit 2; }
      MEMORY_MODEL="$2"
      shift 2
      ;;
    --use-thread-local)
      USE_THREAD_LOCAL=1
      shift
      ;;
    --no-use-thread-local)
      USE_THREAD_LOCAL=0
      shift
      ;;
    --define)
      [[ $# -ge 2 ]] || { echo "ERR: --define requires a macro" >&2; exit 2; }
      BUILD_DEFINES+=("$2")
      shift 2
      ;;
    --extra-cxxflag)
      [[ $# -ge 2 ]] || { echo "ERR: --extra-cxxflag requires a flag" >&2; exit 2; }
      BUILD_CXXFLAGS+=("$2")
      shift 2
      ;;
    --extra-ldflag)
      [[ $# -ge 2 ]] || { echo "ERR: --extra-ldflag requires a flag" >&2; exit 2; }
      BUILD_LDFLAGS+=("$2")
      shift 2
      ;;
    --keep)
      KEEP_WORK=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "ERR: unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

case "${FUZZ_CASES}" in
  ''|*[!0-9]*)
    echo "ERR: --fuzz-cases must be a non-negative integer, got: ${FUZZ_CASES}" >&2
    exit 2
    ;;
esac

case "${COMBINE}" in
  none|soft|heavy|extra) ;;
  *) echo "ERR: unsupported combine mode: ${COMBINE}" >&2; exit 2 ;;
esac

case "${DELAY}" in
  none|soft|heavy) ;;
  *) echo "ERR: unsupported delay mode: ${DELAY}" >&2; exit 2 ;;
esac

case "${MEMORY_MODEL}" in
  default|mmap|single-thread-alloc|multi-thread-alloc) ;;
  *) echo "ERR: unsupported memory model: ${MEMORY_MODEL}" >&2; exit 2 ;;
esac

case "${USE_THREAD_LOCAL}" in
  1|true|TRUE|yes|YES|on|ON) USE_THREAD_LOCAL=1 ;;
  0|false|FALSE|no|NO|off|OFF|'') USE_THREAD_LOCAL=0 ;;
  *) echo "ERR: unsupported thread-local toggle: ${USE_THREAD_LOCAL}" >&2; exit 2 ;;
esac

if [[ -z "${FUZZ_SEED}" ]]; then
  FUZZ_SEED="$(random_seed)"
  echo "INFO: generated backend fuzzer seed = ${FUZZ_SEED}"
else
  echo "INFO: backend fuzzer seed = ${FUZZ_SEED}"
fi

resolve_path() {
  local p="$1"
  case "${p}" in
    /*) printf '%s\n' "${p}" ;;
    *) printf '%s\n' "${ROOT_DIR}/${p}" ;;
  esac
}

require_tool() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "ERR: required tool not found on PATH: $1" >&2
    exit 2
  fi
}

tool_path() {
  local base="$1"
  local p="${WABT_ROOT}/build/${base}"
  if [[ -x "${p}" ]]; then
    printf '%s\n' "${p}"
    return
  fi
  if [[ -x "${p}.exe" ]]; then
    printf '%s\n' "${p}.exe"
    return
  fi
  return 1
}

ensure_wabt_tools() {
  WABT_ROOT="$(resolve_path "${WABT_ROOT}")"
  local wasm_validate
  local wasm_interp
  if wasm_validate="$(tool_path wasm-validate)" && wasm_interp="$(tool_path wasm-interp)"; then
    WASM_VALIDATE="${wasm_validate}"
    WASM_INTERP="${wasm_interp}"
    return
  fi

  require_tool git
  require_tool cmake

  mkdir -p -- "$(dirname -- "${WABT_ROOT}")"
  if [[ ! -d "${WABT_ROOT}/.git" && ! -f "${WABT_ROOT}/CMakeLists.txt" ]]; then
    if [[ -d "${WABT_ROOT}" && -n "$(find "${WABT_ROOT}" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null)" ]]; then
      echo "ERR: WABT root exists but is not a WABT checkout: ${WABT_ROOT}" >&2
      exit 2
    fi
    echo "INFO: cloning WABT into ${WABT_ROOT}"
    git clone --depth 1 --recursive https://github.com/WebAssembly/wabt.git "${WABT_ROOT}"
  elif [[ -d "${WABT_ROOT}/.git" ]]; then
    if [[ "${UWVM_BACKEND_FUZZER_WABT_PULL:-0}" != "0" ]]; then
      echo "INFO: updating WABT checkout in ${WABT_ROOT}"
      git -C "${WABT_ROOT}" pull --ff-only
    fi
    git -C "${WABT_ROOT}" submodule update --init --recursive
  fi

  echo "INFO: building WABT tools"
  cmake -S "${WABT_ROOT}" -B "${WABT_ROOT}/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DBUILD_TOOLS=ON \
    -DBUILD_LIBWASM=OFF \
    -DUSE_INTERNAL_SHA256=ON \
    -DHAVE_OPENSSL_SHA_H=OFF
  cmake --build "${WABT_ROOT}/build" --target wasm-validate wasm-interp --config Release

  WASM_VALIDATE="$(tool_path wasm-validate)"
  WASM_INTERP="$(tool_path wasm-interp)"
}

WORK_DIR="$(resolve_path "${WORK_DIR}")"
CASES_DIR="${WORK_DIR}/cases"
LOG_DIR="${WORK_DIR}/logs"
RUNNER_DIR="${WORK_DIR}/runner"
THREAD_LOCAL_SUFFIX="global"
if [[ "${USE_THREAD_LOCAL}" -eq 1 ]]; then
  THREAD_LOCAL_SUFFIX="thread-local"
fi
RUNNER="${RUNNER_DIR}/backend_fuzzer_runner_combine-${COMBINE}_delay-${DELAY}_memory-${MEMORY_MODEL}_${THREAD_LOCAL_SUFFIX}"

if [[ "${KEEP_WORK}" -eq 0 ]]; then
  rm -rf -- "${CASES_DIR}" "${LOG_DIR}" "${RUNNER_DIR}"
fi
mkdir -p -- "${CASES_DIR}" "${LOG_DIR}" "${RUNNER_DIR}"

ensure_wabt_tools

GEN_TRAP_FLAG="--include-traps"
if [[ "${INCLUDE_TRAPS}" == "0" || "${INCLUDE_TRAPS}" == "false" || "${INCLUDE_TRAPS}" == "no" ]]; then
  GEN_TRAP_FLAG="--no-include-traps"
fi
GEN_STRATEGY_FLAG="--include-strategy"
if [[ "${INCLUDE_STRATEGY}" == "0" || "${INCLUDE_STRATEGY}" == "false" || "${INCLUDE_STRATEGY}" == "no" ]]; then
  GEN_STRATEGY_FLAG="--no-include-strategy"
fi

python3 "${SCRIPT_DIR}/generate_cases.py" \
  --out-dir "${CASES_DIR}" \
  --cases "${FUZZ_CASES}" \
  --seed "${FUZZ_SEED}" \
  "${GEN_TRAP_FLAG}" \
  "${GEN_STRATEGY_FLAG}"

BUILD_ARGS=(
  --generated-dir "${CASES_DIR}/generated" \
  --out "${RUNNER}" \
  --combine "${COMBINE}" \
  --delay "${DELAY}" \
  --memory-model "${MEMORY_MODEL}"
)
if [[ "${USE_THREAD_LOCAL}" -eq 1 ]]; then
  BUILD_ARGS+=(--use-thread-local)
fi
for define in "${BUILD_DEFINES[@]}"; do
  BUILD_ARGS+=(--define "${define}")
done
for flag in "${BUILD_CXXFLAGS[@]}"; do
  BUILD_ARGS+=(--extra-cxxflag "${flag}")
done
for flag in "${BUILD_LDFLAGS[@]}"; do
  BUILD_ARGS+=(--extra-ldflag "${flag}")
done

UWVM_BACKEND_FUZZER_USE_THREAD_LOCAL="${USE_THREAD_LOCAL}" \
  python3 "${SCRIPT_DIR}/build_runner.py" "${BUILD_ARGS[@]}"

if [[ -n "${UWVM_BACKEND_FUZZER_MODES:-}" ]]; then
  read -r -a MODES <<< "${UWVM_BACKEND_FUZZER_MODES//,/ }"
else
  MODES=("${DEFAULT_MODES[@]}")
fi

MODES_JOINED="${MODES[*]}"
export WASM_VALIDATE WASM_INTERP RUNNER CASES_DIR LOG_DIR MODES_JOINED
export WABT_FLAGS_JOINED="${WABT_MVP_FLAGS[*]}"

python3 - <<'PY'
from __future__ import annotations

import json
import os
import shlex
import subprocess
import sys
from pathlib import Path


wasm_validate = os.environ["WASM_VALIDATE"]
wasm_interp = os.environ["WASM_INTERP"]
runner = os.environ["RUNNER"]
cases_dir = Path(os.environ["CASES_DIR"])
log_dir = Path(os.environ["LOG_DIR"])
modes = os.environ["MODES_JOINED"].split()
wabt_flags = shlex.split(os.environ["WABT_FLAGS_JOINED"])

manifest = json.loads((cases_dir / "manifest.json").read_text(encoding="utf-8"))
if not modes:
    raise SystemExit("ERR: no runner modes selected")


def run_logged(argv: list[str], log_path: Path) -> int:
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with log_path.open("wb") as f:
        proc = subprocess.run(argv, stdout=f, stderr=subprocess.STDOUT)
    return proc.returncode


def classify(returncode: int) -> str:
    return "ok" if returncode == 0 else "trap"


def classify_wabt(returncode: int, log_path: Path) -> str:
    if returncode != 0:
        return "trap"
    text = log_path.read_text(encoding="utf-8", errors="replace")
    return "trap" if "error:" in text else "ok"


for item in manifest:
    idx = int(item["index"])
    name = str(item["name"])
    wasm = Path(item["wasm"])
    if not wasm.is_absolute():
        wasm = (cases_dir / wasm).resolve()

    validate_log = log_dir / f"{idx:04d}.validate.log"
    v_rc = run_logged([wasm_validate, *wabt_flags, str(wasm)], validate_log)
    if v_rc != 0:
        print(f"ERR: WABT validation failed for case {idx} {name}; log={validate_log}", file=sys.stderr)
        raise SystemExit(1)

    oracle_log = log_dir / f"{idx:04d}.wabt.log"
    oracle_rc = run_logged([wasm_interp, *wabt_flags, "--run-all-exports", str(wasm)], oracle_log)
    oracle = classify_wabt(oracle_rc, oracle_log)

    for mode in modes:
        mode_log = log_dir / f"{idx:04d}.{mode}.log"
        got_rc = run_logged([runner, "--case", str(idx), "--mode", mode], mode_log)
        got = classify(got_rc)
        if got != oracle:
            print(
                f"ERR: mismatch case={idx} name={name} mode={mode} oracle={oracle} runner={got}\n"
                f"     wasm={wasm}\n"
                f"     oracle_log={oracle_log}\n"
                f"     runner_log={mode_log}",
                file=sys.stderr,
            )
            raise SystemExit(1)

    print(f"OK case={idx:04d} name={name} oracle={oracle}")

print(f"OK: compared {len(manifest)} cases across {len(modes)} modes")
PY

echo "OK: backend fuzzer completed (combine=${COMBINE}, delay=${DELAY}, memory=${MEMORY_MODEL}, thread-local=${USE_THREAD_LOCAL})"
