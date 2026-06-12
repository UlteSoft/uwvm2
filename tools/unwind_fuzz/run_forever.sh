#!/usr/bin/env bash
set -euo pipefail

ROOT=${ROOT:-$(pwd)}
UWVM=${UWVM:-"$ROOT/build/macosx/arm64/release/uwvm"}
WAT2WASM=${WAT2WASM:-$(command -v wat2wasm || true)}
WORK_DIR=${WORK_DIR:-"$ROOT/build/unwind-fuzz-work"}
LOG_DIR=${LOG_DIR:-"$ROOT/build/unwind-fuzz-logs"}
MIN_FREE_KB=${MIN_FREE_KB:-1048576}
UNWIND_CASES=${UNWIND_CASES:-512}
OSR_CASES=${OSR_CASES:-256}
LOOP_LIMIT=${LOOP_LIMIT:-0}

if [[ -z "$WAT2WASM" ]]; then
  echo "[fuzz-loop] wat2wasm not found; set WAT2WASM=/path/to/wat2wasm" >&2
  exit 2
fi

if [[ ! -x "$UWVM" ]]; then
  echo "[fuzz-loop] uwvm executable not found: $UWVM" >&2
  echo "[fuzz-loop] build it first, or set UWVM=/absolute/path/to/uwvm" >&2
  exit 2
fi

mkdir -p "$WORK_DIR" "$LOG_DIR"

free_kb() {
  df -Pk "$WORK_DIR" | awk 'NR == 2 { print $4 }'
}

check_space() {
  local free
  free=$(free_kb)
  if (( free < MIN_FREE_KB )); then
    echo "[fuzz-loop] low disk space: ${free}KB free under $WORK_DIR; need ${MIN_FREE_KB}KB" >&2
    exit 3
  fi
}

rand_seed() {
  python3 -c 'import random; print(random.randrange(1 << 32))'
}

loops=0
while true; do
  if (( LOOP_LIMIT > 0 && loops >= LOOP_LIMIT )); then
    echo "[fuzz-loop] reached LOOP_LIMIT=$LOOP_LIMIT"
    exit 0
  fi
  loops=$((loops + 1))

  check_space
  rm -rf "$WORK_DIR"/uwvm_unwind_fuzz_* "$WORK_DIR"/uwvm_osr_unwind_fuzz_*

  seed=$(rand_seed)
  log="$LOG_DIR/unwind-$seed.log"
  echo "[loop] unwind seed=$seed log=$log"
  if ! python3 -u "$ROOT/tools/unwind_fuzz/uwvm_unwind_fuzzer.py" \
      --root "$ROOT" \
      --uwvm "$UWVM" \
      --wat2wasm "$WAT2WASM" \
      --work-dir "$WORK_DIR" \
      --cases "$UNWIND_CASES" \
      --seed "$seed" 2>&1 | tee "$log"; then
    echo "[FOUND] unwind failed seed=$seed log=$log"
    exit 1
  fi

  check_space
  rm -rf "$WORK_DIR"/uwvm_unwind_fuzz_* "$WORK_DIR"/uwvm_osr_unwind_fuzz_*

  seed=$(rand_seed)
  log="$LOG_DIR/osr-$seed.log"
  echo "[loop] osr seed=$seed log=$log"
  if ! python3 -u "$ROOT/tools/unwind_fuzz/uwvm_osr_unwind_fuzzer.py" \
      --root "$ROOT" \
      --uwvm "$UWVM" \
      --wat2wasm "$WAT2WASM" \
      --work-dir "$WORK_DIR" \
      --cases "$OSR_CASES" \
      --seed "$seed" 2>&1 | tee "$log"; then
    echo "[FOUND] osr failed seed=$seed log=$log"
    exit 1
  fi
done
