#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"

WORK_DIR="${UWVM_BACKEND_LIBFUZZER_WORK_DIR:-${ROOT_DIR}/build/test/0015.backend_fuzzer/libfuzzer}"
CORPUS_DIR="${UWVM_BACKEND_LIBFUZZER_CORPUS:-${WORK_DIR}/corpus}"
ARTIFACT_DIR="${UWVM_BACKEND_LIBFUZZER_ARTIFACTS:-${WORK_DIR}/artifacts}"
TARGET="${WORK_DIR}/backend_libfuzzer"
CORPUS_EXPLICIT=0
ARTIFACT_EXPLICIT=0
COMBINE="${UWVM_BACKEND_FUZZER_COMBINE:-none}"
DELAY="${UWVM_BACKEND_FUZZER_DELAY:-none}"
MEMORY_MODEL="${UWVM_BACKEND_FUZZER_MEMORY_MODEL:-single-thread-alloc}"
USE_THREAD_LOCAL="${UWVM_BACKEND_FUZZER_USE_THREAD_LOCAL:-0}"
SANITIZERS="${UWVM_BACKEND_LIBFUZZER_SANITIZERS:-fuzzer,address,undefined}"
SYSROOT_ARG="${UWVM_BACKEND_LIBFUZZER_SYSROOT:-${SYSROOT:-${SDKROOT:-}}}"
USE_LLD="${UWVM_BACKEND_LIBFUZZER_USE_LLD:-}"
LIBFUZZER_ARGS=()

usage() {
  cat <<'EOF'
usage: run_backend_libfuzzer.sh [options] [-- libFuzzer-args...]

Options:
  --work-dir <path>       Build/corpus/artifact root.
  --corpus-dir <path>     Corpus directory.
  --artifact-dir <path>   Crash artifact output directory.
  --combine <mode>        uwvm-int combine macros: none|soft|heavy|extra.
  --delay <mode>          uwvm-int delay-local macros: none|soft|heavy.
  --memory-model <mode>   default|mmap|single-thread-alloc|multi-thread-alloc.
  --use-thread-local      Build target with UWVM_USE_THREAD_LOCAL.
  --no-use-thread-local   Build target without UWVM_USE_THREAD_LOCAL.
  --sanitizers <list>     clang -fsanitize list. Default: fuzzer,address,undefined.
  --sysroot <path>        Compiler sysroot. Default: $SYSROOT, then $SDKROOT.
  --use-lld               Pass -fuse-ld=lld to clang++.
  --no-use-lld            Do not pass -fuse-ld=lld.
  -h, --help              Show this help.

Environment:
  UWVM_BACKEND_LIBFUZZER_MODES="uwvm-int-ring-matrix llvm-jit-lazy ..."
  CXX=clang++
  SYSROOT=/path/to/MacOSX.sdk
  LLVM_CONFIG=/path/to/llvm-config

The target runs in a single process by default. Pass libFuzzer job/worker flags
after -- if you really want parallel fuzzing.
EOF
}

