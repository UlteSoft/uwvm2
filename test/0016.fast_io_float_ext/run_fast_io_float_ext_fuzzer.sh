#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fast_float_root="/tmp/fast_float"
dragonbox_root="/tmp/dragonbox"
build_dir="/tmp/uwvm2_fast_io_float_ext"

if [[ ! -d "${fast_float_root}/.git" ]]; then
  rm -rf "${fast_float_root}"
  git clone --depth=1 https://github.com/fastfloat/fast_float.git "${fast_float_root}"
else
  git -C "${fast_float_root}" pull --ff-only
fi

if [[ ! -d "${dragonbox_root}/.git" ]]; then
  rm -rf "${dragonbox_root}"
  git clone --depth=1 https://github.com/jk-jeon/dragonbox.git "${dragonbox_root}"
else
  git -C "${dragonbox_root}" pull --ff-only
fi

mkdir -p "${build_dir}"

cxx="${CXX:-clang++}"
"${cxx}" \
  -std=c++26 \
  -O2 \
  -g \
  -Wall \
  -Wextra \
  -Wpedantic \
  -Werror \
  -Wno-unused-parameter \
  -Wno-shift-count-overflow \
  -I"${root}/third-parties/fast_io/include" \
  -I"${fast_float_root}/include" \
  -I"${dragonbox_root}/include" \
  -fsanitize=fuzzer \
  -DFAST_IO_FLOAT_EXT_USE_DRAGONBOX=1 \
  "${root}/test/0009.libfuzzer/fast_io_float_ext_fuzzer.cc" \
  "${dragonbox_root}/source/dragonbox_to_chars.cpp" \
  -o "${build_dir}/fast_io_float_ext_fuzzer"

"${build_dir}/fast_io_float_ext_fuzzer" \
  "-rss_limit_mb=${FUZZ_RSS:-512}" \
  "-max_total_time=${FUZZ_MAX_TIME:-30}" \
  "-max_len=${FUZZ_MAX_LEN:-256}" \
  "-timeout=${FUZZ_TIMEOUT:-5}"
