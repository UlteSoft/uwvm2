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
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>  // wasip1
# endif
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/utils/depend/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/cmdline/params/impl.h>
# include <uwvm2/uwvm/imported/wasi/wasip1/storage/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params::details
{
#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# if defined(UWVM_IMPORT_WASI_WASIP1)

    UWVM_GNU_COLD inline constexpr ::uwvm2::utils::cmdline::parameter_return_type wasip1_set_fd_limit_callback(
        [[maybe_unused]] ::uwvm2::utils::cmdline::parameter_parsing_results * para_begin,
        ::uwvm2::utils::cmdline::parameter_parsing_results * para_curr,
        ::uwvm2::utils::cmdline::parameter_parsing_results * para_end) noexcept
    {
        // [... curr] ...
        // [  safe  ] unsafe (could be the module_end)
        //      ^^ para_curr

        auto currp1{para_curr + 1u};

        // [... curr] ...
        // [  safe  ] unsafe (could be the module_end)
        //            ^^ currp1

        // Check for out-of-bounds and not-argument
        if(currp1 == para_end || currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg) [[unlikely]]
        {
            // (currp1 == para_end):
            // [... curr] (end) ...
            // [  safe  ] unsafe (could be the module_end)
            //            ^^ currp1

            // (currp1->type != ::uwvm2::utils::cmdline::parameter_parsing_results_type::arg):
            // [... curr para] ...
            // [     safe    ] unsafe (could be the module_end)
            //           ^^ currp1

            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasip1_set_fd_limit),
                                // print_usage comes with UWVM_COLOR_U8_RST_ALL
                                u8"\n\n");

            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        // [... curr arg1] ...
        // [     safe     ] unsafe (could be the module_end)
        //           ^^ currp1

        // Setting the argument is already taken
        currp1->type = ::uwvm2::utils::cmdline::parameter_parsing_results_type::occupied_arg;

        // name
        auto const currp1_str{currp1->str};

        ::std::size_t limit;  // No initialization necessary
        auto const [next, err]{::fast_io::parse_by_scan(currp1_str.cbegin(), currp1_str.cend(), limit)};

        // parse u32 error
        if(err != ::fast_io::parse_code::ok || next != currp1_str.cend()) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"Invalid fd limit (size_t): \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                currp1_str,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\". Usage: ",
                                ::uwvm2::utils::cmdline::print_usage(::uwvm2::uwvm::cmdline::params::wasip1_set_fd_limit),
                                u8"\n\n");

            return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
        }

        using fd_t = ::uwvm2::imported::wasi::wasip1::abi::wasi_posix_fd_t;
        if constexpr(::std::numeric_limits<::std::size_t>::max() > ::std::numeric_limits<fd_t>::max())
        {
            if(limit > ::std::numeric_limits<fd_t>::max()) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                    u8"[error] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"Illegal fd limit (Exceeding the maximum value of fd): \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    currp1_str,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\".\n\n",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL));

                return ::uwvm2::utils::cmdline::parameter_return_type::return_m1_imme;
            }
        }

        // limit == 0: fd max
        if(limit == 0uz) { limit = ::std::numeric_limits<fd_t>::max(); }

        ::uwvm2::uwvm::imported::wasi::wasip1::storage::default_wasip1_env.fd_storage.fd_limit = limit;

        return ::uwvm2::utils::cmdline::parameter_return_type::def;
    }

# endif
#endif
}  // namespace uwvm2::uwvm::cmdline::params::details

#ifndef UWVM_MODULE
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>  // wasip1
# endif
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

