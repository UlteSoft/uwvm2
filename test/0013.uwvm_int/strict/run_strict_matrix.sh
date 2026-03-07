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

# Optional: limit xmake parallel jobs (useful on memory-limited machines, e.g. macOS).
# Example: export UWVM_XMAKE_JOBS=4
XMAKE_JOBS_ARGS=()
if [[ -n "${UWVM_XMAKE_JOBS:-}" ]]; then
  if [[ "${UWVM_XMAKE_JOBS}" =~ ^[1-9][0-9]*$ ]]; then
    XMAKE_JOBS_ARGS=(-j "${UWVM_XMAKE_JOBS}")
    echo "INFO: xmake jobs limited via UWVM_XMAKE_JOBS=${UWVM_XMAKE_JOBS}"
  else
    echo "ERR: UWVM_XMAKE_JOBS must be a positive integer, got: ${UWVM_XMAKE_JOBS}" >&2
    exit 2
  fi
fi

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
  --use-llvm=y
  "--sysroot=${SYSROOT}"
  --test-libfuzzer=y
  --use-cxx-module=n
  --static=n
  --enable-int=uwvm-int
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
DELAY_MODES=(none soft heavy)

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
          xmake b -v "${XMAKE_JOBS_ARGS[@]}" "${t}"
        done
      else
        xmake b -v "${XMAKE_JOBS_ARGS[@]}" -g "${STRICT_DIR}/*"
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
    xmake b -v "${XMAKE_JOBS_ARGS[@]}" "${t}"
    xmake r "${t}"
  done
fi
