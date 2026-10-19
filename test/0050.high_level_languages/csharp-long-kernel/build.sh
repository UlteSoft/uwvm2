#!/usr/bin/env bash
set -euo pipefail
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${DOTNET_ROOT:?Select the pinned .NET SDK 8.0.408}"
: "${WASI_SDK_PATH:?Select the pinned WASI SDK 20}"
: "${NUGET_PACKAGES:?Select the existing offline package cache}"
configuration=${1:?Usage: build.sh Debug|Release native|wasi /absolute/output}
target_kind=${2:?Select native or wasi}
task_output_dir=${3:?Select an absolute output directory}
case "$configuration" in Debug|Release) ;; *) exit 2 ;; esac
case "$target_kind" in native|wasi) ;; *) exit 2 ;; esac
case "$task_output_dir" in /*) ;; *) exit 2 ;; esac
export PATH="$DOTNET_ROOT:$PATH"
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
export DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1
export DOTNET_NOLOGO=1
export DOTNET_CLI_USE_MSBUILD_SERVER=0 MSBUILDDISABLENODEREUSE=1 DOTNET_gcServer=0
export DOTNET_GCHeapHardLimit=0x20000000
test "$(dotnet --version)" = 8.0.408
mkdir -p "$task_output_dir"
cd "$source_dir"
project=HighLevelCSharpProfileNative.csproj
if [[ "$target_kind" == wasi ]]; then project=HighLevelCSharpProfile.csproj; fi
dotnet publish "$project" -c "$configuration" --disable-build-servers \
 -maxcpucount:1 -p:BuildInParallel=false -p:UseSharedCompilation=false \
 -p:RestoreConfigFile="$source_dir/NuGet.Config" -p:NuGetAudit=false \
 -p:BaseIntermediateOutputPath="$task_output_dir/obj/" \
 -p:MSBuildProjectExtensionsPath="$task_output_dir/obj/" \
 -p:BaseOutputPath="$task_output_dir/bin/" \
 -p:WasmAppDir="$task_output_dir/AppBundle/" -o "$task_output_dir/native"
if [[ "$target_kind" == wasi ]]; then
 python3 "$source_dir/retain_symbols.py" "$task_output_dir" "$WASI_SDK_PATH/bin/clang" highlevel-csharp-long "$configuration"
 test -s "$task_output_dir/AppBundle/highlevel-csharp-long.wasm"
else
 test -s "$task_output_dir/native/highlevel-csharp-long-native.dll"
fi
