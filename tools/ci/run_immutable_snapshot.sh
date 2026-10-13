#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
out=${1:?output directory}
mkdir -p "$out"
out=$(cd "$out" && pwd)
compiler=${CXX:-clang++}
sha256sum src/uwvm2/utils/thread/immutable_snapshot.{h,cppm} test/0017.runtime/immutable_snapshot.cc >"$out/source.sha256"
for sanitizer in address,undefined thread; do
    extra=()
    # A fixed main image reduces PIE/ASLR layout collisions under Docker's
    # seccomp policy without changing sandbox permissions or host ASLR settings.
    if [[ $sanitizer == thread ]]; then extra=(-fno-pie -no-pie); fi
    command=("$compiler" "${extra[@]}" -std=c++26 -stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind
        -O1 -g -DNDEBUG -fsanitize="$sanitizer" -fno-sanitize-recover=all -pthread -I src
        test/0017.runtime/immutable_snapshot.cc -o "$out/snapshot-$sanitizer")
    printf '%q ' "${command[@]}" >"$out/$sanitizer.command"
    "${command[@]}" >"$out/$sanitizer.build.log" 2>&1
    timeout 30 "$out/snapshot-$sanitizer" >"$out/$sanitizer.run.log" 2>&1
    cat "$out/$sanitizer.run.log"
done
sha256sum --quiet -c "$out/source.sha256"
