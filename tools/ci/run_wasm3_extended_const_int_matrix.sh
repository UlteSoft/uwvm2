#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
output=${1:?usage: run_wasm3_extended_const_int_matrix.sh OUTPUT_DIRECTORY}
mkdir -p -- "$output"
output=$(cd -- "$output" && pwd)
compiler=${CXX:-clang++}
ulimit -c 0

for combine in none soft heavy extra; do
    for delay in none soft heavy; do
        flags=()
        if [[ $combine != none ]]; then flags+=(-DUWVM_ENABLE_UWVM_INT_COMBINE_OPS); fi
        if [[ $combine == heavy || $combine == extra ]]; then flags+=(-DUWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS); fi
        if [[ $combine == extra ]]; then flags+=(-DUWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS); fi
        if [[ $delay != none ]]; then flags+=(-DUWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT); fi
        if [[ $delay == heavy ]]; then flags+=(-DUWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY); fi
        name="combine-$combine-delay-$delay"
        "$compiler" -std=c++26 -stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind \
            -O2 -g0 -Wno-undefined-inline -DUWVM=2 -DUWVM_USE_UWVM_INT -DUWVM_DISABLE_JIT \
            -DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1 -I src -I third-parties/bizwen/include \
            -I third-parties/fast_io/include -I third-parties/boost_unordered/include \
            "${flags[@]}" test/0013.uwvm_int/wasm3/extended_const.cc -o "$output/$name" >"$output/$name.build.log" 2>&1
        "$output/$name" >"$output/$name.run.log" 2>&1
        echo "PASS $name (uncached and register-ring)"
    done
done
