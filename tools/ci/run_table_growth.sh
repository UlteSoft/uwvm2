#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
out=${1:?output directory};mkdir -p "$out";out=$(cd "$out" && pwd)
before=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-before.json")
"${CXX:-clang++}" -std=c++26 -stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind -O2 -g0 \
 -fsanitize=address,undefined -fno-sanitize-recover=all -I src -I third-parties/fast_io/include \
 -I third-parties/bizwen/include -I third-parties/boost_unordered/include test/0017.runtime/table_growth.cc -o "$out/test" >"$out/build.log" 2>&1
ASAN_OPTIONS=allocator_may_return_null=1 "$out/test" >"$out/run.log" 2>&1
cat "$out/run.log"
after=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-after.json")
[[ $before == "$after" ]]
source tools/ci/require_wasm3_test_cgroup.sh
