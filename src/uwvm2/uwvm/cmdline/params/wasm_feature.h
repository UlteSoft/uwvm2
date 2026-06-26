/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#ifndef UWVM_MODULE
# ifndef UWVM
#  define UWVM
# endif
// std
# include <memory>
# include <type_traits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/cmdline/handle.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params
{
    namespace details
    {
        inline bool wasm_feature_1p1_is_exist{};
        inline bool wasm_feature_enable_multi_value_is_exist{};
        inline bool wasm_feature_enable_reference_types_is_exist{};
        inline bool wasm_feature_enable_bulk_memory_is_exist{};
        inline bool wasm_feature_enable_sign_extension_is_exist{};
        inline bool wasm_feature_enable_nontrapping_float_to_int_is_exist{};
        inline bool wasm_feature_enable_simd_is_exist{};

#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_1p1_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                     ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                     ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type
            wasm_feature_enable_multi_value_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                     ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                     ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type
            wasm_feature_enable_reference_types_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                         ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                         ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type
            wasm_feature_enable_bulk_memory_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                     ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                     ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type
            wasm_feature_enable_sign_extension_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                        ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                        ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type
            wasm_feature_enable_nontrapping_float_to_int_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                  ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                  ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_enable_simd_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                             ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                             ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;
    }  // namespace details

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wbraced-scalar-init"
#endif
    /// @brief Command-line switch that enables the full WebAssembly 1.1 feature collection.
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_1p1{.name{u8"--wasm-feature-1p1"},
                                                                         .describe{u8"Enable the WebAssembly 1.1 feature set."},
                                                                         .handle{::std::addressof(details::wasm_feature_1p1_callback)},
                                                                         .is_exist{::std::addressof(details::wasm_feature_1p1_is_exist)},
                                                                         .cate{::uwvm2::utils::cmdline::categorization::wasm}};

    /// @brief Command-line switch that enables only the multi-value feature.
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_enable_multi_value{
        .name{u8"--wasm-feature-enable-multi-value"},
        .describe{u8"Enable the WebAssembly multi-value feature."},
        .handle{::std::addressof(details::wasm_feature_enable_multi_value_callback)},
        .is_exist{::std::addressof(details::wasm_feature_enable_multi_value_is_exist)},
        .cate{::uwvm2::utils::cmdline::categorization::wasm}};

    /// @brief Command-line switch that enables only the reference-types feature.
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_enable_reference_types{
        .name{u8"--wasm-feature-enable-reference-types"},
        .describe{u8"Enable the WebAssembly reference-types feature."},
        .handle{::std::addressof(details::wasm_feature_enable_reference_types_callback)},
        .is_exist{::std::addressof(details::wasm_feature_enable_reference_types_is_exist)},
        .cate{::uwvm2::utils::cmdline::categorization::wasm}};

    /// @brief Command-line switch that enables only the bulk-memory feature.
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_enable_bulk_memory{
        .name{u8"--wasm-feature-enable-bulk-memory"},
        .describe{u8"Enable the WebAssembly bulk-memory feature."},
        .handle{::std::addressof(details::wasm_feature_enable_bulk_memory_callback)},
        .is_exist{::std::addressof(details::wasm_feature_enable_bulk_memory_is_exist)},
        .cate{::uwvm2::utils::cmdline::categorization::wasm}};

    /// @brief Command-line switch that enables only the sign-extension-operators feature.
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_enable_sign_extension{
        .name{u8"--wasm-feature-enable-sign-extension"},
        .describe{u8"Enable the WebAssembly sign-extension-operators feature."},
        .handle{::std::addressof(details::wasm_feature_enable_sign_extension_callback)},
        .is_exist{::std::addressof(details::wasm_feature_enable_sign_extension_is_exist)},
        .cate{::uwvm2::utils::cmdline::categorization::wasm}};

    /// @brief Command-line switch that enables only the non-trapping float-to-int conversion feature.
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_enable_nontrapping_float_to_int{
        .name{u8"--wasm-feature-enable-nontrapping-float-to-int"},
        .describe{u8"Enable the WebAssembly non-trapping float-to-int conversion feature."},
        .handle{::std::addressof(details::wasm_feature_enable_nontrapping_float_to_int_callback)},
        .is_exist{::std::addressof(details::wasm_feature_enable_nontrapping_float_to_int_is_exist)},
        .cate{::uwvm2::utils::cmdline::categorization::wasm}};

    /// @brief Command-line switch that enables only the SIMD feature.
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_enable_simd{.name{u8"--wasm-feature-enable-simd"},
                                                                                 .describe{u8"Enable the WebAssembly SIMD feature."},
                                                                                 .handle{::std::addressof(details::wasm_feature_enable_simd_callback)},
                                                                                 .is_exist{::std::addressof(details::wasm_feature_enable_simd_is_exist)},
                                                                                 .cate{::uwvm2::utils::cmdline::categorization::wasm}};
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
