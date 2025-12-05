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
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_push_macro.h>  // wasip1
# endif
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
#ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
# if defined(UWVM_IMPORT_WASI_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1_SUPPORT_SOCKET)

    namespace details
    {
        inline constexpr ::uwvm2::utils::container::u8string_view wasip1_socket_tcp_listen_alias{u8"-I1tcplisten"};
        inline constexpr ::uwvm2::utils::cmdline::parameter_return_type
            wasip1_socket_tcp_listen_callback(::uwvm2::utils::cmdline::parameter_parsing_results*,
                                              ::uwvm2::utils::cmdline::parameter_parsing_results*,
                                              ::uwvm2::utils::cmdline::parameter_parsing_results*) noexcept;
    }  // namespace details

#  if defined(__clang__)
#   pragma clang diagnostic push
#   pragma clang diagnostic ignored "-Wbraced-scalar-init"
#  endif
    inline constexpr ::uwvm2::utils::cmdline::parameter wasip1_socket_tcp_listen{
        .name{u8"--wasip1-socket-tcp-listen"},
        .describe{u8"Configure a WASI Preview 1 TCP listening socket from the command line."},
        .usage{u8"<fd:i32> [<ipv4|ipv6>:<port>"
#  if defined(UWVM_SUPPORT_UNIX_PATH_SOCKET)
               u8"|unix <path>"
#  endif
               u8"]"},
        .alias{::uwvm2::utils::cmdline::kns_u8_str_scatter_t{::std::addressof(details::wasip1_socket_tcp_listen_alias), 1uz}},
        .handle{::std::addressof(details::wasip1_socket_tcp_listen_callback)},
        .cate{::uwvm2::utils::cmdline::categorization::wasi}};
#  if defined(__clang__)
#   pragma clang diagnostic pop
#  endif

# endif
#endif
}

#ifndef UWVM_MODULE
# ifndef UWVM_DISABLE_LOCAL_IMPORTED_WASIP1
#  include <uwvm2/imported/wasi/wasip1/feature/feature_pop_macro.h>  // wasip1
# endif
# include <uwvm2/uwvm/utils/ansies/uwvm_color_pop_macro.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif
