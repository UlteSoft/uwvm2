#!/usr/bin/env bash
set -euo pipefail

source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"

cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
output=${1:?usage: run_wasm3_extended_const.sh OUTPUT_DIRECTORY [sanitizers]}
mkdir -p -- "$output"
output=$(cd -- "$output" && pwd)
compiler=${CXX:-clang++}
extra=()
if [[ ${2:-} == sanitizers ]]; then extra+=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer); fi

# No NDEBUG: test failures must remain observable in optimized builds.
"$compiler" -std=c++26 -stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind \
    -O1 -g -Wno-undefined-inline -DUWVM=2 -DUWVM_DISABLE_INT -DUWVM_DISABLE_JIT \
    -DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1 -I src -I third-parties/bizwen/include \
    -I third-parties/fast_io/include -I third-parties/boost_unordered/include \
    "${extra[@]}" test/0011.initializer/wasm3_extended_const.cc -o "$output/wasm3_extended_const"
ulimit -c 0
"$output/wasm3_extended_const"
