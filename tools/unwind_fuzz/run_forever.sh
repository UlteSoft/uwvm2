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
KEEP_PASS_LOGS=${KEEP_PASS_LOGS:-0}

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

prune_pass_logs() {
  local prefix=$1
  if (( KEEP_PASS_LOGS <= 0 )); then
    rm -f "$LOG_DIR"/"$prefix"-*.pass.log "$LOG_DIR"/"$prefix"-*.log
    return
  fi

  python3 - "$LOG_DIR" "$prefix" "$KEEP_PASS_LOGS" <<'PY'
import pathlib
import sys

log_dir = pathlib.Path(sys.argv[1])
prefix = sys.argv[2]
keep = int(sys.argv[3])
logs = sorted(
    log_dir.glob(f"{prefix}-*.pass.log"),
    key=lambda path: path.stat().st_mtime,
    reverse=True,
)
for path in logs[keep:]:
    path.unlink(missing_ok=True)
PY
}

cleanup_pass_artifacts() {
  rm -rf "$WORK_DIR"/uwvm_unwind_fuzz_* "$WORK_DIR"/uwvm_osr_unwind_fuzz_*
  prune_pass_logs unwind
  prune_pass_logs osr
}

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

  cleanup_pass_artifacts
  check_space

  seed=$(rand_seed)
  log="$LOG_DIR/unwind-$seed.pass.log"
  fail_log="$LOG_DIR/unwind-$seed.fail.log"
  echo "[loop] unwind seed=$seed log=$log"
  if ! python3 -u "$ROOT/tools/unwind_fuzz/uwvm_unwind_fuzzer.py" \
      --root "$ROOT" \
      --uwvm "$UWVM" \
      --wat2wasm "$WAT2WASM" \
      --work-dir "$WORK_DIR" \
      --cases "$UNWIND_CASES" \
      --seed "$seed" 2>&1 | tee "$log"; then
    mv -f "$log" "$fail_log"
    echo "[FOUND] unwind failed seed=$seed log=$fail_log"
    exit 1
  fi
  prune_pass_logs unwind

  cleanup_pass_artifacts
  check_space

  seed=$(rand_seed)
  log="$LOG_DIR/osr-$seed.pass.log"
  fail_log="$LOG_DIR/osr-$seed.fail.log"
  echo "[loop] osr seed=$seed log=$log"
  if ! python3 -u "$ROOT/tools/unwind_fuzz/uwvm_osr_unwind_fuzzer.py" \
      --root "$ROOT" \
      --uwvm "$UWVM" \
      --wat2wasm "$WAT2WASM" \
      --work-dir "$WORK_DIR" \
      --cases "$OSR_CASES" \
      --seed "$seed" 2>&1 | tee "$log"; then
    mv -f "$log" "$fail_log"
    echo "[FOUND] osr failed seed=$seed log=$fail_log"
    exit 1
  fi
  prune_pass_logs osr
done
