/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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
# include <cstdint>
# include <cstddef>
# include <cstring>
# include <new>
# include <memory>
# include <type_traits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/features/impl.h>
# include <uwvm2/parser/wasm_custom/customs/impl.h>
# include <uwvm2/uwvm/wasm/base/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include "para.h"
# include "file.h"
# include "local_imported.h"
# include "dl.h"
# include "weak_symbol.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::type
{
    enum class module_type_t : unsigned
    {
        // The running WASM module, initialization begins from this module.
        exec_wasm,  // wasm_file

        // Pre-initialized WASM modules, similar to system DLLs.
        preloaded_wasm,  // wasm_file

#if defined(UWVM_SUPPORT_PRELOAD_DL)
                         // Dynamically loaded WASM binary import API, suitable for use as a plugin system, only supports platforms capable of dynamically
                         // loading DLLs. Currently, only the module import function is available.
        preloaded_dl,  // wasm_dl
#endif

#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
                       // Static WASM modules implemented with weak symbols, available across all platforms, specified during linking, primarily for embedded
                       // environments. Currently, only the module import function is available.
        weak_symbol,  // wasm_weak_symbol
#endif

        // Local import, uwvm2 is a custom-built module that supports full WASM module functionality through conceptual implementation.
        // Enables custom functionality for complete WASM modules through the parser concept.
        local_import  // wasip1, wasip2, ...
    };

    // binfmt version 1
    using binfmt_ver1_feature_list_t =
        decltype(::uwvm2::uwvm::wasm::type::get_feature_list_from_tuple_impl(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));
    using local_imported_t =
        decltype(::uwvm2::uwvm::wasm::type::get_local_imported_module_from_feature_list(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));

    // all module
    union module_storage_ptr_u
    {
        // exec_wasm
        // preloaded_wasm
        ::uwvm2::uwvm::wasm::type::wasm_file_t const* wf;

#if defined(UWVM_SUPPORT_PRELOAD_DL)
        // preloaded_dl
        ::uwvm2::uwvm::wasm::type::wasm_dl_t const* wd;
#endif

#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
        // weak_symbol
        ::uwvm2::uwvm::wasm::type::wasm_weak_symbol_t const* wws;
#endif

        local_imported_t const* li;
    };

    struct all_module_t
    {
        module_storage_ptr_u module_storage_ptr{};
        module_type_t type{};
    };

    // all module export

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_binfmt_ver1_final_export_type_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1::features::final_export_type_t<Fs...>{}; }

    // exec_wasm
    // preloaded_wasm
    struct wasm_file_export_t
    {
        using wasm_binfmt_ver1_export_storage_t = decltype(get_binfmt_ver1_final_export_type_from_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features));

        union wasm_file_export_storage_u
        {
            wasm_binfmt_ver1_export_storage_t const* wasm_binfmt_ver1_export_storage_ptr;
        };

        wasm_file_export_storage_u storage{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_ver{1u};

        // wasm only supported binfmt version 1 currently
        static_assert(::uwvm2::uwvm::wasm::feature::max_binfmt_version == 1u, "missing implementation of other binfmt version");
    };

#if defined(UWVM_SUPPORT_PRELOAD_DL)
    // preloaded_dl
    struct wasm_dl_export_t
    {
        using wasm_dl_export_storage_t = ::uwvm2::uwvm::wasm::type::capi_function_t;
        wasm_dl_export_storage_t const* storage{};
    };
#endif

#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
    // weak_symbol
    struct wasm_weak_symbol_export_t
    {
        using wasm_weak_symbol_export_storage_t = ::uwvm2::uwvm::wasm::type::capi_function_t;
        wasm_weak_symbol_export_storage_t const* storage{};
    };
#endif

    enum class local_imported_export_type_t : unsigned
    {
        func,
        global,
        memory
    };

    struct local_imported_export_t
    {
        local_imported_t const* storage{};
        ::std::size_t index{};
        local_imported_export_type_t type{};
    };

    struct all_module_export_t
    {
        // exec_wasm
        // preloaded_wasm
        using wasm_file_export_storage_t = wasm_file_export_t;

#if defined(UWVM_SUPPORT_PRELOAD_DL)
        // preloaded_dl
        using wasm_dl_export_storage_t = wasm_dl_export_t;
#endif

#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
        // weak_symbol
        using wasm_weak_symbol_export_storage_t = wasm_weak_symbol_export_t;
#endif

        // local_import
        using local_imported_export_storage_t = local_imported_export_t;

        union all_module_export_storage_u
        {
            // exec_wasm
            // preloaded_wasm
            wasm_file_export_storage_t wasm_file_export_storage_ptr;
            static_assert(::std::is_trivially_copyable_v<wasm_file_export_storage_t> && ::std::is_trivially_destructible_v<wasm_file_export_storage_t>);

#if defined(UWVM_SUPPORT_PRELOAD_DL)
            // preloaded_dl
            wasm_dl_export_storage_t wasm_dl_export_storage_ptr;
            static_assert(::std::is_trivially_copyable_v<wasm_dl_export_storage_t> && ::std::is_trivially_destructible_v<wasm_dl_export_storage_t>);
#endif

#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
            // weak_symbol
            wasm_weak_symbol_export_storage_t wasm_weak_symbol_export_storage_ptr;
            static_assert(::std::is_trivially_copyable_v<wasm_weak_symbol_export_storage_t> &&
                          ::std::is_trivially_destructible_v<wasm_weak_symbol_export_storage_t>);
#endif

            // local_import
            local_imported_export_storage_t local_imported_export_storage_ptr;
            static_assert(::std::is_trivially_copyable_v<local_imported_export_storage_t> &&
                          ::std::is_trivially_destructible_v<local_imported_export_storage_t>);
        };

        all_module_export_storage_u storage{};

        module_type_t type{};
    };

    // map<module_name, map<export_name, all_module_export_t>>

}  // namespace uwvm2::uwvm::wasm::type

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
