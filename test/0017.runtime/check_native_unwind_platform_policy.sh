#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cxx="${CXX:-c++}"

fail() {
    printf 'native-unwind platform matrix: %s\n' "$*" >&2
    exit 1
}

command -v "$cxx" >/dev/null 2>&1 || fail "C++ preprocessor is unavailable: $cxx"

base_flags=(
    -E
    -dM
    -x c++
    -U__APPLE__
    -U__MACH__
    -U_WIN32
    -U_WIN64
    -U__linux
    -U__linux__
    -U__gnu_linux__
    -U__FreeBSD__
    -U__x86_64__
    -U_M_X64
    -U_M_AMD64
    -U__aarch64__
    -U_M_ARM64
    -U__arm64ec__
    -U_M_ARM64EC
    -U__CYGWIN__
    -U__ILP32__
    -I"$repo_root/src"
)

preprocess_macros() {
    "$cxx" "${base_flags[@]}" "$@" -
}

assert_macro() {
    local macros="$1"
    local macro_name="$2"
    local expected="$3"
    local case_name="$4"

    if ! grep -q "^#define ${macro_name} ${expected}$" <<<"$macros"; then
        grep "^#define ${macro_name} " <<<"$macros" >&2 || true
        fail "$case_name expected $macro_name=$expected"
    fi
}

check_platform() {
    local case_name="$1"
    local expected_native="$2"
    local expected_win64="$3"
    shift 3

    local macros
    macros="$(printf '%s\n' '#include <uwvm2/runtime/compiler/llvm_jit/native_unwind_platform.h>' | preprocess_macros "$@")"
    assert_macro "$macros" UWVM2_RUNTIME_LLVM_JIT_NATIVE_UNWIND_PLATFORM_SUPPORTED "$expected_native" "$case_name"
    assert_macro "$macros" UWVM2_RUNTIME_LLVM_JIT_WIN64_SEH_PLATFORM_SUPPORTED "$expected_win64" "$case_name"
    printf 'native-unwind platform matrix: %s native=%s win64=%s\n' "$case_name" "$expected_native" "$expected_win64"
}

check_runtime() {
    local case_name="$1"
    local expected_enable="$2"
    local expected_unwind_h="$3"
    local expected_win64="$4"
    local expected_any="$5"
    local expected_replace="$6"
    local expected_frame_pointer="$7"
    shift 7

    local macros
    macros="$(printf '%s\n' '#include <uwvm2/runtime/lib/uwvm_runtime_native_unwind.h>' | preprocess_macros "$@")"
    assert_macro "$macros" UWVM2_RUNTIME_LLVM_JIT_ENABLE_NATIVE_UNWIND_BACKTRACE "$expected_enable" "$case_name"
    assert_macro "$macros" UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_H_BACKTRACE "$expected_unwind_h" "$case_name"
    assert_macro "$macros" UWVM2_RUNTIME_LLVM_JIT_HAS_WIN64_SEH_BACKTRACE "$expected_win64" "$case_name"
    assert_macro "$macros" UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE "$expected_any" "$case_name"
    assert_macro "$macros" UWVM2_RUNTIME_LLVM_JIT_UNWIND_REPLACES_INSTRUCTION_FRAMES "$expected_replace" "$case_name"
    assert_macro "$macros" UWVM2_RUNTIME_LLVM_JIT_HAS_TRAP_FRAME_POINTER_CHAIN "$expected_frame_pointer" "$case_name"
    printf 'native-unwind platform matrix: %s enable=%s unwind-h=%s win64=%s any=%s replace=%s frame-pointer=%s\n' \
        "$case_name" "$expected_enable" "$expected_unwind_h" "$expected_win64" "$expected_any" "$expected_replace" "$expected_frame_pointer"
}

check_consumer_scope() {
    local case_name="$1"
    local header="$2"
    local native_macro="$3"
    local authoritative_macro="$4"

    local macros
    macros="$({
        printf '%s\n' '#define UWVM2_RUNTIME_LLVM_JIT_WIN64_SEH_PLATFORM_SUPPORTED 71'
        printf '%s\n' '#define UWVM2_RUNTIME_LLVM_JIT_NATIVE_UNWIND_PLATFORM_SUPPORTED 73'
        printf '%s\n' '#include <uwvm2/runtime/compiler/llvm_jit/native_unwind_platform.h>'
        printf '%s\n' '#define UWVM_MODULE 1'
        printf '%s\n' '#define UWVM_MODULE_EXPORT'
        printf '%s\n' '#define UWVM_RUNTIME_LLVM_JIT 1'
        printf '%s\n' '#define UWVM_HAS_FEATURE(...) 0'
        printf '#define %s 79\n' "$native_macro"
        printf '#define %s 83\n' "$authoritative_macro"
        printf '#include <%s>\n' "$header"
    } | preprocess_macros)"

    assert_macro "$macros" UWVM2_RUNTIME_LLVM_JIT_WIN64_SEH_PLATFORM_SUPPORTED 71 "$case_name"
    assert_macro "$macros" UWVM2_RUNTIME_LLVM_JIT_NATIVE_UNWIND_PLATFORM_SUPPORTED 73 "$case_name"
    assert_macro "$macros" "$native_macro" 79 "$case_name"
    assert_macro "$macros" "$authoritative_macro" 83 "$case_name"
    printf 'native-unwind platform matrix: %s scoped-macros=restored\n' "$case_name"
}

