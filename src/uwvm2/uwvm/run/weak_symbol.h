/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-27
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
# include <type_traits>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/madvise/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
# include <uwvm2/parser/wasm/binfmt/base/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/utils/memory/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/wasm/impl.h>
# include "retval.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
// msvc not support __weak__
UWVM_MODULE_EXPORT extern "C"
{
    struct uwvm_weak_symbol_module_c
    {
        char const* module_name_ptr;
        ::std::size_t module_name_length;
        ::uwvm2::uwvm::wasm::type::capi_custom_handler_vec_t custom_handler_vec;
        ::uwvm2::uwvm::wasm::type::capi_function_vec_t function_vec;
    };

    static_assert(sizeof(uwvm_weak_symbol_module_c) == sizeof(::uwvm2::uwvm::wasm::type::uwvm_weak_symbol_module_t));
    static_assert(alignof(uwvm_weak_symbol_module_c) == alignof(::uwvm2::uwvm::wasm::type::uwvm_weak_symbol_module_t));
    static_assert(__builtin_offsetof(uwvm_weak_symbol_module_c, module_name_ptr) ==
                  __builtin_offsetof(::uwvm2::uwvm::wasm::type::uwvm_weak_symbol_module_t, module_name_ptr));
    static_assert(__builtin_offsetof(uwvm_weak_symbol_module_c, module_name_length) ==
                  __builtin_offsetof(::uwvm2::uwvm::wasm::type::uwvm_weak_symbol_module_t, module_name_length));
    static_assert(__builtin_offsetof(uwvm_weak_symbol_module_c, custom_handler_vec) ==
                  __builtin_offsetof(::uwvm2::uwvm::wasm::type::uwvm_weak_symbol_module_t, custom_handler_vec));
    static_assert(__builtin_offsetof(uwvm_weak_symbol_module_c, function_vec) ==
                  __builtin_offsetof(::uwvm2::uwvm::wasm::type::uwvm_weak_symbol_module_t, function_vec));

    struct uwvm_weak_symbol_module_vector_c
    {
        uwvm_weak_symbol_module_c const* module_ptr;
        ::std::size_t module_count;
    };

    [[__gnu__::__weak__]] [[__gnu__::__used__]] inline uwvm_weak_symbol_module_vector_c const* uwvm_weak_symbol_module() { return nullptr; }
}
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::run
{
    inline constexpr int load_weak_symbol_modules_details([[maybe_unused]] ::uwvm2::uwvm::wasm::type::wasm_parameter_t const& para) noexcept
    {
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
        auto const vec_ptr{uwvm_weak_symbol_module()};
        if(vec_ptr == nullptr) { return static_cast<int>(::uwvm2::uwvm::run::retval::ok); }

        auto const& vec{*vec_ptr};

        if(vec.module_ptr == nullptr && vec.module_count != 0uz) { return static_cast<int>(::uwvm2::uwvm::run::retval::check_module_error); }
        if(vec.module_ptr == nullptr || vec.module_count == 0uz) { return static_cast<int>(::uwvm2::uwvm::run::retval::ok); }

        // Consume each provided weak module like preloaded dl modules.
        auto const modules_begin{vec.module_ptr};
        auto const modules_end{modules_begin + vec.module_count};

        ::uwvm2::uwvm::wasm::storage::weak_symbol.reserve(::uwvm2::uwvm::wasm::storage::weak_symbol.size() + vec.module_count);

        for(auto mod_curr{modules_begin}; mod_curr != modules_end; ++mod_curr)
        {
            ::uwvm2::uwvm::wasm::type::wasm_weak_symbol_t tmp{};

            using uwvm_weak_symbol_module_c_may_alias_t UWVM_GNU_MAY_ALIAS = ::uwvm2::uwvm::wasm::type::uwvm_weak_symbol_module_t const*;

            ::uwvm2::uwvm::wasm::loader::load_weak_symbol(tmp, *reinterpret_cast<uwvm_weak_symbol_module_c_may_alias_t>(mod_curr), para);

            ::uwvm2::uwvm::wasm::storage::weak_symbol.push_back_unchecked(::std::move(tmp));
        }

        return static_cast<int>(::uwvm2::uwvm::run::retval::ok);
#else
        return static_cast<int>(::uwvm2::uwvm::run::retval::ok);
#endif
    }

}  // namespace uwvm2::uwvm::run

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
