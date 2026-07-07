#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/../../.." && pwd)"
cd -- "${ROOT_DIR}"

mkdir -p -- "${ROOT_DIR}/build"
LOCK_DIR="${UWVM_LAZY_LOCK_DIR:-${ROOT_DIR}/build/uwvm_int_lazy.lock}"
if ! mkdir -- "${LOCK_DIR}" 2>/dev/null; then
  echo "ERR: another uwvm_int lazy run appears to be active: ${LOCK_DIR}" >&2
  exit 9
fi
printf '%s\n' "$$" > "${LOCK_DIR}/pid"
cleanup_lock() { rm -rf -- "${LOCK_DIR}"; }
trap cleanup_lock EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

if [[ -n "${UWVM_XMAKE_JOBS:-}" ]]; then
  if [[ ! "${UWVM_XMAKE_JOBS}" =~ ^[1-9][0-9]*$ ]]; then
    echo "ERR: UWVM_XMAKE_JOBS must be a positive integer, got: ${UWVM_XMAKE_JOBS}" >&2
    exit 2
  fi
  echo "INFO: xmake jobs limited via UWVM_XMAKE_JOBS=${UWVM_XMAKE_JOBS}"
fi

xmake_build() {
  if [[ -n "${UWVM_XMAKE_JOBS:-}" ]]; then
    xmake b -v -j "${UWVM_XMAKE_JOBS}" "$@"
  else
    xmake b -v "$@"
  fi
}

if [[ "$(uname -s)" == "Darwin" ]]; then
  CLANG_BIN=""
  if [[ -n "${SYSROOT:-}" ]]; then
    TOOLCHAIN_ROOT="$(cd -- "$(dirname -- "${SYSROOT}")" && pwd)"
    CLANG_BIN="${TOOLCHAIN_ROOT}/llvm/bin/clang"
  fi
  if [[ ! -x "${CLANG_BIN}" ]]; then
    CLANG_BIN="$(command -v clang || true)"
  fi
  if [[ -x "${CLANG_BIN}" ]]; then
    CLANG_RUNTIME_DIR="$("${CLANG_BIN}" --print-runtime-dir 2>/dev/null || true)"
    if [[ -n "${CLANG_RUNTIME_DIR}" ]]; then
      export DYLD_LIBRARY_PATH="${CLANG_RUNTIME_DIR}${DYLD_LIBRARY_PATH:+:${DYLD_LIBRARY_PATH}}"
    fi
  fi
fi

COMMON_F_FLAGS=(
  -m debug
  --use-llvm-compiler=y
  --ccache=n
  --cxflags=-Wno-error
  --test-libfuzzer=y
  --enable-test-uwvm-int=y
  --use-cxx-module=n
  --static=none
  --execution-int=uwvm-int
)

if [[ -n "${SYSROOT:-}" ]]; then
  COMMON_F_FLAGS+=("--sysroot=${SYSROOT}")
fi

TARGETS=()
if [[ "$#" -gt 0 ]]; then
  TARGETS=("$@")
else
  TARGETS=(
    uwvm_int_lazy_split
    uwvm_int_lazy_wasm1p1_alignment
    uwvm_int_lazy_scheduler
    uwvm_int_lazy_runtime
  )
fi

echo "INFO: lazy target count = ${#TARGETS[@]}"
echo "=== uwvm_int lazy: configure ==="
xmake f -c
xmake f "${COMMON_F_FLAGS[@]}"

for i in "${!TARGETS[@]}"; do
  t="${TARGETS[$i]}"
  printf '=== [%03d/%03d] build %s ===\n' "$((i + 1))" "${#TARGETS[@]}" "$t"
  xmake_build "$t"
  exe="$(xmake show -t "$t" | perl -pe 's/\e\[[0-9;]*m//g' | sed -n 's/^[[:space:]]*targetfile:[[:space:]]*//p' | head -n1 || true)"
  if [[ -z "${exe}" || ! -f "${ROOT_DIR}/${exe}" ]]; then
    echo "ERR: targetfile not found for: ${t}" >&2
    exit 4
  fi
  printf '=== [%03d/%03d] run %s ===\n' "$((i + 1))" "${#TARGETS[@]}" "$t"
  "${ROOT_DIR}/${exe}"
done

echo "OK: ${UWVM_LAZY_RUN_LABEL:-lazy basic} run completed"
