/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @file        uwvm.cppm
 * @brief       uwvm cpp main function
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-20
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
# include <limits>
# include <utility>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/utils/debug/impl.h>
# include <uwvm2/uwvm/io/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
# include <uwvm2/uwvm/cmdline/impl.h>
# include <uwvm2/uwvm/run/impl.h>
# include <uwvm2/uwvm/crtmain/global/impl.h>
// callback
# include <uwvm2/uwvm/cmdline/callback/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm
{
    /// @brief      uwvm c++ uz u8 main function
    /// @param      argc Argument Count
    /// @param      argv Argument Vector
    /// @return     exit value
    inline int uwvm_uz_u8main(::std::size_t argc, char8_t const* const* argv) noexcept
    {
        switch(::uwvm2::uwvm::cmdline::parsing(argc, argv))
        {
            case ::uwvm2::uwvm::cmdline::parsing_return_val::def:
            {
                break;
            }
            case ::uwvm2::uwvm::cmdline::parsing_return_val::return0:
            {
                return static_cast<int>(::uwvm2::uwvm::run::retval::ok);
            }
            case ::uwvm2::uwvm::cmdline::parsing_return_val::returnm1:
            {
                return static_cast<int>(::uwvm2::uwvm::run::retval::parameter_error);
            }
            [[unlikely]] default:
            {
#if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                ::uwvm2::utils::debug::trap_and_inform_bug_pos();
#endif
                ::std::unreachable();
            }
        }

        return ::uwvm2::uwvm::run::run();
    }

#if !((defined(_WIN32) && !defined(__CYGWIN__)) && !defined(_WIN32_WINDOWS))  // NOT WINNT

    /// @brief      uwvm c++ main function (nont winnt)
    /// @details    Handles initialization of all global variables. Only non-raii global variables, or constexpr raii global variables, are allowed, rtti global
    ///             variables and constinit raii global variables are forbidden.
    /// @param      argc Argument Count
    /// @param      argv Argument Vector
    /// @return     exit value
    /// @see        main()
    inline int uwvm_main_non_winnt(int argc, char** argv) noexcept
    {
        // Automatically get the correct timezone data
        ::uwvm2::uwvm::global::tz_set_s tz_set_ele{};

        // The size_t of some platforms is smaller than int, in these platforms you need to do a size check before conversion
        constexpr auto size_t_max{::std::numeric_limits<::std::size_t>::max()};
        constexpr auto int_max{::std::numeric_limits<int>::max()};
        if constexpr(size_t_max < int_max)
        {
            // The size_t of current platforms is smaller than int, in these platforms you need to do a size check before conversion
            if(argc > size_t_max) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                    u8"[error] ",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"argc \"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                    argc,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\" exceeds the maximum value of size_t :\"",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_LT_GREEN),
                                    size_t_max,
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                    u8"\".",
                                    ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                    u8"\n\n");
                return static_cast<int>(::uwvm2::uwvm::run::retval::parameter_error);
            }
        }

        // Just in case argc is less than 0.
        if(argc < 0) [[unlikely]]
        {
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RED),
                                u8"[error] ",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"argc \"",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_CYAN),
                                argc,
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_WHITE),
                                u8"\" is less than zero.",
                                ::fast_io::mnp::cond(::uwvm2::uwvm::utils::ansies::put_color, UWVM_COLOR_U8_RST_ALL),
                                u8"\n\n");
            return static_cast<int>(::uwvm2::uwvm::run::retval::parameter_error);
        }

        // cast
        auto const argc_uz{static_cast<::std::size_t>(argc)};
        using char8_t_const_ptr_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const* const*;
        auto const argv_u8{reinterpret_cast<char8_t_const_ptr_const_may_alias_ptr>(argv)};

        // u8main
        return uwvm_uz_u8main(argc_uz, argv_u8);
    }

#else

    /// @brief      uwvm c++ main function (Windows NT)
    /// @details    Handles initialization of all global variables. Only non-raii global variables, or constexpr raii global variables, are allowed, rtti global
    ///             variables and constinit raii global variables are forbidden.
    /// @param      argc Argument Count
    /// @param      argv Argument Vector
    /// @return     exit value
    /// @see        main()
    inline int uwvm_main_winnt() noexcept
    {
        // program manipulation
        // Generate guard to protect global pod pointer

        // set win32-winnt console output and windows cp to utf8
        ::uwvm2::uwvm::global::set_win32_console_io_cp_to_utf8 set_native_console_io_cp_to_utf8_ele{};
        // set win32-winnt console ansi escape
        ::uwvm2::uwvm::global::enable_win32_ansi enable_native_ansi_ele{};

        // Automatically get the correct timezone data
        ::uwvm2::uwvm::global::tz_set_s tz_set_ele{};

        // codecvt parameters on winnt
        ::uwvm2::uwvm::cmdline::nt_code_cvt_argv_storage const u8_cmdline{::uwvm2::uwvm::cmdline::nt_code_cvt_argv()};
        auto const argc_uz{u8_cmdline.argc};
        auto const argv_u8{u8_cmdline.argv.data()};

        // fast_io network servive
        ::fast_io::win32_wsa_service service{};

        // u8main
        return uwvm_uz_u8main(argc_uz, argv_u8);
    }

#endif

}  // namespace uwvm2::uwvm

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
