/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2 License (see LICENSE file).      *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-31
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

#if defined(_WIN32) && (_WIN32_WINNT < 0x0A00 || defined(_WIN32_WINDOWS))

# ifdef UWVM_MODULE
import fast_io;
import uwvm2.utils.cmdline;
import uwvm2.utils.ansies;
import uwvm2.uwvm.utils.ansies;
# else
// std
#  include <memory>
// macro
#  include <uwvm2/utils/macro/push_macros.h>
#  include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>
// improt
#  include <fast_io.h>
#  include <fast_io_dsal/string_view.h>
#  include <uwvm2/utils/cmdline/impl.h>
#  include <uwvm2/utils/ansies/impl.h>
#  include <uwvm2/uwvm/utils/ansies/impl.h>
# endif

# ifndef UWVM_MODULE_EXPORT
#  define UWVM_MODULE_EXPORT
# endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::paras
{
    namespace details
    {
        inline constexpr ::fast_io::u8string_view log_win32_use_ansi_alias{u8"-ansi-log"};
    }  // namespace details

# if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wbraced-scalar-init"
# endif
    inline constexpr ::uwvm2::utils::cmdline::parameter log_win32_use_ansi{
        .name{u8"--log-win32-use-ansi"},
        .describe{u8"Log on Win32 system using ANSI sequence under Win10. SetConsoleTextAttribute api is used by default."},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::log_win32_use_ansi_alias), 1}},
        .is_exist{::std::addressof(::uwvm2::uwvm::utils::ansies::log_win32_use_ansi_b)}};
# if defined(__clang__)
#  pragma clang diagnostic pop
# endif
}  // namespace uwvm2::uwvm::cmdline::paras

# ifndef UWVM_MODULE
// macro
#  include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
#  include <uwvm2/utils/macro/pop_macros.h>
# endif

#endif
