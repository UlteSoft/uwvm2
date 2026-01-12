/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.0 (2019-07-20)
 * @details     antecedent dependency: null
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-07-03
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
# include <cstring>
# include <concepts>
# include <type_traits>
# include <utility>
# include <memory>
# include <limits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/section/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/opcode/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include "def.h"
# include "feature_def.h"
# include "types.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    struct wasm1_final_check
    {
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void define_final_check([[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<wasm1_final_check> final_adl,
                                             [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> &
                                                 module_storage,
                                             [[maybe_unused]] ::std::byte const* const module_end,
                                             [[maybe_unused]] ::uwvm2::parser::wasm::base::error_impl& err,
                                             [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // Verify whether the number of defined functions matches the number of code entries (funcsec.funcs.size() == codesec.codes.size()).
        // A mismatch means the module is malformed (e.g. function section exists but code section is missing, or vice versa).
        auto const& codesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<code_section_storage_t<Fs...>>(module_storage.sections)};
        auto const defined_code_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(codesec.codes.size())};

        auto const& funcsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<function_section_storage_t>(module_storage.sections)};
        auto const defined_func_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(funcsec.funcs.size())};

        if(defined_code_count != defined_func_count) [[unlikely]]
        {
            // Use a stable in-module pointer for error reporting (avoid null when the code section is missing).
            ::std::byte const* err_pos{module_end};
            if(codesec.sec_span.sec_begin != nullptr) { err_pos = reinterpret_cast<::std::byte const*>(codesec.sec_span.sec_begin); }
            else if(funcsec.sec_span.sec_begin != nullptr) { err_pos = reinterpret_cast<::std::byte const*>(funcsec.sec_span.sec_begin); }

            err.err_curr = err_pos;
            err.err_selectable.u32arr[0] = defined_code_count;
            err.err_selectable.u32arr[1] = defined_func_count;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::code_ne_defined_func;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
