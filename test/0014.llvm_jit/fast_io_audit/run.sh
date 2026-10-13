#!/usr/bin/env bash
# Run only in the isolated SSH Linux container after prepare.py + source transfer.
set -euo pipefail
cd "${1:?source root}"
out=${2:?new output directory}
export UWVM_TEST_CPUSET=${UWVM_TEST_CPUSET:-16-19}
bash tools/ci/require_wasm3_test_cgroup.sh
[[ $(cat /sys/fs/cgroup/memory.max) -le 8589934592 ]]
mkdir "$out"
cp comparison/sources.json "$out/sources.json"
export LD_LIBRARY_PATH="$PWD/host-libs:/toolchain/lib/x86_64-unknown-linux-gnu:/toolchain/lib"
export TMPDIR="$PWD"
export ASAN_OPTIONS=detect_leaks=1:abort_on_error=1
export UBSAN_OPTIONS=halt_on_error=1
ulimit -c 0
includes=(-I"$PWD/src" -I"$PWD/third-parties/fast_io/include" -I"$PWD/third-parties/bizwen/include" -I"$PWD/third-parties/boost_unordered/include")
common=(-std=c++26 -DUWVM_DISABLE_INT -DUWVM_USE_LLVM_JIT -ferror-limit=3)
native=(-stdlib=libc++ -rtlib=compiler-rt -unwindlib=libunwind -fuse-ld=lld -idirafter "$PWD/system/usr/include" -idirafter "$PWD/system/usr/include/x86_64-linux-gnu" -B"$PWD/system/usr/lib/x86_64-linux-gnu" -L"$PWD/system/usr/lib/x86_64-linux-gnu")
run() { local name=$1;shift;printf '%q ' "$@" > "$out/$name.command";printf '\n' >> "$out/$name.command"; if ! "$@" > "$out/$name.log" 2>&1;then cat "$out/$name.log";return 1;fi; }
for profile in o3 sanitize no-exceptions;do
 flags=(-O3)
 [[ $profile != sanitize ]] || flags=(-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer)
 [[ $profile != no-exceptions ]] || flags=(-O2 -fno-exceptions)
 for version in before after;do
  run "$profile-$version-build" /toolchain/bin/clang++ "${common[@]}" "${native[@]}" "${flags[@]}" -I"$PWD/comparison/$version/src" -I"$PWD/comparison/$version" "${includes[@]}" test/0014.llvm_jit/fast_io_audit/codec.cc -o "$out/$profile-$version"
  run "$profile-$version-run" "$out/$profile-$version"
 done
 cmp "$out/$profile-before-run.log" "$out/$profile-after-run.log"
 cat "$out/$profile-after-run.log"
done
for row in s390x-linux-gnu:s390x i686-linux-gnu:i386 aarch64-linux-gnu:aarch64;do
 triple=${row%:*};emulator=${row#*:};sysroot=/work/deps/usr/$triple
 cross=(--target="$triple" --sysroot=/work/deps --gcc-install-dir="/work/deps/usr/lib/gcc-cross/$triple/15" -idirafter "$sysroot/include" -stdlib=libstdc++ -fuse-ld=lld -L"$sysroot/lib" -O2)
 for version in before after;do
  run "$triple-$version-build" /toolchain/bin/clang++ "${common[@]}" "${cross[@]}" -I"$PWD/comparison/$version/src" -I"$PWD/comparison/$version" "${includes[@]}" -S -emit-llvm test/0014.llvm_jit/fast_io_audit/codec.cc -o "$out/$triple-$version.ll"
  run "$triple-$version-object" /work/artifacts/uwvm2-ros-jit/llvm/bin/llc -O2 -filetype=obj -relocation-model=pic "$out/$triple-$version.ll" -o "$out/$triple-$version.o"
  run "$triple-$version-link" /toolchain/bin/clang++ "${cross[@]}" "$out/$triple-$version.o" -o "$out/$triple-$version"
  run "$triple-$version-run" "/work/deps/usr/bin/qemu-$emulator" -U LD_LIBRARY_PATH -L "$sysroot" "$out/$triple-$version"
 done
 cmp "$out/$triple-before-run.log" "$out/$triple-after-run.log"
 cat "$out/$triple-after-run.log"
 run "$triple-float-build" /toolchain/bin/clang++ -std=c++26 "${cross[@]}" "${includes[@]}" -S -emit-llvm test/0017.runtime/strict_float.cc -o "$out/$triple-float.ll"
 run "$triple-float-object" /work/artifacts/uwvm2-ros-jit/llvm/bin/llc -O2 -filetype=obj -relocation-model=pic "$out/$triple-float.ll" -o "$out/$triple-float.o"
 run "$triple-float-link" /toolchain/bin/clang++ "${cross[@]}" "$out/$triple-float.o" -o "$out/$triple-float"
 run "$triple-float-run" "/work/deps/usr/bin/qemu-$emulator" -U LD_LIBRARY_PATH -L "$sysroot" "$out/$triple-float"
done
run native-float-build /toolchain/bin/clang++ -std=c++26 "${native[@]}" -O2 "${includes[@]}" test/0017.runtime/strict_float.cc -o "$out/native-float"
run native-float-run "$out/native-float"
# Interleave old/new batches on one E-core; each executable emits 9 samples.
for batch in 1 2 3;do
 versions=(before after)
 [[ $batch != 2 ]] || versions=(after before)
 for version in "${versions[@]}";do
  run "benchmark-$batch-$version" taskset -c 16 "$out/o3-$version" bench
 done
done
for field in memory.max memory.swap.max memory.peak memory.events cpuset.cpus.effective;do
 cat "/sys/fs/cgroup/$field" > "$out/$field"
done
printf 'PASS old/new equality, sanitizers, endian/ABI matrix, floating-point regressions and benchmarks\n'
