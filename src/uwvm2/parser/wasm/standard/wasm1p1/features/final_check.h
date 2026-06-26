/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.1 (Draft 2021-11-16)
 * @details     Final parser consistency checks
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-26
 * @copyright   APL-2.0 License
 */

#pragma once

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <concepts>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include "data_count_section.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1p1::features
{
    /// @brief Final module check tag for wasm1.1.
    struct wasm1p1_final_check
    {
    };

    /// @brief Run wasm1 final checks and then verify the optional data count section.
    /// @details Code-section opcode/index validation belongs to src/uwvm2/validation/standard/wasm1p1.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void define_final_check([[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<wasm1p1_final_check> final_adl,
                                             ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
                                             ::std::byte const* const module_end,
                                             ::uwvm2::parser::wasm::base::error_impl& err,
                                             ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // Reuse the wasm1 final check first; it verifies func/code cardinality and is independent of the extra 1.1 section.
        ::uwvm2::parser::wasm::standard::wasm1::features::define_final_check(
            ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::wasm1_final_check>{},
            module_storage,
            module_end,
            err,
            fs_para);

        auto const& datacountsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_t<Fs...>>(module_storage.sections)};
        auto const& datasec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::data_section_storage_t<Fs...>>(module_storage.sections)};
        if(datacountsec.present)
        {
            auto const actual_data_count{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(datasec.datas.size())};
            if(actual_data_count != datacountsec.count) [[unlikely]]
            {
                err.err_curr = module_end;
                err.err_selectable.wasm1p1_data_count_mismatch.actual = actual_data_count;
                err.err_selectable.wasm1p1_data_count_mismatch.expected = datacountsec.count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1p1_data_count_section_resolved_not_match_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
