#!/usr/bin/env bash
set -euo pipefail
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
: "${WASI_SDK_PATH:?Set WASI_SDK_PATH to official wasi-sdk-20.0 (Clang 16.0.0)}"
task_output=${1:?Usage: build.sh /absolute/artifact-directory}
case "$task_output" in /*) ;; *) echo 'Artifact directory must be absolute' >&2; exit 2 ;; esac
mkdir -p "$task_output"
"$WASI_SDK_PATH/bin/clang" -std=c17 -O2 -g "$source_dir/main.c" -o "$task_output/c-abi.wasm"
"$WASI_SDK_PATH/bin/clang++" -std=c++20 -O2 -g -fno-exceptions -fno-rtti \
  "$source_dir/main.cc" -o "$task_output/cxx-abi.wasm"
