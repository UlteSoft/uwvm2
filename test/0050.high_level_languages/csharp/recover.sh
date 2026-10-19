#!/usr/bin/env bash
# Run via docker exec in the existing 64 GiB cgroup, with chroot /host.
set -euo pipefail
task_arena=${1:?Usage: recover.sh /absolute/arena /absolute/wasmtime}
task_wasmtime=${2:?Usage: recover.sh /absolute/arena /absolute/wasmtime}
task_cgroup=/system.slice/docker-4dd58ad4560fde2ab9b539e4ef9cab1fac531654e99ad554aff1fce8ae0eae37.scope
test "$(cat /proc/self/cgroup)" = "0::$task_cgroup"
test "$(cat "/sys/fs/cgroup$task_cgroup/memory.max")" = 68719476736
test "$(cat "/sys/fs/cgroup$task_cgroup/memory.swap.max")" = 0
case "$task_arena" in /*) ;; *) exit 2 ;; esac
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
task_out="$task_arena/recovery/csharp"
mkdir -p "$task_out"
du -sk "$task_arena/csharp" > "$task_out/disk-before-kib.txt"
python3 "$source_dir/bootstrap.py" "$task_arena/csharp"
. "$task_arena/csharp/env.sh"
export PATH="$DOTNET_ROOT:$PATH"
export MSBUILDDISABLENODEREUSE=1
export DOTNET_CLI_USE_MSBUILD_SERVER=0
export UseSharedCompilation=false
trap '"$DOTNET_ROOT/dotnet" build-server shutdown >/dev/null 2>&1 || true' EXIT
bash "$source_dir/build.sh" "$task_arena/csharp/build"
bash "$source_dir/build-timed.sh" "$task_arena/csharp/target/build-timed"
cd "$source_dir"
for task_project in HighLevelCSharp HighLevelCSharpTimed; do
    task_suffix=
    if test "$task_project" = HighLevelCSharpTimed; then task_suffix=-timed; fi
    dotnet build "$task_project.csproj" -c Release --disable-build-servers \
      -p:RuntimeIdentifier= -p:PublishTrimmed=false -p:SelfContained=false \
      -p:BaseIntermediateOutputPath="$task_arena/csharp/target/obj-native$task_suffix/" \
      -p:BaseOutputPath="$task_arena/csharp/target/bin-native$task_suffix/"
done
bash "$task_arena/c_abi_compat/src/build.sh" "$task_arena/c_abi_compat/artifacts"
python3 "$source_dir/check_recovery_references.py" --arena "$task_arena" \
  --wasmtime "$task_wasmtime" --out "$task_out/references" --expected-cgroup "$task_cgroup"
python3 "$task_arena/c_abi_compat/src/run.py" --arena "$task_arena" \
  --artifacts "$task_arena/c_abi_compat/artifacts" --wasmtime "$task_wasmtime" \
  --out "$task_arena/recovery/c_abi_wasmtime" --expected-cgroup "$task_cgroup" --runner wasmtime
du -sk "$task_arena/csharp" "$task_arena/c_abi_compat" > "$task_out/disk-after-kib.txt"
