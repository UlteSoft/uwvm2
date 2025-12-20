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

        static void call(local_imported_function_type&) {}
    };

    struct log_i64
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view function_name{u8"log_i64"};

        using res_type = ::uwvm2::uwvm::wasm::type::import_function_result_tuple_t<feature_list0>;
        using para_type = ::uwvm2::uwvm::wasm::type::import_function_parameter_tuple_t<feature_list0, value_type::i64>;
        using local_imported_function_type = ::uwvm2::uwvm::wasm::type::local_imported_function_type_t<res_type, para_type>;

        static void call(local_imported_function_type&) {}
    };

    struct f32_to_i32
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view function_name{u8"f32_to_i32"};

        using res_type = ::uwvm2::uwvm::wasm::type::import_function_result_tuple_t<feature_list0, value_type::i32>;
        using para_type = ::uwvm2::uwvm::wasm::type::import_function_parameter_tuple_t<feature_list0, value_type::f32>;
        using local_imported_function_type = ::uwvm2::uwvm::wasm::type::local_imported_function_type_t<res_type, para_type>;

        static void call(local_imported_function_type&) {}
    };

    struct demo_local_import
    {
        ::uwvm2::utils::container::u8string_view module_name{u8"demo"};
        using local_function_tuple = ::uwvm2::utils::container::tuple<add_i32, log_i64, f32_to_i32>;
    };

    inline int run_lookup_tests() noexcept
    {
        ::uwvm2::uwvm::wasm::type::local_imported_module<wasm1> mod{demo_local_import{}};

        {
            auto const r0{mod.get_function_information_from_index(0)};
            if(!r0.successed)
            {
                ::fast_io::io::panic(::uwvm2::uwvm::io::u8log_output, u8"test failed: get_function_information_from_index(0) failed\n");
                return 1;
            }
            if(r0.function_name != u8"add_i32")
            {
                ::fast_io::io::panic(::uwvm2::uwvm::io::u8log_output, u8"test failed: get_function_information_from_index(0) function_name != u8\"add_i32\"\n");
                return 2;
            }
            if(r0.index != 0)
            {
                ::fast_io::io::panic(::uwvm2::uwvm::io::u8log_output, u8"test failed: get_function_information_from_index(0) index != 0\n");
                return 3;
            }
            if(static_cast<::std::size_t>(r0.function_type.parameter.end - r0.function_type.parameter.begin) != 2uz)
            {
                ::fast_io::io::panic(
                    ::uwvm2::uwvm::io::u8log_output,
                    u8"test failed: get_function_information_from_index(0) function_type.parameter.end - function_type.parameter.begin != 2uz\n");
                return 4;
            }
            if(static_cast<::std::size_t>(r0.function_type.result.end - r0.function_type.result.begin) != 1uz)
            {
                ::fast_io::io::panic(::uwvm2::uwvm::io::u8log_output,
                                     u8"test failed: get_function_information_from_index(0) function_type.result.end - function_type.result.begin != 1uz\n");
                return 5;
            }
            if(r0.function_type.parameter.begin[0] != value_type::i32)
            {
                ::fast_io::io::panic(::uwvm2::uwvm::io::u8log_output,
                                     u8"test failed: get_function_information_from_index(0) function_type.parameter.begin[0] != value_type::i32\n");
                return 6;
            }
            if(r0.function_type.parameter.begin[1] != value_type::i32)
            {
                ::fast_io::io::panic(::uwvm2::uwvm::io::u8log_output,
                                     u8"test failed: get_function_information_from_index(0) function_type.parameter.begin[1] != value_type::i32\n");
                return 7;
            }
            if(r0.function_type.result.begin[0] != value_type::i32)
            {
                ::fast_io::io::panic(::uwvm2::uwvm::io::u8log_output,
                                     u8"test failed: get_function_information_from_index(0) function_type.result.begin[0] != value_type::i32\n");
                return 8;
            }
        }

        {
            auto const r2{mod.get_function_information_from_name(u8"f32_to_i32")};
            if(!r2.successed)
            {
                ::fast_io::io::panic(::uwvm2::uwvm::io::u8log_output, u8"test failed: get_function_information_from_name(u8\"f32_to_i32\") failed\n");
                return 9;
            }
            if(r2.index != 2uz)
            {
                ::fast_io::io::panic(::uwvm2::uwvm::io::u8log_output, u8"test failed: get_function_information_from_name(u8\"f32_to_i32\") index != 2uz\n");
                return 10;
            }
            if(static_cast<::std::size_t>(r2.function_type.parameter.end - r2.function_type.parameter.begin) != 1uz)
            {
                ::fast_io::io::panic(
                    ::uwvm2::uwvm::io::u8log_output,
                    u8"test failed: get_function_information_from_name(u8\"f32_to_i32\") function_type.parameter.end - function_type.parameter.begin != 1uz\n");
                return 11;
            }
            if(static_cast<::std::size_t>(r2.function_type.result.end - r2.function_type.result.begin) != 1uz)
            {
                ::fast_io::io::panic(
                    ::uwvm2::uwvm::io::u8log_output,
                    u8"test failed: get_function_information_from_name(u8\"f32_to_i32\") function_type.result.end - function_type.result.begin != 1uz\n");
                return 12;
            }
            if(r2.function_type.parameter.begin[0] != value_type::f32)
            {
                ::fast_io::io::panic(
                    ::uwvm2::uwvm::io::u8log_output,
                    u8"test failed: get_function_information_from_name(u8\"f32_to_i32\") function_type.parameter.begin[0] != value_type::f32\n");
                return 13;
            }
            if(r2.function_type.result.begin[0] != value_type::i32)
            {
                ::fast_io::io::panic(::uwvm2::uwvm::io::u8log_output,
                                     u8"test failed: get_function_information_from_name(u8\"f32_to_i32\") function_type.result.begin[0] != value_type::i32\n");
                return 14;
            }
        }

        {
            auto const rx{mod.get_function_information_from_name(u8"no_such_func")};
            if(rx.successed)
            {
                ::fast_io::io::panic(::uwvm2::uwvm::io::u8log_output, u8"test failed: get_function_information_from_name(u8\"no_such_func\") successed\n");
                return 15;
            }
        }

        {
            auto const all{mod.get_all_function_information()};
            auto const count{static_cast<::std::size_t>(all.end - all.begin)};
            if(count != 3uz)
            {
                ::fast_io::io::panic(::uwvm2::uwvm::io::u8log_output, u8"test failed: get_all_function_information() count != 3uz\n");
                return 16;
            }
            if(all.begin[0].function_name != u8"add_i32")
            {
                ::fast_io::io::panic(::uwvm2::uwvm::io::u8log_output, u8"test failed: get_all_function_information()[0].function_name != u8\"add_i32\"\n");
                return 17;
            }
            if(all.begin[1].function_name != u8"log_i64")
            {
                ::fast_io::io::panic(::uwvm2::uwvm::io::u8log_output, u8"test failed: get_all_function_information()[1].function_name != u8\"log_i64\"\n");
                return 18;
            }
            if(all.begin[2].function_name != u8"f32_to_i32")
            {
                ::fast_io::io::panic(::uwvm2::uwvm::io::u8log_output, u8"test failed: get_all_function_information()[2].function_name != u8\"f32_to_i32\"\n");
                return 19;
            }
        }

        return 0;
    }
}  // namespace

int main() { return run_lookup_tests(); }
