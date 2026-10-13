#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
out=${1:?output directory};mkdir -p "$out"
ulimit -c 0
before=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-before.json")
for backend in mmap allocator threaded-allocator; do
 flags=(-fsanitize=address,undefined)
 if [[ $backend == mmap ]]; then flags=(-DUWVM_FORCE_USE_MMAP -DUWVM_TEST_EXPECT_MMAP -fsanitize=undefined); fi
 if [[ $backend != mmap ]]; then flags+=(-DUWVM_FORCE_DISABLE_MMAP); fi
 if [[ $backend == threaded-allocator ]]; then flags+=(-DUWVM_USE_MULTITHREAD_ALLOCATOR); fi
 "${CXX:-clang++}" -std=c++26 -stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind \
  -DUWVM=2 -DUWVM_DISABLE_INT -DUWVM_DISABLE_JIT -DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1 -O1 -g0 \
  -fno-sanitize-recover=all -Wno-undefined-inline "${flags[@]}" \
  -I src -I third-parties/fast_io/include -I third-parties/bizwen/include -I third-parties/boost_unordered/include \
  test/0011.initializer/memory64_initializer.cc -o "$out/$backend" >"$out/$backend.build.log" 2>&1
 "$out/$backend" >"$out/$backend.run.log" 2>&1
 cat "$out/$backend.run.log"
done
 python3 test/0011.initializer/run_memory64_data_wasmtime.py --parser "$out/allocator" \
  --wasmtime /work/artifacts/wasmtime-v48.0.2-x86_64-linux/wasmtime --out "$out/cases" >"$out/parse.run.log" 2>&1
 cat "$out/parse.run.log"
after=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-after.json")
[[ $before == "$after" ]]
