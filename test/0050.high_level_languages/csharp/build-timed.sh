#!/usr/bin/env bash
set -euo pipefail
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${DOTNET_ROOT:?Set DOTNET_ROOT to the pinned .NET SDK 8.0.408 installation}"
: "${WASI_SDK_PATH:?Set WASI_SDK_PATH to the official wasi-sdk-20.0 installation}"
task_build_dir=${1:?Usage: build-timed.sh /absolute/output-directory}
case "$task_build_dir" in /*) ;; *) echo 'Output directory must be absolute' >&2; exit 2 ;; esac
export PATH="$DOTNET_ROOT:$PATH"
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
export DOTNET_CLI_TELEMETRY_OPTOUT=1
export DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1
test "$(dotnet --version)" = 8.0.408
mkdir -p "$task_build_dir"
cd "$source_dir"
dotnet publish HighLevelCSharpTimed.csproj -c Release \
  -p:BaseIntermediateOutputPath="$task_build_dir/obj/" \
  -p:BaseOutputPath="$task_build_dir/bin/" \
  -p:WasmAppDir="$task_build_dir/AppBundle/"
python3 "$source_dir/retain_symbols.py" "$task_build_dir" "$WASI_SDK_PATH/bin/clang" highlevel-csharp-timed
test -s "$task_build_dir/AppBundle/highlevel-csharp-timed.wasm"
printf '%s\n' "$task_build_dir/AppBundle/highlevel-csharp-timed.wasm"
