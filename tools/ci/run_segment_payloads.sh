#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
out=${1:?output directory};mkdir -p "$out";out=$(cd "$out" && pwd)
ulimit -c 0
before=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-before.json")
sanitizers=(address,undefined thread)
if [[ ${UWVM_TEST_SEGMENT_PROFILE:-all} == tsan ]]; then sanitizers=(thread); fi
for sanitizer in "${sanitizers[@]}"; do
    extra=()
    if [[ $sanitizer == thread ]]; then extra=(-fno-pie -no-pie); fi
    for payload in data element; do
        command=("${CXX:-clang++}" -std=c++26 -stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind
            -O1 -g -fsanitize="$sanitizer" -fno-sanitize-recover=all -pthread "${extra[@]}"
            -I src -I third-parties/fast_io/include -I third-parties/bizwen/include -I third-parties/boost_unordered/include
            "test/0017.runtime/${payload}_payload.cc" -o "$out/$payload-$sanitizer")
        printf '%q ' "${command[@]}" >"$out/$payload-$sanitizer.command"
        "${command[@]}" >"$out/$payload-$sanitizer.build.log" 2>&1
        passed=0
        for attempt in {1..8}; do
            set +e
            TSAN_OPTIONS=external_symbolizer_path=/toolchain/bin/llvm-symbolizer timeout 45 "$out/$payload-$sanitizer" >"$out/$payload-$sanitizer-$attempt.run.log" 2>&1
            status=$?
            set -e
            if (( status == 0 )); then passed=1; cat "$out/$payload-$sanitizer-$attempt.run.log"; break; fi
            # Retry only a recognized pre-main TSan/ASLR collision. Preserve every
            # failed startup; a sanitizer report or test failure stops immediately.
            if [[ $sanitizer != thread ]] || ! grep -q 'ThreadSanitizer: encountered an incompatible memory layout' "$out/$payload-$sanitizer-$attempt.run.log"; then
                cat "$out/$payload-$sanitizer-$attempt.run.log"; exit "$status"
            fi
        done
        (( passed == 1 )) || exit 66
    done
done
after=$(python3 tools/ci/wasm3_source_fingerprint.py . "$out/source-after.json")
[[ $before == "$after" ]]
source tools/ci/require_wasm3_test_cgroup.sh
