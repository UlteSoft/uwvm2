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
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm_custom/customs/impl.h>
# include <uwvm2/uwvm/wasm/base/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include "para.h"
# include "cwrapper.h"
# include "preload_module_attribute.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::type
{

    // There is no need to add the UWVM_SUPPORT_PRELOAD_DL macro here, as it will be reused by the weak symbol module.

    extern "C"
    {
        struct capi_module_name_t
        {
            char const* name;
            ::std::size_t name_length;
        };

        /// @brief uwvm_get_module_name
        using capi_get_module_name_t = capi_module_name_t (*)();

        struct capi_custom_handler_t
        {
            char const* custom_name_ptr;
            ::std::size_t custom_name_length;
            ::uwvm2::uwvm::wasm::type::imported_c_handlefunc_ptr_t custom_handle_func;
        };

        struct capi_custom_handler_vec_t
        {
            capi_custom_handler_t const* custom_handler_begin;
            ::std::size_t custom_handler_size;
        };

        /// @brief uwvm_get_custom_handler_vec
        using capi_get_custom_handler_vec_t = capi_custom_handler_vec_t (*)();

        using capi_wasm_function = void (*)(::std::byte* res, ::std::byte* para);

        /*
            `capi_wasm_function` ABI and preload-DL authoring example:
            the parameter/result structs must use `packed` or `aligned(1)`.

            ```cpp
            #include <cstddef>
            #include <cstdint>
            #include "dl.h"
            #include "preload_api.h"

            using namespace uwvm2::uwvm::wasm::type;

            struct __attribute__((packed)) add_para
            {
                ::std::uint_least32_t lhs;
                ::std::uint_least32_t rhs;
            };

            struct __attribute__((packed)) add_res
            {
                ::std::uint_least32_t value;
            };

            static uwvm_preload_host_api_v1 const* g_preload_host_api{};
            static char const module_name[] = "demo";
            static char const function_name[] = "add_i32";
            static ::std::uint_least8_t const para_types[] = {WASM_VALTYPE_I32, WASM_VALTYPE_I32};
            static ::std::uint_least8_t const res_types[] = {WASM_VALTYPE_I32};

            extern "C" void uwvm_set_preload_host_api_v1(uwvm_preload_host_api_v1 const* api) noexcept
            {
                g_preload_host_api = api;
            }

            extern "C" capi_module_name_t uwvm_get_module_name() noexcept
            {
                return capi_module_name_t{module_name, sizeof(module_name) - 1u};
            }

            extern "C" void add_i32(add_res* result, add_para const* para) noexcept
            {
                result->value = para->lhs + para->rhs;
            }

            extern "C" capi_function_vec_t uwvm_function() noexcept
            {
                static capi_function_t const functions[] = {
                    {function_name,
                     sizeof(function_name) - 1u,
                     para_types,
                     sizeof(para_types) / sizeof(para_types[0]),
                     res_types,
                     sizeof(res_types) / sizeof(res_types[0]),
                     reinterpret_cast<capi_wasm_function>(&add_i32)},
                };
                return capi_function_vec_t{functions, sizeof(functions) / sizeof(functions[0])};
            }
            ```

            Recommended plugin authoring workflow:
            - export `uwvm_get_module_name()`;
            - export `uwvm_function()`;
            - export `uwvm_get_custom_handler()` only when you actually provide custom sections;
            - export `uwvm_set_preload_host_api_v1()` when the plugin wants the stable host API;
            - keep wasm-facing ABI structs trivial, packed, and byte-layout stable;
            - route all memory interaction through the delivery-state contract documented in `preload_api.h`.

            If memory access is required, prefer the stable preload host API from `preload_api.h`
            instead of relying on internal hooks or runtime-private layouts.
        */

        struct capi_function_t
        {
            char const* func_name_ptr;
            ::std::size_t func_name_length;

            ::std::uint_least8_t const* para_type_vec_begin;
            ::std::size_t para_type_vec_size;

            ::std::uint_least8_t const* res_type_vec_begin;
            ::std::size_t res_type_vec_size;

            capi_wasm_function func_ptr;
        };

        struct capi_function_vec_t
        {
            capi_function_t const* function_begin;
            ::std::size_t function_size;
        };

        /// @brief uwvm_get_function_vec_t
        using capi_get_function_vec_t = capi_function_vec_t (*)();
    }

    struct wasm_dl_storage_t
    {
        capi_get_module_name_t get_module_name{};
        capi_get_custom_handler_vec_t get_custom_handler_vec{};
        capi_get_function_vec_t get_function_vec{};

        capi_module_name_t capi_module_name{};
        capi_custom_handler_vec_t capi_custom_handler_vec{};
        capi_function_vec_t capi_function_vec{};
    };

#if defined(UWVM_SUPPORT_PRELOAD_DL)
    // the wasm_dl_t only supported on the platform which make the UWVM_SUPPORT_PRELOAD_DL macro defined
    struct wasm_dl_t
    {
        // wasm file name
        ::uwvm2::utils::container::u8cstring_view file_name{};
        // Accurate module names that must work
        ::uwvm2::utils::container::u8string_view module_name{};
        // DL File
        ::fast_io::native_dll_file import_dll_file{};
        // DL handler
        wasm_dl_storage_t wasm_dl_storage{};
        // wasm_parameter_t
        ::uwvm2::uwvm::wasm::type::wasm_parameter_t wasm_parameter{};
        // preload memory attribute
        ::uwvm2::uwvm::wasm::type::preload_module_memory_attribute_t preload_module_memory_attribute{};
    };
#endif
}  // namespace uwvm2::uwvm::wasm::storage

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