matrix_tmp="$(mktemp -d)"
trap 'rm -rf "$matrix_tmp"' EXIT
printf '%s\n' '/* synthetic header for __has_include(<unwind.h>) */' >"$matrix_tmp/unwind.h"

check_platform unsupported 0 0
check_platform apple-arm64 1 0 -D__APPLE__=1 -D__aarch64__=1
check_platform linux-x86_64 1 0 -D__linux__=1 -D__x86_64__=1
check_platform freebsd-x86_64 1 0 -D__FreeBSD__=1 -D__x86_64__=1
check_platform linux-aarch64 0 0 -D__linux__=1 -D__aarch64__=1
check_platform linux-x86_64-ilp32 0 0 -D__linux__=1 -D__x86_64__=1 -D__ILP32__=1
check_platform win64-x86_64 1 1 -D_WIN32=1 -D_WIN64=1 -D__x86_64__=1
check_platform win64-arm64 1 1 -D_WIN32=1 -D_WIN64=1 -D__aarch64__=1
check_platform win64-arm64ec 0 0 -D_WIN32=1 -D_WIN64=1 -D__aarch64__=1 -D__arm64ec__=1
check_platform win64-cygwin 0 0 -D_WIN32=1 -D_WIN64=1 -D__x86_64__=1 -D__CYGWIN__=1

check_runtime backend-off-linux 0 0 0 0 0 0 -nostdinc -I"$matrix_tmp" -D__linux__=1 -D__x86_64__=1
check_runtime linux-no-unwind-header 1 0 0 0 0 0 -nostdinc -DUWVM_RUNTIME_LLVM_JIT=1 -D__linux__=1 -D__x86_64__=1
check_runtime linux-with-unwind-header 1 1 0 1 1 0 -nostdinc -I"$matrix_tmp" -DUWVM_RUNTIME_LLVM_JIT=1 -D__linux__=1 -D__x86_64__=1
check_runtime freebsd-with-unwind-header 1 1 0 1 1 0 -nostdinc -I"$matrix_tmp" -DUWVM_RUNTIME_LLVM_JIT=1 -D__FreeBSD__=1 -D__x86_64__=1
check_runtime apple-with-unwind-header 1 1 0 1 1 0 -nostdinc -I"$matrix_tmp" -DUWVM_RUNTIME_LLVM_JIT=1 -D__APPLE__=1 -D__aarch64__=1
check_runtime win64-x86_64 1 0 1 1 1 1 -nostdinc -DUWVM_RUNTIME_LLVM_JIT=1 -D_WIN32=1 -D_WIN64=1 -D__x86_64__=1
check_runtime win64-arm64 1 0 1 1 1 1 -nostdinc -DUWVM_RUNTIME_LLVM_JIT=1 -D_WIN32=1 -D_WIN64=1 -D__aarch64__=1
check_runtime win64-arm64ec 0 0 0 0 0 0 -nostdinc -DUWVM_RUNTIME_LLVM_JIT=1 -D_WIN32=1 -D_WIN64=1 -D__aarch64__=1 -D__arm64ec__=1

check_consumer_scope params \
    uwvm2/uwvm/cmdline/params/runtime_llvm_jit_call_stack.h \
    UWVM2_UWVM_CMDLINE_RUNTIME_LLVM_JIT_CALL_STACK_HAS_NATIVE_UNWIND \
    UWVM2_UWVM_CMDLINE_RUNTIME_LLVM_JIT_CALL_STACK_HAS_AUTHORITATIVE_UNWIND
check_consumer_scope callback \
    uwvm2/uwvm/cmdline/callback/runtime_llvm_jit_call_stack.h \
    UWVM2_UWVM_CMDLINE_RUNTIME_LLVM_JIT_CALL_STACK_HAS_NATIVE_UNWIND \
    UWVM2_UWVM_CMDLINE_RUNTIME_LLVM_JIT_CALL_STACK_HAS_AUTHORITATIVE_UNWIND
check_consumer_scope version \
    uwvm2/uwvm/cmdline/callback/version.h \
    UWVM2_UWVM_CMDLINE_VERSION_LLVM_JIT_CALL_STACK_HAS_NATIVE_UNWIND \
    UWVM2_UWVM_CMDLINE_VERSION_LLVM_JIT_CALL_STACK_HAS_AUTHORITATIVE_UNWIND

runtime_source="$repo_root/src/uwvm2/runtime/lib/uwvm_runtime.default.cpp"
grep -Fq '# if UWVM2_RUNTIME_LLVM_JIT_HAS_UNWIND_BACKTRACE && UWVM2_RUNTIME_LLVM_JIT_UNWIND_REPLACES_INSTRUCTION_FRAMES' "$runtime_source" ||
    fail 'unwind-uncheck requested path is not explicitly gated by authoritative frame replacement'
grep -Fq '# elif !UWVM2_RUNTIME_LLVM_JIT_UNWIND_REPLACES_INSTRUCTION_FRAMES' "$runtime_source" ||
    fail 'unwind-uncheck validation path can bypass authoritative frame replacement'

printf 'native-unwind platform matrix: ok\n'
