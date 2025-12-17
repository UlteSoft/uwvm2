/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      GPT
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

#include <type_traits>
#include <utility>

#include <uwvm2/uwvm/wasm/type/local_imported.h>

namespace
{
    using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using feature_list0 = ::uwvm2::uwvm::wasm::type::feature_list<wasm1>;

    using value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;
    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
    using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;

    using result_tuple_t = ::uwvm2::uwvm::wasm::type::
        import_function_result_tuple_t<feature_list0, value_type::i32, value_type::i64, value_type::i64, value_type::f32, value_type::i32>;

    using expected_tuple_t = ::uwvm2::utils::container::tuple<wasm_i32, wasm_i64, wasm_i64, wasm_f32, wasm_i32>;

    static_assert(::std::same_as<result_tuple_t, expected_tuple_t>);

    static_assert(::std::same_as<::std::remove_cvref_t<decltype(::uwvm2::utils::container::get<0>(::std::declval<result_tuple_t&>()))>, wasm_i32>);
    static_assert(::std::same_as<::std::remove_cvref_t<decltype(::uwvm2::utils::container::get<1>(::std::declval<result_tuple_t&>()))>, wasm_i64>);
    static_assert(::std::same_as<::std::remove_cvref_t<decltype(::uwvm2::utils::container::get<2>(::std::declval<result_tuple_t&>()))>, wasm_i64>);
    static_assert(::std::same_as<::std::remove_cvref_t<decltype(::uwvm2::utils::container::get<3>(::std::declval<result_tuple_t&>()))>, wasm_f32>);
    static_assert(::std::same_as<::std::remove_cvref_t<decltype(::uwvm2::utils::container::get<4>(::std::declval<result_tuple_t&>()))>, wasm_i32>);
}  // namespace

int main() { return 0; }
