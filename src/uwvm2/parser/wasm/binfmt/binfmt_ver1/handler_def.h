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
# include <cstddef>
# include <cstdint>
# include <cstring>
# include <climits>
# include <concepts>
# include <type_traits>
# include <utility>
# include <vector>
# include <initializer_list>
# include <algorithm>
# include <memory>
# include <limits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/hash/impl.h>
# include <uwvm2/utils/utf/impl.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/parser/wasm/text_format/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/section/impl.h>
# include <uwvm2/parser/wasm/binfmt/base/impl.h>
# include "section.h"
# include "def.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::binfmt::ver1
{
    // size of char8_t is equal to the size of ::std::byte.

    using wasm_order_t = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte;

    inline constexpr wasm_order_t wasm_order_add_or_overflow_die_chain(wasm_order_t a, wasm_order_t b) noexcept
    {
        constexpr auto wasm_order_max{::std::numeric_limits<wasm_order_t>::max()};
        if(a > wasm_order_max - b) [[unlikely]] { ::fast_io::fast_terminate(); }
        return a + b;
    }

    struct max_section_id_map_sec_id_t
    {
        wasm_order_t max_section_id{};
        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte max_section_id_map_sec_id{};
    };

    template <typename Sec, typename... Fs>
    concept has_handle_binfmt_ver1_extensible_section_define =
        requires(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::std::remove_cvref_t<Sec>> ref,
                 ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                 ::std::byte const* const section_begin,
                 ::std::byte const* const section_end,
                 ::uwvm2::parser::wasm::base::error_impl& err,
                 ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
                 max_section_id_map_sec_id_t& wasm_order,
                 ::std::byte const* const sec_id_module_ptr) {
            {
                handle_binfmt_ver1_extensible_section_define(ref, module_storage, section_begin, section_end, err, fs_para, wasm_order, sec_id_module_ptr)
            } -> ::std::same_as<void>;
        };

    /// @see WebAssembly Release 1.0 (2019-07-20) § 5.5.2
    template <typename Sec>
    concept has_section_id_define =
        requires { requires std::same_as<std::remove_cvref_t<decltype(Sec::section_id)>, ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>; };

    template <typename Sec>
    concept has_section_name_define =
        requires { requires std::same_as<std::remove_cvref_t<decltype(Sec::section_name)>, ::uwvm2::utils::container::u8string_view>; };

    template <typename Sec, typename... Fs>
    concept has_section_id_and_handle_binfmt_ver1_extensible_section_define =
        has_handle_binfmt_ver1_extensible_section_define<Sec, Fs...> && has_section_id_define<Sec>;

    /// @brief      has final_check type
    /// @details
    ///             ```cpp
    ///
    ///             struct basic_final_check
    ///             {
    ///             };
    ///
    ///             struct F
    ///             {
    ///                 template <typename ... Fs>
    ///                 using final_check = type_replacer<root_of_replacement, basic_final_check>;
    ///             };
    ///             ```
    template <typename FeatureType>
    concept has_final_check = requires {
        typename FeatureType::final_check;
        requires ::uwvm2::parser::wasm::concepts::operation::details::check_is_type_replacer<::uwvm2::parser::wasm::concepts::operation::type_replacer,
                                                                                             typename FeatureType::final_check>;
    };

    template <typename FeatureType>
    inline consteval auto get_final_check() noexcept
    {
        if constexpr(has_final_check<FeatureType>) { return typename FeatureType::final_check{}; }
        else
        {
            return ::uwvm2::parser::wasm::concepts::operation::irreplaceable_t{};
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using final_final_check_t = ::uwvm2::parser::wasm::concepts::operation::replacement_structure_t<decltype(get_final_check<Fs>())...>;

    template <typename... Fs>
    concept has_final_check_handler = requires(::uwvm2::parser::wasm::concepts::feature_reserve_type_t<final_final_check_t<Fs...>> final_adl,
                                               ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...>& module_storage,
                                               ::std::byte const* const module_end,
                                               ::uwvm2::parser::wasm::base::error_impl& err,
                                               ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) {
        { define_final_check(final_adl, module_storage, module_end, err, fs_para) } -> ::std::same_as<void>;
    };

    namespace details
    {
        template <typename... Sec>
        inline consteval void check_extensible_section_is_series() noexcept
        {
            // use std vector for consteval environment
            ::std::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte> vec{};
            [&vec]<::std::size_t... I>(::std::index_sequence<I...>) constexpr noexcept
            { ((vec.push_back(Sec...[I] ::section_id)), ...); }(::std::make_index_sequence<sizeof...(Sec)>{});
            ::std::ranges::sort(vec);
            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte counter{0u};
            for(auto i: vec)
            {
#if __cpp_contracts >= 202502L
                contract_assert(counter != ::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>::max());
                contract_assert(i == counter);
                ++counter;
#else
                if(counter == ::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>::max()) { ::fast_io::fast_terminate(); }
                if(i != counter) { ::fast_io::fast_terminate(); }
                ++counter;
#endif
            }
        }

        template <typename... Sec>
        inline consteval ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte generate_section_max_id() noexcept
        {
            static_assert(sizeof...(Sec) != 0);
            // use std vector for consteval environment
            ::std::vector<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte> vec{};
            [&vec]<::std::size_t... I>(::std::index_sequence<I...>) constexpr noexcept
            { ((vec.push_back(Sec...[I] ::section_id)), ...); }(::std::make_index_sequence<sizeof...(Sec)>{});
            ::std::ranges::sort(vec);
            auto const max{vec.back()};
            return max;
        }

        template <typename... Sec>
        inline consteval ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte
            generate_section_max_id_from_tuple(::uwvm2::utils::container::tuple<Sec...> const&) noexcept
        { return generate_section_max_id<Sec...>(); }

        template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
        inline consteval double generate_section_probability_from_Fs() noexcept
        {
#if __cpp_structured_bindings >= 202411L
            // for __builtin_expect_with_probability
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> ret{};
            auto const& [... secs]{ret.sections};
            constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte max_id{generate_section_max_id<::std::remove_cvref_t<decltype(secs)>...>()};
            constexpr double max_id_probability{1.0 - 1.0 / static_cast<double>(max_id + 1)};
            static_assert(0.0 <= max_id_probability && max_id_probability <= 1.0);
            return max_id_probability;
#else
            // for __builtin_expect_with_probability
            ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> ret{};
            constexpr auto get_max_id_from_tuple{[]<typename... Secs> UWVM_ALWAYS_INLINE(::uwvm2::utils::container::tuple<Secs...> const&) constexpr noexcept
                                                 { return generate_section_max_id<Secs...>(); }};
            constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte max_id{get_max_id_from_tuple(ret.sections)};
            constexpr double max_id_probability{1.0 - 1.0 / static_cast<double>(max_id + 1)};
            static_assert(0.0 <= max_id_probability && max_id_probability <= 1.0);
            return max_id_probability;
#endif
        }

        template <typename... Sec>
        inline consteval auto generate_section_name_table() noexcept
        {
            static_assert(sizeof...(Sec) != 0);

            constexpr auto max{generate_section_max_id<Sec...>};
            ::uwvm2::utils::container::array<::uwvm2::utils::container::u8string_view, max + 1u> res{};

            [&res]<::std::size_t... I>(::std::index_sequence<I...>) constexpr noexcept
            { ((res[Sec...[I] ::section_id] = Sec...[I] ::section_name), ...); }(::std::make_index_sequence<sizeof...(Sec)>{});

            return res;
        }
    }  // namespace details

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr double probability_from_Fs{details::generate_section_probability_from_Fs<Fs...>()};

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline consteval void check_extensible_section_is_series_from_Fs() noexcept
    {
#if __cpp_structured_bindings >= 202411L
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> ret{};
        auto const& [... secs]{ret.sections};
        details::check_extensible_section_is_series<::std::remove_cvref_t<decltype(secs)>...>();
#else
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> ret{};
        []<typename... Secs> UWVM_ALWAYS_INLINE(::uwvm2::utils::container::tuple<Secs...> const&) constexpr noexcept -> void
        { details::check_extensible_section_is_series<Secs...>(); }(ret.sections);
#endif
    }

    // Custom Section: compile-time name -> wasm_byte hash table
    namespace details
    {
        struct mapping_entry
        {
            ::uwvm2::utils::container::u8string_view name{};
            wasm_order_t order{};
        };

        // Minimum hash table size exponent (2 ^ hash_size_base)
        inline constexpr ::std::size_t hash_size_base{4uz};
        // Expand when any bucket exceeds this conflict size
        inline constexpr ::std::size_t max_conflict_size{8uz};

        struct calculate_hash_table_size_res
        {
            ::std::size_t hash_table_size{};
            ::std::size_t extra_size{};
            ::std::size_t real_max_conflict_size{};
        };

        inline consteval calculate_hash_table_size_res calculate_hash_table_size(::std::initializer_list<mapping_entry> entries) noexcept
        {
            constexpr auto sizet_d10{static_cast<::std::size_t>(::std::numeric_limits<::std::size_t>::digits10)};

            ::uwvm2::utils::hash::xxh3_64bits_context xxh3{};
            for(auto i{hash_size_base}; i < sizet_d10; ++i)
            {
                ::std::size_t const hash_size{1uz << i};
                bool need_expand{};
                ::std::size_t extra_size{};
                ::std::size_t real_max_conflict_size{};
                ::std::size_t* const hash_size_array{::new ::std::size_t[hash_size]{}};
                for(auto const& j: entries)
                {
                    ::std::size_t const j_str_size{j.name.size()};
                    ::std::byte* const ptr{::new ::std::byte[j_str_size]{}};
                    for(::std::size_t k{}; k < j_str_size; ++k) { ptr[k] = static_cast<::std::byte>(j.name.index_unchecked(k)); }
                    xxh3.reset();
                    xxh3.update(ptr, ptr + j_str_size);
                    ::delete[] ptr;
                    auto const val{xxh3.digest_value() % hash_size};
                    ++hash_size_array[val];
                    if(hash_size_array[val] > real_max_conflict_size) { real_max_conflict_size = hash_size_array[val]; }  // Record maximum conflict size
                    if(hash_size_array[val] == 2uz) { ++extra_size; }                                                     // Initiate additional conflict tables
                    if(hash_size_array[val] > max_conflict_size) { need_expand = true; }  // Maximum allowed conflict value exceeded, expanding hash_table
                }

                ::delete[] hash_size_array;
                if(!need_expand) { return {hash_size, extra_size, real_max_conflict_size}; }
            }

            // The conflict size has not been able to stay within the maximum conflict size, try changing the initial seed.
            // The consteval function reports an error if the memory is not properly freed.
            ::fast_io::fast_terminate();
        }

        struct ht_entry
        {
            ::uwvm2::utils::container::u8string_view name{};
            wasm_order_t order{};  // 0 means empty/not-set
        };

        template <::std::size_t real_max_conflict_size>
        struct conflict_table
        {
            static_assert(real_max_conflict_size <= ::std::numeric_limits<::std::size_t>::max() - 1uz);

            // Add one sentinel slot at the end with order==0 to stop lookup loop
            ::uwvm2::utils::container::array<mapping_entry, real_max_conflict_size + 1uz> ctmem{};
        };

        template <::std::size_t hash_table_size, ::std::size_t conflict_size, ::std::size_t real_max_conflict_size>
        struct hash_table
        {
            ::uwvm2::utils::container::array<ht_entry, hash_table_size> ht{};
            ::std::conditional_t<static_cast<bool>(conflict_size),
                                 ::uwvm2::utils::container::array<conflict_table<real_max_conflict_size>, conflict_size>,
                                 ::std::in_place_t>
                ct{};
        };

        template <::std::size_t hash_table_size, ::std::size_t conflict_size, ::std::size_t real_max_conflict_size>
        inline consteval auto generate_custom_section_sequential_mapping_table(::std::initializer_list<mapping_entry> entries) noexcept
        {
            hash_table<hash_table_size, conflict_size, real_max_conflict_size> res{};

            ::uwvm2::utils::hash::xxh3_64bits_context xxh3{};
            // conflictplace always 1 greater than its offset
            ::std::size_t conflictplace{1uz};

            for(auto const& j: entries)
            {
                ::std::size_t const j_str_size{j.name.size()};
                ::std::byte* const ptr{::new ::std::byte[j_str_size]{}};
                for(::std::size_t k{}; k < j_str_size; ++k) { ptr[k] = static_cast<::std::byte>(j.name.index_unchecked(k)); }
                xxh3.reset();
                xxh3.update(ptr, ptr + j_str_size);
                ::delete[] ptr;
                auto const val{xxh3.digest_value() % hash_table_size};

                if constexpr(conflict_size)
                {
                    if(res.ht.index_unchecked(val).order == 0u)
                    {
                        if(!res.ht.index_unchecked(val).name.empty())
                        {
                            // Write a conflict table backward.
                            for(auto& i: res.ct.index_unchecked(res.ht.index_unchecked(val).name.size() - 1uz).ctmem)
                            {
                                if(i.order == 0u)
                                {
                                    i.name = j.name;
                                    i.order = j.order;
                                    break;
                                }
                            }
                        }
                        else
                        {
                            // Without any conflict, this is the first
                            res.ht.index_unchecked(val).name = j.name;
                            res.ht.index_unchecked(val).order = j.order;
                        }
                    }
                    else
                    {
                        // When a hash conflict occurs, move the original data to the conflict table.
                        res.ct.index_unchecked(conflictplace - 1uz).ctmem.front_unchecked().name = res.ht.index_unchecked(val).name;
                        res.ct.index_unchecked(conflictplace - 1uz).ctmem.front_unchecked().order = res.ht.index_unchecked(val).order;

                        // Marking hash tables as having conflicting patterns
                        res.ht.index_unchecked(val).order = 0;               // Mark this as no available
                        res.ht.index_unchecked(val).name.ptr = nullptr;      // Mark this as no available
                        res.ht.index_unchecked(val).name.n = conflictplace;  // Conflict Table Location

                        // Setting the current content
                        res.ct.index_unchecked(conflictplace - 1uz).ctmem.index_unchecked(1uz).name = j.name;
                        res.ct.index_unchecked(conflictplace - 1uz).ctmem.index_unchecked(1uz).order = j.order;
                        ++conflictplace;
                    }
                }
                else
                {
                    // No conflicts of any kind
                    res.ht.index_unchecked(val).name = j.name;
                    res.ht.index_unchecked(val).order = j.order;
                }
            }
            return res;
        }

        template <::std::size_t hash_table_size, ::std::size_t conflict_size, ::std::size_t real_max_conflict_size>
        inline constexpr wasm_order_t
            find_from_custom_section_sequential_mapping_table(hash_table<hash_table_size, conflict_size, real_max_conflict_size> const& table,
                                                              ::uwvm2::utils::container::u8string_view name) noexcept
        {
            ::uwvm2::utils::hash::xxh3_64bits_context xxh3{};

            // calculate xxh3
            if UWVM_IF_CONSTEVAL
            {
                auto const str_size{name.size()};
                ::std::byte* const ptr{::new ::std::byte[str_size]{}};
                for(::std::size_t k{}; k < str_size; ++k) { ptr[k] = static_cast<::std::byte>(name.index_unchecked(k)); }
                xxh3.update(ptr, ptr + str_size);
                ::delete[] ptr;
            }
            else
            {
                // Because the char8_t size is always equal to ::std::byte
                static_assert(sizeof(char8_t) == sizeof(::std::byte));

                auto const i{reinterpret_cast<::std::byte const*>(name.data())};
                xxh3.update(i, i + name.size());
            }

            auto const val{xxh3.digest_value() % hash_table_size};
            auto const htval{table.ht.index_unchecked(val)};

            if constexpr(conflict_size)
            {
                if(htval.order == 0u)
                {
                    if(!htval.name.empty()) [[likely]]
                    {
                        // Get Conflict Table
                        // htval.name.size() is created at compile time and is always greater than or equal to 1
                        // Note: htval.name.size() is always 1 greater than its offset
                        // index == htval.name.size() - 1uz
                        auto const& ct{table.ct.index_unchecked(htval.name.size() - 1uz).ctmem};

                        // Conflict table is always the last one is order==0, here no longer judge cend, improve speed
                        for(auto curr_conflict{ct.cbegin()};; ++curr_conflict)
                        {
                            if(curr_conflict->name == name) { return curr_conflict->order; }
                            else if(curr_conflict->order == 0u) [[unlikely]] { return static_cast<wasm_order_t>(0u); }
                        }

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif

                        ::std::unreachable();
                    }
                    else [[unlikely]]
                    {
                        return static_cast<wasm_order_t>(0u);
                    }
                }
                else
                {
                    if(name == htval.name) [[likely]] { return htval.order; }
                    else [[unlikely]]
                    {
                        return static_cast<wasm_order_t>(0u);
                    }
                }
            }
            else
            {
                if(name == htval.name) [[likely]] { return htval.order; }
                else [[unlikely]]
                {
                    return static_cast<wasm_order_t>(0u);
                }
            }
        }
    }  // namespace details

    /// @brief      has section sequential packer
    /// @details
    ///             ```cpp
    ///             struct F
    ///             {
    ///                 using section_sequential_packer = type_replacer<root_of_replacement, section_sequential_packer_t>;
    ///             };
    ///             ```
    ///
    /// @note        0 indicates no participation in judgment; subsequent repetition of content indicates that the order priority is the same.
    template <typename FeatureType>
    concept has_section_sequential_packer = requires {
        typename FeatureType::section_sequential_packer;
        requires ::uwvm2::parser::wasm::concepts::operation::details::check_is_type_replacer<::uwvm2::parser::wasm::concepts::operation::type_replacer,
                                                                                             typename FeatureType::section_sequential_packer>;
    };

    template <typename FeatureType>
    inline consteval auto get_section_sequential_packer() noexcept
    {
        if constexpr(has_section_sequential_packer<FeatureType>) { return typename FeatureType::section_sequential_packer{}; }
        else
        {
            return ::uwvm2::parser::wasm::concepts::operation::irreplaceable_t{};
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using final_section_sequential_packer_t =
        ::uwvm2::parser::wasm::concepts::operation::replacement_structure_t<decltype(get_section_sequential_packer<Fs>())...>;

    /// @brief   Sequential mapping table, mapping to 0 is any position
    /// @details The size must be the same as the array size of the entire section.
    template <typename... Fs>
    concept has_section_id_sequential_mapping_table_define = requires {
        requires ::std::same_as<::std::remove_cvref_t<decltype(final_section_sequential_packer_t<Fs...>::section_id_sequential_mapping_table)>,
                                ::uwvm2::utils::container::
                                    array<wasm_order_t, ::std::tuple_size_v<::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...>>>>;
    };

    /// @brief Custom Section: compile-time name -> wasm_byte hash table
    template <typename... Fs>
    concept has_custom_section_sequential_mapping_table_define = requires {
        details::find_from_custom_section_sequential_mapping_table(final_section_sequential_packer_t<Fs...>::custom_section_sequential_mapping_table, {});
    };

    /// @brief Prevent inheritance effects when adl matching
    template <typename FeatureType>
    struct code_version_reserve_type_t
    {
        using feature_type = FeatureType;

        static_assert(::std::is_same_v<::std::remove_cvref_t<FeatureType>, FeatureType>,
                      "code_version_reserve_type_t: typename 'FeatureType' cannot have refer and const attributes");
        explicit constexpr code_version_reserve_type_t() noexcept = default;
    };

    template <typename FeatureType>
    inline constexpr code_version_reserve_type_t<FeatureType> code_version_reserve_type{};

    /// @brief      has final_check type
    /// @details
    ///             ```cpp
    ///
    ///             struct basic_final_check
    ///             {
    ///             };
    ///
    ///             struct F
    ///             {
    ///                 template <typename ... Fs>
    ///                 using final_check = type_replacer<root_of_replacement, basic_final_check>;
    ///             };
    ///             ```
    template <typename FeatureType>
    concept has_code_version_reserve_type = requires {
        typename FeatureType::code_version;
        requires ::uwvm2::parser::wasm::concepts::operation::details::check_is_type_replacer<::uwvm2::parser::wasm::concepts::operation::type_replacer,
                                                                                             typename FeatureType::code_version>;
    };

    template <typename FeatureType>
    inline consteval auto get_code_version_reserve_type() noexcept
    {
        if constexpr(has_code_version_reserve_type<FeatureType>) { return typename FeatureType::code_version{}; }
        else
        {
            return ::uwvm2::parser::wasm::concepts::operation::irreplaceable_t{};
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    using final_code_version_reserve_type_t =
        ::uwvm2::parser::wasm::concepts::operation::replacement_structure_t<decltype(get_code_version_reserve_type<Fs>())...>;
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
