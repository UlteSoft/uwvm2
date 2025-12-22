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
# include <type_traits>
# include <concepts>
# include <utility>
# include <algorithm>
# include <vector>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include "root.h"
# include "operation.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::concepts
{
    template <typename Fc>
    concept has_feature_parameter = requires { typename Fc::parameter; };

    /// @brief Splicing the structure used to store feature parameters
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto splice_feature_parameter() noexcept
    {
        if constexpr(sizeof...(Fs) == 0) { return ::uwvm2::parser::wasm::concepts::operation::tuple_megger<>{}; }
        else
        {
            return []<::std::size_t... I>(::std::index_sequence<I...>) constexpr noexcept
            {
                return ((
                            []<::uwvm2::parser::wasm::concepts::wasm_feature Fc>() constexpr noexcept
                            {
                                if constexpr(has_feature_parameter<Fc>)
                                {
                                    using fc_para = typename Fc::parameter;
                                    return ::uwvm2::parser::wasm::concepts::operation::tuple_megger<fc_para>{};
                                }
                                else
                                {
                                    return ::uwvm2::parser::wasm::concepts::operation::tuple_megger<>{};
                                }
                            }.template operator()<Fs...[I]>()),  // This is an overloaded comma expression
                        ...);
            }(::std::make_index_sequence<sizeof...(Fs)>{});
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using splice_feature_parameter_t = decltype(splice_feature_parameter<Fs...>())::Type;

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto splice_splice_feature_parameter_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    { return splice_feature_parameter<Fs...>(); }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval void check_has_duplicate_feature_parameter() noexcept
    {
        // Check for duplicate Fs, wasm feature cannot be duplicated
        []<::std::size_t... I1>(::std::index_sequence<I1...>) constexpr noexcept
        {
            ((
                 []<::uwvm2::parser::wasm::concepts::wasm_feature F1>() constexpr noexcept
                 {
                     if constexpr(has_feature_parameter<F1>)
                     {
                         bool has{};
                         [&has]<::std::size_t... I2>(::std::index_sequence<I2...>) constexpr noexcept
                         {
                             ((
                                  [&has]<::uwvm2::parser::wasm::concepts::wasm_feature F2>() constexpr noexcept
                                  {
                                      if constexpr(has_feature_parameter<F2>)
                                      {
                                          if constexpr(::std::same_as<typename F1::parameter, typename F2::parameter>)
                                          {
#if __cpp_contracts >= 202502L
                                              contract_assert(!has);
#else
                                              if(has) { ::fast_io::fast_terminate(); }
#endif
                                              has = true;  // itself
                                          }
                                      }
                                  }.template operator()<Fs...[I2]>()),
                              ...);
                         }(::std::make_index_sequence<sizeof...(Fs)>{});
                     }
                 }.template operator()<Fs...[I1]>()),
             ...);
        }(::std::make_index_sequence<sizeof...(Fs)>{});
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval bool check_has_duplicate_feature_parameter_ret_true() noexcept
    {
        check_has_duplicate_feature_parameter<Fs...>();
        return true;
    }

    /// @brief      It must be the same as the previous definition (in root.h), so static_assert is used
    /// @details    feature_parameter_t<Fs...> is used to store those parameters used to control feature parsing, such as whether or not to start the parsing
    ///             function, and is passed as a constant reference.
    /// @see        root.h
    /// @see        test/0001.parser/0001.concept/feature_parameters.cc
    template <typename... Fs>
    struct feature_parameter_t
    {
        using storage_t = splice_feature_parameter_t<Fs...>;  // ::uwvm2::utils::container::tuple
        storage_t parameters{};

        // check is wasm feature
        static_assert((::uwvm2::parser::wasm::concepts::wasm_feature<Fs> && ...), "Fs is not wasm feature");
        // check is tuple
        static_assert(::fast_io::is_tuple<storage_t>, "storage_t is not fast_io::tuple");
        // check no duplicate
        static_assert(check_has_duplicate_feature_parameter_ret_true<Fs...>());
        // check whether or not it is stored in union without side effects
        static_assert(::std::is_trivially_copyable_v<storage_t> && ::std::is_trivially_destructible_v<storage_t>);
    };

    /// @brief get feature parameter type from wasm feature tuple
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval auto get_feature_parameter_type_from_tuple(::uwvm2::utils::container::tuple<Fs...>) noexcept
    {
        check_has_duplicate_feature_parameter<Fs...>();
        return feature_parameter_t<Fs...>{};
    }

    /// @brief Get a reference to a single parameter
    template <::uwvm2::parser::wasm::concepts::wasm_feature Fc, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr auto&& get_curr_feature_parameter(feature_parameter_t<Fs...> & fs_para) noexcept
    {
        check_has_duplicate_feature_parameter<Fs...>();
        static_assert(has_feature_parameter<Fc> && (::std::same_as<Fs, Fc> || ...));  // Check if Fc has feature parameter and if it is one of the Fs
        using fc_para = typename Fc::parameter;
        return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<fc_para>(fs_para.parameters);
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature Fc, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr auto&& get_curr_feature_parameter(feature_parameter_t<Fs...> const& fs_para) noexcept
    {
        check_has_duplicate_feature_parameter<Fs...>();
        static_assert(has_feature_parameter<Fc> && (::std::same_as<Fs, Fc> || ...));  // Check if Fc has feature parameter and if it is one of the Fs
        using fc_para = typename Fc::parameter;
        return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<fc_para>(fs_para.parameters);
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature Fc, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr auto&& get_curr_feature_parameter(feature_parameter_t<Fs...> && fs_para) noexcept
    {
        check_has_duplicate_feature_parameter<Fs...>();
        static_assert(has_feature_parameter<Fc> && (::std::same_as<Fs, Fc> || ...));  // Check if Fc has feature parameter and if it is one of the Fs
        using fc_para = typename Fc::parameter;
        return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<fc_para>(fs_para.parameters);
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature Fc, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr auto&& get_curr_feature_parameter(feature_parameter_t<Fs...> const&& fs_para) noexcept
    {
        check_has_duplicate_feature_parameter<Fs...>();
        static_assert(has_feature_parameter<Fc> && (::std::same_as<Fs, Fc> || ...));  // Check if Fc has feature parameter and if it is one of the Fs
        using fc_para = typename Fc::parameter;
        return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<fc_para>(fs_para.parameters);
    }
}
