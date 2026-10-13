#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
root=$PWD
output=${1:?usage: build_wasm3_llvm_cli.sh OUTPUT_DIRECTORY}
mkdir -p -- "$output"
output=$(cd -- "$output" && pwd)
compiler=${CXX:-clang++}
c_compiler=${CC:-clang}
deps_root=${UWVM_TEST_DEPS_ROOT:?set UWVM_TEST_DEPS_ROOT to the extracted build dependencies}
cmake=${CMAKE:-cmake}
ninja=${NINJA:-ninja}
bundled=${UWVM_TEST_ROS_LLVM_BUILD:-$output/llvm}
mkdir -p "$bundled/.cmake/api/v1/query"
touch "$bundled/.cmake/api/v1/query/codemodel-v2"
# The source manifest and repository's own CMake contract enforce the exact patched release.
(cd third-parties/llvm && sha256sum -c --quiet sources.sha256)
"$cmake" -S third-parties/llvm/llvm -B "$bundled" -G Ninja \
    -DCMAKE_MAKE_PROGRAM="$ninja" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PROJECT_LLVM_INCLUDE="$root/xmake/llvm/contract.cmake" \
    -DCMAKE_C_COMPILER="$c_compiler" -DCMAKE_CXX_COMPILER="$compiler" \
    -DCMAKE_CXX_FLAGS=-stdlib=libc++ \
    '-DCMAKE_EXE_LINKER_FLAGS=-stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind' \
    '-DCMAKE_SHARED_LINKER_FLAGS=-stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind' \
    '-DCMAKE_MODULE_LINKER_FLAGS=-stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind' \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DLLVM_TARGETS_TO_BUILD="${UWVM_TEST_LLVM_TARGETS:-X86}" \
    -DLLVM_VERSION_SUFFIX=-uwvm-ros.13 -DLLVM_APPEND_VC_REV=OFF \
    -DLLVM_ENABLE_PROJECTS= -DLLVM_ENABLE_RUNTIMES= \
    -DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_BENCHMARKS=OFF -DLLVM_INCLUDE_EXAMPLES=OFF \
    -DLLVM_INCLUDE_DOCS=OFF -DLLVM_ENABLE_BINDINGS=OFF -DLLVM_BUILD_TOOLS=OFF \
    -DLLVM_TOOL_LLVM_CONFIG_BUILD=OFF -DLLVM_BUILD_UTILS=OFF \
    -DLLVM_BUILD_LLVM_DYLIB=OFF -DLLVM_LINK_LLVM_DYLIB=OFF -DBUILD_SHARED_LIBS=OFF \
    -DLLVM_ENABLE_ZLIB=OFF -DLLVM_ENABLE_ZSTD=OFF -DLLVM_ENABLE_LIBXML2=OFF \
    -DLLVM_ENABLE_CURL=OFF -DLLVM_ENABLE_LIBEDIT=OFF -DLLVM_ENABLE_ASSERTIONS=OFF \
    -DLLVM_ENABLE_EH=OFF -DLLVM_ENABLE_RTTI=OFF -DLLVM_ENABLE_LTO=OFF \
    -DLLVM_PARALLEL_LINK_JOBS=1 -DLLVM_PARALLEL_COMPILE_JOBS=16 >"$output/llvm.configure.log" 2>&1
"$cmake" --build "$bundled" --parallel 16 --target llvm-libraries >"$output/llvm.build.log" 2>&1
grep -Fqx '#define LLVM_VERSION_STRING "23.1.1-uwvm-ros.13"' "$bundled/include/llvm/Config/llvm-config.h"
# Consume CMake's complete dependency order; never substitute a system LLVM or infer archive names.
python3 - "$bundled" <<'PY'
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
flags=(-std=c++26 -stdlib=libc++ -fno-rtti -fasynchronous-unwind-tables -O1 -g0 -Wno-undefined-inline
    -DUWVM=2 -DUWVM_USE_LLVM_JIT
    -DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1 -DUWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519
    -DUWVM_VERSION_X=2 -DUWVM_VERSION_Y=0 -DUWVM_VERSION_Z=4 -DUWVM_VERSION_S=0
    -I "$deps_root/include" -I "$deps_root/include/x86_64-linux-gnu"
    -I "$bundled/include" -I third-parties/llvm/llvm/include
    -I src -I third-parties/bizwen/include -I third-parties/fast_io/include -I third-parties/boost_unordered/include)
# Remote source archives lack Git metadata. Record their actual inputs and verify
# that no source changed while building before publishing a cache-enabled binary.
source_id=$(python3 tools/ci/wasm3_source_fingerprint.py "$PWD" "$output/source-manifest.json")
flags+=("-DUWVM2_BUILD_SOURCE_ID=u8\"$source_id\"")
if [[ ${UWVM_TEST_INTERPRETER:-0} == 1 ]]; then
    flags+=(-DUWVM_USE_UWVM_INT)
else
    flags+=(-DUWVM_DISABLE_INT)
fi
"$compiler" "${flags[@]}" -c src/uwvm2/runtime/lib/uwvm_runtime.default.cpp -o "$output/runtime.o" >"$output/runtime.build.log" 2>&1
"$compiler" "${flags[@]}" -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind \
    src/uwvm2/uwvm/main.default.cpp src/uwvm2/uwvm/host_api.default.cpp "$output/runtime.o" \
    "@$bundled/consumer-link.rsp" -L "$deps_root/lib/x86_64-linux-gnu" -lssl -lcrypto -ldl -pthread \
    -o "$output/uwvm" >"$output/cli.build.log" 2>&1

final_source_id=$(python3 tools/ci/wasm3_source_fingerprint.py "$PWD" "$output/source-manifest-after.json")
if [[ $source_id != "$final_source_id" ]]; then
    mv -- "$output/uwvm" "$output/uwvm-source-changed-invalid"
    echo 'Source inputs changed during compilation; discard this test binary.' >&2
    exit 1
fi
