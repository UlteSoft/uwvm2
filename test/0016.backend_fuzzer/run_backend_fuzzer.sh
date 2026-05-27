#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"

UWVM_EXE="${UWVM_BACKEND_FUZZER_UWVM:-}"
WABT_ROOT="${UWVM_BACKEND_FUZZER_WABT_ROOT:-${ROOT_DIR}/build/test/third-parties/wabt}"
WORK_DIR="${UWVM_BACKEND_FUZZER_WORK_DIR:-${ROOT_DIR}/build/test/0016.backend_fuzzer}"
FUZZ_CASES="${UWVM_BACKEND_FUZZ_CASES:-32}"
KEEP_WORK=0
WABT_MVP_FLAGS=(
  --disable-mutable-globals
  --disable-saturating-float-to-int
  --disable-sign-extension
  --disable-simd
  --disable-multi-value
  --disable-bulk-memory
  --disable-reference-types
)

setup_sanitizer_env() {
  case "$(uname -s)" in
    Darwin)
      ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=1}"
      UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}"
      export ASAN_OPTIONS UBSAN_OPTIONS
      ;;
    *)
      ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=1:halt_on_error=1}"
      LSAN_OPTIONS="${LSAN_OPTIONS:-print_suppressions=0}"
      UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}"
      export ASAN_OPTIONS LSAN_OPTIONS UBSAN_OPTIONS
      ;;
  esac
}

usage() {
  cat <<'EOF'
usage: run_backend_fuzzer.sh [options]

Options:
  --uwvm <path>        Path to the uwvm executable. If omitted, xmake show -t uwvm is used.
  --wabt-root <path>   WABT checkout/build root. Default: build/test/third-parties/wabt.
  --work-dir <path>    Generated corpus/log directory. Default: build/test/0016.backend_fuzzer.
  --fuzz-cases <n>     Number of generated pseudo-random i32 cases. Default: 32.
  --keep               Keep existing generated work directory contents.
  -h, --help           Show this help.

WABT is run with post-MVP features disabled so the oracle is Wasm MVP/1.0.
EOF
}

