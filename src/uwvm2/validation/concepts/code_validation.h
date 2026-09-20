/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.0 (2019-07-20)
 * @details     antecedent dependency: null
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-07-07
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
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <concepts>
# include <type_traits>
# include <utility>
# include <memory>
# include <limits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/intrinsics/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/utils/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
# include <uwvm2/validation/error/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::validation::concepts
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_code_version_reserve_type_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return ::uwvm2::parser::wasm::binfmt::ver1::final_code_version_reserve_type_t<Fs...>{}; }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using code_version_type_t = ::uwvm2::parser::wasm::binfmt::ver1::final_code_version_reserve_type_t<Fs...>;

    template <typename CodeVersionType, typename... Fs>
    concept can_validate_code = requires(CodeVersionType code_adl,
                                         ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
                                         ::std::size_t const function_index,
                                         ::std::byte const* code_begin,
                                         ::std::byte const* code_end,
                                         ::uwvm2::validation::error::code_validation_error_impl& err) {
        { validate_code(code_adl, module_storage, function_index, code_begin, code_end, err) } -> ::std::same_as<void>;
    };

    /// @brief Code-validation strategy accepting the parser feature-policy tuple.
    /// @details The code-version tag is parser-owned and the validation function is selected through ADL, preserving the COP strategy extension point.
    template <typename CodeVersionType, typename... Fs>
    concept can_validate_code_with_parameter = requires(
        CodeVersionType code_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
        ::std::size_t const function_index,
        ::std::byte const* code_begin,
        ::std::byte const* code_end,
        ::uwvm2::validation::error::code_validation_error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) {
        { validate_code(code_adl, module_storage, function_index, code_begin, code_end, err, fs_para) } -> ::std::same_as<void>;
    };

    /// @brief Dispatch code validation through the final parser-composed code-version strategy.
    /// @warning Extension point: a replacement code-version tag must provide an ADL-visible validate_code overload in the tag's associated namespace.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void dispatch_validate_code(
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
        ::std::size_t const function_index,
        ::std::byte const* code_begin,
        ::std::byte const* code_end,
        ::uwvm2::validation::error::code_validation_error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        using code_version_type = code_version_type_t<Fs...>;
        if constexpr(can_validate_code_with_parameter<code_version_type, Fs...>)
        {
            validate_code(code_version_type{}, module_storage, function_index, code_begin, code_end, err, fs_para);
        }
        else
        {
            // Parameter-free legacy strategies remain valid for feature sets whose validator has no runtime policy.
            static_assert(can_validate_code<code_version_type, Fs...>,
                          "The final code-version COP tag has no ADL-visible code-validation strategy");
            validate_code(code_version_type{}, module_storage, function_index, code_begin, code_end, err);
        }
    }

    /// @brief Parameter-free dispatch retained for callers that intentionally use a strategy's default policy.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void dispatch_validate_code(
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> const& module_storage,
        ::std::size_t const function_index,
        ::std::byte const* code_begin,
        ::std::byte const* code_end,
        ::uwvm2::validation::error::code_validation_error_impl& err) UWVM_THROWS
    {
        using code_version_type = code_version_type_t<Fs...>;
        static_assert(can_validate_code<code_version_type, Fs...>,
                      "The final code-version COP tag has no ADL-visible code-validation strategy");
        validate_code(code_version_type{}, module_storage, function_index, code_begin, code_end, err);
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
