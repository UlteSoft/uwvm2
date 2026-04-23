/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2026-04-23
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
# include <limits>
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/utils/utf/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
# include <uwvm2/uwvm/wasm/feature/impl.h>
# include <uwvm2/uwvm/wasm/type/impl.h>
# include <uwvm2/uwvm/wasm/storage/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
#if defined(UWVM_MODULE)
    extern "C++" UWVM_GNU_COLD
#else
    UWVM_GNU_COLD inline constexpr
#endif
        ::uwvm2::utils::cmdline::parameter_return_type wasm_set_memory_limit_callback([[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results *
                                                                                          para_begin,
                                                                                      ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
                                                                                      ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        auto currp1{para_curr + 1u};
        auto currp2{para_curr + 2u};
        auto currp3{para_curr + 3u};

        if(currp1 == para_end || currp2 == para_end || currp3 == para_end || currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg ||
           currp2->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg ||
           currp3->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_set_memory_limit),
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        auto const module_name{::uwvm2::utils::container::u8string_view{currp1->str}};
        auto const target_text{::uwvm2::utils::container::u8string_view{currp2->str}};
        if(module_name.empty()) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Module name cannot be empty. Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_set_memory_limit),
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        // This option creates/updates configuration keyed by a Wasm module name,
        // so validate it here instead of deferring to a later loader path.
        if(auto const [module_name_pos, module_name_err]{
               ::uwvm2::uwvm::wasm::feature::handle_text_format(::uwvm2::uwvm::wasm::feature::wasm_binfmt_ver1_text_format_wapper,
                                                                module_name.cbegin(),
                                                                module_name.cend())};
           module_name_err != ::uwvm2::utils::utf::utf_error_code::success) [[unlikely]]
        {
            static_cast<void>(module_name_pos);
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid module name: \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                module_name,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\". WebAssembly names must be valid UTF-8. Reason: \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                ::uwvm2::utils::utf::get_utf_error_description<char8_t>(module_name_err),
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\". Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_set_memory_limit),
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        bool apply_to_all{};
        ::std::size_t parsed_index{};
        if(target_text == u8"all") { apply_to_all = true; }
        else
        {
            auto const [next, err]{::fast_io::parse_by_scan(currp2->str.cbegin(), currp2->str.cend(), parsed_index)};
            if(err != ::fast_io::parse_code::ok || next != currp2->str.cend()) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                    u8"[error] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Invalid memory selector: \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    currp2->str,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\". Expected `all` or `<index:size_t>`. Usage: ",
                                    ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_set_memory_limit),
                                    u8"\n\n");
                return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
            }
        }

        ::std::size_t parsed_min{};
        if(auto const [next, err]{::fast_io::parse_by_scan(currp3->str.cbegin(), currp3->str.cend(), parsed_min)};
           err != ::fast_io::parse_code::ok || next != currp3->str.cend()) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid minimum memory limit: \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                currp3->str,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\". Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_set_memory_limit),
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        currp1->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
        currp2->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
        currp3->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;

        ::std::size_t parsed_max{};
        bool parsed_max_present{};
        if(auto currp4{para_curr + 4u}; currp4 != para_end && currp4->type == ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg)
        {
            auto const [next, err]{::fast_io::parse_by_scan(currp4->str.cbegin(), currp4->str.cend(), parsed_max)};
            if(err == ::fast_io::parse_code::ok && next == currp4->str.cend())
            {
                currp4->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
                parsed_max_present = true;
            }
        }

        if(parsed_max_present && parsed_max < parsed_min) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid memory limit override: max (",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                parsed_max,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8") is smaller than min (",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                parsed_min,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"). Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_set_memory_limit),
                                u8"\n\n");
            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        auto& limit{::uwvm2::uwvm::wasm::storage::upsert_configured_module_memory_limit(module_name)};
        ::uwvm2::uwvm::wasm::type::module_memory_limit_t parsed_limit{.min = parsed_min,
                                                                      .max = parsed_max_present ? parsed_max
                                                                                                : ::uwvm2::uwvm::wasm::type::module_memory_limit_t::default_max,
                                                                      .present_max = parsed_max_present};

        if(apply_to_all)
        {
            limit.apply_to_all_memories = true;
            limit.all_limits = parsed_limit;
        }
        else
        {
            limit.local_defined_memory_limits.insert_or_assign(parsed_index, parsed_limit);
        }

        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }
}

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
