#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
out=${1:?output directory}
mkdir -p "$out"
out=$(cd "$out" && pwd)
compiler=${CXX:-clang++}
flags=()
if [[ -d src/uwvm2/runtime/compiler/uwvm_int/compile_cu_from_lazy_validator ]]; then flags+=(-DUWVM2TEST_FENCE_LAZY); fi
# One demanding combine/delay configuration for development; the complete
# configuration/platform matrix belongs to final qualification.
before=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-before.json")
"$compiler" -std=c++26 -stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind \
    -O1 -g0 -Wno-undefined-inline -DUWVM=2 -DUWVM_USE_UWVM_INT -DUWVM_DISABLE_JIT \
    -DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1 -DUWVM_ENABLE_UWVM_INT_COMBINE_OPS \
    -DUWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS -DUWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS \
    -DUWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT -DUWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY \
    -I src -I third-parties/bizwen/include -I third-parties/fast_io/include \
    -I third-parties/boost_unordered/include "${flags[@]}" \
    test/0013.uwvm_int/wasm3/threads_atomic_loads.cc -o "$out/atomic-loads-int" >"$out/int.build.log" 2>&1
"$out/atomic-loads-int" >"$out/int.run.log" 2>&1
cat "$out/int.run.log"
after=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-after.json")
[[ $before == "$after" ]]
