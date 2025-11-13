/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-06
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
# include <climits>
# include <type_traits>
# include <concepts>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/utf/impl.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/text_format/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::concepts
{
#if CHAR_BIT > 8
# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wcpp"
# elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wcpp"
# endif

# warning                                                                                                                                                      \
     "The uwvm2 parser design accommodates platforms with non-8-bit CHAR_BIT values, such as certain DSP architectures. "                                      \
     "However, during parsing, it is essential to ensure that all non-least-8 bits in the entire WASM file are set to zero. "                                  \
     "Failure to do so may cause the parser to malfunction."

// Pop warning pragmas
# if defined(__clang__)
#  pragma clang diagnostic pop
# elif defined(__GNUC__)
#  pragma GCC diagnostic pop
# endif
#endif

    struct empty_t
    {
    };

    /// @brief Prevent inheritance effects when adl matching
    template <typename FeatureType>
    struct feature_reserve_type_t
    {
        using feature_type = FeatureType;

        static_assert(::std::is_same_v<::std::remove_cvref_t<FeatureType>, FeatureType>,
                      "feature_reserve_type_t: typename 'FeatureType' cannot have refer and const attributes");
        explicit constexpr feature_reserve_type_t() noexcept = default;
    };

    template <typename FeatureType>
    inline constexpr feature_reserve_type_t<FeatureType> feature_reserve_type{};

    /// @brief For adl function "define_wasm_binfmt_parsering_strategy()" matching
    template <::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 Version>
    struct binfmt_version_t
    {
        explicit constexpr binfmt_version_t() noexcept = default;
    };

    template <::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 Version>
    inline constexpr binfmt_version_t<Version> binfmt_version{};

    /// @brief Used to wrap the code into a type
    template <::uwvm2::parser::wasm::text_format::text_format TFWapper>
    struct text_format_wapper
    {
        inline static constexpr ::uwvm2::parser::wasm::text_format::text_format type{TFWapper};
    };

    /// @brief      Determine if there is a feature name
    /// @see        test\0001.parser\0001.concept\feature_name.cc
    /// @details    Whether the type is provided with a feature name or not, this is a mandatory option.
    ///
    ///             example:
    ///
    ///             Conformity with the concept:
    ///             ```cpp
    ///             struct feature
    ///             {
    ///                 inline static constexpr ::uwvm2::utils::container::u8string_view feature_name{u8"<name>"};
    ///             };
    ///             ```
    ///
    ///             Satisfy the concept, but subsequent operations will be wrong:
    ///             1. Must be a variable that statically stores duration
    ///             ```cpp
    ///             struct feature
    ///             {
    ///                 ::uwvm2::utils::container::u8string_view feature_name{u8"<name>"};
    ///             };
    ///             ```
    template <typename FeatureType>
    concept has_feature_name = requires {
        requires ::std::same_as<::std::remove_cvref_t<decltype(::std::remove_cvref_t<FeatureType>::feature_name)>, ::uwvm2::utils::container::u8string_view>;
    };

    /// @brief      declaration feature parameter
    template <typename... Fs>
    struct feature_parameter_t;

    /// @brief      binfmt handle version func
    template <typename module_storage_t, has_feature_name... Fs>
    using binfmt_handle_version_func_p_type =
        module_storage_t (*)(::std::byte const*, ::std::byte const*, ::uwvm2::parser::wasm::base::error_impl&, feature_parameter_t<Fs...> const&) UWVM_THROWS;

    /// @brief      Define the version number of the required wasm file binary format tagging
    /// @see        test\0001.parser\0001.concept\binfmt.cc
    /// @details    Changing the binfmt version requires a new binfmt version parsing policy, as the entire parsing behavior tree will be replaced with the
    ///             binfmt version. Version must be greater than 0.
    ///
    ///             Whether the type is provided with a binfmt version or not, this is a mandatory option.
    ///
    ///             example:
    ///
    ///             Conformity with the concept:
    ///             ```cpp
    ///             struct feature
    ///             {
    ///                 inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};
    ///             };
    ///             ```
    ///
    ///             Satisfy the concept, but subsequent operations will be wrong:
    ///             1. Must be a variable that statically stores duration
    ///             ```cpp
    ///             struct feature
    ///             {
    ///                 ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};
    ///             };
    ///             ```
    template <typename FeatureType>
    concept has_wasm_binfmt_version = requires {
        requires ::std::same_as<::std::remove_cvref_t<decltype(::std::remove_cvref_t<FeatureType>::binfmt_version)>,
                                ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>;
    };

    /// @brief      Checks if a "define_wasm_binfmt_parsering_strategy" function is defined
    /// @details    Checks if a version handler function is defined. There can be only one version handler for the same binfmt version
    ///             Cannot define a parsing policy that differs from your wasm version.
    ///
    ///             example:
    ///
    ///             Conformity with the concept:
    ///             ```cpp
    ///             struct feature
    ///             {
    ///                 inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 binfmt_version{1u};
    ///             };
    ///
    ///             template <wasm_feature ... Fs>
    ///             inline constexpr binfmt_handle_version_func_p_type<Fs...> define_wasm_binfmt_parsering_strategy(feature_reserve_type_t<feature>,
    ///             ::uwvm2::utils::container::tuple<Fs ...>) {}
    ///
    ///             static_assert(has_wasm_binfmt_parsering_strategy<feature>); // OK. Provide a parsing strategy where binfmt version is 1
    ///             ```
    ///
    ///             Doesn't fit the concept:
    ///             1. Version information must be provided
    ///             ```cpp
    ///             struct feature
    ///             {
    ///             };
    ///
    ///             template <wasm_feature ... Fs>
    ///             inline constexpr binfmt_handle_version_func_p_type<Fs...> define_wasm_binfmt_parsering_strategy(feature_reserve_type_t<feature>,
    ///             ::uwvm2::utils::container::tuple<Fs ...>) {}
    ///
    ///             static_assert(has_wasm_binfmt_parsering_strategy<feature>); // ERROR.
    ///             ```
    template <typename FeatureType>
    concept has_wasm_binfmt_parsering_strategy = requires {
        {
            define_wasm_binfmt_parsering_strategy(feature_reserve_type<::std::remove_cvref_t<FeatureType>>, {/* ::uwvm2::utils::container::tuple<Fs ...> */})
        } /* -> binfmt_handle_version_func_p_type<Fs...> */;
    };

    template <typename FeatureType>
    concept wasm_feature = has_feature_name<FeatureType> && has_wasm_binfmt_version<FeatureType> && ::std::is_empty_v<FeatureType>;
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
