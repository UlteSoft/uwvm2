#!/usr/bin/env bash
# Build a real target-host LLVM and VM for QEMU, not just target object files.
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
root=$PWD
output=${1:?usage: build_wasm3_cross_llvm_cli.sh OUTPUT}
mkdir -p -- "$output"
output=$(cd -- "$output" && pwd)
triple=${UWVM_TEST_TARGET_TRIPLE:?set the target triple}
backend=${UWVM_TEST_TARGET_BACKEND:?set the LLVM target backend}
# Debian i386 libraries use i386-linux-gnu while the cross compiler uses i686.
multiarch=${UWVM_TEST_TARGET_MULTIARCH:-$triple}
deps=${UWVM_TEST_DEPS_ROOT:?set the extracted dependency root, e.g. /work/deps}
vendor_repo=${UWVM_TEST_ROS_ROOT:?set the audited ROS source root}
native_llvm=${UWVM_TEST_NATIVE_LLVM:?set the native LLVM build with llvm-tblgen}
target_llvm=${UWVM_TEST_TARGET_LLVM:-$output/llvm}
jobs=${UWVM_TEST_CROSS_JOBS:-4}
[[ $jobs =~ ^[1-4]$ ]] || { echo 'Cross LLVM jobs must be 1..4 inside the shared cgroup' >&2; exit 1; }
compiler=${CXX:-clang++}
c_compiler=${CC:-clang}
cmake=${CMAKE:-cmake}
ninja=${NINJA:-ninja}
sysroot=$deps/usr/$triple
gcc=$deps/usr/lib/gcc-cross/$triple/15
[[ -d $gcc && -d $sysroot/include ]] || { echo 'Missing cross GCC runtime/sysroot' >&2; exit 1; }
# Keep native TableGen and the target SDK on the same patched ROS revision.
# Source/archive provenance is still required; matching metadata alone is not
# evidence that the binary contains the downstream patches.
[[ -x "$native_llvm/bin/llvm-tblgen" ]] || { echo 'Missing native ros.11 llvm-tblgen' >&2; exit 1; }
grep -Fqx '#define LLVM_VERSION_STRING "23.1.1-uwvm-ros.11"' \
    "$native_llvm/include/llvm/Config/llvm-config.h" || { echo 'Wrong native TableGen version header' >&2; exit 1; }
python3 - "$native_llvm/uwvm-llvm-version.json" <<'PY_NATIVE_LLVM_VERSION'
import json, pathlib, sys
metadata = json.loads(pathlib.Path(sys.argv[1]).read_text())
if metadata.get('version') != '23.1.1-uwvm-ros.11':
    raise SystemExit('Wrong native TableGen build version metadata')
PY_NATIVE_LLVM_VERSION
if [[ $backend == RISCV ]]; then
    python3 tools/ci/check_llvm_riscv_probe_cfi.py \
        "$vendor_repo/third-parties/llvm/llvm/lib/Target/RISCV/RISCVFrameLowering.cpp"
fi
(cd "$vendor_repo/third-parties/llvm" && sha256sum -c --quiet sources.sha256)
sha256sum "$vendor_repo/third-parties/llvm/sources.sha256" > "$output/llvm-source-manifest.sha256"
mkdir -p "$target_llvm/.cmake/api/v1/query"
touch "$target_llvm/.cmake/api/v1/query/codemodel-v2"
cross_flags="--gcc-install-dir=$gcc -idirafter $sysroot/include"
# Debian ELFv1 PowerPC CRT objects require their matching GNU linker. Code
# generation remains in Clang/LLVM; this does not substitute a GCC compiler.
linker_flags=(-fuse-ld=lld)
case "$triple" in
    powerpc64-linux-gnu|powerpc-linux-gnu|sparc64-linux-gnu)
        linker_flags=("--ld-path=$deps/usr/bin/$triple-ld") ;;
esac
"$cmake" -S "$vendor_repo/third-parties/llvm/llvm" -B "$target_llvm" -G Ninja \
    -DCMAKE_MAKE_PROGRAM="$ninja" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PROJECT_LLVM_INCLUDE="$vendor_repo/xmake/llvm/contract.cmake" \
    -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR="$backend" \
    -DCMAKE_C_COMPILER="$c_compiler" -DCMAKE_CXX_COMPILER="$compiler" \
    -DCMAKE_C_COMPILER_TARGET="$triple" -DCMAKE_CXX_COMPILER_TARGET="$triple" \
    -DCMAKE_SYSROOT="$deps" -DCMAKE_C_FLAGS="$cross_flags" \
    -DCMAKE_CXX_FLAGS="$cross_flags -stdlib=libstdc++" \
    -DCMAKE_EXE_LINKER_FLAGS="${linker_flags[*]} -L$sysroot/lib" \
    -DCMAKE_SHARED_LINKER_FLAGS="${linker_flags[*]} -L$sysroot/lib" \
    -DCMAKE_MODULE_LINKER_FLAGS="${linker_flags[*]} -L$sysroot/lib" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DLLVM_TARGETS_TO_BUILD="$backend" \
    -DLLVM_HOST_TRIPLE="$triple" -DLLVM_TABLEGEN="$native_llvm/bin/llvm-tblgen" \
    -DLLVM_NATIVE_TOOL_DIR="$native_llvm/bin" \
    -DLLVM_VERSION_SUFFIX=-uwvm-ros.11 -DLLVM_APPEND_VC_REV=OFF \
    -DLLVM_ENABLE_PROJECTS= -DLLVM_ENABLE_RUNTIMES= \
    -DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_BENCHMARKS=OFF -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_INCLUDE_DOCS=OFF -DLLVM_ENABLE_BINDINGS=OFF -DLLVM_BUILD_TOOLS=OFF \
    -DLLVM_TOOL_LLVM_CONFIG_BUILD=OFF -DLLVM_BUILD_UTILS=OFF \
    -DLLVM_BUILD_LLVM_DYLIB=OFF -DLLVM_LINK_LLVM_DYLIB=OFF -DBUILD_SHARED_LIBS=OFF \
    -DLLVM_ENABLE_ZLIB=OFF -DLLVM_ENABLE_ZSTD=OFF -DLLVM_ENABLE_LIBXML2=OFF \
    -DLLVM_ENABLE_CURL=OFF -DLLVM_ENABLE_LIBEDIT=OFF -DLLVM_ENABLE_ASSERTIONS=OFF \
    -DLLVM_ENABLE_EH=OFF -DLLVM_ENABLE_RTTI=OFF -DLLVM_ENABLE_LTO=OFF \
    -DLLVM_PARALLEL_LINK_JOBS=1 -DLLVM_PARALLEL_COMPILE_JOBS="$jobs" \
    > "$output/llvm.configure.log" 2>&1
