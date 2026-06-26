/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly parser feature-list smoke test
 * @details     Ensures both wasm1-only and wasm1+wasm1.1 feature lists instantiate and parse.
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-06-26
 * @copyright   APL-2.0 License
 */

#include <cstddef>
#include <type_traits>

#ifndef UWVM_MODULE
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1p1/impl.h>
#else
# error "Module testing is not currently supported"
#endif

namespace test
{
    inline constexpr unsigned char empty_wasm[]{
        0x00u,
        0x61u,
        0x73u,
        0x6du,
        0x01u,
        0x00u,
        0x00u,
        0x00u,
    };

    /// @brief Parse the minimal empty module with the requested parser feature list.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline void parse_empty_module(::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para)
    {
        auto const* const begin{reinterpret_cast<::std::byte const*>(empty_wasm)};
        auto const* const end{begin + sizeof(empty_wasm)};
        ::uwvm2::parser::wasm::base::error_impl err{};
        auto module_storage{::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_handle_func<Fs...>(begin, end, err, fs_para)};
        static_cast<void>(module_storage);
    }
}  // namespace test

int main()
{
    using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using wasm1p1 = ::uwvm2::parser::wasm::standard::wasm1p1::features::wasm1p1;

    static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<wasm1>,
                                 ::uwvm2::parser::wasm::standard::wasm1::type::value_type>);
    static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<wasm1, wasm1p1>,
                                 ::uwvm2::parser::wasm::standard::wasm1p1::type::value_type>);

    ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1> wasm1_para{};
    test::parse_empty_module<wasm1>(wasm1_para);

    ::uwvm2::parser::wasm::concepts::feature_parameter_t<wasm1, wasm1p1> wasm1p1_para{};
    auto& p1_para{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1p1>(wasm1p1_para)};
    p1_para.enable_multi_value = true;
    p1_para.enable_reference_types = true;
    p1_para.enable_bulk_memory = true;
    p1_para.enable_sign_extension = true;
    p1_para.enable_nontrapping_float_to_int = true;
    p1_para.enable_simd = true;
    p1_para.controllable_allow_multi_result_vector = false;
    p1_para.controllable_allow_multi_table = false;
    test::parse_empty_module<wasm1, wasm1p1>(wasm1p1_para);
}
