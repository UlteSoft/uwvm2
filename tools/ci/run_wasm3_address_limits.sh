#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
out=${1:?output directory}
mkdir -p "$out"
ulimit -c 0
before=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-before.json")
"${CXX:-clang++}" -std=c++26 -stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind \
    -O2 -g0 -I src -I third-parties/fast_io/include \
    test/0012.validator/wasm3/address_limits.cc -o "$out/address-limits" >"$out/build.log" 2>&1
"$out/address-limits" >"$out/run.log" 2>&1
cat "$out/run.log"
after=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-after.json")
[[ $before == "$after" ]]
