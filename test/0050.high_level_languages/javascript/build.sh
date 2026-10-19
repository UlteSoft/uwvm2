#!/bin/sh
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
output_dir=${1:?usage: build.sh OUTPUT_DIRECTORY}
mkdir -p "$output_dir"
output_dir=$(CDPATH= cd -- "$output_dir" && pwd)
javy=${JAVY:-javy}
node=${NODE:-node}
test "$("$javy" --version)" = 'javy 9.1.0'
if test "${CALIBRATION_ONLY:-0}" != 1; then
"$javy" build "$source_dir/workload.js" -o "$output_dir/javascript-debug.wasm" -C source=uncompressed -C deterministic=y
sed 's/workload(20000)/workload(200000)/' "$source_dir/workload.js" > "$output_dir/javascript-perf.js"
"$javy" build "$output_dir/javascript-perf.js" -o "$output_dir/javascript-perf.wasm" -C source=uncompressed -C deterministic=y
"$node" "$source_dir/node_oracle.cjs" "$output_dir/javascript-debug.wasm" 20000 'javy 9.1.0' "$source_dir/workload.js"
"$node" "$source_dir/node_oracle.cjs" "$output_dir/javascript-perf.wasm" 200000 'javy 9.1.0' "$output_dir/javascript-perf.js"
fi
for rounds in ${CALIBRATION_ROUNDS:-50 200 1000}; do
  case "$rounds" in ''|*[!0-9]*) echo 'Calibration rounds must be positive integers' >&2; exit 1 ;; esac
  test "$rounds" -gt 0
  base="$output_dir/javascript-rounds-$rounds"
  sed "s/workload(20000)/workload($rounds)/" "$source_dir/workload.js" > "$base.js"
  "$javy" build "$base.js" -o "$base.wasm" -C source=uncompressed -C deterministic=y
  "$node" "$source_dir/node_oracle.cjs" "$base.wasm" "$rounds" 'javy 9.1.0' "$base.js"
done
