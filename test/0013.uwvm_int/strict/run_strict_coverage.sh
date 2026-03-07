#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/../../.." && pwd)"
cd -- "${ROOT_DIR}"

STRICT_DIR="test/0013.uwvm_int/strict"
WASM1_H="src/uwvm2/runtime/compiler/uwvm_int/compile_all_from_uwvm/wasm1.h"

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

TOOLCHAIN_ROOT="$(cd -- "$(dirname -- "${SYSROOT}")" && pwd)"
LLVM_BIN="${TOOLCHAIN_ROOT}/llvm/bin"
LLVM_PROFDATA="${LLVM_BIN}/llvm-profdata"
LLVM_COV="${LLVM_BIN}/llvm-cov"
CLANG_BIN="${LLVM_BIN}/clang"

if [[ ! -x "${LLVM_PROFDATA}" || ! -x "${LLVM_COV}" ]]; then
  echo "ERR: llvm-profdata/llvm-cov not found under: ${LLVM_BIN}" >&2
  exit 3
fi

# macOS: dyld may not find clang runtime libraries (profile runtime, etc).
if [[ "$(uname -s)" == "Darwin" ]]; then
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
  --ccache=n
  "--sysroot=${SYSROOT}"
  --test-libfuzzer=y
  --use-cxx-module=n
  --static=n
  --enable-int=uwvm-int
  --enable-uwvm-int-combine-ops=extra
  --enable-uwvm-int-delay-local=heavy
)

COVER_CXFLAGS="-fprofile-instr-generate -fcoverage-mapping"
COVER_LDFLAGS="-fprofile-instr-generate -fcoverage-mapping"

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
  exit 4
fi

COVER_DIR="${ROOT_DIR}/build/uwvm_int_strict_coverage"
PROFRAW_DIR="${COVER_DIR}/profraw"
HTML_DIR="${COVER_DIR}/html"
PROFDATA_FILE="${COVER_DIR}/merged.profdata"

rm -rf -- "${COVER_DIR}"
mkdir -p -- "${PROFRAW_DIR}"

echo "=== uwvm_int strict coverage: configure (combine=extra, delay=heavy) ==="
xmake f -c
xmake f "${COMMON_F_FLAGS[@]}" --cxflags="${COVER_CXFLAGS}" --ldflags="${COVER_LDFLAGS}"

export LLVM_PROFILE_FILE="${PROFRAW_DIR}/%p.profraw"

echo "=== uwvm_int strict coverage: build+run strict targets (profile) ==="
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

if ! ls "${PROFRAW_DIR}"/*.profraw >/dev/null 2>&1; then
  echo "ERR: no .profraw generated at: ${PROFRAW_DIR}" >&2
  exit 5
fi

echo "=== uwvm_int strict coverage: merge profraw -> profdata ==="
"${LLVM_PROFDATA}" merge -sparse "${PROFRAW_DIR}"/*.profraw -o "${PROFDATA_FILE}"

echo "=== uwvm_int strict coverage: generate html for wasm1.h ==="
mkdir -p -- "${HTML_DIR}"

OBJECT_ARGS=()
for t in "${STRICT_TARGETS[@]}"; do
  tf="$(xmake show -t "${t}" | perl -pe 's/\e\[[0-9;]*m//g' | sed -n 's/^[[:space:]]*targetfile:[[:space:]]*//p' | head -n1 || true)"
  if [[ -z "${tf}" ]]; then
    echo "ERR: failed to query targetfile for: ${t}" >&2
    exit 6
  fi
  if [[ ! -f "${ROOT_DIR}/${tf}" ]]; then
    echo "ERR: targetfile not found: ${ROOT_DIR}/${tf}" >&2
    exit 7
  fi
  OBJECT_ARGS+=(--object "${ROOT_DIR}/${tf}")
done

"${LLVM_COV}" show \
  -instr-profile="${PROFDATA_FILE}" \
  --format=html \
  --output-dir="${HTML_DIR}" \
  --show-region-summary \
  --show-branch-summary \
  --include-filename-regex='.*compile_all_from_uwvm/wasm1[.]h$' \
  "${OBJECT_ARGS[@]}" >/dev/null

echo "OK: coverage html written to: ${HTML_DIR}"

echo "=== uwvm_int strict coverage: export json for wasm1.h ==="
"${LLVM_COV}" export \
  -instr-profile="${PROFDATA_FILE}" \
  --include-filename-regex='.*compile_all_from_uwvm/wasm1[.]h$' \
  "${OBJECT_ARGS[@]}" > "${COVER_DIR}/wasm1_export.json"

echo "OK: coverage json written to: ${COVER_DIR}/wasm1_export.json"