"$cmake" --build "$target_llvm" --parallel "$jobs" --target uwvm_ros_llvm_contract_dependencies > "$output/llvm.build.log" 2>&1
grep -Fqx '#define LLVM_VERSION_STRING "23.1.1-uwvm-ros.11"' "$target_llvm/include/llvm/Config/llvm-config.h"
python3 - "$target_llvm" <<'PY'
import json, pathlib, shlex, sys
root = pathlib.Path(sys.argv[1]).resolve()
reply = root / '.cmake/api/v1/reply'
index = json.loads(max(reply.glob('index-*.json')).read_text())
model = json.loads((reply / index['reply']['codemodel-v2']['jsonFile']).read_text())
target = next(t for t in model['configurations'][0]['targets'] if t['name'] == 'uwvm_ros_llvm_contract')
data = json.loads((reply / target['jsonFile']).read_text())
args = []
for fragment in data['link']['commandFragments']:
    for token in shlex.split(fragment['fragment']):
        if token.endswith('.a'):
            archive = (root / token).resolve()
            if not archive.is_relative_to(root) or not archive.is_file():
                raise RuntimeError(f'Non-bundled or missing LLVM archive: {archive}')
            token = str(archive)
        args.append(token)
(root / 'consumer-link.rsp').write_text('\n'.join('"' + a.replace('\\', '\\\\').replace('"', '\\"') + '"' for a in args))
PY
if [[ ${UWVM_TEST_LLVM_ONLY:-0} == 1 ]]; then
    (cd "$vendor_repo/third-parties/llvm" && sha256sum -c --quiet sources.sha256)
    sha256sum -c --quiet "$output/llvm-source-manifest.sha256"
    echo "PASS source-matched target LLVM $triple ($backend); VM build deferred" >&2
    exit 0
fi
flags=(--target="$triple" --sysroot="$deps" --gcc-install-dir="$gcc" -idirafter "$sysroot/include"
    -std=c++26 -stdlib=libstdc++ -fno-rtti -fasynchronous-unwind-tables -O1 -g0 -Wno-undefined-inline
    -DUWVM=2 -DUWVM_USE_LLVM_JIT -DUWVM_USE_UWVM_INT -DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1
    -DUWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519
    -DUWVM_VERSION_X=2 -DUWVM_VERSION_Y=0 -DUWVM_VERSION_Z=4 -DUWVM_VERSION_S=0
    -I "$deps/usr/include/$multiarch" -I "$deps/usr/include"
    -I "$target_llvm/include" -I "$vendor_repo/third-parties/llvm/llvm/include"
    -I src -I third-parties/bizwen/include -I third-parties/fast_io/include -I third-parties/boost_unordered/include)
# This recipe verified and built the exact patched ROS dependency above.
# Ordinary uwvm2 still needs the explicit RISC-V TailCC capability; ROS enables
# it through pinned_version.h. Do not silently test an interpreter fallback.
case "$triple" in
    riscv32-*|riscv64-*) flags+=(-DUWVM_LLVM_RISCV_TAILCC_FIXED=1) ;;
esac
source_id=$(python3 tools/ci/wasm3_source_fingerprint.py "$root" "$output/source-manifest.json")
flags+=("-DUWVM2_BUILD_SOURCE_ID=u8\"$source_id\"")
"$compiler" "${flags[@]}" -c src/uwvm2/runtime/lib/uwvm_runtime.default.cpp -o "$output/runtime.o" > "$output/runtime.build.log" 2>&1
# The CMake link contract also carries Release driver flags (-O3). Restore the
# test build optimization after that response file so it cannot silently override
# -O1 and exhaust the aggregate cgroup during CLI frontend optimization.
"$compiler" "${flags[@]}" "${linker_flags[@]}" \
    src/uwvm2/uwvm/main.default.cpp src/uwvm2/uwvm/host_api.default.cpp "$output/runtime.o" \
    "@$target_llvm/consumer-link.rsp" -O1 -L "$deps/usr/lib/$multiarch" -L "$sysroot/lib" \
    -lssl -lcrypto -latomic -ldl -pthread -o "$output/uwvm" > "$output/cli.build.log" 2>&1
(cd "$vendor_repo/third-parties/llvm" && sha256sum -c --quiet sources.sha256)
final_source_id=$(python3 tools/ci/wasm3_source_fingerprint.py "$root" "$output/source-manifest-after.json")
if [[ $source_id != "$final_source_id" ]] || ! sha256sum -c --quiet "$output/llvm-source-manifest.sha256"; then
    mv -- "$output/uwvm" "$output/uwvm-source-changed-invalid"
    echo 'Source inputs changed during cross compilation; discard the binary.' >&2
    exit 1
fi
