#!/bin/sh
set -eu
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
output_dir=${1:?usage: build.sh OUTPUT_DIRECTORY}
mkdir -p "$output_dir"
output_dir=$(CDPATH= cd -- "$output_dir" && pwd)
moon=${MOON:-moon}
moonc=${MOONC:-moonc}
node=${NODE:-node}
case "$("$moon" version --all)" in
  *'moonc v0.10.15+943464bfa (2026-10-12)'*) ;;
  *) echo 'Requires MoonBit v0.10.15+943464bfa (2026-10-12)' >&2; exit 1 ;;
esac
variants='smoke perf upstream-stdio'
if test "${CALIBRATION_ONLY:-0}" = 1; then variants=''; fi
for rounds in ${CALIBRATION_ROUNDS:-1000 20000 2000000}; do
  case "$rounds" in ''|*[!0-9]*) echo 'Calibration rounds must be positive integers' >&2; exit 1 ;; esac
  test "$rounds" -gt 0
  variants="$variants rounds-$rounds-debug rounds-$rounds-release"
done
for variant in $variants; do
  work="$output_dir/source-$variant"
  mkdir -p "$work"
  cp "$source_dir/moon.mod" "$source_dir/moon.pkg" "$work/"
  case "$variant" in
    smoke) cp "$source_dir/workload.mbt" "$work/" ;;
    perf) sed 's/workload(20000)/workload(2000000)/' "$source_dir/workload.mbt" > "$work/workload.mbt" ;;
    upstream-stdio) sed 's/  print_ascii_line("moonbit/  println("moonbit/' "$source_dir/workload.mbt" > "$work/workload.mbt" ;;
    rounds-*)
      rounds=${variant#rounds-}
      rounds=${rounds%-*}
      sed "s/workload(20000)/workload($rounds)/" "$source_dir/workload.mbt" > "$work/workload.mbt" ;;
  esac
  for target in wasm wasm-gc; do
    if test "$variant" = perf || test "${variant##*-}" = release; then
      "$moon" -C "$work" build --target "$target" --release --no-strip
      mode=release
      suffix=$variant
    else
      "$moon" -C "$work" build --target "$target" --debug
      suffix=debug
      if test "$variant" = upstream-stdio; then suffix=upstream-stdio; fi
      mode=debug
      case "$variant" in rounds-*) suffix=$variant ;; esac
    fi
    # Relink through the producer with the final basename. Copying a module
    # would leave sourceMappingURL pointing at the original high_level name.
    bundle="$MOON_HOME/lib/core/_build/$target/release/bundle"
    set -- "$moonc" link-core "$bundle/abort/abort.core" "$bundle/core.core" \
      "$work/_build/$target/$mode/build/high_level.core" \
      -main uwvm/high_level -o "$output_dir/moonbit-$target-$suffix.wasm" \
      -pkg-config-path "$work/moon.pkg" -pkg-sources "uwvm/high_level:$work" \
      -pkg-sources "moonbitlang/core:$MOON_HOME/lib/core" -target "$target" -g \
      -source-map -source-map-url "./moonbit-$target-$suffix.wasm.map" \
      -exported_functions=workload -export-memory-name memory
    if test "$mode" = debug; then set -- "$@" -O0; fi
    "$@"
    case "$variant" in
      rounds-*) "$node" "$source_dir/../javascript/node_oracle.cjs" \
        "$output_dir/moonbit-$target-$suffix.wasm" "$rounds" "moonc v0.10.15+943464bfa $mode" ;;
    esac
  done
done
