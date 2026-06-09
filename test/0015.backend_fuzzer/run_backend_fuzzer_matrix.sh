#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"

FUZZ_CASES="${UWVM_BACKEND_FUZZ_CASES:-32}"
FUZZ_SEED="${UWVM_BACKEND_FUZZ_SEED:-0xC0DEF00D}"
WABT_ROOT="${UWVM_BACKEND_FUZZER_WABT_ROOT:-${ROOT_DIR}/build/test/third-parties/wabt}"
WORK_ROOT="${UWVM_BACKEND_FUZZER_WORK_DIR:-${ROOT_DIR}/build/test/0015.backend_fuzzer/matrix}"

COMBINE_MODES=(none soft heavy extra)
if [[ -n "${UWVM_BACKEND_MATRIX_COMBINE_MODES:-}" ]]; then
  read -r -a COMBINE_MODES <<< "${UWVM_BACKEND_MATRIX_COMBINE_MODES//,/ }"
fi

DELAY_MODES=(none soft heavy)
if [[ -n "${UWVM_BACKEND_MATRIX_DELAY_MODES:-}" ]]; then
  read -r -a DELAY_MODES <<< "${UWVM_BACKEND_MATRIX_DELAY_MODES//,/ }"
fi

MEMORY_MODELS=(default)
if [[ -n "${UWVM_BACKEND_MATRIX_MEMORY_MODELS:-}" ]]; then
  read -r -a MEMORY_MODELS <<< "${UWVM_BACKEND_MATRIX_MEMORY_MODELS//,/ }"
fi

THREAD_LOCAL_MODES=(0)
if [[ -n "${UWVM_BACKEND_MATRIX_THREAD_LOCAL_MODES:-}" ]]; then
  read -r -a THREAD_LOCAL_MODES <<< "${UWVM_BACKEND_MATRIX_THREAD_LOCAL_MODES//,/ }"
fi

usage() {
  cat <<'EOF'
usage: run_backend_fuzzer_matrix.sh

Environment:
  UWVM_BACKEND_MATRIX_COMBINE_MODES="none soft heavy extra"
  UWVM_BACKEND_MATRIX_DELAY_MODES="none soft heavy"
  UWVM_BACKEND_MATRIX_MEMORY_MODELS="default mmap single-thread-alloc multi-thread-alloc"
  UWVM_BACKEND_MATRIX_THREAD_LOCAL_MODES="0 1"
  UWVM_BACKEND_FUZZ_CASES=32
  UWVM_BACKEND_FUZZ_SEED=0xC0DEF00D
  UWVM_BACKEND_FUZZER_INCLUDE_TRAPS=0|1
  UWVM_BACKEND_FUZZER_MODES="uwvm-int-ring-matrix uwvm-int-lazy ..."
  UWVM_BACKEND_FUZZER_WABT_ROOT=build/test/third-parties/wabt
  UWVM_BACKEND_FUZZER_WORK_DIR=build/test/0015.backend_fuzzer/matrix

The matrix is fully shell/Python driven; it does not configure, build, or query
xmake.
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

if [[ "${#COMBINE_MODES[@]}" -eq 0 || "${#DELAY_MODES[@]}" -eq 0 || "${#MEMORY_MODELS[@]}" -eq 0 || "${#THREAD_LOCAL_MODES[@]}" -eq 0 ]]; then
  echo "ERR: matrix dimensions cannot be empty" >&2
  exit 2
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

echo "INFO: backend fuzzer combine modes = ${COMBINE_MODES[*]}"
echo "INFO: backend fuzzer delay-local modes = ${DELAY_MODES[*]}"
echo "INFO: backend fuzzer memory models = ${MEMORY_MODELS[*]}"
echo "INFO: backend fuzzer thread-local modes = ${THREAD_LOCAL_MODES[*]}"
echo "INFO: backend fuzzer random cases = ${FUZZ_CASES}"
echo "INFO: backend fuzzer seed = ${FUZZ_SEED}"

for combine in "${COMBINE_MODES[@]}"; do
  for delay in "${DELAY_MODES[@]}"; do
    for memory_model in "${MEMORY_MODELS[@]}"; do
      for use_thread_local in "${THREAD_LOCAL_MODES[@]}"; do
        echo "=== backend fuzzer matrix: combine=${combine}, delay-local=${delay}, memory=${memory_model}, thread-local=${use_thread_local} ==="
        case_dir="${WORK_ROOT}/combine-${combine}_delay-${delay}_memory-${memory_model}_thread-local-${use_thread_local}"
        UWVM_BACKEND_FUZZ_CASES="${FUZZ_CASES}" \
          UWVM_BACKEND_FUZZ_SEED="${FUZZ_SEED}" \
          UWVM_BACKEND_FUZZER_WABT_ROOT="${WABT_ROOT}" \
          UWVM_BACKEND_FUZZER_WORK_DIR="${case_dir}" \
          UWVM_BACKEND_FUZZER_COMBINE="${combine}" \
          UWVM_BACKEND_FUZZER_DELAY="${delay}" \
          UWVM_BACKEND_FUZZER_MEMORY_MODEL="${memory_model}" \
          UWVM_BACKEND_FUZZER_USE_THREAD_LOCAL="${use_thread_local}" \
          "${SCRIPT_DIR}/run_backend_fuzzer.sh"
      done
    done
  done
done

echo "OK: backend fuzzer matrix completed"
