#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
out=${1:?output directory}; mkdir -p "$out"; out=$(cd "$out" && pwd)
compiler=${CXX:-clang++}
before=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-before.json")
command=("$compiler" -std=c++26 -stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind
    -O1 -g0 -Wno-undefined-inline -DUWVM=2 -DUWVM_DISABLE_INT -DUWVM_DISABLE_JIT
    -DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1 -DUWVM_USE_MULTITHREAD_ALLOCATOR -fsanitize=address,undefined -fno-sanitize-recover=all -pthread
    -I src -I third-parties/bizwen/include -I third-parties/fast_io/include -I third-parties/boost_unordered/include
    test/0011.initializer/threads_shared_memory.cc -o "$out/shared-memory")
printf '%q ' "${command[@]}" >"$out/build.command"
"${command[@]}" >"$out/build.log" 2>&1
timeout 30 "$out/shared-memory" >"$out/run.log" 2>&1
cat "$out/run.log"
after=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-after.json")
[[ $before == "$after" ]]
