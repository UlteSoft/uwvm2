#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"
cd -- "${ROOT_DIR}"

BUILD_MODE="${UWVM_BACKEND_MATRIX_BUILD_MODE:-debug}"
FUZZ_CASES="${UWVM_BACKEND_FUZZ_CASES:-32}"
WABT_ROOT="${UWVM_BACKEND_FUZZER_WABT_ROOT:-${ROOT_DIR}/build/test/third-parties/wabt}"
WORK_ROOT="${UWVM_BACKEND_FUZZER_WORK_DIR:-${ROOT_DIR}/build/test/0016.backend_fuzzer/matrix}"
if [[ "$(uname -s)" == "Darwin" ]]; then
  DEFAULT_SANITIZER_POLICIES="build.sanitizer.address,build.sanitizer.undefined"
else
  DEFAULT_SANITIZER_POLICIES="build.sanitizer.address,build.sanitizer.leak,build.sanitizer.undefined"
fi
SANITIZER_POLICIES="${UWVM_BACKEND_MATRIX_POLICIES:-${DEFAULT_SANITIZER_POLICIES}}"

COMBINE_MODES=(none soft heavy extra)
if [[ -n "${UWVM_BACKEND_MATRIX_COMBINE_MODES:-}" ]]; then
  read -r -a COMBINE_MODES <<< "${UWVM_BACKEND_MATRIX_COMBINE_MODES//,/ }"
fi
DELAY_MODES=(none soft heavy)
if [[ -n "${UWVM_BACKEND_MATRIX_DELAY_MODES:-}" ]]; then
  read -r -a DELAY_MODES <<< "${UWVM_BACKEND_MATRIX_DELAY_MODES//,/ }"
fi

mkdir -p -- "${ROOT_DIR}/build"
LOCK_DIR="${UWVM_BACKEND_MATRIX_LOCK_DIR:-${ROOT_DIR}/build/backend_fuzzer_matrix.lock}"
if ! mkdir -- "${LOCK_DIR}" 2>/dev/null; then
  echo "ERR: another backend fuzzer matrix run appears to be active: ${LOCK_DIR}" >&2
  exit 9
fi
printf '%s\n' "$$" > "${LOCK_DIR}/pid"
cleanup_lock() {
  rm -rf -- "${LOCK_DIR}"
}
trap cleanup_lock EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

usage() {
  cat <<'EOF'
usage: run_backend_fuzzer_matrix.sh

Environment:
  UWVM_BACKEND_MATRIX_COMBINE_MODES="none soft heavy extra"
  UWVM_BACKEND_MATRIX_DELAY_MODES="none soft heavy"
  UWVM_BACKEND_MATRIX_BUILD_MODE=debug
  UWVM_BACKEND_MATRIX_POLICIES=build.sanitizer.address,build.sanitizer.leak,build.sanitizer.undefined
  UWVM_BACKEND_FUZZ_CASES=32
  UWVM_BACKEND_FUZZER_INCLUDE_TRAPS=0|1
  UWVM_XMAKE_JOBS=<count>

The script reconfigures xmake for each combine/delay-local pair, builds uwvm,
then runs run_backend_fuzzer.sh against:
  uwvm-int lazy/full and llvm-jit lazy/full.

Mode lists may be separated by spaces or commas.
EOF
}

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi
if [[ "$#" -ne 0 ]]; then
  echo "ERR: unknown argument: $1" >&2
  usage >&2
  exit 2
fi

if [[ "${#COMBINE_MODES[@]}" -eq 0 || "${#DELAY_MODES[@]}" -eq 0 ]]; then
  echo "ERR: combine/delay mode matrix cannot be empty" >&2
  exit 2
fi

if [[ -n "${UWVM_XMAKE_JOBS:-}" && ! "${UWVM_XMAKE_JOBS}" =~ ^[1-9][0-9]*$ ]]; then
  echo "ERR: UWVM_XMAKE_JOBS must be a positive integer, got: ${UWVM_XMAKE_JOBS}" >&2
  exit 2
fi

xmake_build() {
  if [[ -n "${UWVM_XMAKE_JOBS:-}" ]]; then
    xmake b -v -j "${UWVM_XMAKE_JOBS}" "$@"
  else
    xmake b -v "$@"
  fi
}

resolve_uwvm() {
  local shown
  shown="$(
    xmake show -t uwvm --json 2>/dev/null |
      sed -n 's/.*"targetfile":"\([^"]*\)".*/\1/p' |
      sed 's#\\/#/#g; s#\\\\#\\#g' |
      head -n1 || true
  )"
  if [[ -z "${shown}" ]]; then
    echo "ERR: could not resolve uwvm targetfile after build" >&2
    exit 2
  fi
  case "${shown}" in
    /*) printf '%s\n' "${shown}" ;;
    *) printf '%s\n' "${ROOT_DIR}/${shown}" ;;
  esac
}

COMMON_F_FLAGS=(
  -m "${BUILD_MODE}"
  --use-llvm-compiler=y
  --ccache=n
  --cxflags=-Wno-error
  --use-cxx-module=n
  --static=none
  "--policies=${SANITIZER_POLICIES}"
  --execution-int=uwvm-int
  --execution-jit=llvm
  --enable-test-backend-fuzzer=y
)

if [[ -n "${SYSROOT:-}" ]]; then
  COMMON_F_FLAGS+=("--sysroot=${SYSROOT}")
fi

echo "INFO: backend matrix build mode = ${BUILD_MODE}"
echo "INFO: backend matrix combine modes = ${COMBINE_MODES[*]}"
echo "INFO: backend matrix delay-local modes = ${DELAY_MODES[*]}"
echo "INFO: backend matrix fuzz cases = ${FUZZ_CASES}"

for combine in "${COMBINE_MODES[@]}"; do
  for delay in "${DELAY_MODES[@]}"; do
    echo "=== backend fuzzer matrix: combine=${combine}, delay-local=${delay} ==="
    xmake f -c
    xmake f "${COMMON_F_FLAGS[@]}" \
      "--enable-uwvm-int-combine-ops=${combine}" \
      "--enable-uwvm-int-delay-local=${delay}"

    xmake_build uwvm
    uwvm_exe="$(resolve_uwvm)"

    case_dir="${WORK_ROOT}/combine-${combine}_delay-${delay}"
    UWVM_BACKEND_FUZZ_CASES="${FUZZ_CASES}" \
      UWVM_BACKEND_FUZZER_WABT_ROOT="${WABT_ROOT}" \
      UWVM_BACKEND_FUZZER_WORK_DIR="${case_dir}" \
      "${SCRIPT_DIR}/run_backend_fuzzer.sh" --uwvm "${uwvm_exe}"
  done
done

echo "OK: backend fuzzer matrix completed"
