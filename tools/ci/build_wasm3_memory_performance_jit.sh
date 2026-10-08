#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
root=$PWD
baseline=${1:?usage: build_wasm3_memory_performance_jit.sh HEAD_SOURCE_DIRECTORY OUTPUT_DIRECTORY [both|current]}
output=${2:?output directory}
mkdir -p "$output/full-pbo3" "$baseline/test/0013.uwvm_int/wasm3"
output=$(cd "$output" && pwd)
baseline=$(cd "$baseline" && pwd)
cp test/0013.uwvm_int/wasm3/memory_performance_jit.cc "$baseline/test/0013.uwvm_int/wasm3/"
# Publish shared test headers atomically for concurrent frontend readers.
for header in memory_performance_reference.h memory_performance_timing.h; do
    reference_tmp=$(mktemp "$baseline/test/0013.uwvm_int/wasm3/.reference.XXXXXX")
    cp "test/0013.uwvm_int/wasm3/$header" "$reference_tmp"
    mv -f "$reference_tmp" "$baseline/test/0013.uwvm_int/wasm3/$header"
done
compiler=${CXX:-clang++}
versions=(baseline current)
case ${3:-both} in
    both) ;;
    current) versions=(current) ;;
    *) echo 'Build selection must be both or current' >&2; exit 2 ;;
esac
before=$(python3 tools/ci/wasm3_source_fingerprint.py "$root" "$output/source-manifest.json")
deps=${UWVM_TEST_DEPS_ROOT:?extracted dependencies}
includes=()
links=()
if [[ -n ${UWVM_TEST_ROS_LLVM_BUILD:-} ]]; then
    bundled=$UWVM_TEST_ROS_LLVM_BUILD
    # Reuse this task's completed bundled library build, not its archived .o
    # intermediates. This is a consumer build and never reconfigures LLVM.
    (cd third-parties/llvm && sha256sum -c --quiet sources.sha256)
    python3 - "$bundled" <<'PY'
import json, pathlib, shlex, sys
root = pathlib.Path(sys.argv[1]).resolve()
metadata = json.loads((root / 'uwvm-llvm-version.json').read_text())
if metadata['version'] != '23.1.1-uwvm-ros.9':
    raise SystemExit('wrong bundled LLVM patch version')
for token in shlex.split((root / 'consumer-link.rsp').read_text()):
    if token.endswith('.a') and (not pathlib.Path(token).is_relative_to(root) or not pathlib.Path(token).is_file()):
        raise SystemExit('missing or foreign LLVM archive: ' + token)
PY
    includes+=(-I "$bundled/include" -I "$root/third-parties/llvm/llvm/include")
    links+=("@$bundled/consumer-link.rsp")
else
    llvm_root=${UWVM_TEST_LLVM_ROOT:?LLVM installation}
    includes+=(-I "$llvm_root/include")
    links+=(-L "$llvm_root/lib" -lLLVM)
fi
"$compiler" --version >"$output/compiler.txt"
for version in "${versions[@]}"; do
    source_root=$root
    definitions=()
    if [[ $version == baseline ]]; then source_root=$baseline; definitions+=(-DUWVM2TEST_BASELINE); fi
    version_source_id=$(python3 tools/ci/wasm3_source_fingerprint.py "$source_root" "$output/$version-source-manifest.json")
    flags=("-DUWVM2_BUILD_SOURCE_ID=u8\"$version_source_id\"" -std=c++26 -stdlib=libc++ -fno-rtti -fasynchronous-unwind-tables -O1 -g0 -Wno-undefined-inline
        -DUWVM=2 -DUWVM_USE_LLVM_JIT -DUWVM_DISABLE_INT -DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1
        -DUWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519
        -I "$source_root/src" -I "$root/third-parties/bizwen/include"
        -I "$root/third-parties/fast_io/include" -I "$root/third-parties/boost_unordered/include"
        -I "$deps/include" -I "$deps/include/x86_64-linux-gnu" "${includes[@]}" "${definitions[@]}")
    command=("$compiler" "${flags[@]}" -c "$source_root/src/uwvm2/runtime/lib/uwvm_runtime.default.cpp"
        -o "$output/full-pbo3/$version.runtime.o")
    printf '%q ' "${command[@]}" >"$output/full-pbo3/$version.runtime.command"
    if [[ -n ${UWVM_TEST_JIT_RUNTIME_REUSE:-} ]]; then
        # Reuse only an identical compiler invocation over the same verified
        # production tree. Harness-only diagnostics do not require rebuilding
        # the runtime. Keep the original object and its provenance intact.
        python3 - "$UWVM_TEST_JIT_RUNTIME_REUSE" "$output" "$version" <<'REUSE'
import hashlib, json, pathlib, shlex, shutil, sys
old, new = map(pathlib.Path, sys.argv[1:3])
version = sys.argv[3]
name = version + '.runtime.command'
a = shlex.split((old/'full-pbo3'/name).read_text())
b = shlex.split((new/'full-pbo3'/name).read_text())
if a[-2] != '-o' or b[-2] != '-o' or a[:-1] != b[:-1]:
    raise SystemExit('refuse runtime reuse: compiler arguments changed')
if (old/'compiler.txt').read_bytes() != (new/'compiler.txt').read_bytes():
    raise SystemExit('refuse runtime reuse: compiler identity changed')
if json.loads((old/(version+'-source-manifest-after.json')).read_text()) != json.loads((new/(version+'-source-manifest.json')).read_text()):
    raise SystemExit('refuse runtime reuse: production source manifest changed')
source, target = pathlib.Path(a[-1]), pathlib.Path(b[-1])
digest = hashlib.sha256(source.read_bytes()).hexdigest()
shutil.copyfile(source, target)
if hashlib.sha256(target.read_bytes()).hexdigest() != digest:
    raise SystemExit('runtime object copy did not preserve its hash')
(new/'full-pbo3'/(version+'.runtime-reuse.json')).write_text(json.dumps(
    dict(source=str(source), sha256=digest, command=a, checked_source_manifest=str(old/(version+'-source-manifest-after.json'))), indent=2)+'\n')
REUSE
    else
        "${command[@]}" >"$output/full-pbo3/$version.runtime.build.log" 2>&1
    fi
    command=("$compiler" "${flags[@]}" -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind
        "$source_root/test/0013.uwvm_int/wasm3/memory_performance_jit.cc"
        "$source_root/src/uwvm2/uwvm/host_api.default.cpp" "$output/full-pbo3/$version.runtime.o"
        "${links[@]}" -O1 -L "$deps/lib/x86_64-linux-gnu" -lssl -lcrypto -ldl -pthread
        -o "$output/full-pbo3/$version")
    # -O1 AFTER the CMake response prevents its LLVM Release -O3 from changing
    # host compilation. The generated Wasm code explicitly uses PassBuilder O3.
    printf '%q ' "${command[@]}" >"$output/full-pbo3/$version.command"
    "${command[@]}" >"$output/full-pbo3/$version.build.log" 2>&1
    version_source_after=$(python3 tools/ci/wasm3_source_fingerprint.py "$source_root" "$output/$version-source-manifest-after.json")
    [[ $version_source_id == "$version_source_after" ]] || { echo 'Source changed during compilation' >&2; exit 1; }
    echo "BUILT full-pbo3/$version"
done
after=$(python3 tools/ci/wasm3_source_fingerprint.py "$root" "$output/source-manifest-after.json")
[[ $before == "$after" ]] || { echo 'Source changed during compilation; reject this build' >&2; exit 1; }
