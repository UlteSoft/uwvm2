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
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/type/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/section/impl.h>
# include <uwvm2/parser/wasm/standard/wasm1/opcode/impl.h>
# include <uwvm2/parser/wasm/binfmt/binfmt_ver1/impl.h>
# include "def.h"
# include "feature_def.h"
# include "types.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1::features
{
    struct start_section_storage_t UWVM_TRIVIALLY_RELOCATABLE_IF_ELIGIBLE
    {
        inline static constexpr ::uwvm2::utils::container::u8string_view section_name{u8"Start"};
        inline static constexpr ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte section_id{
            static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(::uwvm2::parser::wasm::standard::wasm1::section::section_id::start_sec)};

        ::uwvm2::parser::wasm::standard::wasm1::section::section_span_view sec_span{};

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 start_idx{};
    };

    /// @brief Define the handler function for start_section
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void handle_binfmt_ver1_extensible_section_define(
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<start_section_storage_t> sec_adl,
        ::uwvm2::parser::wasm::binfmt::ver1::wasm_binfmt_ver1_module_extensible_storage_t<Fs...> & module_storage,
        ::std::byte const* const section_begin,
        ::std::byte const* const section_end,
        ::uwvm2::parser::wasm::base::error_impl& err,
        [[maybe_unused]] ::uwvm2::parser::wasm::concepts::feature_parameter_t<Fs...> const& fs_para,
        [[maybe_unused]] ::uwvm2::parser::wasm::binfmt::ver1::max_section_id_map_sec_id_t& wasm_order,
        ::std::byte const* const sec_id_module_ptr) UWVM_THROWS
    {
#ifdef UWVM_TIMER
        ::uwvm2::utils::debug::timer parsing_timer{u8"parse start section (id: 8)"};
#endif
        // Note that section_begin may be equal to section_end
        // No explicit checking required because ::fast_io::parse_by_scan self-checking (::fast_io::parse_code::end_of_file)

        // get start_section_storage_t from storages
        auto& startsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<start_section_storage_t>(module_storage.sections)};

        // check duplicate
        if(startsec.sec_span.sec_begin) [[unlikely]]
        {
            err.err_curr = sec_id_module_ptr;
            err.err_selectable.u8 = startsec.section_id;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::duplicate_section;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        using wasm_byte_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte const*;

        startsec.sec_span.sec_begin = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_begin);
        startsec.sec_span.sec_end = reinterpret_cast<wasm_byte_const_may_alias_ptr>(section_end);

        auto section_curr{section_begin};

        // [before_section ... ] | start_idx ...
        // [        safe       ] | unsafe (could be the section_end)
        //                         ^^ section_curr

        ::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32 start_idx;  // No initialization necessary

        using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

        auto const [start_idx_next, start_idx_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(section_curr),
                                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(section_end),
                                                                            ::fast_io::mnp::leb128_get(start_idx))};

        if(start_idx_err != ::fast_io::parse_code::ok) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::invalid_start_idx;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(start_idx_err);
        }

        // [before_section ... ] | start_idx ...] (end)
        // [        safe                        ] unsafe (could be the section_end)
        //                         ^^ section_curr

        // check
        // import section
        auto const& importsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<import_section_storage_t<Fs...>>(module_storage.sections)};
        // importdesc[0]: func
        constexpr ::std::size_t importdesc_count{importsec.importdesc_count};
        static_assert(importdesc_count > 0uz);  // Ensure that subsequent index visits do not cross boundaries
        auto const& imported_func{importsec.importdesc.index_unchecked(0uz)};
        auto const imported_func_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(imported_func.size())};

        // function section
        auto const& funcsec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<function_section_storage_t>(module_storage.sections)};
        auto const defined_funcsec_funcs_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(funcsec.funcs.size())};
        // Addition does not overflow, Dependency Pre-Fill Pre-Check.
        auto const all_func_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(defined_funcsec_funcs_size + imported_func_size)};

        if(start_idx >= all_func_size) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_selectable.start_index_exceeds_maxvul.idx = start_idx;
            err.err_selectable.start_index_exceeds_maxvul.maxval = all_func_size;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::start_index_exceeds_maxvul;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }

        // Storing Temporary Variables into Modules
        startsec.start_idx = start_idx;

        section_curr = reinterpret_cast<::std::byte const*>(start_idx_next);

        // [before_section ... ] | start_idx ...] (end)
        // [        safe                        ] unsafe (could be the section_end)
        //                                        ^^ section_curr

        // Check if the signature of this function is "() -> ()"

        if(start_idx >= imported_func_size)
        {
            auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<type_section_storage_t<Fs...>>(module_storage.sections)};

            auto const defined_func_index{start_idx - imported_func_size};
            auto const func_type_idx{funcsec.funcs.index_unchecked(static_cast<::std::size_t>(defined_func_index))};

            auto const& curr_type{typesec.types.index_unchecked(static_cast<::std::size_t>(func_type_idx))};

            auto const func_para_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(curr_type.parameter.end - curr_type.parameter.begin)};

            auto const func_res_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(curr_type.result.end - curr_type.result.begin)};

            if(func_para_size != 0u || func_res_size != 0u) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = start_idx;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_ref_by_start_has_illegal_sign;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }
        else
        {
            auto const& curr_type{*(imported_func.index_unchecked(static_cast<::std::size_t>(start_idx))->imports.storage.function)};

            auto const func_para_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(curr_type.parameter.end - curr_type.parameter.begin)};

            auto const func_res_size{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(curr_type.result.end - curr_type.result.begin)};

            if(func_para_size != 0u || func_res_size != 0u) [[unlikely]]
            {
                err.err_curr = section_curr;
                err.err_selectable.u32 = start_idx;
                err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::func_ref_by_start_has_illegal_sign;
                ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
            }
        }

        // [before_section ... ] | start_idx ...] (end)
        // [        safe                        ] unsafe (could be the section_end)
        //                                        ^^ section_curr

        // Fixed count loop (here it is once), need to determine whether there are extra bytes at the end.

        if(section_curr != section_end) [[unlikely]]
        {
            err.err_curr = section_curr;
            err.err_code = ::uwvm2::parser::wasm::base::wasm_parse_error_code::unexpected_section_data;
            ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::invalid);
        }
    }

    /// @brief Wrapper for the start section storage structure
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    struct start_section_storage_section_details_wrapper_t
    {
        start_section_storage_t const* start_section_storage_ptr{};
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const* all_sections_ptr{};
    };

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr start_section_storage_section_details_wrapper_t<Fs...> section_details(
        start_section_storage_t const& start_section_storage,
        ::uwvm2::parser::wasm::binfmt::ver1::splice_section_storage_structure_t<Fs...> const& all_sections) noexcept
    { return {::std::addressof(start_section_storage), ::std::addressof(all_sections)}; }

    /// @brief Print the start section details
    /// @throws maybe throw fast_io::error, see the implementation of the stream
    template <::std::integral char_type, typename Stm, ::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void print_define(::fast_io::io_reserve_type_t<char_type, start_section_storage_section_details_wrapper_t<Fs...>>,
                                       Stm && stream,
                                       start_section_storage_section_details_wrapper_t<Fs...> const start_section_details_wrapper)
    {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
        if(start_section_details_wrapper.start_section_storage_ptr == nullptr || start_section_details_wrapper.all_sections_ptr == nullptr) [[unlikely]]
        {
            ::uwvm2::utils::debug::trap_and_inform_bug_pos();
        }
#endif

        auto const start_section_span{start_section_details_wrapper.start_section_storage_ptr->sec_span};
        auto const start_section_size{static_cast<::std::size_t>(start_section_span.sec_end - start_section_span.sec_begin)};

        if(start_section_size)
        {
            if constexpr(::std::same_as<char_type, char>) { ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "\nStart]:\n"); }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"\nStart:\n");
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"\nStart:\n");
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"\nStart:\n");
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"\nStart:\n");
            }

            auto const start_idx{start_section_details_wrapper.start_section_storage_ptr->start_idx};

            if constexpr(::std::same_as<char_type, char>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), "func[", start_idx, "]\n");
            }
            else if constexpr(::std::same_as<char_type, wchar_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), L"func[", start_idx, L"]\n");
            }
            else if constexpr(::std::same_as<char_type, char8_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u8"func[", start_idx, u8"]\n");
            }
            else if constexpr(::std::same_as<char_type, char16_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), u"func[", start_idx, u"]\n");
            }
            else if constexpr(::std::same_as<char_type, char32_t>)
            {
                ::fast_io::operations::print_freestanding<false>(::std::forward<Stm>(stream), U"func[", start_idx, U"]\n");
            }
        }
    }
}

/// @brief Define container optimization operations for use with fast_io
UWVM_MODULE_EXPORT namespace fast_io::freestanding
{
    template <>
    struct is_trivially_copyable_or_relocatable<::uwvm2::parser::wasm::standard::wasm1::features::start_section_storage_t>
    {
        inline static constexpr bool value = true;
    };

    template <>
    struct is_zero_default_constructible<::uwvm2::parser::wasm::standard::wasm1::features::start_section_storage_t>
    {
        inline static constexpr bool value = true;
    };
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/utils/macro/pop_macros.h>
#endif
