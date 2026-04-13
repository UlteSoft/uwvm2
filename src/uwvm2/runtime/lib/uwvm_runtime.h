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

#include <cstddef>
#include <cstdint>

#ifndef UWVM_MODULE
# include <uwvm2/utils/container/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::type { struct uwvm_preload_memory_descriptor_t; }

UWVM_MODULE_EXPORT namespace uwvm2::runtime::uwvm_int
{
    struct full_compile_run_config
    {
        /// @brief The first function index to enter in the main module.
        /// @note  This is the WASM function index space (imports first, then local-defined).
        /// @note  If this points to an imported function, the runtime will fast_terminate().
        ::std::size_t entry_function_index{};
    };

    /// @brief Full-compile and run the main module using the uwvm_int interpreter backend.
    /// @note  This expects uwvm runtime initialization to be complete (runtime storages + import resolution).
    extern "C++" void full_compile_and_run_main_module(::uwvm2::utils::container::u8string_view main_module_name, full_compile_run_config) noexcept;

    [[nodiscard]] ::std::size_t preload_memory_descriptor_count_host_api() noexcept;
    [[nodiscard]] bool preload_memory_descriptor_at_host_api(::std::size_t descriptor_index,
                                                             ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_descriptor_t* out) noexcept;
    [[nodiscard]] bool preload_memory_read_host_api(::std::size_t memory_index, ::std::uint_least64_t offset, void* destination, ::std::size_t size) noexcept;
    [[nodiscard]] bool preload_memory_write_host_api(::std::size_t memory_index, ::std::uint_least64_t offset, void const* source, ::std::size_t size) noexcept;
}  // namespace uwvm2::runtime::uwvm_int
