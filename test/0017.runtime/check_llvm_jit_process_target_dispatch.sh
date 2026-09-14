#!/usr/bin/env bash

set -euo pipefail

cxx="${CXX:-c++}"

fail() {
    printf 'llvm-jit process-target matrix: %s\n' "$*" >&2
    exit 1
}

command -v "$cxx" >/dev/null 2>&1 || fail "C++ compiler is unavailable: $cxx"

if [[ $# -eq 0 ]]; then
    repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
    sources=("$repo_root/src/uwvm2/runtime/lib/uwvm_runtime.default.cpp")
else
    sources=("$@")
fi

matrix_tmp="$(mktemp -d)"
trap 'rm -rf "$matrix_tmp"' EXIT

base_flags=(
    -std=c++23
    -x c++
    -U__x86_64__
    -U__i386__
    -U_M_AMD64
    -U_M_X64
    -U_M_IX86
    -U__aarch64__
    -U__arm64__
    -U_M_ARM64
    -U__arm__
    -U_M_ARM
    -U__powerpc__
    -U__powerpc64__
    -U__ppc__
    -U__ppc64__
    -U__riscv
    -U__riscv_xlen
    -U__s390x__
    -U__loongarch__
    -U__loongarch64
    -U__mips__
    -U__MIPS__
    -U_MIPS_ARCH
    -U__sparc__
    -U__sparc64__
)

emit_probe_preamble() {
    cat <<'CPP'
inline int selected_target{};

#define DEFINE_TARGET_INITIALIZERS(NAME, VALUE) \
    inline void LLVMInitialize##NAME##TargetInfo() { selected_target = VALUE; } \
    inline void LLVMInitialize##NAME##Target() { selected_target = VALUE; } \
    inline void LLVMInitialize##NAME##TargetMC() { selected_target = VALUE; } \
    inline void LLVMInitialize##NAME##AsmPrinter() { selected_target = VALUE; }

DEFINE_TARGET_INITIALIZERS(X86, 1)
DEFINE_TARGET_INITIALIZERS(AArch64, 2)
DEFINE_TARGET_INITIALIZERS(ARM, 3)
DEFINE_TARGET_INITIALIZERS(PowerPC, 4)
DEFINE_TARGET_INITIALIZERS(RISCV, 5)
DEFINE_TARGET_INITIALIZERS(SystemZ, 6)
DEFINE_TARGET_INITIALIZERS(LoongArch, 7)
DEFINE_TARGET_INITIALIZERS(Mips, 8)
DEFINE_TARGET_INITIALIZERS(Sparc, 9)

namespace llvm
{
    inline bool InitializeNativeTarget()
    {
        selected_target = 99;
        return false;
    }

    inline bool InitializeNativeTargetAsmPrinter()
    {
        selected_target = 99;
        return false;
    }
}
CPP
}

extract_initializer() {
    awk '
        /\[\[nodiscard\]\] inline constexpr bool initialize_llvm_jit_process_target\(\) noexcept/ { emit = 1 }
        emit { print }
        emit && /^        }$/ { exit }
    ' "$1"
}

check_case() {
    local source_path="$1"
    local case_name="$2"
    local expected="$3"
    shift 3

    local output="$matrix_tmp/$(basename "$source_path").$case_name"
    {
        emit_probe_preamble
        extract_initializer "$source_path"
        printf '%s\n' "int main() { return initialize_llvm_jit_process_target() && selected_target == $expected ? 0 : 1; }"
    } | "$cxx" "${base_flags[@]}" "$@" -o "$output" -

    "$output" || fail "$(basename "$source_path") selected the wrong LLVM target for $case_name"
    printf 'llvm-jit process-target matrix: %-32s %s\n' "$(basename "$source_path")/$case_name" PASS
}

for source_path in "${sources[@]}"; do
    [[ -f "$source_path" ]] || fail "source does not exist: $source_path"
    initializer="$(extract_initializer "$source_path")"
    [[ -n "$initializer" ]] || fail "initializer was not found in: $source_path"

    check_case "$source_path" x86_64 1 -D__x86_64__=1
    check_case "$source_path" aarch64 2 -D__aarch64__=1
    check_case "$source_path" arm 3 -D__arm__=1
    check_case "$source_path" powerpc64 4 -D__powerpc64__=1
    check_case "$source_path" riscv64 5 -D__riscv=1 -D__riscv_xlen=64
    check_case "$source_path" s390x 6 -D__s390x__=1
    check_case "$source_path" loongarch64 7 -D__loongarch__=1 -D__loongarch64=1
    check_case "$source_path" mips64 8 -D__mips__=1
    check_case "$source_path" sparc64 9 -D__sparc__=1 -D__sparc64__=1
    check_case "$source_path" fallback 99
done

printf 'llvm-jit process-target matrix: ok\n'
