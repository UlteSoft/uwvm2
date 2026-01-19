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
# include <cstring>
# include <cstdlib>
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
# include <uwvm2/uwvm/runtime/initializer/init_limit.h>
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
        ::uwvm2::utils::cmdline::parameter_return_type wasm_set_initializer_limit_callback(
            [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
            ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        auto currp{para_curr + 1u};

        if(currp == para_end || currp->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_set_initializer_limit),
                                u8"\n\n");

            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        currp->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
        auto const set_type_name{currp->str};

        ++currp;
        if(currp == para_end || currp->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_set_initializer_limit),
                                u8"\n\n");

            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        currp->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;
        auto const limit_size_t_str{currp->str};

        ::std::size_t limit;
        auto const [next, err]{::fast_io::parse_by_scan(limit_size_t_str.cbegin(), limit_size_t_str.cend(), limit)};
        if(err != ::fast_io::parse_code::ok || next != limit_size_t_str.cend()) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid limit (size_t): \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                limit_size_t_str,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\". Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasm_set_initializer_limit),
                                u8"\n\n");

            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        auto& lim{::uwvm2::uwvm::runtime::initializer::initializer_limit};

        if(set_type_name == u8"runtime_modules") { lim.max_runtime_modules = limit; }
        else if(set_type_name == u8"imported_functions") { lim.max_imported_functions = limit; }
        else if(set_type_name == u8"imported_tables") { lim.max_imported_tables = limit; }
        else if(set_type_name == u8"imported_memories") { lim.max_imported_memories = limit; }
        else if(set_type_name == u8"imported_globals") { lim.max_imported_globals = limit; }
        else if(set_type_name == u8"local_defined_functions") { lim.max_local_defined_functions = limit; }
        else if(set_type_name == u8"local_defined_codes") { lim.max_local_defined_codes = limit; }
        else if(set_type_name == u8"local_defined_tables") { lim.max_local_defined_tables = limit; }
        else if(set_type_name == u8"local_defined_memories") { lim.max_local_defined_memories = limit; }
        else if(set_type_name == u8"local_defined_globals") { lim.max_local_defined_globals = limit; }
        else if(set_type_name == u8"local_defined_elements") { lim.max_local_defined_elements = limit; }
        else if(set_type_name == u8"local_defined_datas") { lim.max_local_defined_datas = limit; }
        else
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid type: \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                set_type_name,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\".\n" u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                u8"[info]  ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Available types: \n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_YELLOW),
                                u8"              - runtime_modules (default=",
                                ::uwvm2::uwvm::runtime::initializer::default_max_runtime_modules,
                                u8")\n" u8"              - imported_functions (default=",
                                ::uwvm2::uwvm::runtime::initializer::default_max_imported_functions,
                                u8")\n" u8"              - imported_tables (default=",
                                ::uwvm2::uwvm::runtime::initializer::default_max_imported_tables,
                                u8")\n" u8"              - imported_memories (default=",
                                ::uwvm2::uwvm::runtime::initializer::default_max_imported_memories,
                                u8")\n" u8"              - imported_globals (default=",
                                ::uwvm2::uwvm::runtime::initializer::default_max_imported_globals,
                                u8")\n" u8"              - local_defined_functions (default=",
                                ::uwvm2::uwvm::runtime::initializer::default_max_local_defined_functions,
                                u8")\n" u8"              - local_defined_codes (default=",
                                ::uwvm2::uwvm::runtime::initializer::default_max_local_defined_codes,
                                u8")\n" u8"              - local_defined_tables (default=",
                                ::uwvm2::uwvm::runtime::initializer::default_max_local_defined_tables,
                                u8")\n" u8"              - local_defined_memories (default=",
                                ::uwvm2::uwvm::runtime::initializer::default_max_local_defined_memories,
                                u8")\n" u8"              - local_defined_globals (default=",
                                ::uwvm2::uwvm::runtime::initializer::default_max_local_defined_globals,
                                u8")\n" u8"              - local_defined_elements (default=",
                                ::uwvm2::uwvm::runtime::initializer::default_max_local_defined_elements,
                                u8")\n" u8"              - local_defined_datas (default=",
                                ::uwvm2::uwvm::runtime::initializer::default_max_local_defined_datas,
                                u8")\n\n",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }
}  // namespace uwvm2::uwvm::cmdline::params::details

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

