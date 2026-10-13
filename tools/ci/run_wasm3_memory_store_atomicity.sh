#!/usr/bin/env bash
set -euo pipefail
source "$(dirname -- "${BASH_SOURCE[0]}")/require_wasm3_test_cgroup.sh"
cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.."
output=${1:?usage: run_wasm3_memory_store_atomicity.sh OUTPUT_DIRECTORY WAT2WASM [growth] [matrix]}
wat2wasm=${2:?WAT2WASM}
growth=${3:-}
[[ -z $growth || $growth == growth ]] || { echo "Expected optional growth argument" >&2; exit 2; }
mkdir -p "$output"
output=$(cd "$output" && pwd)
python3 test/0013.uwvm_int/wasm3/memory_store_atomicity_cases.py "$output/fixtures" --wat2wasm "$wat2wasm" --include-fused-inputs
before=$(python3 tools/ci/wasm3_source_fingerprint.py "$PWD" "$output/source-manifest.json")
compiler=${CXX:-clang++}
combinations=(none); delays=(none)
if [[ ${4:-} == matrix ]]; then combinations=(none soft heavy extra); delays=(none soft heavy); fi
for combine in "${combinations[@]}"; do
for delay in "${delays[@]}"; do
flags=()
if [[ $combine != none ]]; then flags+=(-DUWVM_ENABLE_UWVM_INT_COMBINE_OPS); fi
if [[ $combine == heavy || $combine == extra ]]; then flags+=(-DUWVM_ENABLE_UWVM_INT_HEAVY_COMBINE_OPS); fi
if [[ $combine == extra ]]; then flags+=(-DUWVM_ENABLE_UWVM_INT_EXTRA_HEAVY_COMBINE_OPS); fi
if [[ $delay != none ]]; then flags+=(-DUWVM_ENABLE_UWVM_INT_DELAY_LOCAL_SOFT); fi
if [[ $delay == heavy ]]; then flags+=(-DUWVM_ENABLE_UWVM_INT_DELAY_LOCAL_HEAVY); fi
configuration="combine-$combine-delay-$delay"
directory="$output/$configuration"
mkdir -p "$directory"
command=("$compiler" -std=c++26 -stdlib=libc++ -fuse-ld=lld -rtlib=compiler-rt -unwindlib=libunwind
    -O3 -g0 -Wno-undefined-inline -DUWVM=2 -DUWVM_USE_UWVM_INT -DUWVM_DISABLE_JIT
    -DUWVM_DISABLE_LOCAL_IMPORTED_WASIP1 -I src -I third-parties/bizwen/include
    -I third-parties/fast_io/include -I third-parties/boost_unordered/include
    "${flags[@]}" test/0013.uwvm_int/wasm3/memory_store_atomicity.cc -o "$directory/check")
printf '%q ' "${command[@]}" >"$directory/build.command"
"${command[@]}" >"$directory/build.log" 2>&1
ulimit -c 0
python3 - "$output" "$growth" "$directory" <<'PY'
import hashlib, json, pathlib, subprocess, sys
fixtures = pathlib.Path(sys.argv[1])
out = pathlib.Path(sys.argv[3])
results = []
growth = sys.argv[2] == "growth"
for case in json.loads((fixtures / 'fixtures/cases.json').read_text()):
    command = [str(out / 'check'), case['wasm'], str(case['width']), *(['growth'] if growth else [])]
    run = subprocess.run(command, capture_output=True, text=True, timeout=120)
    results.append(dict(**case, command=command, exit=run.returncode, stdout=run.stdout, stderr=run.stderr))
    (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    if run.returncode:
        raise RuntimeError(results[-1])
    print(case['opcode'], run.stdout.strip(), flush=True)
summary = dict(passed=True, cases=len(results), prefix_checks=sum(2*(2 if growth else 1)*(r['width']-1) for r in results),
               growth=growth, positive_old_boundary_checks=sum(2*(r['width']-1) for r in results) if growth else 0,
               binary_sha256=hashlib.sha256((out/'check').read_bytes()).hexdigest())
(out/'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
PY
echo "PASS $configuration split-store safety"
done
done
after=$(python3 tools/ci/wasm3_source_fingerprint.py "$PWD" "$output/source-manifest-after.json")
[[ $before == "$after" ]] || { echo 'Source changed during compilation' >&2; exit 1; }
