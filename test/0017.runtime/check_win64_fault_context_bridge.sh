#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
memory_error="$repo_root/src/uwvm2/object/memory/error/error.h"
signal_handler="$repo_root/src/uwvm2/object/memory/signal/signal.h"
runtime_impl="$repo_root/src/uwvm2/runtime/lib/uwvm_runtime.default.cpp"

fail() {
    printf 'Win64 fault-context bridge check: %s\n' "$*" >&2
    exit 1
}

if rg -n 'platform_context' "$memory_error"; then
    fail 'public mmap error ABI contains a native-context field'
fi
rg -q 'mmap_memory_out_of_bounds_with_context_func_t' "$signal_handler" ||
    fail 'signal layer has no ABI-preserving native-context callback'
rg -U -q 'handler\(mmapmemerr, platform_context\);\s*::fast_io::fast_terminate\(\);\s*::std::unreachable\(\);' "$signal_handler" ||
    fail 'signal dispatcher does not pass native context separately from the public error aggregate'
rg -q 'get_windows_fault_context\(exception_pointers->ContextRecord\)' "$signal_handler" ||
    fail 'VEH does not read the fault-time CONTEXT'
rg -U -q 'handle_fault_address_with_context\(fault_addr,\s*instruction_address,\s*fault_context\.frame_address,\s*fault_context\.stack_pointer,\s*fault_context\.platform_context\)' \
    "$signal_handler" || fail 'VEH does not forward the complete context view'
rg -q 'handle_fault_address_with_context\(fault_addr, instruction_address, 0u, 0u, nullptr\)' "$signal_handler" ||
    fail 'POSIX fault handling does not explicitly reject native context seeding'

if rg -n '(static_cast|reinterpret_cast).*context_record|context_record.*(static_cast|reinterpret_cast)|\*\s*\([^)]*\*[^)]*\)\s*context_record' \
    "$signal_handler"; then
    fail 'OS CONTEXT is dereferenced through an unrelated C++ object type'
fi
context_copy_count="$(rg -c 'memcpy\(::std::addressof\(context\), context_record, sizeof\(context\)\)' "$signal_handler" || true)"
[[ "$context_copy_count" == 3 ]] || fail 'all three supported Windows CONTEXT layouts must use bytewise copies'
if rg -n 'memerr\.platform_context' "$runtime_impl"; then
    fail 'runtime reads native context from the public error aggregate'
fi
rg -q 'set_mmap_memory_out_of_bounds_with_context_handler' "$runtime_impl" ||
    fail 'runtime does not install the ABI-preserving native-context callback'

python3 - "$runtime_impl" <<'PY' || fail 'Win64 ARM64 leaf unwind does not use only the fault-time LR'
import sys
from pathlib import Path

source = Path(sys.argv[1]).read_text(encoding="utf-8")

store_start = source.rindex("UWVM_NOINLINE inline constexpr void store_llvm_jit_win64_trap_caller_context")
store_end = source.index("[[nodiscard]] inline constexpr bool load_llvm_jit_win64_trap_caller_context", store_start)
store_context = source[store_start:store_end]
for required in (
    "void const* platform_context",
    "memcpy(::std::addressof(context), platform_context, sizeof(context))",
    "llvm_jit_win64_context_set_instruction_pointer(context, expected_return_address)",
):
    if required not in store_context:
        raise SystemExit(f"runtime context store is missing {required}")

reporter_start = source.index("inline constexpr void print_mmap_memory_out_of_bounds_trap")
reporter_end = source.index("inline constexpr void ensure_memory_signal_trap_bridge_initialized", reporter_start)
reporter = source[reporter_start:reporter_end]
if "void const* platform_context" not in reporter or "memerr.platform_context" in reporter:
    raise SystemExit("mmap reporter does not receive context through the separate callback argument")
if not __import__("re").search(
    r"store_llvm_jit_win64_trap_caller_context\(.*?memerr\.instruction_address\),\s*"
    r"memerr\.frame_address,\s*memerr\.stack_pointer,\s*platform_context\);",
    reporter,
    __import__("re").S,
):
    raise SystemExit("mmap reporter drops the separate native-context argument")

function_start = source.index("llvm_jit_win64_virtual_unwind_once")
leaf_start = source.index("if(function_entry == nullptr)", function_start)
arm64_start = source.index("#  if defined(_WIN64) && (defined(__aarch64__)", leaf_start)
arm64_end = source.index("#  else", arm64_start)
arm64_leaf = source[arm64_start:arm64_end]

for required in (
    "llvm_jit_win64_context_link_register(context)",
    "llvm_jit_win64_context_set_instruction_pointer(context, return_address)",
    "return true;",
):
    if required not in arm64_leaf:
        raise SystemExit(f"ARM64 leaf unwind is missing {required}")
for forbidden in (
    "llvm_jit_load_frame_record_word",
    "llvm_jit_win64_context_frame_pointer(context)",
    "llvm_jit_win64_context_stack_pointer(context)",
    "next_frame_pointer",
):
    if forbidden in arm64_leaf:
        raise SystemExit(f"ARM64 leaf unwind reconstructs a frame through {forbidden}")
PY

printf 'Win64 fault-context bridge check: ok\n'
