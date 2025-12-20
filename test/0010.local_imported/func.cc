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

#include <cstddef>
#include <cstring>

#include <uwvm2/uwvm/wasm/type/local_imported.h>

namespace
{
    using wasm1 = ::uwvm2::parser::wasm::standard::wasm1::features::wasm1;
    using feature_list0 = ::uwvm2::uwvm::wasm::type::feature_list<wasm1>;
    using value_type = ::uwvm2::parser::wasm::standard::wasm1::type::value_type;

    struct add_i32
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view function_name{u8"add_i32"};

        using res_type = ::uwvm2::uwvm::wasm::type::import_function_result_tuple_t<feature_list0, value_type::i32>;
        using para_type = ::uwvm2::uwvm::wasm::type::import_function_parameter_tuple_t<feature_list0, value_type::i32, value_type::i32>;
        using local_imported_function_type = ::uwvm2::uwvm::wasm::type::local_imported_function_type_t<res_type, para_type>;

        static void call(local_imported_function_type& func_type)
        {
            auto const lhs{::uwvm2::utils::container::get<0>(func_type.params)};
            auto const rhs{::uwvm2::utils::container::get<1>(func_type.params)};
            ::uwvm2::utils::container::get<0>(func_type.res) = static_cast<::std::remove_cvref_t<decltype(lhs)>>(lhs + rhs);
        }
    };

    struct log_i64
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view function_name{u8"log_i64"};

        using res_type = ::uwvm2::uwvm::wasm::type::import_function_result_tuple_t<feature_list0>;
        using para_type = ::uwvm2::uwvm::wasm::type::import_function_parameter_tuple_t<feature_list0, value_type::i64>;
        using local_imported_function_type = ::uwvm2::uwvm::wasm::type::local_imported_function_type_t<res_type, para_type>;

        inline static ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 last{};
        inline static ::std::size_t count{};

        static void call(local_imported_function_type& func_type)
        {
            last = ::uwvm2::utils::container::get<0>(func_type.params);
            ++count;
        }
    };

    struct f32_to_i32
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view function_name{u8"f32_to_i32"};

        using res_type = ::uwvm2::uwvm::wasm::type::import_function_result_tuple_t<feature_list0, value_type::i32>;
        using para_type = ::uwvm2::uwvm::wasm::type::import_function_parameter_tuple_t<feature_list0, value_type::f32>;
        using local_imported_function_type = ::uwvm2::uwvm::wasm::type::local_imported_function_type_t<res_type, para_type>;

        static void call(local_imported_function_type& func_type)
        {
            auto const input{::uwvm2::utils::container::get<0>(func_type.params)};
            ::uwvm2::utils::container::get<0>(func_type.res) = static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32>(input);
        }
    };

    static_assert(::uwvm2::uwvm::wasm::type::is_local_imported_function<add_i32>);
    static_assert(::uwvm2::uwvm::wasm::type::is_local_imported_function<log_i64>);
    static_assert(::uwvm2::uwvm::wasm::type::is_local_imported_function<f32_to_i32>);

    struct demo_local_import
    {
        ::uwvm2::utils::container::u8string_view module_name{u8"demo"};

        using local_function_tuple = ::uwvm2::utils::container::tuple<add_i32, log_i64, f32_to_i32>;
    };

    static_assert(::uwvm2::uwvm::wasm::type::is_local_imported_module<demo_local_import>);
    static_assert(::uwvm2::uwvm::wasm::type::has_local_function_tuple<demo_local_import>);
    static_assert(::uwvm2::uwvm::wasm::type::is_local_imported_function_tuple<typename demo_local_import::local_function_tuple>);

    struct bad_function_no_name
    {
        using res_type = ::uwvm2::uwvm::wasm::type::import_function_result_tuple_t<feature_list0>;
        using para_type = ::uwvm2::uwvm::wasm::type::import_function_parameter_tuple_t<feature_list0>;
        using local_imported_function_type = ::uwvm2::uwvm::wasm::type::local_imported_function_type_t<res_type, para_type>;

        static void call(local_imported_function_type&) {}
    };

    static_assert(!::uwvm2::uwvm::wasm::type::is_local_imported_function<bad_function_no_name>);

    struct bad_module_with_bad_tuple
    {
        ::uwvm2::utils::container::u8string_view module_name{u8"bad"};

        using local_function_tuple = ::uwvm2::utils::container::tuple<bad_function_no_name>;
    };

    static_assert(::uwvm2::uwvm::wasm::type::is_local_imported_module<bad_module_with_bad_tuple>);
    static_assert(!::uwvm2::uwvm::wasm::type::has_local_function_tuple<bad_module_with_bad_tuple>);

    inline int run_call_func_index_tests() noexcept
    {
        using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
        using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
        using wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;

        ::uwvm2::uwvm::wasm::type::local_imported_module<wasm1> mod{demo_local_import{}};

        // add_i32: (i32,i32)->i32
        {
            ::std::byte params[sizeof(wasm_i32) * 2]{};
            wasm_i32 const a{40};
            wasm_i32 const b{2};
            ::std::memcpy(params, ::std::addressof(a), sizeof(wasm_i32));
            ::std::memcpy(params + sizeof(wasm_i32), ::std::addressof(b), sizeof(wasm_i32));

            ::std::byte res[sizeof(wasm_i32)]{};
            mod.call_func_index(0uz, res, params);

            wasm_i32 out{};
            ::std::memcpy(::std::addressof(out), res, sizeof(wasm_i32));
            if(out != 42) { return 1; }
        }

        // log_i64: (i64)->()
        {
            log_i64::last = {};
            log_i64::count = 0;

            ::std::byte params[sizeof(wasm_i64)]{};
            wasm_i64 const v{123};
            ::std::memcpy(params, ::std::addressof(v), sizeof(wasm_i64));

            mod.call_func_index(1uz, nullptr, params);

            if(log_i64::count != 1uz) { return 2; }
            if(log_i64::last != v) { return 3; }
        }

        // f32_to_i32: (f32)->i32
        {
            ::std::byte params[sizeof(wasm_f32)]{};
            wasm_f32 const v{12.75f};
            ::std::memcpy(params, ::std::addressof(v), sizeof(wasm_f32));

            ::std::byte res[sizeof(wasm_i32)]{};
            mod.call_func_index(2uz, res, params);

            wasm_i32 out{};
            ::std::memcpy(::std::addressof(out), res, sizeof(wasm_i32));
            if(out != 12) { return 4; }
        }

        return 0;
    }
}  // namespace

int main() { return run_call_func_index_tests(); }
