/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
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
# include <climits>
# include <type_traits>
# include <concepts>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/utils/madvise/impl.h>
# include <uwvm2/utils/utf/impl.h>
# include <uwvm2/parser/wasm/base/impl.h>
# include <uwvm2/parser/wasm/concepts/impl.h>
# include <uwvm2/parser/wasm/standard/impl.h>
# include <uwvm2/parser/wasm/binfmt/base/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/utils/memory/impl.h>
# include <uwvm2/uwvm/wasm/base/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/custom/impl.h>
# include "warn_storage.h"
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::wasm::warning
{
    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void check_unused_type(::uwvm2::parser::wasm::standard::wasm1::features::wasm1_final_extern_type<Fs...> const& curr_import,  // [adl]
                                            ::uwvm2::uwvm::wasm::type::wasm_file_t const& wasm,
                                            ::uwvm2::uwvm::wasm::warning::binfmt_ver1_warning_storage_t& warn_storage) noexcept
    {
        // get wasm module storage
        auto const& wasm_module_storage{wasm.get_curr_binfmt_version_wasm_storage<1u>()};

        auto const& typesec{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
            ::uwvm2::parser::wasm::standard::wasm1::features::type_section_storage_t<Fs...>>(wasm_module_storage.sections)};
        auto const type_cbegin{typesec.types.cbegin()};

        switch(curr_import.type)
        {
            case ::uwvm2::parser::wasm::standard::wasm1::type::external_types::func:
            {
                auto const type_index{static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_u32>(curr_import.storage.function - type_cbegin)};

                auto& curr_type_unused_checker{
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    warn_storage.unused_type_checker[type_index]
#else
                    warn_storage.unused_type_checker.index_unchecked(type_index)
#endif
                };

                curr_type_unused_checker = true;

                break;
            }
            default:
            {
                break;
            }
        }
    }

    inline constexpr void show_wasm_import_section_warning(::uwvm2::uwvm::wasm::type::wasm_file_t const& wasm,
                                                           ::uwvm2::uwvm::wasm::warning::binfmt_ver1_warning_storage_t& warn_storage) noexcept
    {
        // get wasm module storage
        auto const& wasm_module_storage{wasm.get_curr_binfmt_version_wasm_storage<1u>()};

        // get import section
        constexpr auto get_importsec_from_features_tuple{
            []<::uwvm2::parser::wasm::concepts::wasm_feature... Fs> UWVM_ALWAYS_INLINE(auto const& section,
                                                                                       ::uwvm2::utils::container::tuple<Fs...>) constexpr noexcept
            {
                return ::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    ::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>(section);
            }};

        auto const& import_section{get_importsec_from_features_tuple(wasm_module_storage.sections, ::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features)};

        // check duplicate imports
        constexpr auto get_check_duplicate_imports_from_features_tuple{
            []<::uwvm2::parser::wasm::concepts::wasm_feature... Fs> UWVM_ALWAYS_INLINE(::uwvm2::utils::container::tuple<Fs...>) consteval noexcept -> bool
            { return ::uwvm2::parser::wasm::standard::wasm1::features::check_duplicate_imports<Fs...>(); }};

        constexpr bool check_duplicate_imports{get_check_duplicate_imports_from_features_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features)};

        // If the parser does not check, a warning is issued here.
        ::std::conditional_t<!check_duplicate_imports,
                             ::uwvm2::utils::container::unordered_flat_set<::uwvm2::parser::wasm::standard::wasm1::features::name_checker>,
                             ::uwvm2::parser::wasm::concepts::empty_t>
            duplicate_name_checker{};  // use for check duplicate name

        if constexpr(!check_duplicate_imports) { duplicate_name_checker.reserve(import_section.imports.size()); }

        // check zero length
        constexpr auto get_disable_zero_length_string_from_features_tuple{
            []<::uwvm2::parser::wasm::concepts::wasm_feature... Fs> UWVM_ALWAYS_INLINE(::uwvm2::utils::container::tuple<Fs...>) consteval noexcept -> bool
            { return ::uwvm2::parser::wasm::standard::wasm1::features::disable_zero_length_string<Fs...>(); }};

        constexpr bool disable_zero_length_string{get_disable_zero_length_string_from_features_tuple(::uwvm2::uwvm::wasm::feature::wasm_binfmt1_features)};

        // import type name
        constexpr auto get_duplicate_imports_type_name{
            []<::std::integral char_type2>(::std::uint_least8_t type) constexpr noexcept -> ::uwvm2::utils::container::basic_string_view<char_type2>
            {
                switch(type)
                {
                    case 0u:
                    {
                        if constexpr(::std::same_as<char_type2, char>) { return {"func"}; }
                        else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"func"}; }
                        else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"func"}; }
                        else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"func"}; }
                        else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"func"}; }
                        ::std::unreachable();
                    }
                    case 1u:
                    {
                        if constexpr(::std::same_as<char_type2, char>) { return {"table"}; }
                        else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"table"}; }
                        else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"table"}; }
                        else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"table"}; }
                        else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"table"}; }
                        ::std::unreachable();
                    }
                    case 2u:
                    {
                        if constexpr(::std::same_as<char_type2, char>) { return {"mem"}; }
                        else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"mem"}; }
                        else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"mem"}; }
                        else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"mem"}; }
                        else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"mem"}; }
                        ::std::unreachable();
                    }
                    case 3u:
                    {
                        if constexpr(::std::same_as<char_type2, char>) { return {"global"}; }
                        else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"global"}; }
                        else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"global"}; }
                        else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"global"}; }
                        else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"global"}; }
                        ::std::unreachable();
                    }
                    case 4u:
                    {
                        if constexpr(::std::same_as<char_type2, char>) { return {"tag"}; }
                        else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"tag"}; }
                        else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"tag"}; }
                        else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"tag"}; }
                        else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"tag"}; }
                        ::std::unreachable();
                    }
                    [[unlikely]] default:
                    {
/// @warning Maybe I forgot to realize it.
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                        ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif

                        if constexpr(::std::same_as<char_type2, char>) { return {"unknown"}; }
                        else if constexpr(::std::same_as<char_type2, wchar_t>) { return {L"unknown"}; }
                        else if constexpr(::std::same_as<char_type2, char8_t>) { return {u8"unknown"}; }
                        else if constexpr(::std::same_as<char_type2, char16_t>) { return {u"unknown"}; }
                        else if constexpr(::std::same_as<char_type2, char32_t>) { return {U"unknown"}; }
                    }
                }
            }};

        for(auto const& curr_import: import_section.imports)
        {
            check_unused_type(curr_import.imports, wasm, warn_storage);

            // check duplicate imports
            if constexpr(!check_duplicate_imports)
            {
                // No runtime control is required here.
                // Runtime control can directly shut down errors and not output warnings.
                if(!duplicate_name_checker.emplace(curr_import.module_name, curr_import.extern_name).second) [[unlikely]]
                {
                    ::fast_io::io::perr(
                        ::uwvm2::uwvm::io::u8log_output,
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                        u8"uwvm: ",
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                        u8"[warn]  ",
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                        u8"Duplicate imports of the same import type (",
                        get_duplicate_imports_type_name.template operator()<char8_t>(static_cast<::std::uint_least8_t>(curr_import.imports.type)),
                        u8"): \"",
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                        curr_import.module_name,
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                        u8".",
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                        curr_import.extern_name,
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                        u8"\". ",
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                        u8"(parser)\n",
                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

                    if(::uwvm2::uwvm::io::parser_warning_fatal) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"Convert warnings to fatal errors. ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                            u8"(parser)\n\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }
                }
            }

            // check zero length
            if constexpr(!disable_zero_length_string)
            {
                if(curr_import.module_name.empty()) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        u8"[warn]  ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"It is not recommended that the length of Imported Module Name is 0. ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(parser)\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

                    if(::uwvm2::uwvm::io::parser_warning_fatal) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"Convert warnings to fatal errors. ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                            u8"(parser)\n\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }
                }

                if(curr_import.extern_name.empty()) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                        u8"[warn]  ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"It is not recommended that the length of Imported Extern Name is 0. ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(parser)\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

                    if(::uwvm2::uwvm::io::parser_warning_fatal) [[unlikely]]
                    {
                        ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                            u8"uwvm: ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                            u8"[fatal] ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                            u8"Convert warnings to fatal errors. ",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                            u8"(parser)\n\n",
                                            ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                        ::fast_io::fast_terminate();
                    }
                }
            }

            // check utf8
            // Checks opposite to those of the parser:
            // null -> utf8+zero
            // zero -> utf8
            // utf8 -> zero
            // utf8+zero -> null

            auto const [module_utf8pos, module_utf8err]{
                ::uwvm2::uwvm::wasm::feature::warn_unchecked_text_format_error(::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_text_format_wapper,
                                                                               curr_import.module_name.cbegin(),
                                                                               curr_import.module_name.cend())};

            if(module_utf8err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
            {
                // default print_memory
                ::uwvm2::uwvm::utils::memory::print_memory const memory_printer{reinterpret_cast<::std::byte const*>(curr_import.module_name.cbegin()),
                                                                                reinterpret_cast<::std::byte const*>(module_utf8pos),
                                                                                reinterpret_cast<::std::byte const*>(curr_import.module_name.cend())};

                // Output the main information and memory indication
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    // 1
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    u8"[warn]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"(offset=",
                                    ::fast_io::mnp::addrvw(module_utf8pos - curr_import.module_name.cbegin()),
                                    u8") Imported Module Name contains characters that are not recommended. Details: \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    ::uwvm2::utils::utf::get_utf_error_description<char8_t>(module_utf8err),
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\". ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8"(parser)\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                    u8"\n"
                                    // 2
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                    u8"[info]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Parser Memory Indication: ",
                                    memory_printer,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                    u8"\n");

                if(::uwvm2::uwvm::io::parser_warning_fatal) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Convert warnings to fatal errors. ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(parser)\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
            }

            auto const [extern_utf8pos, extern_utf8err]{
                ::uwvm2::uwvm::wasm::feature::warn_unchecked_text_format_error(::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_text_format_wapper,
                                                                               curr_import.extern_name.cbegin(),
                                                                               curr_import.extern_name.cend())};

            if(extern_utf8err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
            {
                // default print_memory
                ::uwvm2::uwvm::utils::memory::print_memory const memory_printer{reinterpret_cast<::std::byte const*>(curr_import.extern_name.cbegin()),
                                                                                reinterpret_cast<::std::byte const*>(extern_utf8pos),
                                                                                reinterpret_cast<::std::byte const*>(curr_import.extern_name.cend())};

                // Output the main information and memory indication
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    // 1
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    u8"[warn]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"(offset=",
                                    ::fast_io::mnp::addrvw(extern_utf8pos - curr_import.extern_name.cbegin()),
                                    u8") Imported Extern Name contains characters that are not recommended. Details: \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                    ::uwvm2::utils::utf::get_utf_error_description<char8_t>(extern_utf8err),
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\". ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                    u8"(parser)\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                    u8"\n"
                                    // 2
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                    u8"[info]  ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Parser Memory Indication: ",
                                    memory_printer,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                    u8"\n");

                if(::uwvm2::uwvm::io::parser_warning_fatal) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_RED),
                                        u8"[fatal] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Convert warnings to fatal errors. ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_ORANGE),
                                        u8"(parser)\n\n",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));
                    ::fast_io::fast_terminate();
                }
            }
        }
    }

    template <::uwvm2::parser::wasm::concepts::wasm_feature... Fs>
    inline constexpr void show_wasm_section_warning(
        ::uwvm2::parser::wasm::concepts::feature_reserve_type_t<::uwvm2::parser::wasm::standard::wasm1::features::import_section_storage_t<Fs...>>,
        ::uwvm2::uwvm::wasm::type::wasm_file_t const& wasm,
        ::uwvm2::uwvm::wasm::warning::binfmt_ver1_warning_storage_t& warn_storage) noexcept
    { return show_wasm_import_section_warning(wasm, warn_storage); }
}

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
