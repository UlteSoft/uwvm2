#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
root=$PWD
baseline=${1:?usage: build_wasm3_memory_performance.sh HEAD_SOURCE_DIRECTORY OUTPUT_DIRECTORY [matrix] [generic|native]}
output=${2:?output directory}
mkdir -p "$output" "$baseline/test/0013.uwvm_int/wasm3"
output=$(cd "$output" && pwd)
baseline=$(cd "$baseline" && pwd)
cp test/0013.uwvm_int/wasm3/memory_performance.cc "$baseline/test/0013.uwvm_int/wasm3/"
# Publish shared test headers atomically for concurrent frontend readers.
for header in memory_performance_reference.h memory_performance_timing.h; do
    reference_tmp=$(mktemp "$baseline/test/0013.uwvm_int/wasm3/.reference.XXXXXX")
    cp "test/0013.uwvm_int/wasm3/$header" "$reference_tmp"
    mv -f "$reference_tmp" "$baseline/test/0013.uwvm_int/wasm3/$header"
done
compiler=${CXX:-clang++}
target_flags=()
case ${4:-generic} in
    generic) ;;
    native) target_flags+=(-march=native) ;;
    *) echo 'Target profile must be generic or native' >&2; exit 2 ;;
esac
fingerprint=(python3 tools/ci/wasm3_source_fingerprint.py)
before=$("${fingerprint[@]}" "$root" "$output/source-manifest.json")
baseline_before=$("${fingerprint[@]}" "$baseline" "$output/baseline-source-manifest.json")
sha256sum test/0013.uwvm_int/wasm3/memory_performance.cc >"$output/harness.sha256"
combinations=(none); delays=(none)
if [[ ${3:-} == matrix ]]; then combinations=(none soft heavy extra); delays=(none soft heavy); fi
"$compiler" --version >"$output/compiler.txt"
for combine in "${combinations[@]}"; do
    for delay in "${delays[@]}"; do
        flags=()
        if [[ $combine != none ]]; then flags+=(-DUWVM_ENABLE_UWVM_INT_COMBINE_OPS); fi
        if [[ $combine == heavy || $combine == extra ]]; then flags+=(-DUWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS); fi
        if [[ $combine == extra ]]; then flags+=(-DUWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS); fi
        if [[ $delay != none ]]; then flags+=(-DUWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT); fi
        if [[ $delay == heavy ]]; then flags+=(-DUWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY); fi
        name="combine-$combine-delay-$delay"
        mkdir -p "$output/$name"
        for version in baseline current; do
            source_root=$root
            definitions=()
            if [[ $version == baseline ]]; then source_root=$baseline; definitions+=(-DUWVM2TEST_BASELINE); fi
            command=("$compiler" -std=c++26 -stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind
                -O3 -g0 -Wno-undefined-inline -DUWVM=2 -DUWVM_USE_UWVM_INT -DUWVM_DISABLE_JIT
                -DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1 -I "$source_root/src" -I "$root/third-parties/bizwen/include"
                -I "$root/third-parties/fast_io/include" -I "$root/third-parties/boost_unordered/include"
                "${target_flags[@]}" "${flags[@]}" "${definitions[@]}" "$source_root/test/0013.uwvm_int/wasm3/memory_performance.cc"
                -o "$output/$name/$version")
            printf '%q ' "${command[@]}" >"$output/$name/$version.command"
            "${command[@]}" >"$output/$name/$version.build.log" 2>&1
            echo "BUILT $name/$version"
        done
    done
done
after=$("${fingerprint[@]}" "$root" "$output/source-manifest-after.json")
baseline_after=$("${fingerprint[@]}" "$baseline" "$output/baseline-source-manifest-after.json")
[[ $before == "$after" && $baseline_before == "$baseline_after" ]] || {
    echo 'Source changed during compilation; reject this performance build' >&2; exit 1;
}
