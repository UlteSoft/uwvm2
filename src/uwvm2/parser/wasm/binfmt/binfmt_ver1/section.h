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
 * @date        2025-04-12
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
# include <concepts>
# include <type_traits>
# include <utility>
# include <memory>
# include <tuple>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::binfmt::ver1
{
    /// @brief  The method that will get the extensible section type
    /// @see    test\0001.parser\0001.concept\splice_section_storage_structure.cc
    template <typename Feature, typename... Fs>
    concept has_binfmt_ver1_extensible_section_define = ::fast_io::is_tuple<typename Feature::template binfmt_ver1_section_type<Fs...>>;

    template <typename Feature, typename... Fs>
    concept has_binfmt_ver1_section_define = ::fast_io::is_tuple<typename Feature::binfmt_ver1_section_type>;

    /// @brief  Merge sections together with a tuple merger, supports template extensions.
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Features>
    inline consteval auto splice_section_storage_structure() noexcept
    {
        if constexpr(sizeof...(Features) == 0) { return ::uwvm2::parser::wasm::concepts::operation::tuple_megger<>{}; }
        else
        {
            return []<::std::size_t... I>(::std::index_sequence<I...>) constexpr noexcept
            {
                return ((
                            []<::uwvm2::parser::wasm::concepts::wasm_feature FeatureCurr>() constexpr noexcept
                            {
                                if constexpr(has_binfmt_ver1_extensible_section_define<FeatureCurr, Features...>)
                                {
                                    using extensible_section_t = typename FeatureCurr::template binfmt_ver1_section_type<Features...>;

                                    static_assert(
                                        ::uwvm2::parser::wasm::concepts::operation::is_trivially_copyable_or_relocatable_tuple(extensible_section_t{}),
                                        "extensible_section_t is not trivially copyable or relocatable");

                                    return ::uwvm2::parser::wasm::concepts::operation::get_tuple_megger_from_tuple(extensible_section_t{});
                                }
                                else if constexpr(has_binfmt_ver1_section_define<FeatureCurr>)
                                {
                                    using section_t = typename FeatureCurr::binfmt_ver1_section_type;

                                    static_assert(::uwvm2::parser::wasm::concepts::operation::is_trivially_copyable_or_relocatable_tuple(section_t{}),
                                                  "section_t is not trivially copyable or relocatable");

                                    return ::uwvm2::parser::wasm::concepts::operation::get_tuple_megger_from_tuple(section_t{});
                                }
                                else
                                {
                                    static_assert(!((
                                                        requires { typename FeatureCurr::template binfmt_ver1_section_type<Features...>; } ||
                                                        requires { typename FeatureCurr::binfmt_ver1_section_type; }) &&
                                                    !has_binfmt_ver1_extensible_section_define<FeatureCurr, Features...>),
                                                  "binfmt_ver1_section_type is not tuple");
                                    return ::uwvm2::parser::wasm::concepts::operation::tuple_megger<>{};
                                }
                            }.template operator()<Features...[I]>()),  // This is an overloaded comma expression
                        ...);
            }(::std::make_index_sequence<sizeof...(Features)>{});
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Features>
    using splice_section_storage_structure_t = decltype(splice_section_storage_structure<Features...>())::Type;

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Features>
    inline consteval auto splice_section_storage_structure_from_tuple(::uwvm2::utils::container::tuple<Features...>) noexcept
    { return splice_section_storage_structure<Features...>(); }

    /// @brief Wrapper for the section storage structure
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct splice_section_storage_structure_section_details_wrapper_t
    {
        splice_section_storage_structure_t<Fs...> const* section_storage_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr splice_section_storage_structure_section_details_wrapper_t<Fs...> section_details(
        splice_section_storage_structure_t<Fs...> const& section_storage) noexcept
    { return {::std::addressof(section_storage)}; }

    /// @brief Print the module section details
    /// @throws maybe throw fast_io::error, see the implementation of the stream
    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, splice_section_storage_structure_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       splice_section_storage_structure_section_details_wrapper_t<Fs...> const section_details_wrapper)
    {
        if(section_details_wrapper.section_storage_ptr == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

        auto const& all_sections{*section_details_wrapper.section_storage_ptr};

#if __cpp_structured_bindings >= 202411L
        auto const& [... secs]{all_sections};
        ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), section_details<Fs...>(secs, all_sections)...);
#else
        /// @throws maybe throw fast_io::error, see the implementation of the stream
        [&all_sections]<::std::size_t... I> UWVM_ALWAYS_INLINE(::std::index_sequence<I...>, Stm&& stream) constexpr -> void
        {
            ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), section_details<Fs...>(get<I>(all_sections), all_sections)...);
        }(::std::make_index_sequence<::std::tuple_size_v<::std::remove_cvref_t<decltype(all_sections)>>>{}, ::std::forward<Stm>(stream));
#endif
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
