#!/bin/bash
set -euo pipefail
if [[ $(uname -s) != Linux ]]; then
    echo "SKIP: deterministic sigaction interposition requires a Linux linker with --wrap"
    exit 77
fi
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
out=${1:-$(mktemp -d /tmp/uwvm-signal-install.XXXXXX)}
mkdir -p "$out"
# Run inside the caller's aggregate cgroup/resource budget. CXX is one driver
# path, not an evaluated shell command; invoke again to cover another compiler.
for opt in O0 O3; do
    "${CXX:-c++}" -std=c++23 "-$opt" -Wall -Wextra -Werror -pthread \
        -I"$root/src" "$root/test/0017.runtime/native_stack_signal_install.cpp" \
        -Wl,--wrap=sigaction -o "$out/$opt"
    "$out/$opt"
done
