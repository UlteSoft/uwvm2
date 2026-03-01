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
  --policies=build.sanitizer.address,build.sanitizer.leak,build.sanitizer.undefined
)

STRICT_TARGETS=()
VALIDATE_TARGET=""

if [[ "$#" -gt 0 ]]; then
  STRICT_TARGETS=("$@")
else
  for f in "${STRICT_DIR}"/*.cc; do
    b="$(basename -- "${f}" .cc)"
    if [[ "${b}" == "uwvm_int_validate_errors_strict" ]]; then
      VALIDATE_TARGET="${b}"
    else
      STRICT_TARGETS+=("${b}")
    fi
  done
fi

if [[ "${#STRICT_TARGETS[@]}" -eq 0 ]]; then
  echo "ERR: no strict targets found." >&2
  exit 3
fi

COMBINE_MODES=(none soft heavy extra)
DELAY_MODES=(none soft heavy)

for combine in "${COMBINE_MODES[@]}"; do
  for delay in "${DELAY_MODES[@]}"; do
    echo "=== uwvm_int strict matrix: combine=${combine}, delay=${delay} (sanitizers) ==="
    xmake f -c
    xmake f "${COMMON_F_FLAGS[@]}" \
      "--enable-uwvm-int-combine-ops=${combine}" \
      "--enable-uwvm-int-delay-local=${delay}" \
      "${SAN_POLICIES_FLAGS[@]}"
    xmake -v
    for t in "${STRICT_TARGETS[@]}"; do
      xmake b -v "${t}"
      xmake r "${t}"
    done
  done
done

if [[ -n "${VALIDATE_TARGET}" ]]; then
  echo "=== uwvm_int strict: run ${VALIDATE_TARGET} (no sanitizer policies; for catching C++ exceptions on macOS) ==="
  xmake f -c
  xmake f "${COMMON_F_FLAGS[@]}" \
    --enable-uwvm-int-combine-ops=heavy \
    --enable-uwvm-int-delay-local=heavy
  xmake -v
  xmake b -v "${VALIDATE_TARGET}"
  xmake r "${VALIDATE_TARGET}"
fi
