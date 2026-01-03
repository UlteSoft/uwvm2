/*************************************************************
 * Ultimate WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

/**
 * @author      MacroModel
 * @version     2.0.0
 * @date        2025-03-30
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
# include <uwvm2/imported/wasi/feature/feature_push_macro.h>  // wasi
// import
# include <fast_io.h>
# include <uwvm2/utils/container/impl.h>
# include <uwvm2/utils/cmdline/impl.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::uwvm::cmdline::params
{
    namespace details
    {
        inline bool help_is_exist{};  // [global]
        inline constexpr ::uwvm2::utils::container::u8string_view help_alias{u8"-h"};
#if defined(UWVM_MODULE)
        extern "C++"
#else
        inline constexpr
#endif
            ::uwvm2::utils::cmdline::parameter_return_type help_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                         ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                                                         ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;

    }  // namespace details

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wbraced-scalar-init"
#endif
    inline constexpr ::uwvm2::utils::cmdline::parameter help{.name{u8"--help"},
                                                             .describe{u8"Get help information."},
                                                             .usage{u8"([all|global|debug|wasm|log|runtime"
#if defined(UWVM_IMPORT_WASI)
                                                                    u8"|wasi"
#endif
                                                                    u8"])"},
                                                             .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::help_alias), 1uz}},
                                                             .handle{::std::addressof(details::help_callback)},
                                                             .is_exist{::std::addressof(details::help_is_exist)}};
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
}

#ifndef UWVM_MODULE
// macro
# include <uwvm2/imported/wasi/feature/feature_pop_macro.h>  // wasi
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
