#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
out=${1:?output directory}
mkdir -p "$out"
out=$(cd "$out" && pwd)
compiler=${CXX:-clang++}
# Small concurrent development test. Keep checks active in release builds too.
command=("$compiler" -std=c++26 -stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind
    -O1 -g -DNDEBUG -fsanitize=address,undefined -fno-sanitize-recover=all -pthread
    -I src test/0017.runtime/keyed_wait_set.cc -o "$out/keyed-wait-set")
printf '%q ' "${command[@]}" >"$out/build.command"
sha256sum src/uwvm2/utils/thread/{keyed_wait_set,execution_lifetime,execution_domain}.h \
    src/uwvm2/utils/macro/push_macros.h src/uwvm2/utils/macro/pop_macros.h \
    test/0017.runtime/{keyed_wait_set,execution_domain}.cc >"$out/source.sha256"
"${command[@]}" >"$out/build.log" 2>&1
timeout 30 "$out/keyed-wait-set" >"$out/run.log" 2>&1
sha256sum --quiet -c "$out/source.sha256"
cat "$out/run.log"
command[${#command[@]}-3]=test/0017.runtime/execution_domain.cc
command[${#command[@]}-1]="$out/execution-domain"
printf '%q ' "${command[@]}" >"$out/domain-build.command"
"${command[@]}" >"$out/domain-build.log" 2>&1
timeout 30 "$out/execution-domain" >"$out/domain-run.log" 2>&1
sha256sum --quiet -c "$out/source.sha256"
cat "$out/domain-run.log"