while (($#)); do
  case "$1" in
    --uwvm)
      [[ $# -ge 2 ]] || { echo "ERR: --uwvm requires a path" >&2; exit 2; }
      UWVM_EXE="$2"
      shift 2
      ;;
    --wabt-root)
      [[ $# -ge 2 ]] || { echo "ERR: --wabt-root requires a path" >&2; exit 2; }
      WABT_ROOT="$2"
      shift 2
      ;;
    --work-dir)
      [[ $# -ge 2 ]] || { echo "ERR: --work-dir requires a path" >&2; exit 2; }
      WORK_DIR="$2"
      shift 2
      ;;
    --fuzz-cases)
      [[ $# -ge 2 ]] || { echo "ERR: --fuzz-cases requires an integer" >&2; exit 2; }
      FUZZ_CASES="$2"
      shift 2
      ;;
    --keep)
      KEEP_WORK=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "ERR: unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

case "${FUZZ_CASES}" in
  ''|*[!0-9]*)
    echo "ERR: --fuzz-cases must be a non-negative integer, got: ${FUZZ_CASES}" >&2
    exit 2
    ;;
esac

require_tool() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "ERR: required tool not found on PATH: $1" >&2
    exit 2
  fi
}

resolve_path() {
  local p="$1"
  case "${p}" in
    /*) printf '%s\n' "${p}" ;;
    *) printf '%s\n' "${ROOT_DIR}/${p}" ;;
  esac
}

resolve_uwvm() {
  if [[ -n "${UWVM_EXE}" ]]; then
    UWVM_EXE="$(resolve_path "${UWVM_EXE}")"
    return
  fi

  require_tool xmake
  local shown
  shown="$(
    cd "${ROOT_DIR}" &&
      xmake show -t uwvm --json 2>/dev/null |
        sed -n 's/.*"targetfile":"\([^"]*\)".*/\1/p' |
        sed 's#\\/#/#g; s#\\\\#\\#g' |
        head -n1 || true
  )"
  if [[ -z "${shown}" ]]; then
    echo "ERR: could not resolve uwvm targetfile; pass --uwvm explicitly" >&2
    exit 2
  fi
  UWVM_EXE="$(resolve_path "${shown}")"
}

tool_path() {
  local base="$1"
  local p="${WABT_ROOT}/build/${base}"
  if [[ -x "${p}" ]]; then
    printf '%s\n' "${p}"
    return
  fi
  if [[ -x "${p}.exe" ]]; then
    printf '%s\n' "${p}.exe"
    return
  fi
  return 1
}

ensure_wabt_tools() {
  WABT_ROOT="$(resolve_path "${WABT_ROOT}")"
  local wat2wasm
  local wasm_interp
  if wat2wasm="$(tool_path wat2wasm)" && wasm_interp="$(tool_path wasm-interp)"; then
    WAT2WASM="${wat2wasm}"
    WASM_INTERP="${wasm_interp}"
    return
  fi

  require_tool git
  require_tool cmake

  mkdir -p -- "$(dirname -- "${WABT_ROOT}")"
  if [[ ! -d "${WABT_ROOT}/.git" && ! -f "${WABT_ROOT}/CMakeLists.txt" ]]; then
    if [[ -d "${WABT_ROOT}" && -n "$(find "${WABT_ROOT}" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null)" ]]; then
      echo "ERR: WABT root exists but is not a WABT checkout: ${WABT_ROOT}" >&2
      exit 2
    fi
    echo "INFO: cloning WABT into ${WABT_ROOT}"
    git clone --depth 1 --recursive https://github.com/WebAssembly/wabt.git "${WABT_ROOT}"
  elif [[ -d "${WABT_ROOT}/.git" ]]; then
    if [[ "${UWVM_BACKEND_FUZZER_WABT_PULL:-0}" != "0" ]]; then
      echo "INFO: updating WABT checkout in ${WABT_ROOT}"
      git -C "${WABT_ROOT}" pull --ff-only
    fi
    git -C "${WABT_ROOT}" submodule update --init --recursive
  fi

  echo "INFO: building WABT tools"
  cmake -S "${WABT_ROOT}" -B "${WABT_ROOT}/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -DBUILD_TOOLS=ON \
    -DBUILD_LIBWASM=OFF \
    -DUSE_INTERNAL_SHA256=ON \
    -DHAVE_OPENSSL_SHA_H=OFF
  cmake --build "${WABT_ROOT}/build" --target wat2wasm wasm-interp --config Release

  WAT2WASM="$(tool_path wat2wasm)"
  WASM_INTERP="$(tool_path wasm-interp)"
}

write_case() {
  local name="$1"
  shift
  local out="${CORPUS_DIR}/${name}.wat"
  cat > "${out}"
}

next_rand() {
  RAND_STATE=$(( (RAND_STATE * 1103515245 + 12345) & 0x7fffffff ))
  printf '%d\n' "${RAND_STATE}"
}

generate_corpus() {
  WORK_DIR="$(resolve_path "${WORK_DIR}")"
  CORPUS_DIR="${WORK_DIR}/corpus"
  WASM_DIR="${WORK_DIR}/wasm"
  LOG_DIR="${WORK_DIR}/logs"

  if [[ "${KEEP_WORK}" -eq 0 ]]; then
    rm -rf -- "${CORPUS_DIR}" "${WASM_DIR}" "${LOG_DIR}"
  fi
  mkdir -p -- "${CORPUS_DIR}" "${WASM_DIR}" "${LOG_DIR}"

  write_case 000_empty <<'EOF'
(module
  (func (export "main")))
EOF

  write_case 001_i32_arith <<'EOF'
(module
  (func (export "main")
    (if
      (i32.ne
        (i32.xor
          (i32.mul
            (i32.add (i32.const 12345) (i32.const 6789))
            (i32.const 3))
          (i32.shl (i32.const 99) (i32.const 4)))
        (i32.const 58890))
      (then unreachable))))
EOF

  write_case 002_i64_arith <<'EOF'
(module
  (func (export "main")
    (if
      (i64.ne
        (i64.xor
          (i64.mul
            (i64.add (i64.const 123456789) (i64.const 987654321))
            (i64.const 5))
          (i64.const 305419896))
        (i64.const 5789670054))
      (then unreachable))))
EOF

  write_case 003_branch_loop <<'EOF'
(module
  (func (export "main") (local $i i32) (local $sum i32)
    (local.set $i (i32.const 1))
    (local.set $sum (i32.const 0))
    (block $exit
      (loop $loop
        (br_if $exit (i32.ge_u (local.get $i) (i32.const 17)))
        (local.set $sum (i32.add (local.get $sum) (local.get $i)))
        (local.set $i (i32.add (local.get $i) (i32.const 1)))
        (br $loop)))
    (if
      (i32.ne (local.get $sum) (i32.const 136))
      (then unreachable))))
EOF

  write_case 004_memory <<'EOF'
(module
  (memory 1)
  (func (export "main")
    (i32.store (i32.const 16) (i32.const 0x11223344))
    (if
      (i32.ne
        (i32.load16_u (i32.const 18))
        (i32.const 0x1122))
      (then unreachable))))
EOF

  write_case 005_call_indirect <<'EOF'
(module
  (type $t (func (result i32)))
  (table 2 funcref)
  (elem (i32.const 0) $a $b)
  (func $a (type $t) (i32.const 11))
  (func $b (type $t) (i32.const 31))
  (func (export "main")
    (if
      (i32.ne
        (call_indirect (type $t) (i32.const 1))
        (i32.const 31))
      (then unreachable))))
EOF

  write_case 006_float <<'EOF'
(module
  (func (export "main")
    (if
      (f64.ne
        (f64.add
          (f64.mul (f64.const 1.5) (f64.const 4.0))
          (f64.const 0.25))
        (f64.const 6.25))
      (then unreachable))))
EOF

  write_case 007_global_select <<'EOF'
(module
  (global $g (mut i32) (i32.const 7))
  (func (export "main")
    (global.set $g (i32.const 10))
    (if
      (i32.ne
        (select
          (global.get $g)
          (i32.const 20)
          (i32.const 0))
        (i32.const 20))
      (then unreachable))))
EOF

  if [[ "${UWVM_BACKEND_FUZZER_INCLUDE_TRAPS:-1}" != "0" ]]; then
    write_case 100_trap_unreachable <<'EOF'
(module
  (func (export "main")
    unreachable))
EOF

    write_case 101_trap_div_zero <<'EOF'
(module
  (func (export "main")
    (drop (i32.div_s (i32.const 1) (i32.const 0)))))
EOF

    write_case 102_trap_oob_load <<'EOF'
(module
  (memory 1)
  (func (export "main")
    (drop (i32.load (i32.const 65536)))))
EOF
  fi

  RAND_STATE=305419896
  local i
  for ((i = 0; i < FUZZ_CASES; ++i)); do
    local a b m shift expected
    a=$(( $(next_rand) % 10000 ))
    b=$(( $(next_rand) % 10000 ))
    m=$(( ($(next_rand) % 7) + 1 ))
    shift=$(( $(next_rand) % 5 ))
    expected=$(( ((((a + b) * m) ^ (a << shift)) & 0x7fffffff) ))

    write_case "$(printf '200_fuzz_i32_%03d' "${i}")" <<EOF
(module
  (func (export "main")
    (if
      (i32.ne
        (i32.xor
          (i32.mul
            (i32.add (i32.const ${a}) (i32.const ${b}))
            (i32.const ${m}))
          (i32.shl (i32.const ${a}) (i32.const ${shift})))
        (i32.const ${expected}))
      (then unreachable))))
EOF
  done
}

run_and_capture() {
  local out="$1"
  local err="$2"
  shift 2
  set +e
  { "$@" >"${out}" 2>"${err}"; CAPTURE_STATUS=$?; } 2>>"${err}"
  set -e
  return 0
}

classify_exit() {
  local ec="$1"
  if [[ "${ec}" -eq 0 ]]; then
    printf 'ok\n'
  else
    printf 'trap-or-error\n'
  fi
}

classify_wabt_result() {
  local ec="$1"
  local out="$2"
  local err="$3"
  if grep -Eq '=>[[:space:]]*error:' "${out}" "${err}" 2>/dev/null; then
    printf 'trap-or-error\n'
  elif [[ "${ec}" -eq 0 ]]; then
    printf 'ok\n'
  else
    printf 'trap-or-error\n'
  fi
}

run_uwvm_mode() {
  local mode="$1"
  local wasm="$2"
  local out="$3"
  local err="$4"
  case "${mode}" in
    uwvm-int-lazy)
      run_and_capture "${out}" "${err}" "${UWVM_EXE}" --runtime-custom-mode lazy --runtime-custom-compiler int --run "${wasm}"
      ;;
    uwvm-int-full)
      run_and_capture "${out}" "${err}" "${UWVM_EXE}" --runtime-custom-mode full --runtime-custom-compiler int --run "${wasm}"
      ;;
    llvm-jit-lazy)
      run_and_capture "${out}" "${err}" "${UWVM_EXE}" --runtime-custom-mode lazy --runtime-custom-compiler jit --run "${wasm}"
      ;;
    llvm-jit-full)
      run_and_capture "${out}" "${err}" "${UWVM_EXE}" --runtime-custom-mode full --runtime-custom-compiler jit --run "${wasm}"
      ;;
    *)
      echo "ERR: unknown mode: ${mode}" >&2
      exit 2
      ;;
  esac
}

