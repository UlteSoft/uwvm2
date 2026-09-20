#!/usr/bin/env bash
set -euo pipefail

# Integration test only: invoke explicitly after a full LLVM-lazy binary exists.
# The small standalone llvm_lazy_worker_policy.cc test needs neither LLVM nor
# this guest. All execution here is serial and uses no extra compile worker.
if [[ $# != 1 || $1 != /* || ! -x $1 ]]; then
    printf 'usage: bash %s /absolute/path/to/full/uwvm\n' "$0" >&2
    exit 2
fi
command -v wat2wasm >/dev/null 2>&1 || { echo 'wat2wasm is required' >&2; exit 2; }
command -v rg >/dev/null 2>&1 || { echo 'rg is required' >&2; exit 2; }
command -v python3 >/dev/null 2>&1 || { echo 'python3 is required' >&2; exit 2; }

test_binary=$1
test_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
test_output=$(mktemp -d /tmp/uwvm-llvm-lazy-zero-workers.XXXXXX)
# Retain artifacts on either outcome so failed demand/worker evidence survives.
printf 'LLVM-lazy zero-worker artifacts: %s\n' "$test_output"
wat2wasm "$test_dir/fixtures/llvm_lazy_large_demand.wat" -o "$test_output/large-demand.wasm"

python3 - "${UWVM_TEST_TIMEOUT_SECONDS:-60}" "$test_binary" --runtime-custom-mode lazy --runtime-custom-compiler jit \
    --runtime-compile-threads 0 --runtime-llvm-jit-call-stack instruction \
    --runtime-llvm-jit-cache-path disable --wasip1-noinherit-system-environment \
    --log-verbose --runtime-compiler-log file "$test_output/compiler.log" \
    --run "$test_output/large-demand.wasm" >"$test_output/stdout.log" 2>"$test_output/stderr.log" <<'PY'
import math
import subprocess
import sys

try:
    timeout = float(sys.argv[1])
    if not math.isfinite(timeout) or timeout <= 0:
        raise ValueError
except ValueError:
    print("UWVM_TEST_TIMEOUT_SECONDS must be a finite positive number", file=sys.stderr)
    sys.exit(2)
try:
    result = subprocess.run(sys.argv[2:], check=False, timeout=timeout)
except subprocess.TimeoutExpired:
    # subprocess.run kills and reaps the timed-out runtime before returning.
    print(f"LLVM-lazy zero-worker test timed out after {timeout:g}s", file=sys.stderr)
    sys.exit(124)
sys.exit(result.returncode)
PY

# A successful guest result alone is insufficient: prove the oversized cold CU
# reached the demand gate and selected the caller-thread lane, not a worker.
rg -q '\[llvm-jit-lazy\] demand-request .* fn=1 .* size=4101 .* state=uncompiled .* lane=inline$' \
    "$test_output/compiler.log"
rg -q '\[llvm-jit-lazy\] compile-end .* fn=1 .* state=compiled' "$test_output/compiler.log"
rg -q '\[llvm-jit-lazy\] scheduler .* inline_compiles=[1-9][0-9]* worker_compiles=0 ' "$test_output/compiler.log"
if rg -n 'lane=urgent|llvm-urgent-scheduler|Registered LLVM JIT lazy urgent scheduler' \
    "$test_output/compiler.log" "$test_output/stdout.log" "$test_output/stderr.log"; then
    echo 'FAIL: -Rct 0 started or used the LLVM-lazy urgent worker' >&2
    exit 1
fi
echo 'PASS: oversized LLVM-lazy demand compiled on the caller thread with -Rct 0'
