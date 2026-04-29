#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/../../.." && pwd)"
cd -- "${ROOT_DIR}"

STRICT_DIR="test/0013.uwvm_int/strict"

if [[ -z "${SYSROOT:-}" ]]; then
  echo "ERR: SYSROOT is empty. Example: export SYSROOT=/path/to/sdk-or-sysroot" >&2
  exit 2
fi

mkdir -p -- "${ROOT_DIR}/build"
LOCK_DIR="${UWVM_STRICT_LOCK_DIR:-${ROOT_DIR}/build/uwvm_int_strict.lock}"
if ! mkdir -- "${LOCK_DIR}" 2>/dev/null; then
  echo "ERR: another uwvm_int strict run appears to be active: ${LOCK_DIR}" >&2
  echo "ERR: remove the lock only after confirming no xmake/uwvm_int test process is still running." >&2
  exit 9
fi
printf '%s\n' "$$" > "${LOCK_DIR}/pid"
cleanup_lock() {
  rm -rf -- "${LOCK_DIR}"
}
trap cleanup_lock EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

if [[ -n "${UWVM_XMAKE_JOBS:-}" ]]; then
  if [[ "${UWVM_XMAKE_JOBS}" =~ ^[1-9][0-9]*$ ]]; then
    echo "INFO: xmake jobs limited via UWVM_XMAKE_JOBS=${UWVM_XMAKE_JOBS}"
  else
    echo "ERR: UWVM_XMAKE_JOBS must be a positive integer, got: ${UWVM_XMAKE_JOBS}" >&2
    exit 2
  fi
fi

xmake_build() {
  if [[ -n "${UWVM_XMAKE_JOBS:-}" ]]; then
    xmake b -v -j "${UWVM_XMAKE_JOBS}" "$@"
  else
    xmake b -v "$@"
  fi
}

if [[ "$(uname -s)" == "Darwin" ]]; then
  TOOLCHAIN_ROOT="$(cd -- "$(dirname -- "${SYSROOT}")" && pwd)"
  CLANG_BIN="${TOOLCHAIN_ROOT}/llvm/bin/clang"
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
  "--sysroot=${SYSROOT}"
  --test-libfuzzer=y
  --enable-test-uwvm-int=y
  --use-cxx-module=n
  --static=n
  --enable-int=uwvm-int
  "--enable-uwvm-int-combine-ops=${UWVM_STRICT_BASIC_COMBINE_MODE:-heavy}"
  "--enable-uwvm-int-delay-local=${UWVM_STRICT_BASIC_DELAY_MODE:-heavy}"
)

export UWVM2TEST_ABI_MODES="${UWVM_STRICT_ABI_MODES:-all}"
export UWVM2TEST_MATRIX_LEVEL="${UWVM_STRICT_MATRIX_LEVEL:-coverage}"

STRICT_TARGETS=()
if [[ "$#" -gt 0 ]]; then
  STRICT_TARGETS=("$@")
else
  while IFS= read -r f; do
    STRICT_TARGETS+=("$(basename -- "${f}" .cc)")
  done < <(find "${STRICT_DIR}" -type f -name '*.cc' | sort)
fi

if [[ "${#STRICT_TARGETS[@]}" -eq 0 ]]; then
  echo "ERR: no strict targets found." >&2
  exit 3
fi

LOG_FILE="${UWVM_STRICT_BASIC_LOG:-${ROOT_DIR}/build/uwvm_int_basic_full.log}"
: > "${LOG_FILE}"

echo "INFO: strict basic ABI modes = ${UWVM2TEST_ABI_MODES}" | tee -a "${LOG_FILE}"
echo "INFO: strict basic matrix level = ${UWVM2TEST_MATRIX_LEVEL}" | tee -a "${LOG_FILE}"
echo "INFO: strict basic target count = ${#STRICT_TARGETS[@]}" | tee -a "${LOG_FILE}"

echo "=== uwvm_int strict basic full: configure ===" | tee -a "${LOG_FILE}"
xmake f -c >> "${LOG_FILE}" 2>&1
xmake f "${COMMON_F_FLAGS[@]}" >> "${LOG_FILE}" 2>&1

for i in "${!STRICT_TARGETS[@]}"; do
  t="${STRICT_TARGETS[$i]}"
  printf "\n=== [%03d/%03d] build %s ===\n" "$((i + 1))" "${#STRICT_TARGETS[@]}" "$t" | tee -a "${LOG_FILE}"
  xmake_build "$t" >> "${LOG_FILE}" 2>&1

  exe="$(xmake show -t "$t" | perl -pe 's/\e\[[0-9;]*m//g' | sed -n 's/^[[:space:]]*targetfile:[[:space:]]*//p' | head -n1 || true)"
  if [[ -z "${exe}" || ! -f "${ROOT_DIR}/${exe}" ]]; then
    echo "ERR: targetfile not found for: ${t}" | tee -a "${LOG_FILE}" >&2
    exit 4
  fi

  printf "=== [%03d/%03d] run %s ===\n" "$((i + 1))" "${#STRICT_TARGETS[@]}" "$t" | tee -a "${LOG_FILE}"
  "${ROOT_DIR}/${exe}" >> "${LOG_FILE}" 2>&1
done

echo "OK: strict basic full completed. Log: ${LOG_FILE}" | tee -a "${LOG_FILE}"