show_failure_context() {
  local case_name="$1"
  local mode="$2"
  local ref_ec="$3"
  local got_ec="$4"
  local ref_class="$5"
  local got_class="$6"
  local wat="$7"
  local ref_err="$8"
  local got_err="$9"

  echo "ERR: backend differential mismatch"
  echo "  case: ${case_name}"
  echo "  mode: ${mode}"
  echo "  wabt exit: ${ref_ec} (${ref_class})"
  echo "  uwvm exit: ${got_ec} (${got_class})"
  echo "  wat: ${wat}"
  echo "  wabt stderr: ${ref_err}"
  echo "  uwvm stderr: ${got_err}"
}

main() {
  cd -- "${ROOT_DIR}"
  setup_sanitizer_env
  resolve_uwvm
  if [[ ! -x "${UWVM_EXE}" ]]; then
    echo "ERR: uwvm executable not found or not executable: ${UWVM_EXE}" >&2
    exit 2
  fi

  ensure_wabt_tools
  generate_corpus

  local modes=(uwvm-int-lazy uwvm-int-full llvm-jit-lazy llvm-jit-full)
  local total=0
  local ok_count=0

  echo "INFO: uwvm = ${UWVM_EXE}"
  echo "INFO: wat2wasm = ${WAT2WASM}"
  echo "INFO: wasm-interp = ${WASM_INTERP}"
  echo "INFO: corpus = ${CORPUS_DIR}"

  local wat
  while IFS= read -r wat; do
    local base wasm ref_out ref_err ref_ec ref_class mode got_out got_err got_ec got_class
    base="$(basename -- "${wat}" .wat)"
    wasm="${WASM_DIR}/${base}.wasm"
    "${WAT2WASM}" "${WABT_MVP_FLAGS[@]}" "${wat}" -o "${wasm}"

    ref_out="${LOG_DIR}/${base}.wabt.out"
    ref_err="${LOG_DIR}/${base}.wabt.err"
    run_and_capture "${ref_out}" "${ref_err}" "${WASM_INTERP}" "${WABT_MVP_FLAGS[@]}" "${wasm}" --run-all-exports
    ref_ec=${CAPTURE_STATUS}
    ref_class="$(classify_wabt_result "${ref_ec}" "${ref_out}" "${ref_err}")"

    for mode in "${modes[@]}"; do
      got_out="${LOG_DIR}/${base}.${mode}.out"
      got_err="${LOG_DIR}/${base}.${mode}.err"
      run_uwvm_mode "${mode}" "${wasm}" "${got_out}" "${got_err}"
      got_ec=${CAPTURE_STATUS}
      got_class="$(classify_exit "${got_ec}")"
      total=$((total + 1))

      if [[ "${ref_class}" != "${got_class}" ]]; then
        show_failure_context "${base}" "${mode}" "${ref_ec}" "${got_ec}" "${ref_class}" "${got_class}" "${wat}" "${ref_err}" "${got_err}"
        exit 1
      fi
      ok_count=$((ok_count + 1))
    done
  done < <(find "${CORPUS_DIR}" -type f -name '*.wat' | sort)

  echo "OK: backend fuzzer matched ${ok_count}/${total} backend runs"
}

main "$@"
