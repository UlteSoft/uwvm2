#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
out=${1:?output directory}; profile=${2:-ubsan}
mkdir -p "$out"; out=$(cd "$out" && pwd)
compiler=${CXX:-clang++}; extra=()
test_source=${UWVM_TEST_SOURCE:-test/0017.runtime/wasm_wait.cc}
test_name=$(basename "$test_source" .cc)
case "$profile" in
 asan) extra=(-fsanitize=address,undefined -fno-sanitize-recover=all) ;; # mmap intentionally unavailable under ASan
 ubsan) extra=(-fsanitize=undefined -fno-sanitize-recover=all) ;; # checks both mmap and allocator
 tsan) extra=(-fsanitize=thread -fno-pie -no-pie) ;;
 *) exit 2 ;;
esac
before=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-before.json")
sha256sum test/0017.runtime/{wasm_wait,keyed_wait_set,shared_memory_size}.cc >"$out/tests.sha256"
command=("$compiler" -std=c++26 -stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind
 -O1 -gline-tables-only -pthread "${extra[@]}" -I src -I third-parties/fast_io/include -I third-parties/bizwen/include
 -I third-parties/boost_unordered/include "$test_source" -o "$out/$test_name")
printf '%q ' "${command[@]}" >"$out/build.command"
"${command[@]}" >"$out/build.log" 2>&1
for attempt in $(seq 1 32); do
 set +e
 TSAN_OPTIONS=external_symbolizer_path=/toolchain/bin/llvm-symbolizer timeout 30 "$out/$test_name" >"$out/run-$attempt.log" 2>&1
 status=$?
 set -e
 if [[ $status == 0 ]]; then cp "$out/run-$attempt.log" "$out/run.log"; break; fi
 # The container forbids personality(ADDR_NO_RANDOMIZE); TSan may fail before main
 # when high-entropy ASLR collides with its shadow. Retry ONLY this diagnosed
 # startup failure, preserving every log. Never retry a race or a test failure.
 if [[ $profile != tsan ]] || ! grep -q 'ThreadSanitizer: encountered an incompatible memory layout' "$out/run-$attempt.log"; then
   cat "$out/run-$attempt.log"; exit "$status"
 fi
 if [[ $attempt == 32 ]]; then cat "$out/run-$attempt.log"; exit "$status"; fi
done
cat "$out/run.log"
after=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-after.json")
[[ $before == "$after" ]]
sha256sum --quiet -c "$out/tests.sha256"
