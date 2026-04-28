/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-31
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
# include <memory>
// macro
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
# include <uwvm2/utils/ansies/impl.h>
# include <uwvm2/uwvm/utils/ansies/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))
UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params
{
    namespace details
    {
        inline constexpr ::uwvm2::utils::container::u8string_view log_win32_use_ansi_alias{u8"-log-ansi"};
    }

# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wbraced-scalar-init"
# endif
    inline constexpr ::uwvm2::utils::cmdline::parameter log_win32_use_ansi{
        .name{u8"--log-win32-use-ansi"},
        .describe{u8"Use ANSI escape sequences for colored logs on Win32."},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::log_win32_use_ansi_alias), 1uz}},
        .is_exist{::std::addressof(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b)},
        .cate{::uwvm2::utils::cmdline::categorization::log}};
# if defined(__clang__)
#  pragma clang diagnostic pop
# endif
}
#endif

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
