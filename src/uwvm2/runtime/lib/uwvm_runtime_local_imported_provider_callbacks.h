/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#if defined(UWVM_RUNTIME_LLVM_JIT)
namespace uwvm2::runtime::lib::details
{
    // Keep this bridge ABI independent of the local-imported provider type.  Besides keeping the runtime/lib boundary
    // narrow, this lets module compiler partitions see one declaration from their global module fragments instead of
    // attaching provider-type redeclarations to multiple named modules.
    struct local_imported_provider_memory_snapshot_t
    {
        ::std::byte* memory_begin{};
        ::std::uint_least64_t page_count{};
    };

    struct local_imported_provider_function_signature_t
    {
        // Own normalized Wasm value-type bytes. No provider-owned pointer is allowed to escape the guarded metadata
        // callback, even though the built-in type-erasure adapter currently sources them from constexpr arrays.
        ::std::vector<::std::uint_least8_t> parameter_types{};
        ::std::vector<::std::uint_least8_t> result_types{};
    };

    // A local-imported provider is native extension code, even when the operation looks like an ordinary metadata,
    // global, or memory query. These narrow runtime entry points apply the same conservative host-callback boundary as
    // imported functions: suspend the generated-only bridge capability and preserve an active LLVM-Wasm FP environment.
    extern "C++" void invoke_local_imported_provider_global_get(void* module,
                                                                 ::std::size_t global_index,
                                                                 ::std::byte* out) noexcept;
    extern "C++" bool invoke_local_imported_provider_global_set(void* module,
                                                                 ::std::size_t global_index,
                                                                 ::std::byte const* in) noexcept;
    extern "C++" ::std::uint_least8_t
        invoke_local_imported_provider_function_wasm_fp_control_policy(void const* module,
                                                                       ::std::size_t function_index) noexcept;
    extern "C++" bool invoke_local_imported_provider_function_signature(
        void const* module,
        ::std::size_t function_index,
        local_imported_provider_function_signature_t& out) noexcept;

    extern "C++" ::std::uint_least64_t
        invoke_local_imported_provider_memory_page_size(void const* module,
                                                        ::std::size_t memory_index) noexcept;
    // Compilation uses a distinct entry point so lazy single-CU materialization can reject public raw/reset re-entry
    // even when it is not currently holding the global runtime-state publication lock.
    extern "C++" ::std::uint_least64_t
        invoke_local_imported_provider_memory_page_size_for_compilation(void const* module,
                                                                        ::std::size_t memory_index) noexcept;
    extern "C++" bool invoke_local_imported_provider_memory_access_snapshot(
        void* module,
        ::std::size_t memory_index,
        local_imported_provider_memory_snapshot_t& out) noexcept;
    extern "C++" bool invoke_local_imported_provider_memory_read(void* module,
                                                                  ::std::size_t memory_index,
                                                                  ::std::uint_least64_t offset,
                                                                  void* destination,
                                                                  ::std::size_t size) noexcept;
    extern "C++" bool invoke_local_imported_provider_memory_write(void* module,
                                                                   ::std::size_t memory_index,
                                                                   ::std::uint_least64_t offset,
                                                                   void const* source,
                                                                   ::std::size_t size) noexcept;
    extern "C++" bool invoke_local_imported_provider_memory_try_grow(void* module,
                                                                      ::std::size_t memory_index,
                                                                      ::std::uint_least64_t delta_pages,
                                                                      ::std::size_t max_limit_memory_length,
                                                                      ::std::uint_least64_t* old_page_size_out) noexcept;
}
#endif
