#!/bin/sh
set -eu
# Match Node's physical process.cwd() when the persistent arena is reached
# through /tmp's symlink. Logical/physical path mixtures change producer source
# names and map paths even though the WebAssembly instructions are identical.
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
output_dir=${1:?usage: build.sh OUTPUT_DIRECTORY; run npm ci in the source directory first}
mkdir -p "$output_dir"
output_dir=$(CDPATH= cd -- "$output_dir" && pwd -P)
node=${NODE:-node}
asc="$source_dir/node_modules/assemblyscript/bin/asc.js"
test "$("$node" "$asc" --version)" = 'Version 0.28.20'
cd "$source_dir"
if test "${CALIBRATION_ONLY:-0}" != 1; then
"$node" "$asc" "$source_dir/workload.ts" --outFile "$output_dir/assemblyscript-debug.wasm" --debug --sourceMap --use abort=
"$node" "$asc" "$source_dir/workload.ts" --outFile "$output_dir/assemblyscript-release.wasm" --optimize --debug --sourceMap --use abort=
# Binaryen records these roots in the debug/release sidecars. Keep the user
# source available at the exact resolved path used by a native Wasm debugger.
for variant in debug release; do
  mkdir -p "$output_dir/assemblyscript-$variant"
  cp "$source_dir/workload.ts" "$output_dir/assemblyscript-$variant/workload.ts"
done
sed 's/workload(20000)/workload(2000000)/' "$source_dir/workload.ts" > "$output_dir/assemblyscript-perf.ts"
"$node" "$asc" "$output_dir/assemblyscript-perf.ts" --outFile "$output_dir/assemblyscript-perf.wasm" --optimize --debug --sourceMap --use abort=
fi
for rounds in ${CALIBRATION_ROUNDS:-1000 20000 2000000}; do
  case "$rounds" in ''|*[!0-9]*) echo 'Calibration rounds must be positive integers' >&2; exit 1 ;; esac
  test "$rounds" -gt 0
  base="$output_dir/assemblyscript-rounds-$rounds"
  sed "s/workload(20000)/workload($rounds)/" "$source_dir/workload.ts" > "$base.ts"
  for mode in debug release; do
    if test "$mode" = release; then
      "$node" "$asc" "$base.ts" --outFile "$base-$mode.wasm" --optimize --debug --sourceMap --use abort=
    else
      "$node" "$asc" "$base.ts" --outFile "$base-$mode.wasm" --debug --sourceMap --use abort=
    fi
    "$node" "$source_dir/../javascript/node_oracle.cjs" "$base-$mode.wasm" "$rounds" "assemblyscript 0.28.20 $mode"
  done
done
