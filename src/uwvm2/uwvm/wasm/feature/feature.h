/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2 License (see LICENSE file).      *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-09
 * @copyright   APL-2 License
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

#ifdef UWVM_MODULE
import fast_io;
import uwvm2.parser.wasm.concepts;
import uwvm2.parser.wasm.standard;
#else
// std
# include <type_traits>
# include <concepts>
// import
# include <fast_io.h>
# include <fast_io_dsal/tuple.h>
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
    inline constexpr ::fast_io::tuple all_features{
        ::uwvm2::parser::wasm::standard::wasm1::features::wasm1{}
        // Add here
    };
    /// @brief All feature type (::fast_io::tuple)
    using all_feature_t = decltype(all_features);
    static_assert(::fast_io::is_tuple<all_feature_t>);  // check is tuple
    static_assert(::std::is_empty_v<all_feature_t>);    // check is empty

    ////////////////////////////////////////
    /// @brief wasm binfmt ver1 features ///
    ////////////////////////////////////////
    using wasm_binfmt_ver1_features_t =
        decltype(::uwvm2::parser::wasm::concepts::operation::get_specified_binfmt_feature_tuple_from_all_freatures_tuple<1>(all_features));
    static_assert(::fast_io::is_tuple<wasm_binfmt_ver1_features_t>);  // check is tuple
    static_assert(::std::is_empty_v<wasm_binfmt_ver1_features_t>);    // check is empty
    inline constexpr wasm_binfmt_ver1_features_t wasm_binfmt1_features{};
    /// @brief binfmt ver1 module storage
    using wasm_binfmt_ver1_module_storage_t = decltype(::uwvm2::parser::wasm::concepts::operation::get_module_storage_type_from_tuple(wasm_binfmt1_features));
    /// @brief binfmt ver1 module wasm parser (func pointer)
    inline constexpr auto binfmt_ver1_handler{::uwvm2::parser::wasm::concepts::operation::get_binfmt_handler_func_p_from_tuple<1>(wasm_binfmt1_features)};
    static_assert(::std::is_pointer_v<decltype(binfmt_ver1_handler)>);  // check is func pointer
    /// @brief binfmt ver1 module parameter storage_t (feature_parameter_t)
    using wasm_binfmt_ver1_feature_parameter_storage_t =
        decltype(::uwvm2::parser::wasm::concepts::get_feature_parameter_type_from_tuple(wasm_binfmt1_features));
}  // namespace uwvm2::uwvm::wasm::feature
