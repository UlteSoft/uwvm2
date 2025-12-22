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
 * @date        2025-07-03
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
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/utils/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/section/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/opcode/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include "def.h"
# include "feature_def.h"
# include "parser_limit.h"
# include "types.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct memory_section_storage_t UWVM_TRIVIALLY_RELOCATABLE_IF_ELIGIBLE
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Memory"};
        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::section::section_id::memory_sec)};

        ::uwvm2::parser::wasm::standard::wasm1::section::section_span_view sec_span{};

        ::uwvm2::utils::container::vector<::uwvm2::parser::wasm::standard::wasm1::features::final_memory_type<Fs...>> memories{};
    };

    /// @brief define handler for ::uwvm2::parser::wasm::standard::wasm1::type::memory_type
    /// @note  ADL for distribution to the correct handler function
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr ::std::byte const* memory_section_memory_handler(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<memory_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::standard::wasm1::type::memory_type & memory_r,  // [adl] can be replaced
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* section_curr,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para) UWVM_THROWS
    {
        // [... memory_curr] ...
        // [   safe        ] unsafe (could be the section_end)
        //      ^^ section_curr

        // Handling of scan_memory_type is completely memory-safe

        return ::uwvm2::parser::wasm::standard::wasm1::features::scan_memory_type(memory_r, section_curr, section_end, err);
    }

    /// @brief Define the handler function for memory_section
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void handle_binfmt_ver1_extensible_section_define(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<memory_section_storage_t<Fs...>> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* const section_begin,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
        ::std::byte const* const sec_id_module_ptr) UWVM_THROWS
    {
#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer parsing_timer{u8"parse memory section (id: 5)"};
#endif
        // Note that section_begin may be equal to section_end
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // get memory_section_storage_t from storages
        auto& memorysec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<memory_section_storage_t<Fs...>>(module_storage.sections)};

        // import section
        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(module_storage.sections)};
        // importdesc[2]: memory
        constexpr ::std::size_t importdesc_count{importsec.importdesc_count};
        static_assert(importdesc_count > 2uz);  // Ensure that subsequent index visits do not cross boundaries
        auto const imported_memory_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(2uz).size())};

        // check duplicate
        if(memorysec.sec_span.sec_begin) [[unlikely]]
        {
            err.err_curr = sec_id_module_ptr;
            err.err_selectable.u8 = memorysec.section_id;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::duplicate_section;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        using wasm_byte_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const*;

        memorysec.sec_span.sec_begin = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_begin);
        memorysec.sec_span.sec_end = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_end);

        auto section_curr{section_begin};

        // [before_section ... ] | memory_count ... memory1 ...
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 memory_count;  // No initialization necessary

        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        auto const [memory_count_next, memory_count_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                                  reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                                  ::fast_io::mnp::leb128_get(memory_count))};

        if(memory_count_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_memory_count;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(memory_count_err);
        }

        // [before_section ... | memory_count ...] memory1 ...
        // [             safe                    ] unsafe (could be the section_end)
        //                       ^^ section_curr

        // The size_t of some platforms is smaller than u32, in these platforms you need to do a size check before conversion
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};
        if constexpr(size_t_max < wasm_u32_max)
        {
            // The size_t of current platforms is smaller than u32, in these platforms you need to do a size check before conversion
            if(memory_count > size_t_max) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u64 = static_cast<::std::uint_least64_t>(memory_count);
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::size_exceeds_the_maximum_value_of_size_t;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        constexpr bool allow_multi_memory{::uwvm2::parser::wasm::standard::wasm1::features::allow_multi_memory<Fs...>()};
        if constexpr(!allow_multi_memory)
        {
            /// @details    In the current version of WebAssembly, at most one memory may be defined or imported in a single module,
            ///             and all constructs implicitly reference this memory 0. This restriction may be lifted in future versions
            /// @see        WebAssembly Release 1.0 (2019-07-20) § 2.5.5

            // Since it has already been checked and an exception thrown in the import_section, it can never be greater than 1.
            UWVM_ASSERT(imported_memory_size <= 1u);
            [[assume(imported_memory_size <= 1u)]];

            // memory_count is not incremental and requires overflow checking
            constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};

            if(memory_count > wasm_u32_max - imported_memory_size ||
               memory_count + imported_memory_size > static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(1u)) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::wasm1_not_allow_multi_memory;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
        else
        {
            constexpr auto wasm_u32_max{::std::numeric_limits<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>::max()};

            if(memory_count > wasm_u32_max - imported_memory_size) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.imp_def_num_exceed_u32max.type = 0x02;  // memory
                err.err_selectable.imp_def_num_exceed_u32max.defined = memory_count;
                err.err_selectable.imp_def_num_exceed_u32max.imported = imported_memory_size;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::imp_def_num_exceed_u32max;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        if constexpr((::std::same_as<wasm1, Fs> || ...))
        {
            auto const& wasm1_feapara_r{::uwvm2::parser::wasm::concepts::get_curr_feature_parameter<wasm1>(fs_para)};
            auto const& parser_limit{wasm1_feapara_r.parser_limit};
            if(static_cast<::std::size_t>(memory_count) > parser_limit.max_memory_sec_memories) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.exceed_the_max_parser_limit.name = u8"memorysec_memories";
                err.err_selectable.exceed_the_max_parser_limit.value = static_cast<::std::size_t>(memory_count);
                err.err_selectable.exceed_the_max_parser_limit.maxval = parser_limit.max_memory_sec_memories;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::exceed_the_max_parser_limit;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        memorysec.memories.reserve(static_cast<::std::size_t>(memory_count));

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 memory_counter{};  // use for check

        section_curr = reinterpret_cast<::std::byte const*>(memory_count_next);  // never out of bounds
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // [before_section ... | memory_count ...] memory1 ...
        // [              safe                   ] unsafe (could be the section_end)
        //                                         ^^ section_curr

        while(section_curr != section_end) [[likely]]
        {
            // Ensuring the existence of valid information

            // [... memory_curr] ...
            // [   safe        ] unsafe (could be the section_end)
            //      ^^ section_curr

            // check memory counter
            // Ensure content is available before counting (section_curr != section_end)
            if(::uwvm2::parser::wasm::utils::counter_selfinc_throw_when_overflow(memory_counter, section_curr, err) > memory_count) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = memory_count;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::memory_section_resolved_exceeded_the_actual_number;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }

            // storage memory (need move)
            ::uwvm2::parser::wasm::standard::wasm1::features::final_memory_type<Fs...> memory{};

            // [... memory_curr] ...
            // [   safe        ] unsafe (could be the section_end)
            //      ^^ section_curr

            // Function call matching via adl
            static_assert(::uwvm2::parser::wasm::standard::wasm1::features::has_memory_section_memory_handler<Fs...>);

            section_curr = memory_section_memory_handler(sec_adl, memory, module_storage, section_curr, section_end, err, fs_para);

            // [... memory_curr ...] memory_next
            // [   safe            ] unsafe (could be the section_end)
            //                       ^^ section_curr

            memorysec.memories.push_back_unchecked(::std::move(memory));
        }

        // [... ] (section_end)
        // [safe] unsafe (could be the section_end)
        //        ^^ section_curr

        // check memory counter match
        if(memory_counter != memory_count) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.u32arr[0] = memory_counter;
            err.err_selectable.u32arr[1] = memory_count;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::memory_section_resolved_not_match_the_actual_number;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    /// @brief Wrapper for the memory section storage structure
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct memory_section_storage_section_details_wrapper_t
    {
        memory_section_storage_t<Fs...> const* memory_section_storage_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr memory_section_storage_section_details_wrapper_t<Fs...> section_details(
        memory_section_storage_t<Fs...> const& memory_section_storage,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    { return {::std::addressof(memory_section_storage), ::std::addressof(all_sections)}; }

    /// @brief Print the memory section details
    /// @throws maybe throw fast_io::error, see the implementation of the stream
    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, memory_section_storage_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       memory_section_storage_section_details_wrapper_t<Fs...> const memory_section_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(memory_section_details_wrapper.memory_section_storage_ptr == nullptr || memory_section_details_wrapper.all_sections_ptr == nullptr) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif
        auto const memory_section_span{memory_section_details_wrapper.memory_section_storage_ptr->sec_span};
        auto const memory_section_size{static_cast<::std::size_t>(memory_section_span.sec_end - memory_section_span.sec_begin)};

        if(memory_section_size)
        {
            auto const memory_size{memory_section_details_wrapper.memory_section_storage_ptr->memories.size()};

            if constexpr(::std::same_as<char_type, char>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "\nMemory[", memory_size, "]:\n");
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"\nMemory[", memory_size, L"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"\nMemory[", memory_size, u8"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"\nMemory[", memory_size, u"]:\n");
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"\nMemory[", memory_size, U"]:\n");
            }

            auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(
                *memory_section_details_wrapper.all_sections_ptr)};
            static_assert(importsec.importdesc_count > 2uz);
            auto memory_counter{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(importsec.importdesc.index_unchecked(2uz).size())};

            ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 localdef_counter{};

            for(auto const& curr_memory: memory_section_details_wrapper.memory_section_storage_ptr->memories)
            {
                if constexpr(::std::same_as<char_type, char>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     " - localmemory[",
                                                                     localdef_counter,
                                                                     "] -> memory[",
                                                                     memory_counter,
                                                                     "]: {",
                                                                     section_details(curr_memory),
                                                                     "}\n");
                }
                else if constexpr(::std::same_as<char_type, wchar_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     L" - localmemory[",
                                                                     localdef_counter,
                                                                     L"] -> memory[",
                                                                     memory_counter,
                                                                     L"]: {",
                                                                     section_details(curr_memory),
                                                                     L"}\n");
                }
                else if constexpr(::std::same_as<char_type, char8_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u8" - localmemory[",
                                                                     localdef_counter,
                                                                     u8"] -> memory[",
                                                                     memory_counter,
                                                                     u8"]: {",
                                                                     section_details(curr_memory),
                                                                     u8"}\n");
                }
                else if constexpr(::std::same_as<char_type, char16_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     u" - localmemory[",
                                                                     localdef_counter,
                                                                     u"] -> memory[",
                                                                     memory_counter,
                                                                     u"]: {",
                                                                     section_details(curr_memory),
                                                                     u"}\n");
                }
                else if constexpr(::std::same_as<char_type, char32_t>)
                {
                    ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream),
                                                                     U" - localmemory[",
                                                                     localdef_counter,
                                                                     U"] -> memory[",
                                                                     memory_counter,
                                                                     U"]: {",
                                                                     section_details(curr_memory),
                                                                     U"}\n");
                }

                ++memory_counter;
                ++localdef_counter;
            }
        }
    }
}

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::memory_section_storage_t<Fs...>>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
