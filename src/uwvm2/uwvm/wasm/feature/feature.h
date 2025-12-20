/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-09
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

#pragma once

#ifndef UWVM_MODULE
// std
# include <type_traits>
# include <concepts>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::feature
{
    //////////////////////////
    /// @brief All feature ///
    //////////////////////////
    inline constexpr ::uwvm2::utils::container::tuple all_features{
        ::uwvm2::parser::wasm::standard::wasm1::features::wasm1{}
        // Add here
    };
    /// @brief All feature type (::uwvm2::utils::container::tuple)
    using all_feature_t = decltype(all_features);
    static_assert(::fast_io::is_tuple<all_feature_t>);  // check is tuple
    static_assert(::std::is_empty_v<all_feature_t>);    // check is empty

    inline constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 max_binfmt_version{
        ::uwvm2::parser::wasm::concepts::operation::get_max_binfmt_version_from_tuple(all_features)};

    /// @todo support component module (version = 0x0001000d)
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

}  // namespace uwvm2::uwvm::wasm::feature
