#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
out=${1:?output directory}
backend=${2:?backend: int, jit or tiered}
test_source=${UWVM_TEST_SOURCE:-test/0017.runtime/wasm_execution_domain.cc}
mkdir -p "$out"
out=$(cd "$out" && pwd)
compiler=${CXX:-clang++}
ulimit -c 0
before=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-before.json")
flags=(-std=c++26 -stdlib=libc++ -fno-rtti -fasynchronous-unwind-tables -O1 -g0 -Wno-undefined-inline
    -DUWVM=2 -DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1 "-DUWVM2_BUILD_SOURCE_ID=u8\"$before\""
    -I src -I third-parties/bizwen/include -I third-parties/fast_io/include -I third-parties/boost_unordered/include)
case ${UWVM_TEST_MEMORY_BACKEND:-mmap} in
    mmap) ;;
    allocator) flags+=(-DUWVM_FORCE_DISABLE_MMAP -DUWVM_USE_MULTITHREAD_ALLOCATOR) ;;
    *) echo 'invalid memory backend' >&2; exit 2 ;;
esac
libs=(-ldl -pthread)
test_flags=()
if [[ ${UWVM_TEST_EXECUTION_LAZY:-0} == 1 ]]; then test_flags+=(-DUWVM2TEST_EXECUTION_LAZY); fi
case "$backend" in
    int) flags+=(-DUWVM_USE_UWVM_INT -DUWVM_DISABLE_JIT) ;;
    jit|tiered)
        if [[ $backend == tiered ]]; then
            [[ -d src/uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator ]] || {
                echo 'tiered execution is not part of the ROS product' >&2; exit 2;
            }
            flags+=(-DUWVM_USE_UWVM_INT)
            test_flags+=(-DUWVM2TEST_EXECUTION_LAZY -DUWVM2TEST_EXECUTION_TIERED)
        elif [[ ${UWVM_TEST_WITH_INTERPRETER:-0} == 1 ]]; then
            flags+=(-DUWVM_USE_UWVM_INT)
        else
            flags+=(-DUWVM_DISABLE_INT)
        fi
        flags+=(-DUWVM_USE_LLVM_JIT -DUWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519
            -I /work/deps/usr/include -I /work/deps/usr/include/x86_64-linux-gnu)
        if [[ -n ${UWVM_TEST_LLVM_BUILD:-} ]]; then
            [[ -n ${UWVM_TEST_LLVM_SOURCE_INCLUDE:-} && -f "$UWVM_TEST_LLVM_BUILD/consumer-link.rsp" ]]
            flags+=(-I "$UWVM_TEST_LLVM_BUILD/include" -I "$UWVM_TEST_LLVM_SOURCE_INCLUDE")
            libs+=("@$UWVM_TEST_LLVM_BUILD/consumer-link.rsp")
        elif [[ -d src/uwvm2/runtime/compiler/llvm_jit/compile_cu_from_lazy_validator ]]; then
            flags+=(-I /toolchain/include)
            libs+=(-L /toolchain/lib -lLLVM)
        else
            flags+=(-I /work/artifacts/uwvm2-ros-jit/llvm/include -I third-parties/llvm/llvm/include)
            libs+=(@/work/artifacts/uwvm2-ros-jit/llvm/consumer-link.rsp)
        fi
        libs+=(-O1 -L /work/deps/usr/lib/x86_64-linux-gnu -lssl -lcrypto)
        test_flags+=(-DUWVM2TEST_RUNNER_USE_LLVM_JIT -DUWVM2TEST_STRICT_NO_INTERPRETER)
        ;;
    *) exit 2 ;;
esac
command=("$compiler" "${flags[@]}" -c src/uwvm2/runtime/lib/uwvm_runtime.default.cpp -o "$out/runtime.o")
printf '%q ' "${command[@]}" >"$out/runtime.command"
"${command[@]}" >"$out/runtime.build.log" 2>&1
command=("$compiler" "${flags[@]}" "${test_flags[@]}" -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind
    "$test_source" src/uwvm2/uwvm/host_api.default.cpp "$out/runtime.o"
    "${libs[@]}" -o "$out/wasm-execution-domain")
printf '%q ' "${command[@]}" >"$out/test.command"
"${command[@]}" >"$out/test.build.log" 2>&1
if [[ $backend == tiered && $test_source == test/0017.runtime/wasm_execution_domain.cc ]]; then
    for policy in instruction unwind; do
        for tiers in all no-t0 no-t2 no-t0-no-t2; do
            timeout 90 "$out/wasm-execution-domain" "$policy" "$tiers" >"$out/$policy-$tiers.run.log" 2>&1
            cat "$out/$policy-$tiers.run.log"
        done
    done
else
    timeout 90 "$out/wasm-execution-domain" >"$out/test.run.log" 2>&1
    cat "$out/test.run.log"
fi
if [[ $backend == jit && $test_source == test/0017.runtime/wasm_execution_domain.cc ]]; then
    timeout 60 "$out/wasm-execution-domain" unwind >"$out/unwind.run.log" 2>&1
    cat "$out/unwind.run.log"
fi
after=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-after.json")
[[ $before == "$after" ]]
source tools/ci/require_wasm3_test_cgroup.sh
