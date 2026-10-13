#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
output=${1:?usage: build_wasm3_llvm_cli.sh OUTPUT_DIRECTORY}
mkdir -p -- "$output"
output=$(cd -- "$output" && pwd)
compiler=${CXX:-clang++}
llvm_root=${UWVM_TEST_LLVM_ROOT:?set UWVM_TEST_LLVM_ROOT to the LLVM installation}
deps_root=${UWVM_TEST_DEPS_ROOT:?set UWVM_TEST_DEPS_ROOT to the OpenSSL installation}
flags=(-std=c++26 -stdlib=libc++ -fno-rtti -fasynchronous-unwind-tables -O1 -g0 -Wno-undefined-inline
    -DUWVM=2 -DUWVM_USE_LLVM_JIT
    -DUWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519
    -DUWVM_VERSION_X=2 -DUWVM_VERSION_Y=0 -DUWVM_VERSION_Z=4 -DUWVM_VERSION_S=0
    -I "$deps_root/include" -I "$deps_root/include/x86_64-linux-gnu" -I "$llvm_root/include"
    -I src -I third-parties/bizwen/include -I third-parties/fast_io/include -I third-parties/boost_unordered/include)
if [[ ${UWVM_TEST_WASIP1:-0} != 1 ]]; then
    flags+=(-DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1)
fi
# Remote source archives lack Git metadata. Record their actual inputs and verify
# that no source changed while building before publishing a cache-enabled binary.
source_id=$(python3 tools/ci/wasm3_source_fingerprint.py "$PWD" "$output/source-manifest.json")
flags+=("-DUWVM2_BUILD_SOURCE_ID=u8\"$source_id\"")
if [[ ${UWVM_TEST_INTERPRETER:-0} == 1 ]]; then
    flags+=(-DUWVM_USE_UWVM_INT)
else
    flags+=(-DUWVM_DISABLE_INT)
fi
# The selected LLVM installation is built without RTTI. Preserve C++ exceptions for parser error paths.
runtime_command=("$compiler" "${flags[@]}" -c src/uwvm2/runtime/lib/uwvm_runtime.default.cpp -o "$output/runtime.o")
cli_command=("$compiler" "${flags[@]}" -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind \
    src/uwvm2/uwvm/main.default.cpp src/uwvm2/uwvm/host_api.default.cpp "$output/runtime.o" \
    -L "$llvm_root/lib" -lLLVM -L "$deps_root/lib/x86_64-linux-gnu" -lssl -lcrypto -ldl -pthread \
    -o "$output/uwvm")
"${runtime_command[@]}" >"$output/runtime.build.log" 2>&1
"${cli_command[@]}" >"$output/cli.build.log" 2>&1

final_source_id=$(python3 tools/ci/wasm3_source_fingerprint.py "$PWD" "$output/source-manifest-after.json")
if [[ $source_id != "$final_source_id" ]]; then
    mv -- "$output/uwvm" "$output/uwvm-source-changed-invalid"
    echo 'Source inputs changed during compilation; discard this test binary.' >&2
    exit 1
fi
# Preserve the successful exact argv for source-matched optimized rebuilds.
python3 - "$output/runtime.command" "${runtime_command[@]}" <<'PY'
import pathlib, shlex, sys
pathlib.Path(sys.argv[1]).write_text(shlex.join(sys.argv[2:]) + '\n')
PY
python3 - "$output/cli.command" "${cli_command[@]}" <<'PY'
import pathlib, shlex, sys
pathlib.Path(sys.argv[1]).write_text(shlex.join(sys.argv[2:]) + '\n')
PY
