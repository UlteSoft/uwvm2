#!/bin/sh

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$script_dir/../../../.." && pwd)

clang=${CLANGXX:-/Users/liyinan/Documents/MacroModel/tool-chain/tools/aarch64-apple-darwin-llvm/llvm/bin/clang++}
xjkp_root=${XJKP_SYSROOT:-/Users/liyinan/Documents/MacroModel/tool-chain/sysroot/xjkp-sysroot}
out_dir=${UWVM2_CODEGEN_OUT:-${TMPDIR:-/tmp}/uwvm2-wasm2-codegen}
probe="$script_dir/wasm2_hot_opfunc_codegen_probe.cc"

mkdir -p "$out_dir"

compile_target()
{
    triple=$1
    sysroot_dir=$2
    "$clang" \
        -std=c++26 \
        -O3 \
        -S \
        -Wall \
        -Wextra \
        -Wpedantic \
        -Werror \
        --target="$triple" \
        --sysroot="$xjkp_root/$sysroot_dir" \
        -stdlib=libc++ \
        -isystem "$xjkp_root/include/c++/v1" \
        -I"$repo_root/third-parties/fast_io/include" \
        -I"$repo_root/third-parties/bizwen/include" \
        -I"$repo_root/third-parties/boost_unordered/include" \
        -I"$repo_root/src" \
        -DUWVM=2 \
        -fno-exceptions \
        -fno-rtti \
        -fno-asynchronous-unwind-tables \
        -fno-unwind-tables \
        "$probe" \
        -o "$out_dir/$triple.s"
}

extract_function()
{
    assembly=$1
    function_name=$2
    output=$3
    awk -v fn="$function_name" '
        $0 ~ "^" fn ":" { emit = 1 }
        emit { print }
        emit && /^\.Lfunc_end[0-9]+:/ { exit }
    ' "$assembly" > "$output"
}

require_pattern()
{
    pattern=$1
    file=$2
    message=$3
    if ! grep -Eq "$pattern" "$file"; then
        printf '%s\n' "codegen check failed: $message" >&2
        exit 1
    fi
}

reject_pattern()
{
    pattern=$1
    file=$2
    message=$3
    if grep -Eq "$pattern" "$file"; then
        printf '%s\n' "codegen check failed: $message" >&2
        grep -En "$pattern" "$file" >&2
        exit 1
    fi
}

require_count()
{
    expected=$1
    pattern=$2
    file=$3
    message=$4
    actual=$(grep -Ec "$pattern" "$file" || true)
    if [ "$actual" -ne "$expected" ]; then
        printf '%s\n' "codegen check failed: $message (expected $expected, got $actual)" >&2
        exit 1
    fi
}

audit_target()
{
    triple=$1
    call_pattern=$2
    stack_pattern=$3
    tail_pattern=$4
    simd_pattern=$5
    assembly="$out_dir/$triple.s"

    for function in \
        wasm2_codegen_ref_is_null \
        wasm2_codegen_i32_trunc_sat_f32_s \
        wasm2_codegen_f32x4_add \
        wasm2_codegen_v128_const \
        wasm2_codegen_table_get_funcref \
        wasm2_codegen_memory_fill
    do
        body="$out_dir/$triple.$function.body.s"
        extract_function "$assembly" "$function" "$body"
        require_pattern "^$function:" "$body" "$triple did not emit $function"
    done

    for function in \
        wasm2_codegen_ref_is_null \
        wasm2_codegen_i32_trunc_sat_f32_s \
        wasm2_codegen_f32x4_add \
        wasm2_codegen_v128_const
    do
        body="$out_dir/$triple.$function.body.s"
        reject_pattern "$call_pattern" "$body" "$triple $function contains an out-of-line call"
        reject_pattern "$stack_pattern" "$body" "$triple $function spills or creates a native frame"
        require_pattern "$tail_pattern" "$body" "$triple $function lost direct-threaded tail dispatch"
    done

    require_pattern "$simd_pattern" "$out_dir/$triple.wasm2_codegen_f32x4_add.body.s" \
        "$triple f32x4.add was not lowered to one native vector add"
    require_pattern 'table_oob_terminate' "$out_dir/$triple.wasm2_codegen_table_get_funcref.body.s" \
        "$triple table.get lost its required OOB trap path"
    require_pattern 'memset' "$out_dir/$triple.wasm2_codegen_memory_fill.body.s" \
        "$triple memory.fill was not lowered to the platform bulk-memory primitive"
    require_count 1 "$call_pattern" "$out_dir/$triple.wasm2_codegen_table_get_funcref.body.s" \
        "$triple table.get contains an unexpected helper call"
    require_count 2 "$call_pattern" "$out_dir/$triple.wasm2_codegen_memory_fill.body.s" \
        "$triple memory.fill must call only memset and the cold OOB trap"

    printf '%-28s %s\n' "$triple" 'PASS (leaf opfuncs have no frame, spill, or helper call)'
}

# Keep these invocations sequential: each clang frontend can consume substantial
# memory when the complete interpreter header surface is instantiated.
if [ "${UWVM2_CODEGEN_AUDIT_ONLY:-0}" != 1 ]; then
    compile_target aarch64-linux-gnu aarch64-linux-gnu
fi
audit_target \
    aarch64-linux-gnu \
    '^[[:space:]]+(bl|blr)[[:space:]]' \
    '^[[:space:]].*(^|[^[:alnum:]_])sp([^[:alnum:]_]|$)' \
    '^[[:space:]]+br[[:space:]]+x[0-9]+' \
    '^[[:space:]]+fadd[[:space:]]+v[0-9]+\.4s'

if [ "${UWVM2_CODEGEN_AUDIT_ONLY:-0}" != 1 ]; then
    compile_target x86_64-linux-gnu x86_64-linux-gnu
fi
audit_target \
    x86_64-linux-gnu \
    '^[[:space:]]+callq?[[:space:]]' \
    '^[[:space:]].*(%rsp|pushq?|popq?)([[:space:]]|$)' \
    '^[[:space:]]+jmpq?[[:space:]]+\*%[a-z0-9]+' \
    '^[[:space:]]+addps[[:space:]]'

if [ "${UWVM2_CODEGEN_AUDIT_ONLY:-0}" != 1 ]; then
    compile_target loongarch64-linux-gnu loongarch64-linux-gnu
fi
audit_target \
    loongarch64-linux-gnu \
    '^[[:space:]]+(bl|jirl)[[:space:]]' \
    '^[[:space:]].*\$sp([,[:space:]]|$)' \
    '^[[:space:]]+jr[[:space:]]+\$a[0-9]+' \
    '^[[:space:]]+vfadd\.s[[:space:]]'

# RISC-V is useful as an extra diagnostic. Its baseline ISA does not promise
# unaligned scalar accesses, while the interpreter stream and packed operand
# stack are intentionally unaligned. Compile it on request, but do not apply the
# no-frame primary-target gate to code that must legally synthesize byte loads.
if [ "${UWVM2_CODEGEN_INCLUDE_RISCV:-0}" = 1 ]; then
    if [ "${UWVM2_CODEGEN_AUDIT_ONLY:-0}" != 1 ]; then
        compile_target riscv64-linux-gnu riscv64-linux-gnu
    fi
    printf '%-28s %s\n' riscv64-linux-gnu 'OBSERVED (baseline strict-alignment audit only)'
fi

printf '%s\n' "assembly output: $out_dir"
