#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
out=${1:?output directory};mkdir -p "$out"
ulimit -c 0
before=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-before.json")
for backend in mmap allocator; do
    flags=()
    if [[ $backend == allocator ]]; then flags+=(-DUWVM_FORCE_DISABLE_MMAP); fi
    "${CXX:-clang++}" -std=c++26 -stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind \
        -O2 -g0 -Wno-undefined-inline -DUWVM=2 -DUWVM_TEST_MEMORY64_ATOMIC -DUWVM_USE_UWVM_INT -DUWVM_DISABLE_JIT \
        -I src -I third-parties/fast_io/include -I third-parties/bizwen/include -I third-parties/boost_unordered/include \
        "${flags[@]}" test/0017.runtime/memory64_integer.cc -o "$out/$backend" >"$out/$backend.build.log" 2>&1
    "$out/$backend" >"$out/$backend.run.log" 2>&1
    cat "$out/$backend.run.log"
done
after=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-after.json")
[[ $before == "$after" ]]
