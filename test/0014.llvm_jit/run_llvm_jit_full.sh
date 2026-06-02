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
  --enable-test-llvm-jit=y
  --use-cxx-module=n
  --static=none
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
      find test/0013.uwvm_int/strict -type f -name '*.cc' | sort | while IFS= read -r file; do
        rel="${file#test/0013.uwvm_int/strict/}"
        suffix="${rel%.cc}"
        suffix="${suffix//\//_}"
        suffix="${suffix//./_}"
        suffix="${suffix//-/_}"
        printf 'llvm_jit_reuse_0013_%s\n' "${suffix}"
      done
      find test/0013.uwvm_int/lazy -type f -name '*.cc' | sort | while IFS= read -r file; do
        case "${file}" in
          *"/uwvm_int_lazy_split.cc"|*"/uwvm_int_lazy_strategy_matrix.cc") continue ;;
        esac
        rel="${file#test/0013.uwvm_int/lazy/}"
        suffix="${rel%.cc}"
        suffix="${suffix//\//_}"
        suffix="${suffix//./_}"
        suffix="${suffix//-/_}"
        printf 'llvm_jit_lazy_reuse_0013_%s\n' "${suffix}"
      done
    } | awk '!seen[$0]++'
  )
fi

if [[ "${#TARGETS[@]}" -eq 0 ]]; then
  echo "ERR: no llvm-jit targets found." >&2
  exit 3
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

echo "OK: llvm-jit full run completed"