while (($#)); do
  case "$1" in
    --work-dir)
      [[ $# -ge 2 ]] || { echo "ERR: --work-dir requires a path" >&2; exit 2; }
      WORK_DIR="$2"
      TARGET="${WORK_DIR}/backend_libfuzzer"
      if [[ "${CORPUS_EXPLICIT}" -eq 0 ]]; then
        CORPUS_DIR="${WORK_DIR}/corpus"
      fi
      if [[ "${ARTIFACT_EXPLICIT}" -eq 0 ]]; then
        ARTIFACT_DIR="${WORK_DIR}/artifacts"
      fi
      shift 2
      ;;
    --corpus-dir)
      [[ $# -ge 2 ]] || { echo "ERR: --corpus-dir requires a path" >&2; exit 2; }
      CORPUS_DIR="$2"
      CORPUS_EXPLICIT=1
      shift 2
      ;;
    --artifact-dir)
      [[ $# -ge 2 ]] || { echo "ERR: --artifact-dir requires a path" >&2; exit 2; }
      ARTIFACT_DIR="$2"
      ARTIFACT_EXPLICIT=1
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
    --sanitizers)
      [[ $# -ge 2 ]] || { echo "ERR: --sanitizers requires a value" >&2; exit 2; }
      SANITIZERS="$2"
      shift 2
      ;;
    --sysroot)
      [[ $# -ge 2 ]] || { echo "ERR: --sysroot requires a path" >&2; exit 2; }
      SYSROOT_ARG="$2"
      shift 2
      ;;
    --use-lld)
      USE_LLD=1
      shift
      ;;
    --no-use-lld)
      USE_LLD=0
      shift
      ;;
    --)
      shift
      LIBFUZZER_ARGS+=("$@")
      break
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      LIBFUZZER_ARGS+=("$1")
      shift
      ;;
  esac
done

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
if [[ -z "${USE_LLD}" ]]; then
  if [[ -n "${SYSROOT_ARG}" ]]; then
    USE_LLD=1
  else
    USE_LLD=0
  fi
fi
case "${USE_LLD}" in
  1|true|TRUE|yes|YES|on|ON) USE_LLD=1 ;;
  0|false|FALSE|no|NO|off|OFF|'') USE_LLD=0 ;;
  *) echo "ERR: unsupported lld toggle: ${USE_LLD}" >&2; exit 2 ;;
esac

resolve_path() {
  local p="$1"
  case "${p}" in
    /*) printf '%s\n' "${p}" ;;
    *) printf '%s\n' "${ROOT_DIR}/${p}" ;;
  esac
}

WORK_DIR="$(resolve_path "${WORK_DIR}")"
CORPUS_DIR="$(resolve_path "${CORPUS_DIR}")"
ARTIFACT_DIR="$(resolve_path "${ARTIFACT_DIR}")"
TARGET="$(resolve_path "${TARGET}")"

mkdir -p -- "${WORK_DIR}" "${CORPUS_DIR}" "${ARTIFACT_DIR}"

if [[ -z "$(find "${CORPUS_DIR}" -mindepth 1 -maxdepth 1 -type f -print -quit 2>/dev/null)" ]]; then
  python3 - "${CORPUS_DIR}" <<'PY'
from pathlib import Path
import os
import sys

corpus = Path(sys.argv[1])
for i in range(8):
    (corpus / f"seed-{i:02d}").write_bytes(os.urandom(32 + i * 7))
PY
fi

BUILD_ARGS=(
  --out "${TARGET}"
  --combine "${COMBINE}"
  --delay "${DELAY}"
  --memory-model "${MEMORY_MODEL}"
  --sanitizers "${SANITIZERS}"
)
if [[ -n "${SYSROOT_ARG}" ]]; then
  BUILD_ARGS+=(--sysroot "${SYSROOT_ARG}")
fi
if [[ "${USE_LLD}" -eq 1 ]]; then
  BUILD_ARGS+=(--use-lld)
else
  BUILD_ARGS+=(--no-use-lld)
fi
if [[ "${USE_THREAD_LOCAL}" -eq 1 ]]; then
  BUILD_ARGS+=(--use-thread-local)
fi

UWVM_BACKEND_FUZZER_USE_THREAD_LOCAL="${USE_THREAD_LOCAL}" \
  python3 "${SCRIPT_DIR}/build_libfuzzer.py" "${BUILD_ARGS[@]}"

CLANG_RUNTIME_DIRS=()
if command -v "${CXX:-c++}" >/dev/null 2>&1; then
  RESOURCE_DIR="$("${CXX:-c++}" -print-resource-dir 2>/dev/null || true)"
  if [[ -n "${RESOURCE_DIR}" && -d "${RESOURCE_DIR}/lib/darwin" ]]; then
    CLANG_RUNTIME_DIRS+=("${RESOURCE_DIR}/lib/darwin")
  fi
fi
for runtime_dir in \
  /opt/homebrew/opt/llvm/lib/clang/*/lib/darwin \
  /opt/homebrew/Cellar/llvm/*/lib/clang/*/lib/darwin \
  /usr/local/opt/llvm/lib/clang/*/lib/darwin \
  /usr/local/Cellar/llvm/*/lib/clang/*/lib/darwin
do
  if [[ -d "${runtime_dir}" ]]; then
    CLANG_RUNTIME_DIRS+=("${runtime_dir}")
  fi
done
if [[ "${#CLANG_RUNTIME_DIRS[@]}" -ne 0 ]]; then
  RUNTIME_DYLD_PATH="$(IFS=:; printf '%s' "${CLANG_RUNTIME_DIRS[*]}")"
  export DYLD_LIBRARY_PATH="${RUNTIME_DYLD_PATH}${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}"
fi

export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:abort_on_error=1}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-abort_on_error=1:print_stacktrace=1}"

echo "INFO: running backend libFuzzer target"
echo "INFO: modes=${UWVM_BACKEND_LIBFUZZER_MODES:-all default modes}"
"${TARGET}" \
  "-artifact_prefix=${ARTIFACT_DIR}/" \
  "${CORPUS_DIR}" \
  "${LIBFUZZER_ARGS[@]}"
