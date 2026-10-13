#!/usr/bin/env bash
# Compile C/C++ -g Wasm fixtures only inside a 64 GiB Linux user scope.
# Invocation (on ssh linux):
# systemd-run --user --scope -p MemoryMax=68719476736 \
#   taskset -c 0,2,4,6,16-31 bash build_debug_source_wasm_host.sh SOURCE_DIR OUT_DIR
set -euo pipefail
source_dir=${1:?source directory}
out_dir=${2:?output directory under /tmp}
[[ "$out_dir" == /tmp/* ]] || { echo 'output must be host /tmp tmpfs' >&2; exit 2; }
cg_path=$(cut -d: -f3 /proc/self/cgroup)
memory_max=$(cat "/sys/fs/cgroup${cg_path}/memory.max")
allowed_cpus=$(awk '/^Cpus_allowed_list:/ {print $2}' /proc/self/status)
[[ "$memory_max" == 68719476736 ]] || { echo "wrong cgroup memory.max=$memory_max" >&2; exit 2; }
[[ "$allowed_cpus" == '0,2,4,6,16-31' ]] || { echo "wrong CPU affinity=$allowed_cpus" >&2; exit 2; }
mkdir -p "$out_dir"
echo "CGROUP=$cg_path"
echo "MEMORY_MAX=$memory_max"
echo "CPUS_ALLOWED_LIST=$allowed_cpus"
/usr/bin/clang-20 --version | head -1
/usr/bin/wasm-ld-21 --version
for language in c cpp; do
    if [[ "$language" == c ]]; then
        source="$source_dir/debug_source_c.c"
    else
        source="$source_dir/debug_source_cpp.cc"
    fi
    for dwarf in 4 5; do
        stem="debug-source-${language}-dwarf${dwarf}"
        object="$out_dir/${stem}.o"
        wasm="$out_dir/${stem}.wasm"
        command=(/usr/bin/clang-20 --target=wasm32-unknown-unknown "-gdwarf-${dwarf}" -g -O0 -nostdlib -c "$source" -o "$object")
        printf 'COMPILE='; printf '%q ' "${command[@]}"; echo
        "${command[@]}"
        /usr/bin/wasm-ld-21 --no-entry --export-all "$object" -o "$wasm"
        sha256sum "$source" "$object" "$wasm"
    done
done
