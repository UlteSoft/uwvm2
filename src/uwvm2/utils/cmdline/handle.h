/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-25
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
# include <cstdint>
# include <cstddef>
# include <memory>
# include <new>
# include <algorithm>
# include <limits>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/imported/wasi/feature/feature_push_macro.h>  // wasi
// import
# include <fast_io.h>
# include <fast_io_crypto.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/hash/impl.h>
# include <uwvm2/utils/debug/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::utils::cmdline
{
    /// @brief Used to indicate parameter type
    enum class parameter_parsing_results_type : unsigned
    {
        arg,                  // normal args
        occupied_arg,         // The occupied args are used for preprocessing or running wasm
        parameter,            // Correct parameters
        invalid_parameter,    // Invalid parameter
        duplicate_parameter,  // Duplicate parameter
        dir                   // The type of the first parameter, indicating the current program running path
    };

    struct parameter;

    /// @brief Parsing results of each crtmain parameter
    struct parameter_parsing_results
    {
        ::uwvm2::utils::container::u8cstring_view str{};  // Parameter content
        parameter const* para{};                          // Corresponding parameter. If it is not a parameter, it is nullptr
        parameter_parsing_results_type type{};            // Parameter type
    };

    /// @brief Used to indicate the return type of the parameter parser
    enum class parameter_return_type : unsigned
    {
        def,             // Continue with the program
        return_m1_imme,  // return -1 immediately
        return_imme,     // return 0 immediately
        return_soon,     // return 0 after parsing
        err_imme,        // trap immediately
        err_soon         // trap after parsing
    };

    /// @brief type for consecutive alias
    using kns_u8_str_scatter_t = ::fast_io::basic_io_scatter_t<::uwvm2::utils::container::u8string_view>;

    /// @brief type for handle function
    using handle_func_type =
        parameter_return_type (*)(parameter_parsing_results* para_begin, parameter_parsing_results* para_curr, parameter_parsing_results* para_end) noexcept;

    /// @brief type for pretreatment function
    using parameter_func_type =
        void (*)(char8_t const* const*& argv_curr, char8_t const* const* argv_end, ::uwvm2::utils::container::vector<parameter_parsing_results>& pr) noexcept;

    /// @brief Categorization of parameter types for help displays
    enum class categorization : unsigned
    {
        global = 0u,
        debug,
        wasm,
        log,
#if defined(UWVM_IMPORT_WASI)
        wasi,
#endif
    };

    /// @brief Execution charset EBCDIC not supported
    static_assert((!::fast_io::details::is_ebcdic<char>), "exec-code EBCDIC not supported");

    /// @brief User-defined parameters and handlers
    /// @brief Command line arguments will be encoded in ascii and will not be specialized for encodings such as ebcdic.
    struct parameter
    {
        ::uwvm2::utils::container::u8string_view const name{};      // parameter name
        ::uwvm2::utils::container::u8string_view const describe{};  // describtion shown in help
        ::uwvm2::utils::container::u8string_view const usage{};     // usage shown in help
        kns_u8_str_scatter_t alias{};                               // alias names
        handle_func_type handle{};                                  // formal processing results
        parameter_func_type pretreatment{};                         // pretreatment
        bool* is_exist{};                                           // When it is not nullptr, repeated errors will be reported
        categorization cate{};                                      // Categorization of parameter types for help displays
    };

    /// @brief      sort the parameter const* const [N]
    /// @details    Sort user-defined parameters.
    ///             This function only allows consteval to implement.
    template <::std::size_t N>
    inline consteval auto parameter_sort(parameter const* const(&punsort)[N]) noexcept
    {
        ::uwvm2::utils::container::array<parameter const*, N> res{};
        for(::std::size_t i{}; i < N; ++i) { res.index_unchecked(i) = punsort[i]; }
        ::std::ranges::sort(res, [](parameter const* const a, parameter const* const b) constexpr noexcept -> bool { return a->name < b->name; });
        return res;
    }

    /// @brief This structure is used to store the parameters of all aliases
    struct alias_parameter
    {
        ::uwvm2::utils::container::u8string_view str{};  // raw name or alias name
        parameter const* para{};                         // Equivalent parameter
    };

    /// @brief      Calculate all parameters sizes
    /// @details    Calculate the total number of aliases in the sorting parameter results obtained by parameter_sort
    ///             This function only allows consteval to implement.
    template <::std::size_t N>
    inline consteval ::std::size_t calculate_alias_parameters_size(::uwvm2::utils::container::array<parameter const*, N> const& punsort) noexcept
    {
        ::std::size_t res{};
        for(::std::size_t i{}; i < N; ++i)
        {
            ++res;                                         // raw name size == 1
            res += punsort.index_unchecked(i)->alias.len;  // alias name size == alias.len
        }
        return res;
    }

    /// @brief judge whether parameter has invalid char
    inline constexpr bool is_invalid_parameter_char(char8_t c) noexcept
    {
        if(c >= 0x21 && c <= 0x7E)
        {
            switch(c)
            {
                case u8'/': [[fallthrough]];
                case u8'\\': [[fallthrough]];
                case u8':': [[fallthrough]];
                case u8'*': [[fallthrough]];
                case u8'?': [[fallthrough]];
                case u8'\"': [[fallthrough]];
                case u8'<': [[fallthrough]];
                case u8'>': [[fallthrough]];
                case u8'|': return true;
                default: return false;
            }
        }
        else
        {
            return true;
        }
    }

    /// @brief this function expand all parameter raw name and it alias name and check whether they are valid
    /// @details AllParaSize is the value calculate_alias_parameters_size return
    template <::std::size_t AllParaSize, ::std::size_t N>
    inline consteval auto expand_alias_parameters_and_check(::uwvm2::utils::container::array<parameter const*, N> const& punsort) noexcept
    {
        ::uwvm2::utils::container::array<alias_parameter, AllParaSize> res{};
        ::std::size_t res_pos{};

        // expand all alias patameters
        for(::std::size_t i{}; i < N; ++i)
        {
            res.index_unchecked(res_pos) = {punsort.index_unchecked(i)->name, punsort.index_unchecked(i)};
            ++res_pos;
            for(::std::size_t j{}; j < punsort.index_unchecked(i)->alias.len; ++j)
            {
                res.index_unchecked(res_pos) = {punsort.index_unchecked(i)->alias.base[j], punsort.index_unchecked(i)};
                ++res_pos;
            }
        }

        // sort
        ::std::ranges::sort(res, [](alias_parameter const& a, alias_parameter const& b) constexpr noexcept -> bool { return a.str < b.str; });

        // check is invalid
        ::uwvm2::utils::container::u8string_view check{};  // Empty strings will be sorted and placed first.
        for(auto const& i: res)
        {
#if __cpp_contracts >= 202502L
            contract_assert(i.str != check &&                  // Duplicate parameters are not allowed
                            i.str.front_unchecked == u8'-' &&  // The first character of the parameter must be '-'
                            i.str.size() != 1uz);              // "-" is invalid
#else
            if(i.str == check || i.str.front_unchecked() != u8'-')
            {
                // Duplicate parameters are not allowed
                // The first character of the parameter must be '-'
                ::fast_io::fast_terminate();
            }

            if(i.str.size() == 1uz)
            {
                // "-" is invalid
                ::fast_io::fast_terminate();
            }
#endif
            for(auto c: i.str)
            {
#if __cpp_contracts >= 202502L
                contract_assert(!is_invalid_parameter_char(c));
#else
                if(is_invalid_parameter_char(c))
                {
                    // invalid parameter character
                    ::fast_io::fast_terminate();
                }
#endif
            }

            check = i.str;  // check duplicate
        }
        return res;
    }

    /// @brief      Calculate max parameter principal name size
    template <::std::size_t N>
    inline consteval ::std::size_t calculate_max_principal_name_size(::uwvm2::utils::container::array<parameter const*, N> const& punsort) noexcept
    {
        ::std::size_t max_size{};
        for(::std::size_t i{}; i < N; ++i) { max_size = ::std::max(max_size, punsort.index_unchecked(i)->name.size()); }
        return max_size;
    }

    /// @brief      Calculate max parameter size
    /// @details    Maximum long distance used to provide to the shortest path
    template <::std::size_t N>
    inline consteval ::std::size_t calculate_max_para_size(::uwvm2::utils::container::array<alias_parameter, N> const& punsort) noexcept
    {
        ::std::size_t max_size{};
        for(::std::size_t i{}; i < N; ++i) { max_size = ::std::max(max_size, punsort.index_unchecked(i).str.size()); }
        return max_size;
    }

    /// @brief      Minimum hash table size to start generating
    /// @details    2 ^ hash_size_base
    inline constexpr ::std::size_t hash_size_base{4uz};

    /// @brief      Maximum conflict size above which the hashtable will be expanded.
    inline constexpr ::std::size_t max_conflict_size{8uz};

    struct calculate_hash_table_size_res
    {
        ::std::size_t hash_table_size{};
        ::std::size_t extra_size{};
        ::std::size_t real_max_conflict_size{};
    };

    /// @brief      Calculate the size of the hash_table with the maximum conflict size
    template <::std::size_t N>
    inline consteval calculate_hash_table_size_res calculate_hash_table_size(::uwvm2::utils::container::array<alias_parameter, N> const& ord) noexcept
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
            for(auto const& j: ord)
            {
                ::std::size_t const j_str_size{j.str.size()};
                ::std::byte* const ptr{::new ::std::byte[j_str_size]{}};
                for(::std::size_t k{}; k < j_str_size; ++k) { ptr[k] = static_cast<::std::byte>(j.str.index_unchecked(k)); }
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

    struct ht_para_cpos
    {
        ::uwvm2::utils::container::u8string_view str{};
        parameter const* para{};
    };

    using ct_para_str = alias_parameter;

    template <::std::size_t real_max_conflict_size>
    struct conflict_table
    {
        static_assert(real_max_conflict_size <= ::std::numeric_limits<::std::size_t>::max() - 1uz);

        // Add one, store the last nullptr, to reduce the number of judgments when looking up
        ::uwvm2::utils::container::array<ct_para_str, real_max_conflict_size + 1uz> ctmem{};
    };

    template <::std::size_t hash_table_size, ::std::size_t conflict_size, ::std::size_t real_max_conflict_size>
    struct parameters_hash_table
    {
        static_assert(hash_table_size > 1);
        // Hash Table
        ::uwvm2::utils::container::array<ht_para_cpos, hash_table_size> ht{};
        // Conflict Table
        ::std::conditional_t<static_cast<bool>(conflict_size),
                             ::uwvm2::utils::container::array<conflict_table<real_max_conflict_size>, conflict_size>,
                             ::std::in_place_t>
            ct{};
    };

    /// @brief generate hash table
    template <::std::size_t hash_table_size, ::std::size_t conflict_size, ::std::size_t real_max_conflict_size, ::std::size_t N>
    inline consteval auto generate_hash_table(::uwvm2::utils::container::array<alias_parameter, N> const& ord) noexcept
    {
        parameters_hash_table<hash_table_size, conflict_size, real_max_conflict_size> res{};

        ::uwvm2::utils::hash::xxh3_64bits_context xxh3{};

        // Note: conflictplace is always 1 greater than its offset, because it prevents subsequent lookups that are equal to 0 (the first element) from being
        // judged as having no content, leading to parsing failures.
        ::std::size_t conflictplace{1uz};

        for(auto const& j: ord)
        {
            ::std::size_t const j_str_size{j.str.size()};
            ::std::byte* const ptr{::new ::std::byte[j_str_size]{}};
            for(::std::size_t k{}; k < j_str_size; ++k) { ptr[k] = static_cast<::std::byte>(j.str.index_unchecked(k)); }
            xxh3.reset();
            xxh3.update(ptr, ptr + j_str_size);
            ::delete[] ptr;
            auto const val{xxh3.digest_value() % hash_table_size};
            if constexpr(conflict_size)
            {
                if(res.ht.index_unchecked(val).para == nullptr)
                {
                    if(!res.ht.index_unchecked(val).str.empty())
                    {
                        // Write a conflict table backward.

                        // str.n (str.size()) records the number of conflicts, not the last position of the conflict table,
                        // if you need to get the last position, you need to subtract one.

                        for(auto& i: res.ct.index_unchecked(res.ht.index_unchecked(val).str.size() - 1uz).ctmem)
                        {
                            // Find an opening.
                            if(i.para == nullptr)
                            {
                                i.para = j.para;
                                i.str = j.str;
                                break;
                            }
                        }
                    }
                    else
                    {
                        // Without any conflict, this is the first
                        res.ht.index_unchecked(val).para = j.para;
                        res.ht.index_unchecked(val).str = j.str;
                    }
                }
                else
                {
                    // When a hash conflict occurs, move the original data to the conflict table.
                    res.ct.index_unchecked(conflictplace - 1uz).ctmem.front_unchecked().para = res.ht.index_unchecked(val).para;
                    res.ct.index_unchecked(conflictplace - 1uz).ctmem.front_unchecked().str = res.ht.index_unchecked(val).str;

                    // Marking hash tables as having conflicting patterns
                    res.ht.index_unchecked(val).para = nullptr;         // Mark this as no available
                    res.ht.index_unchecked(val).str.ptr = nullptr;      // Mark this as no available
                    res.ht.index_unchecked(val).str.n = conflictplace;  // Conflict Table Location
                    // Note: conflictplace is always 1 greater than its offset, because it prevents subsequent lookups that are equal to 0 (the first element)
                    // from being judged as having no content, leading to parsing failures.

                    // Setting the current content
                    res.ct.index_unchecked(conflictplace - 1uz).ctmem.index_unchecked(1uz).para = j.para;
                    res.ct.index_unchecked(conflictplace - 1uz).ctmem.index_unchecked(1uz).str = j.str;
                    ++conflictplace;
                }
            }
            else
            {
                // No conflicts of any kind
                res.ht.index_unchecked(val).para = j.para;
                res.ht.index_unchecked(val).str = j.str;
            }
        }
        return res;
    }

    template <::std::size_t hash_table_size, ::std::size_t conflict_size, ::std::size_t real_max_conflict_size>
    inline constexpr parameter const* find_from_hash_table(parameters_hash_table<hash_table_size, conflict_size, real_max_conflict_size> const& ght,
                                                           ::uwvm2::utils::container::u8string_view str) noexcept
    {
        ::uwvm2::utils::hash::xxh3_64bits_context xxh3{};

        // calculate xxh3
        if UWVM_IF_CONSTEVAL
        {
            auto const str_size{str.size()};
            ::std::byte* const ptr{::new ::std::byte[str_size]{}};
            for(::std::size_t k{}; k < str_size; ++k) { ptr[k] = static_cast<::std::byte>(str.index_unchecked(k)); }
            xxh3.update(ptr, ptr + str_size);
            ::delete[] ptr;
        }
        else
        {
            // Because the char8_t size is always equal to ::std::byte
            static_assert(sizeof(char8_t) == sizeof(::std::byte));

            auto const i{reinterpret_cast<::std::byte const*>(str.data())};
            xxh3.update(i, i + str.size());
        }

        // get hash table
        auto const val{xxh3.digest_value() % hash_table_size};
        auto const htval{ght.ht.index_unchecked(val)};

        if constexpr(conflict_size)
        {
            if(htval.para == nullptr)
            {
                if(!htval.str.empty()) [[likely]]
                {
                    // Get Conflict Table
                    // htval.str.size() is created at compile time and is always greater than or equal to 1
                    // Note: htval.str.size() is always 1 greater than its offset
                    // index == htval.str.size() - 1uz
                    auto const& ct{ght.ct.index_unchecked(htval.str.size() - 1uz).ctmem};

                    // Conflict table is always the last one is nullptr, here no longer judge cend, improve speed
                    for(auto curr_conflict{ct.cbegin()};; ++curr_conflict)
                    {
                        if(curr_conflict->str == str) { return curr_conflict->para; }
                        else if(curr_conflict->para == nullptr) [[unlikely]] { return nullptr; }
                    }

                    // If the above scenario is defined, there will never be a hit here.

#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                    ::std::unreachable();
                }
                else [[unlikely]]
                {
                    return nullptr;
                }
            }
            else
            {
                if(str == htval.str) [[likely]] { return htval.para; }
                else [[unlikely]]
                {
                    return nullptr;
                }
            }
        }
        else
        {
            if(str == htval.str) [[likely]] { return htval.para; }
            else [[unlikely]]
            {
                return nullptr;
            }
        }
    }
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/imported/wasi/feature/feature_pop_macro.h>  // wasi
# include <uwvm2/utils/macro/pop_macros.h>
#endif
