#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"
cd -- "${ROOT_DIR}"

mkdir -p -- "${ROOT_DIR}/build"
LOCK_DIR="${UWVM_LLVM_JIT_LOCK_DIR:-${ROOT_DIR}/build/llvm_jit.lock}"
if ! mkdir -- "${LOCK_DIR}" 2>/dev/null; then
  echo "ERR: another llvm-jit run appears to be active: ${LOCK_DIR}" >&2
  exit 9
fi
printf '%s\n' "$$" > "${LOCK_DIR}/pid"
cleanup_lock() { rm -rf -- "${LOCK_DIR}"; }
trap cleanup_lock EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

UWVM_XMAKE_JOBS="${UWVM_XMAKE_JOBS:-1}"
if [[ "${UWVM_XMAKE_JOBS}" != "1" ]]; then
  echo "ERR: uwvm2 builds are globally limited to -j1; got UWVM_XMAKE_JOBS=${UWVM_XMAKE_JOBS}" >&2
  exit 2
fi
echo "INFO: uwvm2 xmake jobs enforced at -j1"

UWVM_XMAKE_MODE="${UWVM_XMAKE_MODE:-debug}"
case "${UWVM_XMAKE_MODE}" in
  debug|release|releasedbg|minsizerel) ;;
  *)
    echo "ERR: UWVM_XMAKE_MODE must be debug, release, releasedbg, or minsizerel; got: ${UWVM_XMAKE_MODE}" >&2
    exit 2
    ;;
esac
echo "INFO: xmake mode = ${UWVM_XMAKE_MODE}"

# The full repository supports two intentional LLVM validation surfaces.  Keep AOT as the resource-safe default used by
# existing CI; request `full` explicitly for the combined uwvm-int + LLVM lazy/tiered matrix.
UWVM_LLVM_JIT_TEST_PROFILE="${UWVM_LLVM_JIT_TEST_PROFILE:-aot}"
case "${UWVM_LLVM_JIT_TEST_PROFILE}" in
  full|aot) ;;
  *)
    echo "ERR: UWVM_LLVM_JIT_TEST_PROFILE must be full or aot; got: ${UWVM_LLVM_JIT_TEST_PROFILE}" >&2
    exit 2
    ;;
esac
echo "INFO: llvm-jit test profile = ${UWVM_LLVM_JIT_TEST_PROFILE}"

xmake_build() {
  xmake b -v -j 1 "$@"
}

resolve_wat2wasm() {
  local candidate=""
  if [[ -n "${WAT2WASM:-}" ]]; then
    if [[ -x "${WAT2WASM}" ]]; then
      candidate="${WAT2WASM}"
    else
      candidate="$(command -v -- "${WAT2WASM}" 2>/dev/null || true)"
    fi
  else
    local name
    local path
    for name in wat2wasm wat2wasm.exe; do
      for path in \
        "${ROOT_DIR}/build/test/third-parties/wabt/build/${name}" \
        "${ROOT_DIR}/build/test/third-parties/wabt/build/bin/${name}" \
        "${ROOT_DIR}/build/test/third-parties/wabt/build/Release/${name}" \
        "${ROOT_DIR}/build/test/third-parties/wabt/build-ninja/${name}" \
        "${ROOT_DIR}/wabt/build/${name}" \
        "${ROOT_DIR}/wabt/build/bin/${name}" \
        "${ROOT_DIR}/wabt/build/Release/${name}" \
        "${ROOT_DIR}/wabt/build-ninja/${name}"; do
        if [[ -x "${path}" ]]; then
          candidate="${path}"
          break 2
        fi
      done
    done
    if [[ -z "${candidate}" ]]; then
      candidate="$(command -v wat2wasm 2>/dev/null || true)"
    fi
  fi

  if [[ -z "${candidate}" || ! -x "${candidate}" ]]; then
    return 1
  fi
  if ! "${candidate}" --version >/dev/null 2>&1; then
    return 1
  fi

  local candidate_dir
  candidate_dir="$(cd -- "$(dirname -- "${candidate}")" && pwd)"
  printf '%s/%s\n' "${candidate_dir}" "$(basename -- "${candidate}")"
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
  -m "${UWVM_XMAKE_MODE}"
  --use-llvm-compiler=y
  --ccache=n
  --cxflags=-Wno-error
  --test-libfuzzer=n
  --enable-test-backend-fuzzer=n
  --enable-test-llvm-jit=y
  --use-cxx-module=n
  --static=none
  --execution-jit=llvm
)

