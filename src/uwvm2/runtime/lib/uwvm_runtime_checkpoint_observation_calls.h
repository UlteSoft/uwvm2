// Include after actual import-cache/effect classification. Observation keeps
// genuine defined-Wasm forwarding inside its logical caller chain. Foreign
// providers retain the host island; neither branch grants replay authority.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT)
extern "C++" void details::llvm_jit_checkpoint_observe_call_raw_from_generated_wasm_abi_bridge(
    ::std::uintptr_t module_address, ::std::uintptr_t function_index,
    ::std::uintptr_t result, ::std::size_t result_bytes,
    ::std::uintptr_t parameters, ::std::size_t parameter_bytes) UWVM_THROWS
{
    if(get_llvm_jit_generated_wasm_bridge_entry_depth()==0u) [[unlikely]] { ::fast_io::fast_terminate(); }
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    if(checkpoint_effect_actual_import(module_address,function_index,parameter_bytes,result_bytes)==
       checkpoint_resolved_effect::defined_wasm)
    {
        // Exact native source/publication/cache/ABI proof precedes entry. The
        // original dispatcher still binds the target's environment and memory.
        details::llvm_jit_call_raw_from_generated_wasm_abi_bridge(
            module_address,function_index,result,result_bytes,parameters,parameter_bytes);
        return;
    }
#endif
    // An unknown/unadapted target keeps the original incomplete-host-island
    // distinction, including nested callback entry and native C++ unwinding.
    details::llvm_jit_debug_host_scope foreign{};
    details::llvm_jit_call_raw_from_generated_wasm_abi_bridge(
        module_address,function_index,result,result_bytes,parameters,parameter_bytes);
}
#endif
