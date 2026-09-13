/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @copyright   APL-2.0 License
 */

/****************************************
 *  _   _ __        ____     __ __  __  *
 * | | | |\ \      / /\ \   / /|  \/  | *
 * | | | | \ \ /\ / /  \ \ / / | |\/| | *
 * | |_| |  \ V  V /    \ V /  | |  | | *
 *  \___/    \_/\_/      \_/   |_|  |_| *
 *                                      *
 ****************************************/

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
// macro
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
// import
# include <uwvm2/utils/container/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::type { struct uwvm_preload_memory_descriptor_t; }

UWVM_MODULE_EXPORT namespace uwvm2::runtime::lib
{
    struct entry_function_abi_buffers
    {
        ::std::byte const* param_buffer{};
        ::std::size_t param_bytes{};
        ::std::byte* result_buffer{};
        ::std::size_t result_bytes{};
    };

    struct full_compile_run_config
    {
        /// @brief The first function index to enter in the main module.
        /// @note  This is the WASM function index space (imports first, then local-defined).
        /// @note  Imported entries are only supported when they resolve to a wasm-defined `() -> ()` function.
        ::std::size_t entry_function_index{};
        entry_function_abi_buffers entry_abi_buffers{};
    };

    /// @brief Full-compile and run the main module using the configured runtime backend.
    /// @note  This expects uwvm runtime initialization to be complete (runtime storages + import resolution).
    /// @note  This entry is not reentrant on one thread. Host callbacks that need generated-Wasm re-entry must use
    ///        llvm_jit_call_raw_host_api().
    extern "C++" void full_compile_and_run_main_module(::uwvm2::utils::container::u8string_view main_module_name, full_compile_run_config) noexcept;

    /// @brief Stop every runtime-owned background worker before a WASI proc_exit leaves the normal run loop.
    /// @note On Linux the fast exit path terminates the calling thread directly. The JIT-cache writer must therefore be joined
    ///       explicitly, otherwise it can keep the reduced-runtime process alive indefinitely.
    extern "C++" void runtime_stop_before_proc_exit_host_api() noexcept;

    /// @brief Clear backend-neutral and selected-backend runtime state before loading a fresh module set in the same process.
    /// @note  Embedders must call this before destroying or replacing the runtime module storage referenced by compiled caches.
    /// @note  The caller must first quiesce wasm execution and host API calls. Reset is not a barrier for caller-owned execution
    ///        threads; their surviving TLS caches are invalidated by the runtime generation on next entry.
    /// @note  Same-thread reset during any active runtime execution entry fails closed.
    /// @note  Reset also fails closed from provider callbacks made while this thread owns runtime publication.
    extern "C++" void reset_runtime_state_host_api() noexcept;

#if defined(UWVM_RUNTIME_LLVM_JIT)
    /// @brief Compatibility spelling retained for existing LLVM-AOT embedding callers.
    extern "C++" void llvm_jit_reset_runtime_state_host_api() noexcept;

    /// @brief Invoke one generated function through the public raw ABI; this is the sole same-thread execution-callback re-entry API.
    /// @note  Re-entry from provider callbacks during registry publication fails closed because the registry is incomplete.
    extern "C++" void llvm_jit_call_raw_host_api(void const* runtime_module_ptr,
                                                 ::std::uint_least32_t func_index,
                                                 void* result_buffer,
                                                 ::std::size_t result_bytes,
                                                 void const* param_buffer,
                                                 ::std::size_t param_bytes) noexcept;

#endif

    extern "C++" ::std::size_t preload_memory_descriptor_count_host_api() noexcept;
    extern "C++" bool preload_memory_descriptor_at_host_api(::std::size_t descriptor_index,
                                                            ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_descriptor_t* out) noexcept;
    extern "C++" bool preload_memory_read_host_api(::std::size_t memory_index, ::std::uint_least64_t offset, void* destination, ::std::size_t size) noexcept;
    extern "C++" bool preload_memory_write_host_api(::std::size_t memory_index, ::std::uint_least64_t offset, void const* source, ::std::size_t size) noexcept;
}  // namespace uwvm2::runtime::lib

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
#endif
