/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
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

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params
{
    namespace details
    {
        inline constexpr ::uwvm2::utils::container::u8string_view log_disable_warning_alias{u8"-log-dw"};
        inline constexpr ::uwvm2::utils::cmdline::parameter_return_type
            log_disable_warning_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                         ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                         ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

    }  // namespace details

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wbraced-scalar-init"
#endif
    inline constexpr ::uwvm2::utils::cmdline::parameter log_disable_warning{
        .name{u8"--log-disable-warning"},
        .describe{u8"Turn off the warning output of the uwvm."},
        .usage{u8"[all|vm|parser|untrusted-dl"
#if defined(UWVM_SUPPORT_PRELOAD_DL)
               u8"|dl"
#endif
#if defined(UWVM_SUPPORT_WEEK_SYMBOL)
               u8"|week-symbol"
#endif
               u8"|depend"
#if defined(_WIN32) && !defined(_WIN32_WINDOWS)
               u8"|nt-path"
#endif
#if defined(_WIN32) && defined(_WIN32_WINDOWS)
               u8"|toctou"
#endif
               u8"]"},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::log_disable_warning_alias), 1uz}},
        .handle{::std::addressof(details::log_disable_warning_callback)},
        .cate{::uwvm2::utils::cmdline::categorization::log}};
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

