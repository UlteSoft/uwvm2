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
        inline bool wasm_feature_wasm2_is_exist{};

        inline constexpr ::uwvm2::utils::container::array<::uwvm2::utils::container::u8string_view, 5uz> wasm_feature_mvp_alias{
            u8"--wasm-feature-mvp",
            u8"-WFmvp",
            u8"-WFwasmmvp",
            u8"-WFwasm1",
            u8"-WF1"};
        inline constexpr ::uwvm2::utils::container::array<::uwvm2::utils::container::u8string_view, 5uz> wasm_feature_wasm1p1_alias{
            u8"--wasm-feature-wasm1.1",
            u8"-WF1.1",
            u8"-WF1p1",
            u8"-WFwasm1.1",
            u8"-WFwasm1p1"};
        inline constexpr ::uwvm2::utils::container::array<::uwvm2::utils::container::u8string_view, 2uz> wasm_feature_wasm2_alias{
            u8"-WF2", u8"-WFwasm2"};

#if defined(UWVM_MODULE)
# define UWVM_WASM_FEATURE_CALLBACK_DECL extern "C++"
#else
# define UWVM_WASM_FEATURE_CALLBACK_DECL inline constexpr
#endif

        UWVM_WASM_FEATURE_CALLBACK_DECL ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_mvp_callback(
            ::uwvm2::utils::cmdline::parameter_parsing_results*,
            ::uwvm2::utils::cmdline::parameter_parsing_results*,
            ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;
        UWVM_WASM_FEATURE_CALLBACK_DECL ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_wasm1p1_callback(
            ::uwvm2::utils::cmdline::parameter_parsing_results*,
            ::uwvm2::utils::cmdline::parameter_parsing_results*,
            ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;
        UWVM_WASM_FEATURE_CALLBACK_DECL ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_wasm2_callback(
            ::uwvm2::utils::cmdline::parameter_parsing_results*,
            ::uwvm2::utils::cmdline::parameter_parsing_results*,
            ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

#define UWVM_DECLARE_WASM_FEATURE_PAIR(member_name)                                                                                                    \
    inline bool wasm_feature_enable_##member_name##_is_exist{};                                                                                        \
    inline bool wasm_feature_disable_##member_name##_is_exist{};                                                                                       \
    UWVM_WASM_FEATURE_CALLBACK_DECL ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_enable_##member_name##_callback(                      \
        ::uwvm2::utils::cmdline::parameter_parsing_results*,                                                                                           \
        ::uwvm2::utils::cmdline::parameter_parsing_results*,                                                                                           \
        ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;                                                                                  \
    UWVM_WASM_FEATURE_CALLBACK_DECL ::uwvm2::utils::cmdline::parameter_return_type wasm_feature_disable_##member_name##_callback(                     \
        ::uwvm2::utils::cmdline::parameter_parsing_results*,                                                                                           \
        ::uwvm2::utils::cmdline::parameter_parsing_results*,                                                                                           \
        ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

        UWVM_DECLARE_WASM_FEATURE_PAIR(multi_value)
        UWVM_DECLARE_WASM_FEATURE_PAIR(reference_types)
        UWVM_DECLARE_WASM_FEATURE_PAIR(table_instructions)
        UWVM_DECLARE_WASM_FEATURE_PAIR(multiple_tables)
        UWVM_DECLARE_WASM_FEATURE_PAIR(bulk_memory)
        UWVM_DECLARE_WASM_FEATURE_PAIR(sign_extension)
        UWVM_DECLARE_WASM_FEATURE_PAIR(nontrapping_float_to_int)
        UWVM_DECLARE_WASM_FEATURE_PAIR(simd)

#undef UWVM_DECLARE_WASM_FEATURE_PAIR
#undef UWVM_WASM_FEATURE_CALLBACK_DECL
    }  // namespace details

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wbraced-scalar-init"
#endif
    /// @brief Select the WebAssembly MVP feature set.
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_mvp{
        .name{u8"--wasm-feature-wasmmvp"},
        .describe{u8"Select the complete WebAssembly MVP feature set."},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{details::wasm_feature_mvp_alias.data(), details::wasm_feature_mvp_alias.size()}},
        .handle{::std::addressof(details::wasm_feature_mvp_callback)},
        .is_exist{::std::addressof(details::wasm_feature_mvp_is_exist)},
        .cate{::uwvm2::utils::cmdline::categorization::wasm}};

    /// @brief Select the default WebAssembly 1.1 feature set.
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_wasm1p1{
        .name{u8"--wasm-feature-wasm1p1"},
        .describe{u8"Select the complete WebAssembly 1.1 feature set."},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{details::wasm_feature_wasm1p1_alias.data(), details::wasm_feature_wasm1p1_alias.size()}},
        .handle{::std::addressof(details::wasm_feature_wasm1p1_callback)},
        .is_exist{::std::addressof(details::wasm_feature_wasm1p1_is_exist)},
        .cate{::uwvm2::utils::cmdline::categorization::wasm}};

    /// @brief Select the WebAssembly 2.0 feature set.
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_wasm2{
        .name{u8"--wasm-feature-wasm2"},
        .describe{u8"Select the complete WebAssembly 2.0 feature set."},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{details::wasm_feature_wasm2_alias.data(), details::wasm_feature_wasm2_alias.size()}},
        .handle{::std::addressof(details::wasm_feature_wasm2_callback)},
        .is_exist{::std::addressof(details::wasm_feature_wasm2_is_exist)},
        .cate{::uwvm2::utils::cmdline::categorization::wasm}};

