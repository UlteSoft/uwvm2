/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.1 (Draft 2021-11-16)
 * @details     Feature pack definition
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
# include <type_traits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/type/impl.h>
# include "def.h"
# include "feature_def.h"
# include "types.h"
# include "data_count_section.h"
# include "data_section.h"
# include "element_section.h"
# include "sequence.h"
# include "final_check.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1p1::features
{
    /// @brief WebAssembly 1.1 feature pack layered on top of the wasm1 binary format.
    /// @details Replaces selected wasm1 storage/parser hooks and adds section 12 without registering a second binary-format handler.
    /// @warning Extension point: every new wasm1p1 replacement hook must be registered here or downstream final_* aliases will silently use wasm1 storage.
    struct wasm1p1
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"WebAssembly Release 1.1 (Draft 2021-11-16)"};
        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};

        using parameter = wasm_binfmt1p1_feature_parameter;

        inline static constexpr bool allow_multi_result_vector{true};
        inline static constexpr bool allow_multi_table{true};

        using value_type = ::uwvm2::parser::wasm::concepts::operation::type_replacer<
            ::uwvm2::parser::wasm::standard::wasm1::type::value_type,
            ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type>;

        using table_type = ::uwvm2::parser::wasm::concepts::operation::type_replacer<
            ::uwvm2::parser::wasm::standard::wasm1::type::table_type,
            ::uwvm2::parser::wasm::standard::wasm1p1::features::table_type>;

        using global_type = ::uwvm2::parser::wasm::concepts::operation::type_replacer<
            ::uwvm2::parser::wasm::standard::wasm1::type::global_type,
            ::uwvm2::parser::wasm::standard::wasm1p1::features::global_type>;

        using wasm_const_expr = ::uwvm2::parser::wasm::concepts::operation::type_replacer<
            ::uwvm2::parser::wasm::standard::wasm1::const_expr::wasm1_const_expr_storage_t,
            ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_const_expr_storage_t>;

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        using data_type = ::uwvm2::parser::wasm::concepts::operation::type_replacer<
            ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_data_t<Fs...>,
            ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_data_t<Fs...>>;

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        using element_type = ::uwvm2::parser::wasm::concepts::operation::type_replacer<
            ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_element_t<Fs...>,
            ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_element_t<Fs...>>;

        using final_check = ::uwvm2::parser::wasm::concepts::operation::type_replacer<
            ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_final_check,
            ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_final_check>;

        using section_sequential_packer = ::uwvm2::parser::wasm::concepts::operation::type_replacer<
            ::uwvm2::parser::wasm::standard::wasm1::features::wasm1_sequence_storage_t,
            ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_sequence_storage_t>;

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        using binfmt_ver1_section_type =
            ::uwvm2::utils::container::tuple<::uwvm2::parser::wasm::standard::wasm1p1::features::data_count_section_storage_t<Fs...>>;
    };

    static_assert(::uwvm2::parser::wasm::concepts::wasm_feature<wasm1p1>);
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_value_type<wasm1p1>);
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_table_type<wasm1p1>);
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_global_type<wasm1p1>);
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_element_type<wasm1p1>);
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_data_type<wasm1p1>);
    static_assert(::uwvm2::parser::wasm::binfmt::ver1::has_final_check<wasm1p1>);
    static_assert(::uwvm2::parser::wasm::binfmt::ver1::has_binfmt_ver1_extensible_section_define<wasm1p1>);
    static_assert(::uwvm2::parser::wasm::binfmt::ver1::has_section_sequential_packer<wasm1p1>);
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
