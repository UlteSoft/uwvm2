#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
out=${1:?output directory}; mkdir -p "$out"
ulimit -c 0
before=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-before.json")
for fixture in recursive_type_binary recursive_type_validation local_declarations reference_validation gc_immediate; do
"${CXX:-clang++}" -std=c++26 -stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind \
 -DUWVM_FORCE_DISABLE_MMAP -DUWVM_USE_MULTITHREAD_ALLOCATOR -DUWVM=2 -O1 -g0 \
 -fsanitize=address,undefined -fno-sanitize-recover=all \
 -I src -I third-parties/fast_io/include -I third-parties/bizwen/include -I third-parties/boost_unordered/include \
 test/0012.validator/wasm3/${fixture}.cc -o "$out/$fixture" >"$out/$fixture.build.log" 2>&1
"$out/$fixture" >"$out/$fixture.run.log" 2>&1
cat "$out/$fixture.run.log"
done
after=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-after.json")
[[ $before == "$after" ]]