#define UWVM_DEFINE_WASM_FEATURE_SWITCH(direction, member_name, direction_text, alias_letter, cli_suffix, description_verb)                          \
    namespace details                                                                                                                                \
    {                                                                                                                                                 \
        inline constexpr ::uwvm2::utils::container::u8string_view wasm_feature_##direction##_##member_name##_alias{                                  \
            u8"-WF" alias_letter u8"-" cli_suffix};                                                                                                 \
    }                                                                                                                                                 \
    inline constexpr ::uwvm2::utils::cmdline::parameter wasm_feature_##direction##_##member_name{                                                    \
        .name{u8"--wasm-feature-" direction_text u8"-" cli_suffix},                                                                                 \
        .describe{description_verb u8" the WebAssembly " cli_suffix u8" feature."},                                                                 \
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{                                                                                         \
            ::std::addressof(details::wasm_feature_##direction##_##member_name##_alias), 1uz}},                                                       \
        .handle{::std::addressof(details::wasm_feature_##direction##_##member_name##_callback)},                                                      \
        .is_exist{::std::addressof(details::wasm_feature_##direction##_##member_name##_is_exist)},                                                    \
        .cate{::uwvm2::utils::cmdline::categorization::wasm}};

#define UWVM_DEFINE_WASM_FEATURE_PAIR(member_name, cli_suffix)                                                                                        \
    UWVM_DEFINE_WASM_FEATURE_SWITCH(enable, member_name, u8"enable", u8"E", cli_suffix, u8"Enable")                                               \
    UWVM_DEFINE_WASM_FEATURE_SWITCH(disable, member_name, u8"disable", u8"D", cli_suffix, u8"Disable")

    UWVM_DEFINE_WASM_FEATURE_PAIR(multi_value, u8"multi-value")
    UWVM_DEFINE_WASM_FEATURE_PAIR(reference_types, u8"reference-types")
    UWVM_DEFINE_WASM_FEATURE_PAIR(table_instructions, u8"table-instructions")
    UWVM_DEFINE_WASM_FEATURE_PAIR(multiple_tables, u8"multiple-tables")
    UWVM_DEFINE_WASM_FEATURE_PAIR(bulk_memory, u8"bulk-memory")
    UWVM_DEFINE_WASM_FEATURE_PAIR(sign_extension, u8"sign-extension")
    UWVM_DEFINE_WASM_FEATURE_PAIR(nontrapping_float_to_int, u8"nontrapping-float-to-int")
    UWVM_DEFINE_WASM_FEATURE_PAIR(simd, u8"simd")

#undef UWVM_DEFINE_WASM_FEATURE_PAIR
#undef UWVM_DEFINE_WASM_FEATURE_SWITCH
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
