#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
out=${1:?output directory};mkdir -p "$out"
ulimit -c 0
before=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-before.json")
"${CXX:-clang++}" -std=c++26 -stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind \
 -DUWVM=2 -DUWVM_DISABLE_INT -DUWVM_DISABLE_JIT -O1 -g0 -fsanitize=address,undefined -fno-sanitize-recover=all \
 -I src -I third-parties/fast_io/include -I third-parties/bizwen/include -I third-parties/boost_unordered/include \
 test/0011.initializer/memory64_metadata.cc -o "$out/test" >"$out/build.log" 2>&1
"$out/test" >"$out/run.log" 2>&1
cat "$out/run.log"
after=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-after.json")
[[ $before == "$after" ]]
