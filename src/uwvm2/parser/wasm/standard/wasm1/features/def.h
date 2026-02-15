/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @brief       WebAssembly Release 1.0 (2019-07-20)
 * @details     antecedent dependency: null
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-16
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

#if !(__cpp_pack_indexing >= 202311L)
# error "UWVM requires at least C++26 standard compiler. See https://en.cppreference.com/w/cpp/feature_test#cpp_pack_indexing"
#endif

#ifndef UWVM_MODULE
// std
# include <cstddef>
# include <cstdint>
# include <climits>
# include <concepts>
# include <type_traits>
# include <utility>
# include <memory>
# include <compare>
# include <algorithm>
# include <compare>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/hash/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/section/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/opcode/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include <uwvm2/parser/wasm/text_format/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    ///
    /// @brief Define structures or concepts related to the version of binfmt
    ///

    ///////////////////////////
    //     Binary Format     //
    ///////////////////////////

    /// @brief      has text format
    /// @details
    ///             ```cpp
    ///             struct F
    ///             {
    ///                 using text_format = type_replacer<root_of_replacement, text_format_wapper<text_format::xxx>>;
    ///             };
    ///             ```
    template <typename FeatureType>
    concept has_text_format = requires {
        typename FeatureType::text_format;
        requires ::uwvm2::parser::wasm::concepts::operation::details::check_is_type_replacer<::uwvm2::parser::wasm::concepts::operation::type_replacer,
                                                                                             typename FeatureType::text_format>;
    };

    template <typename FeatureType>
    inline consteval auto get_text_format() noexcept
    {
        if constexpr(has_text_format<FeatureType>) { return typename FeatureType::text_format{}; }
        else
        {
            return ::uwvm2::parser::wasm::concepts::operation::irreplaceable_t{};
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using final_text_format_wapper = ::uwvm2::parser::wasm::concepts::operation::replacement_structure_t<decltype(get_text_format<Fs>())...>;

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_final_text_format_wapper_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return final_text_format_wapper<Fs...>{}; }

    ////////////////////////////
    //      Type Section      //
    ////////////////////////////

    /// @brief      has value type
    /// @details
    ///             ```cpp
    ///             struct F
    ///             {
    ///                 using value_type = type_replacer<root_of_replacement, value_type>;
    ///             };
    ///             ```
    /// @see        test\0001.parser\0002.binfmt1\section\type_section.cc

    /// @brief      has type type
    /// @details
    ///             ```cpp
    ///             struct F
    ///             {
    ///                 using type_type = type_replacer<root_of_replacement, type_type>;
    ///             };
    ///             ```
    /// @see        test\0001.parser\0002.binfmt1\section\type_section.cc
    template <typename FeatureType, typename... Fs>
    concept has_type_type = requires {
        typename FeatureType::template type_type<Fs...>;
        requires ::uwvm2::parser::wasm::concepts::operation::details::check_is_type_replacer<::uwvm2::parser::wasm::concepts::operation::type_replacer,
                                                                                             typename FeatureType::template type_type<Fs...>>;
    };

    template <typename FeatureType, typename... Fs>
    inline consteval auto get_type_type() noexcept
    {
        if constexpr(has_type_type<FeatureType, Fs...>) { return typename FeatureType::template type_type<Fs...>{}; }
        else
        {
            return ::uwvm2::parser::wasm::concepts::operation::irreplaceable_t{};
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using final_type_type_t = ::uwvm2::parser::wasm::concepts::operation::replacement_structure_t<decltype(get_type_type<Fs, Fs...>())...>;

    template <typename FeatureType>
    concept has_value_type = requires {
        typename FeatureType::value_type;
        requires ::uwvm2::parser::wasm::concepts::operation::details::check_is_type_replacer<::uwvm2::parser::wasm::concepts::operation::type_replacer,
                                                                                             typename FeatureType::value_type>;
    };

    template <typename FeatureType>
    inline consteval auto get_value_type() noexcept
    {
        if constexpr(has_value_type<FeatureType>) { return typename FeatureType::value_type{}; }
        else
        {
            return ::uwvm2::parser::wasm::concepts::operation::irreplaceable_t{};
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using final_value_type_t = ::uwvm2::parser::wasm::concepts::operation::replacement_structure_t<decltype(get_value_type<Fs>())...>;

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct vec_value_type
    {
        static_assert(::std::is_enum_v<final_value_type_t<Fs...>>, "final_value_type_t<Fs...> is not enumeration");
        static_assert(::std::same_as<::std::underlying_type_t<final_value_type_t<Fs...>>, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>,
                      "final_value_type_t<Fs...> enumeration is not base on wasm_byte");

        final_value_type_t<Fs...> const* begin{};
        final_value_type_t<Fs...> const* end{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using final_result_type = vec_value_type<Fs...>;

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct final_function_type
    {
        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::function_type_prefix type{
            ::uwvm2::parser::wasm::standard::wasm1::type::function_type_prefix::functype};

        final_result_type<Fs...> parameter{};
        final_result_type<Fs...> result{};
    };

    /// @brief Wrapper for the section storage structure
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct final_function_type_section_details_wrapper_t
    {
        final_function_type<Fs...> const* function_type_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr final_function_type_section_details_wrapper_t<Fs...> section_details(final_function_type<Fs...> const& function_type) noexcept
    { return {::std::addressof(function_type)}; }

    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, final_function_type_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       final_function_type_section_details_wrapper_t<Fs...> const function_type_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(function_type_details_wrapper.function_type_ptr == nullptr) [[unlikely]] { ::uwvm2::utils::debug::trap_and_inform_bug_pos(); }
#endif

        {
            // output para
            auto const para{function_type_details_wrapper.function_type_ptr->parameter};

            // output '('
            if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "("); }
            else if constexpr(::std::same_as<char_type, wchar_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"("); }
            else if constexpr(::std::same_as<char_type, char8_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"("); }
            else if constexpr(::std::same_as<char_type, char16_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"("); }
            else if constexpr(::std::same_as<char_type, char32_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"("); }

            for(auto curr_value_type{para.begin}; curr_value_type != para.end; ++curr_value_type)
            {
                // print ','
                if(curr_value_type != para.begin)
                {
                    if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), ", "); }
                    else if constexpr(::std::same_as<char_type, wchar_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L", ");
                    }
                    else if constexpr(::std::same_as<char_type, char8_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8", ");
                    }
                    else if constexpr(::std::same_as<char_type, char16_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u", ");
                    }
                    else if constexpr(::std::same_as<char_type, char32_t>)
                    {
                        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U", ");
                    }
                }

                // print value type
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), section_details(*curr_value_type));
            }

            // output ')'
            if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), ")"); }
            else if constexpr(::std::same_as<char_type, wchar_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L")"); }
            else if constexpr(::std::same_as<char_type, char8_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8")"); }
            else if constexpr(::std::same_as<char_type, char16_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u")"); }
            else if constexpr(::std::same_as<char_type, char32_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U")"); }
        }

        // output '->'
        if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), " -> "); }
        else if constexpr(::std::same_as<char_type, wchar_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L" -> "); }
        else if constexpr(::std::same_as<char_type, char8_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8" -> "); }
        else if constexpr(::std::same_as<char_type, char16_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u" -> "); }
        else if constexpr(::std::same_as<char_type, char32_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U" -> "); }

        {
            // output res
            auto const res{function_type_details_wrapper.function_type_ptr->result};

            auto const res_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(res.end - res.begin)};
            if(res_size == 0uz)
            {
                if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "nil"); }
                else if constexpr(::std::same_as<char_type, wchar_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"nil"); }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"nil");
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"nil");
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"nil");
                }
            }
            else if(res_size == 1uz) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), section_details(*res.begin)); }
            else
            {
                if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "("); }
                else if constexpr(::std::same_as<char_type, wchar_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"("); }
                else if constexpr(::std::same_as<char_type, char8_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"("); }
                else if constexpr(::std::same_as<char_type, char16_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"("); }
                else if constexpr(::std::same_as<char_type, char32_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"("); }

                for(auto curr_value_type{res.begin}; curr_value_type != res.end; ++curr_value_type)
                {
                    if(curr_value_type != res.begin)
                    {
                        if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), ", "); }
                        else if constexpr(::std::same_as<char_type, wchar_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L", ");
                        }
                        else if constexpr(::std::same_as<char_type, char8_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8", ");
                        }
                        else if constexpr(::std::same_as<char_type, char16_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u", ");
                        }
                        else if constexpr(::std::same_as<char_type, char32_t>)
                        {
                            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U", ");
                        }
                    }

                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), section_details(*curr_value_type));
                }

                if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), ")"); }
                else if constexpr(::std::same_as<char_type, wchar_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L")"); }
                else if constexpr(::std::same_as<char_type, char8_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8")"); }
                else if constexpr(::std::same_as<char_type, char16_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u")"); }
                else if constexpr(::std::same_as<char_type, char32_t>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U")"); }
            }
        }
    }

    /// @brief      has type prefie
    /// @details
    ///             ```cpp
    ///             struct F
    ///             {
    ///                 using type_prefix = type_replacer<root_of_replacement, type_prefix>;
    ///             };
    ///             ```
    /// @see        test\0001.parser\0002.binfmt1\section\type_section.cc
    template <typename FeatureType>
    concept has_type_prefix = requires {
        typename FeatureType::type_prefix;
        requires ::uwvm2::parser::wasm::concepts::operation::details::check_is_type_replacer<::uwvm2::parser::wasm::concepts::operation::type_replacer,
                                                                                             typename FeatureType::type_prefix>;
    };

    template <typename FeatureType>
    inline consteval auto get_type_prefix() noexcept
    {
        if constexpr(has_type_prefix<FeatureType>) { return typename FeatureType::type_prefix{}; }
        else
        {
            return ::uwvm2::parser::wasm::concepts::operation::irreplaceable_t{};
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using final_type_prefix_t = ::uwvm2::parser::wasm::concepts::operation::replacement_structure_t<decltype(get_type_prefix<Fs>())...>;

    /// @brief      allow multi value
    /// @details    In the current version of WebAssembly, the length of the result type vector of a valid function type may be
    ///             at most 1. This restriction may be removed in future versions.
    ///
    ///             Define this to eliminate checking the length of the result.
    ///
    ///             ```cpp
    ///             struct F
    ///             {
    ///                 inline static constexpr bool allow_multi_result_vector{true};
    ///             };
    ///             ```
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.3.3
    /// @see        test\0001.parser\0002.binfmt1\section\type_section.cc
    template <typename FsCurr>
    concept has_allow_multi_result_vector = requires { requires ::std::same_as<::std::remove_cvref_t<decltype(FsCurr::allow_multi_result_vector)>, bool>; };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval bool allow_multi_result_vector() noexcept
    {
        return []<::std::size_t... I>(::std::index_sequence<I...>) constexpr noexcept
        {
            return ((
                        []<typename FsCurr>() constexpr noexcept -> bool
                                                                    {
                                                                        // check irreplaceable
                                                                        if constexpr(has_allow_multi_result_vector<FsCurr>)
                                                                        {
                                                                            constexpr bool tallow{FsCurr::allow_multi_result_vector};
                                                                            return tallow;
                                                                        }
                                                                        else
                                                                        {
                                                                            return false;
                                                                        }
                                                                    }.template operator()<Fs...[I]>()) ||
                    ...);
        }(::std::make_index_sequence<sizeof...(Fs)>{});
    }

    /// @brief      controllable allow multi value
    /// @details    Use a runtime boolean variable to control whether to start the check.
    ///
    ///             Prerequisites, satisfied:
    ///             - allow_multi_result_vector<Fs...>
    ///
    /// @see        allow_multi_result_vector
    /// @see        test/0001.parser/0002.binfmt1/section/type_section.cc
    template <typename FsCurr>
    concept has_curr_feature_parameter_controllable_allow_multi_result_vector = requires(FsCurr const& curr_feature_para) {
        requires ::std::same_as<::std::remove_cvref_t<decltype(curr_feature_para.controllable_allow_multi_result_vector)>, bool>;
    };

    template <typename... Para>
    inline consteval bool has_feature_parameter_controllable_allow_multi_result_vector_from_paras_parameters(
        ::uwvm2::utils::container::tuple<Para...> const&) noexcept
    {
        // e.g.
        // tuple<struct{bool controllable_allow_multi_result_vector{};}, ...> -> true
        // tuple<struct{bool controllable_allow_multi_result_vector{};}, struct{bool controllable_allow_multi_result_vector{};}, ...> -> contract_assert(false)

        bool has{};

        // Using double lambdas in consteval does not affect performance.
        [&]<::std::size_t... I>(::std::index_sequence<I...>) constexpr noexcept
        {
            ((
                 [&]<typename CurrParaType>() constexpr noexcept
                 {
                     if constexpr(has_curr_feature_parameter_controllable_allow_multi_result_vector<CurrParaType>)
                     {
#if __cpp_contracts >= 202502L
                         contract_assert(!has);
#else
                         if(has) { ::fast_io::fast_terminate(); }
#endif
                         has = true;
                     }
                 }.template operator()<Para...[I]>()),
             ...);
        }(::std::make_index_sequence<sizeof...(Para)>{});

        return has;
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval bool has_feature_parameter_controllable_allow_multi_result_vector_from_paras(
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& paras) noexcept
    { return has_feature_parameter_controllable_allow_multi_result_vector_from_paras_parameters(paras.parameters); }

    template <typename... Fs>
    concept has_feature_parameter_controllable_allow_multi_result_vector_from_paras_c =
        has_feature_parameter_controllable_allow_multi_result_vector_from_paras<Fs...>(::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...>{});

    /// @brief      get controllable allow multi value
    /// @details    Get the first controllable allow multi value from the parameters.
    /// @see        has_feature_parameter_controllable_allow_multi_result_vector_from_paras_c
    /// @see        test/0001.parser/0002.binfmt1/section/type_section.cc
    template <::std::size_t N, typename... Paras>
    inline constexpr bool get_feature_parameter_controllable_allow_multi_result_vector_from_paras_parameters_impl(
        ::uwvm2::utils::container::tuple<Paras...> const& paras) noexcept
    {
        if constexpr(N >= sizeof...(Paras)) { return false; }
        else
        {
            auto const& curr_para{get<N>(paras)};
            using curr_para_t = ::std::remove_cvref_t<decltype(curr_para)>;
            if constexpr(has_curr_feature_parameter_controllable_allow_multi_result_vector<curr_para_t>)
            {
                return curr_para.controllable_allow_multi_result_vector;
            }
            else
            {
                return get_feature_parameter_controllable_allow_multi_result_vector_from_paras_parameters_impl<N + 1uz>(paras);
            }
        }
    }

    template <typename... Para>
    inline constexpr bool get_feature_parameter_controllable_allow_multi_result_vector_from_paras_parameters(
        ::uwvm2::utils::container::tuple<Para...> const& paras) noexcept
    { return get_feature_parameter_controllable_allow_multi_result_vector_from_paras_parameters_impl<0uz>(paras); }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        requires (has_feature_parameter_controllable_allow_multi_result_vector_from_paras_c<Fs...>)
    inline constexpr bool get_feature_parameter_controllable_allow_multi_result_vector_from_paras(
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& paras) noexcept
    { return get_feature_parameter_controllable_allow_multi_result_vector_from_paras_parameters(paras.parameters); }

    /// @brief      Prohibit duplicate types
    /// @details    The WASM specification does not prohibit duplicate types; this is a reserved concept.
    ///
    ///             ```cpp
    ///             struct F
    ///             {
    ///                 inline static constexpr bool prohibit_duplicate_types{true};
    ///             };
    ///             ```
    /// @see        test\0001.parser\0002.binfmt1\section\import_section.cc
    template <typename FsCurr>
    concept has_prohibit_duplicate_types = requires { requires ::std::same_as<::std::remove_cvref_t<decltype(FsCurr::prohibit_duplicate_types)>, bool>; };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval bool prohibit_duplicate_types() noexcept
    {
        return []<::std::size_t... I>(::std::index_sequence<I...>) constexpr noexcept
        {
            return ((
                        []<typename FsCurr>() constexpr noexcept -> bool
                                                                    {
                                                                        // check irreplaceable
                                                                        if constexpr(has_prohibit_duplicate_types<FsCurr>)
                                                                        {
                                                                            constexpr bool tallow{FsCurr::prohibit_duplicate_types};
                                                                            return tallow;
                                                                        }
                                                                        else
                                                                        {
                                                                            return false;
                                                                        }
                                                                    }.template operator()<Fs...[I]>()) ||
                    ...);
        }(::std::make_index_sequence<sizeof...(Fs)>{});
    }

    /////////////////////////////
    //      Import Section     //
    /////////////////////////////

    /// @brief      has extern type
    /// @details
    ///             ```cpp
    ///             enum class basic_external_type : wasm_byte
    ///             {
    ///                 e1 = 0x00u,
    ///                 e2 = 0x01u,
    ///                 external_type_end = e2 // Used to generate the length of the array
    ///             };
    ///
    ///             template <typename ... Fs> // Fs is used as an extension to an existing type, but does not extend the type
    ///             struct basic_extern_type
    ///             {
    ///                 union storage_t
    ///                 {
    ///                     basic_function_type<Fs ...> const* function;
    ///                     basic_table_type<Fs ...> table;
    ///                     basic_memory_type<Fs ...> memory;
    ///                     basic_global_type<Fs ...> global;
    ///                 } storage;
    ///
    ///                 basic_external_type type{};
    ///             };
    ///
    ///             struct F
    ///             {
    ///                 template <typename ... Fs>
    ///                 using extern_type = type_replacer<root_of_replacement, basic_extern_type<Fs...>>;
    ///             };
    ///             ```
    template <typename FeatureType, typename... Fs>
    concept has_extern_type = requires {
        typename FeatureType::template extern_type<Fs...>;
        requires ::uwvm2::parser::wasm::concepts::operation::details::check_is_type_replacer<::uwvm2::parser::wasm::concepts::operation::type_replacer,
                                                                                             typename FeatureType::template extern_type<Fs...>>;
    };

    template <typename FeatureType, typename... Fs>
    inline consteval auto get_extern_type() noexcept
    {
        if constexpr(has_extern_type<FeatureType>) { return typename FeatureType::template extern_type<Fs...>{}; }
        else
        {
            return ::uwvm2::parser::wasm::concepts::operation::irreplaceable_t{};
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using final_extern_type_t = ::uwvm2::parser::wasm::concepts::operation::replacement_structure_t<decltype(get_extern_type<Fs, Fs...>())...>;

    template <typename... Fs>
    concept is_valid_final_extern_type_t = requires(final_extern_type_t<Fs...> ext) {
        requires ::std::is_enum_v<decltype(ext.type)>;
        requires ::std::same_as<::std::underlying_type_t<decltype(ext.type)>, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>;
        decltype(ext.type)::external_type_end;
        requires ::std::is_union_v<decltype(ext.storage)>;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct final_import_type 
    {
        static_assert(is_valid_final_extern_type_t<Fs...>);

        ::uwvm2::utils::container::u8string_view module_name{};
        ::uwvm2::utils::container::u8string_view extern_name{};

        final_extern_type_t<Fs...> imports{};

        // check
        static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<final_extern_type_t<Fs...>>);
        static_assert(::fast_io::freestanding::is_zero_default_constructible_v<final_extern_type_t<Fs...>>);
    };

    /// @brief Wrapper for the final_import_type
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct final_import_type_section_details_wrapper_t
    {
        final_import_type<Fs...> const* import_type_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32* importdesc_counter_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr final_import_type_section_details_wrapper_t<Fs...> section_details(
        final_import_type<Fs...> const& import_type,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections,
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32* importdesc_counter_ptr) noexcept
    { return {::std::addressof(import_type), ::std::addressof(all_sections), importdesc_counter_ptr}; }

    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, final_import_type_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       final_import_type_section_details_wrapper_t<Fs...> const import_type_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(import_type_details_wrapper.import_type_ptr == nullptr || import_type_details_wrapper.all_sections_ptr == nullptr ||
           import_type_details_wrapper.importdesc_counter_ptr == nullptr) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        if constexpr(::std::same_as<char_type, char>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             section_details(import_type_details_wrapper.import_type_ptr->imports,
                                                                             *import_type_details_wrapper.all_sections_ptr,
                                                                             import_type_details_wrapper.importdesc_counter_ptr),
                                                             ", Module Name: \"",
                                                             ::fast_io::mnp::code_cvt(import_type_details_wrapper.import_type_ptr->module_name),
                                                             "\", Extern Name: \"",
                                                             ::fast_io::mnp::code_cvt(import_type_details_wrapper.import_type_ptr->extern_name),
                                                             "\"");
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             section_details(import_type_details_wrapper.import_type_ptr->imports,
                                                                             *import_type_details_wrapper.all_sections_ptr,
                                                                             import_type_details_wrapper.importdesc_counter_ptr),
                                                             L", Module Name: \"",
                                                             ::fast_io::mnp::code_cvt(import_type_details_wrapper.import_type_ptr->module_name),
                                                             L"\", Extern Name: \"",
                                                             ::fast_io::mnp::code_cvt(import_type_details_wrapper.import_type_ptr->extern_name),
                                                             L"\"");
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            // All are u8string_view, no transcoding required.
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             section_details(import_type_details_wrapper.import_type_ptr->imports,
                                                                             *import_type_details_wrapper.all_sections_ptr,
                                                                             import_type_details_wrapper.importdesc_counter_ptr),
                                                             u8", Module Name: \"",
                                                             import_type_details_wrapper.import_type_ptr->module_name,
                                                             u8"\", Extern Name: \"",
                                                             import_type_details_wrapper.import_type_ptr->extern_name,
                                                             u8"\"");
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             section_details(import_type_details_wrapper.import_type_ptr->imports,
                                                                             *import_type_details_wrapper.all_sections_ptr,
                                                                             import_type_details_wrapper.importdesc_counter_ptr),
                                                             u", Module Name: \"",
                                                             ::fast_io::mnp::code_cvt(import_type_details_wrapper.import_type_ptr->module_name),
                                                             u"\", Extern Name: \"",
                                                             ::fast_io::mnp::code_cvt(import_type_details_wrapper.import_type_ptr->extern_name),
                                                             u"\"");
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                             section_details(import_type_details_wrapper.import_type_ptr->imports,
                                                                             *import_type_details_wrapper.all_sections_ptr,
                                                                             import_type_details_wrapper.importdesc_counter_ptr),
                                                             U", Module Name: \"",
                                                             ::fast_io::mnp::code_cvt(import_type_details_wrapper.import_type_ptr->module_name),
                                                             U"\", Extern Name: \"",
                                                             ::fast_io::mnp::code_cvt(import_type_details_wrapper.import_type_ptr->extern_name),
                                                             U"\"");
        }
    }

    /// @brief      Prohibited use of strings with a length of 0
    /// @details    The WASM specification does not prohibit zero strings; this is a reserved concept.
    ///
    ///             ```cpp
    ///             struct F
    ///             {
    ///                 inline static constexpr bool disable_zero_length_string{true};
    ///             };
    ///             ```
    /// @see        test\0001.parser\0002.binfmt1\section\import_section.cc
    template <typename FsCurr>
    concept has_disable_zero_length_string = requires { requires ::std::same_as<::std::remove_cvref_t<decltype(FsCurr::disable_zero_length_string)>, bool>; };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval bool disable_zero_length_string() noexcept
    {
        return []<::std::size_t... I>(::std::index_sequence<I...>) constexpr noexcept
        {
            return ((
                        []<typename FsCurr>() constexpr noexcept -> bool
                                                                    {
                                                                        // check irreplaceable
                                                                        if constexpr(has_disable_zero_length_string<FsCurr>)
                                                                        {
                                                                            constexpr bool tallow{FsCurr::disable_zero_length_string};
                                                                            return tallow;
                                                                        }
                                                                        else
                                                                        {
                                                                            return false;
                                                                        }
                                                                    }.template operator()<Fs...[I]>()) ||
                    ...);
        }(::std::make_index_sequence<sizeof...(Fs)>{});
    }

    /// @brief      No duplicate import items allowed
    /// @details    Currently, the WebAssembly core specification does not prohibit the existence of identical import items (same module name + function name +
    ///             type) in a module, but some host environments require that import object keys be unique when instantiating, causing duplicate imports to
    ///             fail during actual runtime. Here is a concept that can prohibit duplicate import items.
    ///
    ///             Here, modules and names are bound together for duplicate checks. It is permissible to import different modules with the same name or
    ///             different names within the same module.
    ///
    ///             ```cpp
    ///             struct F
    ///             {
    ///                 inline static constexpr bool check_duplicate_imports{true};
    ///             };
    ///             ```
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.5.11
    /// @note       Unlike export names, import names are not necessarily unique. It is possible to import the same module/name pair multiple times; such
    ///             imports may even have different type descriptions, including different kinds of entities. A module with such imports can still be
    ///             instantiated depending on the specifics of how an embedder allows resolving and supplying imports. However, embedders are not required to
    ///             support such overloading, and a WebAssembly module itself cannot implement an overloaded name
    template <typename FsCurr>
    concept has_check_duplicate_imports = requires { requires ::std::same_as<::std::remove_cvref_t<decltype(FsCurr::check_duplicate_imports)>, bool>; };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval bool check_duplicate_imports() noexcept
    {
        return []<::std::size_t... I>(::std::index_sequence<I...>) constexpr noexcept
        {
            return ((
                        []<typename FsCurr>() constexpr noexcept -> bool
                                                                    {
                                                                        // check irreplaceable
                                                                        if constexpr(has_check_duplicate_imports<FsCurr>)
                                                                        {
                                                                            constexpr bool tallow{FsCurr::check_duplicate_imports};
                                                                            return tallow;
                                                                        }
                                                                        else
                                                                        {
                                                                            return false;
                                                                        }
                                                                    }.template operator()<Fs...[I]>()) ||
                    ...);
        }(::std::make_index_sequence<sizeof...(Fs)>{});
    }

    /// @brief      controllable check duplicate imports
    /// @details    Use a runtime boolean variable to control whether to start the check.
    ///
    ///             Prerequisites, satisfied:
    ///             - check_duplicate_imports<Fs...>
    ///
    /// @see        check_duplicate_imports
    /// @see        test/0001.parser/0002.binfmt1/section/import_section.cc
    template <typename FsCurr>
    concept has_curr_feature_parameter_controllable_check_duplicate_imports = requires(FsCurr const& curr_feature_para) {
        requires ::std::same_as<::std::remove_cvref_t<decltype(curr_feature_para.controllable_check_duplicate_imports)>, bool>;
    };

    template <typename... Para>
    inline consteval bool has_feature_parameter_controllable_check_duplicate_imports_from_paras_parameters(
        ::uwvm2::utils::container::tuple<Para...> const&) noexcept
    {
        // e.g.
        // tuple<struct{bool controllable_check_duplicate_imports{};}, ...> -> true
        // tuple<struct{bool controllable_check_duplicate_imports{};}, struct{bool controllable_check_duplicate_imports{};}, ...> -> contract_assert(false)

        bool has{};

        // Using double lambdas in consteval does not affect performance.
        [&]<::std::size_t... I>(::std::index_sequence<I...>) constexpr noexcept
        {
            ((
                 [&]<typename CurrParaType>() constexpr noexcept
                 {
                     if constexpr(has_curr_feature_parameter_controllable_check_duplicate_imports<CurrParaType>)
                     {
#if __cpp_contracts >= 202502L
                         contract_assert(!has);
#else
                         if(has) { ::fast_io::fast_terminate(); }
#endif
                         has = true;
                     }
                 }.template operator()<Para...[I]>()),
             ...);
        }(::std::make_index_sequence<sizeof...(Para)>{});

        return has;
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval bool has_feature_parameter_controllable_check_duplicate_imports_from_paras(
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& paras) noexcept
    { return has_feature_parameter_controllable_check_duplicate_imports_from_paras_parameters(paras.parameters); }

    template <typename... Fs>
    concept has_feature_parameter_controllable_check_duplicate_imports_from_paras_c =
        has_feature_parameter_controllable_check_duplicate_imports_from_paras<Fs...>(::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...>{});

    /// @brief      get controllable check duplicate imports
    /// @details    Get the first controllable check duplicate imports from the parameters.
    /// @see        has_feature_parameter_controllable_check_duplicate_imports_from_paras_c
    /// @see        test/0001.parser/0002.binfmt1/section/import_section.cc
    template <::std::size_t N, typename... Paras>
    inline constexpr bool get_feature_parameter_controllable_check_duplicate_imports_from_paras_parameters_impl(
        ::uwvm2::utils::container::tuple<Paras...> const& paras) noexcept
    {
        if constexpr(N >= sizeof...(Paras)) { return false; }
        else
        {
            auto const& curr_para{get<N>(paras)};
            using curr_para_t = ::std::remove_cvref_t<decltype(curr_para)>;
            if constexpr(has_curr_feature_parameter_controllable_check_duplicate_imports<curr_para_t>)
            {
                return curr_para.controllable_check_duplicate_imports;
            }
            else
            {
                return get_feature_parameter_controllable_check_duplicate_imports_from_paras_parameters_impl<N + 1uz>(paras);
            }
        }
    }

    template <typename... Para>
    inline constexpr bool get_feature_parameter_controllable_check_duplicate_imports_from_paras_parameters(
        ::uwvm2::utils::container::tuple<Para...> const& paras) noexcept
    { return get_feature_parameter_controllable_check_duplicate_imports_from_paras_parameters_impl<0uz>(paras); }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        requires (has_feature_parameter_controllable_check_duplicate_imports_from_paras_c<Fs...>)
    inline constexpr bool get_feature_parameter_controllable_check_duplicate_imports_from_paras(
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& paras) noexcept
    { return get_feature_parameter_controllable_check_duplicate_imports_from_paras_parameters(paras.parameters); }

    /// @brief      has table_type
    /// @details
    ///             ```cpp
    ///             struct F
    ///             {
    ///                 using table_type = type_replacer<root_of_replacement, table_type>;
    ///             };
    ///             ```
    template <typename FeatureType>
    concept has_table_type = requires {
        typename FeatureType::table_type;
        requires ::uwvm2::parser::wasm::concepts::operation::details::check_is_type_replacer<::uwvm2::parser::wasm::concepts::operation::type_replacer,
                                                                                             typename FeatureType::table_type>;
    };

    template <typename FeatureType>
    inline consteval auto get_table_type() noexcept
    {
        if constexpr(has_table_type<FeatureType>) { return typename FeatureType::table_type{}; }
        else
        {
            return ::uwvm2::parser::wasm::concepts::operation::irreplaceable_t{};
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using final_table_type = ::uwvm2::parser::wasm::concepts::operation::replacement_structure_t<decltype(get_table_type<Fs>())...>;

    /// @brief      has memory_type
    /// @details
    ///             ```cpp
    ///             struct F
    ///             {
    ///                 using memory_type = type_replacer<root_of_replacement, memory_type>;
    ///             };
    ///             ```
    template <typename FeatureType>
    concept has_memory_type = requires {
        typename FeatureType::memory_type;
        requires ::uwvm2::parser::wasm::concepts::operation::details::check_is_type_replacer<::uwvm2::parser::wasm::concepts::operation::type_replacer,
                                                                                             typename FeatureType::memory_type>;
    };

    template <typename FeatureType>
    inline consteval auto get_memory_type() noexcept
    {
        if constexpr(has_memory_type<FeatureType>) { return typename FeatureType::memory_type{}; }
        else
        {
            return ::uwvm2::parser::wasm::concepts::operation::irreplaceable_t{};
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using final_memory_type = ::uwvm2::parser::wasm::concepts::operation::replacement_structure_t<decltype(get_memory_type<Fs>())...>;

    /// @brief      has global_type
    /// @details
    ///             ```cpp
    ///             struct F
    ///             {
    ///                 using global_type = type_replacer<root_of_replacement, global_type>;
    ///             };
    ///             ```
    template <typename FeatureType>
    concept has_global_type = requires {
        typename FeatureType::global_type;
        requires ::uwvm2::parser::wasm::concepts::operation::details::check_is_type_replacer<::uwvm2::parser::wasm::concepts::operation::type_replacer,
                                                                                             typename FeatureType::global_type>;
    };

    template <typename FeatureType>
    inline consteval auto get_global_type() noexcept
    {
        if constexpr(has_global_type<FeatureType>) { return typename FeatureType::global_type{}; }
        else
        {
            return ::uwvm2::parser::wasm::concepts::operation::irreplaceable_t{};
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using final_global_type = ::uwvm2::parser::wasm::concepts::operation::replacement_structure_t<decltype(get_global_type<Fs>())...>;

    /////////////////////////////
    //     Function Section    //
    /////////////////////////////

    /// @brief      final_local_function_type
    /// @details    Storing pointers instead of u32 doubles the memory on 64-bit platforms, and then the index * size is pre-calculated by simd. on 32-bit
    ///             platforms, the memory footprint is the same. On all platforms, calculating the index * size at the same time as the parse reduces the time
    ///             required for subsequent index * size calculations.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct final_local_function_type
    {
        final_function_type<Fs...> const* func_type{};
    };

    /////////////////////////////
    //      Table Section      //
    /////////////////////////////

    /// @brief      allow multi table
    /// @details    In the current version of WebAssembly, at most one table may be defined or imported in a single module,
    ///             and all constructs implicitly reference this table 0. This restriction may be lifted in future versions.
    ///
    ///             Define this to eliminate checking the length of the result.
    ///
    ///             ```cpp
    ///             struct F
    ///             {
    ///                 inline static constexpr bool allow_multi_table{true};
    ///             };
    ///             ```
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.5.4
    /// @see        test\0001.parser\0002.binfmt1\section\table_section.cc
    template <typename FsCurr>
    concept has_allow_multi_table = requires { requires ::std::same_as<::std::remove_cvref_t<decltype(FsCurr::allow_multi_table)>, bool>; };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval bool allow_multi_table() noexcept
    {
        return []<::std::size_t... I>(::std::index_sequence<I...>) constexpr noexcept
        {
            return ((
                        []<typename FsCurr>() constexpr noexcept -> bool
                                                                    {
                                                                        // check irreplaceable
                                                                        if constexpr(has_allow_multi_table<FsCurr>)
                                                                        {
                                                                            constexpr bool tallow{FsCurr::allow_multi_table};
                                                                            return tallow;
                                                                        }
                                                                        else
                                                                        {
                                                                            return false;
                                                                        }
                                                                    }.template operator()<Fs...[I]>()) ||
                    ...);
        }(::std::make_index_sequence<sizeof...(Fs)>{});
    }

    // Since multi-table changes the parsing behavior, wasm1.1 uses the features of wasm1.0 for extension, so a freely controllable version is not provided
    // here.

    /////////////////////////////
    //      Memory Section     //
    /////////////////////////////

    /// @brief      allow multi memory
    /// @details    In the current version of WebAssembly, at most one memory may be defined or imported in a single module,
    ///             and all constructs implicitly reference this memory 0. This restriction may be lifted in future versions
    ///
    ///             Define this to eliminate checking the length of the result.
    ///
    ///             ```cpp
    ///             struct F
    ///             {
    ///                 inline static constexpr bool allow_multi_memory{true};
    ///             };
    ///             ```
    /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.5.5
    /// @see        test\0001.parser\0002.binfmt1\section\memory_section.cc
    template <typename FsCurr>
    concept has_allow_multi_memory = requires { requires ::std::same_as<::std::remove_cvref_t<decltype(FsCurr::allow_multi_memory)>, bool>; };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval bool allow_multi_memory() noexcept
    {
        return []<::std::size_t... I>(::std::index_sequence<I...>) constexpr noexcept
        {
            return ((
                        []<typename FsCurr>() constexpr noexcept -> bool
                                                                    {
                                                                        // check irreplaceable
                                                                        if constexpr(has_allow_multi_memory<FsCurr>)
                                                                        {
                                                                            constexpr bool tallow{FsCurr::allow_multi_memory};
                                                                            return tallow;
                                                                        }
                                                                        else
                                                                        {
                                                                            return false;
                                                                        }
                                                                    }.template operator()<Fs...[I]>()) ||
                    ...);
        }(::std::make_index_sequence<sizeof...(Fs)>{});
    }

    // Since multi-memory changes the parsing behavior, wasm1.1 uses the features of wasm1.0 for extension, so a freely controllable version is not provided
    // here.

    /////////////////////////////
    //      Global Section     //
    /////////////////////////////

    /// @brief      has wasm_const_expr
    /// @details
    ///             ```cpp
    ///             struct F
    ///             {
    ///                 using wasm_const_expr = type_replacer<root_of_replacement, wasm_const_expr>;
    ///             };
    ///             ```
    template <typename FeatureType>
    concept has_wasm_const_expr = requires {
        typename FeatureType::wasm_const_expr;
        requires ::uwvm2::parser::wasm::concepts::operation::details::check_is_type_replacer<::uwvm2::parser::wasm::concepts::operation::type_replacer,
                                                                                             typename FeatureType::wasm_const_expr>;
    };

    template <typename FeatureType>
    inline consteval auto get_wasm_const_expr() noexcept
    {
        if constexpr(has_wasm_const_expr<FeatureType>) { return typename FeatureType::wasm_const_expr{}; }
        else
        {
            return ::uwvm2::parser::wasm::concepts::operation::irreplaceable_t{};
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using final_wasm_const_expr = ::uwvm2::parser::wasm::concepts::operation::replacement_structure_t<decltype(get_wasm_const_expr<Fs>())...>;

    /// @brief final local global type
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct final_local_global_type 
    {
        final_global_type<Fs...> global{};    // Types used to store global
        final_wasm_const_expr<Fs...> expr{};  // Expressions used to initialize global

        // check
        static_assert(::fast_io::freestanding::is_zero_default_constructible_v<final_global_type<Fs...>>);
        static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<final_global_type<Fs...>>);
        static_assert(::fast_io::freestanding::is_zero_default_constructible_v<final_wasm_const_expr<Fs...>>);
        static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<final_wasm_const_expr<Fs...>>);
    };

    /////////////////////////////
    //      Export Section     //
    /////////////////////////////

    /// @brief      has export type
    /// @details
    ///             ```cpp
    ///             enum class basic_external_type : wasm_byte
    ///             {
    ///                 e1 = 0x00u,
    ///                 e2 = 0x01u,
    ///                 external_type_end = e2 // Used to generate the length of the array
    ///             };
    ///
    ///             template <typename ... Fs> // Fs is used as an extension to an existing type, but does not extend the type
    ///             struct basic_export_type
    ///             {
    ///                 union export_storage_t storage{};
    ///
    ///                 basic_external_type type{};
    ///             };
    ///
    ///             struct F
    ///             {
    ///                 template <typename ... Fs>
    ///                 using export_type = type_replacer<root_of_replacement, basic_export_type<Fs...>>;
    ///             };
    ///             ```
    template <typename FeatureType, typename... Fs>
    concept has_export_type = requires {
        typename FeatureType::template export_type<Fs...>;
        requires ::uwvm2::parser::wasm::concepts::operation::details::check_is_type_replacer<::uwvm2::parser::wasm::concepts::operation::type_replacer,
                                                                                             typename FeatureType::template export_type<Fs...>>;
    };

    template <typename FeatureType, typename... Fs>
    inline consteval auto get_export_type() noexcept
    {
        if constexpr(has_export_type<FeatureType>) { return typename FeatureType::template export_type<Fs...>{}; }
        else
        {
            return ::uwvm2::parser::wasm::concepts::operation::irreplaceable_t{};
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using final_export_type_t = ::uwvm2::parser::wasm::concepts::operation::replacement_structure_t<decltype(get_export_type<Fs, Fs...>())...>;

    template <typename... Fs>
    concept is_valid_final_export_type_t = requires(final_export_type_t<Fs...> ext) {
        requires ::std::is_enum_v<decltype(ext.type)>;
        requires ::std::same_as<::std::underlying_type_t<decltype(ext.type)>, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>;
        decltype(ext.type)::external_type_end;
        requires ::std::is_union_v<decltype(ext.storage)>;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct final_wasm_export_type 
    {
        static_assert(is_valid_final_export_type_t<Fs...>);

        ::uwvm2::utils::container::u8string_view export_name{};

        final_export_type_t<Fs...> exports{};

        // check
        static_assert(::fast_io::freestanding::is_zero_default_constructible_v<final_export_type_t<Fs...>>);
        static_assert(::fast_io::freestanding::is_trivially_copyable_or_relocatable_v<final_export_type_t<Fs...>>);
    };

    /// @brief Wrapper for the final_import_type
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct final_export_type_section_details_wrapper_t
    {
        final_wasm_export_type<Fs...> const* export_type_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr final_export_type_section_details_wrapper_t<Fs...> section_details(
        final_wasm_export_type<Fs...> const& export_type,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    { return {::std::addressof(export_type), ::std::addressof(all_sections)}; }

    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, final_export_type_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       final_export_type_section_details_wrapper_t<Fs...> const export_type_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(export_type_details_wrapper.export_type_ptr == nullptr || export_type_details_wrapper.all_sections_ptr == nullptr) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        if constexpr(::std::same_as<char_type, char>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                section_details(export_type_details_wrapper.export_type_ptr->exports, *export_type_details_wrapper.all_sections_ptr),
                ", Export Name: \"",
                ::fast_io::mnp::code_cvt(export_type_details_wrapper.export_type_ptr->export_name),
                "\"");
        }
        else if constexpr(::std::same_as<char_type, wchar_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                section_details(export_type_details_wrapper.export_type_ptr->exports, *export_type_details_wrapper.all_sections_ptr),
                L", Export Name: \"",
                ::fast_io::mnp::code_cvt(export_type_details_wrapper.export_type_ptr->export_name),
                L"\"");
        }
        else if constexpr(::std::same_as<char_type, char8_t>)
        {
            // All are u8string_view, no transcoding required.
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                section_details(export_type_details_wrapper.export_type_ptr->exports, *export_type_details_wrapper.all_sections_ptr),
                u8", Export Name: \"",
                export_type_details_wrapper.export_type_ptr->export_name,
                u8"\"");
        }
        else if constexpr(::std::same_as<char_type, char16_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                section_details(export_type_details_wrapper.export_type_ptr->exports, *export_type_details_wrapper.all_sections_ptr),
                u", Export Name: \"",
                ::fast_io::mnp::code_cvt(export_type_details_wrapper.export_type_ptr->export_name),
                u"\"");
        }
        else if constexpr(::std::same_as<char_type, char32_t>)
        {
            ::fast_io::operations::print_freestanding<false>(
                ::std::forward<Stm>(stream),
                section_details(export_type_details_wrapper.export_type_ptr->exports, *export_type_details_wrapper.all_sections_ptr),
                U", Export Name: \"",
                ::fast_io::mnp::code_cvt(export_type_details_wrapper.export_type_ptr->export_name),
                U"\"");
        }
    }

    //////////////////////////////
    //      Element Section     //
    //////////////////////////////

    /// @brief      has element type
    /// @details
    ///             ```cpp
    ///             enum class basic_element_type : wasm_u32
    ///             {
    ///                 e1 = 0x00u,
    ///                 e2 = 0x01u,
    ///             };
    ///
    ///             template <typename ... Fs> // Fs is used as an extension to an existing type, but does not extend the type
    ///             struct basic_element_t
    ///             {
    ///                 union storage_t
    ///                 {
    ///                     // ...
    ///                 } storage;
    ///
    ///                 basic_element_type type{};
    ///             };
    ///
    ///             struct F
    ///             {
    ///                 template <typename ... Fs>
    ///                 using element_type = type_replacer<root_of_replacement, basic_element_t<Fs...>>;
    ///             };
    ///             ```
    template <typename FeatureType, typename... Fs>
    concept has_element_type = requires {
        typename FeatureType::template element_type<Fs...>;
        requires ::uwvm2::parser::wasm::concepts::operation::details::check_is_type_replacer<::uwvm2::parser::wasm::concepts::operation::type_replacer,
                                                                                             typename FeatureType::template element_type<Fs...>>;
    };

    template <typename FeatureType, typename... Fs>
    inline consteval auto get_element_type() noexcept
    {
        if constexpr(has_element_type<FeatureType>) { return typename FeatureType::template element_type<Fs...>{}; }
        else
        {
            return ::uwvm2::parser::wasm::concepts::operation::irreplaceable_t{};
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using final_element_type_t = ::uwvm2::parser::wasm::concepts::operation::replacement_structure_t<decltype(get_element_type<Fs, Fs...>())...>;

    template <typename... Fs>
    concept is_valid_final_element_type_t = requires(final_element_type_t<Fs...> ext) {
        requires ::std::is_enum_v<decltype(ext.type)>;
        requires ::std::same_as<::std::underlying_type_t<decltype(ext.type)>, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>;
        requires ::std::is_union_v<decltype(ext.storage)>;
    };

    ////////////////////////////
    //      Data Section      //
    ////////////////////////////

    /// @brief      has data type
    /// @details
    ///             ```cpp
    ///             enum class basic_data_type : wasm_u32
    ///             {
    ///                 e1 = 0x00u,
    ///                 e2 = 0x01u,
    ///             };
    ///
    ///             template <typename ... Fs> // Fs is used as an extension to an existing type, but does not extend the type
    ///             struct basic_data_t
    ///             {
    ///                 union storage_t
    ///                 {
    ///                     // ...
    ///                 } storage;
    ///
    ///                 basic_data_type type{};
    ///             };
    ///
    ///             struct F
    ///             {
    ///                 template <typename ... Fs>
    ///                 using data_type = type_replacer<root_of_replacement, basic_data_t<Fs...>>;
    ///             };
    ///             ```
    template <typename FeatureType, typename... Fs>
    concept has_data_type = requires {
        typename FeatureType::template data_type<Fs...>;
        requires ::uwvm2::parser::wasm::concepts::operation::details::check_is_type_replacer<::uwvm2::parser::wasm::concepts::operation::type_replacer,
                                                                                             typename FeatureType::template data_type<Fs...>>;
    };

    template <typename FeatureType, typename... Fs>
    inline consteval auto get_data_type() noexcept
    {
        if constexpr(has_data_type<FeatureType>) { return typename FeatureType::template data_type<Fs...>{}; }
        else
        {
            return ::uwvm2::parser::wasm::concepts::operation::irreplaceable_t{};
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using final_data_type_t = ::uwvm2::parser::wasm::concepts::operation::replacement_structure_t<decltype(get_data_type<Fs, Fs...>())...>;

    template <typename... Fs>
    concept is_valid_final_data_type_t = requires(final_data_type_t<Fs...> ext) {
        requires ::std::is_enum_v<decltype(ext.type)>;
        requires ::std::same_as<::std::underlying_type_t<decltype(ext.type)>, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>;
        requires ::std::is_union_v<decltype(ext.storage)>;
    };
}

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::final_function_type<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::final_import_type<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::final_import_type<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::final_local_function_type<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::final_local_global_type<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::final_local_global_type<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_export_type<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::final_wasm_export_type<Fs...>>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
