#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
runtime_source="$repo_root/src/uwvm2/runtime/lib/uwvm_runtime.default.cpp"
module_source="$repo_root/src/uwvm2/runtime/lib/uwvm_runtime.module.cpp"

fail() {
    printf 'runtime execution-entry policy: %s\n' "$*" >&2
    exit 1
}

grep -Fq '# include "uwvm_runtime_execution_entry.h"' "$runtime_source" || fail 'traditional runtime does not include the policy header'
grep -Fq '#include "uwvm_runtime_execution_entry.h"' "$module_source" || fail 'module wrapper does not provide the policy header'
grep -Fq 'inline thread_local ::std::size_t g_runtime_execution_entry_depth{};' "$runtime_source" || fail 'TLS depth is missing'
grep -Fq 'return get_thread_state().runtime_execution_entry_depth;' "$runtime_source" || fail 'map-backed depth is missing'
grep -Fq 'inline thread_local ::std::size_t g_runtime_state_publication_depth{};' "$runtime_source" || fail 'TLS publication ownership depth is missing'
grep -Fq 'return get_thread_state().runtime_state_publication_depth;' "$runtime_source" || fail 'map-backed publication ownership depth is missing'
grep -Fq 'inline thread_local ::std::size_t g_runtime_compilation_metadata_callback_depth{};' "$runtime_source" ||
    fail 'TLS compilation-metadata callback depth is missing'
grep -Fq 'return get_thread_state().runtime_compilation_metadata_callback_depth;' "$runtime_source" ||
    fail 'map-backed compilation-metadata callback depth is missing'
grep -Fq 'runtime_compilation_metadata_callback_enter(' "$runtime_source" || fail 'compilation-metadata callback scope enter is missing'
grep -Fq 'runtime_compilation_metadata_callback_leave(' "$runtime_source" || fail 'compilation-metadata callback scope leave is missing'
metadata_scope="$(sed -n '/class runtime_compilation_metadata_callback_scope/,/^        };/p' "$runtime_source")"
grep -Fq 'owns_inserted_node = g_thread_states.try_emplace_and_visit(' <<<"$metadata_scope" ||
    fail 'map-backed compilation-metadata scope does not record insertion ownership'
grep -Fq 'g_thread_states.visit(' <<<"$metadata_scope" ||
    fail 'map-backed compilation-metadata scope does not re-acquire its node on leave'
grep -Fq 'runtime_compilation_metadata_callback_owned_node_should_erase(' <<<"$metadata_scope" ||
    fail 'map-backed compilation-metadata scope does not gate owner cleanup on outermost leave'
grep -Fq 'g_thread_states.erase(thread_id)' <<<"$metadata_scope" ||
    fail 'metadata-only worker node cleanup is missing'
if grep -Fq 'Currently only the main thread is used' "$runtime_source"; then
    fail 'fallback-map ownership documentation still assumes main-thread-only runtime use'
fi
grep -Fq 'runtime_execution_entry_leave(depth)' "$runtime_source" || fail 'entry RAII does not re-acquire and leave the current-thread depth'
grep -Fq 'runtime_execution_entry_leave_result::outermost' "$runtime_source" || fail 'outermost-only cleanup is missing'
grep -Fq 'erase_current_thread_runtime_state();' "$runtime_source" || fail 'complete call-stack plus scratch cleanup is missing'

ordinary_expected=1
if grep -Fq 'void lazy_compile_and_run_main_module' "$runtime_source"; then
    ordinary_expected=2
fi
ordinary_actual="$(grep -Fc 'runtime_execution_entry_scope execution_entry_scope{};' "$runtime_source")"
[[ "$ordinary_actual" == "$ordinary_expected" ]] || fail "expected $ordinary_expected non-reentrant full/lazy scopes, found $ordinary_actual"

[[ "$(grep -Fc 'runtime_execution_entry_reentry::allow_public_llvm_raw' "$runtime_source")" == 1 ]] ||
    fail 'public LLVM raw must be the sole re-entry-enabled execution API'
grep -Fq 'runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth())' "$runtime_source" ||
    fail 'reset does not reject every active backend-neutral execution entry'
[[ "$(grep -Fc 'runtime_state_publication_access_allowed(get_runtime_state_publication_depth())' "$runtime_source")" == 3 ]] ||
    fail 'publication ownership must gate guard acquisition, public raw entry, and reset'
[[ "$(grep -Fc 'runtime_compilation_metadata_callback_access_allowed(' "$runtime_source")" == 2 ]] ||
    fail 'compilation-metadata callback depth must gate public raw entry and reset'
grep -Fq 'runtime_state_publication_leave(get_runtime_state_publication_depth())' "$runtime_source" ||
    fail 'publication guard does not re-acquire ownership storage on leave'

reset_body="$(sed -n '/extern "C++" void reset_runtime_state_host_api()/,/^    }/p' "$runtime_source")"
reset_metadata_gate_line="$(grep -n -m1 'runtime_compilation_metadata_callback_access_allowed' <<<"$reset_body" | cut -d: -f1)"
reset_gate_line="$(grep -n -m1 'runtime_state_publication_access_allowed' <<<"$reset_body" | cut -d: -f1)"
reset_lock_line="$(grep -n -m1 'runtime_state_publication_guard runtime_state_guard' <<<"$reset_body" | cut -d: -f1)"
reset_erase_line="$(grep -n -m1 'erase_current_thread_runtime_state();' <<<"$reset_body" | cut -d: -f1)"
reset_guard_end_line="$(grep -n -m1 'Release current-thread publication ownership before erasing' <<<"$reset_body" | cut -d: -f1)"
[[ -n "$reset_gate_line" && -n "$reset_lock_line" && "$reset_gate_line" -lt "$reset_lock_line" ]] ||
    fail 'reset publication-depth gate is not before publication lock acquisition'
[[ -n "$reset_metadata_gate_line" && -n "$reset_gate_line" && "$reset_metadata_gate_line" -lt "$reset_gate_line" ]] ||
    fail 'reset compilation-metadata gate is not the first re-entry gate'
[[ -n "$reset_guard_end_line" && -n "$reset_erase_line" && "$reset_guard_end_line" -lt "$reset_erase_line" ]] ||
    fail 'reset does not release publication ownership before erasing map-backed state'

raw_body="$(sed -n '/extern "C++" void llvm_jit_call_raw_host_api(/,/^    }/p' "$runtime_source")"
raw_metadata_gate_line="$(grep -n -m1 'runtime_compilation_metadata_callback_access_allowed' <<<"$raw_body" | cut -d: -f1)"
raw_gate_line="$(grep -n -m1 'runtime_state_publication_access_allowed' <<<"$raw_body" | cut -d: -f1)"
raw_compile_line="$(grep -n -m1 'ensure_llvm_jit_raw_call_runtime_ready' <<<"$raw_body" | cut -d: -f1)"
[[ -n "$raw_gate_line" && -n "$raw_compile_line" && "$raw_gate_line" -lt "$raw_compile_line" ]] ||
    fail 'public raw publication-depth gate is not before publication/compilation lock acquisition'
[[ -n "$raw_metadata_gate_line" && -n "$raw_gate_line" && "$raw_metadata_gate_line" -lt "$raw_gate_line" ]] ||
    fail 'public raw compilation-metadata gate is not the first re-entry gate'

if grep -Eq 'current_thread_(runtime_)?state_erase_guard' "$runtime_source"; then
    fail 'legacy unconditional per-thread erase guard remains'
fi

printf 'runtime execution-entry policy: ok\n'
