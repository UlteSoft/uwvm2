#!/usr/bin/env bash
# Rerun timing independently from the correctness/compiler matrix.
set -euo pipefail
cd "${1:?source root}"
out=${2:?new output directory}
export UWVM_TEST_CPUSET=${UWVM_TEST_CPUSET:-16-19}
bash tools/ci/require_wasm3_test_cgroup.sh
mkdir "$out"
export LD_LIBRARY_PATH="$PWD/host-libs:/toolchain/lib/x86_64-unknown-linux-gnu:/toolchain/lib"
for version in before after;do
 cmd=(/toolchain/bin/clang++ -std=c++26 -O3 -DUWVM_DISABLE_INT -DUWVM_USE_LLVM_JIT -stdlib=libc++ -rtlib=compiler-rt -unwindlib=libunwind -fuse-ld=lld
  -idirafter "$PWD/system/usr/include" -idirafter "$PWD/system/usr/include/x86_64-linux-gnu" -B"$PWD/system/usr/lib/x86_64-linux-gnu" -L"$PWD/system/usr/lib/x86_64-linux-gnu"
  -I"$PWD/comparison/$version/src" -I"$PWD/comparison/$version" -I"$PWD/src" -I"$PWD/third-parties/fast_io/include" -I"$PWD/third-parties/bizwen/include" -I"$PWD/third-parties/boost_unordered/include"
  test/0014.llvm_jit/fast_io_audit/codec.cc -o "$out/$version")
 printf '%q ' "${cmd[@]}" > "$out/$version-build.command"
 "${cmd[@]}" > "$out/$version-build.log" 2>&1
done
for batch in 1 2 3;do
 versions=(before after);[[ $batch != 2 ]] || versions=(after before)
 for version in "${versions[@]}";do
  taskset -c 16 "$out/$version" bench > "$out/benchmark-$batch-$version.log"
 done
done
printf 'PASS CPU-time benchmark, wall times retained\n'
