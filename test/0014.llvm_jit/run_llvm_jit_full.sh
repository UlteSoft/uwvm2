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

if [[ -n "${UWVM_XMAKE_JOBS:-}" ]]; then
  if [[ ! "${UWVM_XMAKE_JOBS}" =~ ^[1-9][0-9]*$ ]]; then
    echo "ERR: UWVM_XMAKE_JOBS must be a positive integer, got: ${UWVM_XMAKE_JOBS}" >&2
    exit 2
  fi
  echo "INFO: xmake jobs limited via UWVM_XMAKE_JOBS=${UWVM_XMAKE_JOBS}"
fi

UWVM_XMAKE_MODE="${UWVM_XMAKE_MODE:-debug}"
case "${UWVM_XMAKE_MODE}" in
  debug|release|releasedbg|minsizerel) ;;
  *)
    echo "ERR: UWVM_XMAKE_MODE must be debug, release, releasedbg, or minsizerel; got: ${UWVM_XMAKE_MODE}" >&2
    exit 2
    ;;
esac
echo "INFO: xmake mode = ${UWVM_XMAKE_MODE}"

xmake_build() {
  if [[ -n "${UWVM_XMAKE_JOBS:-}" ]]; then
    xmake b -v -j "${UWVM_XMAKE_JOBS}" "$@"
  else
    xmake b -v "$@"
  fi
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
  --execution-int=none
  --execution-jit=llvm
)

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
      printf '%s\n' call_indirect_encoding_parity
      printf '%s\n' llvm_jit_cache_integration
      printf '%s\n' llvm_jit_imported_bulk_memory
      printf '%s\n' llvm_jit_trap_matrix_wat
      printf '%s\n' llvm_jit_unwind_call_stack_wat
      printf '%s\n' llvm_jit_wasm1p1_clang_cpp_matrix
    } | awk '!seen[$0]++'
  )
fi

# ROS deliberately validates only its reduced LLVM AOT surface.  Targets below inspect or execute the interpreter
# translation path and therefore cannot be requested after --execution-int=none removed that backend.
for t in "${TARGETS[@]}"; do
  case "${t}" in
    lj13s_*|lj13l_*|llvm_jit_trunc_boundary_matrix|wasm32_effective_address)
      echo "ERR: target ${t} depends on uwvm-int and is invalid in the ROS AOT-only suite" >&2
      exit 2
      ;;
  esac
done

if [[ "${#TARGETS[@]}" -eq 0 ]]; then
  echo "ERR: no llvm-jit targets found." >&2
  exit 3
fi

NEEDS_WAT2WASM=0
for t in "${TARGETS[@]}"; do
  case "${t}" in
    llvm_jit_trap_matrix_wat|llvm_jit_unwind_call_stack_wat)
      NEEDS_WAT2WASM=1
      break
      ;;
  esac
done

if [[ "${NEEDS_WAT2WASM}" == "1" ]]; then
  if ! WAT2WASM_RESOLVED="$(resolve_wat2wasm)"; then
    echo "ERR: wat2wasm is required for the selected LLVM AOT WAT tests; set WAT2WASM or install wat2wasm in PATH" >&2
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

echo "OK: LLVM AOT-only run completed"
