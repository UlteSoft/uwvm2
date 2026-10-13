#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
out=${1:?output directory}
mkdir -p "$out"
ulimit -c 0
before=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-before.json")
for fixture in value_immediate function_signature local_value_parser external_value_parser; do
    if ! "${CXX:-clang++}" -std=c++26 -stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind \
        -DUWVM_FORCE_DISABLE_MMAP -DUWVM_USE_MULTITHREAD_ALLOCATOR -DUWVM=2 -O0 -g0 \
        -fsanitize=address,undefined -fno-sanitize-recover=all \
        -I src -I third-parties/fast_io/include -I third-parties/bizwen/include -I third-parties/boost_unordered/include \
        "test/0012.validator/wasm3/${fixture}.cc" -o "$out/$fixture" >"$out/$fixture.build.log" 2>&1; then
        tail -n 60 "$out/$fixture.build.log"
        exit 1
    fi
    if ! "$out/$fixture" >"$out/$fixture.run.log" 2>&1; then
        tail -n 60 "$out/$fixture.run.log"
        exit 1
    fi
    cat "$out/$fixture.run.log"
done
after=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-after.json")
test "$before" = "$after"
