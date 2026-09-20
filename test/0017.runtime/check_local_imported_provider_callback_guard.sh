#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
single_emit="${repo_root}/src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_emit.h"
llvm_compiler="${repo_root}/src/uwvm2/runtime/compiler/llvm_jit"
int_variable="${repo_root}/src/uwvm2/runtime/compiler/uwvm_int/optable/variable.h"
runtime_impl="${repo_root}/src/uwvm2/runtime/lib/uwvm_runtime.default.cpp"
provider_api="${repo_root}/src/uwvm2/runtime/lib/uwvm_runtime_local_imported_provider_callbacks.h"
llvm_translate_module="${repo_root}/src/uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate.cppm"
int_variable_module="${repo_root}/src/uwvm2/runtime/compiler/uwvm_int/optable/variable.cppm"
build_config="${repo_root}/xmake.lua"

direct_pattern='->(global_get_from_index|global_set_from_index|memory_page_size_from_index|memory_try_grow_from_index|memory_access_snapshot_from_index|memory_read_from_index|memory_write_to_index)'
if rg -n --regexp "${direct_pattern}" "${llvm_compiler}"; then
    echo "generated LLVM provider virtual call bypasses the runtime callback guard" >&2
    exit 1
fi

for operation in global_get global_set memory_page_size memory_access_snapshot memory_read memory_write memory_try_grow; do
    symbol="invoke_local_imported_provider_${operation}"
    rg -q --fixed-strings "${symbol}" "${provider_api}"
    rg -q --fixed-strings "${symbol}" "${runtime_impl}"
    rg -q --fixed-strings "${symbol}" "${single_emit}"
done

compilation_page_size_symbol='invoke_local_imported_provider_memory_page_size_for_compilation'
rg -q --fixed-strings "${compilation_page_size_symbol}" "${provider_api}"
rg -q --fixed-strings "${compilation_page_size_symbol}" "${runtime_impl}"
[[ "$(rg -c --fixed-strings "${compilation_page_size_symbol}" "${single_emit}")" == 1 ]] || {
    echo "LLVM compile-time page-size resolution does not use exactly one metadata-phase wrapper" >&2
    exit 1
}

policy_symbol='invoke_local_imported_provider_function_wasm_fp_control_policy'
rg -q --fixed-strings "${policy_symbol}" "${provider_api}"
rg -q --fixed-strings "${policy_symbol}" "${runtime_impl}"
rg -q --fixed-strings 'query_local_imported_function_wasm_fp_control_policy' "${runtime_impl}"
signature_symbol='invoke_local_imported_provider_function_signature'
rg -q --fixed-strings "${signature_symbol}" "${provider_api}"
rg -q --fixed-strings "${signature_symbol}" "${runtime_impl}"
rg -q --fixed-strings 'std::vector<::std::uint_least8_t> parameter_types' "${provider_api}"
rg -q --fixed-strings 'std::vector<::std::uint_least8_t> result_types' "${provider_api}"
if rg -q --regexp '(parameter|result)_begin' "${provider_api}"; then
    echo "provider signature bridge still exports borrowed pointer ranges" >&2
    exit 1
fi
if rg -q --regexp 'memory_begin_address(_out)?|local_imported\.memory\.begin\.addr' "${single_emit}"; then
    echo "local-imported memory.size bridge still exports an unused provider pointer" >&2
    exit 1
fi
[[ "$(rg -c --fixed-strings 'runtime_compilation_metadata_callback_scope metadata_callback_scope{};' "${runtime_impl}")" == 3 ]] || {
    echo "signature, FP policy, and compile-time page-size provider calls must enter the metadata callback phase" >&2
    exit 1
}

# Combined tiered/LLVM builds suspend the generated token through the runtime wrapper. Pure-interpreter optables stay
# header-only, but their direct virtual calls must still preserve FP state before the musttail continuation.
rg -q --fixed-strings 'invoke_local_imported_provider_global_get' "${int_variable}"
rg -q --fixed-strings 'invoke_local_imported_provider_global_set' "${int_variable}"
rg -q --fixed-strings 'local_imported_module->global_get_from_index' "${int_variable}"
rg -q --fixed-strings 'local_imported_module->global_set_from_index' "${int_variable}"
[[ "$(rg -c --fixed-strings 'scoped_wasm_host_fp_control_restore fp_environment_guard{};' "${int_variable}")" == 2 ]] || {
    echo "pure-interpreter provider globals are missing their FP callback guards" >&2
    exit 1
}
if rg -q --fixed-strings 'get_llvm_wasm_fp_environment_active_marker(), runtime_compiler_requests_llvm_jit_translation()' "${runtime_impl}"; then
    echo "full Wasm FP protection still depends on selecting LLVM" >&2
    exit 1
fi
rg -q --fixed-strings 'inline constinit thread_local bool g_interpreter_wasm_fp_environment_active{};' "${runtime_impl}"
rg -q --fixed-strings 'return g_interpreter_wasm_fp_environment_active;' "${runtime_impl}"

# Header-driven 0013 tests instantiate the guarded branch whenever both backends are configured. The wrapper bodies live
# in uwvm_runtime, so the test target must carry that object dependency or the safety bridge becomes an undefined symbol.
combined_int_test_dependency="$(sed -n '/if uwvm_uses_llvm_jit and is_0013_uwvm_int then/,/^[[:space:]]*end$/p' "${build_config}")"
grep -q --fixed-strings 'add_deps("uwvm_runtime")' <<<"${combined_int_test_dependency}" || {
    echo "combined interpreter/LLVM tests do not link the guarded provider bridge implementation" >&2
    exit 1
}

guarded_definition_count="$(sed -n '/namespace details/,/namespace details/p' "${runtime_impl}" | rg -c 'invoke_host_preserving_(llvm_wasm_fp_environment|wasm_fp_control)')"
if [[ "${guarded_definition_count}" -lt 9 ]]; then
    echo "not every local-imported provider wrapper uses the conservative host callback scope" >&2
    exit 1
fi

# The bridge declaration is an opaque runtime ABI. Module partitions include it in their global module fragment so a
# declaration is never attached independently to the LLVM and interpreter named modules.
if rg -q --fixed-strings 'uwvm2::uwvm::wasm::type' "${provider_api}"; then
    echo "provider callback bridge leaked compiler/provider types across the runtime ABI" >&2
    exit 1
fi
for module_unit in "${llvm_translate_module}" "${int_variable_module}"; do
    include_line="$(rg -n --fixed-strings 'uwvm_runtime_local_imported_provider_callbacks.h' "${module_unit}" | cut -d: -f1)"
    export_line="$(rg -n '^export module ' "${module_unit}" | cut -d: -f1)"
    if [[ -z "${include_line}" || -z "${export_line}" || "${include_line}" -ge "${export_line}" ]]; then
        echo "provider callback bridge is not declared in the global module fragment: ${module_unit}" >&2
        exit 1
    fi
done

echo "local-imported provider callback guard audit: PASS"
