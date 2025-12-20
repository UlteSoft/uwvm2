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

    using li_res0 = ::uwvm2::utils::container::tuple<wasm_i32>;
    using li_params0 = ::uwvm2::utils::container::tuple<wasm_i64, wasm_f32>;
    using li_sig0 = ::uwvm2::uwvm::wasm::type::local_imported_function_type_t<li_res0, li_params0>;

    static_assert(::uwvm2::uwvm::wasm::type::is_local_imported_function_type_t<li_sig0>);

    struct local_imported_function_good
    {
        using local_imported_function_type = li_sig0;

        static void call(local_imported_function_type&) {}
    };

    static_assert(::uwvm2::uwvm::wasm::type::has_local_imported_function_type<local_imported_function_good>);
    static_assert(::uwvm2::uwvm::wasm::type::has_function_call<local_imported_function_good>);

    struct local_imported_function_missing_type
    {
    };

    static_assert(!::uwvm2::uwvm::wasm::type::has_local_imported_function_type<local_imported_function_missing_type>);
    static_assert(!::uwvm2::uwvm::wasm::type::has_function_call<local_imported_function_missing_type>);

    struct local_imported_function_wrong_type
    {
        using local_imported_function_type = int;
    };

    static_assert(!::uwvm2::uwvm::wasm::type::has_local_imported_function_type<local_imported_function_wrong_type>);
    static_assert(!::uwvm2::uwvm::wasm::type::has_function_call<local_imported_function_wrong_type>);

    struct local_imported_function_fake_sig
    {
        using result_type = li_res0;
        using parameter_type = li_params0;

        result_type res{};
        parameter_type params{};
    };

    static_assert(!::uwvm2::uwvm::wasm::type::is_local_imported_function_type_t<local_imported_function_fake_sig>);

    struct local_imported_function_has_fake_sig
    {
        using local_imported_function_type = local_imported_function_fake_sig;
    };

    static_assert(!::uwvm2::uwvm::wasm::type::has_local_imported_function_type<local_imported_function_has_fake_sig>);
    static_assert(!::uwvm2::uwvm::wasm::type::has_function_call<local_imported_function_has_fake_sig>);

    struct local_imported_function_call_bad_param
    {
        using local_imported_function_type = li_sig0;

        static void call(local_imported_function_type&&) {}
    };

    static_assert(::uwvm2::uwvm::wasm::type::has_local_imported_function_type<local_imported_function_call_bad_param>);
    static_assert(!::uwvm2::uwvm::wasm::type::has_function_call<local_imported_function_call_bad_param>);

    struct local_imported_function_call_bad_ret
    {
        using local_imported_function_type = li_sig0;

        static int call(local_imported_function_type&) { return 0; }
    };

    static_assert(::uwvm2::uwvm::wasm::type::has_local_imported_function_type<local_imported_function_call_bad_ret>);
    static_assert(!::uwvm2::uwvm::wasm::type::has_function_call<local_imported_function_call_bad_ret>);

    struct local_imported_function_good_named
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view function_name{u8"good"};
        using local_imported_function_type = li_sig0;

        static void call(local_imported_function_type&) {}
    };

    static_assert(::uwvm2::uwvm::wasm::type::is_local_imported_function<local_imported_function_good_named>);

    struct local_imported_module_with_good_tuple
    {
        using local_function_tuple = ::uwvm2::utils::container::tuple<local_imported_function_good_named>;
    };

    static_assert(::uwvm2::uwvm::wasm::type::has_local_function_tuple<local_imported_module_with_good_tuple>);

    struct local_imported_module_with_bad_tuple
    {
        using local_function_tuple = ::uwvm2::utils::container::tuple<local_imported_function_missing_type>;
    };

    static_assert(!::uwvm2::uwvm::wasm::type::has_local_function_tuple<local_imported_module_with_bad_tuple>);

    struct local_imported_module_with_non_tuple
    {
        using local_function_tuple = int;
    };

    static_assert(!::uwvm2::uwvm::wasm::type::has_local_function_tuple<local_imported_module_with_non_tuple>);
}  // namespace

int main() { return 0; }
