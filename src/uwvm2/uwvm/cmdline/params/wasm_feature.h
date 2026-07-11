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
        inline bool wasm_feature_mvp_is_exist{};
        inline bool wasm_feature_wasm1p1_is_exist{};
        inline bool wasm_feature_disable_multi_value_is_exist{};
        inline bool wasm_feature_disable_reference_types_is_exist{};
        inline bool wasm_feature_disable_bulk_memory_is_exist{};
        inline bool wasm_feature_disable_sign_extension_is_exist{};
        inline bool wasm_feature_disable_nontrapping_float_to_int_is_exist{};
        inline bool wasm_feature_disable_simd_is_exist{};

        inline constexpr ::uwvm2::utils::container::array<::uwvm2::utils::container::u8string_view, 4uz> wasm_feature_mvp_alias{
            u8"-WFmvp",
            u8"-WFwasmmvp",
            u8"-WFwasm1",
            u8"-WF1"};
        inline constexpr ::uwvm2::utils::container::array<::uwvm2::utils::container::u8string_view, 4uz> wasm_feature_wasm1p1_alias{
            u8"-WF1.1",
            u8"-WF1p1",
            u8"-WFwasm1.1",
            u8"-WFwasm1p1"};
        inline constexpr ::uwvm2::utils::container::u8string_view wasm_feature_disable_multi_value_alias{u8"-WFD-multi-value"};
        inline constexpr ::uwvm2::utils::container::u8string_view wasm_feature_disable_reference_types_alias{u8"-WFD-reference-types"};
        inline constexpr ::uwvm2::utils::container::u8string_view wasm_feature_disable_bulk_memory_alias{u8"-WFD-bulk-memory"};
        inline constexpr ::uwvm2::utils::container::u8string_view wasm_feature_disable_sign_extension_alias{u8"-WFD-sign-extension"};
        inline constexpr ::uwvm2::utils::container::u8string_view wasm_feature_disable_nontrapping_float_to_int_alias{u8"-WFD-nontrapping-float-to-int"};
        inline constexpr ::uwvm2::utils::container::u8string_view wasm_feature_disable_simd_alias{u8"-WFD-simd"};

#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_mvp_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                     ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                     ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_wasm1p1_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                        ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                        ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type
            wasm_feature_disable_multi_value_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                      ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                      ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type
            wasm_feature_disable_reference_types_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                          ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                          ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type
            wasm_feature_disable_bulk_memory_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                      ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                      ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type
            wasm_feature_disable_sign_extension_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                         ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                         ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type
            wasm_feature_disable_nontrapping_float_to_int_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                   ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                   ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_disable_simd_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                              ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                              ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;
    }  // namespace details

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wbraced-scalar-init"
#endif
    /// @brief Command-line switch that disables the WebAssembly 1.1 feature collection and enforces MVP gates.
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_mvp{.name{u8"--wasm-feature-mvp"},
                                                                         .describe{u8"Disable WebAssembly 1.1 features and enforce the WebAssembly MVP feature set."},
                                                                         .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{
                                                                             details::wasm_feature_mvp_alias.data(), details::wasm_feature_mvp_alias.size()}},
                                                                         .handle{::std::addressof(details::wasm_feature_mvp_callback)},
                                                                         .is_exist{::std::addressof(details::wasm_feature_mvp_is_exist)},
                                                                         .cate{::uwvm2::utils::cmdline::categorization::wasm}};

    /// @brief Command-line switch that enables the WebAssembly 1.1 feature collection.
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_wasm1p1{
        .name{u8"--wasm-feature-wasm1.1"},
        .describe{u8"Enable the WebAssembly 1.1 feature set."},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{details::wasm_feature_wasm1p1_alias.data(), details::wasm_feature_wasm1p1_alias.size()}},
        .handle{::std::addressof(details::wasm_feature_wasm1p1_callback)},
        .is_exist{::std::addressof(details::wasm_feature_wasm1p1_is_exist)},
        .cate{::uwvm2::utils::cmdline::categorization::wasm}};

    /// @brief Command-line switch that disables only the multi-value feature.
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_disable_multi_value{
        .name{u8"--wasm-feature-disable-multi-value"},
        .describe{u8"Disable the WebAssembly multi-value feature."},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::wasm_feature_disable_multi_value_alias), 1uz}},
        .handle{::std::addressof(details::wasm_feature_disable_multi_value_callback)},
        .is_exist{::std::addressof(details::wasm_feature_disable_multi_value_is_exist)},
        .cate{::uwvm2::utils::cmdline::categorization::wasm}};

    /// @brief Command-line switch that disables only the reference-types feature.
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_disable_reference_types{
        .name{u8"--wasm-feature-disable-reference-types"},
        .describe{u8"Disable the WebAssembly reference-types feature."},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::wasm_feature_disable_reference_types_alias), 1uz}},
        .handle{::std::addressof(details::wasm_feature_disable_reference_types_callback)},
        .is_exist{::std::addressof(details::wasm_feature_disable_reference_types_is_exist)},
        .cate{::uwvm2::utils::cmdline::categorization::wasm}};

    /// @brief Command-line switch that disables only the bulk-memory feature.
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_disable_bulk_memory{
        .name{u8"--wasm-feature-disable-bulk-memory"},
        .describe{u8"Disable the WebAssembly bulk-memory feature."},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::wasm_feature_disable_bulk_memory_alias), 1uz}},
        .handle{::std::addressof(details::wasm_feature_disable_bulk_memory_callback)},
        .is_exist{::std::addressof(details::wasm_feature_disable_bulk_memory_is_exist)},
        .cate{::uwvm2::utils::cmdline::categorization::wasm}};

    /// @brief Command-line switch that disables only the sign-extension-operators feature.
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_disable_sign_extension{
        .name{u8"--wasm-feature-disable-sign-extension"},
        .describe{u8"Disable the WebAssembly sign-extension-operators feature."},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::wasm_feature_disable_sign_extension_alias), 1uz}},
        .handle{::std::addressof(details::wasm_feature_disable_sign_extension_callback)},
        .is_exist{::std::addressof(details::wasm_feature_disable_sign_extension_is_exist)},
        .cate{::uwvm2::utils::cmdline::categorization::wasm}};

    /// @brief Command-line switch that disables only the non-trapping float-to-int conversion feature.
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_disable_nontrapping_float_to_int{
        .name{u8"--wasm-feature-disable-nontrapping-float-to-int"},
        .describe{u8"Disable the WebAssembly non-trapping float-to-int conversion feature."},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::wasm_feature_disable_nontrapping_float_to_int_alias), 1uz}},
        .handle{::std::addressof(details::wasm_feature_disable_nontrapping_float_to_int_callback)},
        .is_exist{::std::addressof(details::wasm_feature_disable_nontrapping_float_to_int_is_exist)},
        .cate{::uwvm2::utils::cmdline::categorization::wasm}};

    /// @brief Command-line switch that disables only the SIMD feature.
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_disable_simd{.name{u8"--wasm-feature-disable-simd"},
                                                                                  .describe{u8"Disable the WebAssembly SIMD feature."},
                                                                                  .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{
                                                                                      ::std::addressof(details::wasm_feature_disable_simd_alias), 1uz}},
                                                                                  .handle{::std::addressof(details::wasm_feature_disable_simd_callback)},
                                                                                  .is_exist{::std::addressof(details::wasm_feature_disable_simd_is_exist)},
                                                                                  .cate{::uwvm2::utils::cmdline::categorization::wasm}};
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
