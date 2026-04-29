#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/../../.." && pwd)"
cd -- "${ROOT_DIR}"

STRICT_DIR="test/0013.uwvm_int/strict"

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

if [[ -z "${SYSROOT:-}" ]]; then
  echo "ERR: SYSROOT is empty. Example: export SYSROOT=/path/to/sdk-or-sysroot" >&2
  exit 2
fi

# Optional: limit xmake parallel jobs (useful on memory-limited machines, e.g. macOS).
# Example: export UWVM_XMAKE_JOBS=4
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

# macOS: when using a custom clang toolchain + sanitizer policies, dyld may not find
# `libclang_rt.{asan,lsan,ubsan}_osx_dynamic.dylib` unless we point it at clang's runtime dir.
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
  --static=none
  --execution-int=uwvm-int
)

SAN_POLICIES_FLAGS=(
  # Note: `-fsanitize=leak` (LSan) is not supported on macOS (Darwin).
  # Keep ASan+UBSan for strict tests there.
  --policies=build.sanitizer.address,build.sanitizer.undefined
)

if [[ "$(uname -s)" != "Darwin" ]]; then
  SAN_POLICIES_FLAGS=(
    --policies=build.sanitizer.address,build.sanitizer.leak,build.sanitizer.undefined
  )
fi

STRICT_TARGETS=()
VALIDATE_TARGETS=()

if [[ "$#" -gt 0 ]]; then
  for t in "$@"; do
    if [[ "${t}" == uwvm_int_validate_* ]]; then
      VALIDATE_TARGETS+=("${t}")
    else
      STRICT_TARGETS+=("${t}")
    fi
  done
else
  while IFS= read -r f; do
    b="$(basename -- "${f}" .cc)"
    if [[ "${f}" == *"/validate/"* || "${b}" == uwvm_int_validate_* ]]; then
      VALIDATE_TARGETS+=("${b}")
    else
      STRICT_TARGETS+=("${b}")
    fi
  done < <(find "${STRICT_DIR}" -type f -name '*.cc' | sort)
fi

if [[ "${#STRICT_TARGETS[@]}" -eq 0 && "${#VALIDATE_TARGETS[@]}" -eq 0 ]]; then
  echo "ERR: no strict/validate targets found." >&2
  exit 3
fi

COMBINE_MODES=(none soft heavy extra)
if [[ -n "${UWVM_STRICT_COMBINE_MODES:-}" ]]; then
  read -r -a COMBINE_MODES <<< "${UWVM_STRICT_COMBINE_MODES//,/ }"
fi
if [[ "${#COMBINE_MODES[@]}" -eq 0 ]]; then
  echo "ERR: combine mode list is empty." >&2
  exit 4
fi

DELAY_MODES=(none soft heavy)
if [[ -n "${UWVM_STRICT_DELAY_MODES:-}" ]]; then
  read -r -a DELAY_MODES <<< "${UWVM_STRICT_DELAY_MODES//,/ }"
fi
if [[ "${#DELAY_MODES[@]}" -eq 0 ]]; then
  echo "ERR: delay mode list is empty." >&2
  exit 4
fi

export UWVM2TEST_ABI_MODES="${UWVM_STRICT_ABI_MODES:-byref,tail-min,tail-sysv,tail-aapcs64}"
export UWVM2TEST_MATRIX_LEVEL="${UWVM_STRICT_MATRIX_LEVEL:-default}"

echo "INFO: strict ABI modes = ${UWVM2TEST_ABI_MODES}"
echo "INFO: strict matrix level = ${UWVM2TEST_MATRIX_LEVEL}"
echo "INFO: strict combine modes = ${COMBINE_MODES[*]}"
echo "INFO: strict delay modes = ${DELAY_MODES[*]}"

if [[ "${#STRICT_TARGETS[@]}" -gt 0 ]]; then
  for combine in "${COMBINE_MODES[@]}"; do
    for delay in "${DELAY_MODES[@]}"; do
      echo "=== uwvm_int strict matrix: combine=${combine}, delay=${delay} (sanitizers) ==="
      xmake f -c
      xmake f "${COMMON_F_FLAGS[@]}" \
        "--enable-uwvm-int-combine-ops=${combine}" \
        "--enable-uwvm-int-delay-local=${delay}" \
        "${SAN_POLICIES_FLAGS[@]}"
      if [[ "$#" -gt 0 ]]; then
        for t in "${STRICT_TARGETS[@]}"; do
          xmake_build "${t}"
        done
      else
        xmake_build -g "${STRICT_DIR}/*"
      fi
      for t in "${STRICT_TARGETS[@]}"; do
        xmake r "${t}"
      done
    done
  done
fi

if [[ "${#VALIDATE_TARGETS[@]}" -gt 0 ]]; then
  echo "=== uwvm_int strict: run validate targets (no sanitizer policies; for catching C++ exceptions on macOS) ==="
  xmake f -c
  xmake f "${COMMON_F_FLAGS[@]}" \
    --enable-uwvm-int-combine-ops=heavy \
    --enable-uwvm-int-delay-local=heavy
  xmake -v
  for t in "${VALIDATE_TARGETS[@]}"; do
    xmake_build "${t}"
    xmake r "${t}"
  done
fi
