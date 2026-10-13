#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
output=${1:?usage: run_wasm3_memory64_scalar.sh OUTPUT_DIRECTORY [sanitizers|matrix|stress]}
mkdir -p -- "$output"
output=$(cd -- "$output" && pwd)
compiler=${CXX:-clang++}
test_source=${UWVM_TEST_SOURCE:-test/0013.uwvm_int/wasm3/memory64_scalar.cc}
extra=()
case ${UWVM_TEST_MEMORY_BACKEND:-mmap} in
    mmap) extra+=(-DUWVM_FORCE_USE_MMAP) ;;
    allocator) extra+=(-DUWVM_FORCE_DISABLE_MMAP -DUWVM_USE_MULTITHREAD_ALLOCATOR) ;;
    *) echo 'invalid memory backend' >&2; exit 2 ;;
esac
combinations=(none);delays=(none)
if [[ ${2:-} == sanitizers ]]; then
    if [[ ${UWVM_TEST_MEMORY_BACKEND:-mmap} == mmap ]]; then extra+=(-fsanitize=undefined); else extra+=(-fsanitize=address,undefined); fi
    extra+=(-fno-sanitize-recover=all -fno-omit-frame-pointer)
fi
if [[ ${2:-} == matrix ]]; then combinations=(none soft heavy extra); delays=(none soft heavy); fi
if [[ ${2:-} == stress ]]; then combinations=(extra); delays=(heavy); fi
ulimit -c 0
run_configuration() {
    local combine=$1 delay=$2
    local flags=() name
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
            -I third-parties/fast_io/include -I third-parties/boost_unordered/include -I "$output" \
            "${extra[@]}" "${flags[@]}" "$test_source" -o "$output/$name" >"$output/$name.build.log" 2>&1
        "$output/$name" >"$output/$name.run.log" 2>&1
        echo "PASS $name (uncached and register-ring)"
}

matrix_jobs=${UWVM_TEST_MATRIX_JOBS:-1}
[[ $matrix_jobs =~ ^[1-2]$ ]] || { echo "UWVM_TEST_MATRIX_JOBS must be 1 or 2" >&2; exit 1; }
jobs=()
for combine in "${combinations[@]}"; do
    for delay in "${delays[@]}"; do
        run_configuration "$combine" "$delay" &
        jobs+=("$!")
        if (( ${#jobs[@]} == matrix_jobs )); then
            status=0
            for job in "${jobs[@]}"; do wait "$job" || status=$?; done
            (( status == 0 )) || exit "$status"
            jobs=()
        fi
    done
done
status=0
for job in "${jobs[@]}"; do wait "$job" || status=$?; done
exit "$status"
