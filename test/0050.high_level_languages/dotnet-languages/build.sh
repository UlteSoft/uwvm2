#!/usr/bin/env bash
# Standalone producer example; qualification builds run in the existing Linux cgroup.
set -euo pipefail
language=${1:?Usage: build.sh fsharp\|vb Debug\|Release native\|wasi /absolute/output}
configuration=${2:?Select Debug or Release}
target=${3:?Select native or wasi}
output=${4:?Select an absolute output directory}
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
case "$language" in
  fsharp) project=HighLevelFSharp.fsproj; assembly=highlevel-fsharp ;;
  vb) project=HighLevelVisualBasic.vbproj; assembly=highlevel-vb ;;
  *) echo 'Language must be fsharp or vb' >&2; exit 2 ;;
esac
case "$configuration" in Debug|Release) ;; *) echo 'Configuration must be Debug or Release' >&2; exit 2 ;; esac
case "$output" in /*) ;; *) echo 'Output must be absolute' >&2; exit 2 ;; esac
: "${DOTNET_ROOT:?Set DOTNET_ROOT to the .NET 8.0.408 SDK installation}"
export DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1 DOTNET_CLI_WORKLOAD_UPDATE_NOTIFY_DISABLE=true
export DOTNET_CLI_USE_MSBUILD_SERVER=0 MSBUILDDISABLENODEREUSE=1
test "$("$DOTNET_ROOT/dotnet" --version)" = 8.0.408
mkdir -p "$output"
options=(-c "$configuration" -m:1 --disable-build-servers -p:UseSharedCompilation=false
  "-p:BaseIntermediateOutputPath=$output/obj/" "-p:BaseOutputPath=$output/bin/"
  "-p:PublishDir=$output/publish/" -p:NuGetAudit=false)
if test -n "${HLL_NUGET_CONFIG:-}"; then options+=(--configfile "$HLL_NUGET_CONFIG"); fi
case "$target" in
  native) options+=(-p:RuntimeIdentifier= --self-contained false) ;;
  wasi)
    : "${WASI_SDK_PATH:?Set WASI_SDK_PATH to the wasi-sdk-20.0 installation}"
    options+=(-r wasi-wasm "-p:WasmAppDir=$output/AppBundle/" -p:WasmNativeStrip=false
      -p:WasmNativeDebugSymbols=true -p:WasmSingleFileBundle=true -p:RunAOTCompilation=false)
    ;;
  *) echo 'Target must be native or wasi' >&2; exit 2 ;;
esac
cd "$source_dir"
"$DOTNET_ROOT/dotnet" publish "$project" "${options[@]}"
if test "$target" = wasi; then
  python3 "$source_dir/retain_symbols.py" "$output" "$WASI_SDK_PATH/bin/clang" "$assembly" "$configuration"
  printf '%s\n' "$output/AppBundle/$assembly.wasm"
else
  printf '%s\n' "$output/publish/$assembly.dll"
fi
