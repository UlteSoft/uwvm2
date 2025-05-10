/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2 License (see LICENSE file).      *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-04-16
 * @copyright   APL-2 License
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

#ifdef UWVM_MODULE
import fast_io;
import uwvm2.utils.cmdline;
#else
// std
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <fast_io_dsal/string_view.h>
# include <uwvm2/utils/cmdline/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::paras
{
    namespace details
    {
        inline bool log_output_is_exist{};
        inline constexpr ::fast_io::u8string_view log_output_alias{u8"-log"};
        extern "C++" ::uwvm2::utils::cmdline::parameter_return_type log_output_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                        ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                                        ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

    }  // namespace details

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wbraced-scalar-init"
#endif
    inline constexpr ::uwvm2::utils::cmdline::parameter u8log_output{
        .name{u8"--log-output"},
        .describe{u8"Setting the log output of the uwvm, (DEFAULT: err)."},
        .usage{
#if !defined(__AVR__) && !((defined(_WIN32) && !defined(__WINE__)) && defined(_WIN32_WINDOWS)) && !(defined(__MSDOS__) || defined(__DJGPP__)) &&               \
    !(defined(__NEWLIB__) && !defined(__CYGWIN__))
            u8"[out|err|file <file>]"
#else
            u8"[out|err]"
#endif
        },
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::log_output_alias), 1}},
        .handle{::std::addressof(details::log_output_callback)},
        .is_exist{::std::addressof(details::log_output_is_exist)}};
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
}  // namespace uwvm2::uwvm::cmdline::paras

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
