/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
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

// std
#include <type_traits>
#include <concepts>

#include <uwvm2/utils/macro/push_macros.h>

#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/binfmt/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
#else
# error "Module testing is not currently supported"
#endif

// Only support WASM1 !!!

namespace wasm1_test
{
    //////////////////////////
    /// @brief All feature ///
    //////////////////////////
    inline constexpr ::uwvm2::utils::container::tuple all_features{ ::uwvm2::parser::wasm::standard::wasm1::features::wasm1{}};
    /// @brief All feature type (::uwvm2::utils::container::tuple)
    using all_feature_t = decltype(all_features);
    static_assert(::fast_io::is_tuple<all_feature_t>);  // check is tuple
    static_assert(::std::is_empty_v<all_feature_t>);    // check is empty

    inline constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 max_binfmt_version{
        ::uwvm2::parser::wasm::concepts::operation::get_max_binfmt_version_from_tuple(all_features)};

    static_assert(max_binfmt_version == 1u, "missing implementation of other binfmt version");

    ////////////////////////////////////////
    /// @brief wasm binfmt ver1 features ///
    ////////////////////////////////////////
    using wasm_binfmt_ver1_features_t =
        decltype(::uwvm2::parser::wasm::concepts::operation::get_specified_binfmt_feature_tuple_from_all_features_tuple<1u>(all_features));
    static_assert(::fast_io::is_tuple<wasm_binfmt_ver1_features_t>);  // check is tuple
    static_assert(::std::is_empty_v<wasm_binfmt_ver1_features_t>);    // check is empty
    inline constexpr wasm_binfmt_ver1_features_t wasm_binfmt1_features{};
    /// @brief binfmt ver1 module storage
    using wasm_binfmt_ver1_module_storage_t = decltype(::uwvm2::parser::wasm::concepts::operation::get_module_storage_type_from_tuple(wasm_binfmt1_features));
    /// @brief binfmt ver1 module wasm parser (func pointer)
    inline constexpr auto binfmt_ver1_handler{::uwvm2::parser::wasm::concepts::operation::get_binfmt_handler_func_p_from_tuple<1u>(wasm_binfmt1_features)};
    static_assert(::std::is_pointer_v<decltype(binfmt_ver1_handler)>);  // check is func pointer
    /// @brief binfmt ver1 module parameter storage_t (feature_parameter_t)
    using wasm_binfmt_ver1_feature_parameter_storage_t =
        decltype(::uwvm2::parser::wasm::concepts::get_feature_parameter_type_from_tuple(wasm_binfmt1_features));
    /// @brief Unified utf8 version
    using wasm_binfmt_ver1_text_format_wapper_t =
        decltype(::uwvm2::parser::wasm::standard::wasm1::features::get_final_text_format_wapper_from_tuple(wasm_binfmt1_features));
    inline constexpr wasm_binfmt_ver1_text_format_wapper_t wasm_binfmt_ver1_text_format_wapper{};

}  // namespace wasm1_test

// macro

int main(int argc, char** argv)
{
    if(argc < 2) [[unlikely]]
    {
        if(argc == 0) { return 1; }
        ::fast_io::io::perr("Usage: ", ::fast_io::mnp::os_c_str(*argv), " <wasm>\n");
        return 1;
    }

    ::fast_io::native_file_loader wasm_file{::fast_io::mnp::os_c_str(argv[1u])};

    auto const binfmt_ver{::uwvm2::parser::wasm::binfmt::detect_wasm_binfmt_version(reinterpret_cast<::std::byte const*>(wasm_file.cbegin()),
                                                                                    reinterpret_cast<::std::byte const*>(wasm_file.cend()))};

    if(binfmt_ver != 1) [[unlikely]]
    {
        ::fast_io::io::perr("WASM 1.0 only supports Binary Format Version 1\n");
        return 1;
    }

    // handle exec (main) module

    ::wasm1_test::wasm_binfmt_ver1_module_storage_t storage{};

    // storage wasm err
    ::uwvm2::parser::wasm::base::error_impl execute_wasm_binfmt_ver1_storage_wasm_err{};
    ::wasm1_test::wasm_binfmt_ver1_feature_parameter_storage_t parameter{};

#if defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
    try
#endif
    {
        // parser
        storage = ::wasm1_test::binfmt_ver1_handler(reinterpret_cast<::std::byte const*>(wasm_file.cbegin()),
                                                    reinterpret_cast<::std::byte const*>(wasm_file.cend()),
                                                    execute_wasm_binfmt_ver1_storage_wasm_err,
                                                    parameter);
    }
#if defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
    catch(::fast_io::error)
    {
        ::uwvm2::parser::wasm::base::error_output_t errout;
        errout.module_begin = reinterpret_cast<::std::byte const*>(wasm_file.cbegin());
        errout.err = execute_wasm_binfmt_ver1_storage_wasm_err;
        errout.flag.enable_ansi = static_cast<::std::uint_least8_t>(false);

        // Output the main information and memory indication
        ::fast_io::io::perrln(::fast_io::u8err(), u8"Parsing error in WebAssembly File: ", errout);
        return 1;
    }
#endif

    return 0;
}

#include <uwvm2/utils/macro/pop_macros.h>