if [[ "${UWVM_LLVM_JIT_TEST_PROFILE}" == "full" ]]; then
  COMMON_F_FLAGS+=(--execution-int=uwvm-int)
else
  COMMON_F_FLAGS+=(--execution-int=none)
fi

if [[ -n "${SYSROOT:-}" ]]; then
  COMMON_F_FLAGS+=("--sysroot=${SYSROOT}")
fi

TARGETS=()
if [[ "$#" -gt 0 ]]; then
  TARGETS=("$@")
else
  while IFS= read -r name; do
    TARGETS+=("${name}")
  done < <(
    {
      printf '%s\n' llvm_jit_verify_compile
      printf '%s\n' llvm_aot_fp_environment_wat
      printf '%s\n' llvm_aot_nearest_rounding
      printf '%s\n' llvm_aot_noinline_policy
      printf '%s\n' llvm_aot_unaligned_memory
      printf '%s\n' llvm_jit_imported_bulk_memory
      printf '%s\n' llvm_jit_local_imported_global_byte_bridge
      printf '%s\n' wasm2_feature_validator_parity
      if [[ "${UWVM_LLVM_JIT_TEST_PROFILE}" == "full" ]]; then
        printf '%s\n' llvm_jit_multivalue_typed
        printf '%s\n' llvm_jit_trap_matrix_wat
        printf '%s\n' llvm_jit_unwind_call_stack_wat
        printf '%s\n' tiered_osr_call_stack_wat
        printf '%s\n' tiered_strategy_unwind_wat
        index=0
        find test/0013.uwvm_int/strict -type f -name '*.cc' | sort | while IFS= read -r file; do
          index=$((index + 1))
          printf 'lj13s_%03d\n' "${index}"
        done
        index=0
        find test/0013.uwvm_int/lazy -type f -name '*.cc' | sort | while IFS= read -r file; do
          # Bash 3.2 misparses a nested `case ... pattern)` while this loop lives inside the process substitution above.
          # Keep the portable `[[ ... ]]` form so the directed-suite generator also works on the supported Darwin shell.
          if [[ "${file}" == */uwvm_int_lazy_split.cc || "${file}" == */uwvm_int_lazy_strategy_matrix.cc ]]; then
            continue
          fi
          index=$((index + 1))
          printf 'lj13l_%03d\n' "${index}"
        done
      fi
    } | awk '!seen[$0]++'
  )
fi

if [[ "${UWVM_LLVM_JIT_TEST_PROFILE}" == "aot" ]]; then
  for t in "${TARGETS[@]}"; do
    case "${t}" in
      lj13s_*|lj13l_*|llvm_jit_cache_integration|llvm_jit_lazy_raw_body_scanner|llvm_jit_multivalue_typed|llvm_jit_trap_matrix_wat|llvm_jit_trunc_boundary_matrix|llvm_jit_unwind_call_stack_wat|llvm_jit_wasm1p1_clang_cpp_matrix|tiered_*|wasm32_effective_address)
        echo "ERR: target ${t} requires int, lazy, or tiered coverage and is invalid in the aot profile" >&2
        exit 2
        ;;
    esac
  done
fi

if [[ "${#TARGETS[@]}" -eq 0 ]]; then
  echo "ERR: no llvm-jit targets found." >&2
  exit 3
fi

NEEDS_WAT2WASM=0
for t in "${TARGETS[@]}"; do
  case "${t}" in
    llvm_jit_trap_matrix_wat|llvm_jit_unwind_call_stack_wat|tiered_osr_call_stack_wat|tiered_strategy_unwind_wat)
      NEEDS_WAT2WASM=1
      break
      ;;
  esac
done

if [[ "${NEEDS_WAT2WASM}" == "1" ]]; then
  if ! WAT2WASM_RESOLVED="$(resolve_wat2wasm)"; then
    echo "ERR: wat2wasm is required for the selected llvm-jit WAT tests; set WAT2WASM or install wat2wasm in PATH" >&2
    exit 5
  fi
  export WAT2WASM="${WAT2WASM_RESOLVED}"
  export UWVM_TRAP_MATRIX_STRICT=1
  echo "INFO: wat2wasm = ${WAT2WASM}"
fi

echo "INFO: llvm-jit target count = ${#TARGETS[@]}"
echo "=== llvm-jit full: configure ==="
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

echo "OK: llvm-jit ${UWVM_LLVM_JIT_TEST_PROFILE} run completed"
