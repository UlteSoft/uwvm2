/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-13
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
#include <cstdint>
#include <type_traits>
#include <concepts>
#include <memory>

#include <uwvm2/utils/macro/push_macros.h>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <fast_io_dsal/string_view.h>
# include <fast_io_dsal/tuple.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
#else
# error "Module testing is not currently supported"
#endif

enum class vt1 : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32
{

};

enum class vt2 : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32
{

};

enum class ftp1 : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte
{

};

enum class ftp2 : ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte
{

};

struct F1
{
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"F1"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};

    using value_type = ::uwvm2::parser::wasm::concepts::operation::type_replacer<::uwvm2::parser::wasm::concepts::operation::root_of_replacement, vt1>;
    using type_prefix = ::uwvm2::parser::wasm::concepts::operation::type_replacer<::uwvm2::parser::wasm::concepts::operation::root_of_replacement, ftp1>;
};

struct F2
{
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"F2"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};

    using type_prefix = ::uwvm2::parser::wasm::concepts::operation::type_replacer<ftp1, ftp2>;
    inline static constexpr bool allow_multi_result_vector{false};
};

struct F3
{
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"F3"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};

    using value_type = ::uwvm2::parser::wasm::concepts::operation::type_replacer<vt1, vt2>;
    inline static constexpr bool allow_multi_result_vector{true};
};

// Feature with controllable allow_multi_result_vector parameter
struct F4
{
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"F4"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};

    inline static constexpr bool allow_multi_result_vector{false};

    // Define parameter struct for controllable allow_multi_result_vector
    struct parameter
    {
        bool controllable_allow_multi_result_vector{};
    };
};

// Feature without controllable allow_multi_result_vector parameter
struct F5
{
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"F5"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};

    inline static constexpr bool allow_multi_result_vector{false};
    // No parameter struct defined
};

struct F6
{
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"F6"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};

    inline static constexpr bool allow_multi_result_vector{false};

    // Define parameter struct for controllable allow_multi_result_vector
    struct parameter
    {
        bool controllable_allow_multi_result_vector{true};
    };
};

// Feature with prohibit_duplicate_types set to true
struct F7
{
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"F7"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};

    inline static constexpr bool allow_multi_result_vector{false};
    
    // Enable duplicate type checking
    inline static constexpr bool prohibit_duplicate_types{true};
};

// Feature with prohibit_duplicate_types set to false
struct F8
{
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"F8"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};

    inline static constexpr bool allow_multi_result_vector{false};
    
    // Disable duplicate type checking
    inline static constexpr bool prohibit_duplicate_types{false};
};

// Feature without prohibit_duplicate_types (should default to false)
struct F9
{
    inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"F9"};
    inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};

    inline static constexpr bool allow_multi_result_vector{false};
    
    // No prohibit_duplicate_types defined
};

int main()
{
    static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<F1>, vt1>);
    static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<F1, F2>, vt1>);
    static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<F1, F3>, vt2>);
    static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::features::final_value_type_t<F1, F2, F3>, vt2>);

    static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::features::final_type_prefix_t<F1>, ftp1>);
    static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::features::final_type_prefix_t<F1, F2>, ftp2>);
    static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::features::final_type_prefix_t<F1, F3>, ftp1>);
    static_assert(::std::same_as<::uwvm2::parser::wasm::standard::wasm1::features::final_type_prefix_t<F1, F2, F3>, ftp2>);

    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::allow_multi_result_vector<F1>() == false);
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::allow_multi_result_vector<F1, F2>() == false);
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::allow_multi_result_vector<F1, F3>() == true);
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::allow_multi_result_vector<F1, F2, F3>() == true);

    // Test has_controllable_allow_multi_result_vector concept
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_feature_parameter_controllable_allow_multi_result_vector_from_paras_c<F4>);
    static_assert(!::uwvm2::parser::wasm::standard::wasm1::features::has_feature_parameter_controllable_allow_multi_result_vector_from_paras_c<F5>);
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_feature_parameter_controllable_allow_multi_result_vector_from_paras_c<F6>);
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_feature_parameter_controllable_allow_multi_result_vector_from_paras_c<F1, F4>);
    static_assert(!::uwvm2::parser::wasm::standard::wasm1::features::has_feature_parameter_controllable_allow_multi_result_vector_from_paras_c<F1, F5>);
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_feature_parameter_controllable_allow_multi_result_vector_from_paras_c<F1, F6>);

    // note: F4 and F6 cannot be combined because the parameters are duplicated, which will cause a compilation error.
    // static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_feature_parameter_controllable_allow_multi_result_vector_from_paras_c<F4, F6>);

    // Test runtime get_feature_parameter_controllable_allow_multi_result_vector

    ::uwvm2::parser::wasm::concepts::feature_parameter_t<F4> para_F4{};
    auto const test_can_allow_multi_result_vector_F4{
        ::uwvm2::parser::wasm::standard::wasm1::features::get_feature_parameter_controllable_allow_multi_result_vector_from_paras(para_F4)};
    if(test_can_allow_multi_result_vector_F4 != false)
    {
        ::fast_io::print("test_can_allow_multi_result_vector_F4 is not false\n");
        ::fast_io::fast_terminate();
    }

    ::uwvm2::parser::wasm::concepts::feature_parameter_t<F6> para_F6{};
    auto const test_can_allow_multi_result_vector_F6{
        ::uwvm2::parser::wasm::standard::wasm1::features::get_feature_parameter_controllable_allow_multi_result_vector_from_paras(para_F6)};
    if(test_can_allow_multi_result_vector_F6 != true)
    {
        ::fast_io::print("test_can_allow_multi_result_vector_F6 is not true\n");
        ::fast_io::fast_terminate();
    }

    ::uwvm2::parser::wasm::concepts::feature_parameter_t<F1, F4> para_F1F4{};
    auto const test_can_allow_multi_result_vector_F1F4{
        ::uwvm2::parser::wasm::standard::wasm1::features::get_feature_parameter_controllable_allow_multi_result_vector_from_paras(para_F1F4)};
    if(test_can_allow_multi_result_vector_F1F4 != false)
    {
        ::fast_io::print("test_can_allow_multi_result_vector_F1F4 is not false\n");
        ::fast_io::fast_terminate();
    }

    ::uwvm2::parser::wasm::concepts::feature_parameter_t<F1, F6> para_F1F6{};
    auto const test_can_allow_multi_result_vector_F1F6{
        ::uwvm2::parser::wasm::standard::wasm1::features::get_feature_parameter_controllable_allow_multi_result_vector_from_paras(para_F1F6)};
    if(test_can_allow_multi_result_vector_F1F6 != true)
    {
        ::fast_io::print("test_can_allow_multi_result_vector_F1F6 is not true\n");
        ::fast_io::fast_terminate();
    }

    // Test prohibit_duplicate_types concept and function
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_prohibit_duplicate_types<F7>);
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_prohibit_duplicate_types<F8>);
    static_assert(!::uwvm2::parser::wasm::standard::wasm1::features::has_prohibit_duplicate_types<F9>);
    static_assert(!::uwvm2::parser::wasm::standard::wasm1::features::has_prohibit_duplicate_types<F1>);

    // Test prohibit_duplicate_types function results
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::prohibit_duplicate_types<F7>());
    static_assert(!::uwvm2::parser::wasm::standard::wasm1::features::prohibit_duplicate_types<F8>());
    static_assert(!::uwvm2::parser::wasm::standard::wasm1::features::prohibit_duplicate_types<F9>());
    static_assert(!::uwvm2::parser::wasm::standard::wasm1::features::prohibit_duplicate_types<F1>());

    // Test combinations
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::prohibit_duplicate_types<F1, F7>());
    static_assert(!::uwvm2::parser::wasm::standard::wasm1::features::prohibit_duplicate_types<F1, F8>());
    static_assert(!::uwvm2::parser::wasm::standard::wasm1::features::prohibit_duplicate_types<F1, F9>());
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::prohibit_duplicate_types<F7, F8>());
    static_assert(::uwvm2::parser::wasm::standard::wasm1::features::prohibit_duplicate_types<F7, F9>());
    static_assert(!::uwvm2::parser::wasm::standard::wasm1::features::prohibit_duplicate_types<F8, F9>());

    ::fast_io::print("All prohibit_duplicate_types tests passed!\n");
}

// macro
#include <uwvm2/utils/macro/pop_macros.h>
