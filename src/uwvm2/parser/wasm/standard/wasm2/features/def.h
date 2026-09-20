/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#ifndef UWVM_MODULE
# include <concepts>
# include <cstdint>
# include <uwvm2/utils/macro/push_macros.h>
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/features/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm2::features
{
    struct wasm2_code_version
    {};

    /// @brief WebAssembly 2.0 policy layered on the binfmt-version-1 wasm1p1 parser.
    struct wasm2
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"WebAssembly Release 2.0 (2024-08-09)"};
        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};

        using code_version = ::uwvm2::parser::wasm::concepts::operation::type_replacer<
            ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1_code_version,
            ::uwvm2::parser::wasm::standard::wasm2::features::wasm2_code_version>;
    };

    using wasm2_feature_parameter = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm_binfmt1p1_feature_parameter;

    enum class wasm2_feature_kind : ::std::uint_least8_t
    {
        sign_extension,
        nontrapping_float_to_int,
        multi_value,
        reference_types,
        table_instructions,
        multiple_tables,
        bulk_memory,
        simd
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr wasm2_feature_parameter const& get_wasm2_parameter(
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para); }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr wasm2_feature_parameter& get_wasm2_parameter(::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> & fs_para) noexcept
    { return ::uwvm2::parser::wasm::standard::wasm1p1::features::get_wasm1p1_parameter(fs_para); }

    [[nodiscard]] inline constexpr bool feature_enabled(wasm2_feature_parameter const& parameter, wasm2_feature_kind const feature) noexcept
    {
        switch(feature)
        {
            case wasm2_feature_kind::sign_extension: return !parameter.disable_sign_extension;
            case wasm2_feature_kind::nontrapping_float_to_int: return !parameter.disable_nontrapping_float_to_int;
            case wasm2_feature_kind::multi_value:
                return !(parameter.disable_multi_value || parameter.controllable_allow_multi_result_vector);
            case wasm2_feature_kind::reference_types: return !parameter.disable_reference_types;
            case wasm2_feature_kind::table_instructions: return !parameter.disable_table_instructions;
            case wasm2_feature_kind::multiple_tables:
                return !(parameter.disable_multiple_tables || parameter.controllable_allow_multi_table);
            case wasm2_feature_kind::bulk_memory: return !parameter.disable_bulk_memory;
            case wasm2_feature_kind::simd: return !parameter.disable_simd;
            [[unlikely]] default: return false;
        }
    }

    static_assert(::uwvm2::parser::wasm::concepts::wasm_feature<wasm2>);
    static_assert(::uwvm2::parser::wasm::binfmt::ver1::has_code_version_reserve_type<wasm2>);
    static_assert(!::uwvm2::parser::wasm::concepts::has_wasm_binfmt_parsering_strategy<wasm2>);
}

#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif
