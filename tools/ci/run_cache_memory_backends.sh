#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
out=${1:?output directory};mkdir -p "$out";out=$(cd "$out" && pwd)
compiler=${CXX:-clang++}
before=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-before.json")
for backend in mmap allocator single; do
 flags=()
 if [[ $backend != mmap ]]; then flags+=(-DUWVM_FORCE_DISABLE_MMAP); fi
 if [[ $backend == allocator ]]; then flags+=(-DUWVM_USE_MULTITHREAD_ALLOCATOR); fi
 command=("$compiler" -std=c++26 -stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind -O1 -g0
  -DUWVM=2 -DUWVM_DISABLE_INT -DUWVM_DISABLE_JIT -DUWVM_RUNTIME_LLVM_JIT_CACHE_USE_OPENSSL_ED25519
  -I src -I third-parties/fast_io/include -I third-parties/bizwen/include -I third-parties/boost_unordered/include
  -I /work/deps/usr/include -I /work/deps/usr/include/x86_64-linux-gnu
  "${flags[@]}" test/0014.llvm_jit/cache_product_isolation.cc -L /work/deps/usr/lib/x86_64-linux-gnu -lcrypto -ldl -pthread -o "$out/$backend")
 printf '%q ' "${command[@]}" > "$out/$backend.command"
 "${command[@]}" > "$out/$backend.build.log" 2>&1
done
python3 test/0014.llvm_jit/check_cache_memory_backends.py --mmap "$out/mmap" --allocator "$out/allocator" --single "$out/single" --out "$out/results"
after=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-after.json")
[[ $before == "$after" ]]
