/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-03-08
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
# include <limits>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
    namespace preload_module_attribute_details
    {
#if defined(UWVM_SUPPORT_MMAP)
        inline constexpr ::uwvm2::utils::container::u8string_view supported_memory_access_modes_text{u8"none, copy, mmap"};
#else
        inline constexpr ::uwvm2::utils::container::u8string_view supported_memory_access_modes_text{u8"none, copy"};
#endif

        [[nodiscard]] inline constexpr bool parse_memory_access_mode(
            ::uwvm2::utils::container::u8string_view text,
            ::uwvm2::uwvm::wasm::type::preload_module_memory_access_mode_t& out) noexcept
        {
            using mode = ::uwvm2::uwvm::wasm::type::preload_module_memory_access_mode_t;

            if(text == u8"none")
            {
                out = mode::none;
                return true;
            }
            if(text == u8"copy")
            {
                out = mode::copy;
                return true;
            }
#if defined(UWVM_SUPPORT_MMAP)
            if(text == u8"mmap")
            {
                out = mode::mmap;
                return true;
            }
#endif

            return false;
        }

        [[nodiscard]] inline constexpr bool parse_memory_index_list(
            ::uwvm2::utils::container::u8string_view text,
            ::uwvm2::utils::container::unordered_flat_set<::std::size_t>& out) noexcept
        {
            out.clear();
            if(text == u8"all") { return true; }
            if(text.empty()) { return false; }

            auto curr{text.cbegin()};
            auto const last{text.cend()};

            while(curr != last)
            {
                auto const token_begin{curr};
                while(curr != last && *curr != u8',') { ++curr; }

                if(token_begin == curr) { return false; }

                ::std::size_t index{};
                auto const [next, err]{::fast_io::parse_by_scan(token_begin, curr, index)};
                if(err != ::fast_io::parse_code::ok || next != curr) { return false; }

                out.emplace(index);

                if(curr == last) { break; }
                ++curr;
                if(curr == last) { return false; }
            }

            return true;
        }
    }

#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        ::uwvm2::utils::cmdline::parameter_return_type wasm_set_preload_module_attribute_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        auto currp1{para_curr + 1u};
        auto currp2{para_curr + 2u};

        if(currp1 == para_end || currp2 == para_end || currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg ||
           currp2->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_set_preload_module_attribute),
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        currp1->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
        currp2->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;

        auto const module_name{::uwvm2::utils::container::u8string_view{currp1->str}};
        auto const memory_access_text{::uwvm2::utils::container::u8string_view{currp2->str}};

        if(module_name.empty()) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Module name cannot be empty. Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_set_preload_module_attribute),
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        ::uwvm2::uwvm::wasm::type::preload_module_memory_access_mode_t parsed_mode{};
        if(!preload_module_attribute_details::parse_memory_access_mode(memory_access_text, parsed_mode)) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid preload memory access mode: \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                memory_access_text,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\". Expected one of: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                preload_module_attribute_details::supported_memory_access_modes_text,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8". Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_set_preload_module_attribute),
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        auto& attribute{::uwvm2::uwvm::wasm::storage::upsert_configured_preload_module_attribute(module_name)};
        attribute.memory_access_mode = parsed_mode;
        attribute.apply_to_all_memories = true;
        attribute.memory_index_set.clear();

        if(auto currp3{para_curr + 3u}; !(currp3 == para_end || currp3->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg)) [[unlikely]]
        {
            currp3->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;

            auto const memory_list_text{::uwvm2::utils::container::u8string_view{currp3->str}};
            if(memory_list_text != u8"all")
            {
                attribute.apply_to_all_memories = false;
                if(!preload_module_attribute_details::parse_memory_index_list(memory_list_text, attribute.memory_index_set)) [[unlikely]]
                {
                    ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                        u8"uwvm: ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                        u8"[error] ",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"Invalid memory list: \"",
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                        memory_list_text,
                                        ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                        u8"\". Expected \"all\" or a comma-separated index list such as \"0,1,2\". Usage: ",
                                        ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_set_preload_module_attribute),
                                        u8"\n\n");
                    return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
                }
            }
        }

        ::uwvm2::uwvm::wasm::storage::apply_preload_module_memory_attribute_to_loaded_modules(module_name, attribute);

        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }
}

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
