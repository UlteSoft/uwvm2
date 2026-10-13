#!/usr/bin/env bash
set -euo pipefail
cd "${1:?source root}"
out=${2:?new output directory}
export UWVM_TEST_CPUSET=${UWVM_TEST_CPUSET:-16-19}
bash tools/ci/require_wasm3_test_cgroup.sh
mkdir "$out"
export LD_LIBRARY_PATH="$PWD/host-libs:/toolchain/lib/x86_64-unknown-linux-gnu:/toolchain/lib"
flags=(-std=c++26 -DUWVM_DISABLE_INT -DUWVM_USE_LLVM_JIT -stdlib=libc++ -idirafter "$PWD/system/usr/include" -idirafter "$PWD/system/usr/include/x86_64-linux-gnu" -I"$PWD/src" -I"$PWD/third-parties/fast_io/include" -I"$PWD/third-parties/bizwen/include" -I"$PWD/third-parties/boost_unordered/include")
bindings=()
module() { local name=$1 source=$2 file="$out/${1//:/-}.pcm";shift 2
 local cmd=(/toolchain/bin/clang++ "${flags[@]}" "${bindings[@]}" --precompile "$source" -o "$file")
 printf '%q ' "${cmd[@]}" > "$file.command"
 if ! "${cmd[@]}" > "$file.log" 2>&1;then cat "$file.log";return 1;fi
 bindings+=("-fmodule-file=$name=$file")
}
module fast_io third-parties/fast_io/share/fast_io/fast_io.cppm
for part in prefetch arm_sve;do module "uwvm2.utils.intrinsics:$part" "src/uwvm2/utils/intrinsics/$part.cppm";done
module uwvm2.utils.intrinsics src/uwvm2/utils/intrinsics/impl.cppm
module uwvm2.utils.hash:xxh3 src/uwvm2/utils/hash/xxh3.cppm
module uwvm2.utils.hash src/uwvm2/utils/hash/impl.cppm
for part in allocator wrapper string_concat;do module "uwvm2.utils.container:$part" "src/uwvm2/utils/container/$part.cppm";done
module uwvm2.utils.container src/uwvm2/utils/container/impl.cppm
module uwvm2.runtime.llvm_jit_cache:format src/uwvm2/runtime/llvm_jit_cache/format.cppm
module uwvm2.runtime.llvm_jit_cache:compress src/uwvm2/runtime/llvm_jit_cache/compress.cppm
cat > "$out/consumer.cc" <<'CPP'
#include <cstddef>
#include <cstdint>
import fast_io;
constexpr bool probe() {
    std::byte const bytes[]{std::byte{0x78},std::byte{0x56},std::byte{0x34},std::byte{0x12}};
    std::uint32_t value{};
    auto next=fast_io::freestanding::type_punning_from_bytes(bytes,value);
    return next==bytes+4 && fast_io::little_endian(value)==0x12345678u;
}
static_assert(probe());
CPP
/toolchain/bin/clang++ "${flags[@]}" "${bindings[@]}" -fsyntax-only "$out/consumer.cc" > "$out/consumer.log" 2>&1
printf 'PASS actual cache format/compress modules and constexpr fast_io module consumer\n'
